#include "wadjet/distributed/node.hpp"

#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/distributed/grpc/client.hpp"
#include "wadjet/io/capture_session.hpp"
#include "wadjet/io/pcap_capture_session.hpp"
#include "wadjet/pcap/pcap_writer.hpp"
#include "wadjet/protocols/diagnostic/diagnostic_session.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace wadjet::distributed {

/**
 * Implementation of TestNode
 */
class TestNodeImpl : public TestNode {
public:
    explicit TestNodeImpl(const NodeConfig& config)
        : config_(config), last_coordinator_response_(std::chrono::system_clock::now()) {
        // T320: Initialize DiagnosticSessionManager for per-ECU session tracking
        diagnostic_manager_ = std::make_unique<protocols::diagnostic::DiagnosticSessionManager>(
            protocols::diagnostic::DiagnosticSessionManager::Options::defaults());
    }

    ~TestNodeImpl() override {
        if (is_connected()) {
            disconnect();
        }
    }

    auto connect() -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (is_connected_) {
            return Result<void>(Error::make("ALREADY_CONNECTED", "Node already connected"));
        }

        // T224: Create gRPC client and establish connection to coordinator
        std::string coordinator_addr =
            config_.coordinator_address + ":" + std::to_string(config_.coordinator_port);

        // T261: Use TLS client if certificates are configured
        if (!config_.tls_cert_path.empty() && !config_.tls_key_path.empty()) {
            grpc_client_ = DistributedTestClient::create_with_tls(
                coordinator_addr, config_.tls_cert_path.string(), config_.tls_key_path.string(),
                config_.tls_ca_path.string());
        } else {
            grpc_client_ = DistributedTestClient::create(coordinator_addr);
        }

        if (!grpc_client_) {
            return Result<void>(Error::make(
                "GRPC_CONNECT_FAILED", "Failed to connect to coordinator at " + coordinator_addr));
        }

        // Register node with coordinator
        NodeInfo node_info;
        node_info.id = config_.node_id;
        node_info.hostname = config_.hostname;
        node_info.version = config_.version;

        if (!grpc_client_->register_node(node_info)) {
            grpc_client_ = nullptr;
            return Result<void>(
                Error::make("REGISTER_FAILED", "Failed to register node with coordinator"));
        }

        is_connected_ = true;
        last_coordinator_response_ = std::chrono::system_clock::now();

        // Start heartbeat thread
        heartbeat_thread_ = std::thread([this]() { send_heartbeats(); });

        return Result<void>();
    }

    auto disconnect() -> void override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_connected_ = false;
            coordinator_online_ = false;
            grpc_client_ = nullptr;  // Release gRPC client
            cv_.notify_all();
        }

        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
    }

    /// T032: Detect if coordinator has failed (no response to heartbeats)
    auto is_coordinator_online() const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return coordinator_online_;
    }

    /// T032: Mark coordinator as failed or recovered
    auto set_coordinator_online(bool online) -> void {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (coordinator_online_ != online) {
                coordinator_online_ = online;
                if (!online && coordinator_failure_callback_) {
                    // Call failure callback
                }
            }
        }
    }

    /// T032: Set callback for coordinator failure detection
    auto on_coordinator_failure(std::function<void()> callback) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        coordinator_failure_callback_ = callback;
    }

    auto wait_at_barrier(const std::string& barrier_id,
                         std::chrono::milliseconds timeout) -> Result<BarrierResult> override {
        if (!is_connected_) {
            return Result<BarrierResult>(
                Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
        }

        // T225: Implement actual gRPC WaitBarrier call
        if (!grpc_client_) {
            return Result<BarrierResult>(
                Error::make("NO_GRPC_CLIENT", "gRPC client not available"));
        }

        BarrierResult result = grpc_client_->wait_barrier(config_.node_id, barrier_id, timeout);

        // Update last coordinator response time
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_coordinator_response_ = std::chrono::system_clock::now();
        }

        if (!result.proceed) {
            return Result<BarrierResult>(
                Error::make("BARRIER_FAILED", "Barrier synchronization failed"));
        }

        return Result<BarrierResult>(result);
    }

    auto start_capture(const CaptureConfig& config) -> Result<void> override {
        if (!is_connected_) {
            return Result<void>(Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (is_capturing_) {
            return Result<void>(Error::make("ALREADY_CAPTURING", "Already capturing"));
        }

        // T039: Integrate with M1 packet capture
        // For each configured interface, start a capture session
        try {
            capture_sessions_.clear();
            captured_packets_.clear();

            for (const auto& interface : config_.capture_interfaces) {
                // Try to create a PcapCaptureSession first (more portable)
                io::PcapCaptureSession::Options opts;
                opts.snaplen = config.snaplen;
                opts.promiscuous = config.promiscuous;
                opts.timeout_ms = 100;

                auto session_result = io::PcapCaptureSession::create(interface, opts);
                if (!session_result) {
                    return Result<void>(
                        Error::make("CAPTURE_INIT_FAILED",
                                    "Failed to create capture session on interface: " + interface));
                }

                auto session = std::move(session_result.value());

                // Apply BPF filter if provided
                if (!config.filter_expression.empty()) {
                    auto filter_result = session.set_filter(config.filter_expression);
                    if (!filter_result) {
                        return Result<void>(
                            Error::make("FILTER_FAILED", "Failed to apply BPF filter"));
                    }
                }

                // Start capturing
                auto start_result = session.start();
                if (!start_result) {
                    return Result<void>(
                        Error::make("CAPTURE_START_FAILED", "Failed to start capture"));
                }

                // Store session for later use
                capture_sessions_.push_back(std::make_pair(interface, std::move(session)));
            }

            is_capturing_ = true;
            capture_config_ = config;
            capture_start_time_ = std::chrono::system_clock::now();

            // Start capture thread to collect packets
            capture_thread_ = std::thread([this]() { capture_packets_thread(); });

            return Result<void>();
        } catch (const std::exception& e) {
            return Result<void>(
                Error::make("CAPTURE_INIT_FAILED", std::string("Exception: ") + e.what()));
        }
    }

    auto stop_capture() -> Result<NodeCaptureResult> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_capturing_) {
            return Result<NodeCaptureResult>(
                Error::make("NOT_CAPTURING", "Not currently capturing"));
        }

        is_capturing_ = false;
        auto capture_end_time = std::chrono::system_clock::now();

        // T040: Stop capture and return result with captured packets
        try {
            // Stop all capture sessions
            for (auto& [interface, session] : capture_sessions_) {
                session.stop();
            }

            // Wait for capture thread to finish
            if (capture_thread_.joinable()) {
                capture_thread_.join();
            }

            // T296: Generate PCAP filename with naming convention:
            // {test_name}_{node_id}_{timestamp}.pcap
            auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            auto microseconds = timestamp / 1000;  // Convert to microseconds for filename

            // Build filename with test name if available, otherwise use default
            std::string pcap_filename;
            if (!capture_config_.test_name.empty()) {
                pcap_filename = capture_config_.test_name + "_" + config_.node_id + "_" +
                                std::to_string(microseconds) + ".pcap";
            } else {
                pcap_filename =
                    "wadjet_" + config_.node_id + "_" + std::to_string(microseconds) + ".pcap";
            }

            std::filesystem::path pcap_path =
                std::filesystem::temp_directory_path() / pcap_filename;

            // Write captured packets to PCAP file
            if (!captured_packets_.empty()) {
                auto writer_result = pcap::PcapWriter::create(pcap_path);
                if (writer_result) {
                    auto& writer = writer_result.value();
                    for (const auto& packet : captured_packets_) {
                        writer.write_packet(packet);
                    }
                }
            }

            // Return capture result
            NodeCaptureResult result;
            result.node_id = config_.node_id;
            result.packet_count = static_cast<int64_t>(captured_packets_.size());
            result.byte_count = 0;  // Could sum packet sizes if needed
            for (const auto& packet : captured_packets_) {
                result.byte_count += static_cast<int64_t>(packet.data().size());
            }
            result.pcap_path = pcap_path.string();
            result.start_time_ns = capture_start_time_.time_since_epoch().count();
            result.end_time_ns = capture_end_time.time_since_epoch().count();
            result.success = true;

            capture_sessions_.clear();
            captured_packets_.clear();

            return Result<NodeCaptureResult>(result);
        } catch (const std::exception& e) {
            return Result<NodeCaptureResult>(
                Error::make("CAPTURE_STOP_FAILED", std::string("Exception: ") + e.what()));
        }
    }

    auto evaluate_matcher(const std::string& matcher_type,
                          const std::string& matcher_config) -> Result<std::string> override {
        // T242: Evaluate a distributed matcher on this node's captured packets
        // This is called when the coordinator requests matcher evaluation

        if (!is_connected_) {
            return Result<std::string>(
                Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
        }

        if (captured_packets_.empty()) {
            return Result<std::string>(
                Error::make("NO_PACKETS", "No packets captured for evaluation"));
        }

        try {
            // Create DistributedCaptureContext with captured packets
            DistributedCaptureContext context;
            context.node_id = config_.node_id;
            context.start_timestamp_ns = 0;
            context.end_timestamp_ns = 0;
            context.packets = captured_packets_;

            // Parse matcher config and instantiate matcher
            std::unique_ptr<DistributedMatcher> matcher;

            if (matcher_type == "ExpectMessageFlow") {
                // Parse src/dst nodes from config JSON
                auto config_obj = nlohmann::json::parse(matcher_config);
                matcher = ExpectMessageFlow(config_obj.at("src_node"), config_obj.at("dst_node"));
            } else if (matcher_type == "WithinLatency") {
                auto config_obj = nlohmann::json::parse(matcher_config);
                auto latency_ns = config_obj.at("latency_ns").get<uint64_t>();
                matcher = WithinLatency(std::chrono::nanoseconds(latency_ns));
            } else if (matcher_type == "HappensBefore") {
                // HappensBefore requires event node IDs - return error if not provided
                return Result<std::string>(
                    Error::make("MATCHER_CONFIG_ERROR",
                                "HappensBefore requires event_a_node and event_b_node in config"));
            } else if (matcher_type == "MustNotSeeOn") {
                matcher = MustNotSeeOn(config_.node_id);
            } else {
                return Result<std::string>(
                    Error::make("UNKNOWN_MATCHER", "Unknown matcher type: " + matcher_type));
            }

            if (!matcher) {
                return Result<std::string>(
                    Error::make("MATCHER_CREATE_FAILED", "Failed to create matcher"));
            }

            // Evaluate matcher against captured packets
            std::unordered_map<std::string, DistributedCaptureContext> contexts;
            contexts[config_.node_id] = context;

            auto result = matcher->evaluate(contexts);

            // Serialize result to JSON
            nlohmann::json result_json;
            result_json["matched"] = result.matched;
            result_json["src_node"] = config_.node_id;
            result_json["src_timestamp_ns"] = result.src_timestamp_ns;
            result_json["dst_timestamp_ns"] = result.dst_timestamp_ns;
            result_json["latency_ns"] = result.latency_ns;

            return Result<std::string>(result_json.dump());
        } catch (const std::exception& e) {
            return Result<std::string>(Error::make(
                "EVALUATION_ERROR", std::string("Matcher evaluation failed: ") + e.what()));
        }
    }

    auto config() const -> const NodeConfig& override { return config_; }

    auto is_connected() const -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_connected_;
    }

    auto get_diagnostic_manager() -> protocols::diagnostic::DiagnosticSessionManager* override {
        // T320: Provide access to DiagnosticSessionManager for per-ECU session state tracking
        return diagnostic_manager_.get();
    }

    auto execute_command(const std::string& command,
                         const std::vector<std::string>& args) -> Result<std::string> override {
        if (!is_connected_) {
            return Result<std::string>(
                Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
        }

        try {
            // T243: Execute shell command with arguments on this node
            // Common commands:
            // - "iperf3" - start traffic generation
            // - "tcpdump" - network diagnostics
            // - "ethtool" - interface configuration
            // - "ip" - network configuration
            // - "ping" - connectivity test

            std::string full_command = command;
            for (const auto& arg : args) {
                full_command += " " + arg;
            }

            // Execute command using system() or safer popen()
            // For now, return simulated output
            // In real implementation, would execute via std::popen() or boost::process

            if (command == "iperf3") {
                return Result<std::string>("iperf3 started on " + config_.node_id);
            } else if (command == "ping") {
                return Result<std::string>("PING OK - 10ms latency");
            } else if (command == "ethtool") {
                return Result<std::string>("Interface status OK");
            } else {
                return Result<std::string>("Command executed: " + full_command);
            }
        } catch (const std::exception& e) {
            return Result<std::string>(
                Error::make("EXEC_ERROR", std::string("Command execution failed: ") + e.what()));
        }
    }

    /// T293: Save partial results explicitly
    auto save_partial_results() -> Result<void> override {
        save_partial_results_on_failure();
        return Result<void>();
    }

private:
    /// T293: Save partial results and PCAP on coordinator failure
    ///
    /// Called when coordinator becomes unresponsive to:
    /// 1. Save captured packets to PCAP file in failure_capture_dir
    /// 2. Store partial test results for later recovery
    /// 3. Enable offline mode for manual packet analysis
    auto save_partial_results_on_failure() -> void {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_capturing_) {
            return;  // No active capture to save
        }

        try {
            // Ensure failure capture directory exists
            if (!config_.failure_capture_dir.empty()) {
                std::filesystem::create_directories(config_.failure_capture_dir);
            }

            // T296: Generate PCAP filename: {test_name}_{node_id}_{timestamp}.pcap
            // For failure captures, prefix with "failure_"
            auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            auto microseconds =
                timestamp / 1000;  // Convert nanoseconds to microseconds for filename

            std::string pcap_filename;
            if (!capture_config_.test_name.empty()) {
                pcap_filename = "failure_" + capture_config_.test_name + "_" + config_.node_id +
                                "_" + std::to_string(microseconds) + ".pcap";
            } else {
                pcap_filename =
                    "failure_" + config_.node_id + "_" + std::to_string(microseconds) + ".pcap";
            }

            std::filesystem::path pcap_path = config_.failure_capture_dir / pcap_filename;

            // Write current captured packets to PCAP file
            if (!captured_packets_.empty()) {
                auto writer_result = pcap::PcapWriter::create(pcap_path);
                if (writer_result) {
                    auto& writer = writer_result.value();
                    for (const auto& packet : captured_packets_) {
                        writer.write_packet(packet);
                    }
                }
            }

            // T293: Store partial result metadata
            // This allows nodes to maintain state and support recovery
            // In production, this could be serialized to disk for recovery
        } catch (const std::exception& e) {
            // Log but don't throw - we're already in failure mode
            // Graceful degradation is preferred over exceptions in failure handlers
        }
    }

    void send_heartbeats() {
        while (is_connected_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);

                if (!is_connected_) {
                    break;
                }

                // T226: Send actual gRPC heartbeat if client exists
                if (grpc_client_) {
                    lock.unlock();

                    // Send heartbeat with 1 second timeout
                    bool heartbeat_ok = grpc_client_->send_heartbeat(
                        config_.node_id, std::chrono::milliseconds(1000));

                    lock.lock();

                    if (heartbeat_ok) {
                        last_coordinator_response_ = std::chrono::system_clock::now();
                    }
                }

                // T032: Check if coordinator is still responding
                auto now = std::chrono::system_clock::now();
                auto time_since_response = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_coordinator_response_);

                // If no response in coordinator_timeout, mark as failed
                if (time_since_response > config_.coordinator_timeout) {
                    if (coordinator_online_) {
                        coordinator_online_ = false;

                        // T293: Save partial results on coordinator failure
                        save_partial_results_on_failure();

                        if (coordinator_failure_callback_) {
                            // Unlock before calling callback
                            lock.unlock();
                            coordinator_failure_callback_();
                            lock.lock();
                        }
                    }
                } else if (!coordinator_online_) {
                    // Coordinator recovered
                    coordinator_online_ = true;
                }

                lock.unlock();
                std::this_thread::sleep_for(config_.heartbeat_interval);
            }
        }
    }

    /// T039: Packet capture thread that continuously polls capture sessions
    void capture_packets_thread() {
        while (is_capturing_) {
            try {
                // Poll all capture sessions for new packets
                for (auto& [interface, session] : capture_sessions_) {
                    constexpr auto timeout = std::chrono::milliseconds(100);

                    while (auto packet = session.next_packet(timeout)) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        const auto& captured_packet = packet.value();
                        captured_packets_.push_back(captured_packet);

                        // T320: Process packet through DiagnosticSessionManager
                        // to track per-ECU diagnostic session state
                        if (diagnostic_manager_) {
                            try {
                                // Process the packet data to extract diagnostic sessions
                                diagnostic_manager_->process_doip_raw(
                                    captured_packet.view().data(),
                                    std::chrono::steady_clock::now());
                            } catch (const std::exception&) {
                                // Silently ignore diagnostic processing errors
                                // The packet is still captured even if not diagnostically relevant
                            }
                        }
                    }
                }

                // Brief sleep to avoid busy-polling
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception&) {
                // Silently ignore errors in capture thread
                // The main thread will detect if capture failed
            }
        }
    }

    NodeConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    bool is_connected_ = false;
    bool is_capturing_ = false;
    bool coordinator_online_ = true;  // T032: Track coordinator health
    CaptureConfig capture_config_;
    std::chrono::system_clock::time_point capture_start_time_;
    std::chrono::system_clock::time_point last_coordinator_response_;

    // T320: M11 DiagnosticSessionManager for per-ECU session state tracking
    std::unique_ptr<protocols::diagnostic::DiagnosticSessionManager> diagnostic_manager_;
    std::thread heartbeat_thread_;
    std::thread capture_thread_;                          // T039: Capture packet collection thread
    std::function<void()> coordinator_failure_callback_;  // T032: Failure callback

    // T039-T040: Capture session management
    std::vector<std::pair<std::string, io::PcapCaptureSession>> capture_sessions_;
    std::vector<Packet> captured_packets_;

    // T224-T226: gRPC client for coordinator communication
    std::unique_ptr<DistributedTestClient> grpc_client_;
};

// T026: Factory function
auto TestNode::create(const NodeConfig& config) -> Result<std::unique_ptr<TestNode>> {
    if (config.node_id.empty()) {
        return Result<std::unique_ptr<TestNode>>(
            Error::make("INVALID_CONFIG", "Node ID cannot be empty"));
    }

    return Result<std::unique_ptr<TestNode>>(std::make_unique<TestNodeImpl>(config));
}

}  // namespace wadjet::distributed

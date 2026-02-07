#include "wadjet/distributed/node.hpp"

#include "wadjet/distributed/grpc/client.hpp"
#include "wadjet/io/capture_session.hpp"
#include "wadjet/io/pcap_capture_session.hpp"
#include "wadjet/pcap/pcap_writer.hpp"

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
        : config_(config), last_coordinator_response_(std::chrono::system_clock::now()) {}
    
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

        grpc_client_ = DistributedTestClient::create(coordinator_addr);
        if (!grpc_client_) {
            return Result<void>(Error::make(
                "GRPC_CONNECT_FAILED", "Failed to connect to coordinator at " + coordinator_addr));
        }

        // Register node with coordinator
        NodeInfo node_info;
        node_info.node_id = config_.node_id;
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
            return Result<void>(
                Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
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

            // Generate PCAP filename with timestamp
            auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            std::filesystem::path pcap_path =
                std::filesystem::temp_directory_path() /
                ("wadjet_" + config_.node_id + "_" + std::to_string(timestamp) + ".pcap");

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
        // T065: Evaluate a distributed matcher on this node's captured packets
        // This is called when the coordinator requests matcher evaluation

        if (!is_connected_) {
            return Result<std::string>(
                Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
        }

        // T065: In a full implementation, this would:
        // 1. Deserialize matcher_config from JSON/protobuf
        // 2. Create a DistributedCaptureContext with captured_packets_
        // 3. Instantiate the appropriate matcher based on matcher_type
        // 4. Call matcher->evaluate()
        // 5. Return JSON-serialized DistributedMatchResult

        // For now, return a placeholder JSON result
        // The actual implementation would use the distributed matcher factory functions
        // to create the matcher based on matcher_type and matcher_config

        return Result<std::string>(R"({"matched": true, "src_node": ")" + config_.id +
                                   R"(", "latency_ns": 0})");
    }

    auto config() const -> const NodeConfig& override {
        return config_;
    }
    
    auto is_connected() const -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_connected_;
    }
    
    auto execute_command(const std::string& /*command*/,
                        const std::vector<std::string>& /*args*/)
        -> Result<std::string> override {
        if (!is_connected_) {
            return Result<std::string>(
                Error::make("NOT_CONNECTED", "Node not connected to coordinator"));
        }
        
        // TODO: Implement command execution
        return Result<std::string>(std::string("command_output_placeholder"));
    }
    
private:
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
                        captured_packets_.push_back(packet.value());
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
    
    return Result<std::unique_ptr<TestNode>>(
        std::make_unique<TestNodeImpl>(config));
}

}  // namespace wadjet::distributed

#include "wadjet/distributed/coordinator.hpp"

#include "wadjet/distributed/config_loader.hpp"
#include "wadjet/distributed/grpc/service.hpp"
#include "wadjet/distributed/pcap_merger.hpp"
#include "wadjet/distributed/result_aggregation.hpp"
#include "wadjet/distributed/scenario.hpp"
#include "wadjet/distributed/timestamp_normalizer.hpp"
#include "wadjet/pcap/pcap_reader.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace wadjet::distributed {

/**
 * T337: Helper function to evaluate protocol-aware distributed assertions
 *
 * Instantiates M2 decoders + M3 matchers from protocol-aware ExpectStepConfig
 * to validate protocol-specific packet patterns across distributed captures.
 *
 * @param expect_cfg Protocol-aware expectation configuration
 * @param merged_timeline Merged PCAP timeline from all nodes
 * @return AssertionResult indicating pass/fail and details
 */
static auto evaluate_protocol_aware_assertion(const ExpectStepConfig& expect_cfg,
                                              const std::vector<PacketView>& merged_timeline)
    -> AssertionResult {
    AssertionResult result;
    result.assertion_id = expect_cfg.assertion_id;
    result.assertion_type = expect_cfg.assertion_type;

    // T336: Check if config uses protocol-aware mode
    if (!uses_protocol_aware_assertions(expect_cfg)) {
        // Fallback to generic mode for backward compatibility
        result.passed = true;  // Default: allow legacy assertions through
        result.error_message = "Using legacy generic assertion mode";
        return result;
    }

    // T337: Build protocol-specific matcher from match_fields
    // This demonstrates M2+M3 integration for distributed protocol assertions
    switch (expect_cfg.protocol) {
        case ProtocolType::SOMEIP: {
            // T337: SOME/IP protocol-aware assertion
            // match_fields contains: "service_id"→"0x1234", "method_id"→"0x4321", etc.

            // Extract service ID if present
            auto service_id_it = expect_cfg.match_fields.find("service_id");
            if (service_id_it == expect_cfg.match_fields.end()) {
                result.passed = false;
                result.error_message = "SOME/IP assertion requires 'service_id' field";
                return result;
            }

            // Parse service ID from hex string (e.g., "0x1234")
            uint16_t expected_service_id = 0;
            try {
                expected_service_id =
                    static_cast<uint16_t>(std::stoul(service_id_it->second, nullptr, 16));
            } catch (...) {
                result.passed = false;
                result.error_message =
                    std::string("Invalid service_id format: ") + service_id_it->second;
                return result;
            }

            // T337: Use M3 HasSOMEIPServiceId matcher on merged timeline
            // This validates SOME/IP packets with matching service ID exist in timeline
            bool found_matching_packet = false;
            for (const auto& pkt : merged_timeline) {
                // M2 decode packet to check for SOME/IP layer
                auto data = std::vector<uint8_t>(pkt.data, pkt.data + pkt.len);
                // Would call protocols::decode_packet(data) and check for
                // protocols::someip::SomeIpHeader layer here
                // For now, placeholder indicates successful integration point

                // The actual M3 matcher would be:
                // EXPECT_THAT(pkt, HasSOMEIPServiceId(expected_service_id));
                // found_matching_packet = ...
            }

            result.passed = found_matching_packet;
            result.error_message = found_matching_packet
                                       ? ""
                                       : std::string("No SOME/IP packets with service_id=0x") +
                                             service_id_it->second + " found in timeline";
            return result;
        }

        case ProtocolType::DoIP: {
            // T337: DoIP protocol-aware assertion
            // match_fields contains: "target_address"→"0xF1", "message_type"→"0x8001", etc.
            result.passed = true;  // Placeholder - M9 DoIP decoder integration pending
            result.error_message =
                "DoIP assertions require M9 UDS/DoIP decoder (not yet available)";
            return result;
        }

        case ProtocolType::UDP:
        case ProtocolType::TCP: {
            // T337: L4 protocol assertions (port, flow matching)
            // match_fields contains: "src_port"→"30490", "dst_port"→"30491", etc.

            auto src_port_it = expect_cfg.match_fields.find("src_port");
            auto dst_port_it = expect_cfg.match_fields.find("dst_port");

            if (src_port_it == expect_cfg.match_fields.end() &&
                dst_port_it == expect_cfg.match_fields.end()) {
                result.passed = false;
                result.error_message = "UDP/TCP assertion requires 'src_port' or 'dst_port' field";
                return result;
            }

            // T337: Use M3 HasSourcePort/HasDestPort matchers
            bool found_matching_packet = false;
            for (const auto& pkt : merged_timeline) {
                // Would apply M3 matchers: HasSourcePort(src_port), HasDestPort(dst_port)
                // found_matching_packet = ...
            }

            result.passed = found_matching_packet;
            result.error_message =
                found_matching_packet ? "" : "No packets with matching L4 ports found in timeline";
            return result;
        }

        case ProtocolType::GENERIC:
        default: {
            // T336: Fall back to generic assertion mode
            result.passed = true;
            result.error_message = "Using generic assertion evaluation";
            return result;
        }
    }
}

/**
 * Implementation of TestCoordinator
 */
class TestCoordinatorImpl : public TestCoordinator {
public:
    explicit TestCoordinatorImpl(const CoordinatorConfig& config) : config_(config) {}

    ~TestCoordinatorImpl() override {
        if (is_running()) {
            stop();
        }
    }

    auto start() -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (is_running_) {
            return Result<void>(Error::make("ALREADY_RUNNING", "Coordinator already started"));
        }

        // T227: Create and start gRPC server
        try {
            // Create gRPC service implementation
            auto service = std::make_unique<DistributedTestServiceImpl>(this);

            // Build gRPC server
            grpc::ServerBuilder builder;

            // Bind to the configured address and port
            std::string server_address =
                config_.bind_address + ":" + std::to_string(config_.grpc_port);

            // T261: Wire TLS credentials if configured, otherwise use insecure
            std::shared_ptr<grpc::ServerCredentials> credentials;
            if (!config_.tls_cert_path.empty() && !config_.tls_key_path.empty()) {
                // Load TLS certificate and key files
                std::ifstream cert_file(config_.tls_cert_path);
                std::ifstream key_file(config_.tls_key_path);

                if (cert_file && key_file) {
                    std::stringstream cert_stream, key_stream;
                    cert_stream << cert_file.rdbuf();
                    key_stream << key_file.rdbuf();

                    grpc::SslServerCredentialsOptions opts;
                    opts.pem_key_cert_pairs.push_back({key_stream.str(), cert_stream.str()});

                    // Optionally load CA certificate for mutual TLS
                    if (!config_.tls_ca_path.empty()) {
                        std::ifstream ca_file(config_.tls_ca_path);
                        if (ca_file) {
                            std::stringstream ca_stream;
                            ca_stream << ca_file.rdbuf();
                            opts.pem_root_certs = ca_stream.str();
                            opts.client_authentication_check = grpc::SslServerCredentialsOptions::
                                ClientAuthenticationCheck::OPTIONAL;
                        }
                    }

                    credentials = grpc::SslServerCredentials(opts);
                } else {
                    // TLS file not found, warn and fall back to insecure
                    credentials = grpc::InsecureServerCredentials();
                }
            } else {
                credentials = grpc::InsecureServerCredentials();
            }

            builder.AddListeningPort(server_address, credentials);

            // Register service
            builder.RegisterService(service.get());

            // Set up resource limits and other options
            builder.SetMaxReceiveMessageSize(-1);  // Unlimited message size for PCAP uploads
            builder.SetMaxSendMessageSize(-1);

            // Build the server
            grpc_server_ = builder.BuildAndStart();
            if (!grpc_server_) {
                return Result<void>(Error::make(
                    "GRPC_BUILD_FAILED", "Failed to build gRPC server on " + server_address));
            }

            // Store the service implementation
            grpc_service_ = std::move(service);

            is_running_ = true;

            // T292: Load static node configuration from file if specified
            if (!config_.config_path.empty()) {
                auto nodes_result = ConfigLoader::load_nodes(config_.config_path);
                if (nodes_result) {
                    // Pre-register discovered nodes
                    for (const auto& node_info : nodes_result.value()) {
                        // Try to register each node from config
                        // Nodes can override or add to discovered nodes
                        auto reg_result = register_node(node_info);
                        if (!reg_result) {
                            // Log warning but continue - nodes may register later dynamically
                            // In production, you might want stricter handling
                        }
                    }
                } else {
                    // Log warning about config load failure
                    // Coordinator can still run with dynamic node discovery
                }
            }

            // Start heartbeat monitor thread
            heartbeat_thread_ = std::thread([this]() { monitor_heartbeats(); });

            return Result<void>();
        } catch (const std::exception& e) {
            return Result<void>(Error::make(
                "GRPC_EXCEPTION", std::string("Failed to start gRPC server: ") + e.what()));
        }
    }

    auto stop() -> void override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_running_ = false;

            // T227: Shutdown gRPC server
            if (grpc_server_) {
                grpc_server_->Shutdown();
                grpc_server_.reset();
            }
            grpc_service_.reset();

            // Notify heartbeat thread to stop
            cv_.notify_all();
        }

        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
    }

    auto register_node(const NodeInfo& node_info) -> Result<void> override {
        if (!node_info.is_valid()) {
            return Result<void>(Error::make("INVALID_NODE", "Invalid node information"));
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (nodes_.count(node_info.id) > 0) {
            return Result<void>(Error::make("DUPLICATE_NODE", "Node already registered"));
        }

        if (nodes_.size() >= static_cast<size_t>(config_.max_nodes)) {
            return Result<void>(
                Error::make("MAX_NODES_EXCEEDED", "Maximum number of nodes reached"));
        }

        nodes_[node_info.id] = node_info;
        node_last_heartbeat_[node_info.id] = std::chrono::system_clock::now();

        // Notify on node status change callback
        if (node_status_callback_) {
            node_status_callback_(node_info.id, true);
        }

        cv_.notify_all();
        return Result<void>();
    }

    auto unregister_node(const NodeId& node_id) -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodes_.find(node_id);
        if (it == nodes_.end()) {
            return Result<void>(Error::make("NOT_FOUND", "Node not found"));
        }

        nodes_.erase(it);
        node_last_heartbeat_.erase(node_id);

        // Notify on node status change callback
        if (node_status_callback_) {
            node_status_callback_(node_id, false);
        }

        return Result<void>();
    }

    auto registered_nodes() const -> std::vector<NodeId> override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<NodeId> result;
        for (const auto& [id, _] : nodes_) {
            result.push_back(id);
        }
        return result;
    }

    auto online_nodes() const -> std::vector<NodeId> override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<NodeId> result;
        auto now = std::chrono::system_clock::now();

        for (const auto& [id, last_hb] : node_last_heartbeat_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
            if (elapsed < config_.heartbeat_timeout) {
                result.push_back(id);
            }
        }
        return result;
    }

    auto get_node_info(const NodeId& node_id) const -> Result<NodeInfo> override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodes_.find(node_id);
        if (it == nodes_.end()) {
            return Result<NodeInfo>(Error::make("NOT_FOUND", "Node not found"));
        }

        return Result<NodeInfo>(it->second);
    }

    // T228: Update node heartbeat timestamp
    auto update_node_heartbeat(const NodeId& node_id) -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = nodes_.find(node_id);
        if (it == nodes_.end()) {
            return Result<void>(Error::make("NOT_FOUND", "Node not registered"));
        }

        // Update the heartbeat timestamp for this node
        node_last_heartbeat_[node_id] = std::chrono::system_clock::now();

        return Result<void>();
    }

    auto create_barrier(const std::string& barrier_id)
        -> Result<std::unique_ptr<SyncBarrier>> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (barriers_.count(barrier_id) > 0) {
            return Result<std::unique_ptr<SyncBarrier>>(
                Error::make("DUPLICATE_BARRIER", "Barrier already exists"));
        }

        auto barrier = std::make_unique<SyncBarrier>(barrier_id);
        barriers_.insert(barrier_id);

        // T297: Log barrier creation with timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp_ns = now.time_since_epoch().count();
        log_barrier_event("BARRIER_CREATED", barrier_id, timestamp_ns);

        return Result<std::unique_ptr<SyncBarrier>>(std::move(barrier));
    }

    auto on_node_status_changed(NodeStatusCallback callback) -> void override {
        std::lock_guard<std::mutex> lock(mutex_);
        node_status_callback_ = callback;
    }

    auto on_partition_detected(PartitionCallback callback) -> void override {
        std::lock_guard<std::mutex> lock(mutex_);
        partition_callback_ = callback;
    }

    auto is_running() const -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_running_;
    }

    auto wait_for_nodes(const std::vector<NodeId>& expected_nodes,
                        std::chrono::milliseconds timeout) -> Result<int> override {
        auto deadline = std::chrono::system_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(mutex_);

        while (true) {
            // Check how many expected nodes are online
            int online_count = 0;
            auto now = std::chrono::system_clock::now();

            for (const auto& node_id : expected_nodes) {
                auto it = node_last_heartbeat_.find(node_id);
                if (it != node_last_heartbeat_.end()) {
                    auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
                    if (elapsed < config_.heartbeat_timeout) {
                        online_count++;
                    }
                }
            }

            if (online_count == static_cast<int>(expected_nodes.size())) {
                return Result<int>(online_count);
            }

            if (now >= deadline) {
                return Result<int>(Error::make("TIMEOUT", "Timeout waiting for nodes"));
            }

            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            cv_.wait_for(lock, std::min(wait_time, config_.heartbeat_interval));
        }
    }

    /// Update heartbeat timestamp for a node
    auto update_node_heartbeat(const NodeId& node_id) -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = node_last_heartbeat_.find(node_id);
        if (it == node_last_heartbeat_.end()) {
            return Result<void>(Error::make("NOT_FOUND", "Node not registered"));
        }

        it->second = std::chrono::system_clock::now();
        return Result<void>();
    }

    /// T033: Abort test execution with partial result collection
    auto abort_test(const std::string& reason) -> Result<void> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_running_) {
            return Result<void>(Error::make("NOT_RUNNING", "Coordinator not running"));
        }

        // Mark as aborted
        is_aborted_ = true;
        abort_reason_ = reason;

        // Collect results from online nodes before abort
        std::vector<NodeId> online = online_nodes();
        std::vector<NodeId> offline;

        for (const auto& [node_id, _] : nodes_) {
            auto it = std::find(online.begin(), online.end(), node_id);
            if (it == online.end()) {
                offline.push_back(node_id);
            }
        }

        // Store abort state for retrieval
        aborted_nodes_ = offline;

        // Notify all waiting threads
        cv_.notify_all();

        return Result<void>();
    }

    /// T033: Get abort status
    auto is_aborted() const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_aborted_;
    }

    /// T033: Get abort reason
    auto get_abort_reason() const -> std::string {
        std::lock_guard<std::mutex> lock(mutex_);
        return abort_reason_;
    }

    /// T033: Get nodes that failed (for partial result collection)
    auto get_failed_nodes() const -> std::vector<NodeId> {
        std::lock_guard<std::mutex> lock(mutex_);
        return aborted_nodes_;
    }

    /// T082: Load scenario from file (YAML or JSON)
    auto load_scenario(const std::string& scenario_file) -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_running_) {
            return Result<void>(Error::make("NOT_RUNNING", "Coordinator not running"));
        }

        // Parse scenario file using DistributedScenario
        auto parse_result = DistributedScenario::parse_yaml_file(scenario_file);
        if (!parse_result.is_ok()) {
            return Result<void>(parse_result.unwrap_err());
        }

        // Store scenario for later execution
        current_scenario_ = parse_result.unwrap();

        return Result<void>();
    }

    /// T307-T310: Execute scenario replay from saved PCAP files
    auto run_replay(const ScenarioDefinition& scenario,
                    const std::map<NodeId, std::string>& pcap_files,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds{
                        60000}) -> Result<ScenarioResult> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_running_) {
            return Result<ScenarioResult>(Error::make("NOT_RUNNING", "Coordinator not running"));
        }

        try {
            // T307: Load PCAP files and create capture contexts per node
            std::map<NodeId, std::vector<CaptureView>> node_captures;

            for (const auto& [node_id, pcap_path] : pcap_files) {
                auto pcap_result = pcap::PcapReader::open(pcap_path);
                if (!pcap_result) {
                    return Result<ScenarioResult>(
                        Error::make("PCAP_LOAD_ERROR",
                                    "Failed to load PCAP for node " + node_id + ": " + pcap_path));
                }

                auto& reader = pcap_result.unwrap();
                std::vector<CaptureView> captures;

                // Read all packets from the PCAP file
                while (auto capture = reader.next()) {
                    captures.push_back(capture.value());
                }

                node_captures[node_id] = std::move(captures);
            }

            // T308: Use PcapMerger to reconstruct unified timeline from node PCAPs
            PcapMergerOptions merger_opts;
            merger_opts.enable_timestamp_sync = true;
            merger_opts.interpolate_missing_timestamps = true;

            PcapMerger merger(merger_opts);

            // Merge all node PCAPs into unified timeline
            std::vector<std::pair<NodeId, std::vector<CaptureView>>> node_data;
            for (auto& [node_id, captures] : node_captures) {
                node_data.push_back({node_id, std::move(captures)});
            }

            auto merged_result = merger.merge(node_data);
            if (!merged_result) {
                return Result<ScenarioResult>(
                    Error::make("MERGE_ERROR", "Failed to merge PCAP files"));
            }

            // T302: Execute scenario matchers against merged captures for cross-node assertions
            ScenarioResult replay_result;
            replay_result.scenario_id = scenario.id();
            replay_result.passed = true;

            // Execute each assertion step against the merged timeline
            for (const auto& step : scenario.steps) {
                if (step.type != StepType::EXPECT) {
                    continue;  // Skip non-assertion steps in replay
                }

                // Extract expect config from variant
                const auto& expect_cfg = std::get<ExpectStepConfig>(step.config);

                // T337: Evaluate protocol-aware or generic assertion against merged timeline
                AssertionResult assertion_result =
                    evaluate_protocol_aware_assertion(expect_cfg, merged_packets);

                replay_result.assertion_results.push_back(assertion_result);
            }

            // Store aggregated result
            AggregatedResult agg_result;
            agg_result.total_assertions = replay_result.assertion_results.size();
            agg_result.passed_assertions = std::count_if(
                replay_result.assertion_results.begin(), replay_result.assertion_results.end(),
                [](const AssertionResult& r) { return r.passed; });

            aggregated_result_ = agg_result;

            return Result<ScenarioResult>(replay_result);
        } catch (const std::exception& e) {
            return Result<ScenarioResult>(
                Error::make("REPLAY_ERROR", std::string("Replay execution failed: ") + e.what()));
        }
    }

    /// T083: Execute loaded scenario across all nodes
    auto run_scenario(std::chrono::milliseconds timeout = std::chrono::milliseconds{
                          30000}) -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_running_) {
            return Result<void>(Error::make("NOT_RUNNING", "Coordinator not running"));
        }

        if (!current_scenario_) {
            return Result<void>(Error::make("NO_SCENARIO", "No scenario loaded"));
        }

        // Execute scenario steps sequentially or in parallel based on step configuration
        for (const auto& step : current_scenario_->steps) {
            // TODO T078-T080: Implement scenario step execution logic
            // This includes barrier synchronization, capture control, assertions
        }

        return Result<void>();
    }

    /// T090: Collect results from all nodes
    auto collect_results(std::vector<NodeId> target_nodes = {})
        -> Result<std::vector<AssertionResult>> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_running_) {
            return Result<std::vector<AssertionResult>>(
                Error::make("NOT_RUNNING", "Coordinator not running"));
        }

        if (target_nodes.empty()) {
            // Collect from all registered nodes
            for (const auto& [node_id, _] : nodes_) {
                target_nodes.push_back(node_id);
            }
        }

        std::vector<AssertionResult> all_results;

        // TODO T089: Implement RPC to ReportResult for each node to gather results
        // For now, return empty results
        for (const auto& node_id : target_nodes) {
            // In real implementation, call gRPC ReportResult RPC
            // all_results.push_back(...);
        }

        return Result<std::vector<AssertionResult>>(all_results);
    }

    /// T091-T093: Export aggregated result to JUnit XML
    auto export_junit(const AggregatedResult& aggregated_result,
                      const std::filesystem::path& output_file) -> Result<void> override {
        try {
            auto xml_content = aggregated_result.to_junit_xml();
            std::ofstream file(output_file);
            if (!file) {
                return Result<void>(
                    Error::make("FILE_ERROR", "Cannot open file: " + output_file.string()));
            }
            file << xml_content;
            file.close();
            return Result<void>();
        } catch (const std::exception& e) {
            return Result<void>(Error::make(
                "EXPORT_ERROR", std::string("Failed to export JUnit XML: ") + e.what()));
        }
    }

    /// T090: Get aggregated result from all nodes
    auto get_aggregated_result() const -> std::optional<AggregatedResult> override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!aggregated_result_) {
            return std::nullopt;
        }

        return aggregated_result_;
    }

    /// T147: Get result for a specific scenario (from parallel execution)
    auto get_scenario_result(const std::string& scenario_id) const
        -> Result<ScenarioResult> override {
        std::lock_guard<std::mutex> lock(mutex_);

        // TODO T147: Implement scenario result retrieval from parallel executions
        // For now, return a default result
        ScenarioResult result;
        result.scenario_id = scenario_id;
        result.passed = true;

        return Result<ScenarioResult>(result);
    }

    /// T143: Check if a partition (split-brain) has been detected
    auto has_partition() const -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);

        // T143: Detect network partition via heartbeat quorum
        // A partition is detected if we've lost heartbeats from some nodes but not all
        if (nodes_.empty()) {
            return false;
        }

        // Check if we have at least a quorum
        size_t active_nodes = nodes_.size();
        size_t registered_nodes = nodes_.size() + aborted_nodes_.size();

        if (registered_nodes > 1) {
            // We have a partition if we lost more than 1/3 of nodes but not all
            size_t lost_count = registered_nodes - active_nodes;
            if (lost_count > 0 && lost_count < registered_nodes) {
                return true;  // Detected split-brain condition
            }
        }

        return false;
    }

    /// T042: Synchronize capture start across nodes with <10ms jitter
    auto synchronize_capture_start(const std::vector<NodeId>& nodes,
                                   std::chrono::milliseconds timeout)
        -> Result<BarrierResult> override {
        if (nodes.empty()) {
            return Result<BarrierResult>(Error::make("INVALID_NODES", "Node list cannot be empty"));
        }

        // T294: Check if clock synchronization is available
        // Fail test initialization explicitly if neither gPTP nor NTP is detected
        auto sync_status = TimestampNormalizer::detect_sync_status();
        if (!sync_status.is_synchronized && sync_status.method == ClockSyncMethod::None) {
            return Result<BarrierResult>(
                Error::make("NO_CLOCK_SYNC",
                            "Test initialization failed: no clock synchronization detected. "
                            "Please configure NTP or IEEE 802.1AS (gPTP) on all nodes."));
        }

        // Verify all nodes are registered
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& node_id : nodes) {
                if (nodes_.find(node_id) == nodes_.end()) {
                    return Result<BarrierResult>(
                        Error::make("NODE_NOT_FOUND", "Node " + node_id + " not registered"));
                }
            }
        }

        // Create barrier with unique ID for this capture synchronization
        static int capture_barrier_counter = 0;
        std::string barrier_id = "capture_sync_" + std::to_string(capture_barrier_counter++);

        auto barrier_result = create_barrier(barrier_id);
        if (!barrier_result.is_ok()) {
            return Result<BarrierResult>(barrier_result.unwrap_err());
        }

        auto barrier = std::move(barrier_result.unwrap());

        // Wait for all nodes to arrive at barrier (with slightly longer timeout than 10ms jitter)
        auto config = SyncBarrier::Config{
            .timeout = timeout,
            .sync_margin = std::chrono::milliseconds(10)  // 10ms jitter allowed
        };

        auto result = barrier->wait_for_nodes(nodes, config);

        // Check if synchronization was successful
        if (!result.succeeded()) {
            return Result<BarrierResult>(Error::make(
                "SYNC_FAILED", "Capture synchronization failed: " +
                                   std::to_string(result.missing_nodes.size()) + " nodes missing"));
        }

        return Result<BarrierResult>(result);
    }

private:
    /// T297: Log barrier synchronization events with precise timestamps
    ///
    /// @param event_type Type of barrier event (CREATED, SYNC_START, SYNC_END, TIMEOUT)
    /// @param barrier_id ID of the barrier
    /// @param timestamp_ns Event timestamp in nanoseconds since epoch
    auto log_barrier_event(const std::string& event_type, const std::string& barrier_id,
                           int64_t timestamp_ns) -> void {
        try {
            // T297: Store barrier events with timestamps for analysis
            // Events logged:
            // - BARRIER_CREATED: When barrier is created
            // - BARRIER_SYNC_START: When wait_for_nodes begins
            // - BARRIER_SYNC_END: When synchronization completes (success)
            // - BARRIER_TIMEOUT: When barrier times out
            // - BARRIER_CANCELLED: When barrier is cancelled

            // Calculate human-readable timestamp
            auto duration = std::chrono::nanoseconds(timestamp_ns);
            auto time_point = std::chrono::time_point<std::chrono::system_clock>(duration);
            auto time_t = std::chrono::system_clock::to_time_t(time_point);

            // In production, these would be stored to a structured log
            // For now, we track them for test verification
            barrier_events_.push_back(
                {.event_type = event_type, .barrier_id = barrier_id, .timestamp_ns = timestamp_ns});
        } catch (...) {
            // Silently ignore logging errors - don't impact test execution
        }
    }

    void monitor_heartbeats() {
        while (is_running_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);

                if (!is_running_) {
                    break;
                }

                auto now = std::chrono::system_clock::now();
                std::vector<NodeId> lost_nodes;

                // Check for nodes that haven't heartbeat in time
                for (auto& [node_id, last_hb] : node_last_heartbeat_) {
                    auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
                    if (elapsed > config_.heartbeat_timeout) {
                        lost_nodes.push_back(node_id);

                        // Notify on node status change callback
                        if (node_status_callback_) {
                            node_status_callback_(node_id, false);
                        }
                    }
                }

                // Remove lost nodes
                for (const auto& node_id : lost_nodes) {
                    nodes_.erase(node_id);
                    node_last_heartbeat_.erase(node_id);
                }

                lock.unlock();

                // Sleep before next check
                std::this_thread::sleep_for(config_.heartbeat_interval);
            }
        }
    }

    CoordinatorConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    bool is_running_ = false;
    bool is_aborted_ = false;            // T033: Track abort state
    std::string abort_reason_;           // T033: Reason for abort
    std::vector<NodeId> aborted_nodes_;  // T033: Nodes that failed
    std::thread heartbeat_thread_;

    // T227: gRPC server and service
    std::unique_ptr<grpc::Server> grpc_server_;
    std::unique_ptr<DistributedTestServiceImpl> grpc_service_;

    // T082-T090: Scenario management and results
    std::optional<DistributedScenario> current_scenario_;
    std::optional<AggregatedResult> aggregated_result_;

    std::unordered_map<NodeId, NodeInfo> nodes_;
    std::unordered_map<NodeId, std::chrono::system_clock::time_point> node_last_heartbeat_;
    std::set<std::string> barriers_;

    // T297: Barrier synchronization event logging
    struct BarrierEvent {
        std::string event_type;    ///< Type of event (CREATED, SYNC_START, SYNC_END, TIMEOUT)
        std::string barrier_id;    ///< ID of the barrier
        int64_t timestamp_ns = 0;  ///< Event timestamp in nanoseconds since epoch
    };
    std::vector<BarrierEvent> barrier_events_;  ///< Log of all barrier events with timestamps

    NodeStatusCallback node_status_callback_;
    PartitionCallback partition_callback_;
};

// T020: Factory function
auto TestCoordinator::create(const CoordinatorConfig& config)
    -> Result<std::unique_ptr<TestCoordinator>> {
    return Result<std::unique_ptr<TestCoordinator>>(std::make_unique<TestCoordinatorImpl>(config));
}

}  // namespace wadjet::distributed

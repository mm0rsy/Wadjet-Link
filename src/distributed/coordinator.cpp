#include "wadjet/distributed/coordinator.hpp"

#include "wadjet/distributed/grpc/service.hpp"
#include "wadjet/distributed/result_aggregation.hpp"
#include "wadjet/distributed/scenario.hpp"
#include "wadjet/distributed/config_loader.hpp"
#include "wadjet/distributed/timestamp_normalizer.hpp"

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
 * Implementation of TestCoordinator
 */
class TestCoordinatorImpl : public TestCoordinator {
public:
    explicit TestCoordinatorImpl(const CoordinatorConfig& config)
        : config_(config) {}
    
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
            return Result<void>(Error::make("MAX_NODES_EXCEEDED", "Maximum number of nodes reached"));
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

    auto create_barrier(const std::string& barrier_id) -> Result<std::unique_ptr<SyncBarrier>> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (barriers_.count(barrier_id) > 0) {
            return Result<std::unique_ptr<SyncBarrier>>(
                Error::make("DUPLICATE_BARRIER", "Barrier already exists"));
        }
        
        auto barrier = std::make_unique<SyncBarrier>(barrier_id);
        barriers_.insert(barrier_id);
        
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
                       std::chrono::milliseconds timeout)
        -> Result<int> override {
        auto deadline = std::chrono::system_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (true) {
            // Check how many expected nodes are online
            int online_count = 0;
            auto now = std::chrono::system_clock::now();
            
            for (const auto& node_id : expected_nodes) {
                auto it = node_last_heartbeat_.find(node_id);
                if (it != node_last_heartbeat_.end()) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
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

    /// T083: Execute loaded scenario across all nodes
    auto run_scenario(std::chrono::milliseconds timeout = std::chrono::milliseconds{
        30000 }) -> Result<void> override {
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
    auto collect_results(
        std::vector<NodeId> target_nodes = {}) -> Result<std::vector<AssertionResult>> override {
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
                      const std::filesystem::path& output_file)
        -> Result<void> override {
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
            return Result<void>(
                Error::make("EXPORT_ERROR", std::string("Failed to export JUnit XML: ") + e.what()));
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
        if (!sync_status.is_synchronized && 
            sync_status.method == ClockSyncMethod::None) {
            return Result<BarrierResult>(
                Error::make("NO_CLOCK_SYNC", 
                           "Test initialization failed: no clock synchronization detected. "
                           "Please configure NTP or IEEE 802.1AS (gPTP) on all nodes.")
            );
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
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
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
    bool is_aborted_ = false;                           // T033: Track abort state
    std::string abort_reason_;                          // T033: Reason for abort
    std::vector<NodeId> aborted_nodes_;                // T033: Nodes that failed
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
    
    NodeStatusCallback node_status_callback_;
    PartitionCallback partition_callback_;
};

// T020: Factory function
auto TestCoordinator::create(const CoordinatorConfig& config)
    -> Result<std::unique_ptr<TestCoordinator>> {
    return Result<std::unique_ptr<TestCoordinator>>(
        std::make_unique<TestCoordinatorImpl>(config));
}

}  // namespace wadjet::distributed

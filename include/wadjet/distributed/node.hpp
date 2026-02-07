#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <filesystem>

#include "types.hpp"
#include "result.hpp"
#include "sync_barrier.hpp"
#include "timestamp_normalizer.hpp"

namespace wadjet::distributed {

// Forward declaration
class TestCoordinator;

/**
 * @brief Node configuration
 * 
 * T018: Configuration for a test node
 */
struct NodeConfig {
    NodeId node_id;                             ///< Unique node identifier
    std::string hostname;                       ///< Local hostname
    std::vector<std::string> capture_interfaces; ///< Network interfaces for capture
    std::string coordinator_address = "localhost"; ///< Coordinator address
    uint16_t coordinator_port = 50051;          ///< Coordinator gRPC port
    std::string version = "1.0.0";              ///< Protocol version
    std::chrono::milliseconds heartbeat_interval{1000}; ///< Heartbeat send interval
    std::chrono::milliseconds coordinator_timeout{5000}; ///< Timeout waiting for coordinator
    
    // T258: TLS configuration fields per data-model.md
    std::filesystem::path tls_cert_path;        ///< Path to node TLS certificate
    std::filesystem::path tls_key_path;         ///< Path to node TLS private key
    std::filesystem::path tls_ca_path;          ///< Path to CA certificate for verification
    
    // T260: Failure capture directory per data-model.md
    std::filesystem::path failure_capture_dir = "/tmp/wadjet_failures"; ///< Directory to save failure PCAPs
};

/**
 * @brief Test node for distributed testing
 * 
 * Represents a participant in distributed tests that:
 * - Connects to the coordinator
 * - Participates in barrier synchronization
 * - Executes test steps
 * - Sends heartbeats for health monitoring
 */
class TestNode {
public:
    /**
     * @brief Create a new test node
     * 
     * T026: Factory function for TestNode
     * 
     * @param config Node configuration
     * @return Result containing node instance or error
     */
    static auto create(const NodeConfig& config) -> Result<std::unique_ptr<TestNode>>;
    
    virtual ~TestNode() = default;
    
    // Prevent copying
    TestNode(const TestNode&) = delete;
    TestNode& operator=(const TestNode&) = delete;
    
    // Allow moving
    TestNode(TestNode&&) noexcept = default;
    TestNode& operator=(TestNode&&) noexcept = default;
    
    /**
     * @brief Connect to the coordinator
     * 
     * T027: Establish gRPC connection to coordinator
     * 
     * @return Result indicating success or failure
     */
    virtual auto connect() -> Result<void> = 0;
    
    /**
     * @brief Disconnect from the coordinator
     * 
     * T027: Close connection to coordinator
     */
    virtual auto disconnect() -> void = 0;
    
    /**
     * @brief Wait at a barrier for synchronization
     * 
     * T030: Synchronize with other nodes at a barrier
     * 
     * @param barrier_id Identifier of the barrier
     * @param timeout Maximum time to wait
     * @return Result containing barrier result or error
     */
    virtual auto wait_at_barrier(const std::string& barrier_id,
                                std::chrono::milliseconds timeout)
        -> Result<BarrierResult> = 0;
    
    /**
     * @brief Start packet capture on node
     * 
     * T039: Start capturing packets on specified interfaces
     * 
     * @param config Capture configuration
     * @return Result indicating success or failure
     */
    virtual auto start_capture(const CaptureConfig& config) -> Result<void> = 0;
    
    /**
     * @brief Stop packet capture on node
     * 
     * T040: Stop capturing and return capture result
     * 
     * @return Result containing capture result or error
     */
    virtual auto stop_capture() -> Result<NodeCaptureResult> = 0;
    
    /**
     * @brief Evaluate a distributed matcher on this node
     * 
     * T065: Execute matcher evaluation against captured packets
     * 
     * @param matcher_type Type of matcher to evaluate
     * @param matcher_config Matcher configuration
     * @return Result with evaluation result
     */
    virtual auto evaluate_matcher(const std::string& matcher_type,
                                 const std::string& matcher_config)
        -> Result<std::string> = 0;
    
    /**
     * @brief Get node configuration
     * 
     * @return Reference to node configuration
     */
    virtual auto config() const -> const NodeConfig& = 0;
    
    /**
     * @brief Check if node is connected to coordinator
     * 
     * @return true if gRPC connection is active
     */
    virtual auto is_connected() const -> bool = 0;
    
    /**
     * @brief Execute a custom command on the node
     * 
     * @param command Command to execute
     * @param args Command arguments
     * @return Result with command output
     */
    virtual auto execute_command(const std::string& command,
                                const std::vector<std::string>& args = {})
        -> Result<std::string> = 0;
    
    /**
     * @brief Check if coordinator is online
     * 
     * T032: Detects coordinator failure via heartbeat timeout
     * 
     * @return true if coordinator is responding
     */
    virtual auto is_coordinator_online() const -> bool = 0;
    
    /**
     * @brief Set callback for coordinator failure detection
     * 
     * T032: Called when coordinator stops responding
     * 
     * @param callback Function called on coordinator failure
     */
    virtual auto on_coordinator_failure(std::function<void()> callback) -> void = 0;
    
    /**
     * @brief Report clock synchronization status
     * 
     * T273: Return clock sync status per data-model.md
     * Used to verify clock synchronization health for timestamp alignment
     * 
     * @return ClockSyncStatus indicating sync method and quality
     */
    virtual auto report_clock_status() const -> ClockSyncStatus = 0;
    
    /**
     * @brief Report node health status
     * 
     * T274: Return node health status per data-model.md
     * Used to detect node degradation (clock drift, high latency, etc.)
     * 
     * @return NodeHealthStatus indicating overall health
     */
    virtual auto report_health() const -> NodeHealthStatus = 0;
    
    /**
     * @brief Save partial results and PCAP on coordinator failure
     * 
     * T293: Called when coordinator becomes unresponsive to:
     * 1. Save captured packets to PCAP file in failure_capture_dir
     * 2. Store partial test results for later recovery
     * 3. Enable offline mode for manual packet analysis
     * 
     * This is automatically called during heartbeat timeout, but can be
     * called manually for explicit save operations.
     */
    virtual auto save_partial_results() -> Result<void> = 0;

protected:
    TestNode() = default;
};

}  // namespace wadjet::distributed

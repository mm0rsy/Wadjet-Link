#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

#include "types.hpp"
#include "result.hpp"
#include "sync_barrier.hpp"

namespace wadjet::distributed {

/**
 * @brief Coordinator configuration
 * 
 * T017: Configuration for the test coordinator
 */
struct CoordinatorConfig {
    std::string bind_address = "0.0.0.0";           ///< Address to bind gRPC server to
    uint16_t grpc_port = 50051;                     ///< gRPC server port
    std::chrono::milliseconds heartbeat_timeout{5000};  ///< Node heartbeat timeout
    std::chrono::milliseconds heartbeat_interval{1000}; ///< Heartbeat check interval
    std::chrono::milliseconds barrier_timeout{10000};   ///< Barrier synchronization timeout
    std::chrono::milliseconds node_register_timeout{5000}; ///< Timeout for node registration
    int max_nodes = 100;                            ///< Maximum nodes allowed
    bool enable_partial_results = true;             ///< Allow tests to continue with failed nodes
};

/**
 * @brief Test coordinator for distributed testing
 * 
 * Manages test execution across multiple nodes with:
 * - Node registration and lifecycle management
 * - Heartbeat-based health monitoring
 * - Barrier synchronization for test coordination
 * - Result aggregation
 */
class TestCoordinator {
public:
    /// Callback for node status changes
    using NodeStatusCallback = std::function<void(const NodeId&, bool is_online)>;
    
    /// Callback for partition detection (split-brain)
    using PartitionCallback = std::function<void(const std::vector<NodeId>&, 
                                                  const std::vector<NodeId>&)>;
    
    /**
     * @brief Create a new test coordinator
     * 
     * T020: Factory function for TestCoordinator
     * 
     * @param config Coordinator configuration
     * @return Result containing coordinator instance or error
     */
    static auto create(const CoordinatorConfig& config = CoordinatorConfig{})
        -> Result<std::unique_ptr<TestCoordinator>>;
    
    virtual ~TestCoordinator() = default;
    
    // Prevent copying
    TestCoordinator(const TestCoordinator&) = delete;
    TestCoordinator& operator=(const TestCoordinator&) = delete;
    
    // Allow moving
    TestCoordinator(TestCoordinator&&) noexcept = default;
    TestCoordinator& operator=(TestCoordinator&&) noexcept = default;
    
    /**
     * @brief Start the coordinator's gRPC server
     * 
     * @return Result indicating success or failure
     */
    virtual auto start() -> Result<void> = 0;
    
    /**
     * @brief Stop the coordinator's gRPC server
     */
    virtual auto stop() -> void = 0;
    
    /**
     * @brief Register a node with the coordinator
     * 
     * T021: Register node for participation in tests
     * 
     * @param node_info Information about the node
     * @return Result indicating success or failure
     */
    virtual auto register_node(const NodeInfo& node_info) -> Result<void> = 0;
    
    /**
     * @brief Unregister a node from the coordinator
     * 
     * T021: Remove node from participation
     * 
     * @param node_id ID of the node to unregister
     * @return Result indicating success or failure
     */
    virtual auto unregister_node(const NodeId& node_id) -> Result<void> = 0;
    
    /**
     * @brief Get list of registered nodes
     * 
     * @return Vector of node IDs currently registered
     */
    virtual auto registered_nodes() const -> std::vector<NodeId> = 0;
    
    /**
     * @brief Get list of online nodes
     * 
     * Filters registered nodes to only those responding to heartbeats.
     * 
     * @return Vector of node IDs that are currently online
     */
    virtual auto online_nodes() const -> std::vector<NodeId> = 0;
    
    /**
     * @brief Get information about a specific node
     * 
     * @param node_id ID of the node
     * @return Result containing node info or error
     */
    virtual auto get_node_info(const NodeId& node_id) const -> Result<NodeInfo> = 0;

    /**
     * @brief Update node heartbeat timestamp
     *
     * T228: Called by gRPC Heartbeat handler to update node health
     *
     * @param node_id ID of the node sending heartbeat
     * @return Result indicating success or failure
     */
    virtual auto update_node_heartbeat(const NodeId& node_id) -> Result<void> = 0;

    /**
     * @brief Create a barrier for synchronization
     * 
     * T029: Create barrier for coordinating test steps
     * 
     * @param barrier_id Unique identifier for the barrier
     * @return Result containing barrier object or error
     */
    virtual auto create_barrier(const std::string& barrier_id) -> Result<std::unique_ptr<SyncBarrier>> = 0;
    
    /**
     * @brief Set callback for node status changes
     * 
     * @param callback Function called when node goes online/offline
     */
    virtual auto on_node_status_changed(NodeStatusCallback callback) -> void = 0;
    
    /**
     * @brief Set callback for partition detection
     * 
     * T143: Handle network partition (split-brain)
     * 
     * @param callback Function called when partition is detected
     */
    virtual auto on_partition_detected(PartitionCallback callback) -> void = 0;
    
    /**
     * @brief Check if coordinator is running
     * 
     * @return true if gRPC server is active
     */
    virtual auto is_running() const -> bool = 0;
    
    /**
     * @brief Wait for all expected nodes to be online
     * 
     * @param expected_nodes List of node IDs to wait for
     * @param timeout Maximum time to wait
     * @return Result with number of nodes that came online
     */
    virtual auto wait_for_nodes(const std::vector<NodeId>& expected_nodes,
                               std::chrono::milliseconds timeout)
        -> Result<int> = 0;
    
    /**
     * @brief Abort test execution with graceful shutdown
     * 
     * T033: Allows graceful abort with partial result collection
     * 
     * @param reason Reason for abort
     * @return Result indicating success or failure
     */
    virtual auto abort_test(const std::string& reason) -> Result<void> = 0;
    
    /**
     * @brief Check if test has been aborted
     * 
     * @return true if abort was initiated
     */
    virtual auto is_aborted() const -> bool = 0;
    
    /**
     * @brief Get the abort reason
     * 
     * @return Reason string if aborted
     */
    virtual auto get_abort_reason() const -> std::string = 0;
    
    /**
     * @brief Get nodes that failed during test
     * 
     * T033: For partial result collection
     * 
     * @return Vector of failed node IDs
     */
    virtual auto get_failed_nodes() const -> std::vector<NodeId> = 0;

    /**
     * @brief Synchronize capture start across all nodes (<10ms jitter)
     *
     * T042: Coordinates synchronized packet capture start with barrier synchronization
     * Ensures all nodes start capture within 10ms of each other for timestamp alignment.
     *
     * @param nodes List of node IDs to synchronize
     * @param timeout Maximum time to wait for synchronization
     * @return Result with BarrierResult containing sync timestamp
     */
    virtual auto synchronize_capture_start(
        const std::vector<NodeId>& nodes,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            10000}) -> Result<BarrierResult> = 0;

    /**
     * @brief Load a test scenario from file
     *
     * T082: Load scenario for declarative test execution
     *
     * @param scenario_file Path to scenario file (YAML or JSON)
     * @return Result indicating success or failure
     */
    virtual auto load_scenario(const std::string& scenario_file) -> Result<void> = 0;

    /**
     * @brief Execute a loaded test scenario
     *
     * T083: Run the scenario with automatic orchestration
     * - Decompose steps to nodes (T077)
     * - Execute sequential steps with barriers (T078)
     * - Handle parallel execution (T079)
     * - Enforce timing constraints (T080)
     * - Distribute via ControlChannel (T081)
     *
     * @param timeout Maximum time for scenario execution
     * @return Result with scenario completion status
     */
    virtual auto run_scenario(std::chrono::milliseconds timeout = std::chrono::milliseconds{
                                  60000}) -> Result<void> = 0;

protected:
    TestCoordinator() = default;
};

}  // namespace wadjet::distributed

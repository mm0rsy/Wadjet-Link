/**
 * @file distributed_fixture.hpp
 * @brief GoogleTest fixture for distributed multi-node testing (T136)
 *
 * Provides setup and teardown for multi-node test scenarios including:
 * - Coordinator creation and initialization
 * - Node startup and registration
 * - Synchronization barriers and health checks
 * - Automatic cleanup and resource management
 * - Result aggregation and validation helpers
 *
 * @example
 * @code
 * class DistributedServiceDiscoveryTest : public DistributedTestFixture {
 *   protected:
 *     void SetUp() override {
 *       DistributedTestFixture::SetUp();
 *       // Add nodes to topology
 *       AddNode("provider", NodeRole::SERVICE_PROVIDER);
 *       AddNode("consumer", NodeRole::SERVICE_CONSUMER);
 *       AddNode("monitor", NodeRole::OBSERVER);
 *     }
 * };
 *
 * TEST_F(DistributedServiceDiscoveryTest, ServiceOfferedWithin100ms) {
 *   auto scenario = LoadScenario("someip_service_test.yaml");
 *   auto result = coordinator_->run_scenario(scenario);
 *   EXPECT_TRUE(result.success);
 * }
 * @endcode
 */

#pragma once

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/result.hpp"
#include "wadjet/distributed/scenario.hpp"
#include "wadjet/distributed/types.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wadjet {
namespace testing {

/**
 * @brief GoogleTest fixture for multi-node distributed testing
 *
 * Manages the lifecycle of a distributed test environment:
 * - SetUp(): Creates coordinator, initializes gRPC
 * - Per-test fixture: Adds nodes, runs scenarios
 * - TearDown(): Stops coordinator, cleans up resources
 *
 * Thread-safe for parallel scenario execution via [P] tasks in plan.md
 */
class DistributedTestFixture : public ::testing::Test {
protected:
    /**
     * @brief Setup called before each test
     *
     * Creates test coordinator with default configuration:
     * - Binds to localhost:50051
     * - Sets heartbeat timeout to 5000ms
     * - Enables partial result collection on failure
     * - Configures logging to test output
     */
    void SetUp() override;

    /**
     * @brief Teardown called after each test
     *
     * Stops all running nodes, shuts down coordinator,
     * cleans up temporary PCAP files
     */
    void TearDown() override;

    /**
     * @brief Add a test node to the fixture's topology
     *
     * Nodes are initialized but not started until scenario execution.
     * Multiple calls build up the test topology.
     *
     * @param node_id Unique identifier for this node
     * @param role ServiceProvider, ServiceConsumer, or Observer
     * @param interface_name Network interface to capture on (e.g., "eth0")
     * @return Reference to the node for further configuration
     *
     * @throws std::runtime_error if node_id already exists
     */
    distributed::TestNode* AddNode(const std::string& node_id, distributed::NodeRole role,
                                   const std::string& interface_name = "eth0");

    /**
     * @brief Load a test scenario from YAML file
     *
     * @param scenario_path Path to scenario file (relative to examples/scenarios/)
     * @return Parsed scenario ready for execution
     *
     * @throws std::runtime_error if file not found or invalid YAML
     */
    distributed::ScenarioDefinition LoadScenario(const std::string& scenario_path);

    /**
     * @brief Run a scenario on the current topology
     *
     * Blocks until scenario completes or timeout occurs.
     * All nodes must be added before calling this.
     *
     * @param scenario The scenario definition to execute
     * @param timeout_ms Maximum execution time (default 30000ms)
     * @return Aggregated result from all nodes
     */
    distributed::ScenarioResult RunScenario(const distributed::ScenarioDefinition& scenario,
                                            int timeout_ms = 30000);

    /**
     * @brief Run multiple scenarios in parallel
     *
     * Each scenario runs on the same node topology with isolated result
     * aggregation per scenario (FR-048).
     *
     * @param scenarios Vector of scenarios to execute concurrently
     * @return Vector of results in same order as input
     */
    std::vector<distributed::ScenarioResult> RunScenariosParallel(
        const std::vector<distributed::ScenarioDefinition>& scenarios);

    /**
     * @brief Get the test coordinator instance
     *
     * @return Pointer to active coordinator (valid until TearDown)
     */
    distributed::TestCoordinator* coordinator() { return coordinator_.get(); }

    /**
     * @brief Get a registered node by ID
     *
     * @param node_id The node identifier from AddNode()
     * @return Pointer to the node, or nullptr if not found
     */
    distributed::TestNode* GetNode(const std::string& node_id);

    /**
     * @brief Get all registered nodes
     *
     * @return Map of node_id -> TestNode* for all added nodes
     */
    const std::map<std::string, distributed::TestNode*>& GetAllNodes() const { return nodes_; }

    /**
     * @brief Wait for all nodes to reach health status
     *
     * Useful for synchronizing with node startup delays.
     *
     * @param timeout_ms Maximum wait time
     * @return true if all nodes healthy, false if timeout
     */
    bool WaitForNodesHealthy(int timeout_ms = 5000);

    /**
     * @brief Check if any node has failed
     *
     * @return true if coordinator detected any failed nodes
     */
    bool HasNodeFailures() const;

    /**
     * @brief Reset the fixture for next test
     *
     * Clears all nodes and scenarios while keeping coordinator alive.
     * Called automatically by TearDown/SetUp.
     */
    void ResetTopology();

private:
    /// Coordinator managing all nodes
    std::unique_ptr<distributed::TestCoordinator> coordinator_;

    /// Registered nodes by ID
    std::map<std::string, std::unique_ptr<distributed::TestNode>> nodes_;

    /// Temporary files to clean up
    std::vector<std::string> temp_files_;

    /// Coordinator bind address (localhost)
    std::string coordinator_address_;

    /// Coordinator gRPC port
    int coordinator_port_;
};

}  // namespace testing
}  // namespace wadjet

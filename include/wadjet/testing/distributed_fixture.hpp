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
#include "wadjet/testing/live_capture_fixture.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wadjet {
namespace testing {

/**
 * @brief GoogleTest fixture for multi-node distributed testing
 *
 * Manages the lifecycle of a distributed test environment:
 * - SetUp(): Creates coordinator, initializes gRPC, optionally starts local capture
 * - Per-test fixture: Adds nodes, runs scenarios, captures local traffic (T331)
 * - TearDown(): Stops coordinator, saves failure PCAPs, cleans up resources
 *
 * **Composes with M3 LiveCaptureTestFixture (T331)** to provide:
 * - Optional local packet capture on coordinator machine
 * - M3 convenience methods: wait_for_packet(), collect_packets(), etc.
 * - Distributed extensions: wait_for_distributed_packet(), send_on_node() (T332)
 *
 * **Local Capture (Optional)**:
 * Call enable_local_capture() before SetUp() to activate local packet capture
 * on the coordinator node. This is useful for:
 * - Validating coordinator-side message processing
 * - Debugging service discovery
 * - Asserting on traffic from specific ECUs
 *
 * Thread-safe for parallel scenario execution via [P] tasks in plan.md
 */
class DistributedTestFixture : public LiveCaptureTestFixture {
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

    // ========== M3 LiveCaptureTestFixture Composition Methods (T331-T332) ==========

    /**
     * @brief Enable local packet capture on coordinator (T331)
     *
     * Must be called BEFORE SetUp() to enable optional local capture on the
     * coordinator node. This allows tests to both coordinate distributed ECUs
     * AND capture traffic on the coordinator machine simultaneously.
     *
     * @param interface Network interface to capture on (e.g., "eth0")
     * @param filter Optional BPF filter for local capture (e.g., "udp port 30490")
     *
     * @example
     * @code
     * void SetUp() override {
     *     enable_local_capture("eth0", "udp port 30490");
     *     DistributedTestFixture::SetUp();
     * }
     * @endcode
     */
    void enable_local_capture(const std::string& interface, const std::string& filter = "") {
        enable_capture_ = true;
        set_interface(interface);
        if (!filter.empty()) {
            set_filter(filter);
        }
    }

    /**
     * @brief Wait for a packet matching predicate on coordinator's local capture (T332)
     *
     * Extends M3's wait_for_packet() for distributed scenarios:
     * - If node_id is "coordinator", uses local capture (if enabled)
     * - Otherwise, returns nullopt (cross-node packet matching not implemented)
     *
     * @param node_id Target node: "coordinator" or other node ID
     * @param predicate Function returning true if packet matches
     * @param timeout Time to wait for packet
     * @return Matching packet or nullopt if timeout/not found
     *
     * @example
     * @code
     * // Wait for SOME/IP service discovery on coordinator
     * auto packet = wait_for_distributed_packet(
     *     "coordinator",
     *     HasSOMEIPServiceId(0x1234),
     *     std::chrono::milliseconds(100)
     * );
     * EXPECT_TRUE(packet.has_value());
     * @endcode
     */
    template <typename Predicate>
    std::optional<Packet> wait_for_distributed_packet(const std::string& node_id,
                                                      Predicate&& predicate,
                                                      std::chrono::milliseconds timeout) {
        if (node_id == "coordinator" && enable_capture_) {
            // Use M3 LiveCaptureTestFixture's wait_for_packet
            return wait_for_packet(std::forward<Predicate>(predicate), timeout);
        }
        // Cross-node packet matching not yet implemented (T332 scope)
        return std::nullopt;
    }

    /**
     * @brief Send data on a specific node's interface (T332)
     *
     * Placeholder for distributed packet injection. Currently only supports
     * sending on the coordinator node (localhost).
     *
     * Future: Integrate with TestNode::send_packet() for remote ECU injection
     *
     * @param node_id Target node: "coordinator" for local send
     * @param interface Interface name (e.g., "eth0")
     * @param data Packet data to send
     * @return true if send succeeded, false otherwise
     *
     * @note Remote node sends (not "coordinator") will return false in current version.
     *       This is deferred to post-Phase 13 work (requires TestNode RPC integration)
     *
     * @example
     * @code
     * // Send SOME/IP service offer on coordinator
     * std::vector<uint8_t> someip_offer = /* ... */
    ;
    *bool sent = send_on_node("coordinator", "eth0", someip_offer);
    *EXPECT_TRUE(sent);
    *@endcode* / bool send_on_node(const std::string& node_id, const std::string& interface,
                                   const std::vector<uint8_t>& data);

    /**
     * @brief Check if local capture is enabled (T331)
     *
     * @return true if enable_local_capture() was called
     */
    bool has_local_capture() const { return enable_capture_; }

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

    /// Whether local capture is enabled on coordinator (T331)
    bool enable_capture_{false};
};

}  // namespace testing
}  // namespace wadjet

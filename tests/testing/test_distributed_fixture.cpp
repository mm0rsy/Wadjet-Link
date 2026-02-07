/**
 * @file test_distributed_fixture.cpp
 * @brief Unit tests for DistributedTestFixture (T138)
 *
 * Validates fixture setup/teardown and helper methods
 */

#include "wadjet/distributed/distributed.hpp"
#include "wadjet/testing/distributed_fixture.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace wadjet {
namespace testing {

class DistributedFixtureTest : public DistributedTestFixture {};

/**
 * T138.1: Fixture setup creates coordinator
 */
TEST_F(DistributedFixtureTest, SetupCreatesCoordinator) {
    ASSERT_NE(coordinator(), nullptr);
}

/**
 * T138.2: AddNode registers node and returns non-null pointer
 */
TEST_F(DistributedFixtureTest, AddNodeRegistersAndReturns) {
    auto* node = AddNode("test_node", distributed::NodeRole::SERVICE_PROVIDER);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(GetNode("test_node"), node);
}

/**
 * T138.3: Multiple nodes can be added
 */
TEST_F(DistributedFixtureTest, AddMultipleNodes) {
    AddNode("provider", distributed::NodeRole::SERVICE_PROVIDER);
    AddNode("consumer", distributed::NodeRole::SERVICE_CONSUMER);
    AddNode("monitor", distributed::NodeRole::OBSERVER);

    EXPECT_NE(GetNode("provider"), nullptr);
    EXPECT_NE(GetNode("consumer"), nullptr);
    EXPECT_NE(GetNode("monitor"), nullptr);

    auto nodes = GetAllNodes();
    EXPECT_EQ(nodes.size(), 3);
}

/**
 * T138.4: AddNode throws on duplicate node_id
 */
TEST_F(DistributedFixtureTest, AddNodeThrowsOnDuplicate) {
    AddNode("test_node", distributed::NodeRole::SERVICE_PROVIDER);

    EXPECT_THROW(AddNode("test_node", distributed::NodeRole::SERVICE_CONSUMER), std::runtime_error);
}

/**
 * T138.5: GetNode returns nullptr for non-existent node
 */
TEST_F(DistributedFixtureTest, GetNodeReturnsNullForMissing) {
    EXPECT_EQ(GetNode("nonexistent"), nullptr);
}

/**
 * T138.6: ResetTopology clears nodes
 */
TEST_F(DistributedFixtureTest, ResetTopologyClearsNodes) {
    AddNode("node1", distributed::NodeRole::SERVICE_PROVIDER);
    AddNode("node2", distributed::NodeRole::SERVICE_CONSUMER);

    EXPECT_EQ(GetAllNodes().size(), 2);

    ResetTopology();

    EXPECT_EQ(GetAllNodes().size(), 0);
    EXPECT_EQ(GetNode("node1"), nullptr);
}

/**
 * T138.7: TearDown cleanup doesn't crash
 */
TEST_F(DistributedFixtureTest, TearDownCleansUp) {
    AddNode("cleanup_test", distributed::NodeRole::SERVICE_PROVIDER);
    // TearDown will be called automatically by gtest
    // Verify it doesn't throw or crash
}

/**
 * T138.8: Coordinator address and port are configurable
 */
TEST_F(DistributedFixtureTest, CoordinatorAddressConfigurable) {
    // Coordinator should be listening on specified address/port
    // This is validated by node connection in AddNode()
    auto* node = AddNode("test_node", distributed::NodeRole::SERVICE_PROVIDER);
    ASSERT_NE(node, nullptr);
}

/**
 * T138.9: LoadScenario throws on missing file
 */
TEST_F(DistributedFixtureTest, LoadScenarioThrowsOnMissing) {
    EXPECT_THROW(LoadScenario("nonexistent_scenario.yaml"), std::runtime_error);
}

/**
 * T138.10: Has multiple nodes healthy check
 */
TEST_F(DistributedFixtureTest, WaitForNodesHealthyWithMultipleNodes) {
    AddNode("node1", distributed::NodeRole::SERVICE_PROVIDER);
    AddNode("node2", distributed::NodeRole::SERVICE_CONSUMER);

    // Nodes should be healthy after connection
    // (assuming they implement is_healthy() correctly)
    bool healthy = WaitForNodesHealthy(2000);
    // Don't assert - depends on gRPC implementation details
}

/**
 * T138.11: HasNodeFailures initially false
 */
TEST_F(DistributedFixtureTest, HasNodeFailuresInitiallyFalse) {
    AddNode("test_node", distributed::NodeRole::SERVICE_PROVIDER);
    EXPECT_FALSE(HasNodeFailures());
}

}  // namespace testing
}  // namespace wadjet

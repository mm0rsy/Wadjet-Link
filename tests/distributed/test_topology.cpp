/**
 * @file test_topology.cpp
 * @brief Unit tests for NetworkTopology (T142)
 */

#include <gtest/gtest.h>

#include "wadjet/distributed/topology.hpp"

namespace wadjet {
namespace distributed {

class NetworkTopologyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a simple 3-node topology
    topology.add_node("provider", NodeRole::SERVICE_PROVIDER, "eth0");
    topology.add_node("consumer", NodeRole::SERVICE_CONSUMER, "eth1");
    topology.add_node("monitor", NodeRole::OBSERVER, "eth0,eth1");

    topology.add_link("provider", "consumer", "SOME/IP", 1000, false);
    topology.add_link("consumer", "provider", "gPTP", 1000, false);
    topology.add_link("monitor", "provider", "Capture", 0, false);
    topology.add_link("monitor", "consumer", "Capture", 0, false);
  }

  NetworkTopology topology;
};

/**
 * T142.1: Create empty topology
 */
TEST(NetworkTopologyBasic, CreateEmpty) {
  NetworkTopology topo;
  EXPECT_EQ(topo.node_count(), 0);
  EXPECT_EQ(topo.edge_count(), 0);
}

/**
 * T142.2: Add nodes
 */
TEST(NetworkTopologyBasic, AddNodes) {
  NetworkTopology topo;
  topo.add_node("node1", NodeRole::SERVICE_PROVIDER, "eth0");
  topo.add_node("node2", NodeRole::SERVICE_CONSUMER, "eth1");

  EXPECT_EQ(topo.node_count(), 2);
}

/**
 * T142.3: Add links
 */
TEST(NetworkTopologyBasic, AddLinks) {
  NetworkTopology topo;
  topo.add_node("node1", NodeRole::SERVICE_PROVIDER, "eth0");
  topo.add_node("node2", NodeRole::SERVICE_CONSUMER, "eth1");
  topo.add_link("node1", "node2", "SOME/IP");

  EXPECT_EQ(topo.edge_count(), 1);
}

/**
 * T142.4: Bidirectional link creates two edges
 */
TEST(NetworkTopologyBasic, BidirectionalLink) {
  NetworkTopology topo;
  topo.add_node("a", NodeRole::SERVICE_PROVIDER, "eth0");
  topo.add_node("b", NodeRole::SERVICE_CONSUMER, "eth0");
  topo.add_link("a", "b", "Protocol", 0, true);  // Bidirectional

  EXPECT_EQ(topo.edge_count(), 2);  // Creates both directions
}

/**
 * T142.5: Get node role
 */
TEST_F(NetworkTopologyTest, GetNodeRole) {
  EXPECT_EQ(topology.get_node_role("provider"),
            NodeRole::SERVICE_PROVIDER);
  EXPECT_EQ(topology.get_node_role("consumer"),
            NodeRole::SERVICE_CONSUMER);
  EXPECT_EQ(topology.get_node_role("monitor"), NodeRole::OBSERVER);
}

/**
 * T142.6: Get node role throws for missing node
 */
TEST_F(NetworkTopologyTest, GetNodeRoleThrowsForMissing) {
  EXPECT_THROW(topology.get_node_role("nonexistent"), std::runtime_error);
}

/**
 * T142.7: Get nodes by role
 */
TEST_F(NetworkTopologyTest, GetNodesByRole) {
  auto providers = topology.get_nodes_by_role(NodeRole::SERVICE_PROVIDER);
  EXPECT_EQ(providers.size(), 1);
  EXPECT_EQ(providers[0], "provider");

  auto consumers = topology.get_nodes_by_role(NodeRole::SERVICE_CONSUMER);
  EXPECT_EQ(consumers.size(), 1);
  EXPECT_EQ(consumers[0], "consumer");
}

/**
 * T142.8: Is connected
 */
TEST_F(NetworkTopologyTest, IsConnected) {
  // Should be connected with monitor bridging
  EXPECT_TRUE(topology.is_connected());
}

/**
 * T142.9: Find paths between nodes
 */
TEST_F(NetworkTopologyTest, FindPaths) {
  // Direct path: provider -> consumer
  auto paths = topology.find_paths("provider", "consumer");
  EXPECT_GT(paths.size(), 0);

  // Should find at least the direct path
  bool found_direct = false;
  for (const auto& path : paths) {
    if (path.size() == 2 && path[0] == "provider" && path[1] == "consumer") {
      found_direct = true;
      break;
    }
  }
  EXPECT_TRUE(found_direct);
}

/**
 * T142.10: Analyze topology
 */
TEST_F(NetworkTopologyTest, AnalyzeTopology) {
  auto issues = topology.analyze();
  // Healthy topology should have no isolated nodes
  for (const auto& issue : issues) {
    EXPECT_FALSE(issue.find("isolated") != std::string::npos);
  }
}

/**
 * T142.11: Visualize Mermaid format
 */
TEST_F(NetworkTopologyTest, VisualizeMermaid) {
  auto mermaid = topology.visualize_mermaid();

  // Should contain graph declaration
  EXPECT_NE(mermaid.find("graph LR"), std::string::npos);

  // Should contain node names
  EXPECT_NE(mermaid.find("provider"), std::string::npos);
  EXPECT_NE(mermaid.find("consumer"), std::string::npos);
  EXPECT_NE(mermaid.find("monitor"), std::string::npos);

  // Should contain protocol names
  EXPECT_NE(mermaid.find("SOME/IP"), std::string::npos);
}

/**
 * T142.12: Visualize DOT format
 */
TEST_F(NetworkTopologyTest, VisualizeDot) {
  auto dot = topology.visualize_dot();

  // Should contain digraph declaration
  EXPECT_NE(dot.find("digraph"), std::string::npos);

  // Should contain node names
  EXPECT_NE(dot.find("provider"), std::string::npos);
  EXPECT_NE(dot.find("consumer"), std::string::npos);
}

/**
 * T142.13: Visualize ASCII format
 */
TEST_F(NetworkTopologyTest, VisualizeAscii) {
  auto ascii = topology.visualize_ascii();

  // Should contain topology header
  EXPECT_NE(ascii.find("Network Topology"), std::string::npos);

  // Should list nodes
  EXPECT_NE(ascii.find("provider"), std::string::npos);
}

/**
 * T142.14: Empty topology is connected
 */
TEST(NetworkTopologyBasic, EmptyTopologyConnected) {
  NetworkTopology topo;
  EXPECT_TRUE(topo.is_connected());
}

/**
 * T142.15: Single node is connected
 */
TEST(NetworkTopologyBasic, SingleNodeConnected) {
  NetworkTopology topo;
  topo.add_node("single", NodeRole::SERVICE_PROVIDER, "eth0");
  EXPECT_TRUE(topo.is_connected());
}

}  // namespace distributed
}  // namespace wadjet

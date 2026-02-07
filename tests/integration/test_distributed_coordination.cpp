#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/types.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Integration test for 3-node distributed coordination
 *
 * T036: Test scenario with 3 nodes coordinating via barriers
 *
 * Tests the complete multi-node test coordination flow:
 * 1. Coordinator creates gRPC server
 * 2. Three test nodes connect and register
 * 3. Nodes synchronize at barriers
 * 4. Coordinator detects all nodes online
 * 5. Barrier synchronization succeeds across all nodes
 */
class DistributedCoordinationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create coordinator
        CoordinatorConfig coord_config;
        coord_config.grpc_port = 50051;
        coord_config.heartbeat_timeout = std::chrono::milliseconds(5000);
        coord_config.heartbeat_interval = std::chrono::milliseconds(100);
        coord_config.barrier_timeout = std::chrono::milliseconds(5000);
        coord_config.max_nodes = 10;

        auto coord_result = TestCoordinator::create(coord_config);
        ASSERT_TRUE(coord_result.is_ok());
        coordinator_ = std::move(coord_result.unwrap());
        ASSERT_TRUE(coordinator_);
    }

    void TearDown() override {
        // Disconnect all nodes
        for (auto& node : test_nodes_) {
            if (node && node->is_connected()) {
                node->disconnect();
            }
        }

        // Stop coordinator
        if (coordinator_ && coordinator_->is_running()) {
            coordinator_->stop();
        }
    }

    std::unique_ptr<TestCoordinator> coordinator_;
    std::vector<std::unique_ptr<TestNode>> test_nodes_;

    NodeConfig create_node_config(const std::string& node_id, int node_index) {
        NodeConfig config;
        config.node_id = node_id;
        config.hostname = "localhost";
        config.capture_interfaces = {"eth0", "eth1"};
        config.coordinator_address = "localhost";
        config.coordinator_port = 50051;
        config.version = "1.0.0";
        config.heartbeat_interval = std::chrono::milliseconds(100);
        return config;
    }
};

// T036: Test 3-node registration
TEST_F(DistributedCoordinationIntegrationTest, ThreeNodesRegistration) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Register 3 nodes
    for (int i = 1; i <= 3; i++) {
        NodeInfo info;
        info.id = "node_" + std::to_string(i);
        info.hostname = "host_" + std::to_string(i);
        info.grpc_port = 50051;
        info.capture_interfaces = {"eth0"};
        info.version = "1.0.0";

        auto result = coordinator_->register_node(info);
        EXPECT_TRUE(result.is_ok());
    }

    auto registered = coordinator_->registered_nodes();
    EXPECT_EQ(registered.size(), 3);
}

// T036: Test 3-node connection
TEST_F(DistributedCoordinationIntegrationTest, ThreeNodesConnect) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Create and connect 3 nodes
    for (int i = 1; i <= 3; i++) {
        auto config = create_node_config("node_" + std::to_string(i), i);
        auto result = TestNode::create(config);
        EXPECT_TRUE(result.is_ok());

        auto node = std::move(result.unwrap());
        auto conn_result = node->connect();
        EXPECT_TRUE(conn_result.is_ok());

        test_nodes_.push_back(std::move(node));
    }

    // Verify all connected
    for (const auto& node : test_nodes_) {
        EXPECT_TRUE(node->is_connected());
    }
}

// T036: Test 3-node barrier synchronization
TEST_F(DistributedCoordinationIntegrationTest, ThreeNodesBarrierSync) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Create and connect 3 nodes
    for (int i = 1; i <= 3; i++) {
        auto config = create_node_config("node_" + std::to_string(i), i);
        auto result = TestNode::create(config);
        EXPECT_TRUE(result.is_ok());
        test_nodes_.push_back(std::move(result.unwrap()));
    }

    // Connect all nodes
    for (auto& node : test_nodes_) {
        auto result = node->connect();
        EXPECT_TRUE(result.is_ok());
    }

    // Register nodes with coordinator
    for (const auto& node : test_nodes_) {
        NodeInfo info;
        info.id = node->config().node_id;
        info.hostname = node->config().hostname;
        info.grpc_port = 50051;
        info.capture_interfaces = node->config().capture_interfaces;

        auto result = coordinator_->register_node(info);
        EXPECT_TRUE(result.is_ok());
    }

    // Create barrier
    auto barrier_result = coordinator_->create_barrier("test_barrier_1");
    EXPECT_TRUE(barrier_result.is_ok());

    // Wait at barrier from each node (simulated)
    for (auto& node : test_nodes_) {
        auto wait_result = node->wait_at_barrier("test_barrier_1", std::chrono::milliseconds(5000));
        EXPECT_TRUE(wait_result.is_ok());

        auto barrier_res = wait_result.unwrap();
        EXPECT_TRUE(barrier_res.proceed);
        EXPECT_GT(barrier_res.sync_timestamp_ns, 0);
    }
}

// T036: Test sequential barrier synchronization
TEST_F(DistributedCoordinationIntegrationTest, SequentialBarriers) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Create and connect 3 nodes
    for (int i = 1; i <= 3; i++) {
        auto config = create_node_config("node_" + std::to_string(i), i);
        auto result = TestNode::create(config);
        EXPECT_TRUE(result.is_ok());
        test_nodes_.push_back(std::move(result.unwrap()));
    }

    for (auto& node : test_nodes_) {
        node->connect();

        NodeInfo info;
        info.id = node->config().node_id;
        info.hostname = node->config().hostname;
        info.grpc_port = 50051;
        info.capture_interfaces = node->config().capture_interfaces;
        coordinator_->register_node(info);
    }

    // Create multiple barriers
    std::vector<std::string> barriers = {"setup_barrier", "capture_start_barrier",
                                         "test_execution_barrier", "capture_stop_barrier"};

    for (const auto& barrier_id : barriers) {
        auto barrier_result = coordinator_->create_barrier(barrier_id);
        EXPECT_TRUE(barrier_result.is_ok());
    }

    // Each node waits at each barrier in sequence
    for (const auto& barrier_id : barriers) {
        for (auto& node : test_nodes_) {
            auto wait_result = node->wait_at_barrier(barrier_id, std::chrono::milliseconds(2000));
            EXPECT_TRUE(wait_result.is_ok());
        }
    }
}

// T036: Test capture coordination across 3 nodes
TEST_F(DistributedCoordinationIntegrationTest, ThreeNodesCaptureCoordination) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Create and connect nodes
    for (int i = 1; i <= 3; i++) {
        auto config = create_node_config("node_" + std::to_string(i), i);
        auto result = TestNode::create(config);
        EXPECT_TRUE(result.is_ok());
        test_nodes_.push_back(std::move(result.unwrap()));
    }

    for (auto& node : test_nodes_) {
        node->connect();

        NodeInfo info;
        info.id = node->config().node_id;
        info.hostname = node->config().hostname;
        info.grpc_port = 50051;
        info.capture_interfaces = node->config().capture_interfaces;
        coordinator_->register_node(info);
    }

    // Coordinate capture across nodes
    // 1. Create capture start barrier
    auto barrier_result = coordinator_->create_barrier("capture_start");
    EXPECT_TRUE(barrier_result.is_ok());

    // 2. Start capture on all nodes
    CaptureConfig capture_config;
    capture_config.filter_expression = "tcp";
    capture_config.snaplen = 65535;

    for (auto& node : test_nodes_) {
        auto result = node->start_capture(capture_config);
        EXPECT_TRUE(result.is_ok());
    }

    // 3. Wait at barrier to ensure synchronized capture start
    for (auto& node : test_nodes_) {
        auto wait_result = node->wait_at_barrier("capture_start", std::chrono::milliseconds(2000));
        EXPECT_TRUE(wait_result.is_ok());
    }

    // 4. Stop capture on all nodes
    std::vector<NodeCaptureResult> capture_results;
    for (auto& node : test_nodes_) {
        auto result = node->stop_capture();
        EXPECT_TRUE(result.is_ok());
        capture_results.push_back(result.unwrap());
    }

    // 5. Verify capture results
    EXPECT_EQ(capture_results.size(), 3);
    for (const auto& cap_result : capture_results) {
        EXPECT_FALSE(cap_result.node_id.empty());
        EXPECT_GT(cap_result.start_time_ns, 0);
        EXPECT_GT(cap_result.end_time_ns, cap_result.start_time_ns);
        EXPECT_TRUE(cap_result.success);
    }
}

// T036: Test wait_for_nodes with all nodes online
TEST_F(DistributedCoordinationIntegrationTest, WaitForAllNodesOnline) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Register 3 nodes
    std::vector<NodeId> node_ids = {"node_1", "node_2", "node_3"};
    for (const auto& node_id : node_ids) {
        NodeInfo info;
        info.id = node_id;
        info.hostname = "host_" + node_id;
        info.grpc_port = 50051;
        info.capture_interfaces = {"eth0"};

        coordinator_->register_node(info);
    }

    // Wait for nodes to be online
    auto result = coordinator_->wait_for_nodes(node_ids, std::chrono::milliseconds(1000));
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), 3);
}

// T036: Test node failure detection
TEST_F(DistributedCoordinationIntegrationTest, NodeFailureDetection) {
    CoordinatorConfig config;
    config.heartbeat_timeout = std::chrono::milliseconds(200);
    config.heartbeat_interval = std::chrono::milliseconds(50);

    auto coord_result = TestCoordinator::create(config);
    EXPECT_TRUE(coord_result.is_ok());
    auto local_coordinator = std::move(coord_result.unwrap());

    EXPECT_TRUE(local_coordinator->start().is_ok());

    // Register a node
    NodeInfo node1;
    node1.id = "node_1";
    node1.hostname = "host_1";
    node1.grpc_port = 50051;
    node1.capture_interfaces = {"eth0"};

    auto reg_result = local_coordinator->register_node(node1);
    EXPECT_TRUE(reg_result.is_ok());

    // Node is online
    auto online1 = local_coordinator->online_nodes();
    EXPECT_EQ(online1.size(), 1);

    // Wait for heartbeat timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Node should be detected as offline
    auto online2 = local_coordinator->online_nodes();
    EXPECT_EQ(online2.size(), 0);

    local_coordinator->stop();
}

// T036: Test concurrent node operations
TEST_F(DistributedCoordinationIntegrationTest, ConcurrentNodeOperations) {
    EXPECT_TRUE(coordinator_->start().is_ok());

    // Create 3 nodes
    for (int i = 1; i <= 3; i++) {
        auto config = create_node_config("node_" + std::to_string(i), i);
        auto result = TestNode::create(config);
        EXPECT_TRUE(result.is_ok());
        test_nodes_.push_back(std::move(result.unwrap()));
    }

    // Connect nodes in parallel
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([this, i]() { test_nodes_[i]->connect(); });
    }

    // Wait for all connections
    for (auto& t : threads) {
        t.join();
    }

    // Verify all connected
    for (const auto& node : test_nodes_) {
        EXPECT_TRUE(node->is_connected());
    }
}

}  // namespace wadjet::distributed

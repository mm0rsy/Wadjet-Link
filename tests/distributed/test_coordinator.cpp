#include <gtest/gtest.h>
#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/types.hpp"

#include <thread>
#include <chrono>

namespace wadjet::distributed {

class TestCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        CoordinatorConfig config;
        config.grpc_port = 50051;
        config.heartbeat_timeout = std::chrono::milliseconds(1000);
        config.heartbeat_interval = std::chrono::milliseconds(100);
        config.max_nodes = 10;
        
        auto result = TestCoordinator::create(config);
        ASSERT_TRUE(result.is_ok());
        coordinator_ = std::move(result.unwrap());
        ASSERT_TRUE(coordinator_);
    }
    
    void TearDown() override {
        if (coordinator_ && coordinator_->is_running()) {
            coordinator_->stop();
        }
    }
    
    std::unique_ptr<TestCoordinator> coordinator_;
    
    NodeInfo create_test_node(const std::string& id) {
        NodeInfo info;
        info.id = id;
        info.hostname = "test_host_" + id;
        info.grpc_port = 50051;
        info.capture_interfaces = {"eth0"};
        info.version = "1.0.0";
        return info;
    }
};

// T034: Unit test for TestCoordinator factory
TEST_F(TestCoordinatorTest, CreateReturnsValidCoordinator) {
    EXPECT_TRUE(coordinator_);
    EXPECT_FALSE(coordinator_->is_running());
}

// T034: Test start/stop
TEST_F(TestCoordinatorTest, StartAndStop) {
    auto result = coordinator_->start();
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(coordinator_->is_running());
    
    coordinator_->stop();
    EXPECT_FALSE(coordinator_->is_running());
}

// T034: Test node registration
TEST_F(TestCoordinatorTest, RegisterNodeSuccess) {
    auto result = coordinator_->start();
    EXPECT_TRUE(result.is_ok());
    
    NodeInfo node1 = create_test_node("node1");
    auto reg_result = coordinator_->register_node(node1);
    EXPECT_TRUE(reg_result.is_ok());
    
    auto registered = coordinator_->registered_nodes();
    EXPECT_EQ(registered.size(), 1);
    EXPECT_EQ(registered[0], "node1");
}

// T034: Test duplicate registration
TEST_F(TestCoordinatorTest, RegisterDuplicateNodeFails) {
    coordinator_->start();
    
    NodeInfo node1 = create_test_node("node1");
    coordinator_->register_node(node1);
    
    auto result = coordinator_->register_node(node1);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, "DUPLICATE_NODE");
}

// T034: Test invalid node info
TEST_F(TestCoordinatorTest, RegisterInvalidNodeFails) {
    coordinator_->start();
    
    NodeInfo invalid_node;
    invalid_node.id = "";  // Invalid: empty ID
    invalid_node.hostname = "test";
    invalid_node.grpc_port = 50051;
    
    auto result = coordinator_->register_node(invalid_node);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, "INVALID_NODE");
}

// T034: Test unregister node
TEST_F(TestCoordinatorTest, UnregisterNodeSuccess) {
    coordinator_->start();
    
    NodeInfo node1 = create_test_node("node1");
    coordinator_->register_node(node1);
    
    auto registered = coordinator_->registered_nodes();
    EXPECT_EQ(registered.size(), 1);
    
    auto unreg_result = coordinator_->unregister_node("node1");
    EXPECT_TRUE(unreg_result.is_ok());
    
    registered = coordinator_->registered_nodes();
    EXPECT_EQ(registered.size(), 0);
}

// T034: Test unregister non-existent node
TEST_F(TestCoordinatorTest, UnregisterNonExistentNodeFails) {
    coordinator_->start();
    
    auto result = coordinator_->unregister_node("nonexistent");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, "NOT_FOUND");
}

// T034: Test multiple nodes
TEST_F(TestCoordinatorTest, RegisterMultipleNodes) {
    coordinator_->start();
    
    for (int i = 1; i <= 3; i++) {
        NodeInfo node = create_test_node("node" + std::to_string(i));
        auto result = coordinator_->register_node(node);
        EXPECT_TRUE(result.is_ok());
    }
    
    auto registered = coordinator_->registered_nodes();
    EXPECT_EQ(registered.size(), 3);
}

// T034: Test max nodes limit
TEST_F(TestCoordinatorTest, MaxNodesLimit) {
    CoordinatorConfig config;
    config.max_nodes = 2;
    
    auto result = TestCoordinator::create(config);
    EXPECT_TRUE(result.is_ok());
    auto limited_coordinator = std::move(result.unwrap());
    limited_coordinator->start();
    
    // Register max nodes
    for (int i = 1; i <= 2; i++) {
        NodeInfo node = create_test_node("node" + std::to_string(i));
        auto reg_result = limited_coordinator->register_node(node);
        EXPECT_TRUE(reg_result.is_ok());
    }
    
    // Try to register one more - should fail
    NodeInfo extra_node = create_test_node("node_extra");
    auto reg_result = limited_coordinator->register_node(extra_node);
    EXPECT_TRUE(reg_result.is_err());
    EXPECT_EQ(reg_result.unwrap_err().code, "MAX_NODES_EXCEEDED");
}

// T034: Test get node info
TEST_F(TestCoordinatorTest, GetNodeInfo) {
    coordinator_->start();
    
    NodeInfo node1 = create_test_node("node1");
    coordinator_->register_node(node1);
    
    auto result = coordinator_->get_node_info("node1");
    EXPECT_TRUE(result.is_ok());
    
    auto info = result.unwrap();
    EXPECT_EQ(info.id, "node1");
    EXPECT_EQ(info.hostname, "test_host_node1");
}

// T034: Test get non-existent node info
TEST_F(TestCoordinatorTest, GetNonExistentNodeInfoFails) {
    coordinator_->start();
    
    auto result = coordinator_->get_node_info("nonexistent");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, "NOT_FOUND");
}

// T034: Test barrier creation
TEST_F(TestCoordinatorTest, CreateBarrier) {
    coordinator_->start();
    
    auto result = coordinator_->create_barrier("barrier1");
    EXPECT_TRUE(result.is_ok());
    
    auto barrier = std::move(result.unwrap());
    EXPECT_TRUE(barrier);
    EXPECT_EQ(barrier->barrier_id(), "barrier1");
}

// T034: Test duplicate barrier
TEST_F(TestCoordinatorTest, CreateDuplicateBarrierFails) {
    coordinator_->start();
    
    auto result1 = coordinator_->create_barrier("barrier1");
    EXPECT_TRUE(result1.is_ok());
    
    auto result2 = coordinator_->create_barrier("barrier1");
    EXPECT_TRUE(result2.is_err());
    EXPECT_EQ(result2.unwrap_err().code, "DUPLICATE_BARRIER");
}

// T034: Test online nodes tracking
TEST_F(TestCoordinatorTest, OnlineNodesTracking) {
    coordinator_->start();
    
    NodeInfo node1 = create_test_node("node1");
    coordinator_->register_node(node1);
    
    auto online = coordinator_->online_nodes();
    EXPECT_EQ(online.size(), 1);
    EXPECT_EQ(online[0], "node1");
}

// T034: Test callback on node status change
TEST_F(TestCoordinatorTest, NodeStatusCallback) {
    bool callback_called = false;
    std::string callback_node_id;
    bool callback_online;
    
    coordinator_->on_node_status_changed([&](const NodeId& id, bool is_online) {
        callback_called = true;
        callback_node_id = id;
        callback_online = is_online;
    });
    
    coordinator_->start();
    
    NodeInfo node1 = create_test_node("node1");
    coordinator_->register_node(node1);
    
    // Give callback time to be called
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_node_id, "node1");
    EXPECT_TRUE(callback_online);
}

// T034: Test wait for nodes
TEST_F(TestCoordinatorTest, WaitForNodesSingleNode) {
    coordinator_->start();
    
    NodeInfo node1 = create_test_node("node1");
    coordinator_->register_node(node1);
    
    std::vector<NodeId> expected = {"node1"};
    auto result = coordinator_->wait_for_nodes(expected, std::chrono::milliseconds(500));
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), 1);
}

// T034: Test wait for nodes timeout
TEST_F(TestCoordinatorTest, WaitForNodesTimeout) {
    coordinator_->start();
    
    std::vector<NodeId> expected = {"nonexistent"};
    auto result = coordinator_->wait_for_nodes(expected, std::chrono::milliseconds(100));
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, "TIMEOUT");
}

}  // namespace wadjet::distributed

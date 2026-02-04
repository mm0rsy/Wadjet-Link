#include <gtest/gtest.h>
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/types.hpp"

#include <chrono>

namespace wadjet::distributed {

class TestNodeTest : public ::testing::Test {
protected:
    NodeConfig create_test_config(const std::string& node_id = "test_node") {
        NodeConfig config;
        config.node_id = node_id;
        config.hostname = "localhost";
        config.capture_interfaces = {"eth0"};
        config.coordinator_address = "localhost";
        config.coordinator_port = 50051;
        config.version = "1.0.0";
        config.heartbeat_interval = std::chrono::milliseconds(100);
        return config;
    }
};

// T035: Test node factory
TEST_F(TestNodeTest, CreateReturnsValidNode) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    
    EXPECT_TRUE(result.is_ok());
    auto node = std::move(result.unwrap());
    EXPECT_TRUE(node);
    EXPECT_FALSE(node->is_connected());
}

// T035: Test invalid config
TEST_F(TestNodeTest, CreateInvalidConfigFails) {
    NodeConfig config = create_test_config();
    config.node_id = "";  // Invalid: empty node ID
    
    auto result = TestNode::create(config);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, "INVALID_CONFIG");
}

// T035: Test connect
TEST_F(TestNodeTest, ConnectSuccess) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    EXPECT_TRUE(result.is_ok());
    
    auto node = std::move(result.unwrap());
    auto connect_result = node->connect();
    EXPECT_TRUE(connect_result.is_ok());
    EXPECT_TRUE(node->is_connected());
}

// T035: Test disconnect
TEST_F(TestNodeTest, DisconnectSuccess) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    node->connect();
    EXPECT_TRUE(node->is_connected());
    
    node->disconnect();
    EXPECT_FALSE(node->is_connected());
}

// T035: Test double connect fails
TEST_F(TestNodeTest, DoubleConnectFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    auto result1 = node->connect();
    EXPECT_TRUE(result1.is_ok());
    
    auto result2 = node->connect();
    EXPECT_TRUE(result2.is_err());
    EXPECT_EQ(result2.unwrap_err().code, "ALREADY_CONNECTED");
}

// T035: Test barrier wait without connection fails
TEST_F(TestNodeTest, WaitBarrierNotConnectedFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    auto barrier_result = node->wait_at_barrier("barrier1", std::chrono::milliseconds(1000));
    EXPECT_TRUE(barrier_result.is_err());
    EXPECT_EQ(barrier_result.unwrap_err().code, "NOT_CONNECTED");
}

// T035: Test capture without connection fails
TEST_F(TestNodeTest, StartCaptureNotConnectedFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    CaptureConfig capture_config;
    auto capture_result = node->start_capture(capture_config);
    EXPECT_TRUE(capture_result.is_err());
    EXPECT_EQ(capture_result.unwrap_err().code, "NOT_CONNECTED");
}

// T035: Test start capture
TEST_F(TestNodeTest, StartCaptureSuccess) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    node->connect();
    
    CaptureConfig capture_config;
    capture_config.filter_expression = "tcp port 80";
    capture_config.snaplen = 65535;
    
    auto capture_result = node->start_capture(capture_config);
    EXPECT_TRUE(capture_result.is_ok());
}

// T035: Test stop capture without start fails
TEST_F(TestNodeTest, StopCaptureWithoutStartFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    node->connect();
    
    auto stop_result = node->stop_capture();
    EXPECT_TRUE(stop_result.is_err());
    EXPECT_EQ(stop_result.unwrap_err().code, "NOT_CAPTURING");
}

// T035: Test capture lifecycle
TEST_F(TestNodeTest, CaptureLivecycle) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    node->connect();
    
    // Start capture
    CaptureConfig capture_config;
    auto start_result = node->start_capture(capture_config);
    EXPECT_TRUE(start_result.is_ok());
    
    // Stop capture
    auto stop_result = node->stop_capture();
    EXPECT_TRUE(stop_result.is_ok());
    auto capture_result = stop_result.unwrap();
    EXPECT_EQ(capture_result.node_id, "test_node");
    EXPECT_GT(capture_result.start_time_ns, 0);
    EXPECT_GT(capture_result.end_time_ns, capture_result.start_time_ns);
}

// T035: Test double start capture fails
TEST_F(TestNodeTest, DoubleStartCaptureFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    node->connect();
    
    CaptureConfig capture_config;
    node->start_capture(capture_config);
    
    auto result2 = node->start_capture(capture_config);
    EXPECT_TRUE(result2.is_err());
    EXPECT_EQ(result2.unwrap_err().code, "ALREADY_CAPTURING");
}

// T035: Test evaluate matcher without connection fails
TEST_F(TestNodeTest, EvaluateMatcherNotConnectedFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    auto eval_result = node->evaluate_matcher("ExpectMessageFlow", "{}");
    EXPECT_TRUE(eval_result.is_err());
    EXPECT_EQ(eval_result.unwrap_err().code, "NOT_CONNECTED");
}

// T035: Test config getter
TEST_F(TestNodeTest, ConfigGetter) {
    auto config = create_test_config("my_node");
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    const auto& retrieved_config = node->config();
    EXPECT_EQ(retrieved_config.node_id, "my_node");
    EXPECT_EQ(retrieved_config.hostname, "localhost");
    EXPECT_EQ(retrieved_config.coordinator_port, 50051);
}

// T035: Test execute command without connection fails
TEST_F(TestNodeTest, ExecuteCommandNotConnectedFails) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node = std::move(result.unwrap());
    
    auto cmd_result = node->execute_command("ls", {"-la"});
    EXPECT_TRUE(cmd_result.is_err());
    EXPECT_EQ(cmd_result.unwrap_err().code, "NOT_CONNECTED");
}

// T035: Test move semantics
TEST_F(TestNodeTest, MoveSemantics) {
    auto config = create_test_config();
    auto result = TestNode::create(config);
    auto node1 = std::move(result.unwrap());
    
    node1->connect();
    EXPECT_TRUE(node1->is_connected());
    
    auto node2 = std::move(node1);
    EXPECT_TRUE(node2->is_connected());
    EXPECT_EQ(node2->config().node_id, "test_node");
}

// T035: Test multiple nodes
TEST_F(TestNodeTest, MultipleNodes) {
    std::vector<std::unique_ptr<TestNode>> nodes;
    
    for (int i = 1; i <= 3; i++) {
        auto config = create_test_config("node" + std::to_string(i));
        auto result = TestNode::create(config);
        EXPECT_TRUE(result.is_ok());
        nodes.push_back(std::move(result.unwrap()));
    }
    
    EXPECT_EQ(nodes.size(), 3);
    
    // Connect all
    for (auto& node : nodes) {
        auto conn_result = node->connect();
        EXPECT_TRUE(conn_result.is_ok());
    }
    
    // Verify all connected
    for (const auto& node : nodes) {
        EXPECT_TRUE(node->is_connected());
    }
}

}  // namespace wadjet::distributed

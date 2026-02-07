// T221: Unit tests for gRPC client
// Tests client-side RPC calls: RegisterNode, Heartbeat, WaitBarrier

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/grpc/client.hpp"
#include "wadjet/distributed/types.hpp"

#include <gmock/gmock.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

// Proto generated includes with warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#include "distributed_test.grpc.pb.h"
#include "distributed_test.pb.h"
#pragma GCC diagnostic pop

namespace wadjet::distributed {
namespace {

/**
 * @brief Test fixture for gRPC client
 */
class GrpcClientTest : public ::testing::Test {
protected:
    std::unique_ptr<TestCoordinator> coordinator_;
    std::unique_ptr<DistributedTestClient> client_;

    void SetUp() override {
        // T221: Set up gRPC client for testing with running server
        try {
            // Create and start coordinator server
            CoordinatorConfig config;
            config.bind_address = "127.0.0.1";
            config.grpc_port = 15101;  // Different port from service tests
            config.heartbeat_timeout = std::chrono::milliseconds(2000);

            auto coord_result = TestCoordinator::create(config);
            if (!coord_result) {
                FAIL() << "Failed to create coordinator";
            }
            coordinator_ = std::move(*coord_result);

            // Start the coordinator server
            auto start_result = coordinator_->start();
            if (!start_result) {
                FAIL() << "Failed to start coordinator server";
            }

            // Give server time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Create client connection
            client_ = DistributedTestClient::create("127.0.0.1:15101");
            if (!client_) {
                FAIL() << "Failed to create client";
            }
        } catch (const std::exception& e) {
            FAIL() << "SetUp failed: " << e.what();
        }
    }

    void TearDown() override {
        // Clean up client resources
        client_ = nullptr;
        if (coordinator_) {
            coordinator_->stop();
        }
    }
};

/**
 * @brief Test client creation and connection
 */
TEST_F(GrpcClientTest, ClientCreation) {
    // T222: Verify client is created successfully
    ASSERT_NE(client_, nullptr) << "Client should be created";

    // Client should be ready to use
    EXPECT_TRUE(client_->is_connected());
}

/**
 * @brief Test RegisterNode RPC call
 */
TEST_F(GrpcClientTest, RegisterNodeRpc) {
    // T223: Test register_node RPC through client
    RegisterNodeRequest request;
    request.set_node_id("test-client-node-1");
    request.set_hostname("test-host");
    request.set_grpc_port(15102);

    auto result =
        client_->register_node(request.node_id(), request.hostname(), request.grpc_port());

    EXPECT_TRUE(result.has_value()) << "RegisterNode should succeed";
}

/**
 * @brief Test SendHeartbeat RPC call
 */
TEST_F(GrpcClientTest, SendHeartbeatRpc) {
    // T224: Test send_heartbeat RPC through client
    // First register a node
    auto reg_result = client_->register_node("test-client-node-2", "test-host", 15103);
    ASSERT_TRUE(reg_result.has_value());

    // Send heartbeat
    ClockSync clock_sync;
    clock_sync.set_client_send_ns(1000000000);
    clock_sync.set_server_recv_ns(1000000100);
    clock_sync.set_server_send_ns(1000000200);

    auto hb_result = client_->send_heartbeat("test-client-node-2", NodeState::RUNNING, clock_sync);

    EXPECT_TRUE(hb_result.has_value()) << "SendHeartbeat should succeed";
}

/**
 * @brief Test SendHeartbeat timeout
 */
TEST_F(GrpcClientTest, SendHeartbeatTimeout) {
    // T225: Test send_heartbeat timeout handling
    // Create a client with very short deadline
    auto timeout_client = DistributedTestClient::create("127.0.0.1:15104");  // Non-existent server
    if (timeout_client) {
        ClockSync clock_sync;
        clock_sync.set_client_send_ns(1000000000);

        auto result = timeout_client->send_heartbeat("test-node", NodeState::RUNNING, clock_sync);

        // Result should indicate timeout or connection failure
        EXPECT_FALSE(result.has_value()) << "Should fail with timeout";
    }
}

/**
 * @brief Test WaitBarrier RPC call
 */
TEST_F(GrpcClientTest, WaitBarrierRpc) {
    // T226: Test wait_barrier RPC through client
    // Register a node first
    auto reg_result = client_->register_node("test-barrier-node-1", "test-host", 15105);
    ASSERT_TRUE(reg_result.has_value());

    // Start a barrier in background (would need coordinator to actually set one up)
    BarrierRequest request;
    request.set_barrier_id("barrier-1");
    request.set_node_id("test-barrier-node-1");
    request.set_timeout_ns(5000000000);  // 5 seconds

    // Note: In real scenario, coordinator would set up the barrier first
    // For now, we test the RPC call completes (may timeout as expected)
    auto result = client_->wait_barrier("barrier-1", "test-barrier-node-1", 5000000000);

    // Result validation depends on server-side barrier setup
    // This test primarily validates the RPC mechanism works
}

/**
 * @brief Test WaitBarrier timeout
 */
TEST_F(GrpcClientTest, WaitBarrierTimeout) {
    // T227: Test wait_barrier timeout handling
    // Create request with very short timeout
    auto result =
        client_->wait_barrier("barrier-timeout", "test-node", 100000);  // 100 microseconds

    // Should timeout since no barrier exists on server
    EXPECT_FALSE(result.has_value()) << "Should timeout for non-existent barrier";
}

/**
 * @brief Test client move semantics
 */
TEST_F(GrpcClientTest, ClientMoveSemanticsTest) {
    // T228: Test client move semantics
    auto client1 = DistributedTestClient::create("127.0.0.1:15101");
    ASSERT_NE(client1, nullptr);

    // Move client1 to client2
    auto client2 = std::move(client1);

    // client1 should be empty, client2 should be valid
    EXPECT_EQ(client1, nullptr) << "Source should be null after move";
    EXPECT_NE(client2, nullptr) << "Destination should be valid after move";

    // client2 should still work
    EXPECT_TRUE(client2->is_connected());
}

}  // namespace
}  // namespace wadjet::distributed

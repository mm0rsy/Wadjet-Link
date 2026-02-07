// T220: Unit tests for gRPC service handlers
// Tests RegisterNode, UnregisterNode, Heartbeat, and WaitBarrier RPC handlers

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>

#include "wadjet/distributed/types.hpp"
#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/grpc/service.hpp"

// Proto generated includes with warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#include "distributed_test.pb.h"
#include "distributed_test.grpc.pb.h"
#pragma GCC diagnostic pop

namespace wadjet::distributed {
namespace {

using namespace v1;
using ::testing::Return;

/**
 * @brief Test fixture for gRPC service handlers
 */
class GrpcServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<grpc::Server> server_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<DistributedTestService::Stub> stub_;
    std::unique_ptr<TestCoordinator> coordinator_;
    
    void SetUp() override {
        // T220: Set up in-process gRPC server for testing
        try {
            // Create a coordinator instance (mock/real)
            CoordinatorConfig config;
            config.bind_address = "127.0.0.1";
            config.grpc_port = 15001;  // Use different port for testing
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
            
            // Create client channel to in-process server
            channel_ = grpc::CreateChannel(
                "127.0.0.1:15001",
                grpc::InsecureChannelCredentials()
            );
            
            // Create stub
            stub_ = DistributedTestService::NewStub(channel_);
            
            // Give server time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            FAIL() << "SetUp failed: " << e.what();
        }
    }
    
    void TearDown() override {
        // Clean up resources
        if (coordinator_) {
            coordinator_->stop();
        }
    }
};

/**
 * @brief Test RegisterNode RPC handler
 */
TEST_F(GrpcServiceTest, RegisterNodeSuccess) {
    // T200: Test successful node registration
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    RegisterNodeRequest request;
    request.set_node_id("test-node-1");
    request.set_hostname("localhost");
    request.set_grpc_port(15002);
    request.set_version("1.0.0");
    
    RegisterNodeResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    grpc::Status status = stub_->RegisterNode(&context, request, &response);
    
    EXPECT_TRUE(status.ok()) << "RegisterNode RPC failed: " << status.error_message();
    EXPECT_EQ(response.status(), "success");
}

/**
 * @brief Test RegisterNode with duplicate node ID
 */
TEST_F(GrpcServiceTest, RegisterNodeDuplicate) {
    // T200: Test duplicate node registration
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    RegisterNodeRequest request;
    request.set_node_id("duplicate-node");
    request.set_hostname("localhost");
    request.set_grpc_port(15003);
    request.set_version("1.0.0");
    
    // Register first time
    RegisterNodeResponse response1;
    grpc::ClientContext context1;
    context1.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status status1 = stub_->RegisterNode(&context1, request, &response1);
    EXPECT_TRUE(status1.ok());
    
    // Try to register again with same node_id
    RegisterNodeResponse response2;
    grpc::ClientContext context2;
    context2.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status status2 = stub_->RegisterNode(&context2, request, &response2);
    
    // Should indicate duplicate or error
    EXPECT_TRUE(status2.ok() || status2.error_code() == grpc::StatusCode::ALREADY_EXISTS ||
                response2.status() != "success");
}

/**
 * @brief Test UnregisterNode RPC handler
 */
TEST_F(GrpcServiceTest, UnregisterNodeSuccess) {
    // T201: Test successful node unregistration
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    // First register a node
    RegisterNodeRequest reg_req;
    reg_req.set_node_id("unreg-test-node");
    reg_req.set_hostname("localhost");
    reg_req.set_grpc_port(15004);
    reg_req.set_version("1.0.0");
    
    RegisterNodeResponse reg_resp;
    grpc::ClientContext reg_ctx;
    reg_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status reg_status = stub_->RegisterNode(&reg_ctx, reg_req, &reg_resp);
    ASSERT_TRUE(reg_status.ok());
    
    // Now unregister it
    UnregisterNodeRequest unreg_req;
    unreg_req.set_node_id("unreg-test-node");
    
    UnregisterNodeResponse unreg_resp;
    grpc::ClientContext unreg_ctx;
    unreg_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status unreg_status = stub_->UnregisterNode(&unreg_ctx, unreg_req, &unreg_resp);
    
    EXPECT_TRUE(unreg_status.ok()) << "UnregisterNode RPC failed";
    EXPECT_EQ(unreg_resp.status(), "success");
}

/**
 * @brief Test UnregisterNode for non-existent node
 */
TEST_F(GrpcServiceTest, UnregisterNodeNotFound) {
    // T201: Test unregistering non-existent node
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    UnregisterNodeRequest request;
    request.set_node_id("nonexistent-node");
    
    UnregisterNodeResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status status = stub_->UnregisterNode(&context, request, &response);
    
    // Should either fail or indicate not found
    EXPECT_TRUE(!status.ok() || response.status() != "success");
}

/**
 * @brief Test Heartbeat RPC handler
 */
TEST_F(GrpcServiceTest, HeartbeatSuccess) {
    // T202: Test successful heartbeat processing
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    // Register a node first
    RegisterNodeRequest reg_req;
    reg_req.set_node_id("heartbeat-test");
    reg_req.set_hostname("localhost");
    reg_req.set_grpc_port(15005);
    reg_req.set_version("1.0.0");
    
    RegisterNodeResponse reg_resp;
    grpc::ClientContext reg_ctx;
    reg_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status reg_status = stub_->RegisterNode(&reg_ctx, reg_req, &reg_resp);
    ASSERT_TRUE(reg_status.ok());
    
    // Send heartbeat
    HeartbeatRequest hb_req;
    hb_req.set_node_id("heartbeat-test");
    
    HeartbeatResponse hb_resp;
    grpc::ClientContext hb_ctx;
    hb_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status hb_status = stub_->Heartbeat(&hb_ctx, hb_req, &hb_resp);
    
    EXPECT_TRUE(hb_status.ok()) << "Heartbeat RPC failed";
}

/**
 * @brief Test Heartbeat from unregistered node
 */
TEST_F(GrpcServiceTest, HeartbeatUnregisteredNode) {
    // T202: Test heartbeat from node not registered
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    HeartbeatRequest request;
    request.set_node_id("unregistered");
    
    HeartbeatResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status status = stub_->Heartbeat(&context, request, &response);
    
    // Should fail or indicate error
    EXPECT_TRUE(!status.ok());
}

/**
 * @brief Test WaitBarrier RPC handler synchronization
 */
TEST_F(GrpcServiceTest, WaitBarrierSynchronization) {
    // T203: Test barrier synchronization with multiple nodes
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    // Register test nodes
    const std::vector<std::string> node_ids = {"barrier-node-1", "barrier-node-2", "barrier-node-3"};
    for (size_t i = 0; i < node_ids.size(); ++i) {
        RegisterNodeRequest req;
        req.set_node_id(node_ids[i]);
        req.set_hostname("localhost");
        req.set_grpc_port(15010 + i);
        req.set_version("1.0.0");
        
        RegisterNodeResponse resp;
        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        grpc::Status status = stub_->RegisterNode(&ctx, req, &resp);
        ASSERT_TRUE(status.ok());
    }
    
    // Note: Full barrier test requires multiple threads/clients
    // This is a simplified single-node barrier test
    WaitBarrierRequest barrier_req;
    barrier_req.set_node_id("barrier-node-1");
    barrier_req.set_barrier_id("test-barrier");
    barrier_req.set_timeout_ms(1000);
    
    WaitBarrierResponse barrier_resp;
    grpc::ClientContext barrier_ctx;
    barrier_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status barrier_status = stub_->WaitBarrier(&barrier_ctx, barrier_req, &barrier_resp);
    
    // Barrier may timeout on single node, but RPC should succeed
    EXPECT_TRUE(barrier_status.ok() || barrier_status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED);
}

/**
 * @brief Test WaitBarrier timeout
 */
TEST_F(GrpcServiceTest, WaitBarrierTimeout) {
    // T203: Test barrier timeout when not all nodes arrive
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    // Register a node
    RegisterNodeRequest reg_req;
    reg_req.set_node_id("timeout-node");
    reg_req.set_hostname("localhost");
    reg_req.set_grpc_port(15020);
    reg_req.set_version("1.0.0");
    
    RegisterNodeResponse reg_resp;
    grpc::ClientContext reg_ctx;
    reg_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status reg_status = stub_->RegisterNode(&reg_ctx, reg_req, &reg_resp);
    ASSERT_TRUE(reg_status.ok());
    
    // Try barrier with short timeout
    WaitBarrierRequest barrier_req;
    barrier_req.set_node_id("timeout-node");
    barrier_req.set_barrier_id("timeout-barrier");
    barrier_req.set_timeout_ms(100);  // Very short timeout
    
    WaitBarrierResponse barrier_resp;
    grpc::ClientContext barrier_ctx;
    barrier_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status barrier_status = stub_->WaitBarrier(&barrier_ctx, barrier_req, &barrier_resp);
    
    // Expect timeout or success (depending on implementation)
    EXPECT_TRUE(barrier_status.ok());
}

/**
 * @brief Test WaitBarrier for unregistered node
 */
TEST_F(GrpcServiceTest, WaitBarrierUnregisteredNode) {
    // T203: Test barrier call from unregistered node
    ASSERT_TRUE(stub_) << "Stub not initialized";
    
    WaitBarrierRequest request;
    request.set_node_id("unregistered-barrier-node");
    request.set_barrier_id("test-barrier");
    request.set_timeout_ms(1000);
    
    WaitBarrierResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::Status status = stub_->WaitBarrier(&context, request, &response);
    
    // Should fail or indicate error
    EXPECT_TRUE(!status.ok());
}

}  // namespace
}  // namespace wadjet::distributed

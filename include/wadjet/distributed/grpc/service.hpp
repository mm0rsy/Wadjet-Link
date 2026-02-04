#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>

// Forward declarations - generated proto
namespace wadjet::distributed::proto {
    class DistributedTestService;
    class RegisterNodeRequest;
    class RegisterNodeResponse;
    class UnregisterNodeRequest;
    class UnregisterNodeResponse;
    class HeartbeatRequest;
    class HeartbeatResponse;
    class WaitBarrierRequest;
    class WaitBarrierResponse;
}

namespace wadjet::distributed {

class TestCoordinator;

/**
 * @brief gRPC service implementation for distributed testing
 * 
 * T022-T024: Implements the DistributedTestService gRPC interface
 * - Node registration and unregistration
 * - Heartbeat streaming for health monitoring
 * - Barrier synchronization
 */
class DistributedTestServiceImpl final
    : public proto::DistributedTestService::Service {
public:
    explicit DistributedTestServiceImpl(TestCoordinator* coordinator);
    
    // T023: RegisterNode RPC handler
    ::grpc::Status RegisterNode(
        ::grpc::ServerContext* context,
        const proto::RegisterNodeRequest* request,
        proto::RegisterNodeResponse* response) override;
    
    // T023: UnregisterNode RPC handler
    ::grpc::Status UnregisterNode(
        ::grpc::ServerContext* context,
        const proto::UnregisterNodeRequest* request,
        proto::UnregisterNodeResponse* response) override;
    
    // T024: Heartbeat streaming RPC handler
    ::grpc::Status Heartbeat(
        ::grpc::ServerContext* context,
        ::grpc::ServerReader<proto::HeartbeatRequest>* reader,
        proto::HeartbeatResponse* response) override;
    
    // T029: WaitBarrier RPC handler
    ::grpc::Status WaitBarrier(
        ::grpc::ServerContext* context,
        const proto::WaitBarrierRequest* request,
        proto::WaitBarrierResponse* response) override;
    
private:
    TestCoordinator* coordinator_;
};

}  // namespace wadjet::distributed

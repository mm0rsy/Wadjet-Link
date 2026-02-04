#pragma once

#include <memory>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <grpcpp/server.h>

// Proto generated includes with warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#include "distributed_test.grpc.pb.h"
#pragma GCC diagnostic pop

namespace wadjet::distributed {

class TestCoordinator;

/**
 * @brief gRPC service implementation for distributed testing
 * 
 * T200-T206: Implements the DistributedTestService gRPC interface
 * - Node registration and unregistration (T200, T201)
 * - Heartbeat streaming for health monitoring (T202)
 * - Barrier synchronization (T203)
 * - Control channel for coordinator commands (T204)
 * - Result reporting (T205)
 * - PCAP upload (T206)
 * 
 * Proto files compiled from proto/distributed_test.proto with grpc_cpp_plugin
 */
class DistributedTestServiceImpl : public v1::DistributedTestService::Service {
public:
    explicit DistributedTestServiceImpl(TestCoordinator* coordinator);
    
    ~DistributedTestServiceImpl() override;
    
    // T200: RegisterNode RPC handler
    grpc::Status RegisterNode(grpc::ServerContext* context,
                             const v1::RegisterNodeRequest* request,
                             v1::RegisterNodeResponse* response) override;
    
    // T201: UnregisterNode RPC handler
    grpc::Status UnregisterNode(grpc::ServerContext* context,
                               const v1::UnregisterNodeRequest* request,
                               v1::UnregisterNodeResponse* response) override;
    
    // T202: Heartbeat streaming RPC handler (bidirectional)
    grpc::Status Heartbeat(grpc::ServerContext* context,
                          grpc::ServerReaderWriter<v1::HeartbeatResponse, v1::HeartbeatRequest>* stream) override;
    
    // T203: WaitBarrier RPC handler
    grpc::Status WaitBarrier(grpc::ServerContext* context,
                            const v1::WaitBarrierRequest* request,
                            v1::WaitBarrierResponse* response) override;
    
    // T204: ControlChannel RPC handler (bidirectional streaming)
    grpc::Status ControlChannel(grpc::ServerContext* context,
                               grpc::ServerReaderWriter<v1::CoordinatorMessage, v1::NodeMessage>* stream) override;
    
    // T205: ReportResult RPC handler
    grpc::Status ReportResult(grpc::ServerContext* context,
                             const v1::ReportResultRequest* request,
                             v1::ReportResultResponse* response) override;
    
    // T206: UploadPcap RPC handler (client streaming)
    grpc::Status UploadPcap(grpc::ServerContext* context,
                           grpc::ServerReader<v1::PcapChunk>* reader,
                           v1::UploadPcapResponse* response) override;
    
private:
    TestCoordinator* coordinator_;
};

}  // namespace wadjet::distributed

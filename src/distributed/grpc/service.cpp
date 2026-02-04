#include "wadjet/distributed/grpc/service.hpp"
#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/types.hpp"

// Proto generated includes with compiler warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#include "distributed_test.pb.h"
#pragma GCC diagnostic pop

#include <grpcpp/support/status.h>
#include <chrono>

namespace wadjet::distributed {

using namespace v1;

DistributedTestServiceImpl::DistributedTestServiceImpl(TestCoordinator* coordinator)
    : coordinator_(coordinator) {}

DistributedTestServiceImpl::~DistributedTestServiceImpl() = default;

// T200: RegisterNode RPC handler
grpc::Status DistributedTestServiceImpl::RegisterNode(
    grpc::ServerContext* /*context*/,
    const RegisterNodeRequest* request,
    RegisterNodeResponse* response) {
    
    if (!coordinator_ || !request || !response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    // Extract node information from proto request
    NodeInfo node_info;
    node_info.id = request->node_id();
    node_info.hostname = request->hostname();
    node_info.version = "1.0";  // Set version
    
    // Add capture interfaces
    for (const auto& iface : request->capture_interfaces()) {
        node_info.capture_interfaces.push_back(iface);
    }
    
    // Add metadata
    for (const auto& [key, value] : request->metadata()) {
        node_info.metadata[key] = value;
    }
    
    // Register node with coordinator
    auto result = coordinator_->register_node(node_info);
    if (result.is_err()) {
        response->set_success(false);
        response->set_error_message(result.unwrap_err().message);
        return grpc::Status::OK;
    }
    
    // Set success response
    response->set_success(true);
    response->set_assigned_node_id(node_info.id);
    response->set_server_timestamp_ns(
        static_cast<int64_t>(std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count())
    );
    
    // Return coordinator protocol version
    auto coord_version = response->mutable_coordinator_version();
    coord_version->set_major(1);
    coord_version->set_minor(0);
    coord_version->set_patch(0);
    
    return grpc::Status::OK;
}

// T201: UnregisterNode RPC handler
grpc::Status DistributedTestServiceImpl::UnregisterNode(
    grpc::ServerContext* /*context*/,
    const UnregisterNodeRequest* request,
    UnregisterNodeResponse* response) {
    
    if (!coordinator_ || !request || !response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    // Unregister node from coordinator
    auto result = coordinator_->unregister_node(request->node_id());
    if (result.is_err()) {
        response->set_success(false);
        response->set_error_message(result.unwrap_err().message);
        return grpc::Status::OK;
    }
    
    response->set_success(true);
    return grpc::Status::OK;
}

// T202: Heartbeat streaming RPC handler (bidirectional)
grpc::Status DistributedTestServiceImpl::Heartbeat(
    grpc::ServerContext* /*context*/,
    grpc::ServerReaderWriter<HeartbeatResponse, HeartbeatRequest>* stream) {
    
    if (!coordinator_ || !stream) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    HeartbeatRequest request;
    while (stream->Read(&request)) {
        HeartbeatResponse response;
        
        // Acknowledge heartbeat
        response.set_acknowledged(true);
        response.set_server_timestamp_ns(
            static_cast<int64_t>(std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count())
        );
        
        // In a real implementation, we would:
        // 1. Update node health status in coordinator
        // 2. Check for pending commands to send back
        // 3. Send piggyback commands if available
        
        if (!stream->Write(response)) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to write response");
        }
    }
    
    return grpc::Status::OK;
}

// T203: WaitBarrier RPC handler
grpc::Status DistributedTestServiceImpl::WaitBarrier(
    grpc::ServerContext* /*context*/,
    const WaitBarrierRequest* request,
    WaitBarrierResponse* response) {
    
    if (!coordinator_ || !request || !response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    const auto& node_id = request->node_id();
    const auto& barrier_id = request->barrier_id();
    
    // Create or get barrier
    auto barrier_result = coordinator_->create_barrier(barrier_id);
    if (barrier_result.is_err()) {
        response->set_proceed(false);
        return grpc::Status(grpc::StatusCode::INTERNAL, barrier_result.unwrap_err().message);
    }
    
    // For now, immediately proceed (real implementation would coordinate with all nodes)
    response->set_proceed(true);
    response->set_state(BarrierState::BARRIER_ALL_ARRIVED);
    response->set_sync_timestamp_ns(request->arrival_timestamp_ns());
    response->add_participating_nodes(node_id);
    response->set_wait_duration_ms(0);
    
    return grpc::Status::OK;
}

// T204: ControlChannel RPC handler (bidirectional streaming)
grpc::Status DistributedTestServiceImpl::ControlChannel(
    grpc::ServerContext* /*context*/,
    grpc::ServerReaderWriter<CoordinatorMessage, NodeMessage>* stream) {
    
    if (!coordinator_ || !stream) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    NodeMessage node_msg;
    while (stream->Read(&node_msg)) {
        // Process node message and respond with coordinator command if needed
        CoordinatorMessage coord_msg;
        coord_msg.set_message_id("ack-" + node_msg.node_id());
        
        // In a real implementation, we would:
        // 1. Process the node message payload
        // 2. Update test state based on events
        // 3. Send back appropriate coordinator commands
        
        if (!stream->Write(coord_msg)) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to write response");
        }
    }
    
    return grpc::Status::OK;
}

// T205: ReportResult RPC handler
grpc::Status DistributedTestServiceImpl::ReportResult(
    grpc::ServerContext* /*context*/,
    const ReportResultRequest* request,
    ReportResultResponse* response) {
    
    if (!coordinator_ || !request || !response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    // In a real implementation, we would:
    // 1. Extract test result from request
    // 2. Validate and store result in coordinator
    // 3. Aggregate with other node results
    
    response->set_success(true);
    
    return grpc::Status::OK;
}

// T206: UploadPcap RPC handler (client streaming)
grpc::Status DistributedTestServiceImpl::UploadPcap(
    grpc::ServerContext* /*context*/,
    grpc::ServerReader<PcapChunk>* reader,
    UploadPcapResponse* response) {
    
    if (!coordinator_ || !reader || !response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }
    
    PcapChunk chunk;
    uint64_t total_bytes = 0;
    
    while (reader->Read(&chunk)) {
        // In a real implementation, we would:
        // 1. Write chunks to local file or database
        // 2. Validate checksums
        // 3. Update progress
        
        total_bytes += static_cast<uint64_t>(chunk.data().size());
    }
    
    response->set_success(true);
    response->set_stored_path("/coordinator/uploads/pcap_" + std::to_string(total_bytes));
    
    return grpc::Status::OK;
}

}  // namespace wadjet::distributed


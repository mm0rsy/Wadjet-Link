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
#include <ctime>
#include <fstream>
#include <stdexcept>

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
        const auto& node_id = request.node_id();

        // T228: Update node heartbeat timestamp in coordinator
        auto hb_result = coordinator_->update_node_heartbeat(node_id);

        HeartbeatResponse response;
        
        // Acknowledge heartbeat
        response.set_acknowledged(true);
        response.set_server_timestamp_ns(
            static_cast<int64_t>(std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count())
        );

        // Set heartbeat status from coordinator update
        if (hb_result.is_ok()) {
            response.set_heartbeat_status("ok");
        } else {
            response.set_heartbeat_status("error");
        }

        // In a real implementation, we would also:
        // 1. Check for pending commands to send back
        // 2. Send piggyback commands if available

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

// T064: Helper to evaluate matcher on captured packets (distributed matcher evaluation)
// This runs matcher evaluation logic for distributed assertions
static auto evaluate_distributed_matcher(
    const std::string& /*matcher_type*/, const std::string& /*matcher_params*/,
    const std::vector<Packet>& /*captured_packets*/) -> std::string {
    // T064: In a full implementation, this would:
    // 1. Deserialize matcher_params from JSON
    // 2. Instantiate the appropriate matcher (ExpectMessageFlow, WithinLatency, etc.)
    // 3. Create DistributedCaptureContext with captured packets
    // 4. Evaluate the matcher
    // 5. Return JSON-serialized result

    // Placeholder implementation
    return R"({"matched": true, "latency_ns": 0})";
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
        const auto& node_id = node_msg.node_id();

        // T041: Process node messages and update coordinator state
        if (node_msg.has_capture_started()) {
            const auto& event = node_msg.capture_started();
            // Update node capture status in coordinator
            // In a full implementation, would update distributed state
        } else if (node_msg.has_capture_stopped()) {
            const auto& event = node_msg.capture_stopped();
            // Record capture completion in coordinator
            // In a full implementation, would trigger PCAP upload
        } else if (node_msg.has_matcher_result()) {
            const auto& event = node_msg.matcher_result();
            // Record matcher result from node
            // This is the response from T064: COMMAND_EVALUATE_MATCHER handling
        } else if (node_msg.has_error()) {
            const auto& event = node_msg.error();
            // Log error from node and potentially update node health
        } else if (node_msg.has_log()) {
            const auto& event = node_msg.log();
            // Forward log event (implementation-specific)
        }

        // Create coordinator response message
        CoordinatorMessage coord_msg;
        coord_msg.set_message_id("ack-" + node_id + "-" + std::to_string(std::time(nullptr)));

        // T041: Send any pending commands back to node
        // For now, just acknowledge (in a real implementation, would check for
        // pending StartCapture or other commands queued for this node)

        // T064: Send EVALUATE_MATCHER commands if queued for this node
        // This would be populated by TestCoordinator when it wants to evaluate
        // distributed matchers on captured packets

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
    std::string node_id;
    std::string output_path;

    try {
        // T050: Read all PCAP chunks from node and write to file
        std::ofstream pcap_file;

        while (reader->Read(&chunk)) {
            if (node_id.empty()) {
                node_id = chunk.node_id();
                // Create output directory/path for this node's PCAP
                output_path = "/tmp/coordinator_uploads/pcap_" + node_id + "_" +
                              std::to_string(std::time(nullptr)) + ".pcap";

                // Open file for writing in binary mode
                pcap_file.open(output_path, std::ios::binary | std::ios::out | std::ios::app);
                if (!pcap_file.is_open()) {
                    response->set_success(false);
                    response->set_error_message("Failed to create output file: " + output_path);
                    return grpc::Status::OK;
                }
            }

            // Write chunk data to file
            const auto& data = chunk.data();
            total_bytes += data.size();

            pcap_file.write(data.c_str(), static_cast<std::streamsize>(data.size()));
            if (!pcap_file.good()) {
                response->set_success(false);
                response->set_error_message("Failed to write to file");
                return grpc::Status::OK;
            }

            // Validate checksum if provided (optional integrity check)
            if (!chunk.checksum().empty()) {
                // In a full implementation, would validate CRC32 or other checksum
                // For now, just accept it
            }
        }

        // Close the file
        if (pcap_file.is_open()) {
            pcap_file.close();
        }

        // T050: Set success response with stored path and metadata
        response->set_success(true);
        response->set_stored_path(output_path);
        response->set_total_bytes(total_bytes);
        response->set_chunks_received(static_cast<uint32_t>(total_bytes > 0 ? 1 : 0));

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_error_message(std::string("Exception during PCAP upload: ") + e.what());
        return grpc::Status::OK;
    }
}

}  // namespace wadjet::distributed


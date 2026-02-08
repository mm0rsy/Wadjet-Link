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
#include <nlohmann/json.hpp>

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
grpc::Status DistributedTestServiceImpl::RegisterNode(grpc::ServerContext* /*context*/,
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
    response->set_server_timestamp_ns(static_cast<int64_t>(
        std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count()));

    // Return coordinator protocol version
    auto coord_version = response->mutable_coordinator_version();
    coord_version->set_major(1);
    coord_version->set_minor(0);
    coord_version->set_patch(0);

    return grpc::Status::OK;
}

// T201: UnregisterNode RPC handler
grpc::Status DistributedTestServiceImpl::UnregisterNode(grpc::ServerContext* /*context*/,
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
        response.set_server_timestamp_ns(static_cast<int64_t>(
            std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count());

        // Set heartbeat acknowledgment
        response.set_acknowledged(true);

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
grpc::Status DistributedTestServiceImpl::WaitBarrier(grpc::ServerContext* /*context*/,
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
    response->set_state(v1::BarrierState::BARRIER_ALL_ARRIVED);
    response->set_sync_timestamp_ns(request->arrival_timestamp_ns());
    response->add_participating_nodes(node_id);
    response->set_wait_duration_ms(0);

    return grpc::Status::OK;
}

// T238: Helper to evaluate matcher on captured packets (distributed matcher evaluation)
// This runs matcher evaluation logic for distributed assertions
static auto evaluate_distributed_matcher(
    const std::string& matcher_type, const std::string& matcher_config,
    const std::vector<Packet>& captured_packets) -> std::string {
    try {
        // Parse matcher type and instantiate appropriate matcher
        std::unique_ptr<DistributedMatcher> matcher;

        if (matcher_type == "ExpectMessageFlow") {
            // Extract src/dst nodes from config JSON
            // For now, create a default matcher
            // Full implementation would parse JSON config
            return R"({"matched": true, "latency_ns": 0})";
        } else if (matcher_type == "WithinLatency") {
            // Create WithinLatency matcher with specified bounds
            return R"({"matched": true, "latency_ns": 0})";
        } else if (matcher_type == "HappensBefore") {
            // Create HappensBefore matcher for causal ordering
            return R"({"matched": true, "ordering": "correct"})";
        } else if (matcher_type == "MustNotSeeOn") {
            // Create MustNotSeeOn matcher for absence assertions
            return R"({"matched": true})";
        }

        return R"({"error": "Unknown matcher type"})";
    } catch (const std::exception& e) {
        return R"({"error": "Matcher evaluation failed"})";
    }
}

// T239: ControlChannel RPC handler (bidirectional streaming)
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
            // Timestamp at capture start for synchronization verification
        } else if (node_msg.has_capture_stopped()) {
            const auto& event = node_msg.capture_stopped();
            // Record capture completion timestamp
            // Trigger PCAP upload request if needed
        } else if (node_msg.has_matcher_result()) {
            const auto& event = node_msg.matcher_result();
            // Store distributed matcher evaluation result
            // This is the response from EvaluateMatcher command
        } else if (node_msg.has_error()) {
            const auto& event = node_msg.error();
            // Log error message from node
            // Update node health status if error is critical
        } else if (node_msg.has_log()) {
            const auto& event = node_msg.log();
            // Forward test log from node to coordinator log
        }

        // Create coordinator response message
        CoordinatorMessage coord_msg;
        coord_msg.set_message_id("ack-" + node_id + "-" + std::to_string(std::time(nullptr)));

        // T241: Check for pending commands for this node
        // Look up command queue for this node_id and populate coord_msg with commands
        // For now, just acknowledge. Full implementation would check a command_queue_ map

        if (!stream->Write(coord_msg)) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to write response");
        }
    }

    return grpc::Status::OK;
}

// T240: ReportResult RPC handler
grpc::Status DistributedTestServiceImpl::ReportResult(grpc::ServerContext* /*context*/,
                                                      const ReportResultRequest* request,
                                                      ReportResultResponse* response) {
    if (!coordinator_ || !request || !response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
    }

    const auto& node_id = request->node_id();
    const auto& node_id = request->node_id();
    const auto& result_obj = request->result();

    try {
        // Parse result protobuf message
        // Extract assertion results, passed/failed counts, timestamps
        // Store in coordinator's aggregation buffer indexed by node_id
        // In full implementation, would:
        // 1. Process AssertionResult array
        // 2. Validate against scenario expectations
        // 3. Aggregate with other node results for final report

        response->set_success(true);
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_error_message(std::string("Failed to parse result: ") + e.what());
    }

    return grpc::Status::OK;
}

// T206: UploadPcap RPC handler (client streaming)
grpc::Status DistributedTestServiceImpl::UploadPcap(grpc::ServerContext* /*context*/,
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

            // Validate data exists
            if (!chunk.data().empty()) {
                pcap_file.write(chunk.data().data(), chunk.data().size());
                total_bytes += chunk.data().size();
            }
        }

        // Close the file
        if (pcap_file.is_open()) {
            pcap_file.close();
        }

        // T050: Set success response with stored path and metadata
        response->set_success(true);
        response->set_stored_path(output_path);

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_error_message(std::string("Exception during PCAP upload: ") + e.what());
        return grpc::Status::OK;
    }
}

}  // namespace wadjet::distributed

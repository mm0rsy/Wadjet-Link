#pragma once

#include <string>
#include <memory>
#include <chrono>

// Proto generated includes with warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#include "distributed_test.grpc.pb.h"
#pragma GCC diagnostic pop

namespace wadjet::distributed {

class NodeInfo;
struct BarrierResult;

/**
 * @brief gRPC client for node-to-coordinator communication
 * 
 * T204-T206: Implements client-side gRPC communication
 * - Register/unregister nodes with coordinator (T204)
 * - Send heartbeats for health monitoring (T205)
 * - Participate in barrier synchronization (T206)
 * 
 * Proto-based service stubs generated from proto/distributed_test.proto
 */
class DistributedTestClient {
public:
    /**
     * @brief Create a client connection to the coordinator
     * 
     * @param coordinator_address Address of the coordinator (e.g., "localhost:50051")
     * @return Created client or nullptr on failure
     */
    static auto create(const std::string& coordinator_address)
        -> std::unique_ptr<DistributedTestClient>;
    
    explicit DistributedTestClient(const std::string& coordinator_address);
    
    ~DistributedTestClient();
    
    // Prevent copying
    DistributedTestClient(const DistributedTestClient&) = delete;
    DistributedTestClient& operator=(const DistributedTestClient&) = delete;
    
    // Allow moving
    DistributedTestClient(DistributedTestClient&&) noexcept;
    DistributedTestClient& operator=(DistributedTestClient&&) noexcept;
    
    /**
     * @brief Register this node with the coordinator
     * 
     * T204: Send RegisterNode RPC to coordinator
     * 
     * @param node_info Information about this node
     * @return true if registration successful
     */
    auto register_node(const NodeInfo& node_info) -> bool;
    
    /**
     * @brief Send periodic heartbeat to coordinator
     * 
     * T205: Send HeartbeatRequest and receive HeartbeatResponse
     * 
     * @param node_id ID of this node
     * @param timeout_ms Maximum time to wait for response
     * @return true if heartbeat acknowledged
     */
    auto send_heartbeat(const std::string& node_id, 
                       std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(1000)) -> bool;
    
    /**
     * @brief Participate in a barrier synchronization
     * 
     * T206: Send WaitBarrier RPC and wait for synchronization
     * 
     * @param node_id ID of this node
     * @param barrier_id ID of the barrier to synchronize on
     * @param timeout_ms Maximum time to wait in milliseconds
     * @return Barrier synchronization result
     */
    auto wait_barrier(const std::string& node_id,
                     const std::string& barrier_id, 
                     std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(5000))
        -> BarrierResult;
    
private:
    std::string coordinator_address_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<v1::DistributedTestService::Stub> stub_;
    
    auto connect() -> bool;
};

}  // namespace wadjet::distributed

#pragma once

#include <string>
#include <memory>
#include <chrono>

namespace wadjet::distributed {

class NodeInfo;
struct BarrierResult;

/**
 * @brief gRPC client for node-to-coordinator communication
 * 
 * T028: Implements client-side gRPC communication
 * - Register/unregister nodes with coordinator
 * - Send heartbeats for health monitoring
 * - Participate in barrier synchronization
 * 
 * Note: Requires HAVE_PROTO_LIB for full implementation (T015)
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
    
    /**
     * @brief Register this node with the coordinator
     * 
     * @param node_info Information about this node
     * @return true if registration successful
     */
    auto register_node(const NodeInfo& node_info) -> bool;
    
    /**
     * @brief Participate in a barrier synchronization
     * 
     * @param barrier_id ID of the barrier to synchronize on
     * @param timeout_ms Maximum time to wait in milliseconds
     * @return Barrier synchronization result
     */
    auto wait_barrier(const std::string& barrier_id, std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(5000))
        -> BarrierResult;
    
private:
    std::string coordinator_address_;
};

}  // namespace wadjet::distributed

#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <grpcpp/grpcpp.h>

// Forward declarations - generated proto
namespace wadjet::distributed::proto {
    class DistributedTestService;
    class RegisterNodeRequest;
    class RegisterNodeResponse;
    class WaitBarrierRequest;
    class WaitBarrierResponse;
}

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
    
    DistributedTestClient(std::shared_ptr<::grpc::Channel> channel);
    
    ~DistributedTestClient();
    
    /**
     * @brief Register this node with the coordinator
     * 
     * @param node_info Information about this node
     * @return true if registration successful
     */
    auto register_node(const NodeInfo& node_info) -> bool;
    
    /**
     * @brief Unregister this node from the coordinator
     * 
     * @param node_id Node identifier
     * @return true if unregistration successful
     */
    auto unregister_node(const std::string& node_id) -> bool;
    
    /**
     * @brief Send a heartbeat to the coordinator
     * 
     * @param node_id Node identifier
     * @return true if heartbeat sent successfully
     */
    auto send_heartbeat(const std::string& node_id) -> bool;
    
    /**
     * @brief Wait at a barrier for synchronization
     * 
     * T030: Call WaitBarrier RPC on coordinator
     * 
     * @param barrier_id Barrier identifier
     * @param node_id This node's ID
     * @param timeout Timeout for waiting
     * @return Barrier result or error
     */
    auto wait_barrier(const std::string& barrier_id,
                     const std::string& node_id,
                     std::chrono::milliseconds timeout)
        -> std::pair<bool, BarrierResult>;
    
    /**
     * @brief Check if client is connected
     * 
     * @return true if connected to coordinator
     */
    auto is_connected() const -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed

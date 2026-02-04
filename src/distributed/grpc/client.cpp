#include "wadjet/distributed/grpc/client.hpp"
#include "wadjet/distributed/types.hpp"
#include "wadjet/distributed/sync_barrier.hpp"

namespace wadjet::distributed {

// T028: Placeholder gRPC client implementation
// Full implementation requires proto code generation (T015) and gRPC

auto DistributedTestClient::create(const std::string& coordinator_address)
    -> std::unique_ptr<DistributedTestClient> {
    // TODO: Full implementation in Phase 4 (T028)
    // Requires gRPC channel creation
    return std::make_unique<DistributedTestClient>(coordinator_address);
}

DistributedTestClient::DistributedTestClient(const std::string& coordinator_address)
    : coordinator_address_(coordinator_address) {}

DistributedTestClient::~DistributedTestClient() = default;

auto DistributedTestClient::register_node(const NodeInfo& /*node_info*/) -> bool {
    // TODO: Implement RegisterNode RPC (T023)
    return false;
}

auto DistributedTestClient::wait_barrier(const std::string& /*barrier_id*/, std::chrono::milliseconds /*timeout_ms*/)
    -> BarrierResult {
    // TODO: Implement WaitBarrier RPC (T029)
    return BarrierResult{};
}

}  // namespace wadjet::distributed

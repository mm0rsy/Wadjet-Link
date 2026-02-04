#pragma once

#include <memory>

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
 * Note: Requires proto code generation for full implementation (T218, T219)
 * Proto files successfully compiled from proto/distributed_test.proto
 */
class DistributedTestServiceImpl {
public:
    explicit DistributedTestServiceImpl(TestCoordinator* coordinator);
    
    // T200: RegisterNode RPC handler - awaits proto header integration
    // T201: UnregisterNode RPC handler - awaits proto header integration
    // T202: Heartbeat streaming RPC handler - awaits proto header integration
    // T203: WaitBarrier RPC handler - awaits proto header integration
    // T204: ControlChannel RPC handler (bidirectional streaming) - awaits proto header integration
    // T205: ReportResult RPC handler - awaits proto header integration
    // T206: UploadPcap RPC handler (client streaming) - awaits proto header integration
    
private:
    TestCoordinator* coordinator_;
};

}  // namespace wadjet::distributed

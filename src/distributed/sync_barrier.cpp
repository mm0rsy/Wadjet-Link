#include "wadjet/distributed/sync_barrier.hpp"

#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <chrono>

namespace wadjet::distributed {

/**
 * @brief Implementation of SyncBarrier using condition variables
 */
struct SyncBarrier::Impl {
    std::string barrier_id;
    std::mutex mutex;
    std::condition_variable cv;
    
    // Coordinator-side state
    std::vector<std::string> expected_nodes;
    std::vector<std::string> arrived_nodes;
    std::vector<std::string> missing_nodes;
    int64_t sync_timestamp_ns = 0;
    bool coordinator_proceed = false;
    
    // Node-side state
    bool node_arrived = false;
    bool node_proceed = false;
    
    void reset() {
        expected_nodes.clear();
        arrived_nodes.clear();
        missing_nodes.clear();
        sync_timestamp_ns = 0;
        coordinator_proceed = false;
        node_arrived = false;
        node_proceed = false;
    }
};

// T011: Constructor
SyncBarrier::SyncBarrier(const std::string& barrier_id)
    : impl_(std::make_unique<Impl>()) {
    impl_->barrier_id = barrier_id;
}

// Destructor
SyncBarrier::~SyncBarrier() = default;

// Move constructor
SyncBarrier::SyncBarrier(SyncBarrier&&) noexcept = default;

// Move assignment
SyncBarrier& SyncBarrier::operator=(SyncBarrier&&) noexcept = default;

// T012: Wait for all nodes to arrive (coordinator-side)
auto SyncBarrier::wait_for_nodes(const std::vector<std::string>& expected_nodes,
                                 const Config& config) -> BarrierResult {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    
    impl_->expected_nodes = expected_nodes;
    impl_->arrived_nodes.clear();
    impl_->missing_nodes.clear();
    impl_->coordinator_proceed = false;
    impl_->sync_timestamp_ns = 0;
    
    // Wait for all nodes to arrive or timeout
    auto start_time = std::chrono::high_resolution_clock::now();
    bool all_arrived = false;
    
    while (true) {
        // Check if all nodes have arrived
        all_arrived = (impl_->arrived_nodes.size() == impl_->expected_nodes.size());
        
        if (all_arrived) {
            break;  // All nodes arrived
        }
        
        // Check timeout
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        
        if (elapsed >= config.timeout) {
            break;  // Timeout
        }
        
        // Wait for a node to arrive or timeout
        auto remaining = config.timeout - elapsed;
        if (!impl_->cv.wait_for(lock, remaining, [this]() {
            return impl_->arrived_nodes.size() == impl_->expected_nodes.size();
        })) {
            break;  // Timeout waiting
        }
    }
    
    // Determine missing nodes
    impl_->missing_nodes.clear();
    for (const auto& node : impl_->expected_nodes) {
        if (std::find(impl_->arrived_nodes.begin(), impl_->arrived_nodes.end(), node)
            == impl_->arrived_nodes.end()) {
            impl_->missing_nodes.push_back(node);
        }
    }
    
    // Set proceed signal and sync timestamp
    impl_->coordinator_proceed = all_arrived;
    impl_->sync_timestamp_ns = std::chrono::system_clock::now().time_since_epoch().count();
    impl_->node_proceed = true;
    
    // Notify all waiting nodes
    impl_->cv.notify_all();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    BarrierResult result;
    result.proceed = all_arrived;
    result.sync_timestamp_ns = impl_->sync_timestamp_ns;
    result.participating_nodes = impl_->arrived_nodes;
    result.missing_nodes = impl_->missing_nodes;
    result.wait_duration = duration;
    
    return result;
}

// T012: Signal arrival and wait for proceed (node-side)
auto SyncBarrier::arrive_and_wait(const std::string& node_id,
                                  std::chrono::milliseconds timeout)
    -> Result<BarrierResult> {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    
    // Signal that this node has arrived
    impl_->node_arrived = true;
    impl_->arrived_nodes.push_back(node_id);  // T252: Use actual node ID parameter
    impl_->cv.notify_all();
    
    // Wait for proceed signal or timeout
    bool proceed = impl_->cv.wait_for(lock, timeout, [this]() {
        return impl_->node_proceed;
    });
    
    if (!proceed) {
        return Result<BarrierResult>(
            Error::make("BARRIER_TIMEOUT", "Barrier synchronization timeout"));
    }
    
    BarrierResult result;
    result.proceed = impl_->coordinator_proceed;
    result.sync_timestamp_ns = impl_->sync_timestamp_ns;
    result.participating_nodes = impl_->arrived_nodes;
    result.missing_nodes = impl_->missing_nodes;
    result.wait_duration = timeout;
    
    return Result<BarrierResult>(result);
}

// Get barrier ID
auto SyncBarrier::barrier_id() const -> const std::string& {
    return impl_->barrier_id;
}

// Reset barrier
auto SyncBarrier::reset() -> void {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->reset();
}

}  // namespace wadjet::distributed

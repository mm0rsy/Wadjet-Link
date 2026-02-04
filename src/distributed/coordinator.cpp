#include "wadjet/distributed/coordinator.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <set>
#include <chrono>
#include <algorithm>

namespace wadjet::distributed {

/**
 * Implementation of TestCoordinator
 */
class TestCoordinatorImpl : public TestCoordinator {
public:
    explicit TestCoordinatorImpl(const CoordinatorConfig& config)
        : config_(config) {}
    
    ~TestCoordinatorImpl() override {
        if (is_running()) {
            stop();
        }
    }
    
    auto start() -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_) {
            return Result<void>(Error::make("ALREADY_RUNNING", "Coordinator already started"));
        }
        
        is_running_ = true;
        
        // Start heartbeat monitor thread
        heartbeat_thread_ = std::thread([this]() { monitor_heartbeats(); });
        
        return Result<void>();
    }
    
    auto stop() -> void override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_running_ = false;
            // Notify heartbeat thread to stop
            cv_.notify_all();
        }
        
        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
    }
    
    auto register_node(const NodeInfo& node_info) -> Result<void> override {
        if (!node_info.is_valid()) {
            return Result<void>(Error::make("INVALID_NODE", "Invalid node information"));
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (nodes_.count(node_info.id) > 0) {
            return Result<void>(Error::make("DUPLICATE_NODE", "Node already registered"));
        }
        
        if (nodes_.size() >= static_cast<size_t>(config_.max_nodes)) {
            return Result<void>(Error::make("MAX_NODES_EXCEEDED", "Maximum number of nodes reached"));
        }
        
        nodes_[node_info.id] = node_info;
        node_last_heartbeat_[node_info.id] = std::chrono::system_clock::now();
        
        // Notify on node status change callback
        if (node_status_callback_) {
            node_status_callback_(node_info.id, true);
        }
        
        cv_.notify_all();
        return Result<void>();
    }
    
    auto unregister_node(const NodeId& node_id) -> Result<void> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = nodes_.find(node_id);
        if (it == nodes_.end()) {
            return Result<void>(Error::make("NOT_FOUND", "Node not found"));
        }
        
        nodes_.erase(it);
        node_last_heartbeat_.erase(node_id);
        
        // Notify on node status change callback
        if (node_status_callback_) {
            node_status_callback_(node_id, false);
        }
        
        return Result<void>();
    }
    
    auto registered_nodes() const -> std::vector<NodeId> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<NodeId> result;
        for (const auto& [id, _] : nodes_) {
            result.push_back(id);
        }
        return result;
    }
    
    auto online_nodes() const -> std::vector<NodeId> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<NodeId> result;
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [id, last_hb] : node_last_heartbeat_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
            if (elapsed < config_.heartbeat_timeout) {
                result.push_back(id);
            }
        }
        return result;
    }
    
    auto get_node_info(const NodeId& node_id) const -> Result<NodeInfo> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = nodes_.find(node_id);
        if (it == nodes_.end()) {
            return Result<NodeInfo>(Error::make("NOT_FOUND", "Node not found"));
        }
        
        return Result<NodeInfo>(it->second);
    }
    
    auto create_barrier(const std::string& barrier_id) -> Result<std::unique_ptr<SyncBarrier>> override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (barriers_.count(barrier_id) > 0) {
            return Result<std::unique_ptr<SyncBarrier>>(
                Error::make("DUPLICATE_BARRIER", "Barrier already exists"));
        }
        
        auto barrier = std::make_unique<SyncBarrier>(barrier_id);
        barriers_.insert(barrier_id);
        
        return Result<std::unique_ptr<SyncBarrier>>(std::move(barrier));
    }
    
    auto on_node_status_changed(NodeStatusCallback callback) -> void override {
        std::lock_guard<std::mutex> lock(mutex_);
        node_status_callback_ = callback;
    }
    
    auto on_partition_detected(PartitionCallback callback) -> void override {
        std::lock_guard<std::mutex> lock(mutex_);
        partition_callback_ = callback;
    }
    
    auto is_running() const -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_running_;
    }
    
    auto wait_for_nodes(const std::vector<NodeId>& expected_nodes,
                       std::chrono::milliseconds timeout)
        -> Result<int> override {
        auto deadline = std::chrono::system_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (true) {
            // Check how many expected nodes are online
            int online_count = 0;
            auto now = std::chrono::system_clock::now();
            
            for (const auto& node_id : expected_nodes) {
                auto it = node_last_heartbeat_.find(node_id);
                if (it != node_last_heartbeat_.end()) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
                    if (elapsed < config_.heartbeat_timeout) {
                        online_count++;
                    }
                }
            }
            
            if (online_count == static_cast<int>(expected_nodes.size())) {
                return Result<int>(online_count);
            }
            
            if (now >= deadline) {
                return Result<int>(Error::make("TIMEOUT", "Timeout waiting for nodes"));
            }
            
            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            cv_.wait_for(lock, std::min(wait_time, config_.heartbeat_interval));
        }
    }
    
    /// Update heartbeat timestamp for a node
    auto update_node_heartbeat(const NodeId& node_id) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (node_last_heartbeat_.count(node_id) > 0) {
            node_last_heartbeat_[node_id] = std::chrono::system_clock::now();
        }
    }
    
private:
    void monitor_heartbeats() {
        while (is_running_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                
                if (!is_running_) {
                    break;
                }
                
                auto now = std::chrono::system_clock::now();
                std::vector<NodeId> lost_nodes;
                
                // Check for nodes that haven't heartbeat in time
                for (auto& [node_id, last_hb] : node_last_heartbeat_) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
                    if (elapsed > config_.heartbeat_timeout) {
                        lost_nodes.push_back(node_id);
                        
                        // Notify on node status change callback
                        if (node_status_callback_) {
                            node_status_callback_(node_id, false);
                        }
                    }
                }
                
                // Remove lost nodes
                for (const auto& node_id : lost_nodes) {
                    nodes_.erase(node_id);
                    node_last_heartbeat_.erase(node_id);
                }
                
                lock.unlock();
                
                // Sleep before next check
                std::this_thread::sleep_for(config_.heartbeat_interval);
            }
        }
    }
    
    CoordinatorConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    bool is_running_ = false;
    std::thread heartbeat_thread_;
    
    std::unordered_map<NodeId, NodeInfo> nodes_;
    std::unordered_map<NodeId, std::chrono::system_clock::time_point> node_last_heartbeat_;
    std::set<std::string> barriers_;
    
    NodeStatusCallback node_status_callback_;
    PartitionCallback partition_callback_;
};

// T020: Factory function
auto TestCoordinator::create(const CoordinatorConfig& config)
    -> Result<std::unique_ptr<TestCoordinator>> {
    return Result<std::unique_ptr<TestCoordinator>>(
        std::make_unique<TestCoordinatorImpl>(config));
}

}  // namespace wadjet::distributed

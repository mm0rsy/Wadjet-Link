#pragma once

#include "result.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Result of barrier synchronization
 */
struct BarrierResult {
    bool proceed = false;                          ///< Whether to proceed
    int64_t sync_timestamp_ns = 0;                 ///< Agreed synchronization time
    std::vector<std::string> participating_nodes;  ///< Nodes that participated
    std::vector<std::string> missing_nodes;        ///< Nodes that didn't arrive
    std::chrono::milliseconds wait_duration{0};    ///< Time spent waiting

    /**
     * @brief Check if barrier succeeded (all nodes arrived)
     */
    [[nodiscard]] auto succeeded() const -> bool { return proceed && missing_nodes.empty(); }
};

/**
 * @brief Distributed synchronization barrier
 *
 * Allows multiple nodes to synchronize at a common point.
 * Coordinator-side: waits for all expected nodes to arrive
 * Node-side: signals arrival and waits for proceed signal
 *
 * T011 & T012: Barrier synchronization primitive
 */
class SyncBarrier {
public:
    /**
     * @brief Configuration for barrier behavior
     */
    struct Config {
        std::chrono::milliseconds timeout{5000};    ///< Max time to wait
        std::chrono::milliseconds sync_margin{10};  ///< Extra delay for jitter
    };

    /**
     * @brief Create a new barrier with given ID
     *
     * @param barrier_id Unique identifier for this barrier
     */
    explicit SyncBarrier(const std::string& barrier_id);

    /**
     * @brief Destructor
     */
    ~SyncBarrier();

    // Prevent copying
    SyncBarrier(const SyncBarrier&) = delete;
    SyncBarrier& operator=(const SyncBarrier&) = delete;

    // Allow moving
    SyncBarrier(SyncBarrier&&) noexcept;
    SyncBarrier& operator=(SyncBarrier&&) noexcept;

    /**
     * @brief Wait for all nodes to arrive (coordinator-side)
     *
     * T012: Waits for all expected nodes to signal arrival
     *
     * @param expected_nodes List of node IDs expected to arrive
     * @param config Barrier configuration
     * @return BarrierResult with proceed flag and timing info
     */
    auto wait_for_nodes(const std::vector<std::string>& expected_nodes,
                        const Config& config) -> BarrierResult;

    /**
     * @brief Signal arrival and wait for proceed (node-side)
     *
     * T012: Called by node to signal arrival at barrier
     * Blocks until all nodes have arrived or timeout
     *
     * @param node_id Identifier of the node arriving at barrier
     * @param timeout Maximum time to wait
     * @return BarrierResult or error
     */
    auto arrive_and_wait(const std::string& node_id,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds{
                             5000}) -> Result<BarrierResult>;

    /**
     * @brief Get the barrier ID
     *
     * @return Reference to barrier ID string
     */
    [[nodiscard]] auto barrier_id() const -> const std::string&;

    /**
     * @brief Reset barrier for reuse
     */
    auto reset() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed

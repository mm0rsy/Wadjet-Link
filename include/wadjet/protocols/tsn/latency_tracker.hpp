#pragma once

#include "types.hpp"

#include <array>
#include <map>

namespace wadjet::protocols::tsn {

/**
 * @brief TSN Latency Tracker
 *
 * Tracks per-priority latency statistics across TSN traffic.
 * M12: User Story 3 - Latency measurement with PCP context
 */
class LatencyTracker {
public:
    explicit LatencyTracker(const LatencyConfig& config = LatencyConfig{});

    ~LatencyTracker() = default;

    /// Record latency measurement for a priority class
    void record_latency(PriorityCodePoint pcp, int64_t latency_ns);

    /// Get latency stats for a specific priority
    const LatencyStats* get_priority_stats(PriorityCodePoint pcp) const;

    /// Get all priority stats
    const std::array<LatencyStats, 8>& get_all_priority_stats() const { return stats_; }

    /// Get stream-specific latency stats if tracked
    const std::map<StreamId, LatencyStats>& get_stream_stats() const { return stream_stats_; }

    /// Finalize latency tracking and compute percentiles
    void finalize();

    /// Reset tracked data
    void reset();

private:
    LatencyConfig config_;
    std::array<std::vector<int64_t>, 8> latency_samples_;
    std::array<LatencyStats, 8> stats_;
    std::map<StreamId, LatencyStats> stream_stats_;
};

}  // namespace wadjet::protocols::tsn

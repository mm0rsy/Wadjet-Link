#include "wadjet/protocols/tsn/latency_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace wadjet::protocols::tsn {

LatencyTracker::LatencyTracker(const LatencyConfig& config) : config_(config) {
    // Initialize stats with thresholds
    for (size_t i = 0; i < 8; ++i) {
        stats_[i].min_ns = INT64_MAX;
    }
}

void LatencyTracker::record_latency(PriorityCodePoint pcp, int64_t latency_ns) {
    size_t idx = static_cast<size_t>(pcp);
    if (idx >= 8) {
        return;
    }

    auto& samples = latency_samples_[idx];
    if (samples.size() < config_.max_samples_per_priority) {
        samples.push_back(latency_ns);
    }
}

const LatencyStats* LatencyTracker::get_priority_stats(PriorityCodePoint pcp) const {
    size_t idx = static_cast<size_t>(pcp);
    if (idx >= 8) {
        return nullptr;
    }
    return &stats_[idx];
}

void LatencyTracker::finalize() {
    // Compute statistics for each priority
    for (size_t i = 0; i < 8; ++i) {
        const auto& samples = latency_samples_[i];
        if (samples.empty()) {
            continue;
        }

        auto& stat = stats_[i];
        stat.sample_count = samples.size();
        stat.min_ns = *std::min_element(samples.begin(), samples.end());
        stat.max_ns = *std::max_element(samples.begin(), samples.end());
        stat.mean_ns = static_cast<double>(
            std::accumulate(samples.begin(), samples.end(), int64_t(0)));
        stat.mean_ns /= static_cast<double>(samples.size());

        // Compute percentiles
        auto sorted_samples = samples;
        std::sort(sorted_samples.begin(), sorted_samples.end());

        size_t p50_idx = (sorted_samples.size() * 50) / 100;
        size_t p95_idx = (sorted_samples.size() * 95) / 100;
        size_t p99_idx = (sorted_samples.size() * 99) / 100;

        stat.p50_ns = sorted_samples[p50_idx];
        stat.p95_ns = sorted_samples[std::min(p95_idx, sorted_samples.size() - 1)];
        stat.p99_ns = sorted_samples[std::min(p99_idx, sorted_samples.size() - 1)];

        // Count violations
        int64_t threshold = static_cast<int64_t>(config_.threshold_ns[i]);
        stat.violations = static_cast<uint64_t>(
            std::count_if(samples.begin(), samples.end(),
                         [threshold](int64_t lat) { return lat > threshold; }));
    }
}

void LatencyTracker::reset() {
    for (size_t i = 0; i < 8; ++i) {
        latency_samples_[i].clear();
        stats_[i] = LatencyStats{};
        stats_[i].min_ns = INT64_MAX;
    }
    stream_stats_.clear();
}

}  // namespace wadjet::protocols::tsn

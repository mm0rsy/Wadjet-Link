#include "wadjet/protocols/tsn/stream_tracker.hpp"

namespace wadjet::protocols::tsn {

StreamTracker::StreamTracker(int idle_timeout_ms) : idle_timeout_ms_(idle_timeout_ms) {}

void StreamTracker::process_packet(const StreamId& stream_id, uint8_t pcp, uint64_t byte_count,
                                   int64_t timestamp_ns) {
    auto it = stream_map_.find(stream_id);
    if (it == stream_map_.end()) {
        // New stream
        StreamStats stats;
        stats.id = stream_id;
        stats.expected_pcp = pcp;
        stats.first_seen_ns = timestamp_ns;
        stats.last_seen_ns = timestamp_ns;
        stats.packet_count = 1;
        stats.byte_count = byte_count;
        stats.pcp_distribution[static_cast<PriorityCodePoint>(pcp)] = 1;
        stream_map_[stream_id] = stats;
    } else {
        // Existing stream
        auto& stats = it->second;
        stats.packet_count++;
        stats.byte_count += byte_count;
        stats.last_seen_ns = timestamp_ns;
        stats.pcp_distribution[static_cast<PriorityCodePoint>(pcp)]++;
    }
}

void StreamTracker::finalize() {
    streams_.clear();
    for (const auto& [id, stats] : stream_map_) {
        streams_.push_back(stats);
    }
}

const StreamStats* StreamTracker::get_stream(const StreamId& id) const {
    auto it = stream_map_.find(id);
    if (it != stream_map_.end()) {
        return &it->second;
    }
    return nullptr;
}

}  // namespace wadjet::protocols::tsn

#pragma once

#include "types.hpp"

#include <map>
#include <vector>

namespace wadjet::protocols::tsn {

/**
 * @brief TSN Stream Tracker
 *
 * Tracks individual TSN streams across nodes.
 * M12: User Story 2 - Stream identification and tracking
 */
class StreamTracker {
public:
    explicit StreamTracker(int idle_timeout_ms = 10'000);

    ~StreamTracker() = default;

    /// Record a packet's stream membership
    void process_packet(const StreamId& stream_id, uint8_t pcp,
                       uint64_t byte_count, int64_t timestamp_ns);

    /// Finalize stream tracking
    void finalize();

    /// Get all tracked streams
    const std::vector<StreamStats>& get_all_streams() const { return streams_; }

    /// Get specific stream if tracked
    const StreamStats* get_stream(const StreamId& id) const;

    /// Get active stream count
    size_t get_stream_count() const { return streams_.size(); }

private:
    std::map<StreamId, StreamStats> stream_map_;
    std::vector<StreamStats> streams_;
    int idle_timeout_ms_;
};

}  // namespace wadjet::protocols::tsn

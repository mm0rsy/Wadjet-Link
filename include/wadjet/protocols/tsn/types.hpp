#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::protocols::tsn {

/**
 * @brief Priority Code Point (3 bits from VLAN tag)
 *
 * Defined in IEEE 802.1Q for traffic class mapping
 */
enum class PriorityCodePoint : uint8_t {
    BestEffort = 0,       ///< BE - Default traffic
    Background = 1,       ///< BK - Bulk data transfer
    ExcellentEffort = 2,  ///< EE - Streaming media
    CriticalApp = 3,      ///< CA - Business-critical data
    Video = 4,            ///< VI - Real-time video
    Voice = 5,            ///< VO - Real-time voice
    InternetControl = 6,  ///< IC - Network protocols
    NetworkControl = 7    ///< NC - Network management
};

/**
 * @brief TSN Stream Identifier
 *
 * Uniquely identifies a TSN stream by source MAC and VLAN
 */
struct StreamId {
    std::string source_mac;  ///< Source MAC address as string
    uint16_t vlan_id;        ///< VLAN ID

    bool operator==(const StreamId& other) const {
        return source_mac == other.source_mac && vlan_id == other.vlan_id;
    }

    bool operator<(const StreamId& other) const {
        if (source_mac != other.source_mac) {
            return source_mac < other.source_mac;
        }
        return vlan_id < other.vlan_id;
    }

    std::string to_string() const { return source_mac + ":" + std::to_string(vlan_id); }
};

/**
 * @brief Latency statistics
 *
 * Per-priority latency measurements with percentiles
 */
struct LatencyStats {
    int64_t min_ns = INT64_MAX;  ///< Minimum latency
    int64_t max_ns = 0;          ///< Maximum latency
    double mean_ns = 0.0;        ///< Mean latency
    int64_t p50_ns = 0;          ///< 50th percentile
    int64_t p95_ns = 0;          ///< 95th percentile
    int64_t p99_ns = 0;          ///< 99th percentile
    uint64_t sample_count = 0;   ///< Number of samples
    uint64_t violations = 0;     ///< Count exceeding threshold
};

/**
 * @brief Latency configuration
 *
 * Per-priority latency thresholds and tracking settings
 */
struct LatencyConfig {
    bool enable_per_priority = true;            ///< Track per-priority latency
    bool enable_per_stream = false;             ///< Track per-stream latency
    uint64_t max_samples_per_priority = 10000;  ///< Max latency samples

    // Latency thresholds per priority (nanoseconds)
    std::array<uint64_t, 8> threshold_ns{
        100'000'000,  // PCP 0: 100ms
        100'000'000,  // PCP 1: 100ms
        50'000'000,   // PCP 2: 50ms
        20'000'000,   // PCP 3: 20ms
        5'000'000,    // PCP 4: 5ms
        5'000'000,    // PCP 5: 5ms
        2'000'000,    // PCP 6: 2ms
        1'000'000     // PCP 7: 1ms
    };
};

/**
 * @brief Stream statistics
 *
 * Comprehensive statistics for a single TSN stream
 */
struct StreamStats {
    StreamId id;                ///< Stream identifier
    uint64_t packet_count = 0;  ///< Total packets
    uint64_t byte_count = 0;    ///< Total bytes
    int64_t first_seen_ns = 0;  ///< First packet timestamp
    int64_t last_seen_ns = 0;   ///< Most recent packet timestamp
    uint8_t expected_pcp = 0;   ///< Expected priority

    // Per-priority PCP distribution seen
    std::map<PriorityCodePoint, uint64_t> pcp_distribution;

    // Latency stats if tracking enabled
    std::optional<LatencyStats> latency;

    uint64_t bandwidth_bps = 0;  ///< Calculated bandwidth
};

}  // namespace wadjet::protocols::tsn

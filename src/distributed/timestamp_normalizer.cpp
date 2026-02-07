#include "wadjet/distributed/timestamp_normalizer.hpp"

#include "wadjet/protocols/gptp/gptp.hpp"

#include <sys/timex.h>
#include <time.h>

#include <cmath>

namespace wadjet::distributed {

// T008: Detect clock synchronization status
auto TimestampNormalizer::detect_sync_status() -> ClockSyncStatus {
    ClockSyncStatus status;
    status.method = ClockSyncMethod::None;
    status.is_synchronized = false;
    
    // Try to detect NTP status using adjtimex()
    struct timex ntx = {};
    if (adjtimex(&ntx) >= 0) {
        // Check NTP status bits
        if (ntx.status & STA_UNSYNC) {
            status.is_synchronized = false;
        } else if (ntx.status & (STA_PPSFREQ | STA_PPSTIME)) {
            // PPS source is active (gPTP uses PPS frequency/time discipline)
            status.is_synchronized = true;
            status.method = ClockSyncMethod::GPTP;
            status.estimated_offset_ns = ntx.offset * 1000LL;  // Convert microseconds to nanoseconds
            status.max_error_ns = 1000LL;  // Typical 1 microsecond PPS error
        } else if ((ntx.status & STA_PLL) || (ntx.status & STA_FLL)) {
            // NTP uses PLL (Phase-Locked Loop) or FLL (Frequency-Locked Loop)
            status.is_synchronized = true;
            status.method = ClockSyncMethod::NTP;
            status.estimated_offset_ns = ntx.offset * 1000LL;  // Convert microseconds to nanoseconds
            status.max_error_ns = ntx.maxerror * 1000LL;  // Convert to nanoseconds
        } else {
            status.method = ClockSyncMethod::None;
            status.is_synchronized = false;
        }
    }
    
    return status;
}

// T009: Get current UTC time in nanoseconds
auto TimestampNormalizer::now_utc_ns() -> int64_t {
    struct timespec ts {};
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// T009: Convert hardware timespec to UTC nanoseconds
auto TimestampNormalizer::hardware_to_utc(const struct timespec& ts) -> int64_t {
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Constructor
TimestampNormalizer::TimestampNormalizer(const ClockSyncStatus& status)
    : status_(status) {}

// Get estimated precision in nanoseconds
auto TimestampNormalizer::estimated_precision() const -> std::chrono::nanoseconds {
    // Precision depends on sync method
    switch (status_.method) {
        case ClockSyncMethod::GPTP:
            // gPTP can achieve nanosecond-level precision
            return std::chrono::nanoseconds(1000);  // 1 microsecond typical
        case ClockSyncMethod::NTP:
            // NTP typically achieves millisecond to microsecond precision
            return std::chrono::nanoseconds(1000000);  // 1 millisecond typical
        case ClockSyncMethod::None:
        case ClockSyncMethod::Unknown:
        default:
            // No synchronization - only system clock precision
            // Typically microsecond level on modern systems
            return std::chrono::nanoseconds(1000000);  // 1 millisecond estimate
    }
}

// Check if timestamps are within acceptable drift
auto TimestampNormalizer::within_drift(int64_t ts1, int64_t ts2,
                                      std::chrono::nanoseconds max_drift) const -> bool {
    int64_t delta = std::abs(ts1 - ts2);
    return delta <= max_drift.count();
}

// T215: Normalize packet timestamp to UTC nanoseconds
auto TimestampNormalizer::normalize(const Packet& packet) const -> int64_t {
    // Get the packet's raw timestamp (typically from packet capture metadata)
    // Packet timestamps are usually CLOCK_REALTIME already, but may have clock skew
    int64_t packet_ts_ns = packet.timestamp().total_nanoseconds();

    // Apply clock sync offset if synchronized
    if (status_.is_synchronized) {
        // Adjust by the estimated offset from NTP/gPTP
        // positive offset means our clock is ahead, so subtract it
        packet_ts_ns -= status_.estimated_offset_ns;
    }

    return packet_ts_ns;
}

// T051: Verify gPTP clock sync health via passive message decoding
auto TimestampNormalizer::verify_gptp_health(const Packet& packet)
    -> std::optional<ClockSyncStatus> {
    // T051: Simple gPTP packet detection via byte inspection
    // We perform passive monitoring without requiring complex parsing

    const auto& data = packet.data();
    if (data.size() < 34) {  // Minimum gPTP message size
        return std::nullopt;
    }

    // Check first byte: transport-specific and message type
    // Cast std::byte to uint8_t for bitwise operations
    uint8_t ts_and_type = static_cast<uint8_t>(data[0]);
    uint8_t version = static_cast<uint8_t>(data[1]) & 0x0F;

    // Check transport specific bits (upper 4 bits should be 0 for standard messages)
    if ((ts_and_type & 0xF0) != 0x00) {
        return std::nullopt;
    }

    // Check version (bits 0-3 should be 2 for 802.1AS)
    if (version != 2 && version != 0) {  // Accept 0 for compatibility
        return std::nullopt;
    }

    // Create updated sync status with gPTP info
    ClockSyncStatus status = status_;
    status.method = ClockSyncMethod::GPTP;
    status.is_synchronized = true;

    // Extract source clock identity from offset 20-27 (8 bytes of source port identity)
    if (data.size() >= 28) {
        // Format as hex string for display
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 static_cast<uint8_t>(data[20]), static_cast<uint8_t>(data[21]),
                 static_cast<uint8_t>(data[22]), static_cast<uint8_t>(data[23]),
                 static_cast<uint8_t>(data[24]), static_cast<uint8_t>(data[25]),
                 static_cast<uint8_t>(data[26]), static_cast<uint8_t>(data[27]));

        status.grandmaster_id = buffer;
        status.max_error_ns = 1000;      // 1 microsecond typical for gPTP
        status.estimated_offset_ns = 0;  // Will be measured separately
    }

    return status;
}

}  // namespace wadjet::distributed

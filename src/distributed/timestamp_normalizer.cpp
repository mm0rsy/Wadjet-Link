#include "wadjet/distributed/timestamp_normalizer.hpp"

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

}  // namespace wadjet::distributed

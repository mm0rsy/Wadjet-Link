#pragma once

#include <cstdint>
#include <chrono>
#include <string>

namespace wadjet::distributed {

/**
 * @brief Clock synchronization method enumeration
 * 
 * T007: Clock sync method detection
 */
enum class ClockSyncMethod {
    None,       ///< No synchronization
    NTP,        ///< Network Time Protocol
    GPTP,       ///< IEEE 802.1AS gPTP
    Unknown     ///< Unknown/undetected method
};

/**
 * @brief Clock synchronization status
 * 
 * T007: Represents the current clock synchronization status of a node
 */
struct ClockSyncStatus {
    ClockSyncMethod method = ClockSyncMethod::None;      ///< Sync method in use
    bool is_synchronized = false;                         ///< Whether clock is synchronized
    int64_t estimated_offset_ns = 0;                      ///< Estimated offset in nanoseconds
    int64_t max_error_ns = 0;                             ///< Maximum estimation error
    std::string grandmaster_id;                           ///< Grandmaster ID (for gPTP)
};

/**
 * @brief Timestamp normalizer for cross-node timestamp correlation
 * 
 * Converts between node-local timestamps and normalized UTC timestamps
 * for correlation across multiple nodes. Detects clock synchronization
 * method (NTP, gPTP, etc.) and provides precision estimates.
 */
class TimestampNormalizer {
public:
    /**
     * @brief Detect current clock synchronization status
     * 
     * T008: Detects whether system is using NTP, gPTP, or no synchronization
     * Uses adjtimex() on Linux to detect NTP state
     * 
     * @return ClockSyncStatus indicating sync method and precision
     */
    static auto detect_sync_status() -> ClockSyncStatus;
    
    /**
     * @brief Get current time in UTC nanoseconds since Unix epoch
     * 
     * T009: Returns CLOCK_REALTIME in nanoseconds
     * 
     * @return Current UTC time in nanoseconds
     */
    static auto now_utc_ns() -> int64_t;
    
    /**
     * @brief Convert hardware timestamp to UTC nanoseconds
     * 
     * T009: Converts struct timespec to nanoseconds since epoch
     * 
     * @param ts Hardware timestamp (e.g., from packet headers)
     * @return Timestamp in nanoseconds since Unix epoch
     */
    static auto hardware_to_utc(const struct timespec& ts) -> int64_t;
    
    /**
     * @brief Construct normalizer with specific sync status
     * 
     * @param status Clock synchronization status
     */
    explicit TimestampNormalizer(const ClockSyncStatus& status);
    
    /**
     * @brief Get estimated precision of timestamps
     * 
     * @return Estimated precision in nanoseconds
     */
    [[nodiscard]] auto estimated_precision() const -> std::chrono::nanoseconds;
    
    /**
     * @brief Check if two timestamps are within acceptable drift
     * 
     * @param ts1 First timestamp (nanoseconds)
     * @param ts2 Second timestamp (nanoseconds)
     * @param max_drift Maximum acceptable drift
     * @return true if timestamps are within drift tolerance
     */
    [[nodiscard]] auto within_drift(int64_t ts1, int64_t ts2,
                                   std::chrono::nanoseconds max_drift) const -> bool;
    
    /**
     * @brief Get the sync status this normalizer was created with
     * 
     * @return Reference to sync status
     */
    [[nodiscard]] auto get_status() const -> const ClockSyncStatus& { return status_; }
    
    /**
     * @brief Get the sync status (alias for get_status)
     * 
     * @return Reference to sync status
     */
    [[nodiscard]] auto status() const -> const ClockSyncStatus& { return status_; }

private:
    ClockSyncStatus status_;
};

}  // namespace wadjet::distributed

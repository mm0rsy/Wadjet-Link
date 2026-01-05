#pragma once

/// @file dtc_manager.hpp
/// @brief DTC (Diagnostic Trouble Code) manager for UDS/DoIP
///
/// Provides comprehensive DTC tracking and analysis:
/// - Store and retrieve DTCs from multiple ECUs
/// - Track DTC status changes over time
/// - Analyze DTC patterns and frequencies
/// - Support for snapshot and extended data
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_types.hpp"
#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_services.hpp"
#include "wadjet/protocols/uds/uds_types.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace wadjet::protocols::diagnostic {

// =============================================================================
// DTC Record Types
// =============================================================================

/// @brief Extended DTC information record
struct DTCRecord {
    // Core DTC information
    uds::DTC dtc;                      ///< The DTC code
    uds::DTCStatusMask status;         ///< Current status mask
    LogicalAddress ecu_address{0};     ///< Source ECU
    
    // Timing information
    std::chrono::steady_clock::time_point first_seen;   ///< When first detected
    std::chrono::steady_clock::time_point last_updated; ///< When last status change
    std::uint32_t occurrence_count{1};                  ///< How many times seen
    
    // Extended data
    std::optional<std::uint8_t> severity;         ///< DTC severity (if available)
    std::optional<std::uint8_t> functional_unit;  ///< Functional unit identifier
    std::vector<std::uint8_t> extended_data;      ///< Extended data record
    
    // Snapshot data
    struct SnapshotRecord {
        std::uint8_t record_number{0};
        std::vector<std::uint8_t> data;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<SnapshotRecord> snapshots;
    
    // =========================================================================
    // Helper Methods
    // =========================================================================
    
    /// @brief Get DTC as string (e.g., "P0123")
    [[nodiscard]] std::string dtc_string() const {
        return dtc.to_string();
    }
    
    /// @brief Check if DTC is currently active
    [[nodiscard]] bool is_active() const {
        return status.test_failed;
    }
    
    /// @brief Check if DTC is confirmed
    [[nodiscard]] bool is_confirmed() const {
        return status.confirmed_dtc;
    }
    
    /// @brief Check if DTC is pending
    [[nodiscard]] bool is_pending() const {
        return status.pending_dtc;
    }
    
    /// @brief Check if DTC failed since last clear
    [[nodiscard]] bool failed_since_clear() const {
        return status.test_failed_since_last_clear;
    }
    
    /// @brief Get age since first seen
    [[nodiscard]] std::chrono::milliseconds age() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - first_seen);
    }
    
    /// @brief Get time since last update
    [[nodiscard]] std::chrono::milliseconds time_since_update() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_updated);
    }
};

/// @brief DTC severity levels (ISO 14229)
enum class DTCSeverity : std::uint8_t {
    NoSeverityAvailable = 0x00,
    MaintenanceOnly = 0x20,              ///< Maintenance required
    CheckAtNextHalt = 0x40,              ///< Check at next stop
    CheckImmediately = 0x80,             ///< Check immediately
    // Combined severity masks
    MaintenanceOrCheckHalt = 0x60,
    MaintenanceOrCheckImmediate = 0xA0,
    CheckHaltOrImmediate = 0xC0,
    AllSeverities = 0xE0,
};

/// @brief Convert severity to string
[[nodiscard]] std::string_view dtc_severity_string(DTCSeverity severity);

// =============================================================================
// DTC Events
// =============================================================================

/// @brief Events emitted by DTC manager
enum class DTCEvent {
    DTCAdded,           ///< New DTC discovered
    DTCUpdated,         ///< Existing DTC status changed
    DTCCleared,         ///< DTC was cleared
    AllDTCsCleared,     ///< All DTCs cleared for ECU
    SnapshotReceived,   ///< Snapshot data received
    ExtendedReceived,   ///< Extended data received
    DTCCountChanged,    ///< Total DTC count changed
};

/// @brief Convert DTCEvent to string
[[nodiscard]] std::string_view dtc_event_string(DTCEvent event);

/// @brief Callback for DTC events
using DTCEventCallback = std::function<void(
    DTCEvent event,
    LogicalAddress ecu_address,
    const DTCRecord* dtc_record)>;  // nullptr for clear events

// =============================================================================
// DTC Filter
// =============================================================================

/// @brief Filter criteria for DTC queries
struct DTCFilter {
    /// @brief Filter by ECU (0 = all ECUs)
    LogicalAddress ecu_address{0};
    
    /// @brief Filter by status mask (std::nullopt = any status)
    std::optional<uds::DTCStatusMask> status_mask;
    
    /// @brief Filter by severity (std::nullopt = any severity)
    std::optional<DTCSeverity> severity;
    
    /// @brief Only active DTCs
    bool active_only{false};
    
    /// @brief Only confirmed DTCs
    bool confirmed_only{false};
    
    /// @brief Only pending DTCs
    bool pending_only{false};
    
    /// @brief Only DTCs newer than this age
    std::optional<std::chrono::milliseconds> max_age;
    
    /// @brief DTC code prefix filter (e.g., "P" for powertrain)
    std::optional<char> category_prefix;
    
    /// @brief Create filter for all DTCs
    static DTCFilter all() { return DTCFilter{}; }
    
    /// @brief Create filter for active DTCs
    static DTCFilter active() {
        DTCFilter f;
        f.active_only = true;
        return f;
    }
    
    /// @brief Create filter for confirmed DTCs
    static DTCFilter confirmed() {
        DTCFilter f;
        f.confirmed_only = true;
        return f;
    }
    
    /// @brief Create filter for specific ECU
    static DTCFilter for_ecu(LogicalAddress addr) {
        DTCFilter f;
        f.ecu_address = addr;
        return f;
    }
};

// =============================================================================
// DTC Statistics
// =============================================================================

/// @brief DTC statistics for an ECU
struct ECUDTCStatistics {
    LogicalAddress ecu_address{0};
    std::size_t total_dtcs{0};
    std::size_t active_dtcs{0};
    std::size_t confirmed_dtcs{0};
    std::size_t pending_dtcs{0};
    std::size_t dtcs_since_clear{0};
    std::chrono::steady_clock::time_point last_read;
    std::chrono::steady_clock::time_point last_clear;
};

// =============================================================================
// DTC Manager
// =============================================================================

/// @brief Manages DTCs from multiple ECUs
///
/// Provides comprehensive DTC management including:
/// - Collecting DTCs from ReadDTCInformation responses
/// - Tracking DTC status changes over time
/// - Storing snapshot and extended data
/// - Filtering and querying DTCs
/// - Event notifications for DTC changes
///
/// @example
/// ```cpp
/// DTCManager manager;
///
/// // Register callback for DTC events
/// manager.on_event([](DTCEvent event, LogicalAddress ecu, const DTCRecord* dtc) {
///     if (event == DTCEvent::DTCAdded && dtc) {
///         std::cout << "New DTC: " << dtc->dtc_string() << " from ECU 0x"
///                   << std::hex << ecu << "\n";
///     }
/// });
///
/// // Process ReadDTCInformation response
/// manager.process_read_dtc_response(dtc_response, ecu_address);
///
/// // Query active DTCs
/// auto active_dtcs = manager.get_dtcs(DTCFilter::active());
/// std::cout << "Active DTCs: " << active_dtcs.size() << "\n";
///
/// // Get statistics
/// auto stats = manager.get_ecu_statistics(ecu_address);
/// std::cout << "Total: " << stats.total_dtcs
///           << ", Active: " << stats.active_dtcs << "\n";
/// ```
class DTCManager {
public:
    /// @brief Configuration options
    struct Options {
        /// @brief Maximum DTCs to store per ECU
        std::size_t max_dtcs_per_ecu = 1000;
        
        /// @brief Maximum total DTCs to store
        std::size_t max_total_dtcs = 10000;
        
        /// @brief Keep history of cleared DTCs
        bool keep_cleared_history = true;
        
        /// @brief Maximum cleared DTC history per ECU
        std::size_t max_cleared_history = 100;
        
        /// @brief Track DTC occurrence count
        bool track_occurrences = true;
        
        /// @brief Get default options
        static Options defaults() { return Options{}; }
    };
    
    explicit DTCManager(Options opts = Options::defaults());
    ~DTCManager();
    
    // Non-copyable, non-movable (contains std::mutex)
    DTCManager(const DTCManager&) = delete;
    DTCManager& operator=(const DTCManager&) = delete;
    DTCManager(DTCManager&&) = delete;
    DTCManager& operator=(DTCManager&&) = delete;
    
    // =========================================================================
    // Response Processing
    // =========================================================================
    
    /// @brief Process ReadDTCInformation response
    void process_read_dtc_response(const uds::ReadDTCInformationResponse& resp,
                                   LogicalAddress ecu_address,
                                   std::chrono::steady_clock::time_point timestamp =
                                       std::chrono::steady_clock::now());
    
    /// @brief Process ClearDiagnosticInformation response
    void process_clear_dtc_response(const uds::ClearDiagnosticInformationRequest& req,
                                    LogicalAddress ecu_address,
                                    bool success,
                                    std::chrono::steady_clock::time_point timestamp =
                                        std::chrono::steady_clock::now());
    
    /// @brief Add or update a single DTC
    void add_dtc(const uds::DTC& dtc,
                 const uds::DTCStatusMask& status,
                 LogicalAddress ecu_address,
                 std::chrono::steady_clock::time_point timestamp =
                     std::chrono::steady_clock::now());
    
    /// @brief Add snapshot data to a DTC
    void add_snapshot(const uds::DTC& dtc,
                      LogicalAddress ecu_address,
                      std::uint8_t record_number,
                      std::span<const std::uint8_t> data,
                      std::chrono::steady_clock::time_point timestamp =
                          std::chrono::steady_clock::now());
    
    /// @brief Add extended data to a DTC
    void add_extended_data(const uds::DTC& dtc,
                           LogicalAddress ecu_address,
                           std::span<const std::uint8_t> data,
                           std::chrono::steady_clock::time_point timestamp =
                               std::chrono::steady_clock::now());
    
    // =========================================================================
    // DTC Queries
    // =========================================================================
    
    /// @brief Get DTCs matching filter criteria
    [[nodiscard]] std::vector<DTCRecord> get_dtcs(const DTCFilter& filter = DTCFilter::all()) const;
    
    /// @brief Get specific DTC for ECU
    [[nodiscard]] const DTCRecord* get_dtc(const uds::DTC& dtc,
                                            LogicalAddress ecu_address) const;
    
    /// @brief Get all DTCs for an ECU
    [[nodiscard]] std::vector<DTCRecord> get_ecu_dtcs(LogicalAddress ecu_address) const;
    
    /// @brief Get count of DTCs matching filter
    [[nodiscard]] std::size_t count_dtcs(const DTCFilter& filter = DTCFilter::all()) const;
    
    /// @brief Check if specific DTC exists
    [[nodiscard]] bool has_dtc(const uds::DTC& dtc, LogicalAddress ecu_address) const;
    
    /// @brief Get all ECU addresses with DTCs
    [[nodiscard]] std::vector<LogicalAddress> get_ecus_with_dtcs() const;
    
    // =========================================================================
    // History
    // =========================================================================
    
    /// @brief Get cleared DTC history for ECU
    [[nodiscard]] std::vector<DTCRecord> get_cleared_history(LogicalAddress ecu_address,
                                                              std::size_t max_count = 50) const;
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    /// @brief Get statistics for specific ECU
    [[nodiscard]] ECUDTCStatistics get_ecu_statistics(LogicalAddress ecu_address) const;
    
    /// @brief Get statistics for all ECUs
    [[nodiscard]] std::vector<ECUDTCStatistics> get_all_statistics() const;
    
    /// @brief Global statistics
    struct GlobalStatistics {
        std::size_t total_dtcs{0};
        std::size_t total_ecus{0};
        std::size_t active_dtcs{0};
        std::size_t confirmed_dtcs{0};
        std::uint64_t dtc_events_processed{0};
        std::uint64_t clear_operations{0};
    };
    
    /// @brief Get global statistics
    [[nodiscard]] GlobalStatistics statistics() const;
    
    // =========================================================================
    // Event Handling
    // =========================================================================
    
    /// @brief Register event callback
    void on_event(DTCEventCallback callback);
    
    /// @brief Clear all callbacks
    void clear_callbacks();
    
    // =========================================================================
    // State Management
    // =========================================================================
    
    /// @brief Clear all DTCs for an ECU
    void clear_ecu(LogicalAddress ecu_address);
    
    /// @brief Clear all DTCs
    void clear_all();
    
    /// @brief Reset all state including history
    void reset();
    
private:
    struct ECUData {
        std::unordered_map<std::uint32_t, DTCRecord> dtcs;  // Key: DTC as 24-bit value
        std::vector<DTCRecord> cleared_history;
        std::chrono::steady_clock::time_point last_read;
        std::chrono::steady_clock::time_point last_clear;
    };
    
    /// @brief Get or create a DTC record
    /// @return Pair of (record reference, true if newly created)
    std::pair<DTCRecord&, bool> get_or_create_dtc(LogicalAddress ecu, const uds::DTC& dtc);
    bool matches_filter(const DTCRecord& record, const DTCFilter& filter) const;
    void emit_event(DTCEvent event, LogicalAddress ecu, const DTCRecord* record);
    void enforce_limits(LogicalAddress ecu);
    
    Options options_;
    GlobalStatistics stats_;
    
    std::unordered_map<LogicalAddress, ECUData> ecu_data_;
    std::vector<DTCEventCallback> callbacks_;
    mutable std::mutex mutex_;
};

}  // namespace wadjet::protocols::diagnostic

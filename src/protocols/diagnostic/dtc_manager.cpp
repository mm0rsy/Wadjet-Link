/// @file dtc_manager.cpp
/// @brief DTC (Diagnostic Trouble Code) manager implementation
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/dtc_manager.hpp"

#include <algorithm>

namespace wadjet::protocols::diagnostic {

// =============================================================================
// String Conversion Functions
// =============================================================================

std::string_view dtc_severity_string(DTCSeverity severity) {
    switch (severity) {
        case DTCSeverity::NoSeverityAvailable:
            return "NoSeverityAvailable";
        case DTCSeverity::MaintenanceOnly:
            return "MaintenanceOnly";
        case DTCSeverity::CheckAtNextHalt:
            return "CheckAtNextHalt";
        case DTCSeverity::CheckImmediately:
            return "CheckImmediately";
        case DTCSeverity::MaintenanceOrCheckHalt:
            return "MaintenanceOrCheckHalt";
        case DTCSeverity::MaintenanceOrCheckImmediate:
            return "MaintenanceOrCheckImmediate";
        case DTCSeverity::CheckHaltOrImmediate:
            return "CheckHaltOrImmediate";
        case DTCSeverity::AllSeverities:
            return "AllSeverities";
    }
    return "Unknown";
}

std::string_view dtc_event_string(DTCEvent event) {
    switch (event) {
        case DTCEvent::DTCAdded:
            return "DTCAdded";
        case DTCEvent::DTCUpdated:
            return "DTCUpdated";
        case DTCEvent::DTCCleared:
            return "DTCCleared";
        case DTCEvent::AllDTCsCleared:
            return "AllDTCsCleared";
        case DTCEvent::SnapshotReceived:
            return "SnapshotReceived";
        case DTCEvent::ExtendedReceived:
            return "ExtendedReceived";
        case DTCEvent::DTCCountChanged:
            return "DTCCountChanged";
    }
    return "Unknown";
}

// =============================================================================
// DTCManager Implementation
// =============================================================================

DTCManager::DTCManager(Options opts) : options_(std::move(opts)) {}

DTCManager::~DTCManager() = default;

// Note: DTCManager is not movable due to std::mutex member

// =============================================================================
// Response Processing
// =============================================================================

void DTCManager::process_read_dtc_response(const uds::ReadDTCInformationResponse& resp,
                                           LogicalAddress ecu_address,
                                           std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    stats_.dtc_events_processed++;

    auto& ecu_data = ecu_data_[ecu_address];
    ecu_data.last_read = timestamp;

    std::size_t old_count = ecu_data.dtcs.size();

    // Process DTC list
    for (const auto& dtc_status : resp.dtc_list) {
        add_dtc(dtc_status.dtc, dtc_status.status, ecu_address, timestamp);
    }

    // Handle snapshot and extended data responses
    if (!resp.dtc_record_data.empty()) {
        // This contains additional DTC data (snapshot or extended)
        // The format depends on sub_function
        switch (resp.sub_function) {
            case uds::ReadDTCSubFunction::ReportDTCSnapshotRecordByDTCNumber:
            case uds::ReadDTCSubFunction::ReportDTCSnapshotIdentification:
                // Snapshot data - first 3 bytes are DTC
                if (resp.dtc_record_data.size() >= 3) {
                    uds::DTC dtc(resp.dtc_record_data[0], resp.dtc_record_data[1],
                                 resp.dtc_record_data[2]);

                    // Record number follows DTC
                    std::uint8_t record_num = 0;
                    if (resp.dtc_record_data.size() >= 4) {
                        record_num = resp.dtc_record_data[3];
                    }

                    // Rest is snapshot data
                    if (resp.dtc_record_data.size() > 4) {
                        add_snapshot(dtc, ecu_address, record_num,
                                     std::span(resp.dtc_record_data).subspan(4), timestamp);
                    }
                }
                break;

            case uds::ReadDTCSubFunction::ReportDTCExtDataRecordByDTCNumber:
            case uds::ReadDTCSubFunction::ReportDTCExtDataRecordByRecordNumber:
                // Extended data
                if (resp.dtc_record_data.size() >= 3) {
                    uds::DTC dtc(resp.dtc_record_data[0], resp.dtc_record_data[1],
                                 resp.dtc_record_data[2]);

                    if (resp.dtc_record_data.size() > 3) {
                        add_extended_data(dtc, ecu_address,
                                          std::span(resp.dtc_record_data).subspan(3), timestamp);
                    }
                }
                break;

            default:
                break;
        }
    }

    // Check if count changed
    if (ecu_data.dtcs.size() != old_count) {
        emit_event(DTCEvent::DTCCountChanged, ecu_address, nullptr);
    }
}

void DTCManager::process_clear_dtc_response(const uds::ClearDiagnosticInformationRequest& req,
                                            LogicalAddress ecu_address, bool success,
                                            std::chrono::steady_clock::time_point timestamp) {
    if (!success)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    stats_.clear_operations++;

    auto& ecu_data = ecu_data_[ecu_address];
    ecu_data.last_clear = timestamp;

    if (req.is_clear_all()) {
        // Clear all DTCs
        if (options_.keep_cleared_history) {
            // Move to history
            for (auto& [key, record] : ecu_data.dtcs) {
                ecu_data.cleared_history.push_back(std::move(record));

                // Enforce history limit
                while (ecu_data.cleared_history.size() > options_.max_cleared_history) {
                    ecu_data.cleared_history.erase(ecu_data.cleared_history.begin());
                }
            }
        }

        std::size_t cleared_count = ecu_data.dtcs.size();
        ecu_data.dtcs.clear();

        if (cleared_count > 0) {
            emit_event(DTCEvent::AllDTCsCleared, ecu_address, nullptr);
        }
    } else {
        // Clear specific DTC group
        uds::DTC dtc = uds::DTC::from_value(req.group_of_dtc);
        auto key = dtc.to_value();

        auto it = ecu_data.dtcs.find(key);
        if (it != ecu_data.dtcs.end()) {
            if (options_.keep_cleared_history) {
                ecu_data.cleared_history.push_back(std::move(it->second));
            }

            emit_event(DTCEvent::DTCCleared, ecu_address, &it->second);
            ecu_data.dtcs.erase(it);
        }
    }
}

void DTCManager::add_dtc(const uds::DTC& dtc, const uds::DTCStatusMask& status,
                         LogicalAddress ecu_address,
                         std::chrono::steady_clock::time_point timestamp) {
    // Note: Called with mutex already held from public functions
    // or needs lock for direct public calls

    auto [record, is_new] = get_or_create_dtc(ecu_address, dtc);

    // Check if status changed
    bool status_changed = (record.status.test_failed != status.test_failed ||
                           record.status.confirmed_dtc != status.confirmed_dtc ||
                           record.status.pending_dtc != status.pending_dtc);

    // Update record
    record.status = status;
    record.last_updated = timestamp;

    if (!is_new && options_.track_occurrences) {
        record.occurrence_count++;
    }

    // Emit events
    if (is_new) {
        stats_.total_dtcs++;
        if (status.test_failed)
            stats_.active_dtcs++;
        if (status.confirmed_dtc)
            stats_.confirmed_dtcs++;
        emit_event(DTCEvent::DTCAdded, ecu_address, &record);
    } else if (status_changed) {
        emit_event(DTCEvent::DTCUpdated, ecu_address, &record);
    }
}

void DTCManager::add_snapshot(const uds::DTC& dtc, LogicalAddress ecu_address,
                              std::uint8_t record_number, std::span<const std::uint8_t> data,
                              std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [record, _] = get_or_create_dtc(ecu_address, dtc);

    DTCRecord::SnapshotRecord snapshot;
    snapshot.record_number = record_number;
    snapshot.data.assign(data.begin(), data.end());
    snapshot.timestamp = timestamp;

    record.snapshots.push_back(std::move(snapshot));

    emit_event(DTCEvent::SnapshotReceived, ecu_address, &record);
}

void DTCManager::add_extended_data(const uds::DTC& dtc, LogicalAddress ecu_address,
                                   std::span<const std::uint8_t> data,
                                   std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [record, _ignore] = get_or_create_dtc(ecu_address, dtc);

    record.extended_data.assign(data.begin(), data.end());
    record.last_updated = timestamp;

    emit_event(DTCEvent::ExtendedReceived, ecu_address, &record);
}

// =============================================================================
// DTC Queries
// =============================================================================

std::vector<DTCRecord> DTCManager::get_dtcs(const DTCFilter& filter) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<DTCRecord> result;

    for (const auto& [ecu, data] : ecu_data_) {
        // Filter by ECU
        if (filter.ecu_address != 0 && filter.ecu_address != ecu) {
            continue;
        }

        for (const auto& [key, record] : data.dtcs) {
            if (matches_filter(record, filter)) {
                result.push_back(record);
            }
        }
    }

    return result;
}

const DTCRecord* DTCManager::get_dtc(const uds::DTC& dtc, LogicalAddress ecu_address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto ecu_it = ecu_data_.find(ecu_address);
    if (ecu_it == ecu_data_.end())
        return nullptr;

    auto key = dtc.to_value();
    auto dtc_it = ecu_it->second.dtcs.find(key);
    if (dtc_it == ecu_it->second.dtcs.end())
        return nullptr;

    return &dtc_it->second;
}

std::vector<DTCRecord> DTCManager::get_ecu_dtcs(LogicalAddress ecu_address) const {
    return get_dtcs(DTCFilter::for_ecu(ecu_address));
}

std::size_t DTCManager::count_dtcs(const DTCFilter& filter) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::size_t count = 0;

    for (const auto& [ecu, data] : ecu_data_) {
        if (filter.ecu_address != 0 && filter.ecu_address != ecu) {
            continue;
        }

        for (const auto& [key, record] : data.dtcs) {
            if (matches_filter(record, filter)) {
                count++;
            }
        }
    }

    return count;
}

bool DTCManager::has_dtc(const uds::DTC& dtc, LogicalAddress ecu_address) const {
    return get_dtc(dtc, ecu_address) != nullptr;
}

std::vector<LogicalAddress> DTCManager::get_ecus_with_dtcs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<LogicalAddress> result;
    for (const auto& [ecu, data] : ecu_data_) {
        if (!data.dtcs.empty()) {
            result.push_back(ecu);
        }
    }
    return result;
}

// =============================================================================
// History
// =============================================================================

std::vector<DTCRecord> DTCManager::get_cleared_history(LogicalAddress ecu_address,
                                                       std::size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ecu_data_.find(ecu_address);
    if (it == ecu_data_.end()) {
        return {};
    }

    const auto& hist = it->second.cleared_history;
    std::size_t count = std::min(max_count, hist.size());

    return std::vector<DTCRecord>(hist.end() - static_cast<std::ptrdiff_t>(count), hist.end());
}

// =============================================================================
// Statistics
// =============================================================================

ECUDTCStatistics DTCManager::get_ecu_statistics(LogicalAddress ecu_address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    ECUDTCStatistics stats;
    stats.ecu_address = ecu_address;

    auto it = ecu_data_.find(ecu_address);
    if (it == ecu_data_.end()) {
        return stats;
    }

    const auto& data = it->second;
    stats.total_dtcs = data.dtcs.size();
    stats.last_read = data.last_read;
    stats.last_clear = data.last_clear;

    for (const auto& [key, record] : data.dtcs) {
        if (record.status.test_failed)
            stats.active_dtcs++;
        if (record.status.confirmed_dtc)
            stats.confirmed_dtcs++;
        if (record.status.pending_dtc)
            stats.pending_dtcs++;
        if (record.status.test_failed_since_last_clear)
            stats.dtcs_since_clear++;
    }

    return stats;
}

std::vector<ECUDTCStatistics> DTCManager::get_all_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ECUDTCStatistics> result;
    for (const auto& [ecu, data] : ecu_data_) {
        result.push_back(get_ecu_statistics(ecu));
    }
    return result;
}

DTCManager::GlobalStatistics DTCManager::statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    GlobalStatistics stats = stats_;
    stats.total_ecus = ecu_data_.size();

    // Recalculate counts
    stats.total_dtcs = 0;
    stats.active_dtcs = 0;
    stats.confirmed_dtcs = 0;

    for (const auto& [ecu, data] : ecu_data_) {
        stats.total_dtcs += data.dtcs.size();
        for (const auto& [key, record] : data.dtcs) {
            if (record.status.test_failed)
                stats.active_dtcs++;
            if (record.status.confirmed_dtc)
                stats.confirmed_dtcs++;
        }
    }

    return stats;
}

// =============================================================================
// Event Handling
// =============================================================================

void DTCManager::on_event(DTCEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void DTCManager::clear_callbacks() {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.clear();
}

// =============================================================================
// State Management
// =============================================================================

void DTCManager::clear_ecu(LogicalAddress ecu_address) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ecu_data_.find(ecu_address);
    if (it != ecu_data_.end()) {
        it->second.dtcs.clear();
        emit_event(DTCEvent::AllDTCsCleared, ecu_address, nullptr);
    }
}

void DTCManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [ecu, data] : ecu_data_) {
        data.dtcs.clear();
        emit_event(DTCEvent::AllDTCsCleared, ecu, nullptr);
    }
}

void DTCManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    ecu_data_.clear();
    stats_ = GlobalStatistics{};
}

// =============================================================================
// Private Helpers
// =============================================================================

std::pair<DTCRecord&, bool> DTCManager::get_or_create_dtc(LogicalAddress ecu, const uds::DTC& dtc) {
    auto& ecu_data = ecu_data_[ecu];
    auto key = dtc.to_value();

    auto it = ecu_data.dtcs.find(key);
    if (it != ecu_data.dtcs.end()) {
        return {it->second, false};  // Existing record
    }

    // Create new record
    DTCRecord record;
    record.dtc = dtc;
    record.ecu_address = ecu;
    record.first_seen = std::chrono::steady_clock::now();
    record.last_updated = record.first_seen;
    record.occurrence_count = 1;

    ecu_data.dtcs[key] = std::move(record);

    enforce_limits(ecu);

    return {ecu_data.dtcs[key], true};  // Newly created
}

bool DTCManager::matches_filter(const DTCRecord& record, const DTCFilter& filter) const {
    // ECU filter (already handled in caller, but double-check)
    if (filter.ecu_address != 0 && filter.ecu_address != record.ecu_address) {
        return false;
    }

    // Active filter
    if (filter.active_only && !record.is_active()) {
        return false;
    }

    // Confirmed filter
    if (filter.confirmed_only && !record.is_confirmed()) {
        return false;
    }

    // Pending filter
    if (filter.pending_only && !record.is_pending()) {
        return false;
    }

    // Age filter
    if (filter.max_age.has_value()) {
        if (record.age() > *filter.max_age) {
            return false;
        }
    }

    // Category prefix filter
    if (filter.category_prefix.has_value()) {
        std::string dtc_str = record.dtc_string();
        if (dtc_str.empty() || dtc_str[0] != *filter.category_prefix) {
            return false;
        }
    }

    // Status mask filter
    if (filter.status_mask.has_value()) {
        const auto& mask = *filter.status_mask;
        const auto& status = record.status;

        // Check if any requested status bits are set
        bool matches = false;
        if (mask.test_failed && status.test_failed)
            matches = true;
        if (mask.confirmed_dtc && status.confirmed_dtc)
            matches = true;
        if (mask.pending_dtc && status.pending_dtc)
            matches = true;
        if (mask.test_failed_since_last_clear && status.test_failed_since_last_clear)
            matches = true;

        if (!matches)
            return false;
    }

    return true;
}

void DTCManager::emit_event(DTCEvent event, LogicalAddress ecu, const DTCRecord* record) {
    for (const auto& callback : callbacks_) {
        callback(event, ecu, record);
    }
}

void DTCManager::enforce_limits(LogicalAddress ecu) {
    auto& data = ecu_data_[ecu];

    // Enforce per-ECU limit
    while (data.dtcs.size() > options_.max_dtcs_per_ecu) {
        // Remove oldest DTC
        auto oldest = data.dtcs.begin();
        for (auto it = data.dtcs.begin(); it != data.dtcs.end(); ++it) {
            if (it->second.first_seen < oldest->second.first_seen) {
                oldest = it;
            }
        }
        data.dtcs.erase(oldest);
    }

    // Enforce total limit (remove from ECU with most DTCs)
    std::size_t total = 0;
    for (const auto& [e, d] : ecu_data_) {
        total += d.dtcs.size();
    }

    while (total > options_.max_total_dtcs) {
        // Find ECU with most DTCs
        LogicalAddress max_ecu = 0;
        std::size_t max_count = 0;
        for (const auto& [e, d] : ecu_data_) {
            if (d.dtcs.size() > max_count) {
                max_count = d.dtcs.size();
                max_ecu = e;
            }
        }

        if (max_ecu == 0)
            break;

        auto& max_data = ecu_data_[max_ecu];
        if (max_data.dtcs.empty())
            break;

        // Remove oldest
        auto oldest = max_data.dtcs.begin();
        for (auto it = max_data.dtcs.begin(); it != max_data.dtcs.end(); ++it) {
            if (it->second.first_seen < oldest->second.first_seen) {
                oldest = it;
            }
        }
        max_data.dtcs.erase(oldest);
        total--;
    }
}

}  // namespace wadjet::protocols::diagnostic

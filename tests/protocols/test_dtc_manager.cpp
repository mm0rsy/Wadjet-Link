/// @file test_dtc_manager.cpp
/// @brief Tests for DTC (Diagnostic Trouble Code) manager

#include "wadjet/protocols/diagnostic/dtc_manager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace wadjet::protocols::diagnostic {
namespace {

using namespace ::testing;
using namespace std::chrono_literals;

// =============================================================================
// DTCManager Tests
// =============================================================================

class DTCManagerTest : public Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<DTCManager>();

        // Track events for verification
        manager_->on_event([this](DTCEvent event, LogicalAddress ecu, const DTCRecord* record) {
            events_.push_back({event, ecu, record ? std::optional(record->dtc) : std::nullopt});
        });
    }

    std::unique_ptr<DTCManager> manager_;

    struct EventRecord {
        DTCEvent event;
        LogicalAddress ecu;
        std::optional<uds::DTC> dtc;
    };
    std::vector<EventRecord> events_;

    static constexpr LogicalAddress ECU1 = 0x1234;
    static constexpr LogicalAddress ECU2 = 0x5678;

    // Helper to create a DTC
    static uds::DTC make_dtc(std::uint8_t h, std::uint8_t m, std::uint8_t l) {
        return uds::DTC(h, m, l);
    }

    // Helper to create a status mask
    static uds::DTCStatusMask make_status(bool active, bool confirmed, bool pending) {
        uds::DTCStatusMask status;
        status.test_failed = active;
        status.confirmed_dtc = confirmed;
        status.pending_dtc = pending;
        return status;
    }
};

TEST_F(DTCManagerTest, AddSingleDTC) {
    auto dtc = make_dtc(0x01, 0x23, 0x45);
    auto status = make_status(true, true, false);

    manager_->add_dtc(dtc, status, ECU1);

    auto* record = manager_->get_dtc(dtc, ECU1);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->dtc.high_byte, 0x01);
    EXPECT_EQ(record->dtc.middle_byte, 0x23);
    EXPECT_EQ(record->dtc.low_byte, 0x45);
    EXPECT_TRUE(record->is_active());
    EXPECT_TRUE(record->is_confirmed());
    EXPECT_FALSE(record->is_pending());

    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.back().event, DTCEvent::DTCAdded);
}

TEST_F(DTCManagerTest, UpdateExistingDTC) {
    auto dtc = make_dtc(0x01, 0x23, 0x45);

    // Add with active status
    auto status1 = make_status(true, false, false);
    manager_->add_dtc(dtc, status1, ECU1);

    events_.clear();

    // Update to confirmed
    auto status2 = make_status(true, true, false);
    manager_->add_dtc(dtc, status2, ECU1);

    auto* record = manager_->get_dtc(dtc, ECU1);
    ASSERT_NE(record, nullptr);
    EXPECT_TRUE(record->is_confirmed());
    EXPECT_GE(record->occurrence_count, 1u);

    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.back().event, DTCEvent::DTCUpdated);
}

TEST_F(DTCManagerTest, ProcessReadDTCResponse) {
    uds::ReadDTCInformationResponse resp;
    resp.sub_function = uds::ReadDTCSubFunction::ReportDTCByStatusMask;

    uds::DTCAndStatus dtc1;
    dtc1.dtc = make_dtc(0x01, 0x23, 0x45);
    dtc1.status = make_status(true, true, false);

    uds::DTCAndStatus dtc2;
    dtc2.dtc = make_dtc(0x02, 0x34, 0x56);
    dtc2.status = make_status(false, true, true);

    resp.dtc_list = {dtc1, dtc2};

    manager_->process_read_dtc_response(resp, ECU1);

    EXPECT_EQ(manager_->count_dtcs(DTCFilter::for_ecu(ECU1)), 2);

    auto dtcs = manager_->get_ecu_dtcs(ECU1);
    EXPECT_EQ(dtcs.size(), 2);
}

TEST_F(DTCManagerTest, ClearAllDTCs) {
    // Add some DTCs
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(true, true, false), ECU1);

    EXPECT_EQ(manager_->count_dtcs(DTCFilter::for_ecu(ECU1)), 2);

    events_.clear();

    // Clear all
    uds::ClearDiagnosticInformationRequest clear_req;
    clear_req.group_of_dtc = 0xFFFFFF;  // All

    manager_->process_clear_dtc_response(clear_req, ECU1, true);

    EXPECT_EQ(manager_->count_dtcs(DTCFilter::for_ecu(ECU1)), 0);

    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.back().event, DTCEvent::AllDTCsCleared);
}

TEST_F(DTCManagerTest, FilterByActive) {
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, false, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(false, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x03, 0x00, 0x00), make_status(true, true, false), ECU1);

    auto active_dtcs = manager_->get_dtcs(DTCFilter::active());
    EXPECT_EQ(active_dtcs.size(), 2);  // DTCs 1 and 3

    for (const auto& record : active_dtcs) {
        EXPECT_TRUE(record.is_active());
    }
}

TEST_F(DTCManagerTest, FilterByConfirmed) {
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, false, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(false, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x03, 0x00, 0x00), make_status(true, true, false), ECU1);

    auto confirmed = manager_->get_dtcs(DTCFilter::confirmed());
    EXPECT_EQ(confirmed.size(), 2);  // DTCs 2 and 3
}

TEST_F(DTCManagerTest, MultipleECUs) {
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(true, true, false), ECU2);
    manager_->add_dtc(make_dtc(0x03, 0x00, 0x00), make_status(true, true, false), ECU2);

    EXPECT_EQ(manager_->count_dtcs(DTCFilter::for_ecu(ECU1)), 1);
    EXPECT_EQ(manager_->count_dtcs(DTCFilter::for_ecu(ECU2)), 2);
    EXPECT_EQ(manager_->count_dtcs(DTCFilter::all()), 3);

    auto ecus = manager_->get_ecus_with_dtcs();
    EXPECT_EQ(ecus.size(), 2);
    EXPECT_THAT(ecus, UnorderedElementsAre(ECU1, ECU2));
}

TEST_F(DTCManagerTest, Statistics) {
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(false, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x03, 0x00, 0x00), make_status(true, false, true), ECU2);

    auto stats = manager_->statistics();
    EXPECT_EQ(stats.total_dtcs, 3);
    EXPECT_EQ(stats.total_ecus, 2);
    EXPECT_EQ(stats.active_dtcs, 2);
    EXPECT_EQ(stats.confirmed_dtcs, 2);
}

TEST_F(DTCManagerTest, ECUStatistics) {
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(false, true, true), ECU1);
    manager_->add_dtc(make_dtc(0x03, 0x00, 0x00), make_status(true, false, false), ECU1);

    auto ecu_stats = manager_->get_ecu_statistics(ECU1);
    EXPECT_EQ(ecu_stats.ecu_address, ECU1);
    EXPECT_EQ(ecu_stats.total_dtcs, 3);
    EXPECT_EQ(ecu_stats.active_dtcs, 2);
    EXPECT_EQ(ecu_stats.confirmed_dtcs, 2);
    EXPECT_EQ(ecu_stats.pending_dtcs, 1);
}

TEST_F(DTCManagerTest, AddSnapshot) {
    auto dtc = make_dtc(0x01, 0x23, 0x45);
    manager_->add_dtc(dtc, make_status(true, true, false), ECU1);

    events_.clear();

    std::vector<std::uint8_t> snapshot_data = {0x01, 0x02, 0x03, 0x04};
    manager_->add_snapshot(dtc, ECU1, 1, snapshot_data);

    auto* record = manager_->get_dtc(dtc, ECU1);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->snapshots.size(), 1);
    EXPECT_EQ(record->snapshots[0].record_number, 1);
    EXPECT_EQ(record->snapshots[0].data, snapshot_data);

    EXPECT_EQ(events_.back().event, DTCEvent::SnapshotReceived);
}

TEST_F(DTCManagerTest, AddExtendedData) {
    auto dtc = make_dtc(0x01, 0x23, 0x45);
    manager_->add_dtc(dtc, make_status(true, true, false), ECU1);

    events_.clear();

    std::vector<std::uint8_t> extended_data = {0xAA, 0xBB, 0xCC};
    manager_->add_extended_data(dtc, ECU1, extended_data);

    auto* record = manager_->get_dtc(dtc, ECU1);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->extended_data, extended_data);

    EXPECT_EQ(events_.back().event, DTCEvent::ExtendedReceived);
}

TEST_F(DTCManagerTest, ClearedHistory) {
    DTCManager::Options opts;
    opts.keep_cleared_history = true;
    manager_ = std::make_unique<DTCManager>(opts);

    // Add DTC
    auto dtc = make_dtc(0x01, 0x23, 0x45);
    manager_->add_dtc(dtc, make_status(true, true, false), ECU1);

    // Clear
    uds::ClearDiagnosticInformationRequest clear_req;
    clear_req.group_of_dtc = 0xFFFFFF;
    manager_->process_clear_dtc_response(clear_req, ECU1, true);

    // Check history
    auto history = manager_->get_cleared_history(ECU1);
    EXPECT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].dtc.high_byte, 0x01);
}

TEST_F(DTCManagerTest, Reset) {
    manager_->add_dtc(make_dtc(0x01, 0x00, 0x00), make_status(true, true, false), ECU1);
    manager_->add_dtc(make_dtc(0x02, 0x00, 0x00), make_status(true, true, false), ECU2);

    EXPECT_EQ(manager_->count_dtcs(), 2);

    manager_->reset();

    EXPECT_EQ(manager_->count_dtcs(), 0);
    EXPECT_TRUE(manager_->get_ecus_with_dtcs().empty());
}

// =============================================================================
// DTCRecord Tests
// =============================================================================

TEST(DTCRecordTest, DTCStringPowertrain) {
    DTCRecord record;
    record.dtc = uds::DTC(0x01, 0x23, 0x00);  // P0123

    std::string str = record.dtc_string();
    EXPECT_EQ(str[0], 'P');
}

TEST(DTCRecordTest, DTCStringChassis) {
    DTCRecord record;
    record.dtc = uds::DTC(0x40, 0x00, 0x00);  // Chassis category

    std::string str = record.dtc_string();
    EXPECT_EQ(str[0], 'C');
}

TEST(DTCRecordTest, DTCStringBody) {
    DTCRecord record;
    record.dtc = uds::DTC(0x60, 0x00, 0x00);  // Body category

    std::string str = record.dtc_string();
    EXPECT_EQ(str[0], 'B');
}

TEST(DTCRecordTest, DTCStringNetwork) {
    DTCRecord record;
    record.dtc = uds::DTC(0x80, 0x00, 0x00);  // Network category

    std::string str = record.dtc_string();
    EXPECT_EQ(str[0], 'U');
}

TEST(DTCRecordTest, StatusHelpers) {
    DTCRecord record;

    record.status.test_failed = true;
    EXPECT_TRUE(record.is_active());

    record.status.confirmed_dtc = true;
    EXPECT_TRUE(record.is_confirmed());

    record.status.pending_dtc = true;
    EXPECT_TRUE(record.is_pending());

    record.status.test_failed_since_last_clear = true;
    EXPECT_TRUE(record.failed_since_clear());
}

// =============================================================================
// DTCFilter Tests
// =============================================================================

TEST(DTCFilterTest, AllFilter) {
    auto filter = DTCFilter::all();
    EXPECT_EQ(filter.ecu_address, 0);
    EXPECT_FALSE(filter.active_only);
    EXPECT_FALSE(filter.confirmed_only);
}

TEST(DTCFilterTest, ActiveFilter) {
    auto filter = DTCFilter::active();
    EXPECT_TRUE(filter.active_only);
}

TEST(DTCFilterTest, ConfirmedFilter) {
    auto filter = DTCFilter::confirmed();
    EXPECT_TRUE(filter.confirmed_only);
}

TEST(DTCFilterTest, ForECUFilter) {
    auto filter = DTCFilter::for_ecu(0x1234);
    EXPECT_EQ(filter.ecu_address, 0x1234);
}

// =============================================================================
// String Conversion Tests
// =============================================================================

TEST(DTCStringTest, SeverityStrings) {
    EXPECT_EQ(dtc_severity_string(DTCSeverity::NoSeverityAvailable), "NoSeverityAvailable");
    EXPECT_EQ(dtc_severity_string(DTCSeverity::MaintenanceOnly), "MaintenanceOnly");
    EXPECT_EQ(dtc_severity_string(DTCSeverity::CheckImmediately), "CheckImmediately");
}

TEST(DTCStringTest, EventStrings) {
    EXPECT_EQ(dtc_event_string(DTCEvent::DTCAdded), "DTCAdded");
    EXPECT_EQ(dtc_event_string(DTCEvent::DTCUpdated), "DTCUpdated");
    EXPECT_EQ(dtc_event_string(DTCEvent::DTCCleared), "DTCCleared");
    EXPECT_EQ(dtc_event_string(DTCEvent::AllDTCsCleared), "AllDTCsCleared");
}

}  // namespace
}  // namespace wadjet::protocols::diagnostic

/// @file test_flash_sequence.cpp
/// @brief Tests for flash programming sequence tracker

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "wadjet/protocols/diagnostic/flash_sequence.hpp"

namespace wadjet::protocols::diagnostic {
namespace {

using namespace ::testing;
using namespace std::chrono_literals;

// =============================================================================
// FlashSequenceTracker Tests
// =============================================================================

class FlashSequenceTrackerTest : public Test {
protected:
    void SetUp() override {
        tracker_ = std::make_unique<FlashSequenceTracker>();
        
        // Track events for verification
        tracker_->on_event([this](FlashEvent event, const FlashSequence& seq) {
            events_.push_back(event);
            last_sequence_ = seq;
        });
    }
    
    std::unique_ptr<FlashSequenceTracker> tracker_;
    std::vector<FlashEvent> events_;
    FlashSequence last_sequence_;
    
    static constexpr LogicalAddress TESTER = 0x0E00;
    static constexpr LogicalAddress ECU = 0x1234;
};

TEST_F(FlashSequenceTrackerTest, ProcessRequestDownload) {
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 0x10000;
    req.data_format = uds::DataFormatIdentifier::from_byte(0x00);
    
    tracker_->process_request_download(req, TESTER, ECU);
    
    ASSERT_TRUE(tracker_->has_active_sequence(ECU));
    
    auto* seq = tracker_->get_active_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(seq->operation_type, FlashOperationType::Download);
    EXPECT_EQ(seq->region.start_address, 0x00080000u);
    EXPECT_EQ(seq->region.size, 0x10000u);
    EXPECT_EQ(seq->state, FlashSequenceState::Downloading);
    
    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.front(), FlashEvent::DownloadStarted);
}

TEST_F(FlashSequenceTrackerTest, ProcessRequestDownloadResponse) {
    // First, initiate download
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 0x10000;
    
    tracker_->process_request_download(req, TESTER, ECU);
    
    // Then process response
    uds::RequestDownloadResponse resp;
    resp.max_number_of_block_length = 4096;
    
    tracker_->process_request_download_response(resp, ECU, TESTER);
    
    auto* seq = tracker_->get_active_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(seq->region.max_block_size, 4096);
    EXPECT_EQ(seq->state, FlashSequenceState::Transferring);
}

TEST_F(FlashSequenceTrackerTest, ProcessTransferDataBlocks) {
    // Setup download
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 8192;  // 8KB
    tracker_->process_request_download(req, TESTER, ECU);
    
    uds::RequestDownloadResponse resp;
    resp.max_number_of_block_length = 4096;
    tracker_->process_request_download_response(resp, ECU, TESTER);
    
    events_.clear();
    
    // Transfer first block
    uds::TransferDataRequest block1;
    block1.block_sequence_counter = 1;
    block1.transfer_request_parameter_record.resize(4096);
    
    tracker_->process_transfer_data(block1, TESTER, ECU);
    
    auto* seq = tracker_->get_active_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(seq->blocks.size(), 1);
    EXPECT_EQ(seq->total_bytes_transferred, 4096);
    EXPECT_EQ(seq->progress_percent(), 50);
    
    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.back(), FlashEvent::BlockTransferred);
    
    // Acknowledge first block
    uds::TransferDataResponse ack1;
    ack1.block_sequence_counter = 1;
    tracker_->process_transfer_data_response(ack1, ECU, TESTER);
    
    EXPECT_TRUE(seq->blocks[0].acknowledged);
    EXPECT_EQ(events_.back(), FlashEvent::BlockAcknowledged);
    
    // Transfer second block
    uds::TransferDataRequest block2;
    block2.block_sequence_counter = 2;
    block2.transfer_request_parameter_record.resize(4096);
    
    tracker_->process_transfer_data(block2, TESTER, ECU);
    
    EXPECT_EQ(seq->blocks.size(), 2);
    EXPECT_EQ(seq->total_bytes_transferred, 8192);
    EXPECT_EQ(seq->progress_percent(), 100);
}

TEST_F(FlashSequenceTrackerTest, ProcessTransferExitSuccess) {
    // Setup and transfer
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 4096;
    tracker_->process_request_download(req, TESTER, ECU);
    
    uds::RequestDownloadResponse resp;
    resp.max_number_of_block_length = 4096;
    tracker_->process_request_download_response(resp, ECU, TESTER);
    
    uds::TransferDataRequest block;
    block.block_sequence_counter = 1;
    block.transfer_request_parameter_record.resize(4096);
    tracker_->process_transfer_data(block, TESTER, ECU);
    
    events_.clear();
    
    // Exit transfer
    uds::RequestTransferExitRequest exit_req;
    tracker_->process_transfer_exit(exit_req, TESTER, ECU);
    
    uds::RequestTransferExitResponse exit_resp;
    tracker_->process_transfer_exit_response(exit_resp, ECU, TESTER);
    
    // Sequence should be complete
    auto* seq = tracker_->get_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(seq->state, FlashSequenceState::Complete);
    EXPECT_TRUE(seq->is_successful());
    
    EXPECT_THAT(events_, Contains(FlashEvent::TransferExitOk));
    EXPECT_THAT(events_, Contains(FlashEvent::SequenceComplete));
}

TEST_F(FlashSequenceTrackerTest, ProcessNegativeResponse) {
    // Setup download
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 4096;
    tracker_->process_request_download(req, TESTER, ECU);
    
    // Send transfer data that fails
    uds::TransferDataRequest block;
    block.block_sequence_counter = 1;
    block.transfer_request_parameter_record.resize(4096);
    tracker_->process_transfer_data(block, TESTER, ECU);
    
    events_.clear();
    
    // Negative response
    uds::NegativeResponseMessage nrc;
    nrc.rejected_service_id = uds::ServiceID::TransferData;
    nrc.negative_response_code = uds::NRC::TransferDataSuspended;
    
    tracker_->process_negative_response(nrc, ECU, TESTER);
    
    auto* seq = tracker_->get_active_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_TRUE(seq->last_error.has_value());
    EXPECT_EQ(*seq->last_error, uds::NRC::TransferDataSuspended);
    
    EXPECT_THAT(events_, Contains(FlashEvent::BlockFailed));
}

TEST_F(FlashSequenceTrackerTest, ProcessEraseRoutine) {
    // Start erase routine
    uds::RoutineControlRequest erase_req;
    erase_req.routine_control_type = uds::RoutineControlType::StartRoutine;
    erase_req.routine_identifier = uds::RoutineID::EraseMemory;
    
    tracker_->process_routine_control(erase_req, TESTER, ECU);
    
    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.back(), FlashEvent::EraseStarted);
    
    auto* seq = tracker_->get_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_TRUE(seq->erase_performed);
    EXPECT_EQ(seq->state, FlashSequenceState::EraseStarted);
    
    // Erase complete response
    uds::RoutineControlResponse erase_resp;
    erase_resp.routine_control_type = uds::RoutineControlType::StartRoutine;
    erase_resp.routine_identifier = uds::RoutineID::EraseMemory;
    
    tracker_->process_routine_control_response(erase_resp, ECU, TESTER);
    
    seq = tracker_->get_sequence(ECU);
    EXPECT_EQ(seq->state, FlashSequenceState::EraseComplete);
    EXPECT_EQ(events_.back(), FlashEvent::EraseCompleted);
}

TEST_F(FlashSequenceTrackerTest, RequestUpload) {
    uds::RequestUploadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 0x10000;
    
    tracker_->process_request_upload(req, TESTER, ECU);
    
    auto* seq = tracker_->get_active_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(seq->operation_type, FlashOperationType::Upload);
    
    EXPECT_EQ(events_.back(), FlashEvent::UploadStarted);
}

TEST_F(FlashSequenceTrackerTest, AbortSequence) {
    // Start sequence
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 0x10000;
    tracker_->process_request_download(req, TESTER, ECU);
    
    events_.clear();
    
    // Abort
    tracker_->abort_sequence(ECU);
    
    // Sequence should be in history now
    EXPECT_FALSE(tracker_->has_active_sequence(ECU));
    
    auto hist = tracker_->get_history(ECU);
    ASSERT_EQ(hist.size(), 1);
    EXPECT_EQ(hist[0].state, FlashSequenceState::Aborted);
    
    EXPECT_EQ(events_.back(), FlashEvent::SequenceAborted);
}

TEST_F(FlashSequenceTrackerTest, Statistics) {
    // Complete a successful sequence
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 1024;
    tracker_->process_request_download(req, TESTER, ECU);
    
    uds::RequestDownloadResponse resp;
    resp.max_number_of_block_length = 1024;
    tracker_->process_request_download_response(resp, ECU, TESTER);
    
    uds::TransferDataRequest block;
    block.block_sequence_counter = 1;
    block.transfer_request_parameter_record.resize(1024);
    tracker_->process_transfer_data(block, TESTER, ECU);
    
    uds::RequestTransferExitResponse exit_resp;
    tracker_->process_transfer_exit_response(exit_resp, ECU, TESTER);
    
    auto stats = tracker_->statistics();
    EXPECT_EQ(stats.sequences_started, 1);
    EXPECT_EQ(stats.sequences_completed, 1);
    EXPECT_EQ(stats.total_bytes_transferred, 1024);
    EXPECT_EQ(stats.total_blocks_transferred, 1);
}

TEST_F(FlashSequenceTrackerTest, TransferRateCalculation) {
    auto start = std::chrono::steady_clock::now();
    
    uds::RequestDownloadRequest req;
    req.memory_address = 0x00080000;
    req.memory_size = 10000;
    tracker_->process_request_download(req, TESTER, ECU, start);
    
    // Simulate time passing
    auto later = start + 100ms;
    
    uds::TransferDataRequest block;
    block.block_sequence_counter = 1;
    block.transfer_request_parameter_record.resize(10000);
    tracker_->process_transfer_data(block, TESTER, ECU, later);
    
    auto* seq = tracker_->get_active_sequence(ECU);
    ASSERT_NE(seq, nullptr);
    
    // 10000 bytes in 100ms = 100000 bytes/second
    EXPECT_GT(seq->transfer_rate_bps(), 50000.0);  // Allow some variance
}

// =============================================================================
// FlashSequence Helper Method Tests
// =============================================================================

TEST(FlashSequenceTest, ProgressCalculation) {
    FlashSequence seq;
    seq.region.size = 1000;
    seq.total_bytes_transferred = 500;
    
    EXPECT_DOUBLE_EQ(seq.progress(), 0.5);
    EXPECT_EQ(seq.progress_percent(), 50);
}

TEST(FlashSequenceTest, ProgressZeroSize) {
    FlashSequence seq;
    seq.region.size = 0;
    seq.total_bytes_transferred = 100;
    
    EXPECT_DOUBLE_EQ(seq.progress(), 0.0);
}

TEST(FlashSequenceTest, StateChecks) {
    FlashSequence seq;
    
    seq.state = FlashSequenceState::Transferring;
    EXPECT_TRUE(seq.is_active());
    EXPECT_FALSE(seq.is_successful());
    EXPECT_FALSE(seq.is_failed());
    
    seq.state = FlashSequenceState::Complete;
    EXPECT_FALSE(seq.is_active());
    EXPECT_TRUE(seq.is_successful());
    EXPECT_FALSE(seq.is_failed());
    
    seq.state = FlashSequenceState::Failed;
    EXPECT_FALSE(seq.is_active());
    EXPECT_FALSE(seq.is_successful());
    EXPECT_TRUE(seq.is_failed());
}

TEST(FlashSequenceTest, BlockCounting) {
    FlashSequence seq;
    
    FlashBlock b1, b2, b3;
    b1.acknowledged = true;
    b2.acknowledged = true;
    b3.acknowledged = false;
    b3.error_code = uds::NRC::GeneralReject;
    
    seq.blocks = {b1, b2, b3};
    
    EXPECT_EQ(seq.successful_blocks(), 2);
    EXPECT_EQ(seq.failed_blocks(), 1);
}

// =============================================================================
// String Conversion Tests
// =============================================================================

TEST(FlashSequenceStringTest, StateStrings) {
    EXPECT_EQ(flash_sequence_state_string(FlashSequenceState::Idle), "Idle");
    EXPECT_EQ(flash_sequence_state_string(FlashSequenceState::Downloading), "Downloading");
    EXPECT_EQ(flash_sequence_state_string(FlashSequenceState::Complete), "Complete");
    EXPECT_EQ(flash_sequence_state_string(FlashSequenceState::Failed), "Failed");
}

TEST(FlashSequenceStringTest, EventStrings) {
    EXPECT_EQ(flash_event_string(FlashEvent::DownloadStarted), "DownloadStarted");
    EXPECT_EQ(flash_event_string(FlashEvent::BlockTransferred), "BlockTransferred");
    EXPECT_EQ(flash_event_string(FlashEvent::SequenceComplete), "SequenceComplete");
}

}  // namespace
}  // namespace wadjet::protocols::diagnostic

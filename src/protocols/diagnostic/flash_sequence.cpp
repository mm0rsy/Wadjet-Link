/// @file flash_sequence.cpp
/// @brief Flash programming sequence tracker implementation
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/flash_sequence.hpp"

namespace wadjet::protocols::diagnostic {

// =============================================================================
// String Conversion Functions
// =============================================================================

std::string_view flash_sequence_state_string(FlashSequenceState state) {
    switch (state) {
        case FlashSequenceState::Idle:
            return "Idle";
        case FlashSequenceState::EraseStarted:
            return "EraseStarted";
        case FlashSequenceState::Erasing:
            return "Erasing";
        case FlashSequenceState::EraseComplete:
            return "EraseComplete";
        case FlashSequenceState::EraseFailed:
            return "EraseFailed";
        case FlashSequenceState::Downloading:
            return "Downloading";
        case FlashSequenceState::Transferring:
            return "Transferring";
        case FlashSequenceState::Verifying:
            return "Verifying";
        case FlashSequenceState::Complete:
            return "Complete";
        case FlashSequenceState::Failed:
            return "Failed";
        case FlashSequenceState::Aborted:
            return "Aborted";
    }
    return "Unknown";
}

std::string_view flash_event_string(FlashEvent event) {
    switch (event) {
        case FlashEvent::EraseStarted:
            return "EraseStarted";
        case FlashEvent::EraseProgress:
            return "EraseProgress";
        case FlashEvent::EraseCompleted:
            return "EraseCompleted";
        case FlashEvent::EraseFailed:
            return "EraseFailed";
        case FlashEvent::DownloadStarted:
            return "DownloadStarted";
        case FlashEvent::UploadStarted:
            return "UploadStarted";
        case FlashEvent::BlockTransferred:
            return "BlockTransferred";
        case FlashEvent::BlockAcknowledged:
            return "BlockAcknowledged";
        case FlashEvent::BlockFailed:
            return "BlockFailed";
        case FlashEvent::TransferExitOk:
            return "TransferExitOk";
        case FlashEvent::TransferExitFail:
            return "TransferExitFail";
        case FlashEvent::VerificationStart:
            return "VerificationStart";
        case FlashEvent::VerificationOk:
            return "VerificationOk";
        case FlashEvent::VerificationFail:
            return "VerificationFail";
        case FlashEvent::SequenceComplete:
            return "SequenceComplete";
        case FlashEvent::SequenceFailed:
            return "SequenceFailed";
        case FlashEvent::SequenceAborted:
            return "SequenceAborted";
    }
    return "Unknown";
}

// =============================================================================
// FlashSequenceTracker Implementation
// =============================================================================

FlashSequenceTracker::FlashSequenceTracker(Options opts) : options_(std::move(opts)) {}

FlashSequenceTracker::~FlashSequenceTracker() = default;

// Note: FlashSequenceTracker is not movable due to std::mutex member

// =============================================================================
// Request Processing
// =============================================================================

void FlashSequenceTracker::process_request_download(
    const uds::RequestDownloadRequest& req, LogicalAddress source, LogicalAddress target,
    std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& seq = get_or_create_sequence(target, source);
    seq.operation_type = FlashOperationType::Download;
    seq.state = FlashSequenceState::Downloading;
    seq.start_time = timestamp;

    // Set region info from request
    seq.region.start_address = req.memory_address;
    seq.region.size = req.memory_size;
    seq.region.data_format = req.data_format.to_byte();

    stats_.sequences_started++;

    emit_event(FlashEvent::DownloadStarted, seq);
}

void FlashSequenceTracker::process_request_download_response(
    const uds::RequestDownloadResponse& resp,
    LogicalAddress source,  // ECU sends the response
    [[maybe_unused]] LogicalAddress target, std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Response comes FROM ECU, so look up by source
    auto it = active_sequences_.find(source);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;
    seq.region.max_block_size = static_cast<std::uint16_t>(resp.max_number_of_block_length);
    seq.state = FlashSequenceState::Transferring;

    // Update duration
    seq.total_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seq.start_time);
}

void FlashSequenceTracker::process_request_upload(const uds::RequestUploadRequest& req,
                                                  LogicalAddress source, LogicalAddress target,
                                                  std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& seq = get_or_create_sequence(target, source);
    seq.operation_type = FlashOperationType::Upload;
    seq.state = FlashSequenceState::Downloading;
    seq.start_time = timestamp;

    // Set region info from request
    seq.region.start_address = req.memory_address;
    seq.region.size = req.memory_size;
    seq.region.data_format = req.data_format.to_byte();

    stats_.sequences_started++;

    emit_event(FlashEvent::UploadStarted, seq);
}

void FlashSequenceTracker::process_request_upload_response(
    const uds::RequestUploadResponse& resp,
    LogicalAddress source,  // ECU sends the response
    [[maybe_unused]] LogicalAddress target, std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Response comes FROM ECU, so look up by source
    auto it = active_sequences_.find(source);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;
    seq.region.max_block_size = static_cast<std::uint16_t>(resp.max_number_of_block_length);
    seq.state = FlashSequenceState::Transferring;

    // Update duration
    seq.total_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seq.start_time);
}

void FlashSequenceTracker::process_transfer_data(const uds::TransferDataRequest& req,
                                                 [[maybe_unused]] LogicalAddress source,
                                                 LogicalAddress target,
                                                 std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = active_sequences_.find(target);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;

    // Create block record
    FlashBlock block;
    block.sequence_counter = req.block_sequence_counter;
    block.data_size = req.transfer_request_parameter_record.size();
    block.timestamp = timestamp;
    block.acknowledged = false;

    seq.blocks.push_back(block);
    seq.total_bytes_transferred += block.data_size;

    // Update expected counter
    seq.expected_sequence_counter =
        static_cast<std::uint8_t>((req.block_sequence_counter % 255) + 1);
    if (seq.expected_sequence_counter == 0) {
        seq.expected_sequence_counter = 1;
    }

    // Update duration
    seq.total_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seq.start_time);

    stats_.total_bytes_transferred += block.data_size;
    stats_.total_blocks_transferred++;

    emit_event(FlashEvent::BlockTransferred, seq);
}

void FlashSequenceTracker::process_transfer_data_response(
    const uds::TransferDataResponse& resp,
    LogicalAddress source,  // ECU sends the response
    [[maybe_unused]] LogicalAddress target, std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Response comes FROM ECU, so look up by source
    auto it = active_sequences_.find(source);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;

    // Find matching block by sequence counter
    for (auto& block : seq.blocks) {
        if (block.sequence_counter == resp.block_sequence_counter && !block.acknowledged) {
            block.acknowledged = true;

            // Update duration
            seq.total_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seq.start_time);

            emit_event(FlashEvent::BlockAcknowledged, seq);
            break;
        }
    }
}

void FlashSequenceTracker::process_transfer_exit(
    [[maybe_unused]] const uds::RequestTransferExitRequest& req,
    [[maybe_unused]] LogicalAddress source, [[maybe_unused]] LogicalAddress target,
    [[maybe_unused]] std::chrono::steady_clock::time_point timestamp) {
    // Wait for response to complete
}

void FlashSequenceTracker::process_transfer_exit_response(
    [[maybe_unused]] const uds::RequestTransferExitResponse& resp,
    LogicalAddress source,  // ECU sends the response
    [[maybe_unused]] LogicalAddress target, std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Response comes FROM ECU, so look up by source
    auto it = active_sequences_.find(source);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;
    seq.end_time = timestamp;
    seq.total_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seq.start_time);

    complete_sequence(seq, true);

    emit_event(FlashEvent::TransferExitOk, seq);
    emit_event(FlashEvent::SequenceComplete, seq);
}

void FlashSequenceTracker::process_negative_response(
    const uds::NegativeResponseMessage& nrc,
    LogicalAddress source,  // ECU sends the response
    [[maybe_unused]] LogicalAddress target, std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Response comes FROM ECU, so look up by source
    auto it = active_sequences_.find(source);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;

    // Check if this is a flash-related service
    auto sid = nrc.rejected_service_id;
    bool flash_related =
        (sid == uds::ServiceID::RequestDownload || sid == uds::ServiceID::RequestUpload ||
         sid == uds::ServiceID::TransferData || sid == uds::ServiceID::RequestTransferExit);

    if (!flash_related)
        return;

    seq.last_error = nrc.negative_response_code;
    seq.error_message = std::string(uds::nrc_string(nrc.negative_response_code));

    // Handle based on service
    if (sid == uds::ServiceID::TransferData) {
        // Mark last block as failed
        if (!seq.blocks.empty()) {
            seq.blocks.back().error_code = nrc.negative_response_code;
        }
        seq.retry_count++;
        emit_event(FlashEvent::BlockFailed, seq);

        // Too many retries = failure
        if (seq.retry_count > 3) {
            seq.end_time = timestamp;
            seq.total_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seq.start_time);
            complete_sequence(seq, false);
            emit_event(FlashEvent::SequenceFailed, seq);
        }
    } else if (sid == uds::ServiceID::RequestTransferExit) {
        emit_event(FlashEvent::TransferExitFail, seq);
        seq.end_time = timestamp;
        complete_sequence(seq, false);
        emit_event(FlashEvent::SequenceFailed, seq);
    } else {
        // RequestDownload or RequestUpload failed
        seq.end_time = timestamp;
        complete_sequence(seq, false);
        emit_event(FlashEvent::SequenceFailed, seq);
    }
}

void FlashSequenceTracker::process_routine_control(
    const uds::RoutineControlRequest& req, LogicalAddress source, LogicalAddress target,
    std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check for erase memory routine (0xFF00)
    if (!options_.track_erase_routines)
        return;

    if (req.routine_identifier == uds::RoutineID::EraseMemory) {
        auto& seq = get_or_create_sequence(target, source);

        if (req.routine_control_type == uds::RoutineControlType::StartRoutine) {
            seq.state = FlashSequenceState::EraseStarted;
            seq.erase_performed = true;
            stats_.erase_operations++;
            emit_event(FlashEvent::EraseStarted, seq);
        } else if (req.routine_control_type == uds::RoutineControlType::RequestRoutineResults) {
            // Progress check
            emit_event(FlashEvent::EraseProgress, seq);
        }
    }

    // Check for checksum verification routines (typically 0xFF01 or OEM-specific)
    // Exclude EraseMemory (0xFF00) which is handled above
    if (options_.track_verification) {
        if (req.routine_identifier == uds::RoutineID::CheckProgrammingDependencies ||
            (req.routine_identifier.is_oem_specific() &&
             req.routine_identifier != uds::RoutineID::EraseMemory)) {
            auto it = active_sequences_.find(target);
            if (it != active_sequences_.end()) {
                it->second.state = FlashSequenceState::Verifying;
                stats_.verification_operations++;
                emit_event(FlashEvent::VerificationStart, it->second);
            }
        }
    }

    (void)timestamp;  // May be used for timing
}

void FlashSequenceTracker::process_routine_control_response(
    const uds::RoutineControlResponse& resp,
    LogicalAddress source,  // ECU sends the response
    [[maybe_unused]] LogicalAddress target, std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Response comes FROM ECU, so look up by source
    auto it = active_sequences_.find(source);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;

    // Handle erase memory response
    if (resp.routine_identifier == uds::RoutineID::EraseMemory) {
        if (seq.state == FlashSequenceState::EraseStarted ||
            seq.state == FlashSequenceState::Erasing) {
            seq.state = FlashSequenceState::EraseComplete;

            // Calculate erase duration
            if (seq.start_time != std::chrono::steady_clock::time_point{}) {
                seq.erase_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    timestamp - seq.start_time);
            }

            emit_event(FlashEvent::EraseCompleted, seq);
        }
    }

    // Handle verification response
    if (seq.state == FlashSequenceState::Verifying) {
        seq.checksum_verified = true;

        // Extract checksum from response if available
        if (!resp.routine_status_record.empty() && resp.routine_status_record.size() >= 4) {
            std::uint32_t checksum = 0;
            for (std::size_t i = 0; i < 4 && i < resp.routine_status_record.size(); ++i) {
                checksum = (checksum << 8) | resp.routine_status_record[i];
            }
            seq.checksum_value = checksum;
        }

        emit_event(FlashEvent::VerificationOk, seq);
    }
}

// =============================================================================
// Sequence Access
// =============================================================================

const FlashSequence* FlashSequenceTracker::get_active_sequence(LogicalAddress ecu) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = active_sequences_.find(ecu);
    if (it != active_sequences_.end() && it->second.is_active()) {
        return &it->second;
    }
    return nullptr;
}

const FlashSequence* FlashSequenceTracker::get_sequence(LogicalAddress ecu) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = active_sequences_.find(ecu);
    if (it != active_sequences_.end()) {
        return &it->second;
    }

    // Check history
    auto hist_it = history_.find(ecu);
    if (hist_it != history_.end() && !hist_it->second.empty()) {
        return &hist_it->second.back();
    }

    return nullptr;
}

std::vector<const FlashSequence*> FlashSequenceTracker::get_active_sequences() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const FlashSequence*> result;
    for (const auto& [ecu, seq] : active_sequences_) {
        if (seq.is_active()) {
            result.push_back(&seq);
        }
    }
    return result;
}

std::vector<FlashSequence> FlashSequenceTracker::get_history(LogicalAddress ecu,
                                                             std::size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = history_.find(ecu);
    if (it == history_.end()) {
        return {};
    }

    const auto& hist = it->second;
    std::size_t count = std::min(max_count, hist.size());

    // Return most recent entries
    return std::vector<FlashSequence>(hist.end() - static_cast<std::ptrdiff_t>(count), hist.end());
}

bool FlashSequenceTracker::has_active_sequence(LogicalAddress ecu) const {
    return get_active_sequence(ecu) != nullptr;
}

// =============================================================================
// Event Handling
// =============================================================================

void FlashSequenceTracker::on_event(FlashEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void FlashSequenceTracker::clear_callbacks() {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.clear();
}

// =============================================================================
// State Management
// =============================================================================

void FlashSequenceTracker::abort_sequence(LogicalAddress ecu) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = active_sequences_.find(ecu);
    if (it == active_sequences_.end())
        return;

    auto& seq = it->second;
    if (!seq.is_active())
        return;

    seq.state = FlashSequenceState::Aborted;
    seq.end_time = std::chrono::steady_clock::now();
    seq.total_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(seq.end_time - seq.start_time);

    stats_.sequences_aborted++;

    emit_event(FlashEvent::SequenceAborted, seq);
    archive_sequence(ecu);
}

void FlashSequenceTracker::clear_history(LogicalAddress ecu) {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.erase(ecu);
}

void FlashSequenceTracker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    active_sequences_.clear();
    history_.clear();
    stats_ = Statistics{};
}

std::size_t FlashSequenceTracker::check_timeouts() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    std::size_t timed_out = 0;

    std::vector<LogicalAddress> to_timeout;

    for (auto& [ecu, seq] : active_sequences_) {
        if (!seq.is_active())
            continue;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - seq.start_time);

        if (elapsed > options_.sequence_timeout) {
            to_timeout.push_back(ecu);
        }
    }

    for (auto ecu : to_timeout) {
        auto& seq = active_sequences_[ecu];
        seq.state = FlashSequenceState::Failed;
        seq.error_message = "Sequence timed out";
        seq.end_time = now;

        stats_.sequences_failed++;
        timed_out++;

        emit_event(FlashEvent::SequenceFailed, seq);
        archive_sequence(ecu);
    }

    return timed_out;
}

// =============================================================================
// Statistics
// =============================================================================

FlashSequenceTracker::Statistics FlashSequenceTracker::statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void FlashSequenceTracker::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
}

// =============================================================================
// Private Helpers
// =============================================================================

FlashSequence& FlashSequenceTracker::get_or_create_sequence(LogicalAddress ecu,
                                                            LogicalAddress tester) {
    auto it = active_sequences_.find(ecu);
    if (it != active_sequences_.end() && it->second.is_active()) {
        return it->second;
    }

    // Archive any existing sequence
    if (it != active_sequences_.end()) {
        archive_sequence(ecu);
    }

    // Create new sequence
    FlashSequence seq;
    seq.ecu_address = ecu;
    seq.tester_address = tester;
    seq.state = FlashSequenceState::Idle;

    active_sequences_[ecu] = std::move(seq);
    return active_sequences_[ecu];
}

void FlashSequenceTracker::complete_sequence(FlashSequence& seq, bool success) {
    if (success) {
        seq.state = FlashSequenceState::Complete;
        stats_.sequences_completed++;
    } else {
        seq.state = FlashSequenceState::Failed;
        stats_.sequences_failed++;
    }
}

void FlashSequenceTracker::emit_event(FlashEvent event, const FlashSequence& seq) {
    for (const auto& callback : callbacks_) {
        callback(event, seq);
    }
}

void FlashSequenceTracker::archive_sequence(LogicalAddress ecu) {
    auto it = active_sequences_.find(ecu);
    if (it == active_sequences_.end())
        return;

    auto& hist = history_[ecu];
    hist.push_back(std::move(it->second));

    // Enforce history limit
    while (hist.size() > options_.max_history) {
        hist.erase(hist.begin());
    }

    active_sequences_.erase(it);
}

}  // namespace wadjet::protocols::diagnostic

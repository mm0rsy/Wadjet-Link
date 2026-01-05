#pragma once

/// @file flash_sequence.hpp
/// @brief Flash programming sequence tracker for UDS/DoIP
///
/// Tracks complete flash programming sequences including:
/// - Request Download / Request Upload
/// - Transfer Data blocks
/// - Request Transfer Exit
/// - Erase Memory routines
/// - Checksum verification routines
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_types.hpp"
#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_services.hpp"

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
// Flash Sequence Types
// =============================================================================

/// @brief Flash operation type
enum class FlashOperationType {
    Download,  ///< RequestDownload - programming ECU
    Upload,    ///< RequestUpload - reading from ECU
};

/// @brief Flash sequence state
enum class FlashSequenceState {
    Idle,          ///< No flash operation in progress
    EraseStarted,  ///< Erase memory routine started
    Erasing,       ///< Erase in progress
    EraseComplete, ///< Erase completed successfully
    EraseFailed,   ///< Erase failed
    Downloading,   ///< Data download/upload in progress
    Transferring,  ///< TransferData blocks being sent
    Verifying,     ///< Checksum verification in progress
    Complete,      ///< Transfer completed successfully
    Failed,        ///< Transfer failed
    Aborted,       ///< Transfer aborted by user/error
};

/// @brief Convert FlashSequenceState to string
[[nodiscard]] std::string_view flash_sequence_state_string(FlashSequenceState state);

/// @brief Flash block information
struct FlashBlock {
    std::uint8_t sequence_counter{0};        ///< Block sequence counter (1-255)
    std::size_t data_size{0};                ///< Size of data in this block
    std::chrono::steady_clock::time_point timestamp;  ///< When block was transferred
    bool acknowledged{false};                ///< Whether positive response received
    std::optional<uds::NRC> error_code;      ///< Error if transfer failed
};

/// @brief Memory region being flashed
struct FlashRegion {
    std::uint64_t start_address{0};   ///< Starting memory address
    std::uint64_t size{0};            ///< Region size in bytes
    std::uint8_t data_format{0};      ///< Data format identifier (compression/encryption)
    std::uint16_t max_block_size{0};  ///< Maximum block size from ECU
    
    /// @brief Check if address falls within this region
    [[nodiscard]] bool contains(std::uint64_t address) const {
        return address >= start_address && address < start_address + size;
    }
};

/// @brief Complete flash sequence tracking
struct FlashSequence {
    // Identification
    LogicalAddress ecu_address{0};     ///< Target ECU address
    LogicalAddress tester_address{0};  ///< Source tester address
    FlashOperationType operation_type{FlashOperationType::Download};
    
    // Memory region
    FlashRegion region;
    
    // State
    FlashSequenceState state{FlashSequenceState::Idle};
    
    // Timing
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::chrono::milliseconds total_duration{0};
    
    // Transfer progress
    std::vector<FlashBlock> blocks;
    std::size_t total_bytes_transferred{0};
    std::uint8_t expected_sequence_counter{1};  ///< Next expected block counter
    
    // Error tracking
    std::size_t retry_count{0};
    std::optional<uds::NRC> last_error;
    std::string error_message;
    
    // Erase information (if applicable)
    bool erase_performed{false};
    std::chrono::milliseconds erase_duration{0};
    
    // Verification information (if applicable)
    bool checksum_verified{false};
    std::optional<std::uint32_t> checksum_value;
    
    // =========================================================================
    // Helper Methods
    // =========================================================================
    
    /// @brief Get transfer progress (0.0 - 1.0)
    [[nodiscard]] double progress() const {
        if (region.size == 0) return 0.0;
        return static_cast<double>(total_bytes_transferred) / 
               static_cast<double>(region.size);
    }
    
    /// @brief Get progress as percentage (0 - 100)
    [[nodiscard]] int progress_percent() const {
        return static_cast<int>(progress() * 100.0);
    }
    
    /// @brief Calculate transfer rate in bytes per second
    [[nodiscard]] double transfer_rate_bps() const {
        if (total_duration.count() == 0) return 0.0;
        return (static_cast<double>(total_bytes_transferred) * 1000.0) /
               static_cast<double>(total_duration.count());
    }
    
    /// @brief Check if transfer is in progress
    [[nodiscard]] bool is_active() const {
        return state == FlashSequenceState::Downloading ||
               state == FlashSequenceState::Transferring ||
               state == FlashSequenceState::Erasing ||
               state == FlashSequenceState::Verifying;
    }
    
    /// @brief Check if transfer completed successfully
    [[nodiscard]] bool is_successful() const {
        return state == FlashSequenceState::Complete;
    }
    
    /// @brief Check if transfer failed
    [[nodiscard]] bool is_failed() const {
        return state == FlashSequenceState::Failed ||
               state == FlashSequenceState::Aborted ||
               state == FlashSequenceState::EraseFailed;
    }
    
    /// @brief Get number of successful blocks
    [[nodiscard]] std::size_t successful_blocks() const {
        std::size_t count = 0;
        for (const auto& block : blocks) {
            if (block.acknowledged && !block.error_code) count++;
        }
        return count;
    }
    
    /// @brief Get number of failed blocks
    [[nodiscard]] std::size_t failed_blocks() const {
        std::size_t count = 0;
        for (const auto& block : blocks) {
            if (block.error_code) count++;
        }
        return count;
    }
};

// =============================================================================
// Flash Events
// =============================================================================

/// @brief Events emitted during flash sequences
enum class FlashEvent {
    EraseStarted,      ///< Erase memory routine started
    EraseProgress,     ///< Erase progress (from routine status)
    EraseCompleted,    ///< Erase completed successfully
    EraseFailed,       ///< Erase failed
    DownloadStarted,   ///< RequestDownload accepted
    UploadStarted,     ///< RequestUpload accepted
    BlockTransferred,  ///< TransferData block sent
    BlockAcknowledged, ///< TransferData positive response
    BlockFailed,       ///< TransferData negative response
    TransferExitOk,    ///< RequestTransferExit positive response
    TransferExitFail,  ///< RequestTransferExit negative response
    VerificationStart, ///< Checksum verification started
    VerificationOk,    ///< Checksum verification passed
    VerificationFail,  ///< Checksum verification failed
    SequenceComplete,  ///< Full sequence completed
    SequenceFailed,    ///< Sequence failed
    SequenceAborted,   ///< Sequence aborted
};

/// @brief Convert FlashEvent to string
[[nodiscard]] std::string_view flash_event_string(FlashEvent event);

/// @brief Callback for flash events
using FlashEventCallback = std::function<void(
    FlashEvent event,
    const FlashSequence& sequence)>;

// =============================================================================
// Flash Sequence Tracker
// =============================================================================

/// @brief Tracks flash programming sequences for multiple ECUs
///
/// This class monitors UDS messages related to flash programming:
/// - Detects RequestDownload/RequestUpload initiation
/// - Tracks TransferData block progress
/// - Monitors RequestTransferExit completion
/// - Optionally tracks erase and verification routines
///
/// @example
/// ```cpp
/// FlashSequenceTracker tracker;
///
/// // Register callback for flash events
/// tracker.on_event([](FlashEvent event, const FlashSequence& seq) {
///     if (event == FlashEvent::BlockTransferred) {
///         std::cout << "Progress: " << seq.progress_percent() << "%\n";
///     }
/// });
///
/// // Process UDS messages
/// tracker.process_request_download(download_req, source, target);
/// for (const auto& block : blocks) {
///     tracker.process_transfer_data(block, source, target);
/// }
/// tracker.process_transfer_exit(exit_req, source, target);
///
/// // Get final sequence
/// auto seq = tracker.get_sequence(ecu_addr);
/// std::cout << "Transfer rate: " << seq->transfer_rate_bps() / 1024.0 << " KB/s\n";
/// ```
class FlashSequenceTracker {
public:
    /// @brief Configuration options
    struct Options {
        /// @brief Track erase memory routines (RoutineControl 0xFF00)
        bool track_erase_routines = true;
        
        /// @brief Track checksum verification routines
        bool track_verification = true;
        
        /// @brief Maximum sequences to keep in history
        std::size_t max_history = 100;
        
        /// @brief Timeout for incomplete sequences
        std::chrono::seconds sequence_timeout{300};  // 5 minutes
        
        /// @brief Get default options
        static Options defaults() { return Options{}; }
    };
    
    explicit FlashSequenceTracker(Options opts = Options::defaults());
    ~FlashSequenceTracker();
    
    // Non-copyable, non-movable (contains std::mutex)
    FlashSequenceTracker(const FlashSequenceTracker&) = delete;
    FlashSequenceTracker& operator=(const FlashSequenceTracker&) = delete;
    FlashSequenceTracker(FlashSequenceTracker&&) = delete;
    FlashSequenceTracker& operator=(FlashSequenceTracker&&) = delete;
    
    // =========================================================================
    // Request Processing
    // =========================================================================
    
    /// @brief Process RequestDownload request
    void process_request_download(const uds::RequestDownloadRequest& req,
                                  LogicalAddress source,
                                  LogicalAddress target,
                                  std::chrono::steady_clock::time_point timestamp =
                                      std::chrono::steady_clock::now());
    
    /// @brief Process RequestDownload response
    void process_request_download_response(const uds::RequestDownloadResponse& resp,
                                           LogicalAddress source,
                                           LogicalAddress target,
                                           std::chrono::steady_clock::time_point timestamp =
                                               std::chrono::steady_clock::now());
    
    /// @brief Process RequestUpload request
    void process_request_upload(const uds::RequestUploadRequest& req,
                                LogicalAddress source,
                                LogicalAddress target,
                                std::chrono::steady_clock::time_point timestamp =
                                    std::chrono::steady_clock::now());
    
    /// @brief Process RequestUpload response
    void process_request_upload_response(const uds::RequestUploadResponse& resp,
                                         LogicalAddress source,
                                         LogicalAddress target,
                                         std::chrono::steady_clock::time_point timestamp =
                                             std::chrono::steady_clock::now());
    
    /// @brief Process TransferData request
    void process_transfer_data(const uds::TransferDataRequest& req,
                               LogicalAddress source,
                               LogicalAddress target,
                               std::chrono::steady_clock::time_point timestamp =
                                   std::chrono::steady_clock::now());
    
    /// @brief Process TransferData response
    void process_transfer_data_response(const uds::TransferDataResponse& resp,
                                        LogicalAddress source,
                                        LogicalAddress target,
                                        std::chrono::steady_clock::time_point timestamp =
                                            std::chrono::steady_clock::now());
    
    /// @brief Process RequestTransferExit request
    void process_transfer_exit(const uds::RequestTransferExitRequest& req,
                               LogicalAddress source,
                               LogicalAddress target,
                               std::chrono::steady_clock::time_point timestamp =
                                   std::chrono::steady_clock::now());
    
    /// @brief Process RequestTransferExit response
    void process_transfer_exit_response(const uds::RequestTransferExitResponse& resp,
                                        LogicalAddress source,
                                        LogicalAddress target,
                                        std::chrono::steady_clock::time_point timestamp =
                                            std::chrono::steady_clock::now());
    
    /// @brief Process negative response related to flash
    void process_negative_response(const uds::NegativeResponseMessage& nrc,
                                   LogicalAddress source,
                                   LogicalAddress target,
                                   std::chrono::steady_clock::time_point timestamp =
                                       std::chrono::steady_clock::now());
    
    /// @brief Process RoutineControl for erase/verification
    void process_routine_control(const uds::RoutineControlRequest& req,
                                 LogicalAddress source,
                                 LogicalAddress target,
                                 std::chrono::steady_clock::time_point timestamp =
                                     std::chrono::steady_clock::now());
    
    /// @brief Process RoutineControl response
    void process_routine_control_response(const uds::RoutineControlResponse& resp,
                                          LogicalAddress source,
                                          LogicalAddress target,
                                          std::chrono::steady_clock::time_point timestamp =
                                              std::chrono::steady_clock::now());
    
    // =========================================================================
    // Sequence Access
    // =========================================================================
    
    /// @brief Get active sequence for ECU
    [[nodiscard]] const FlashSequence* get_active_sequence(LogicalAddress ecu) const;
    
    /// @brief Get sequence by ECU address (active or most recent)
    [[nodiscard]] const FlashSequence* get_sequence(LogicalAddress ecu) const;
    
    /// @brief Get all active sequences
    [[nodiscard]] std::vector<const FlashSequence*> get_active_sequences() const;
    
    /// @brief Get sequence history for ECU
    [[nodiscard]] std::vector<FlashSequence> get_history(LogicalAddress ecu,
                                                          std::size_t max_count = 10) const;
    
    /// @brief Check if ECU has active flash sequence
    [[nodiscard]] bool has_active_sequence(LogicalAddress ecu) const;
    
    // =========================================================================
    // Event Handling
    // =========================================================================
    
    /// @brief Register event callback
    void on_event(FlashEventCallback callback);
    
    /// @brief Clear all callbacks
    void clear_callbacks();
    
    // =========================================================================
    // State Management
    // =========================================================================
    
    /// @brief Abort sequence for ECU
    void abort_sequence(LogicalAddress ecu);
    
    /// @brief Clear history for ECU
    void clear_history(LogicalAddress ecu);
    
    /// @brief Reset all state
    void reset();
    
    /// @brief Check for timed-out sequences
    /// @return Number of sequences timed out
    std::size_t check_timeouts();
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct Statistics {
        std::uint64_t sequences_started{0};
        std::uint64_t sequences_completed{0};
        std::uint64_t sequences_failed{0};
        std::uint64_t sequences_aborted{0};
        std::uint64_t total_bytes_transferred{0};
        std::uint64_t total_blocks_transferred{0};
        std::uint64_t erase_operations{0};
        std::uint64_t verification_operations{0};
    };
    
    /// @brief Get statistics
    [[nodiscard]] Statistics statistics() const;
    
    /// @brief Reset statistics
    void reset_statistics();
    
private:
    FlashSequence& get_or_create_sequence(LogicalAddress ecu, LogicalAddress tester);
    void complete_sequence(FlashSequence& seq, bool success);
    void emit_event(FlashEvent event, const FlashSequence& seq);
    void archive_sequence(LogicalAddress ecu);
    
    Options options_;
    Statistics stats_;
    
    // Active sequences per ECU
    std::unordered_map<LogicalAddress, FlashSequence> active_sequences_;
    
    // Historical sequences per ECU
    std::unordered_map<LogicalAddress, std::vector<FlashSequence>> history_;
    
    std::vector<FlashEventCallback> callbacks_;
    mutable std::mutex mutex_;
};

}  // namespace wadjet::protocols::diagnostic

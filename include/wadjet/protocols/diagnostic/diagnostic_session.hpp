#pragma once

/// @file diagnostic_session.hpp
/// @brief Diagnostic session management combining UDS and DoIP
///
/// Provides a complete diagnostic session manager that:
/// - Extracts UDS from DoIP diagnostic messages
/// - Tracks session state (Default/Programming/Extended)
/// - Manages security access levels
/// - Correlates requests with responses
/// - Validates timing constraints
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_types.hpp"
#include "wadjet/protocols/diagnostic/request_correlator.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_session.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace wadjet::protocols::diagnostic {

// =============================================================================
// Diagnostic Session Events
// =============================================================================

/// @brief Events that occur during diagnostic sessions
enum class DiagnosticEvent {
    // Connection events
    RoutingActivated,    ///< DoIP routing activation successful
    RoutingDeactivated,  ///< Routing deactivated or lost
    ConnectionLost,      ///< TCP connection lost

    // Session events
    SessionStarted,  ///< Diagnostic session started
    SessionChanged,  ///< Session type changed
    SessionTimeout,  ///< S3 timeout occurred
    SessionEnded,    ///< Session explicitly ended

    // Security events
    SecurityUnlocked,  ///< Security level unlocked
    SecurityLocked,    ///< Security level locked (failed)
    SecurityLockout,   ///< Lockout due to failed attempts

    // Communication events
    RequestSent,       ///< Request sent to ECU
    ResponseReceived,  ///< Response received from ECU
    ResponsePending,   ///< ECU sent ResponsePending (0x78)
    ResponseTimeout,   ///< No response within timeout
    NegativeResponse,  ///< Negative response received

    // Diagnostic events
    DTCsRead,            ///< DTCs were read
    DTCsCleared,         ///< DTCs were cleared
    DataIdentifierRead,  ///< Data identifier read
    FlashStarted,        ///< Flash download started
    FlashProgress,       ///< Flash download progress
    FlashCompleted,      ///< Flash download completed
    FlashFailed,         ///< Flash download failed
};

/// @brief Convert event to string
[[nodiscard]] std::string_view diagnostic_event_string(DiagnosticEvent event);

// =============================================================================
// Diagnostic Session State
// =============================================================================

/// @brief Complete diagnostic session state
struct DiagnosticSessionState {
    // Connection state
    bool routing_active{false};
    LogicalAddress tester_address{0};
    LogicalAddress gateway_address{0};

    // Session state
    uds::SessionType session_type{uds::SessionType::DefaultSession};
    bool session_active{false};
    std::chrono::steady_clock::time_point session_start;
    std::chrono::steady_clock::time_point last_activity;

    // Security state
    std::uint8_t security_level{0};  ///< Current unlocked security level (0 = none)

    // Timing
    DiagnosticTiming timing{DiagnosticTiming::defaults()};

    // Statistics
    std::uint64_t requests_sent{0};
    std::uint64_t responses_received{0};
    std::uint64_t negative_responses{0};
    std::uint64_t timeouts{0};

    /// @brief Check if in programming session
    [[nodiscard]] bool is_programming() const {
        return session_type == uds::SessionType::ProgrammingSession;
    }

    /// @brief Check if in extended session
    [[nodiscard]] bool is_extended() const {
        return session_type == uds::SessionType::ExtendedDiagnosticSession;
    }

    /// @brief Check if security is unlocked
    [[nodiscard]] bool is_security_unlocked(std::uint8_t level = 1) const {
        return security_level >= level;
    }

    /// @brief Time since last activity
    [[nodiscard]] std::chrono::milliseconds time_since_activity() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_activity);
    }

    /// @brief Check if session might be timed out (S3)
    [[nodiscard]] bool is_potentially_timed_out() const {
        return session_active && time_since_activity() > timing.s3_server;
    }
};

// =============================================================================
// Event Callback
// =============================================================================

/// @brief Callback for diagnostic events
using DiagnosticEventCallback = std::function<void(
    DiagnosticEvent event, const DiagnosticSessionState& state, const RequestResponsePair* pair)>;

// =============================================================================
// Diagnostic Session Manager
// =============================================================================

/// @brief Manages diagnostic sessions over DoIP
///
/// This class provides comprehensive diagnostic session management:
/// - Processes DoIP packets and extracts UDS messages
/// - Tracks session state for multiple ECUs
/// - Correlates requests with responses
/// - Validates timing constraints
/// - Provides event callbacks for monitoring
///
/// @example
/// ```cpp
/// DiagnosticSessionManager manager;
///
/// // Register event callback
/// manager.on_event([](DiagnosticEvent event, const auto& state, auto* pair) {
///     std::cout << "Event: " << diagnostic_event_string(event) << "\n";
/// });
///
/// // Process DoIP packet
/// manager.process_doip_packet(doip_header, doip_payload, timestamp);
///
/// // Check session state
/// auto state = manager.get_session_state(ecu_address);
/// if (state && state->is_programming()) {
///     // ECU is in programming session
/// }
/// ```
class DiagnosticSessionManager {
public:
    /// @brief Configuration options
    struct Options {
        /// @brief Enable request/response correlation
        bool enable_correlation = true;

        /// @brief Enable automatic timeout detection
        bool enable_timeout_detection = true;

        /// @brief Maximum ECUs to track
        std::size_t max_ecus = 256;

        /// @brief Default timing parameters
        DiagnosticTiming default_timing{};

        /// @brief Correlator options
        RequestCorrelator::Options correlator_options{};

        /// @brief Get default options
        static Options defaults() {
            Options opts;
            opts.default_timing = DiagnosticTiming::defaults();
            opts.correlator_options = RequestCorrelator::Options::defaults();
            return opts;
        }
    };

    explicit DiagnosticSessionManager(Options opts = Options::defaults());

    // =========================================================================
    // Packet Processing
    // =========================================================================

    /// @brief Process a DoIP packet
    /// @param header Decoded DoIP header
    /// @param payload DoIP payload data
    /// @param timestamp Packet timestamp
    /// @return True if packet contained diagnostic data
    bool process_doip_packet(
        const doip::DoIPHeader& header, std::span<const std::byte> payload,
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /// @brief Process raw DoIP data (header + payload)
    /// @param data Raw DoIP packet data
    /// @param timestamp Packet timestamp
    /// @return True if packet was valid and processed
    bool process_doip_raw(
        std::span<const std::byte> data,
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /// @brief Process a UDS message directly (without DoIP)
    /// @param uds_data Raw UDS message data
    /// @param source_address Source logical address
    /// @param target_address Target logical address
    /// @param is_request True if this is a request message
    /// @param timestamp Message timestamp
    bool process_uds_message(
        std::span<const std::byte> uds_data, LogicalAddress source_address,
        LogicalAddress target_address, bool is_request,
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    // =========================================================================
    // Session State
    // =========================================================================

    /// @brief Get session state for an ECU
    /// @param ecu_address ECU logical address
    /// @return Session state or nullptr if not tracked
    [[nodiscard]] const DiagnosticSessionState* get_session_state(LogicalAddress ecu_address) const;

    /// @brief Get all tracked ECU addresses
    [[nodiscard]] std::vector<LogicalAddress> get_tracked_ecus() const;

    /// @brief Check if an ECU is being tracked
    [[nodiscard]] bool is_tracking(LogicalAddress ecu_address) const;

    // =========================================================================
    // Correlation
    // =========================================================================

    /// @brief Get the request correlator
    [[nodiscard]] RequestCorrelator& correlator() { return correlator_; }
    [[nodiscard]] const RequestCorrelator& correlator() const { return correlator_; }

    /// @brief Get completed request/response pairs for an ECU
    [[nodiscard]] std::vector<std::shared_ptr<RequestResponsePair>> get_completed_pairs(
        LogicalAddress ecu_address, std::size_t max_count = 100) const;

    // =========================================================================
    // ECU Information
    // =========================================================================

    /// @brief Get ECU information
    [[nodiscard]] const ECUInfo* get_ecu_info(LogicalAddress address) const;

    /// @brief Get all known ECU information
    [[nodiscard]] std::vector<ECUInfo> get_all_ecu_info() const;

    // =========================================================================
    // Timing
    // =========================================================================

    /// @brief Set timing parameters for an ECU
    void set_timing(LogicalAddress ecu_address, const DiagnosticTiming& timing);

    /// @brief Get timing parameters for an ECU
    [[nodiscard]] DiagnosticTiming get_timing(LogicalAddress ecu_address) const;

    /// @brief Check for timed-out requests
    /// @return Number of timed-out requests
    std::size_t check_timeouts();

    // =========================================================================
    // Event Handling
    // =========================================================================

    /// @brief Register event callback
    void on_event(DiagnosticEventCallback callback);

    /// @brief Clear all callbacks
    void clear_callbacks();

    // =========================================================================
    // State Management
    // =========================================================================

    /// @brief Reset tracking for a specific ECU
    void reset_ecu(LogicalAddress address);

    /// @brief Reset all tracking state
    void reset();

    // =========================================================================
    // Statistics
    // =========================================================================

    /// @brief Global statistics
    struct Statistics {
        std::uint64_t doip_packets_processed{0};
        std::uint64_t diagnostic_messages{0};
        std::uint64_t routing_activations{0};
        std::uint64_t vehicle_identifications{0};
        std::uint64_t uds_requests{0};
        std::uint64_t uds_responses{0};
        std::uint64_t decode_errors{0};
    };

    /// @brief Get statistics
    [[nodiscard]] Statistics statistics() const;

    /// @brief Reset statistics
    void reset_statistics();

private:
    // Internal processing methods
    void process_routing_activation(const doip::RoutingActivationRequest& req,
                                    const doip::RoutingActivationResponse* resp);
    void process_diagnostic_message(const doip::DiagnosticMessagePayload& msg,
                                    std::chrono::steady_clock::time_point timestamp);
    void process_vehicle_identification(const doip::VehicleIdentificationResponse& resp);

    void handle_uds_message(const uds::UdsHeader& header, const uds::UdsServiceMessage& message,
                            LogicalAddress source, LogicalAddress target, bool is_request,
                            std::chrono::steady_clock::time_point timestamp);

    void update_session_state(LogicalAddress ecu_address, const uds::UdsHeader& header,
                              const uds::UdsServiceMessage& message, bool is_request);

    void update_ecu_info(LogicalAddress address, const uds::UdsHeader& header,
                         const uds::UdsServiceMessage& message);

    DiagnosticSessionState& get_or_create_state(LogicalAddress address);

    void emit_event(DiagnosticEvent event, const DiagnosticSessionState& state,
                    const RequestResponsePair* pair = nullptr);

    Options options_;
    Statistics stats_;

    RequestCorrelator correlator_;
    uds::UdsDecoder uds_decoder_;
    doip::DoIPDecoder doip_decoder_;

    /// @brief Session state per ECU
    std::unordered_map<LogicalAddress, DiagnosticSessionState> sessions_;

    /// @brief ECU information
    std::unordered_map<LogicalAddress, ECUInfo> ecu_info_;

    std::vector<DiagnosticEventCallback> callbacks_;
    mutable std::mutex mutex_;
};

}  // namespace wadjet::protocols::diagnostic

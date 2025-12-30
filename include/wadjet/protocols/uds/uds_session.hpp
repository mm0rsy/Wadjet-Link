#pragma once

/// @file uds_session.hpp
/// @brief UDS session state tracking
///
/// This file provides session state management for UDS diagnostic sessions,
/// including session type tracking, security level management, and timing
/// parameter handling (P2/P2*).
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/uds/uds_types.hpp"
#include "wadjet/protocols/uds/uds_nrc.hpp"
#include "wadjet/protocols/uds/uds_services.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace wadjet::protocols::uds {

// =============================================================================
// Timing Parameters
// =============================================================================

/// @brief UDS timing parameters as defined in ISO 14229
struct TimingParameters {
    /// @brief P2 Server Max - Maximum time for initial response (default 50ms)
    std::chrono::milliseconds p2_server_max{50};

    /// @brief P2* Server Max - Maximum time after ResponsePending (default 5000ms)
    std::chrono::milliseconds p2_star_server_max{5000};

    /// @brief S3 Server - Session timeout after last TesterPresent (default 5000ms)
    std::chrono::milliseconds s3_server{5000};

    /// @brief Default timing parameters
    static constexpr TimingParameters default_values() {
        return TimingParameters{
            std::chrono::milliseconds{50},
            std::chrono::milliseconds{5000},
            std::chrono::milliseconds{5000}
        };
    }

    /// @brief Programming session timing parameters (typically longer)
    static constexpr TimingParameters programming_values() {
        return TimingParameters{
            std::chrono::milliseconds{50},
            std::chrono::milliseconds{5000},
            std::chrono::milliseconds{5000}
        };
    }
};

// =============================================================================
// Security State
// =============================================================================

/// @brief Security level state for a single security level
struct SecurityLevelState {
    std::uint8_t level{0};                      ///< Security level (1-33)
    bool unlocked{false};                       ///< Whether this level is unlocked
    std::uint8_t failed_attempts{0};            ///< Number of failed key attempts
    std::optional<std::chrono::steady_clock::time_point> lockout_until; ///< Lockout expiry
    std::vector<std::uint8_t> current_seed;     ///< Current seed (if seed requested)

    /// @brief Check if currently locked out
    [[nodiscard]] bool is_locked_out() const {
        if (!lockout_until) return false;
        return std::chrono::steady_clock::now() < *lockout_until;
    }

    /// @brief Reset state (e.g., on session change)
    void reset() {
        unlocked = false;
        current_seed.clear();
        // Note: failed_attempts and lockout_until typically persist
    }
};

/// @brief Security state manager for all security levels
class SecurityState {
public:
    /// @brief Maximum number of failed attempts before lockout
    static constexpr std::uint8_t MAX_FAILED_ATTEMPTS = 3;

    /// @brief Default lockout duration
    static constexpr std::chrono::seconds DEFAULT_LOCKOUT_DURATION{10};

    /// @brief Check if a security level is unlocked
    [[nodiscard]] bool is_unlocked(std::uint8_t level) const;

    /// @brief Get the highest unlocked security level
    [[nodiscard]] std::uint8_t highest_unlocked_level() const;

    /// @brief Check if any security level is unlocked
    [[nodiscard]] bool any_unlocked() const;

    /// @brief Record a seed request for a level
    void seed_requested(std::uint8_t level, std::span<const std::uint8_t> seed);

    /// @brief Record a successful key validation
    void key_accepted(std::uint8_t level);

    /// @brief Record a failed key validation
    void key_rejected(std::uint8_t level);

    /// @brief Check if a level is locked out due to failed attempts
    [[nodiscard]] bool is_locked_out(std::uint8_t level) const;

    /// @brief Get remaining lockout time for a level
    [[nodiscard]] std::chrono::milliseconds lockout_remaining(std::uint8_t level) const;

    /// @brief Reset all security state (e.g., on session change)
    void reset();

    /// @brief Get state for a specific level
    [[nodiscard]] const SecurityLevelState* get_level_state(std::uint8_t level) const;

private:
    std::unordered_map<std::uint8_t, SecurityLevelState> levels_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Session State
// =============================================================================

/// @brief UDS session state
enum class SessionState {
    Idle,           ///< No active session
    Active,         ///< Session is active
    TimedOut,       ///< Session timed out (S3 expired)
};

/// @brief Events that can occur during a session
enum class SessionEvent {
    SessionStarted,         ///< New session started
    SessionChanged,         ///< Session type changed
    SessionEnded,           ///< Session ended (timeout or reset)
    SecurityUnlocked,       ///< Security level unlocked
    SecurityLocked,         ///< Security level locked (failed attempts)
    ResponsePending,        ///< ECU sent ResponsePending NRC
    TesterPresentReceived,  ///< TesterPresent keepalive received
};

/// @brief Session event callback type
using SessionEventCallback = std::function<void(SessionEvent, const class UdsSession&)>;

/// @brief UDS session tracker
///
/// Tracks the state of a UDS diagnostic session including:
/// - Current session type
/// - Security level state
/// - Timing parameters
/// - Session timeout management
///
/// @threadsafe This class is thread-safe.
///
/// @example
/// ```cpp
/// UdsSession session;
///
/// // Process incoming UDS messages
/// session.process_message(uds_data, /*is_request=*/true);
///
/// // Check session state
/// if (session.session_type() == SessionType::ExtendedDiagnosticSession) {
///     if (session.is_security_unlocked(1)) {
///         // Can perform secured operations
///     }
/// }
///
/// // Register event callback
/// session.on_event([](SessionEvent event, const UdsSession& s) {
///     std::cout << "Session event occurred\n";
/// });
/// ```
class UdsSession {
public:
    /// @brief Construct a new session tracker
    /// @param ecu_address Optional ECU address for multi-ECU tracking
    explicit UdsSession(std::uint16_t ecu_address = 0);

    /// @brief Process an incoming UDS message
    /// @param data Raw UDS message data
    /// @param is_request True if this is a request message, false if response
    /// @return True if message was processed successfully
    bool process_message(std::span<const std::uint8_t> data, bool is_request);

    /// @brief Process a decoded UDS message
    /// @param header The decoded UDS header
    /// @param message The decoded service message
    /// @param is_request True if this is a request message
    /// @return True if message was processed successfully
    bool process_decoded(const UdsHeader& header, const UdsServiceMessage& message, bool is_request);

    // =========================================================================
    // Session State Queries
    // =========================================================================

    /// @brief Get current session type
    [[nodiscard]] SessionType session_type() const;

    /// @brief Get current session state
    [[nodiscard]] SessionState state() const;

    /// @brief Check if session is active
    [[nodiscard]] bool is_active() const;

    /// @brief Get ECU address
    [[nodiscard]] std::uint16_t ecu_address() const { return ecu_address_; }

    /// @brief Get time since last activity
    [[nodiscard]] std::chrono::milliseconds time_since_activity() const;

    /// @brief Check if session has timed out
    [[nodiscard]] bool is_timed_out() const;

    // =========================================================================
    // Timing Parameters
    // =========================================================================

    /// @brief Get current timing parameters
    [[nodiscard]] const TimingParameters& timing() const;

    /// @brief Update timing parameters (typically from ECU response)
    void update_timing(const TimingParameters& params);

    /// @brief Get time remaining before session timeout
    [[nodiscard]] std::chrono::milliseconds timeout_remaining() const;

    // =========================================================================
    // Security State
    // =========================================================================

    /// @brief Check if a security level is unlocked
    [[nodiscard]] bool is_security_unlocked(std::uint8_t level = 1) const;

    /// @brief Get highest unlocked security level
    [[nodiscard]] std::uint8_t highest_security_level() const;

    /// @brief Get security state
    [[nodiscard]] const SecurityState& security() const { return security_; }

    // =========================================================================
    // Event Handling
    // =========================================================================

    /// @brief Register an event callback
    void on_event(SessionEventCallback callback);

    /// @brief Clear all event callbacks
    void clear_callbacks();

    // =========================================================================
    // Session Control
    // =========================================================================

    /// @brief Reset session to default state
    void reset();

    /// @brief Manually refresh session timeout (simulate TesterPresent)
    void refresh_timeout();

    /// @brief Force session to a specific type (for testing/simulation)
    void force_session_type(SessionType type);

private:
    void handle_diagnostic_session_control_request(const DiagnosticSessionControlRequest& req);
    void handle_diagnostic_session_control_response(const DiagnosticSessionControlResponse& resp);
    void handle_security_access_request(const SecurityAccessRequest& req);
    void handle_security_access_response(const SecurityAccessResponse& resp);
    void handle_tester_present();
    void handle_ecu_reset(const ECUResetRequest& req);
    void handle_negative_response(const NegativeResponseMessage& nrc);

    void transition_to_session(SessionType new_type);
    void emit_event(SessionEvent event);
    void update_activity();

    std::uint16_t ecu_address_{0};
    SessionType session_type_{SessionType::DefaultSession};
    SessionState state_{SessionState::Idle};
    TimingParameters timing_{TimingParameters::default_values()};
    SecurityState security_;

    std::chrono::steady_clock::time_point last_activity_;
    std::chrono::steady_clock::time_point session_start_;

    std::vector<SessionEventCallback> callbacks_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Multi-ECU Session Manager
// =============================================================================

/// @brief Manages sessions for multiple ECUs
///
/// Tracks diagnostic sessions across multiple ECUs, useful for monitoring
/// complete vehicle diagnostics or gateway scenarios.
class UdsSessionManager {
public:
    /// @brief Get or create a session for an ECU
    /// @param ecu_address The ECU's diagnostic address
    /// @return Reference to the session tracker
    UdsSession& get_session(std::uint16_t ecu_address);

    /// @brief Check if a session exists for an ECU
    [[nodiscard]] bool has_session(std::uint16_t ecu_address) const;

    /// @brief Process a message for an ECU
    /// @param ecu_address Target ECU address
    /// @param data Raw UDS data
    /// @param is_request True if request message
    /// @return True if processed successfully
    bool process_message(std::uint16_t ecu_address, std::span<const std::uint8_t> data, bool is_request);

    /// @brief Get all active sessions
    [[nodiscard]] std::vector<std::uint16_t> active_sessions() const;

    /// @brief Reset all sessions
    void reset_all();

    /// @brief Remove a session
    void remove_session(std::uint16_t ecu_address);

    /// @brief Register a global event callback for all sessions
    void on_event(SessionEventCallback callback);

private:
    std::unordered_map<std::uint16_t, std::unique_ptr<UdsSession>> sessions_;
    std::vector<SessionEventCallback> global_callbacks_;
    mutable std::mutex mutex_;
};

}  // namespace wadjet::protocols::uds

/// @file uds_session.cpp
/// @brief UDS session state tracking implementation
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/uds/uds_session.hpp"
#include "wadjet/protocols/uds/uds.hpp"

#include <algorithm>

namespace wadjet::protocols::uds {

// =============================================================================
// SecurityState Implementation
// =============================================================================

bool SecurityState::is_unlocked(std::uint8_t level) const {
    std::lock_guard lock(mutex_);
    auto it = levels_.find(level);
    if (it == levels_.end()) return false;
    return it->second.unlocked && !it->second.is_locked_out();
}

std::uint8_t SecurityState::highest_unlocked_level() const {
    std::lock_guard lock(mutex_);
    std::uint8_t highest = 0;
    for (const auto& [level, state] : levels_) {
        if (state.unlocked && !state.is_locked_out() && level > highest) {
            highest = level;
        }
    }
    return highest;
}

bool SecurityState::any_unlocked() const {
    std::lock_guard lock(mutex_);
    for (const auto& [_, state] : levels_) {
        if (state.unlocked && !state.is_locked_out()) {
            return true;
        }
    }
    return false;
}

void SecurityState::seed_requested(std::uint8_t level, std::span<const std::uint8_t> seed) {
    std::lock_guard lock(mutex_);
    auto& state = levels_[level];
    state.level = level;
    state.current_seed.assign(seed.begin(), seed.end());
}

void SecurityState::key_accepted(std::uint8_t level) {
    std::lock_guard lock(mutex_);
    auto& state = levels_[level];
    state.level = level;
    state.unlocked = true;
    state.failed_attempts = 0;
    state.lockout_until = std::nullopt;
    state.current_seed.clear();
}

void SecurityState::key_rejected(std::uint8_t level) {
    std::lock_guard lock(mutex_);
    auto& state = levels_[level];
    state.level = level;
    state.failed_attempts++;
    state.current_seed.clear();

    if (state.failed_attempts >= MAX_FAILED_ATTEMPTS) {
        state.lockout_until = std::chrono::steady_clock::now() + DEFAULT_LOCKOUT_DURATION;
    }
}

bool SecurityState::is_locked_out(std::uint8_t level) const {
    std::lock_guard lock(mutex_);
    auto it = levels_.find(level);
    if (it == levels_.end()) return false;
    return it->second.is_locked_out();
}

std::chrono::milliseconds SecurityState::lockout_remaining(std::uint8_t level) const {
    std::lock_guard lock(mutex_);
    auto it = levels_.find(level);
    if (it == levels_.end() || !it->second.lockout_until) {
        return std::chrono::milliseconds{0};
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= *it->second.lockout_until) {
        return std::chrono::milliseconds{0};
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        *it->second.lockout_until - now);
}

void SecurityState::reset() {
    std::lock_guard lock(mutex_);
    for (auto& [_, state] : levels_) {
        state.reset();
    }
}

const SecurityLevelState* SecurityState::get_level_state(std::uint8_t level) const {
    std::lock_guard lock(mutex_);
    auto it = levels_.find(level);
    if (it == levels_.end()) return nullptr;
    return &it->second;
}

// =============================================================================
// UdsSession Implementation
// =============================================================================

UdsSession::UdsSession(std::uint16_t ecu_address)
    : ecu_address_(ecu_address)
    , last_activity_(std::chrono::steady_clock::now())
    , session_start_(std::chrono::steady_clock::now()) {
}

bool UdsSession::process_message(std::span<const std::uint8_t> data, bool is_request) {
    UdsDecoder decoder;
    auto result = decoder.decode(data);
    if (!result) {
        return false;
    }

    return process_decoded(result->header, result->message, is_request);
}

bool UdsSession::process_decoded(const UdsHeader& header, const UdsServiceMessage& message, bool is_request) {
    std::lock_guard lock(mutex_);
    update_activity();

    // Handle negative responses
    if (header.is_negative_response()) {
        if (auto* nrc = std::get_if<NegativeResponseMessage>(&message)) {
            handle_negative_response(*nrc);
        }
        return true;
    }

    // Dispatch based on service ID
    switch (header.service_id) {
        case ServiceID::DiagnosticSessionControl:
            if (is_request) {
                if (auto* req = std::get_if<DiagnosticSessionControlRequest>(&message)) {
                    handle_diagnostic_session_control_request(*req);
                }
            } else {
                if (auto* resp = std::get_if<DiagnosticSessionControlResponse>(&message)) {
                    handle_diagnostic_session_control_response(*resp);
                }
            }
            break;

        case ServiceID::SecurityAccess:
            if (is_request) {
                if (auto* req = std::get_if<SecurityAccessRequest>(&message)) {
                    handle_security_access_request(*req);
                }
            } else {
                if (auto* resp = std::get_if<SecurityAccessResponse>(&message)) {
                    handle_security_access_response(*resp);
                }
            }
            break;

        case ServiceID::TesterPresent:
            handle_tester_present();
            break;

        case ServiceID::ECUReset:
            if (is_request) {
                if (auto* req = std::get_if<ECUResetRequest>(&message)) {
                    handle_ecu_reset(*req);
                }
            }
            break;

        default:
            // Other services don't affect session state directly
            break;
    }

    return true;
}

SessionType UdsSession::session_type() const {
    std::lock_guard lock(mutex_);
    return session_type_;
}

SessionState UdsSession::state() const {
    std::lock_guard lock(mutex_);
    if (is_timed_out()) {
        return SessionState::TimedOut;
    }
    return state_;
}

bool UdsSession::is_active() const {
    std::lock_guard lock(mutex_);
    return state_ == SessionState::Active && !is_timed_out();
}

std::chrono::milliseconds UdsSession::time_since_activity() const {
    std::lock_guard lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_);
}

bool UdsSession::is_timed_out() const {
    // Note: mutex should already be held by caller
    if (state_ == SessionState::Idle) return false;

    auto elapsed = std::chrono::steady_clock::now() - last_activity_;
    return elapsed > timing_.s3_server;
}

const TimingParameters& UdsSession::timing() const {
    std::lock_guard lock(mutex_);
    return timing_;
}

void UdsSession::update_timing(const TimingParameters& params) {
    std::lock_guard lock(mutex_);
    timing_ = params;
}

std::chrono::milliseconds UdsSession::timeout_remaining() const {
    std::lock_guard lock(mutex_);
    if (state_ == SessionState::Idle) {
        return std::chrono::milliseconds::max();
    }

    auto elapsed = std::chrono::steady_clock::now() - last_activity_;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

    if (elapsed_ms >= timing_.s3_server) {
        return std::chrono::milliseconds{0};
    }

    return timing_.s3_server - elapsed_ms;
}

bool UdsSession::is_security_unlocked(std::uint8_t level) const {
    return security_.is_unlocked(level);
}

std::uint8_t UdsSession::highest_security_level() const {
    return security_.highest_unlocked_level();
}

void UdsSession::on_event(SessionEventCallback callback) {
    std::lock_guard lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void UdsSession::clear_callbacks() {
    std::lock_guard lock(mutex_);
    callbacks_.clear();
}

void UdsSession::reset() {
    std::lock_guard lock(mutex_);
    session_type_ = SessionType::DefaultSession;
    state_ = SessionState::Idle;
    timing_ = TimingParameters::default_values();
    security_.reset();
    last_activity_ = std::chrono::steady_clock::now();
    emit_event(SessionEvent::SessionEnded);
}

void UdsSession::refresh_timeout() {
    std::lock_guard lock(mutex_);
    update_activity();
    emit_event(SessionEvent::TesterPresentReceived);
}

void UdsSession::force_session_type(SessionType type) {
    std::lock_guard lock(mutex_);
    transition_to_session(type);
}

void UdsSession::handle_diagnostic_session_control_request(const DiagnosticSessionControlRequest& /* req */) {
    // Request doesn't change state until response confirms it
    // But we track the requested session for correlation
}

void UdsSession::handle_diagnostic_session_control_response(const DiagnosticSessionControlResponse& resp) {
    bool was_idle = (state_ == SessionState::Idle);
    SessionType old_type = session_type_;

    transition_to_session(resp.session_type);

    // Update timing parameters from response
    if (resp.p2_server_max_ms > 0) {
        timing_.p2_server_max = std::chrono::milliseconds(resp.p2_server_max_ms);
    }
    if (resp.p2_star_server_max_ms > 0) {
        timing_.p2_star_server_max = std::chrono::milliseconds(resp.p2_star_ms());
    }

    if (was_idle) {
        emit_event(SessionEvent::SessionStarted);
    } else if (old_type != resp.session_type) {
        emit_event(SessionEvent::SessionChanged);
    }
}

void UdsSession::handle_security_access_request(const SecurityAccessRequest& req) {
    // Track seed requests - SecurityAccessRequest has security_key for sendKey, not seed
    // Seed comes in response, so we don't track anything on request side
    (void)req;  // Mark as intentionally unused
}

void UdsSession::handle_security_access_response(const SecurityAccessResponse& resp) {
    // A positive response to SendKey means security is unlocked
    // Even access_type = requestSeed response, odd = sendKey response
    bool is_request_seed_response = (resp.access_type & 0x01) != 0;
    if (!is_request_seed_response) {
        // This is a key acceptance response
        std::uint8_t level = static_cast<std::uint8_t>((resp.access_type + 1) / 2);
        security_.key_accepted(level);
        emit_event(SessionEvent::SecurityUnlocked);
    }
}

void UdsSession::handle_tester_present() {
    update_activity();
    emit_event(SessionEvent::TesterPresentReceived);
}

void UdsSession::handle_ecu_reset(const ECUResetRequest& /* req */) {
    // ECU reset typically ends the session
    reset();
}

void UdsSession::handle_negative_response(const NegativeResponseMessage& nrc) {
    // Handle ResponsePending
    if (nrc.negative_response_code == NRC::RequestCorrectlyReceivedResponsePending) {
        emit_event(SessionEvent::ResponsePending);
        return;
    }

    // Handle security-related NRCs
    if (nrc.rejected_service_id == ServiceID::SecurityAccess) {
        switch (nrc.negative_response_code) {
            case NRC::InvalidKey:
            case NRC::ExceededNumberOfAttempts:
                // Key was rejected - the level would need to be tracked from request
                // For now, emit a generic security locked event
                emit_event(SessionEvent::SecurityLocked);
                break;
            default:
                break;
        }
    }
}

void UdsSession::transition_to_session(SessionType new_type) {
    SessionType old_type = session_type_;
    session_type_ = new_type;
    state_ = SessionState::Active;
    session_start_ = std::chrono::steady_clock::now();

    // Reset security on session change (except default -> extended is sometimes preserved)
    if (old_type != new_type) {
        // Per ISO 14229, security is typically reset on session change
        // Some OEMs preserve security when going to extended session
        security_.reset();
    }

    // Update timing for session type
    if (new_type == SessionType::ProgrammingSession) {
        timing_ = TimingParameters::programming_values();
    }
}

void UdsSession::emit_event(SessionEvent event) {
    for (const auto& callback : callbacks_) {
        callback(event, *this);
    }
}

void UdsSession::update_activity() {
    last_activity_ = std::chrono::steady_clock::now();
}

// =============================================================================
// UdsSessionManager Implementation
// =============================================================================

UdsSession& UdsSessionManager::get_session(std::uint16_t ecu_address) {
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(ecu_address);
    if (it == sessions_.end()) {
        auto session = std::make_unique<UdsSession>(ecu_address);
        // Register global callbacks
        for (const auto& callback : global_callbacks_) {
            session->on_event(callback);
        }
        auto [new_it, _] = sessions_.emplace(ecu_address, std::move(session));
        return *new_it->second;
    }
    return *it->second;
}

bool UdsSessionManager::has_session(std::uint16_t ecu_address) const {
    std::lock_guard lock(mutex_);
    return sessions_.find(ecu_address) != sessions_.end();
}

bool UdsSessionManager::process_message(std::uint16_t ecu_address,
                                        std::span<const std::uint8_t> data,
                                        bool is_request) {
    return get_session(ecu_address).process_message(data, is_request);
}

std::vector<std::uint16_t> UdsSessionManager::active_sessions() const {
    std::lock_guard lock(mutex_);
    std::vector<std::uint16_t> result;
    for (const auto& [addr, session] : sessions_) {
        if (session->is_active()) {
            result.push_back(addr);
        }
    }
    return result;
}

void UdsSessionManager::reset_all() {
    std::lock_guard lock(mutex_);
    for (auto& [_, session] : sessions_) {
        session->reset();
    }
}

void UdsSessionManager::remove_session(std::uint16_t ecu_address) {
    std::lock_guard lock(mutex_);
    sessions_.erase(ecu_address);
}

void UdsSessionManager::on_event(SessionEventCallback callback) {
    std::lock_guard lock(mutex_);
    global_callbacks_.push_back(callback);
    // Register with existing sessions
    for (auto& [_, session] : sessions_) {
        session->on_event(callback);
    }
}

}  // namespace wadjet::protocols::uds

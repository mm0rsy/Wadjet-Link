/// @file diagnostic_session.cpp
/// @brief Diagnostic session manager implementation

#include "wadjet/protocols/diagnostic/diagnostic_session.hpp"

namespace wadjet::protocols::diagnostic {

// =============================================================================
// Event String Conversion
// =============================================================================

std::string_view diagnostic_event_string(DiagnosticEvent event) {
    switch (event) {
        case DiagnosticEvent::RoutingActivated:
            return "RoutingActivated";
        case DiagnosticEvent::RoutingDeactivated:
            return "RoutingDeactivated";
        case DiagnosticEvent::ConnectionLost:
            return "ConnectionLost";
        case DiagnosticEvent::SessionStarted:
            return "SessionStarted";
        case DiagnosticEvent::SessionChanged:
            return "SessionChanged";
        case DiagnosticEvent::SessionTimeout:
            return "SessionTimeout";
        case DiagnosticEvent::SessionEnded:
            return "SessionEnded";
        case DiagnosticEvent::SecurityUnlocked:
            return "SecurityUnlocked";
        case DiagnosticEvent::SecurityLocked:
            return "SecurityLocked";
        case DiagnosticEvent::SecurityLockout:
            return "SecurityLockout";
        case DiagnosticEvent::RequestSent:
            return "RequestSent";
        case DiagnosticEvent::ResponseReceived:
            return "ResponseReceived";
        case DiagnosticEvent::ResponsePending:
            return "ResponsePending";
        case DiagnosticEvent::ResponseTimeout:
            return "ResponseTimeout";
        case DiagnosticEvent::NegativeResponse:
            return "NegativeResponse";
        case DiagnosticEvent::DTCsRead:
            return "DTCsRead";
        case DiagnosticEvent::DTCsCleared:
            return "DTCsCleared";
        case DiagnosticEvent::DataIdentifierRead:
            return "DataIdentifierRead";
        case DiagnosticEvent::FlashStarted:
            return "FlashStarted";
        case DiagnosticEvent::FlashProgress:
            return "FlashProgress";
        case DiagnosticEvent::FlashCompleted:
            return "FlashCompleted";
        case DiagnosticEvent::FlashFailed:
            return "FlashFailed";
    }
    return "Unknown";
}

// =============================================================================
// DiagnosticSessionManager Implementation
// =============================================================================

DiagnosticSessionManager::DiagnosticSessionManager(Options opts)
    : options_(std::move(opts)), correlator_(options_.correlator_options) {
    // Register correlator event callback to forward events
    correlator_.on_event([this](CorrelationEvent event, const RequestResponsePair* pair,
                                [[maybe_unused]] const PendingRequest* pending) {
        // Map correlation events to diagnostic events
        DiagnosticEvent diag_event;
        switch (event) {
            case CorrelationEvent::RequestTimeout:
                diag_event = DiagnosticEvent::ResponseTimeout;
                break;
            case CorrelationEvent::ResponsePending:
                diag_event = DiagnosticEvent::ResponsePending;
                break;
            case CorrelationEvent::NegativeResponse:
                diag_event = DiagnosticEvent::NegativeResponse;
                break;
            default:
                return;  // Don't emit for other events
        }

        // Find associated session state
        if (pair) {
            auto it = sessions_.find(pair->request.transport.target_address);
            if (it != sessions_.end()) {
                emit_event(diag_event, it->second, pair);
            }
        }
    });
}

bool DiagnosticSessionManager::process_doip_packet(
    const doip::DoIPHeader& header, std::span<const std::byte> payload,
    std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    stats_.doip_packets_processed++;

    // Handle different payload types
    switch (header.payload_type) {
        case doip::PayloadType::DiagnosticMessage: {
            auto diag_msg = doip::DoIPDecoder::parse_diagnostic_message(payload);
            if (diag_msg) {
                process_diagnostic_message(*diag_msg, timestamp);
                return true;
            }
            break;
        }

        case doip::PayloadType::RoutingActivationRequest: {
            auto req = doip::DoIPDecoder::parse_routing_activation_request(payload);
            if (req) {
                process_routing_activation(*req, nullptr);
                stats_.routing_activations++;
                return true;
            }
            break;
        }

        case doip::PayloadType::RoutingActivationResponse: {
            auto resp = doip::DoIPDecoder::parse_routing_activation_response(payload);
            if (resp) {
                process_routing_activation(doip::RoutingActivationRequest{}, &*resp);
                stats_.routing_activations++;
                return true;
            }
            break;
        }

        case doip::PayloadType::VehicleAnnouncementOrIdentificationResponse: {
            auto resp = doip::DoIPDecoder::parse_vehicle_identification_response(payload);
            if (resp) {
                process_vehicle_identification(*resp);
                stats_.vehicle_identifications++;
                return true;
            }
            break;
        }

        default:
            break;
    }

    return false;
}

bool DiagnosticSessionManager::process_doip_raw(std::span<const std::byte> data,
                                                std::chrono::steady_clock::time_point timestamp) {
    // Decode DoIP header
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;
    ctx.timestamp = {};
    auto result = doip_decoder_.decode_impl(ctx);

    if (!result) {
        stats_.decode_errors++;
        return false;
    }

    const auto& header = result.value();

    // Get payload
    if (data.size() < doip::HEADER_SIZE + header.payload_length) {
        stats_.decode_errors++;
        return false;
    }

    auto payload = data.subspan(doip::HEADER_SIZE, header.payload_length);

    return process_doip_packet(header, payload, timestamp);
}

bool DiagnosticSessionManager::process_uds_message(
    std::span<const std::byte> uds_data, LogicalAddress source_address,
    LogicalAddress target_address, bool is_request,
    std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Decode UDS
    auto result = uds_decoder_.decode(uds_data);
    if (!result) {
        stats_.decode_errors++;
        return false;
    }

    const auto& decode = result.value();

    handle_uds_message(decode.header, decode.message, source_address, target_address, is_request,
                       timestamp);

    return true;
}

void DiagnosticSessionManager::process_diagnostic_message(
    const doip::DiagnosticMessagePayload& msg, std::chrono::steady_clock::time_point timestamp) {
    stats_.diagnostic_messages++;

    // Decode UDS from diagnostic message payload
    auto result = uds_decoder_.decode(msg.user_data);
    if (!result) {
        stats_.decode_errors++;
        return;
    }

    const auto& decode = result.value();

    // Determine if request or response
    bool is_request = uds::UdsDecoder::is_request(msg.user_data);

    handle_uds_message(decode.header, decode.message, msg.source_address, msg.target_address,
                       is_request, timestamp);
}

void DiagnosticSessionManager::handle_uds_message(const uds::UdsHeader& header,
                                                  const uds::UdsServiceMessage& message,
                                                  LogicalAddress source, LogicalAddress target,
                                                  bool is_request,
                                                  std::chrono::steady_clock::time_point timestamp) {
    // Create transport info
    TransportInfo transport;
    transport.type = TransportType::DoIP;
    transport.source_address = source;
    transport.target_address = target;
    transport.timestamp = timestamp;

    // Update statistics
    if (is_request) {
        stats_.uds_requests++;
    } else {
        stats_.uds_responses++;
    }

    // Correlate request/response
    if (options_.enable_correlation) {
        if (is_request) {
            correlator_.record_request(header, message, transport);
        } else {
            correlator_.process_response(header, message, transport);
        }
    }

    // Update session state
    LogicalAddress ecu_address = is_request ? target : source;
    update_session_state(ecu_address, header, message, is_request);

    // Update ECU info if relevant
    update_ecu_info(ecu_address, header, message);

    // Emit appropriate events
    auto& state = get_or_create_state(ecu_address);
    if (is_request) {
        emit_event(DiagnosticEvent::RequestSent, state, nullptr);
    } else {
        emit_event(DiagnosticEvent::ResponseReceived, state, nullptr);

        if (header.is_negative_response()) {
            emit_event(DiagnosticEvent::NegativeResponse, state, nullptr);
        }
    }
}

void DiagnosticSessionManager::update_session_state(LogicalAddress ecu_address,
                                                    const uds::UdsHeader& header,
                                                    const uds::UdsServiceMessage& message,
                                                    bool is_request) {
    auto& state = get_or_create_state(ecu_address);
    state.last_activity = std::chrono::steady_clock::now();

    if (is_request) {
        state.requests_sent++;
    } else {
        state.responses_received++;
        if (header.is_negative_response()) {
            state.negative_responses++;
        }
    }

    // Handle specific services
    if (!is_request) {
        // Check for DiagnosticSessionControl response
        if (auto* resp = std::get_if<uds::DiagnosticSessionControlResponse>(&message)) {
            auto old_session = state.session_type;
            state.session_type = resp->session_type;
            state.session_active = true;
            state.session_start = std::chrono::steady_clock::now();

            // Update timing from response
            state.timing.p2_server_max = std::chrono::milliseconds{resp->p2_server_max_ms};
            state.timing.p2_star_server_max =
                std::chrono::milliseconds{resp->p2_star_server_max_ms * 10};

            if (old_session != state.session_type) {
                emit_event(DiagnosticEvent::SessionChanged, state, nullptr);
            } else if (!state.session_active) {
                emit_event(DiagnosticEvent::SessionStarted, state, nullptr);
            }
        }

        // Check for SecurityAccess response
        if (auto* resp = std::get_if<uds::SecurityAccessResponse>(&message)) {
            if (!header.is_negative_response()) {
                // Extract security level from sub-function
                // Odd access_type = seed request, even access_type = key response
                auto level = static_cast<std::uint8_t>((resp->access_type + 1) /
                                                       2);  // Convert to security level
                bool is_seed_response = (resp->access_type % 2 == 1);
                if (is_seed_response) {
                    // Seed received, waiting for key
                } else {
                    // Key accepted, security unlocked
                    state.security_level = std::max(state.security_level, level);
                    emit_event(DiagnosticEvent::SecurityUnlocked, state, nullptr);
                }
            }
        }

        // Check for ECUReset response
        if (std::holds_alternative<uds::ECUResetResponse>(message)) {
            if (!header.is_negative_response()) {
                // ECU will reset, session will end
                state.session_type = uds::SessionType::DefaultSession;
                state.session_active = false;
                state.security_level = 0;
                emit_event(DiagnosticEvent::SessionEnded, state, nullptr);
            }
        }

        // Check for ClearDiagnosticInformation response
        if (std::holds_alternative<uds::ClearDiagnosticInformationResponse>(message)) {
            if (!header.is_negative_response()) {
                emit_event(DiagnosticEvent::DTCsCleared, state, nullptr);
            }
        }

        // Check for ReadDTCInformation response
        if (std::holds_alternative<uds::ReadDTCInformationResponse>(message)) {
            emit_event(DiagnosticEvent::DTCsRead, state, nullptr);
        }

        // Check for ReadDataByIdentifier response
        if (std::holds_alternative<uds::ReadDataByIdentifierResponse>(message)) {
            emit_event(DiagnosticEvent::DataIdentifierRead, state, nullptr);
        }

        // Check for RequestDownload response
        if (std::holds_alternative<uds::RequestDownloadResponse>(message)) {
            if (!header.is_negative_response()) {
                emit_event(DiagnosticEvent::FlashStarted, state, nullptr);
            }
        }

        // Check for TransferData response
        if (std::holds_alternative<uds::TransferDataResponse>(message)) {
            if (!header.is_negative_response()) {
                emit_event(DiagnosticEvent::FlashProgress, state, nullptr);
            }
        }

        // Check for RequestTransferExit response
        if (std::holds_alternative<uds::RequestTransferExitResponse>(message)) {
            if (!header.is_negative_response()) {
                emit_event(DiagnosticEvent::FlashCompleted, state, nullptr);
            } else {
                emit_event(DiagnosticEvent::FlashFailed, state, nullptr);
            }
        }
    }
}

void DiagnosticSessionManager::update_ecu_info(
    LogicalAddress address, [[maybe_unused]] const uds::UdsHeader& header,
    [[maybe_unused]] const uds::UdsServiceMessage& message) {
    auto& info = ecu_info_[address];
    info.logical_address = address;

    // Extract identification from ReadDataByIdentifier responses
    // (Would need specific DID parsing - simplified for now)
}

void DiagnosticSessionManager::process_routing_activation(
    [[maybe_unused]] const doip::RoutingActivationRequest& req,
    const doip::RoutingActivationResponse* resp) {
    if (resp) {
        // Create/update session state for this tester
        auto& state = get_or_create_state(resp->entity_address);
        state.routing_active =
            (resp->response_code == doip::RoutingActivationResponseCode::SuccessfullyActivated);
        state.tester_address = resp->logical_address;
        state.gateway_address = resp->entity_address;

        if (state.routing_active) {
            emit_event(DiagnosticEvent::RoutingActivated, state, nullptr);
        }
    }
}

void DiagnosticSessionManager::process_vehicle_identification(
    const doip::VehicleIdentificationResponse& resp) {
    auto& info = ecu_info_[resp.logical_address];
    info.logical_address = resp.logical_address;

    // Copy VIN
    VIN vin;
    std::copy(resp.vin.begin(), resp.vin.end(), vin.data.begin());
    info.vin = vin;
}

DiagnosticSessionState& DiagnosticSessionManager::get_or_create_state(LogicalAddress address) {
    auto it = sessions_.find(address);
    if (it == sessions_.end()) {
        auto& state = sessions_[address];
        state.timing = options_.default_timing;
        state.last_activity = std::chrono::steady_clock::now();
        return state;
    }
    return it->second;
}

const DiagnosticSessionState* DiagnosticSessionManager::get_session_state(
    LogicalAddress ecu_address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(ecu_address);
    if (it == sessions_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<LogicalAddress> DiagnosticSessionManager::get_tracked_ecus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<LogicalAddress> result;
    result.reserve(sessions_.size());
    for (const auto& [addr, state] : sessions_) {
        result.push_back(addr);
    }
    return result;
}

bool DiagnosticSessionManager::is_tracking(LogicalAddress ecu_address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(ecu_address) != sessions_.end();
}

std::vector<std::shared_ptr<RequestResponsePair>> DiagnosticSessionManager::get_completed_pairs(
    LogicalAddress ecu_address, std::size_t max_count) const {
    auto all_pairs = correlator_.get_completed(max_count * 2);

    std::vector<std::shared_ptr<RequestResponsePair>> result;
    for (const auto& pair : all_pairs) {
        if (pair->request.transport.target_address == ecu_address) {
            result.push_back(pair);
            if (result.size() >= max_count) {
                break;
            }
        }
    }
    return result;
}

const ECUInfo* DiagnosticSessionManager::get_ecu_info(LogicalAddress address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = ecu_info_.find(address);
    if (it == ecu_info_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<ECUInfo> DiagnosticSessionManager::get_all_ecu_info() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ECUInfo> result;
    result.reserve(ecu_info_.size());
    for (const auto& [addr, info] : ecu_info_) {
        result.push_back(info);
    }
    return result;
}

void DiagnosticSessionManager::set_timing(LogicalAddress ecu_address,
                                          const DiagnosticTiming& timing) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& state = get_or_create_state(ecu_address);
    state.timing = timing;
}

DiagnosticTiming DiagnosticSessionManager::get_timing(LogicalAddress ecu_address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(ecu_address);
    if (it == sessions_.end()) {
        return options_.default_timing;
    }
    return it->second.timing;
}

std::size_t DiagnosticSessionManager::check_timeouts() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::size_t count = 0;

    if (options_.enable_timeout_detection) {
        count = correlator_.check_timeouts();

        // Also check session timeouts (S3)
        for (auto& [addr, state] : sessions_) {
            if (state.is_potentially_timed_out()) {
                state.session_active = false;
                state.session_type = uds::SessionType::DefaultSession;
                state.security_level = 0;
                state.timeouts++;
                emit_event(DiagnosticEvent::SessionTimeout, state, nullptr);
            }
        }
    }

    return count;
}

void DiagnosticSessionManager::on_event(DiagnosticEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void DiagnosticSessionManager::clear_callbacks() {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.clear();
}

void DiagnosticSessionManager::reset_ecu(LogicalAddress address) {
    std::lock_guard<std::mutex> lock(mutex_);

    sessions_.erase(address);
    ecu_info_.erase(address);
}

void DiagnosticSessionManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    sessions_.clear();
    ecu_info_.clear();
    correlator_.reset();
}

DiagnosticSessionManager::Statistics DiagnosticSessionManager::statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void DiagnosticSessionManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
}

void DiagnosticSessionManager::emit_event(DiagnosticEvent event,
                                          const DiagnosticSessionState& state,
                                          const RequestResponsePair* pair) {
    for (const auto& callback : callbacks_) {
        callback(event, state, pair);
    }
}

}  // namespace wadjet::protocols::diagnostic

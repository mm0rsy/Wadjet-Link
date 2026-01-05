#pragma once

/// @file diagnostic.hpp
/// @brief Main include for UDS over DoIP diagnostic integration
///
/// Provides complete diagnostic session management for automotive
/// Ethernet networks using UDS (ISO 14229) over DoIP (ISO 13400).
///
/// ## Overview
///
/// The diagnostic module integrates:
/// - **DoIP transport**: ISO 13400 Diagnostics over IP
/// - **UDS services**: ISO 14229 Unified Diagnostic Services
/// - **Session management**: Track session state, security, timing
/// - **Request correlation**: Match requests with responses
///
/// ## Example Usage
///
/// ```cpp
/// #include <wadjet/protocols/diagnostic.hpp>
///
/// using namespace wadjet::protocols::diagnostic;
///
/// // Create session manager
/// DiagnosticSessionManager manager;
///
/// // Register event callback
/// manager.on_event([](DiagnosticEvent event, const auto& state, auto* pair) {
///     std::cout << diagnostic_event_string(event) << "\n";
///     if (pair && pair->is_complete()) {
///         std::cout << "Response time: " << pair->response_time()->count() << "ms\n";
///     }
/// });
///
/// // Process captured DoIP packets
/// for (const auto& packet : captured_packets) {
///     manager.process_doip_raw(packet.data(), packet.timestamp());
/// }
///
/// // Check results
/// for (auto ecu : manager.get_tracked_ecus()) {
///     auto* state = manager.get_session_state(ecu);
///     std::cout << "ECU 0x" << std::hex << ecu << ": "
///               << "Session=" << static_cast<int>(state->session_type)
///               << " Security=" << static_cast<int>(state->security_level) << "\n";
/// }
/// ```
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_session.hpp"
#include "wadjet/protocols/diagnostic/diagnostic_types.hpp"
#include "wadjet/protocols/diagnostic/dtc_manager.hpp"
#include "wadjet/protocols/diagnostic/flash_sequence.hpp"
#include "wadjet/protocols/diagnostic/request_correlator.hpp"
#include "wadjet/protocols/diagnostic/uds_doip_decoder.hpp"

namespace wadjet::protocols::diagnostic {

// Re-export commonly used types for convenience
using wadjet::protocols::doip::DoIPHeader;
using wadjet::protocols::doip::PayloadType;
using wadjet::protocols::uds::NRC;
using wadjet::protocols::uds::ServiceID;
using wadjet::protocols::uds::SessionType;

}  // namespace wadjet::protocols::diagnostic

/// @file diagnostic_bindings.cpp
/// @brief Diagnostic session management bindings for Python
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_diagnostic(py::module_& m) {
    using namespace wadjet::protocols::diagnostic;
    using namespace wadjet::protocols;

    // =========================================================================
    // Diagnostic Types
    // =========================================================================

    py::class_<DiagnosticTiming>(m, "DiagnosticTiming",
        "UDS timing parameters for diagnostic sessions")
        .def(py::init<>())
        .def_readwrite("p2_server_max", &DiagnosticTiming::p2_server_max,
                       "P2 Server Max (initial response timeout)")
        .def_readwrite("p2_star_server_max", &DiagnosticTiming::p2_star_server_max,
                       "P2* Server Max (response pending timeout)")
        .def_readwrite("s3_server", &DiagnosticTiming::s3_server,
                       "S3 Server (session keep-alive timeout)")
        .def_static("defaults", &DiagnosticTiming::defaults,
                    "Get default timing parameters")
        .def("within_p2", &DiagnosticTiming::within_p2,
             py::arg("response_time"), "Check if response time is within P2")
        .def("within_p2_star", &DiagnosticTiming::within_p2_star,
             py::arg("response_time"), "Check if response time is within P2*")
        .def("__repr__", [](const DiagnosticTiming& t) {
            return "<DiagnosticTiming p2=" + 
                   std::to_string(t.p2_server_max.count()) + "ms p2*=" +
                   std::to_string(t.p2_star_server_max.count()) + "ms>";
        });

    py::class_<TransportInfo>(m, "TransportInfo",
        "Transport layer information for diagnostic messages")
        .def(py::init<>())
        .def_readwrite("source_address", &TransportInfo::source_address,
                       "Source logical address")
        .def_readwrite("target_address", &TransportInfo::target_address,
                       "Target logical address")
        .def_readwrite("timestamp", &TransportInfo::timestamp,
                       "Message timestamp")
        .def("__repr__", [](const TransportInfo& t) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "<TransportInfo 0x%04X -> 0x%04X>",
                          t.source_address, t.target_address);
            return std::string(buf);
        });

    // =========================================================================
    // Diagnostic Events
    // =========================================================================

    py::enum_<DiagnosticEvent>(m, "DiagnosticEvent",
        "Events emitted during diagnostic session processing")
        .value("RoutingActivated", DiagnosticEvent::RoutingActivated)
        .value("RoutingDeactivated", DiagnosticEvent::RoutingDeactivated)
        .value("ConnectionLost", DiagnosticEvent::ConnectionLost)
        .value("SessionStarted", DiagnosticEvent::SessionStarted)
        .value("SessionChanged", DiagnosticEvent::SessionChanged)
        .value("SessionTimeout", DiagnosticEvent::SessionTimeout)
        .value("SessionEnded", DiagnosticEvent::SessionEnded)
        .value("SecurityUnlocked", DiagnosticEvent::SecurityUnlocked)
        .value("SecurityLocked", DiagnosticEvent::SecurityLocked)
        .value("SecurityLockout", DiagnosticEvent::SecurityLockout)
        .value("RequestSent", DiagnosticEvent::RequestSent)
        .value("ResponseReceived", DiagnosticEvent::ResponseReceived)
        .value("ResponsePending", DiagnosticEvent::ResponsePending)
        .value("ResponseTimeout", DiagnosticEvent::ResponseTimeout)
        .value("NegativeResponse", DiagnosticEvent::NegativeResponse)
        .value("DTCsRead", DiagnosticEvent::DTCsRead)
        .value("DTCsCleared", DiagnosticEvent::DTCsCleared)
        .value("DataIdentifierRead", DiagnosticEvent::DataIdentifierRead)
        .value("FlashStarted", DiagnosticEvent::FlashStarted)
        .value("FlashProgress", DiagnosticEvent::FlashProgress)
        .value("FlashCompleted", DiagnosticEvent::FlashCompleted)
        .value("FlashFailed", DiagnosticEvent::FlashFailed)
        .export_values();

    // =========================================================================
    // Request/Response Correlation
    // =========================================================================

    py::class_<PendingRequest>(m, "PendingRequest",
        "A UDS request awaiting a response")
        .def_readonly("request", &PendingRequest::request,
                      "The pending request information")
        .def_readonly("pending_count", &PendingRequest::pending_count,
                      "Number of ResponsePending (0x78) received")
        .def("is_timed_out", &PendingRequest::is_timed_out,
             py::arg("timing"), "Check if request has timed out");

    py::class_<RequestResponsePair>(m, "RequestResponsePair",
        "Correlated UDS request and response")
        .def("is_complete", &RequestResponsePair::is_complete,
             "Check if response has been received")
        .def("is_positive", &RequestResponsePair::is_positive,
             "Check if response is positive")
        .def("is_negative", &RequestResponsePair::is_negative,
             "Check if response is negative")
        .def("get_nrc", &RequestResponsePair::get_nrc,
             "Get negative response code if applicable")
        .def("response_time", &RequestResponsePair::response_time,
             "Get response time in milliseconds")
        .def("within_p2", &RequestResponsePair::within_p2,
             py::arg("timing"), "Check if response arrived within P2 timeout")
        .def("within_p2_star", &RequestResponsePair::within_p2_star,
             py::arg("timing"), "Check if response arrived within P2* timeout")
        .def("__repr__", [](const RequestResponsePair& p) {
            std::string state = p.is_complete() ? 
                (p.is_positive() ? "positive" : "negative") : "pending";
            return "<RequestResponsePair " + state + ">";
        });

    py::class_<RequestCorrelator::Statistics>(m, "CorrelationStatistics",
        "Statistics from request/response correlation")
        .def_readonly("requests_recorded", &RequestCorrelator::Statistics::requests_recorded)
        .def_readonly("responses_matched", &RequestCorrelator::Statistics::responses_matched)
        .def_readonly("responses_unmatched", &RequestCorrelator::Statistics::responses_unmatched)
        .def_readonly("pending_timeouts", &RequestCorrelator::Statistics::pending_timeouts)
        .def_readonly("response_pending_count", &RequestCorrelator::Statistics::response_pending_count)
        .def("match_rate", &RequestCorrelator::Statistics::match_rate,
             "Get match rate (0.0 - 1.0)")
        .def("__repr__", [](const RequestCorrelator::Statistics& s) {
            return "<CorrelationStatistics recorded=" + 
                   std::to_string(s.requests_recorded) + " matched=" +
                   std::to_string(s.responses_matched) + ">";
        });

    py::class_<RequestCorrelator::Options>(m, "RequestCorrelatorOptions",
        "Configuration options for RequestCorrelator")
        .def(py::init<>())
        .def_readwrite("max_pending_per_address", 
                       &RequestCorrelator::Options::max_pending_per_address)
        .def_readwrite("max_total_pending",
                       &RequestCorrelator::Options::max_total_pending)
        .def_readwrite("auto_timeout_check",
                       &RequestCorrelator::Options::auto_timeout_check)
        .def_readwrite("timing", &RequestCorrelator::Options::timing)
        .def_static("defaults", &RequestCorrelator::Options::defaults);

    py::class_<RequestCorrelator>(m, "RequestCorrelator",
        "Correlates UDS requests with their responses")
        .def(py::init<RequestCorrelator::Options>(),
             py::arg("options") = RequestCorrelator::Options::defaults())
        .def("record_request", &RequestCorrelator::record_request,
             py::arg("header"), py::arg("message"), py::arg("transport"),
             "Record an outgoing UDS request")
        .def("process_response", &RequestCorrelator::process_response,
             py::arg("header"), py::arg("message"), py::arg("transport"),
             "Process an incoming UDS response")
        .def("pending_count", 
             py::overload_cast<LogicalAddress, LogicalAddress>(
                 &RequestCorrelator::pending_count, py::const_),
             py::arg("source"), py::arg("target"),
             "Get pending request count for address pair")
        .def("total_pending", &RequestCorrelator::total_pending,
             "Get total pending request count")
        .def("check_timeouts", &RequestCorrelator::check_timeouts,
             "Check for timed-out requests")
        .def("statistics", &RequestCorrelator::statistics,
             "Get correlation statistics")
        .def("timing", &RequestCorrelator::timing,
             py::return_value_policy::reference,
             "Get current timing parameters");

    // =========================================================================
    // Diagnostic Session State
    // =========================================================================

    py::class_<DiagnosticSessionState>(m, "DiagnosticSessionState",
        "Complete state of a diagnostic session with an ECU")
        .def_readonly("routing_active", &DiagnosticSessionState::routing_active)
        .def_readonly("tester_address", &DiagnosticSessionState::tester_address)
        .def_readonly("gateway_address", &DiagnosticSessionState::gateway_address)
        .def_readonly("session_type", &DiagnosticSessionState::session_type)
        .def_readonly("session_active", &DiagnosticSessionState::session_active)
        .def_readonly("security_level", &DiagnosticSessionState::security_level)
        .def_readonly("timing", &DiagnosticSessionState::timing)
        .def_readonly("session_start", &DiagnosticSessionState::session_start)
        .def_readonly("last_activity", &DiagnosticSessionState::last_activity)
        .def_readonly("requests_sent", &DiagnosticSessionState::requests_sent)
        .def_readonly("responses_received", &DiagnosticSessionState::responses_received)
        .def_readonly("negative_responses", &DiagnosticSessionState::negative_responses)
        .def_readonly("timeouts", &DiagnosticSessionState::timeouts)
        .def("is_programming", &DiagnosticSessionState::is_programming,
             "Check if in programming session")
        .def("is_extended", &DiagnosticSessionState::is_extended,
             "Check if in extended diagnostic session")
        .def("is_security_unlocked", &DiagnosticSessionState::is_security_unlocked,
             py::arg("level") = 1,
             "Check if security is unlocked at given level")
        .def("time_since_activity", &DiagnosticSessionState::time_since_activity,
             "Get time since last activity")
        .def("is_potentially_timed_out", &DiagnosticSessionState::is_potentially_timed_out,
             "Check if session might be timed out (S3)")
        .def("__repr__", [](const DiagnosticSessionState& s) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), 
                "<DiagnosticSessionState tester=0x%04X session=%s security=%d>",
                s.tester_address, 
                std::string(uds::session_type_string(s.session_type)).c_str(),
                s.security_level);
            return std::string(buf);
        });

    py::class_<DiagnosticSessionManager::Options>(m, "DiagnosticSessionManagerOptions",
        "Configuration options for DiagnosticSessionManager")
        .def(py::init<>())
        .def_readwrite("enable_correlation",
                       &DiagnosticSessionManager::Options::enable_correlation)
        .def_readwrite("enable_timeout_detection",
                       &DiagnosticSessionManager::Options::enable_timeout_detection)
        .def_readwrite("max_ecus",
                       &DiagnosticSessionManager::Options::max_ecus)
        .def_readwrite("default_timing",
                       &DiagnosticSessionManager::Options::default_timing)
        .def_readwrite("correlator_options",
                       &DiagnosticSessionManager::Options::correlator_options)
        .def_static("defaults", &DiagnosticSessionManager::Options::defaults);

    py::class_<DiagnosticSessionManager>(m, "DiagnosticSessionManager",
        R"doc(
        High-level diagnostic session manager for UDS over DoIP.
        
        This class manages diagnostic sessions with multiple ECUs,
        tracking session state, security levels, and correlating
        requests with responses.
        
        Example:
            manager = wadjet.DiagnosticSessionManager()
            
            # Register event callback
            def on_event(event, state, pair):
                if event == wadjet.DiagnosticEvent.SessionStarted:
                    print(f"Session started with ECU 0x{state.ecu_address:04X}")
            
            manager.on_event(on_event)
            
            # Process captured DoIP packets
            for packet in capture:
                manager.process_doip_raw(packet.data)
        )doc")
        .def(py::init<DiagnosticSessionManager::Options>(),
             py::arg("options") = DiagnosticSessionManager::Options::defaults())
        .def("process_doip_packet", &DiagnosticSessionManager::process_doip_packet,
             py::arg("header"), py::arg("payload"), 
             py::arg("timestamp") = std::chrono::steady_clock::now(),
             "Process a DoIP packet with decoded header")
        .def("process_doip_raw", 
             [](DiagnosticSessionManager& self, py::bytes data,
                std::chrono::steady_clock::time_point timestamp) {
                 std::string str = data;
                 std::span<const std::byte> span(
                     reinterpret_cast<const std::byte*>(str.data()), str.size());
                 return self.process_doip_raw(span, timestamp);
             },
             py::arg("data"),
             py::arg("timestamp") = std::chrono::steady_clock::now(),
             "Process raw DoIP packet data")
        .def("get_session_state",
             [](DiagnosticSessionManager& self, LogicalAddress address) 
                 -> std::optional<DiagnosticSessionState> {
                 auto* state = self.get_session_state(address);
                 if (state) return *state;
                 return std::nullopt;
             },
             py::arg("ecu_address"),
             "Get session state for an ECU")
        .def("get_tracked_ecus", &DiagnosticSessionManager::get_tracked_ecus,
             "Get all tracked ECU addresses")
        .def("is_tracking", &DiagnosticSessionManager::is_tracking,
             py::arg("ecu_address"),
             "Check if an ECU is being tracked")
        .def("correlator", 
             static_cast<RequestCorrelator& (DiagnosticSessionManager::*)()>(
                 &DiagnosticSessionManager::correlator),
             py::return_value_policy::reference,
             "Get the underlying request correlator")
        .def("on_event",
             [](DiagnosticSessionManager& self,
                std::function<void(DiagnosticEvent, 
                                   const DiagnosticSessionState&,
                                   const RequestResponsePair*)> callback) {
                 self.on_event(std::move(callback));
             },
             py::arg("callback"),
             "Register event callback");

    // =========================================================================
    // UDS over DoIP Decoder
    // =========================================================================

    py::enum_<MessageDirection>(m, "MessageDirection",
        "Direction of a diagnostic message")
        .value("Unknown", MessageDirection::Unknown)
        .value("Request", MessageDirection::Request)
        .value("Response", MessageDirection::Response)
        .export_values();

    py::class_<UdsOverDoipResult>(m, "UdsOverDoipResult",
        "Result of decoding a UDS-over-DoIP message")
        .def_readonly("doip_header", &UdsOverDoipResult::doip_header)
        .def_readonly("source_address", &UdsOverDoipResult::source_address)
        .def_readonly("target_address", &UdsOverDoipResult::target_address)
        .def_readonly("uds_header", &UdsOverDoipResult::uds_header)
        .def_readonly("direction", &UdsOverDoipResult::direction)
        .def("is_positive_response", &UdsOverDoipResult::is_positive_response,
             "Check if this is a positive UDS response")
        .def("is_negative_response", &UdsOverDoipResult::is_negative_response,
             "Check if this is a negative UDS response")
        .def("service_id", &UdsOverDoipResult::service_id,
             "Get the UDS service ID")
        .def("__repr__", [](const UdsOverDoipResult& r) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "<UdsOverDoipResult 0x%04X -> 0x%04X service=%s>",
                r.source_address, r.target_address,
                std::string(uds::service_id_string(r.uds_header.service_id)).c_str());
            return std::string(buf);
        });

    py::class_<UdsOverDoipError>(m, "UdsOverDoipError",
        "Error when decoding UDS over DoIP")
        .def_readonly("code", &UdsOverDoipError::code)
        .def_readonly("message", &UdsOverDoipError::message)
        .def("__repr__", [](const UdsOverDoipError& e) {
            return "<UdsOverDoipError " + e.message + ">";
        });

    py::enum_<UdsOverDoipError::Code>(m, "UdsOverDoipErrorCode")
        .value("DoipDecodeFailed", UdsOverDoipError::Code::DoipDecodeFailed)
        .value("NotDiagnosticMessage", UdsOverDoipError::Code::NotDiagnosticMessage)
        .value("PayloadTooShort", UdsOverDoipError::Code::PayloadTooShort)
        .value("UdsDecodeFailed", UdsOverDoipError::Code::UdsDecodeFailed)
        .value("InvalidVersion", UdsOverDoipError::Code::InvalidVersion)
        .export_values();

    py::class_<UdsOverDoipDecoder>(m, "UdsOverDoipDecoder",
        "Combined decoder for UDS messages inside DoIP packets")
        .def(py::init<>())
        .def("decode",
             [](const UdsOverDoipDecoder& self, py::bytes data) 
                 -> std::variant<UdsOverDoipResult, UdsOverDoipError> {
                 std::string str = data;
                 std::span<const std::byte> span(
                     reinterpret_cast<const std::byte*>(str.data()), str.size());
                 auto result = self.decode(span);
                 if (result.is_ok()) {
                     return result.value();
                 }
                 return result.error();
             },
             py::arg("data"),
             "Decode a DoIP packet containing a UDS message")
        .def_static("looks_like_diagnostic_message",
             [](py::bytes data) {
                 std::string str = data;
                 std::span<const std::byte> span(
                     reinterpret_cast<const std::byte*>(str.data()), str.size());
                 return UdsOverDoipDecoder::looks_like_diagnostic_message(span);
             },
             py::arg("data"),
             "Quick check if data looks like a DoIP diagnostic message");

    // Helper function for event name
    m.def("diagnostic_event_string", &diagnostic_event_string,
          py::arg("event"), "Get string representation of diagnostic event");
}

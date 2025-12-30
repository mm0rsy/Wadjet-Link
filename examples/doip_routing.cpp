/// @file doip_routing.cpp
/// @brief Example: DoIP Routing Activation validator
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to capture and validate DoIP routing activation
/// sequences according to ISO 13400-2. It monitors for routing activation requests
/// and responses, validating the protocol flow.
///
/// Usage:
///   ./doip_routing eth0               # Live capture on interface
///   ./doip_routing capture.pcap       # Analyze from PCAP file

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/doip.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace std::chrono_literals;

// =============================================================================
// Routing Activation State Machine
// =============================================================================

enum class SessionState {
    Idle,
    AwaitingResponse,
    AwaitingConfirmation,
    Active,
    Failed
};

std::string_view state_string(SessionState state) {
    switch (state) {
        case SessionState::Idle: return "IDLE";
        case SessionState::AwaitingResponse: return "AWAITING_RESPONSE";
        case SessionState::AwaitingConfirmation: return "AWAITING_CONFIRMATION";
        case SessionState::Active: return "ACTIVE";
        case SessionState::Failed: return "FAILED";
    }
    return "UNKNOWN";
}

struct RoutingSession {
    std::uint16_t source_address = 0;    ///< External tester address
    std::uint16_t entity_address = 0;    ///< DoIP gateway address
    SessionState state = SessionState::Idle;
    std::chrono::system_clock::time_point request_time;
    std::chrono::system_clock::time_point response_time;
    std::optional<doip::RoutingActivationResponseCode> response_code;
    std::uint32_t diagnostic_messages = 0;
};

// =============================================================================
// DoIP Session Tracker
// =============================================================================

class DoIPSessionTracker {
public:
    void process_header(const doip::DoIPHeader& header, std::span<const std::byte> payload) {
        packet_count_++;

        // Validate protocol version
        if (!header.is_version_valid()) {
            validation_errors_++;
            std::cout << "⚠️  Invalid DoIP version: 0x" << std::hex
                      << static_cast<int>(header.protocol_version) << " / 0x"
                      << static_cast<int>(header.inverse_protocol_version) << "\n";
            return;
        }

        switch (header.payload_type) {
            case doip::PayloadType::RoutingActivationRequest:
                handle_routing_request(payload);
                break;

            case doip::PayloadType::RoutingActivationResponse:
                handle_routing_response(payload);
                break;

            case doip::PayloadType::DiagnosticMessage:
                handle_diagnostic_message(payload);
                break;

            case doip::PayloadType::DiagnosticMessagePositiveAck:
            case doip::PayloadType::DiagnosticMessageNegativeAck:
                handle_diagnostic_ack(header.payload_type, payload);
                break;

            case doip::PayloadType::VehicleAnnouncementOrIdentificationResponse:
                handle_vehicle_announcement(payload);
                break;

            case doip::PayloadType::AliveCheckRequest:
                std::cout << "💓 Alive check request\n";
                alive_check_count_++;
                break;

            case doip::PayloadType::AliveCheckResponse:
                std::cout << "💓 Alive check response\n";
                break;

            default:
                std::cout << "📨 DoIP message: "
                          << doip::payload_type_string(header.payload_type) << "\n";
                break;
        }
    }

    void print_summary() const {
        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                  DoIP Session Summary                        ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

        std::cout << "📊 Statistics:\n";
        std::cout << "   Total DoIP packets:     " << packet_count_ << "\n";
        std::cout << "   Routing requests:       " << routing_requests_ << "\n";
        std::cout << "   Routing responses:      " << routing_responses_ << "\n";
        std::cout << "   Diagnostic messages:    " << diagnostic_count_ << "\n";
        std::cout << "   Alive checks:           " << alive_check_count_ << "\n";
        std::cout << "   Vehicle announcements:  " << vehicle_announcements_ << "\n";
        std::cout << "   Validation errors:      " << validation_errors_ << "\n\n";

        // Print sessions
        if (!sessions_.empty()) {
            std::cout << "🔗 Routing Sessions (" << sessions_.size() << "):\n";
            std::cout << "┌────────────────┬────────────────┬────────────────────┬───────────┐\n";
            std::cout << "│ Tester Addr    │ Gateway Addr   │ State              │ Diag Msgs │\n";
            std::cout << "├────────────────┼────────────────┼────────────────────┼───────────┤\n";

            for (const auto& [addr, session] : sessions_) {
                std::cout << "│ 0x" << std::hex << std::setw(12) << std::setfill('0')
                          << session.source_address << " │ 0x"
                          << std::setw(12) << std::setfill('0') << session.entity_address
                          << " │ " << std::setw(18) << std::setfill(' ')
                          << state_string(session.state) << " │ "
                          << std::dec << std::setw(9) << session.diagnostic_messages << " │\n";
            }
            std::cout << "└────────────────┴────────────────┴────────────────────┴───────────┘\n";
        }

        // Print validation results
        std::cout << "\n✅ Validation Results:\n";
        if (validation_errors_ == 0) {
            std::cout << "   All DoIP messages passed validation\n";
        } else {
            std::cout << "   " << validation_errors_ << " validation errors detected\n";
        }

        // Check for proper routing activation sequence
        for (const auto& [addr, session] : sessions_) {
            if (session.state == SessionState::Active) {
                std::cout << "   ✓ Session 0x" << std::hex << session.source_address
                          << " successfully activated\n";
            } else if (session.state == SessionState::Failed) {
                std::cout << "   ✗ Session 0x" << std::hex << session.source_address
                          << " activation failed";
                if (session.response_code) {
                    std::cout << " (code: " << std::dec
                              << static_cast<int>(*session.response_code) << ")";
                }
                std::cout << "\n";
            } else if (session.state == SessionState::AwaitingResponse) {
                std::cout << "   ⏳ Session 0x" << std::hex << session.source_address
                          << " awaiting response (timeout?)\n";
            }
        }
    }

private:
    void handle_routing_request(std::span<const std::byte> payload) {
        routing_requests_++;

        if (payload.size() < 7) {
            std::cout << "⚠️  Routing activation request too short\n";
            validation_errors_++;
            return;
        }

        // Parse routing activation request
        std::uint16_t source_addr =
            (static_cast<std::uint16_t>(payload[0]) << 8) |
            static_cast<std::uint16_t>(payload[1]);
        std::uint8_t activation_type = static_cast<std::uint8_t>(payload[2]);

        std::cout << "🔑 Routing Activation Request\n";
        std::cout << "   Source Address: 0x" << std::hex << std::setw(4)
                  << std::setfill('0') << source_addr << "\n";
        std::cout << "   Activation Type: 0x" << std::setw(2)
                  << static_cast<int>(activation_type) << "\n";

        // Create or update session
        auto& session = sessions_[source_addr];
        session.source_address = source_addr;
        session.state = SessionState::AwaitingResponse;
        session.request_time = std::chrono::system_clock::now();
    }

    void handle_routing_response(std::span<const std::byte> payload) {
        routing_responses_++;

        if (payload.size() < 9) {
            std::cout << "⚠️  Routing activation response too short\n";
            validation_errors_++;
            return;
        }

        // Parse routing activation response
        std::uint16_t logical_addr =
            (static_cast<std::uint16_t>(payload[0]) << 8) |
            static_cast<std::uint16_t>(payload[1]);
        std::uint16_t entity_addr =
            (static_cast<std::uint16_t>(payload[2]) << 8) |
            static_cast<std::uint16_t>(payload[3]);
        auto response_code = static_cast<doip::RoutingActivationResponseCode>(payload[4]);

        std::cout << "🔓 Routing Activation Response\n";
        std::cout << "   Tester Address:  0x" << std::hex << std::setw(4)
                  << std::setfill('0') << logical_addr << "\n";
        std::cout << "   Entity Address:  0x" << std::setw(4) << entity_addr << "\n";
        std::cout << "   Response Code:   0x" << std::setw(2)
                  << static_cast<int>(response_code);

        // Interpret response code
        switch (response_code) {
            case doip::RoutingActivationResponseCode::SuccessfullyActivated:
                std::cout << " (Successfully Activated)";
                break;
            case doip::RoutingActivationResponseCode::ActivationRequiresConfirmation:
                std::cout << " (Requires Confirmation)";
                break;
            case doip::RoutingActivationResponseCode::UnknownSourceAddress:
                std::cout << " (Unknown Source Address)";
                break;
            case doip::RoutingActivationResponseCode::NoSocketsAvailable:
                std::cout << " (No Sockets Available)";
                break;
            case doip::RoutingActivationResponseCode::AlreadyActive:
                std::cout << " (Already Active)";
                break;
            default:
                std::cout << " (Error)";
                break;
        }
        std::cout << "\n";

        // Update session state
        auto it = sessions_.find(logical_addr);
        if (it != sessions_.end()) {
            auto& session = it->second;
            session.entity_address = entity_addr;
            session.response_code = response_code;
            session.response_time = std::chrono::system_clock::now();

            if (response_code == doip::RoutingActivationResponseCode::SuccessfullyActivated) {
                session.state = SessionState::Active;
            } else if (response_code == doip::RoutingActivationResponseCode::ActivationRequiresConfirmation) {
                session.state = SessionState::AwaitingConfirmation;
            } else {
                session.state = SessionState::Failed;
            }
        }
    }

    void handle_diagnostic_message(std::span<const std::byte> payload) {
        diagnostic_count_++;

        if (payload.size() < 4) {
            std::cout << "⚠️  Diagnostic message too short\n";
            validation_errors_++;
            return;
        }

        std::uint16_t source_addr =
            (static_cast<std::uint16_t>(payload[0]) << 8) |
            static_cast<std::uint16_t>(payload[1]);
        std::uint16_t target_addr =
            (static_cast<std::uint16_t>(payload[2]) << 8) |
            static_cast<std::uint16_t>(payload[3]);

        std::cout << "💬 Diagnostic Message: 0x" << std::hex << std::setw(4)
                  << std::setfill('0') << source_addr << " → 0x"
                  << std::setw(4) << target_addr
                  << " (" << std::dec << (payload.size() - 4) << " bytes)\n";

        // Update session diagnostic count
        auto it = sessions_.find(source_addr);
        if (it != sessions_.end()) {
            it->second.diagnostic_messages++;
        }
    }

    void handle_diagnostic_ack(doip::PayloadType type, std::span<const std::byte> payload) {
        if (payload.size() < 5) {
            return;
        }

        std::uint16_t source_addr =
            (static_cast<std::uint16_t>(payload[0]) << 8) |
            static_cast<std::uint16_t>(payload[1]);
        std::uint16_t target_addr =
            (static_cast<std::uint16_t>(payload[2]) << 8) |
            static_cast<std::uint16_t>(payload[3]);
        std::uint8_t ack_code = static_cast<std::uint8_t>(payload[4]);

        const char* ack_type = (type == doip::PayloadType::DiagnosticMessagePositiveAck)
                                   ? "ACK+" : "ACK-";

        std::cout << "📬 Diagnostic " << ack_type << ": 0x" << std::hex << std::setw(4)
                  << std::setfill('0') << source_addr << " → 0x"
                  << std::setw(4) << target_addr
                  << " (code: 0x" << std::setw(2) << static_cast<int>(ack_code) << ")\n";
    }

    void handle_vehicle_announcement(std::span<const std::byte> payload) {
        vehicle_announcements_++;

        if (payload.size() < 32) {
            std::cout << "⚠️  Vehicle announcement too short\n";
            validation_errors_++;
            return;
        }

        // Extract VIN (first 17 bytes)
        std::string vin(reinterpret_cast<const char*>(payload.data()), 17);

        // Logical address at offset 17-18
        std::uint16_t logical_addr =
            (static_cast<std::uint16_t>(payload[17]) << 8) |
            static_cast<std::uint16_t>(payload[18]);

        std::cout << "🚗 Vehicle Announcement\n";
        std::cout << "   VIN: " << vin << "\n";
        std::cout << "   Logical Address: 0x" << std::hex << std::setw(4)
                  << std::setfill('0') << logical_addr << "\n";
    }

    std::map<std::uint16_t, RoutingSession> sessions_;
    std::size_t packet_count_ = 0;
    std::size_t routing_requests_ = 0;
    std::size_t routing_responses_ = 0;
    std::size_t diagnostic_count_ = 0;
    std::size_t alive_check_count_ = 0;
    std::size_t vehicle_announcements_ = 0;
    std::size_t validation_errors_ = 0;
};

// =============================================================================
// Packet processor
// =============================================================================

class DoIPPacketProcessor {
public:
    void process_packet(const net::PacketView& view) {
        // Decode the packet stack
        auto result = decode_packet(view.data());
        if (!result.success()) {
            return;
        }

        // Check for DoIP header
        if (!result.has_layer<doip::DoIPHeader>()) {
            return;
        }

        const auto* doip_header = result.get_layer<doip::DoIPHeader>();

        // Get the DoIP payload
        auto payload = result.payload_after<doip::DoIPHeader>();

        tracker_.process_header(*doip_header, payload);
    }

    void print_summary() const {
        tracker_.print_summary();
    }

private:
    DoIPSessionTracker tracker_;
};

// =============================================================================
// Main entry point
// =============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <interface|pcap_file>\n"
              << "\n"
              << "Monitor and validate DoIP routing activation sequences.\n"
              << "\n"
              << "Arguments:\n"
              << "  interface    Network interface for live capture (e.g., eth0)\n"
              << "  pcap_file    PCAP file for offline analysis\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " eth0           # Live capture\n"
              << "  " << prog << " capture.pcap   # Analyze PCAP file\n";
}

int main(int argc, char* argv[]) {
    std::cout << "𓆓 Wadjet-Link DoIP Routing Activation Validator\n\n";

    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string source = argv[1];
    DoIPPacketProcessor processor;

    // Check if source is a file or interface
    if (std::filesystem::exists(source)) {
        // PCAP file mode
        std::cout << "📁 Reading from PCAP file: " << source << "\n\n";

        auto reader_result = pcap::PcapReader::open(source);
        if (!reader_result) {
            std::cerr << "Error opening PCAP file: " << reader_result.error().message() << "\n";
            return 1;
        }

        auto& reader = reader_result.value();
        while (auto packet = reader.next_packet()) {
            processor.process_packet(packet->view());
        }
    } else {
        // Live capture mode
        std::cout << "🔴 Live capture on interface: " << source << "\n";
        std::cout << "   Filtering: tcp port 13400 (DoIP)\n";
        std::cout << "   Press Ctrl+C to stop\n\n";

        io::CaptureSessionOptions opts;
        opts.promiscuous = true;

        auto session_result = io::CaptureSession::create(source, opts);
        if (!session_result) {
            std::cerr << "Error creating capture session: "
                      << session_result.error().message() << "\n";
            return 1;
        }

        auto& session = session_result.value();

        // Set BPF filter for DoIP port
        auto filter_result = session.set_filter("tcp port 13400");
        if (!filter_result) {
            std::cerr << "Warning: Could not set filter: "
                      << filter_result.error().message() << "\n";
        }

        // Start capture
        auto start_result = session.start();
        if (!start_result) {
            std::cerr << "Error starting capture: "
                      << start_result.error().message() << "\n";
            return 1;
        }

        // Capture loop with 5 second timeout
        while (session.is_running()) {
            if (auto packet = session.next_packet(5s)) {
                processor.process_packet(packet->view());
            }
        }
    }

    // Print results
    processor.print_summary();

    return 0;
}

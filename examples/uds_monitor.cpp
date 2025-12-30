/// @file uds_monitor.cpp
/// @brief Example: Real-time UDS diagnostic traffic monitor
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to monitor UDS (Unified Diagnostic Services)
/// traffic over DoIP (Diagnostics over IP), tracking session states, security
/// levels, and decoded diagnostic messages in real-time.
///
/// Usage:
///   ./uds_monitor eth0                # Live capture on interface
///   ./uds_monitor capture.pcap        # Analyze from PCAP file
///   ./uds_monitor -e 0x1234 eth0      # Filter for specific ECU address

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds.hpp>

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
// UDS Session State Tracker
// =============================================================================

struct EcuState {
    std::uint16_t address = 0;
    uds::SessionType session = uds::SessionType::Default;
    uds::SecurityLevel security_level = uds::SecurityLevel::Locked;
    std::chrono::system_clock::time_point last_activity;
    std::uint32_t request_count = 0;
    std::uint32_t response_count = 0;
    std::uint32_t negative_response_count = 0;
    std::vector<std::string> recent_errors;
};

class UdsSessionTracker {
public:
    void process_request(std::uint16_t source, std::uint16_t target,
                         const uds::UdsDecoder::Result& decoded) {
        auto& ecu = get_or_create_ecu(target);
        ecu.last_activity = std::chrono::system_clock::now();
        ecu.request_count++;
        
        // Track session control requests
        if (decoded.service_id == uds::ServiceId::DiagnosticSessionControl) {
            pending_session_[target] = static_cast<uds::SessionType>(
                decoded.payload.empty() ? 0 : decoded.payload[0]);
        }
        
        // Track security access requests (seed request = odd subfunction)
        if (decoded.service_id == uds::ServiceId::SecurityAccess) {
            if (!decoded.payload.empty() && (decoded.payload[0] & 0x01)) {
                pending_security_[target] = decoded.payload[0];
            }
        }
    }

    void process_response(std::uint16_t source, std::uint16_t target,
                          const uds::UdsDecoder::Result& decoded) {
        auto& ecu = get_or_create_ecu(source);
        ecu.last_activity = std::chrono::system_clock::now();
        ecu.response_count++;

        // Handle positive session control response
        if (decoded.service_id == uds::ServiceId::DiagnosticSessionControlResponse) {
            if (auto it = pending_session_.find(source); it != pending_session_.end()) {
                ecu.session = it->second;
                pending_session_.erase(it);
            }
        }

        // Handle positive security access response (key accepted)
        if (decoded.service_id == uds::ServiceId::SecurityAccessResponse) {
            if (auto it = pending_security_.find(source); it != pending_security_.end()) {
                // Even subfunction = key send response
                if (!decoded.payload.empty() && !(decoded.payload[0] & 0x01)) {
                    auto level = (it->second + 1) / 2;  // Convert to security level
                    ecu.security_level = static_cast<uds::SecurityLevel>(level);
                }
                pending_security_.erase(it);
            }
        }
    }

    void process_negative_response(std::uint16_t source, 
                                   uds::ServiceId rejected_service,
                                   uds::NegativeResponseCode nrc) {
        auto& ecu = get_or_create_ecu(source);
        ecu.negative_response_count++;
        
        // Track recent errors
        std::string error = uds::service_id_name(rejected_service) + ": " +
                           uds::nrc_name(nrc);
        ecu.recent_errors.push_back(error);
        if (ecu.recent_errors.size() > 10) {
            ecu.recent_errors.erase(ecu.recent_errors.begin());
        }

        // Security lockout detection
        if (nrc == uds::NegativeResponseCode::ExceedNumberOfAttempts) {
            ecu.security_level = uds::SecurityLevel::Locked;
            std::cout << "⚠️  Security lockout on ECU 0x" << std::hex << source << std::dec << "\n";
        }
    }

    const EcuState* get_ecu(std::uint16_t address) const {
        auto it = ecus_.find(address);
        return it != ecus_.end() ? &it->second : nullptr;
    }

    const std::map<std::uint16_t, EcuState>& all_ecus() const { return ecus_; }

    void print_status() const {
        std::cout << "\n╔════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    UDS Session Status Summary                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════════╝\n\n";

        std::cout << "┌──────────┬─────────────────┬──────────────┬────────┬────────┬────────┐\n";
        std::cout << "│ ECU Addr │ Session         │ Security     │ Req    │ Resp   │ NRC    │\n";
        std::cout << "├──────────┼─────────────────┼──────────────┼────────┼────────┼────────┤\n";

        for (const auto& [addr, state] : ecus_) {
            std::cout << "│   0x" << std::hex << std::setw(4) << std::setfill('0') << addr
                      << " │ " << std::dec << std::setw(15) << std::left << std::setfill(' ')
                      << session_name(state.session)
                      << " │ " << std::setw(12) << security_name(state.security_level)
                      << " │ " << std::right << std::setw(6) << state.request_count
                      << " │ " << std::setw(6) << state.response_count
                      << " │ " << std::setw(6) << state.negative_response_count << " │\n";
        }

        std::cout << "└──────────┴─────────────────┴──────────────┴────────┴────────┴────────┘\n";
    }

private:
    EcuState& get_or_create_ecu(std::uint16_t address) {
        auto& ecu = ecus_[address];
        if (ecu.address == 0) {
            ecu.address = address;
        }
        return ecu;
    }

    static std::string session_name(uds::SessionType session) {
        switch (session) {
            case uds::SessionType::Default: return "Default";
            case uds::SessionType::Programming: return "Programming";
            case uds::SessionType::Extended: return "Extended";
            default: return "Unknown";
        }
    }

    static std::string security_name(uds::SecurityLevel level) {
        switch (level) {
            case uds::SecurityLevel::Locked: return "Locked";
            case uds::SecurityLevel::Level1: return "Level 1";
            case uds::SecurityLevel::Level2: return "Level 2";
            default: return "Unknown";
        }
    }

    std::map<std::uint16_t, EcuState> ecus_;
    std::map<std::uint16_t, uds::SessionType> pending_session_;
    std::map<std::uint16_t, std::uint8_t> pending_security_;
};

// =============================================================================
// UDS Message Formatter
// =============================================================================

class UdsMessageFormatter {
public:
    static void print_request(std::uint16_t source, std::uint16_t target,
                             const uds::UdsDecoder::Result& decoded,
                             std::chrono::system_clock::time_point timestamp) {
        print_timestamp(timestamp);
        std::cout << " [" << std::hex << std::setw(4) << std::setfill('0') << source
                  << " → " << std::setw(4) << target << std::dec << "] ";
        std::cout << "📤 " << uds::service_id_name(decoded.service_id);
        print_service_details(decoded, true);
        std::cout << "\n";
    }

    static void print_response(std::uint16_t source, std::uint16_t target,
                              const uds::UdsDecoder::Result& decoded,
                              std::chrono::system_clock::time_point timestamp) {
        print_timestamp(timestamp);
        std::cout << " [" << std::hex << std::setw(4) << std::setfill('0') << source
                  << " → " << std::setw(4) << target << std::dec << "] ";
        std::cout << "📥 " << uds::service_id_name(decoded.service_id);
        print_service_details(decoded, false);
        std::cout << "\n";
    }

    static void print_negative_response(std::uint16_t source,
                                        uds::ServiceId rejected_service,
                                        uds::NegativeResponseCode nrc,
                                        std::chrono::system_clock::time_point timestamp) {
        print_timestamp(timestamp);
        std::cout << " [" << std::hex << std::setw(4) << std::setfill('0') << source
                  << std::dec << "]      ";
        std::cout << "❌ NRC for " << uds::service_id_name(rejected_service)
                  << ": " << uds::nrc_name(nrc) << " (0x" << std::hex
                  << static_cast<int>(nrc) << std::dec << ")\n";
    }

private:
    static void print_timestamp(std::chrono::system_clock::time_point tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;
        std::cout << std::put_time(std::localtime(&time), "%H:%M:%S")
                  << "." << std::setw(3) << std::setfill('0') << ms.count();
    }

    static void print_service_details(const uds::UdsDecoder::Result& decoded,
                                      bool is_request) {
        switch (decoded.service_id) {
            case uds::ServiceId::DiagnosticSessionControl:
            case uds::ServiceId::DiagnosticSessionControlResponse:
                if (!decoded.payload.empty()) {
                    auto session = static_cast<uds::SessionType>(decoded.payload[0]);
                    std::cout << " → " << session_type_name(session);
                }
                break;

            case uds::ServiceId::SecurityAccess:
            case uds::ServiceId::SecurityAccessResponse:
                if (!decoded.payload.empty()) {
                    auto subfunc = decoded.payload[0];
                    if (subfunc & 0x01) {
                        std::cout << " → Request Seed (Level " << ((subfunc + 1) / 2) << ")";
                    } else {
                        std::cout << " → Send Key (Level " << (subfunc / 2) << ")";
                    }
                }
                break;

            case uds::ServiceId::ReadDataByIdentifier:
            case uds::ServiceId::ReadDataByIdentifierResponse:
                if (decoded.payload.size() >= 2) {
                    auto did = (decoded.payload[0] << 8) | decoded.payload[1];
                    std::cout << " → DID 0x" << std::hex << std::setw(4)
                              << std::setfill('0') << did << std::dec;
                    std::cout << " (" << did_name(did) << ")";
                }
                break;

            case uds::ServiceId::WriteDataByIdentifier:
            case uds::ServiceId::WriteDataByIdentifierResponse:
                if (decoded.payload.size() >= 2) {
                    auto did = (decoded.payload[0] << 8) | decoded.payload[1];
                    std::cout << " → DID 0x" << std::hex << std::setw(4)
                              << std::setfill('0') << did << std::dec;
                }
                break;

            case uds::ServiceId::ECUReset:
            case uds::ServiceId::ECUResetResponse:
                if (!decoded.payload.empty()) {
                    std::cout << " → " << reset_type_name(decoded.payload[0]);
                }
                break;

            case uds::ServiceId::RoutineControl:
            case uds::ServiceId::RoutineControlResponse:
                if (decoded.payload.size() >= 3) {
                    auto routine_id = (decoded.payload[1] << 8) | decoded.payload[2];
                    std::cout << " → " << routine_control_name(decoded.payload[0])
                              << " Routine 0x" << std::hex << std::setw(4)
                              << std::setfill('0') << routine_id << std::dec;
                }
                break;

            case uds::ServiceId::TesterPresent:
            case uds::ServiceId::TesterPresentResponse:
                std::cout << " (keep-alive)";
                break;

            default:
                // Show payload size for other services
                if (!decoded.payload.empty()) {
                    std::cout << " [" << decoded.payload.size() << " bytes]";
                }
                break;
        }
    }

    static std::string session_type_name(uds::SessionType session) {
        switch (session) {
            case uds::SessionType::Default: return "Default Session";
            case uds::SessionType::Programming: return "Programming Session";
            case uds::SessionType::Extended: return "Extended Session";
            default: return "Session 0x" + std::to_string(static_cast<int>(session));
        }
    }

    static std::string reset_type_name(std::uint8_t type) {
        switch (type) {
            case 0x01: return "Hard Reset";
            case 0x02: return "Key Off/On Reset";
            case 0x03: return "Soft Reset";
            default: return "Reset Type " + std::to_string(type);
        }
    }

    static std::string routine_control_name(std::uint8_t type) {
        switch (type) {
            case 0x01: return "Start";
            case 0x02: return "Stop";
            case 0x03: return "Results";
            default: return "Control " + std::to_string(type);
        }
    }

    static std::string did_name(std::uint16_t did) {
        switch (did) {
            case 0xF186: return "Active Session";
            case 0xF187: return "Spare Part Number";
            case 0xF188: return "SW Number";
            case 0xF189: return "SW Version";
            case 0xF18A: return "Supplier ID";
            case 0xF18B: return "Mfg Date";
            case 0xF18C: return "Serial Number";
            case 0xF190: return "VIN";
            case 0xF191: return "HW Number";
            case 0xF192: return "Supplier HW";
            case 0xF193: return "HW Version";
            case 0xF194: return "Supplier SW";
            case 0xF195: return "SW Version";
            case 0xF197: return "System Name";
            case 0xF199: return "Programming Date";
            default:
                if (did >= 0xF100 && did <= 0xF1FF) return "Identification";
                if (did >= 0x0100 && did <= 0xA5FF) return "OEM Specific";
                if (did >= 0xF400 && did <= 0xF5FF) return "OBD";
                return "Custom";
        }
    }
};

// =============================================================================
// Command Line Parsing
// =============================================================================

struct Options {
    std::string input;
    std::optional<std::uint16_t> filter_ecu;
    bool verbose = false;
    bool summary_only = false;
};

Options parse_args(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-e" && i + 1 < argc) {
            opts.filter_ecu = std::stoi(argv[++i], nullptr, 0);
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-s" || arg == "--summary") {
            opts.summary_only = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: uds_monitor [options] <interface|pcap_file>\n"
                      << "\nOptions:\n"
                      << "  -e <addr>    Filter by ECU address (hex)\n"
                      << "  -v           Verbose output\n"
                      << "  -s           Summary only (no live output)\n"
                      << "  -h           Show this help\n";
            std::exit(0);
        } else {
            opts.input = arg;
        }
    }
    
    if (opts.input.empty()) {
        std::cerr << "Error: No input specified. Use -h for help.\n";
        std::exit(1);
    }
    
    return opts;
}

// =============================================================================
// Main Processing Loop
// =============================================================================

int main(int argc, char* argv[]) {
    auto opts = parse_args(argc, argv);
    
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  𓆓 Wadjet-Link UDS Monitor                                    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    UdsSessionTracker tracker;
    uds::UdsDecoder decoder;
    doip::DoIPDecoder doip_decoder;
    std::uint32_t packet_count = 0;
    std::uint32_t uds_message_count = 0;

    // Packet handler for UDS over DoIP
    auto handle_packet = [&](const std::span<const std::uint8_t>& data,
                             std::chrono::system_clock::time_point timestamp) {
        packet_count++;

        // First decode DoIP
        auto doip_result = doip_decoder.decode(data);
        if (!doip_result) return;

        // Check for diagnostic message
        if (doip_result->payload_type != doip::PayloadType::DiagnosticMessage &&
            doip_result->payload_type != doip::PayloadType::DiagnosticMessagePositiveAck &&
            doip_result->payload_type != doip::PayloadType::DiagnosticMessageNegativeAck) {
            return;
        }

        // Extract UDS payload and addresses
        if (doip_result->payload.size() < 4) return;  // Need at least source/target

        std::uint16_t source = (doip_result->payload[0] << 8) | doip_result->payload[1];
        std::uint16_t target = (doip_result->payload[2] << 8) | doip_result->payload[3];
        
        // Apply ECU filter if specified
        if (opts.filter_ecu && *opts.filter_ecu != source && *opts.filter_ecu != target) {
            return;
        }

        // Extract UDS message (after DoIP addressing)
        std::span<const std::uint8_t> uds_data(
            doip_result->payload.data() + 4,
            doip_result->payload.size() - 4);

        auto uds_result = decoder.decode(uds_data);
        if (!uds_result) return;

        uds_message_count++;

        // Handle negative response
        if (uds_result->service_id == uds::ServiceId::NegativeResponse) {
            if (uds_result->payload.size() >= 2) {
                auto rejected = static_cast<uds::ServiceId>(uds_result->payload[0]);
                auto nrc = static_cast<uds::NegativeResponseCode>(uds_result->payload[1]);
                
                tracker.process_negative_response(source, rejected, nrc);
                
                if (!opts.summary_only) {
                    UdsMessageFormatter::print_negative_response(
                        source, rejected, nrc, timestamp);
                }
            }
            return;
        }

        // Determine if request or response (response SID = request SID + 0x40)
        bool is_response = (static_cast<std::uint8_t>(uds_result->service_id) & 0x40) != 0;

        if (is_response) {
            tracker.process_response(source, target, *uds_result);
            if (!opts.summary_only) {
                UdsMessageFormatter::print_response(source, target, *uds_result, timestamp);
            }
        } else {
            tracker.process_request(source, target, *uds_result);
            if (!opts.summary_only) {
                UdsMessageFormatter::print_request(source, target, *uds_result, timestamp);
            }
        }
    };

    // Process input (file or live capture)
    bool is_file = std::filesystem::exists(opts.input) && 
                   (opts.input.ends_with(".pcap") || opts.input.ends_with(".pcapng"));

    if (is_file) {
        std::cout << "📁 Reading from file: " << opts.input << "\n\n";
        
        pcap::PcapReader reader(opts.input);
        while (auto packet = reader.next_packet()) {
            handle_packet(packet->data, packet->timestamp);
        }
    } else {
        std::cout << "🔴 Live capture on interface: " << opts.input << "\n";
        std::cout << "   (Press Ctrl+C to stop)\n\n";
        
        io::CaptureSession session(opts.input);
        session.set_filter("tcp port 13400");  // DoIP port
        
        // Run until interrupted
        session.capture([&](const io::CaptureSession::Packet& pkt) {
            handle_packet(pkt.data, pkt.timestamp);
        });
    }

    // Print summary
    tracker.print_status();

    std::cout << "\n📊 Statistics:\n";
    std::cout << "   Total packets:   " << packet_count << "\n";
    std::cout << "   UDS messages:    " << uds_message_count << "\n";
    std::cout << "   ECUs discovered: " << tracker.all_ecus().size() << "\n";

    return 0;
}

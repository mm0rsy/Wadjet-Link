/// @file uds_monitor.cpp
/// @brief Example: Real-time UDS diagnostic traffic monitor
///
/// This example demonstrates how to monitor UDS (Unified Diagnostic Services)
/// traffic over DoIP (Diagnostics over IP), tracking session states, security
/// levels, and decoded diagnostic messages in real-time.
///
/// Usage:
///   ./uds_monitor eth0                # Live capture on interface
///   ./uds_monitor capture.pcap        # Analyze from PCAP file
///   ./uds_monitor -e 0x1234 eth0      # Filter for specific ECU address

#include <wadjet/protocols/decoder.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds/uds.hpp>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::uds;
using namespace std::chrono_literals;

// =============================================================================
// UDS Session State Tracker
// =============================================================================

struct EcuState {
    std::uint16_t address = 0;
    SessionType session = SessionType::DefaultSession;
    std::uint8_t security_level = 0;  // 0 = locked
    std::chrono::system_clock::time_point last_activity;
    std::uint32_t request_count = 0;
    std::uint32_t response_count = 0;
    std::uint32_t negative_response_count = 0;
    std::vector<std::string> recent_errors;
};

class UdsSessionTracker {
public:
    void process_request(std::uint16_t source, std::uint16_t target,
                         const UdsDecodeResult& decoded) {
        auto& ecu = get_or_create_ecu(target);
        ecu.last_activity = std::chrono::system_clock::now();
        ecu.request_count++;
        
        // Track session control requests
        if (decoded.header.service_id == ServiceID::DiagnosticSessionControl) {
            if (auto* req = decoded.as<DiagnosticSessionControlRequest>()) {
                pending_session_[target] = req->session_type;
            }
        }

        // Track security access requests (seed request = odd subfunction)
        if (decoded.header.service_id == ServiceID::SecurityAccess) {
            if (auto* req = decoded.as<SecurityAccessRequest>()) {
                if (req->is_request_seed()) {
                    pending_security_[target] = req->security_level();
                }
            }
        }
    }

    void process_response(std::uint16_t source, std::uint16_t target,
                          const UdsDecodeResult& decoded) {
        auto& ecu = get_or_create_ecu(source);
        ecu.last_activity = std::chrono::system_clock::now();
        ecu.response_count++;

        // Handle positive session control response
        if (decoded.header.service_id == ServiceID::DiagnosticSessionControl) {
            if (auto* resp = decoded.as<DiagnosticSessionControlResponse>()) {
                ecu.session = resp->session_type;
                pending_session_.erase(source);
            }
        }

        // Handle positive security access response (key accepted)
        if (decoded.header.service_id == ServiceID::SecurityAccess) {
            if (auto* resp = decoded.as<SecurityAccessResponse>()) {
                // Even access_type = key send response, security unlocked
                if ((resp->access_type & 0x01) == 0) {
                    // Calculate security level from access_type
                    ecu.security_level = static_cast<std::uint8_t>((resp->access_type + 1) / 2);
                }
                pending_security_.erase(source);
            }
        }
    }

    void process_negative_response(std::uint16_t source, ServiceID rejected_service, NRC nrc) {
        auto& ecu = get_or_create_ecu(source);
        ecu.negative_response_count++;
        
        // Track recent errors
        std::string error =
            std::string(service_id_string(rejected_service)) + ": " + std::string(nrc_string(nrc));
        ecu.recent_errors.push_back(error);
        if (ecu.recent_errors.size() > 10) {
            ecu.recent_errors.erase(ecu.recent_errors.begin());
        }

        // Security lockout detection
        if (nrc == NRC::ExceededNumberOfAttempts) {
            ecu.security_level = 0;  // Locked
            std::cout << "WARNING: Security lockout on ECU 0x" << std::hex << source << std::dec
                      << "\n";
        }
    }

    const EcuState* get_ecu(std::uint16_t address) const {
        auto it = ecus_.find(address);
        return it != ecus_.end() ? &it->second : nullptr;
    }

    const std::map<std::uint16_t, EcuState>& all_ecus() const { return ecus_; }

    void print_status() const {
        std::cout << "\n========== UDS Session Status Summary ==========\n\n";

        std::cout << "ECU Addr  | Session             | Security    | Req    | Resp   | NRC\n";
        std::cout << "----------|---------------------|-------------|--------|--------|------\n";

        for (const auto& [addr, state] : ecus_) {
            std::cout << "   0x" << std::hex << std::setw(4) << std::setfill('0') << addr << " | "
                      << std::dec << std::setw(19) << std::left << std::setfill(' ')
                      << session_name(state.session) << " | " << std::setw(11)
                      << security_name(state.security_level) << " | " << std::right << std::setw(6)
                      << state.request_count << " | " << std::setw(6) << state.response_count
                      << " | " << std::setw(6) << state.negative_response_count << "\n";
        }
        std::cout << "\n";
    }

private:
    EcuState& get_or_create_ecu(std::uint16_t address) {
        auto& ecu = ecus_[address];
        if (ecu.address == 0) {
            ecu.address = address;
        }
        return ecu;
    }

    static std::string session_name(SessionType session) {
        return std::string(session_type_string(session));
    }

    static std::string security_name(std::uint8_t level) {
        if (level == 0)
            return "Locked";
        return "Level " + std::to_string(level);
    }

    std::map<std::uint16_t, EcuState> ecus_;
    std::map<std::uint16_t, SessionType> pending_session_;
    std::map<std::uint16_t, std::uint8_t> pending_security_;
};

// =============================================================================
// UDS Message Formatter
// =============================================================================

class UdsMessageFormatter {
public:
    static void print_request(std::uint16_t source, std::uint16_t target,
                              const UdsDecodeResult& decoded) {
        std::cout << " [" << std::hex << std::setw(4) << std::setfill('0') << source << " -> "
                  << std::setw(4) << target << std::dec << "] ";
        std::cout << "REQ " << service_id_string(decoded.header.service_id);
        print_service_details(decoded, true);
        std::cout << "\n";
    }

    static void print_response(std::uint16_t source, std::uint16_t target,
                               const UdsDecodeResult& decoded) {
        std::cout << " [" << std::hex << std::setw(4) << std::setfill('0') << source << " -> "
                  << std::setw(4) << target << std::dec << "] ";
        std::cout << "RSP " << service_id_string(decoded.header.service_id);
        print_service_details(decoded, false);
        std::cout << "\n";
    }

    static void print_negative_response(std::uint16_t source, ServiceID rejected_service, NRC nrc) {
        std::cout << " [" << std::hex << std::setw(4) << std::setfill('0') << source
                  << std::dec << "]      ";
        std::cout << "NRC for " << service_id_string(rejected_service) << ": " << nrc_string(nrc)
                  << " (0x" << std::hex << static_cast<int>(nrc) << std::dec << ")\n";
    }

private:
    static void print_service_details(const UdsDecodeResult& decoded, bool is_request) {
        // Diagnostic Session Control
        if (auto* req = decoded.as<DiagnosticSessionControlRequest>()) {
            std::cout << " -> " << session_type_string(req->session_type);
            return;
        }
        if (auto* resp = decoded.as<DiagnosticSessionControlResponse>()) {
            std::cout << " -> " << session_type_string(resp->session_type);
            std::cout << " (P2=" << resp->p2_server_max_ms << "ms)";
            return;
        }

        // Security Access
        if (auto* req = decoded.as<SecurityAccessRequest>()) {
            if (req->is_request_seed()) {
                std::cout << " -> Request Seed (Level " << static_cast<int>(req->security_level())
                          << ")";
            } else {
                std::cout << " -> Send Key (Level " << static_cast<int>(req->security_level())
                          << ")";
            }
            return;
        }
        if (auto* resp = decoded.as<SecurityAccessResponse>()) {
            // Odd access_type = seed response
            if ((resp->access_type & 0x01) != 0) {
                std::cout << " -> Seed (" << resp->security_seed.size() << " bytes)";
            } else {
                std::cout << " -> Key Accepted";
            }
            return;
        }

        // Read Data By Identifier
        if (auto* req = decoded.as<ReadDataByIdentifierRequest>()) {
            std::cout << " -> DIDs: ";
            for (size_t i = 0; i < req->data_identifiers.size() && i < 3; ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << "0x" << std::hex << req->data_identifiers[i].value << std::dec;
            }
            if (req->data_identifiers.size() > 3) {
                std::cout << "... (" << req->data_identifiers.size() << " total)";
            }
            return;
        }

        // ECU Reset
        if (auto* req = decoded.as<ECUResetRequest>()) {
            std::cout << " -> " << reset_type_string(req->reset_type);
            return;
        }

        // Routine Control
        if (auto* req = decoded.as<RoutineControlRequest>()) {
            std::cout << " -> " << routine_control_type_string(req->routine_control_type)
                      << " Routine 0x" << std::hex << req->routine_identifier.value << std::dec;
            return;
        }

        // Tester Present
        if (decoded.header.service_id == ServiceID::TesterPresent) {
            std::cout << " (keep-alive)";
            return;
        }

        // Generic - show raw data size
        if (decoded.header.raw_data.size() > 1) {
            std::cout << " [" << (decoded.header.raw_data.size() - 1) << " bytes]";
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
            opts.filter_ecu = static_cast<std::uint16_t>(std::stoi(argv[++i], nullptr, 0));
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
// Demo Mode - Process Sample Data
// =============================================================================

void run_demo(UdsSessionTracker& tracker) {
    std::cout << "=== Wadjet-Link UDS Monitor Demo ===\n\n";
    std::cout << "Processing sample UDS messages...\n\n";

    UdsDecoder uds_decoder;

    // Sample 1: Diagnostic Session Control Request (Extended Session)
    std::vector<std::uint8_t> dsc_request = {0x10,
                                             0x03};  // DiagnosticSessionControl, ExtendedSession
    auto result1 = uds_decoder.decode(dsc_request);
    if (result1.is_ok()) {
        std::cout << "Sample 1: ";
        UdsMessageFormatter::print_request(0x0E00, 0x0001, *result1);
        tracker.process_request(0x0E00, 0x0001, *result1);
    }

    // Sample 2: Diagnostic Session Control Response
    std::vector<std::uint8_t> dsc_response = {0x50, 0x03, 0x00,
                                              0x19, 0x01, 0xF4};  // Response with timing
    auto result2 = uds_decoder.decode(dsc_response);
    if (result2.is_ok()) {
        std::cout << "Sample 2: ";
        UdsMessageFormatter::print_response(0x0001, 0x0E00, *result2);
        tracker.process_response(0x0001, 0x0E00, *result2);
    }

    // Sample 3: Security Access - Request Seed
    std::vector<std::uint8_t> sec_seed_req = {0x27, 0x01};  // SecurityAccess, RequestSeed level 1
    auto result3 = uds_decoder.decode(sec_seed_req);
    if (result3.is_ok()) {
        std::cout << "Sample 3: ";
        UdsMessageFormatter::print_request(0x0E00, 0x0001, *result3);
        tracker.process_request(0x0E00, 0x0001, *result3);
    }

    // Sample 4: Security Access - Seed Response
    std::vector<std::uint8_t> sec_seed_resp = {0x67, 0x01, 0xAB,
                                               0xCD, 0xEF, 0x12};  // Seed response
    auto result4 = uds_decoder.decode(sec_seed_resp);
    if (result4.is_ok()) {
        std::cout << "Sample 4: ";
        UdsMessageFormatter::print_response(0x0001, 0x0E00, *result4);
        tracker.process_response(0x0001, 0x0E00, *result4);
    }

    // Sample 5: Read Data By Identifier Request
    std::vector<std::uint8_t> rdbi_req = {0x22, 0xF1, 0x90};  // ReadDID VIN
    auto result5 = uds_decoder.decode(rdbi_req);
    if (result5.is_ok()) {
        std::cout << "Sample 5: ";
        UdsMessageFormatter::print_request(0x0E00, 0x0001, *result5);
        tracker.process_request(0x0E00, 0x0001, *result5);
    }

    // Sample 6: Negative Response
    std::vector<std::uint8_t> nrc = {0x7F, 0x2E,
                                     0x33};  // NRC for WriteDataByIdentifier, SecurityAccessDenied
    auto result6 = uds_decoder.decode(nrc);
    if (result6.is_ok()) {
        std::cout << "Sample 6: ";
        if (auto* nrc_msg = result6->as<NegativeResponseMessage>()) {
            UdsMessageFormatter::print_negative_response(0x0001, nrc_msg->rejected_service_id,
                                                         nrc_msg->negative_response_code);
            tracker.process_negative_response(0x0001, nrc_msg->rejected_service_id,
                                              nrc_msg->negative_response_code);
        }
    }

    std::cout << "\n";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    auto opts = parse_args(argc, argv);

    std::cout << "========================================\n";
    std::cout << "  Wadjet-Link UDS Monitor\n";
    std::cout << "========================================\n\n";

    UdsSessionTracker tracker;

    // For this example, run in demo mode if no file provided
    if (opts.input == "demo") {
        run_demo(tracker);
    } else {
        std::cout << "Note: Live capture and PCAP reading require additional setup.\n";
        std::cout << "Running demo mode instead...\n\n";
        run_demo(tracker);
    }

    // Print summary
    tracker.print_status();

    std::cout << "Statistics:\n";
    std::cout << "   ECUs discovered: " << tracker.all_ecus().size() << "\n";

    return 0;
}

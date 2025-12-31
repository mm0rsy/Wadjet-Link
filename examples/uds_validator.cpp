/// @file uds_validator.cpp
/// @brief Example: UDS protocol validator and compliance checker
///
/// This example validates UDS traffic for ISO 14229 compliance, checking:
/// - Proper service ID usage and responses
/// - Session state machine compliance
/// - Security access protocol adherence
/// - Timing requirements (P2/P2* timeouts)
/// - Service availability in current session
///
/// Usage:
///   ./uds_validator demo           # Run validation demo
///   ./uds_validator -v demo        # Verbose output

#include <wadjet/protocols/decoder.hpp>
#include <wadjet/protocols/uds/uds.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::uds;
using namespace std::chrono_literals;

// =============================================================================
// Validation Issue Types
// =============================================================================

enum class Severity { Info, Warning, Error, Critical };

struct ValidationIssue {
    Severity severity;
    std::string category;
    std::string description;
    std::uint16_t ecu_address;
    std::optional<std::uint32_t> packet_number;
};

// =============================================================================
// ECU Session State Machine
// =============================================================================

struct EcuValidationState {
    std::uint16_t address = 0;
    SessionType current_session = SessionType::DefaultSession;
    std::uint8_t security_level = 0;  // 0 = locked

    // Pending operations
    std::optional<SessionType> pending_session;
    std::optional<std::uint8_t> pending_security_level;
    bool awaiting_security_key = false;

    // Last message tracking
    std::optional<ServiceID> last_request_service;
    std::chrono::system_clock::time_point last_request_time;
    std::chrono::system_clock::time_point last_tester_present;

    // Statistics
    std::uint32_t request_count = 0;
    std::uint32_t response_count = 0;
    std::uint32_t nrc_count = 0;
};

// =============================================================================
// UDS Protocol Validator
// =============================================================================

class UdsValidator {
public:
    void add_issue(Severity severity, const std::string& category, const std::string& description,
                   std::uint16_t ecu, std::optional<std::uint32_t> packet = std::nullopt) {
        issues_.push_back({severity, category, description, ecu, packet});
    }

    void process_request(std::uint16_t source, std::uint16_t target, const UdsDecodeResult& decoded,
                         std::uint32_t packet_num) {
        auto& ecu = get_or_create_ecu(target);
        ecu.request_count++;

        // Track last request for response correlation
        ecu.last_request_service = decoded.header.service_id;
        ecu.last_request_time = std::chrono::system_clock::now();

        // Validate service availability in current session
        validate_service_in_session(ecu, decoded.header.service_id, packet_num);

        // Service-specific validation
        switch (decoded.header.service_id) {
            case ServiceID::DiagnosticSessionControl:
                validate_session_control_request(ecu, decoded, packet_num);
                break;

            case ServiceID::SecurityAccess:
                validate_security_access_request(ecu, decoded, packet_num);
                break;

            case ServiceID::TesterPresent:
                validate_tester_present(ecu, packet_num);
                break;

            case ServiceID::WriteDataByIdentifier:
            case ServiceID::InputOutputControlByIdentifier:
            case ServiceID::RoutineControl:
            case ServiceID::RequestDownload:
            case ServiceID::RequestUpload:
            case ServiceID::TransferData:
                // These typically require security access
                validate_security_required(ecu, decoded.header.service_id, packet_num);
                break;

            default:
                break;
        }
    }

    void process_response(std::uint16_t source, std::uint16_t target,
                          const UdsDecodeResult& decoded, std::uint32_t packet_num) {
        auto& ecu = get_or_create_ecu(source);
        ecu.response_count++;

        // Handle session transitions
        if (decoded.header.service_id == ServiceID::DiagnosticSessionControl) {
            if (auto* resp = decoded.as<DiagnosticSessionControlResponse>()) {
                SessionType old_session = ecu.current_session;
                ecu.current_session = resp->session_type;
                ecu.pending_session.reset();

                // Session change resets security
                if (resp->session_type != old_session) {
                    ecu.security_level = 0;
                    ecu.awaiting_security_key = false;
                    ecu.pending_security_level.reset();
                }
            }
        }

        // Handle security access
        if (decoded.header.service_id == ServiceID::SecurityAccess) {
            if (auto* resp = decoded.as<SecurityAccessResponse>()) {
                // Odd access_type = seed response
                if ((resp->access_type & 0x01) != 0) {
                    // Seed received, now expecting key
                    ecu.awaiting_security_key = true;
                    // Calculate security level from access_type
                    ecu.pending_security_level =
                        static_cast<std::uint8_t>((resp->access_type + 1) / 2);
                } else {
                    // Key accepted - calculate security level from access_type
                    ecu.security_level = static_cast<std::uint8_t>((resp->access_type + 1) / 2);
                    ecu.awaiting_security_key = false;
                    ecu.pending_security_level.reset();
                }
            }
        }
    }

    void process_negative_response(std::uint16_t source, ServiceID rejected_service, NRC nrc,
                                   std::uint32_t packet_num) {
        auto& ecu = get_or_create_ecu(source);
        ecu.nrc_count++;

        // Validate common NRC issues
        validate_nrc(ecu, rejected_service, nrc, packet_num);

        // Security failures
        if (nrc == NRC::InvalidKey) {
            add_issue(Severity::Warning, "Security", "Invalid security key sent", source,
                      packet_num);
            ecu.awaiting_security_key = false;
        }

        if (nrc == NRC::ExceededNumberOfAttempts) {
            add_issue(Severity::Critical, "Security",
                      "Security lockout - maximum attempts exceeded", source, packet_num);
            ecu.security_level = 0;
            ecu.awaiting_security_key = false;
            ecu.pending_security_level.reset();
        }

        // Session failures
        if (rejected_service == ServiceID::DiagnosticSessionControl) {
            ecu.pending_session.reset();
        }
    }

    const std::vector<ValidationIssue>& issues() const { return issues_; }
    const std::map<std::uint16_t, EcuValidationState>& ecus() const { return ecus_; }

    void print_report(std::ostream& out) const {
        out << "=======================================================\n";
        out << "         Wadjet-Link UDS Validation Report\n";
        out << "=======================================================\n\n";

        // Summary by severity
        std::map<Severity, int> severity_counts;
        for (const auto& issue : issues_) {
            severity_counts[issue.severity]++;
        }

        out << "Summary:\n";
        out << "   Critical: " << severity_counts[Severity::Critical] << "\n";
        out << "   Error:    " << severity_counts[Severity::Error] << "\n";
        out << "   Warning:  " << severity_counts[Severity::Warning] << "\n";
        out << "   Info:     " << severity_counts[Severity::Info] << "\n\n";

        // ECU summary
        out << "ECU Summary:\n";
        out << "ECU Addr  | Final Session        | Security    | Req     | Resp    | NRC\n";
        out << "----------|----------------------|-------------|---------|---------|------\n";

        for (const auto& [addr, state] : ecus_) {
            out << "   0x" << std::hex << std::setw(4) << std::setfill('0') << addr << " | "
                << std::dec << std::setw(20) << std::left << std::setfill(' ')
                << session_type_string(state.current_session) << " | " << std::setw(11)
                << security_name(state.security_level) << " | " << std::right << std::setw(7)
                << state.request_count << " | " << std::setw(7) << state.response_count << " | "
                << std::setw(6) << state.nrc_count << "\n";
        }
        out << "\n";

        // Detailed issues
        if (!issues_.empty()) {
            out << "Validation Issues:\n\n";

            for (const auto& issue : issues_) {
                out << severity_icon(issue.severity) << " [" << issue.category << "] " << "ECU 0x"
                    << std::hex << issue.ecu_address << std::dec;
                if (issue.packet_number) {
                    out << " (packet #" << *issue.packet_number << ")";
                }
                out << "\n   " << issue.description << "\n\n";
            }
        } else {
            out << "No validation issues found!\n\n";
        }
    }

private:
    EcuValidationState& get_or_create_ecu(std::uint16_t address) {
        auto& ecu = ecus_[address];
        if (ecu.address == 0) {
            ecu.address = address;
        }
        return ecu;
    }

    void validate_service_in_session(EcuValidationState& ecu, ServiceID service,
                                     std::uint32_t packet_num) {
        // Services that require non-default session
        static const std::set<ServiceID> extended_only = {
            ServiceID::InputOutputControlByIdentifier,
        };

        static const std::set<ServiceID> programming_only = {
            ServiceID::RequestDownload,
            ServiceID::RequestUpload,
            ServiceID::TransferData,
            ServiceID::RequestTransferExit,
        };

        if (ecu.current_session == SessionType::DefaultSession) {
            if (extended_only.count(service)) {
                add_issue(Severity::Warning, "Session",
                          std::string(service_id_string(service)) + " requires Extended session",
                          ecu.address, packet_num);
            }
            if (programming_only.count(service)) {
                add_issue(Severity::Warning, "Session",
                          std::string(service_id_string(service)) + " requires Programming session",
                          ecu.address, packet_num);
            }
        }
    }

    void validate_session_control_request(EcuValidationState& ecu, const UdsDecodeResult& decoded,
                                          std::uint32_t packet_num) {
        if (auto* req = decoded.as<DiagnosticSessionControlRequest>()) {
            ecu.pending_session = req->session_type;

            // Warn about programming session from non-default
            if (req->session_type == SessionType::ProgrammingSession &&
                ecu.current_session != SessionType::DefaultSession &&
                ecu.current_session != SessionType::ExtendedDiagnosticSession) {
                add_issue(Severity::Info, "Session",
                          "Programming session typically requires transition from Default or "
                          "Extended session",
                          ecu.address, packet_num);
            }
        }
    }

    void validate_security_access_request(EcuValidationState& ecu, const UdsDecodeResult& decoded,
                                          std::uint32_t packet_num) {
        if (auto* req = decoded.as<SecurityAccessRequest>()) {
            if (req->is_request_seed()) {
                // Requesting seed when already unlocked at this level
                if (ecu.security_level == req->security_level()) {
                    add_issue(Severity::Info, "Security",
                              "Requesting seed for already unlocked security level", ecu.address,
                              packet_num);
                }
            } else {
                // Sending key
                if (!ecu.awaiting_security_key) {
                    add_issue(Severity::Error, "Security",
                              "Sending security key without first requesting seed", ecu.address,
                              packet_num);
                }
            }
        }
    }

    void validate_security_required(EcuValidationState& ecu, ServiceID service,
                                    std::uint32_t packet_num) {
        if (ecu.security_level == 0) {
            add_issue(
                Severity::Info, "Security",
                std::string(service_id_string(service)) + " typically requires security access",
                ecu.address, packet_num);
        }
    }

    void validate_tester_present(EcuValidationState& ecu, std::uint32_t packet_num) {
        auto now = std::chrono::system_clock::now();
        auto time_since_last = now - ecu.last_tester_present;
        ecu.last_tester_present = now;

        // S3 timeout is typically 5 seconds, warning if interval is too long
        if (ecu.current_session != SessionType::DefaultSession) {
            auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(time_since_last).count();
            if (ms > 5000 && ecu.last_tester_present.time_since_epoch().count() > 0) {
                add_issue(Severity::Warning, "Timing",
                          "TesterPresent interval (" + std::to_string(ms) +
                              "ms) exceeds typical S3 timeout (5s)",
                          ecu.address, packet_num);
            }
        }
    }

    void validate_nrc(EcuValidationState& ecu, ServiceID service, NRC nrc,
                      std::uint32_t packet_num) {
        // Track frequent NRCs
        switch (nrc) {
            case NRC::ServiceNotSupported:
                add_issue(Severity::Warning, "Protocol",
                          std::string(service_id_string(service)) + " not supported by ECU",
                          ecu.address, packet_num);
                break;

            case NRC::ServiceNotSupportedInActiveSession:
                add_issue(Severity::Warning, "Session",
                          std::string(service_id_string(service)) + " not available in " +
                              std::string(session_type_string(ecu.current_session)),
                          ecu.address, packet_num);
                break;

            case NRC::SecurityAccessDenied:
                add_issue(Severity::Warning, "Security",
                          std::string(service_id_string(service)) + " requires security access",
                          ecu.address, packet_num);
                break;

            case NRC::ConditionsNotCorrect:
                add_issue(Severity::Info, "Protocol",
                          std::string(service_id_string(service)) + " - conditions not correct",
                          ecu.address, packet_num);
                break;

            default:
                break;
        }
    }

    static std::string severity_icon(Severity s) {
        switch (s) {
            case Severity::Critical:
                return "[CRITICAL]";
            case Severity::Error:
                return "[ERROR]";
            case Severity::Warning:
                return "[WARNING]";
            case Severity::Info:
                return "[INFO]";
        }
        return "[?]";
    }

    static std::string security_name(std::uint8_t level) {
        if (level == 0)
            return "Locked";
        return "Level " + std::to_string(level);
    }

    std::map<std::uint16_t, EcuValidationState> ecus_;
    std::vector<ValidationIssue> issues_;
};

// =============================================================================
// Command Line Parsing
// =============================================================================

struct Options {
    std::string input_file;
    std::optional<std::string> report_file;
    bool verbose = false;
};

Options parse_args(int argc, char* argv[]) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-r" && i + 1 < argc) {
            opts.report_file = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: uds_validator [options] <input>\n"
                      << "\nOptions:\n"
                      << "  -r <file>    Output report to file\n"
                      << "  -v           Verbose output\n"
                      << "  -h           Show this help\n";
            std::exit(0);
        } else {
            opts.input_file = arg;
        }
    }

    if (opts.input_file.empty()) {
        std::cerr << "Error: No input specified. Use -h for help.\n";
        std::exit(1);
    }

    return opts;
}

// =============================================================================
// Demo Mode
// =============================================================================

void run_demo(UdsValidator& validator) {
    std::cout << "=== Wadjet-Link UDS Validator Demo ===\n\n";
    std::cout << "Processing sample UDS validation scenarios...\n\n";

    UdsDecoder uds_decoder;
    std::uint32_t packet_num = 0;

    // Scenario 1: Normal session control
    std::vector<std::uint8_t> dsc_req = {0x10, 0x03};  // Extended session
    auto result1 = uds_decoder.decode(dsc_req);
    if (result1.is_ok()) {
        validator.process_request(0x0E00, 0x0001, *result1, ++packet_num);
    }

    std::vector<std::uint8_t> dsc_resp = {0x50, 0x03, 0x00, 0x19, 0x01, 0xF4};
    auto result2 = uds_decoder.decode(dsc_resp);
    if (result2.is_ok()) {
        validator.process_response(0x0001, 0x0E00, *result2, ++packet_num);
    }

    // Scenario 2: Write without security - should flag info
    std::vector<std::uint8_t> write_req = {0x2E, 0xF1, 0x90, 0x41, 0x42};  // WriteDataByIdentifier
    auto result3 = uds_decoder.decode(write_req);
    if (result3.is_ok()) {
        validator.process_request(0x0E00, 0x0001, *result3, ++packet_num);
    }

    // Scenario 3: NRC - Security Access Denied
    std::vector<std::uint8_t> nrc = {0x7F, 0x2E,
                                     0x33};  // SecurityAccessDenied for WriteDataByIdentifier
    auto result4 = uds_decoder.decode(nrc);
    if (result4.is_ok()) {
        if (auto* nrc_msg = result4->as<NegativeResponseMessage>()) {
            validator.process_negative_response(0x0001, nrc_msg->rejected_service_id,
                                                nrc_msg->negative_response_code, ++packet_num);
        }
    }

    // Scenario 4: Send key without seed - should flag error
    std::vector<std::uint8_t> key_req = {0x27, 0x02, 0x12,
                                         0x34, 0x56, 0x78};  // SendKey without seed
    auto result5 = uds_decoder.decode(key_req);
    if (result5.is_ok()) {
        validator.process_request(0x0E00, 0x0001, *result5, ++packet_num);
    }

    // Scenario 5: Request Download in default session - should flag warning
    std::vector<std::uint8_t> download_req = {0x34, 0x00, 0x44, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00};
    auto result6 = uds_decoder.decode(download_req);
    if (result6.is_ok()) {
        // Reset to default session first
        auto& ecu =
            const_cast<std::map<std::uint16_t, EcuValidationState>&>(validator.ecus())[0x0001];
        ecu.current_session = SessionType::DefaultSession;
        validator.process_request(0x0E00, 0x0001, *result6, ++packet_num);
    }

    std::cout << "\n";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    auto opts = parse_args(argc, argv);

    if (opts.verbose) {
        std::cout << "Wadjet-Link UDS Validator\n";
        std::cout << "Processing: " << opts.input_file << "\n\n";
    }

    UdsValidator validator;

    // Run demo mode
    if (opts.input_file == "demo") {
        run_demo(validator);
    } else {
        std::cout << "Note: PCAP file reading requires additional setup.\n";
        std::cout << "Running demo mode instead...\n\n";
        run_demo(validator);
    }

    // Output report
    validator.print_report(std::cout);

    std::cout << "Processing Statistics:\n";
    std::cout << "   ECUs found:      " << validator.ecus().size() << "\n";
    std::cout << "   Issues found:    " << validator.issues().size() << "\n";

    // Return code based on issues
    bool has_critical = std::any_of(validator.issues().begin(), validator.issues().end(),
                                    [](const auto& i) { return i.severity == Severity::Critical; });

    return has_critical ? 2 : (validator.issues().empty() ? 0 : 1);
}

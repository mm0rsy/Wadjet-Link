/// @file uds_validator.cpp
/// @brief Example: UDS protocol validator and compliance checker
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to validate UDS messages against ISO 14229
/// specification rules, checking for protocol compliance, timing violations,
/// and common implementation errors.
///
/// Usage:
///   ./uds_validator capture.pcap           # Validate PCAP file
///   ./uds_validator --strict capture.pcap  # Strict mode (more checks)

#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace std::chrono_literals;

// =============================================================================
// Validation Issue Types
// =============================================================================

enum class IssueSeverity {
    Info,
    Warning,
    Error,
    Critical
};

struct ValidationIssue {
    IssueSeverity severity;
    std::string category;
    std::string message;
    std::uint16_t source_addr;
    std::uint16_t target_addr;
    std::chrono::system_clock::time_point timestamp;
    std::string service_name;
};

// =============================================================================
// UDS Protocol Validator
// =============================================================================

class UdsValidator {
public:
    struct Config {
        bool strict_mode = false;
        std::chrono::milliseconds p2_max{50};      // Default P2 timing
        std::chrono::milliseconds p2_star_max{5000}; // Extended P2* timing
        std::chrono::milliseconds s3_timeout{5000};  // Session timeout
    };

    explicit UdsValidator(Config config = {}) : config_(config) {}

    void validate_request(std::uint16_t source, std::uint16_t target,
                         const uds::UdsDecoder::Result& decoded,
                         std::chrono::system_clock::time_point timestamp) {
        // Record request for response matching
        pending_requests_[{target, static_cast<std::uint8_t>(decoded.service_id)}] = {
            source, target, decoded.service_id, timestamp
        };

        // Service-specific validation
        switch (decoded.service_id) {
            case uds::ServiceId::DiagnosticSessionControl:
                validate_session_control_request(source, target, decoded, timestamp);
                break;
            case uds::ServiceId::SecurityAccess:
                validate_security_access_request(source, target, decoded, timestamp);
                break;
            case uds::ServiceId::ReadDataByIdentifier:
                validate_rdbi_request(source, target, decoded, timestamp);
                break;
            case uds::ServiceId::WriteDataByIdentifier:
                validate_wdbi_request(source, target, decoded, timestamp);
                break;
            case uds::ServiceId::RoutineControl:
                validate_routine_control_request(source, target, decoded, timestamp);
                break;
            case uds::ServiceId::RequestDownload:
            case uds::ServiceId::RequestUpload:
                validate_transfer_request(source, target, decoded, timestamp);
                break;
            default:
                break;
        }

        // Check message length
        validate_message_length(source, target, decoded, timestamp, true);
    }

    void validate_response(std::uint16_t source, std::uint16_t target,
                          const uds::UdsDecoder::Result& decoded,
                          std::chrono::system_clock::time_point timestamp) {
        // Find matching request
        auto request_sid = static_cast<std::uint8_t>(decoded.service_id) - 0x40;
        auto key = std::make_pair(source, request_sid);
        
        if (auto it = pending_requests_.find(key); it != pending_requests_.end()) {
            // Check P2 timing
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp - it->second.timestamp);
            
            if (elapsed > config_.p2_max) {
                auto& ecu = ecu_state_[source];
                if (ecu.response_pending_active) {
                    // P2* timing applies
                    if (elapsed > config_.p2_star_max) {
                        add_issue(IssueSeverity::Error, "Timing",
                                 "P2* timeout exceeded: " + std::to_string(elapsed.count()) + "ms",
                                 source, target, timestamp, decoded.service_id);
                    }
                } else {
                    add_issue(IssueSeverity::Warning, "Timing",
                             "P2 timeout exceeded: " + std::to_string(elapsed.count()) + "ms",
                             source, target, timestamp, decoded.service_id);
                }
            }
            
            pending_requests_.erase(it);
        } else {
            if (config_.strict_mode) {
                add_issue(IssueSeverity::Warning, "Protocol",
                         "Response without matching request",
                         source, target, timestamp, decoded.service_id);
            }
        }

        // Service-specific validation
        switch (decoded.service_id) {
            case uds::ServiceId::DiagnosticSessionControlResponse:
                validate_session_control_response(source, target, decoded, timestamp);
                break;
            case uds::ServiceId::SecurityAccessResponse:
                validate_security_access_response(source, target, decoded, timestamp);
                break;
            default:
                break;
        }

        // Update session activity
        ecu_state_[source].last_activity = timestamp;
        ecu_state_[source].response_pending_active = false;
    }

    void validate_negative_response(std::uint16_t source,
                                    uds::ServiceId rejected_service,
                                    uds::NegativeResponseCode nrc,
                                    std::chrono::system_clock::time_point timestamp) {
        // Track response pending
        if (nrc == uds::NegativeResponseCode::ResponsePending) {
            ecu_state_[source].response_pending_active = true;
            ecu_state_[source].response_pending_count++;
            
            // ISO 14229 recommends max 10 consecutive response pending
            if (ecu_state_[source].response_pending_count > 10) {
                add_issue(IssueSeverity::Warning, "Protocol",
                         "Excessive ResponsePending count: " + 
                         std::to_string(ecu_state_[source].response_pending_count),
                         source, 0, timestamp, rejected_service);
            }
            return;
        }

        // Reset response pending counter
        ecu_state_[source].response_pending_active = false;
        ecu_state_[source].response_pending_count = 0;

        // Check for suspicious NRC patterns
        validate_nrc(source, rejected_service, nrc, timestamp);
    }

    void check_session_timeouts(std::chrono::system_clock::time_point current_time) {
        for (auto& [addr, state] : ecu_state_) {
            if (state.session != uds::SessionType::Default &&
                state.last_activity.time_since_epoch().count() > 0) {
                
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - state.last_activity);
                
                if (elapsed > config_.s3_timeout) {
                    add_issue(IssueSeverity::Info, "Session",
                             "Session likely timed out after " + 
                             std::to_string(elapsed.count()) + "ms",
                             addr, 0, current_time, uds::ServiceId::DiagnosticSessionControl);
                    state.session = uds::SessionType::Default;
                }
            }
        }
    }

    const std::vector<ValidationIssue>& issues() const { return issues_; }

    void print_report() const {
        std::cout << "\n╔══════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                       UDS Validation Report                              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════════╝\n\n";

        // Count by severity
        std::map<IssueSeverity, int> counts;
        for (const auto& issue : issues_) {
            counts[issue.severity]++;
        }

        std::cout << "📊 Summary:\n";
        std::cout << "   🔴 Critical: " << counts[IssueSeverity::Critical] << "\n";
        std::cout << "   🟠 Errors:   " << counts[IssueSeverity::Error] << "\n";
        std::cout << "   🟡 Warnings: " << counts[IssueSeverity::Warning] << "\n";
        std::cout << "   🔵 Info:     " << counts[IssueSeverity::Info] << "\n\n";

        if (issues_.empty()) {
            std::cout << "✅ No validation issues found!\n";
            return;
        }

        // Group by category
        std::map<std::string, std::vector<const ValidationIssue*>> by_category;
        for (const auto& issue : issues_) {
            by_category[issue.category].push_back(&issue);
        }

        for (const auto& [category, cat_issues] : by_category) {
            std::cout << "📁 " << category << " (" << cat_issues.size() << " issues):\n";
            std::cout << "┌──────────────────────────────────────────────────────────────────────┐\n";
            
            for (const auto* issue : cat_issues) {
                char severity_char = ' ';
                switch (issue->severity) {
                    case IssueSeverity::Critical: severity_char = '!'; break;
                    case IssueSeverity::Error:    severity_char = 'E'; break;
                    case IssueSeverity::Warning:  severity_char = 'W'; break;
                    case IssueSeverity::Info:     severity_char = 'I'; break;
                }
                
                std::cout << "│ [" << severity_char << "] ";
                if (issue->source_addr != 0) {
                    std::cout << "ECU 0x" << std::hex << std::setw(4) << std::setfill('0')
                              << issue->source_addr << std::dec << ": ";
                }
                std::cout << issue->message << "\n";
            }
            
            std::cout << "└──────────────────────────────────────────────────────────────────────┘\n\n";
        }
    }

private:
    struct PendingRequest {
        std::uint16_t source;
        std::uint16_t target;
        uds::ServiceId service;
        std::chrono::system_clock::time_point timestamp;
    };

    struct EcuState {
        uds::SessionType session = uds::SessionType::Default;
        std::uint8_t security_level = 0;
        std::chrono::system_clock::time_point last_activity;
        bool response_pending_active = false;
        int response_pending_count = 0;
        int security_attempt_count = 0;
    };

    void add_issue(IssueSeverity severity, const std::string& category,
                   const std::string& message, std::uint16_t source,
                   std::uint16_t target, std::chrono::system_clock::time_point timestamp,
                   uds::ServiceId service) {
        issues_.push_back({
            severity, category, message, source, target, timestamp,
            uds::service_id_name(service)
        });
    }

    void validate_session_control_request(std::uint16_t source, std::uint16_t target,
                                          const uds::UdsDecoder::Result& decoded,
                                          std::chrono::system_clock::time_point timestamp) {
        if (decoded.payload.empty()) {
            add_issue(IssueSeverity::Error, "Format",
                     "DiagnosticSessionControl missing session type",
                     source, target, timestamp, decoded.service_id);
            return;
        }

        auto session = decoded.payload[0];
        if (session == 0 || session > 0x7F) {
            add_issue(IssueSeverity::Warning, "Format",
                     "Invalid session type: 0x" + to_hex(session),
                     source, target, timestamp, decoded.service_id);
        }
    }

    void validate_session_control_response(std::uint16_t source, std::uint16_t target,
                                           const uds::UdsDecoder::Result& decoded,
                                           std::chrono::system_clock::time_point timestamp) {
        // Response should contain session type and timing parameters
        if (decoded.payload.size() < 5) {
            add_issue(IssueSeverity::Warning, "Format",
                     "DiagnosticSessionControl response missing timing parameters",
                     source, target, timestamp, decoded.service_id);
        } else {
            // Update tracked session
            ecu_state_[source].session = static_cast<uds::SessionType>(decoded.payload[0]);
        }
    }

    void validate_security_access_request(std::uint16_t source, std::uint16_t target,
                                          const uds::UdsDecoder::Result& decoded,
                                          std::chrono::system_clock::time_point timestamp) {
        if (decoded.payload.empty()) {
            add_issue(IssueSeverity::Error, "Format",
                     "SecurityAccess missing subfunction",
                     source, target, timestamp, decoded.service_id);
            return;
        }

        auto& ecu = ecu_state_[target];
        auto subfunction = decoded.payload[0];

        // Check if attempting security without proper session
        if (config_.strict_mode && ecu.session == uds::SessionType::Default) {
            add_issue(IssueSeverity::Warning, "Protocol",
                     "SecurityAccess attempted in default session",
                     source, target, timestamp, decoded.service_id);
        }

        // Odd subfunction = seed request
        if (subfunction & 0x01) {
            ecu.security_attempt_count++;
        }
    }

    void validate_security_access_response(std::uint16_t source, std::uint16_t target,
                                           const uds::UdsDecoder::Result& decoded,
                                           std::chrono::system_clock::time_point timestamp) {
        if (!decoded.payload.empty()) {
            auto subfunction = decoded.payload[0];
            // Even subfunction = successful key validation
            if (!(subfunction & 0x01)) {
                auto level = subfunction / 2;
                ecu_state_[source].security_level = level;
                ecu_state_[source].security_attempt_count = 0;
            }
        }
    }

    void validate_rdbi_request(std::uint16_t source, std::uint16_t target,
                               const uds::UdsDecoder::Result& decoded,
                               std::chrono::system_clock::time_point timestamp) {
        if (decoded.payload.size() < 2) {
            add_issue(IssueSeverity::Error, "Format",
                     "ReadDataByIdentifier missing DID",
                     source, target, timestamp, decoded.service_id);
            return;
        }

        // Check for multiple DIDs (must be pairs)
        if (decoded.payload.size() % 2 != 0) {
            add_issue(IssueSeverity::Error, "Format",
                     "ReadDataByIdentifier has incomplete DID (odd byte count)",
                     source, target, timestamp, decoded.service_id);
        }

        // ISO 14229 allows up to 65535 DIDs, but practical limit is much lower
        int num_dids = decoded.payload.size() / 2;
        if (config_.strict_mode && num_dids > 100) {
            add_issue(IssueSeverity::Warning, "Performance",
                     "ReadDataByIdentifier with " + std::to_string(num_dids) + 
                     " DIDs may cause timeout",
                     source, target, timestamp, decoded.service_id);
        }
    }

    void validate_wdbi_request(std::uint16_t source, std::uint16_t target,
                               const uds::UdsDecoder::Result& decoded,
                               std::chrono::system_clock::time_point timestamp) {
        if (decoded.payload.size() < 3) {
            add_issue(IssueSeverity::Error, "Format",
                     "WriteDataByIdentifier missing DID or data",
                     source, target, timestamp, decoded.service_id);
        }

        // Check security requirements for write
        auto& ecu = ecu_state_[target];
        if (config_.strict_mode && ecu.security_level == 0) {
            add_issue(IssueSeverity::Info, "Security",
                     "WriteDataByIdentifier without security unlock",
                     source, target, timestamp, decoded.service_id);
        }
    }

    void validate_routine_control_request(std::uint16_t source, std::uint16_t target,
                                          const uds::UdsDecoder::Result& decoded,
                                          std::chrono::system_clock::time_point timestamp) {
        if (decoded.payload.size() < 3) {
            add_issue(IssueSeverity::Error, "Format",
                     "RoutineControl missing subfunction or routine ID",
                     source, target, timestamp, decoded.service_id);
            return;
        }

        auto control_type = decoded.payload[0];
        if (control_type < 1 || control_type > 3) {
            add_issue(IssueSeverity::Warning, "Format",
                     "RoutineControl invalid control type: 0x" + to_hex(control_type),
                     source, target, timestamp, decoded.service_id);
        }
    }

    void validate_transfer_request(std::uint16_t source, std::uint16_t target,
                                   const uds::UdsDecoder::Result& decoded,
                                   std::chrono::system_clock::time_point timestamp) {
        auto& ecu = ecu_state_[target];

        // Programming operations require programming session
        if (ecu.session != uds::SessionType::Programming) {
            add_issue(IssueSeverity::Error, "Protocol",
                     "Transfer service requires programming session",
                     source, target, timestamp, decoded.service_id);
        }

        // Should be unlocked
        if (ecu.security_level == 0) {
            add_issue(IssueSeverity::Warning, "Security",
                     "Transfer service typically requires security unlock",
                     source, target, timestamp, decoded.service_id);
        }
    }

    void validate_message_length(std::uint16_t source, std::uint16_t target,
                                 const uds::UdsDecoder::Result& decoded,
                                 std::chrono::system_clock::time_point timestamp,
                                 bool is_request) {
        // Total message size (service ID + payload)
        std::size_t total_length = 1 + decoded.payload.size();

        // Check against typical CAN limits (for informational purposes)
        if (total_length > 4095) {
            add_issue(IssueSeverity::Info, "Format",
                     "Message length " + std::to_string(total_length) + 
                     " bytes requires multi-frame",
                     source, target, timestamp, decoded.service_id);
        }
    }

    void validate_nrc(std::uint16_t source, uds::ServiceId rejected_service,
                      uds::NegativeResponseCode nrc,
                      std::chrono::system_clock::time_point timestamp) {
        switch (nrc) {
            case uds::NegativeResponseCode::SecurityAccessDenied:
            case uds::NegativeResponseCode::InvalidKey:
            case uds::NegativeResponseCode::ExceedNumberOfAttempts:
                add_issue(IssueSeverity::Warning, "Security",
                         "Security failure: " + std::string(uds::nrc_name(nrc)),
                         source, 0, timestamp, rejected_service);
                break;

            case uds::NegativeResponseCode::ServiceNotSupported:
            case uds::NegativeResponseCode::ServiceNotSupportedInActiveSession:
                add_issue(IssueSeverity::Info, "Compatibility",
                         std::string(uds::nrc_name(nrc)),
                         source, 0, timestamp, rejected_service);
                break;

            case uds::NegativeResponseCode::ConditionsNotCorrect:
                add_issue(IssueSeverity::Info, "Sequence",
                         "Conditions not correct - check prerequisites",
                         source, 0, timestamp, rejected_service);
                break;

            default:
                break;
        }
    }

    static std::string to_hex(std::uint8_t value) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X", value);
        return buf;
    }

    Config config_;
    std::vector<ValidationIssue> issues_;
    std::map<std::pair<std::uint16_t, std::uint8_t>, PendingRequest> pending_requests_;
    std::map<std::uint16_t, EcuState> ecu_state_;
};

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: uds_validator [--strict] <pcap_file>\n";
        return 1;
    }

    std::string input_file;
    bool strict_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--strict" || arg == "-s") {
            strict_mode = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: uds_validator [options] <pcap_file>\n"
                      << "\nOptions:\n"
                      << "  --strict, -s   Enable strict validation\n"
                      << "  --help, -h     Show this help\n";
            return 0;
        } else {
            input_file = arg;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        return 1;
    }

    if (!std::filesystem::exists(input_file)) {
        std::cerr << "Error: File not found: " << input_file << "\n";
        return 1;
    }

    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  𓆓 Wadjet-Link UDS Validator                                  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "📁 Analyzing: " << input_file << "\n";
    if (strict_mode) {
        std::cout << "⚠️  Strict mode enabled\n";
    }
    std::cout << "\n";

    UdsValidator::Config config;
    config.strict_mode = strict_mode;
    UdsValidator validator(config);

    uds::UdsDecoder decoder;
    doip::DoIPDecoder doip_decoder;
    std::uint32_t packet_count = 0;
    std::uint32_t uds_message_count = 0;
    std::chrono::system_clock::time_point last_timestamp;

    pcap::PcapReader reader(input_file);
    while (auto packet = reader.next_packet()) {
        packet_count++;
        last_timestamp = packet->timestamp;

        // Check for session timeouts periodically
        if (packet_count % 100 == 0) {
            validator.check_session_timeouts(packet->timestamp);
        }

        // Decode DoIP layer
        auto doip_result = doip_decoder.decode(packet->data);
        if (!doip_result) continue;

        if (doip_result->payload_type != doip::PayloadType::DiagnosticMessage) {
            continue;
        }

        if (doip_result->payload.size() < 4) continue;

        std::uint16_t source = (doip_result->payload[0] << 8) | doip_result->payload[1];
        std::uint16_t target = (doip_result->payload[2] << 8) | doip_result->payload[3];

        std::span<const std::uint8_t> uds_data(
            doip_result->payload.data() + 4,
            doip_result->payload.size() - 4);

        auto uds_result = decoder.decode(uds_data);
        if (!uds_result) continue;

        uds_message_count++;

        // Handle negative response
        if (uds_result->service_id == uds::ServiceId::NegativeResponse) {
            if (uds_result->payload.size() >= 2) {
                auto rejected = static_cast<uds::ServiceId>(uds_result->payload[0]);
                auto nrc = static_cast<uds::NegativeResponseCode>(uds_result->payload[1]);
                validator.validate_negative_response(source, rejected, nrc, packet->timestamp);
            }
            continue;
        }

        // Determine request vs response
        bool is_response = (static_cast<std::uint8_t>(uds_result->service_id) & 0x40) != 0;

        if (is_response) {
            validator.validate_response(source, target, *uds_result, packet->timestamp);
        } else {
            validator.validate_request(source, target, *uds_result, packet->timestamp);
        }
    }

    // Final timeout check
    validator.check_session_timeouts(last_timestamp);

    // Print results
    validator.print_report();

    std::cout << "\n📊 Processing Statistics:\n";
    std::cout << "   Total packets:   " << packet_count << "\n";
    std::cout << "   UDS messages:    " << uds_message_count << "\n";
    std::cout << "   Issues found:    " << validator.issues().size() << "\n";

    return validator.issues().empty() ? 0 : 1;
}

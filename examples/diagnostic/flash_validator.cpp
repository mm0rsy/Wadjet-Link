/// @file flash_validator.cpp
/// @brief Example: ECU flash sequence validator
///
/// This example validates ECU flash/programming sequences according to
/// automotive standards, checking proper session transitions, security
/// unlock, and download sequence integrity.
///
/// Usage:
///   ./flash_validator capture.pcap         # Analyze flash sequence
///   ./flash_validator -e 0x1234 eth0       # Monitor specific ECU

#include <wadjet/protocols/decoder.hpp>
#include <wadjet/protocols/diagnostic.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds/uds.hpp>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::diagnostic;
using namespace wadjet::protocols::uds;
using namespace std::chrono_literals;

// =============================================================================
// Flash Sequence State Machine
// =============================================================================

enum class FlashPhase {
    Idle,
    SessionControl,         // Enter programming session
    SecurityAccess,         // Unlock security
    EraseMemory,            // Erase flash memory
    RequestDownload,        // Initiate download
    TransferData,           // Transfer blocks
    RequestTransferExit,    // Complete transfer
    Verify,                 // Verify flash (routine)
    Reset,                  // ECU reset
    Complete,
    Failed
};

std::string phase_to_string(FlashPhase phase) {
    switch (phase) {
        case FlashPhase::Idle: return "Idle";
        case FlashPhase::SessionControl: return "SessionControl";
        case FlashPhase::SecurityAccess: return "SecurityAccess";
        case FlashPhase::EraseMemory: return "EraseMemory";
        case FlashPhase::RequestDownload: return "RequestDownload";
        case FlashPhase::TransferData: return "TransferData";
        case FlashPhase::RequestTransferExit: return "RequestTransferExit";
        case FlashPhase::Verify: return "Verify";
        case FlashPhase::Reset: return "Reset";
        case FlashPhase::Complete: return "Complete";
        case FlashPhase::Failed: return "Failed";
        default: return "Unknown";
    }
}

struct FlashValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // Sequence metrics
    FlashPhase final_phase = FlashPhase::Idle;
    std::uint32_t total_blocks = 0;
    std::uint64_t total_bytes = 0;
    std::chrono::milliseconds total_duration{0};
    std::chrono::milliseconds transfer_duration{0};
    
    // Timing violations
    std::uint32_t timeout_violations = 0;
    
    void add_error(const std::string& msg) {
        valid = false;
        errors.push_back(msg);
    }
    
    void add_warning(const std::string& msg) {
        warnings.push_back(msg);
    }
};

class FlashSequenceValidator {
public:
    explicit FlashSequenceValidator(std::uint16_t target_ecu = 0)
        : target_ecu_(target_ecu) {
        
        // Configure diagnostic manager
        DiagnosticSessionManager::Options opts = DiagnosticSessionManager::Options::defaults();
        
        manager_ = std::make_unique<DiagnosticSessionManager>(opts);
        
        // Track events
        manager_->on_event([this](DiagnosticEvent event,
                                   const DiagnosticSessionState& state,
                                   const RequestResponsePair* pair) {
            handle_event(event, state, pair);
        });
    }
    
    void process(std::span<const std::byte> data) {
        manager_->process_doip_raw(data);
    }
    
    FlashValidationResult validate() {
        // Check final state
        if (current_phase_ == FlashPhase::Complete) {
            result_.final_phase = FlashPhase::Complete;
        } else if (current_phase_ == FlashPhase::Failed) {
            result_.final_phase = FlashPhase::Failed;
            result_.add_error("Flash sequence failed at phase: " + 
                            phase_to_string(failed_at_phase_));
        } else {
            result_.final_phase = current_phase_;
            result_.add_warning("Flash sequence incomplete, stopped at: " +
                               phase_to_string(current_phase_));
        }
        
        // Check mandatory phases were seen
        if (!saw_programming_session_) {
            result_.add_error("Missing programming session transition");
        }
        if (!saw_security_unlock_) {
            result_.add_error("Missing security unlock");
        }
        if (!saw_request_download_) {
            result_.add_error("Missing RequestDownload");
        }
        if (result_.total_blocks == 0 && saw_request_download_) {
            result_.add_error("No TransferData blocks received");
        }
        if (!saw_transfer_exit_ && saw_request_download_) {
            result_.add_warning("Missing RequestTransferExit");
        }
        
        // Calculate duration
        if (start_time_.time_since_epoch().count() > 0 &&
            end_time_.time_since_epoch().count() > 0) {
            result_.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time_ - start_time_);
        }
        
        return result_;
    }

private:
    void handle_event(DiagnosticEvent event,
                     const DiagnosticSessionState& state,
                     const RequestResponsePair* pair) {
        // Filter by target ECU if specified
        if (target_ecu_ != 0 && state.gateway_address != target_ecu_) {
            return;
        }
        
        // Track start time
        if (start_time_.time_since_epoch().count() == 0) {
            start_time_ = std::chrono::steady_clock::now();
        }
        end_time_ = std::chrono::steady_clock::now();
        
        // Handle by event type
        switch (event) {
            case DiagnosticEvent::SessionChanged:
                handle_session_change(state);
                break;
                
            case DiagnosticEvent::SecurityUnlocked:
                saw_security_unlock_ = true;
                current_phase_ = FlashPhase::EraseMemory;
                break;
                
            case DiagnosticEvent::FlashStarted:
                saw_request_download_ = true;
                current_phase_ = FlashPhase::TransferData;
                transfer_start_ = std::chrono::steady_clock::now();
                break;
                
            case DiagnosticEvent::FlashProgress:
                if (pair) {
                    result_.total_blocks++;
                    // Estimate bytes from request size
                }
                break;
                
            case DiagnosticEvent::FlashCompleted:
                saw_transfer_exit_ = true;
                current_phase_ = FlashPhase::Verify;
                result_.transfer_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - transfer_start_);
                break;
                
            case DiagnosticEvent::FlashFailed:
                current_phase_ = FlashPhase::Failed;
                failed_at_phase_ = current_phase_;
                if (pair && pair->is_negative()) {
                    auto nrc = pair->get_nrc();
                    if (nrc) {
                        result_.add_error("Flash failed with NRC 0x" + to_hex_string(*nrc));
                    }
                }
                break;
                
            case DiagnosticEvent::ResponseTimeout:
                result_.timeout_violations++;
                result_.add_warning("Response timeout violation detected");
                break;
                
            case DiagnosticEvent::NegativeResponse:
                if (pair) {
                    handle_negative_response(pair);
                }
                break;
                
            default:
                break;
        }
    }
    
    void handle_session_change(const DiagnosticSessionState& state) {
        if (state.session_type == SessionType::ProgrammingSession) {
            saw_programming_session_ = true;
            current_phase_ = FlashPhase::SecurityAccess;
        }
    }
    
    void handle_negative_response(const RequestResponsePair* pair) {
        auto nrc = pair->get_nrc();
        if (!nrc) return;
        
        // ResponsePending is acceptable
        if (*nrc == 0x78) {  // ResponsePending
            return;
        }
        
        // Some NRCs are more serious
        if (*nrc == 0x33 ||   // SecurityAccessDenied
            *nrc == 0x72 ||   // GeneralProgrammingFailure
            *nrc == 0x92 ||   // VoltageTooHigh
            *nrc == 0x93) {   // VoltageTooLow
            current_phase_ = FlashPhase::Failed;
            failed_at_phase_ = current_phase_;
            result_.add_error("Critical NRC received: 0x" + to_hex_string(*nrc));
        } else {
            result_.add_warning("Negative response: NRC 0x" + to_hex_string(*nrc));
        }
    }
    
    static std::string to_hex_string(std::uint8_t value) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<int>(value);
        return oss.str();
    }
    
    std::uint16_t target_ecu_;
    std::unique_ptr<DiagnosticSessionManager> manager_;
    FlashValidationResult result_;
    
    FlashPhase current_phase_ = FlashPhase::Idle;
    FlashPhase failed_at_phase_ = FlashPhase::Idle;
    
    bool saw_programming_session_ = false;
    bool saw_security_unlock_ = false;
    bool saw_request_download_ = false;
    bool saw_transfer_exit_ = false;
    
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    std::chrono::steady_clock::time_point transfer_start_;
};

// =============================================================================
// Main Application
// =============================================================================

void print_result(const FlashValidationResult& result) {
    std::cout << "\n========== Flash Validation Result ==========\n\n";
    
    std::cout << "Status: " << (result.valid ? "✅ VALID" : "❌ INVALID") << "\n";
    std::cout << "Final Phase: " << phase_to_string(result.final_phase) << "\n\n";
    
    std::cout << "Transfer Metrics:\n";
    std::cout << "  Total blocks: " << result.total_blocks << "\n";
    std::cout << "  Total bytes: " << result.total_bytes << " ("
              << (result.total_bytes / 1024.0) << " KB)\n";
    std::cout << "  Total duration: " << result.total_duration.count() << " ms\n";
    std::cout << "  Transfer duration: " << result.transfer_duration.count() << " ms\n";
    
    if (result.transfer_duration.count() > 0) {
        double throughput = (result.total_bytes * 1000.0) / 
                           result.transfer_duration.count() / 1024.0;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " KB/s\n";
    }
    
    std::cout << "\nTiming Violations:\n";
    std::cout << "  Timeout violations: " << result.timeout_violations << "\n";
    
    if (!result.errors.empty()) {
        std::cout << "\nErrors:\n";
        for (const auto& err : result.errors) {
            std::cout << "  ❌ " << err << "\n";
        }
    }
    
    if (!result.warnings.empty()) {
        std::cout << "\nWarnings:\n";
        for (const auto& warn : result.warnings) {
            std::cout << "  ⚠️  " << warn << "\n";
        }
    }
    
    std::cout << "\n==============================================\n";
}

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options] <pcap-file>\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -e <addr>  Target ECU address (hex)\n";
    std::cerr << "  -h         Show this help\n\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << program << " -e 0x1234 flash_capture.pcap\n";
}

int main(int argc, char* argv[]) {
    std::string pcap_file;
    std::uint16_t target_ecu = 0;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            target_ecu = static_cast<std::uint16_t>(
                std::stoul(argv[++i], nullptr, 0));
        } else if (argv[i][0] != '-') {
            pcap_file = argv[i];
        }
    }
    
    if (pcap_file.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::cout << "𓆓 Wadjet-Link Flash Sequence Validator\n";
    std::cout << "========================================\n";
    std::cout << "Input: " << pcap_file << "\n";
    if (target_ecu != 0) {
        std::cout << "Target ECU: 0x" << std::hex << target_ecu << std::dec << "\n";
    }
    std::cout << "\nValidating...\n";
    
    FlashSequenceValidator validator(target_ecu);
    
    // Note: In real implementation, read and process PCAP file
    // For now, show placeholder
    std::cerr << "PCAP processing not implemented in this example.\n";
    std::cerr << "Use the full wadjet replay functionality.\n";
    
    auto result = validator.validate();
    print_result(result);
    
    return result.valid ? 0 : 1;
}

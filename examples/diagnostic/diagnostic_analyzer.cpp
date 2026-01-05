/// @file diagnostic_analyzer.cpp
/// @brief Example: Comprehensive diagnostic session analyzer using DiagnosticSessionManager
///
/// This example demonstrates using the DiagnosticSessionManager to analyze
/// automotive diagnostic traffic, correlate requests with responses, and
/// track session states across multiple ECUs.
///
/// Usage:
///   ./diagnostic_analyzer eth0              # Live capture on interface
///   ./diagnostic_analyzer capture.pcap      # Analyze from PCAP file
///   ./diagnostic_analyzer -t 5000 eth0      # Set P2 timeout (ms)

#include <wadjet/protocols/decoder.hpp>
#include <wadjet/protocols/diagnostic.hpp>
#include <wadjet/protocols/doip.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::diagnostic;
using namespace std::chrono_literals;

// Global flag for clean shutdown
static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

// =============================================================================
// Output Helpers
// =============================================================================

std::string event_to_string(DiagnosticEvent event) {
    switch (event) {
        case DiagnosticEvent::RoutingActivated: return "RoutingActivated";
        case DiagnosticEvent::RoutingDeactivated: return "RoutingDeactivated";
        case DiagnosticEvent::ConnectionLost: return "ConnectionLost";
        case DiagnosticEvent::SessionStarted: return "SessionStarted";
        case DiagnosticEvent::SessionChanged: return "SessionChanged";
        case DiagnosticEvent::SessionTimeout: return "SessionTimeout";
        case DiagnosticEvent::SessionEnded: return "SessionEnded";
        case DiagnosticEvent::SecurityUnlocked: return "SecurityUnlocked";
        case DiagnosticEvent::SecurityLocked: return "SecurityLocked";
        case DiagnosticEvent::SecurityLockout: return "SecurityLockout";
        case DiagnosticEvent::RequestSent: return "RequestSent";
        case DiagnosticEvent::ResponseReceived: return "ResponseReceived";
        case DiagnosticEvent::ResponsePending: return "ResponsePending";
        case DiagnosticEvent::ResponseTimeout: return "ResponseTimeout";
        case DiagnosticEvent::NegativeResponse: return "NegativeResponse";
        case DiagnosticEvent::DTCsRead: return "DTCsRead";
        case DiagnosticEvent::DTCsCleared: return "DTCsCleared";
        case DiagnosticEvent::DataIdentifierRead: return "DataIdentifierRead";
        case DiagnosticEvent::FlashStarted: return "FlashStarted";
        case DiagnosticEvent::FlashProgress: return "FlashProgress";
        case DiagnosticEvent::FlashCompleted: return "FlashCompleted";
        case DiagnosticEvent::FlashFailed: return "FlashFailed";
        default: return "Unknown";
    }
}

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void print_event(DiagnosticEvent event, const DiagnosticSessionState& state,
                 const RequestResponsePair* pair) {
    std::cout << "[" << timestamp() << "] ";
    std::cout << std::setfill('0');
    std::cout << "ECU 0x" << std::hex << std::setw(4) << state.tester_address;
    std::cout << " -> 0x" << std::setw(4) << state.gateway_address;
    std::cout << std::dec << " | " << event_to_string(event);
    
    if (pair) {
        std::cout << " | SID 0x" << std::hex << std::setw(2) 
                  << static_cast<int>(pair->request.header.service_id);
        if (auto rt = pair->response_time()) {
            std::cout << std::dec << " | " << rt->count() << "ms";
        }
        if (pair->is_negative()) {
            auto nrc = pair->get_nrc();
            if (nrc) {
                std::cout << " | NRC 0x" << std::hex << std::setw(2) 
                          << static_cast<int>(*nrc);
            }
        }
    }
    
    std::cout << std::dec << "\n";
}

void print_statistics(DiagnosticSessionManager& manager) {
    auto stats = manager.statistics();
    auto corr_stats = manager.correlator().statistics();
    
    std::cout << "\n========== Diagnostic Analysis Summary ==========\n\n";
    
    std::cout << "Session Manager Statistics:\n";
    std::cout << "  DoIP packets processed:  " << stats.doip_packets_processed << "\n";
    std::cout << "  Diagnostic messages:     " << stats.diagnostic_messages << "\n";
    std::cout << "  Routing activations:     " << stats.routing_activations << "\n";
    std::cout << "  Vehicle identifications: " << stats.vehicle_identifications << "\n";
    std::cout << "  UDS requests:            " << stats.uds_requests << "\n";
    std::cout << "  UDS responses:           " << stats.uds_responses << "\n";
    std::cout << "  Decode errors:           " << stats.decode_errors << "\n\n";
    
    std::cout << "Request Correlator Statistics:\n";
    std::cout << "  Requests recorded:   " << corr_stats.requests_recorded << "\n";
    std::cout << "  Responses matched:   " << corr_stats.responses_matched << "\n";
    std::cout << "  Responses unmatched: " << corr_stats.responses_unmatched << "\n";
    std::cout << "  Pending timeouts:    " << corr_stats.pending_timeouts << "\n";
    std::cout << "  Positive responses:  " << corr_stats.positive_responses << "\n";
    std::cout << "  Negative responses:  " << corr_stats.negative_responses << "\n";
    std::cout << "  ResponsePending:     " << corr_stats.response_pending_count << "\n";
    std::cout << "  Match rate:          " << std::fixed << std::setprecision(1) 
              << (corr_stats.match_rate() * 100) << "%\n\n";
    
    // ECU summary
    auto ecus = manager.get_tracked_ecus();
    if (!ecus.empty()) {
        std::cout << "Tracked ECUs:\n";
        for (auto ecu : ecus) {
            auto* state = manager.get_session_state(ecu);
            if (state) {
                std::cout << "  ECU 0x" << std::hex << std::setw(4) 
                          << std::setfill('0') << ecu << std::dec << ":\n";
                std::cout << "    Session active: " 
                          << (state->session_active ? "Yes" : "No") << "\n";
                std::cout << "    Session type: " 
                          << static_cast<int>(state->session_type) << "\n";
                std::cout << "    Security level: " 
                          << static_cast<int>(state->security_level) << "\n";
                std::cout << "    Requests: " << state->requests_sent << "\n";
                std::cout << "    Responses: " << state->responses_received << "\n";
                std::cout << "    Negative: " << state->negative_responses << "\n";
                std::cout << "    Timeouts: " << state->timeouts << "\n\n";
            }
        }
    }
    
    std::cout << "=================================================\n";
}

// =============================================================================
// Main Application
// =============================================================================

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options] <interface|pcap>\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -t <ms>    P2 server timeout in milliseconds (default: 50)\n";
    std::cerr << "  -T <ms>    P2* server timeout in milliseconds (default: 5000)\n";
    std::cerr << "  -v         Verbose output (show all events)\n";
    std::cerr << "  -h         Show this help\n\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << program << " eth0\n";
    std::cerr << "  " << program << " -t 100 capture.pcap\n";
}

int main(int argc, char* argv[]) {
    // Parse arguments
    std::string source;
    DiagnosticTiming timing = DiagnosticTiming::defaults();
    bool verbose = false;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            timing.p2_server_max = std::chrono::milliseconds(std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            timing.p2_star_server_max = std::chrono::milliseconds(std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (argv[i][0] != '-') {
            source = argv[i];
        }
    }
    
    if (source.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Configure session manager
    DiagnosticSessionManager::Options options = DiagnosticSessionManager::Options::defaults();
    options.default_timing = timing;
    options.correlator_options.timing = timing;
    
    DiagnosticSessionManager manager(options);
    
    // Register event callback
    manager.on_event([verbose](DiagnosticEvent event,
                                const DiagnosticSessionState& state,
                                const RequestResponsePair* pair) {
        // Always show important events
        bool important = (event == DiagnosticEvent::SessionStarted ||
                         event == DiagnosticEvent::SessionEnded ||
                         event == DiagnosticEvent::SessionChanged ||
                         event == DiagnosticEvent::SecurityUnlocked ||
                         event == DiagnosticEvent::NegativeResponse ||
                         event == DiagnosticEvent::ResponseTimeout ||
                         event == DiagnosticEvent::FlashStarted ||
                         event == DiagnosticEvent::FlashCompleted ||
                         event == DiagnosticEvent::FlashFailed);
        
        if (verbose || important) {
            print_event(event, state, pair);
        }
    });
    
    std::cout << "𓆓 Wadjet-Link Diagnostic Analyzer\n";
    std::cout << "===================================\n";
    std::cout << "Source: " << source << "\n";
    std::cout << "P2 timeout: " << timing.p2_server_max.count() << "ms\n";
    std::cout << "P2* timeout: " << timing.p2_star_server_max.count() << "ms\n";
    std::cout << "Verbose: " << (verbose ? "Yes" : "No") << "\n\n";
    
    // Check if source is a PCAP file or live interface
    bool is_pcap = (source.find(".pcap") != std::string::npos ||
                    source.find(".pcapng") != std::string::npos);
    
    if (is_pcap) {
        std::cout << "Analyzing PCAP file: " << source << "\n\n";
        
        // Process PCAP file
        // Note: In real implementation, use wadjet pcap reader
        std::cerr << "PCAP analysis not implemented in this example.\n";
        std::cerr << "Use the full wadjet replay functionality.\n";
    } else {
        std::cout << "Starting live capture on: " << source << "\n";
        std::cout << "Press Ctrl+C to stop...\n\n";
        
        // Live capture loop
        while (g_running) {
            // Note: In real implementation, integrate with pcap live capture
            // manager.process_doip_raw(raw_data);
            std::this_thread::sleep_for(100ms);
        }
    }
    
    // Print final statistics
    print_statistics(manager);
    
    return 0;
}

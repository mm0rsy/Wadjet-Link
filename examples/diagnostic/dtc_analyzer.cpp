/// @file dtc_analyzer.cpp
/// @brief Example: DTC (Diagnostic Trouble Code) analyzer
///
/// This example analyzes ReadDTCInformation responses to extract and
/// categorize diagnostic trouble codes, tracking their status and history.
///
/// Usage:
///   ./dtc_analyzer capture.pcap         # Analyze DTCs from capture
///   ./dtc_analyzer -e 0x1234 eth0       # Live monitor specific ECU

#include <wadjet/protocols/decoder.hpp>
#include <wadjet/protocols/diagnostic.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds/uds.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::diagnostic;
using namespace wadjet::protocols::uds;
using namespace std::chrono_literals;

// =============================================================================
// DTC Data Structures
// =============================================================================

struct DTCInfo {
    std::uint32_t dtc_code = 0;      // 3-byte DTC
    std::uint8_t status_mask = 0;    // DTC status byte
    std::uint16_t ecu_address = 0;
    std::chrono::steady_clock::time_point first_seen;
    std::chrono::steady_clock::time_point last_seen;
    std::uint32_t occurrence_count = 0;
    
    // Status bit helpers
    bool is_test_failed() const { return (status_mask & 0x01) != 0; }
    bool is_test_failed_this_cycle() const { return (status_mask & 0x02) != 0; }
    bool is_pending() const { return (status_mask & 0x04) != 0; }
    bool is_confirmed() const { return (status_mask & 0x08) != 0; }
    bool is_test_not_completed_since_clear() const { return (status_mask & 0x10) != 0; }
    bool is_test_failed_since_clear() const { return (status_mask & 0x20) != 0; }
    bool is_test_not_completed_this_cycle() const { return (status_mask & 0x40) != 0; }
    bool is_warning_indicator_on() const { return (status_mask & 0x80) != 0; }
    
    std::string status_string() const {
        std::string s;
        if (is_test_failed()) s += "F";
        if (is_test_failed_this_cycle()) s += "C";
        if (is_pending()) s += "P";
        if (is_confirmed()) s += "D";  // Confirmed/Definitive
        if (is_test_failed_since_clear()) s += "S";  // Since-clear
        if (is_warning_indicator_on()) s += "W";
        return s.empty() ? "-" : s;
    }
};

// Common DTC categories
enum class DTCCategory {
    Unknown,
    Powertrain,
    Chassis,
    Body,
    Network
};

std::string category_string(DTCCategory cat) {
    switch (cat) {
        case DTCCategory::Powertrain: return "Powertrain";
        case DTCCategory::Chassis: return "Chassis";
        case DTCCategory::Body: return "Body";
        case DTCCategory::Network: return "Network";
        default: return "Unknown";
    }
}

DTCCategory categorize_dtc(std::uint32_t dtc) {
    // First nibble (first hex digit) determines category
    std::uint8_t category_byte = (dtc >> 16) & 0xFF;
    std::uint8_t first_nibble = (category_byte >> 4) & 0x0F;
    
    switch (first_nibble) {
        case 0x00: case 0x01: case 0x02: case 0x03:
            return DTCCategory::Powertrain;
        case 0x04: case 0x05: case 0x06: case 0x07:
            return DTCCategory::Chassis;
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            return DTCCategory::Body;
        case 0x0C: case 0x0D: case 0x0E: case 0x0F:
            return DTCCategory::Network;
        default:
            return DTCCategory::Unknown;
    }
}

std::string format_dtc(std::uint32_t dtc) {
    // Format as standard OBD-II style: P1234
    char prefix;
    std::uint8_t first_byte = (dtc >> 16) & 0xFF;
    std::uint8_t first_nibble = (first_byte >> 4) & 0x03;  // bits 4-5
    
    switch ((first_byte >> 6) & 0x03) {
        case 0: prefix = 'P'; break;
        case 1: prefix = 'C'; break;
        case 2: prefix = 'B'; break;
        case 3: prefix = 'U'; break;
        default: prefix = '?';
    }
    
    std::ostringstream oss;
    oss << prefix << std::hex << std::uppercase 
        << std::setfill('0') << std::setw(1) << static_cast<int>(first_nibble)
        << std::setw(2) << ((dtc >> 8) & 0xFF)
        << std::setw(2) << (dtc & 0xFF);
    return oss.str();
}

// =============================================================================
// DTC Analyzer
// =============================================================================

class DTCAnalyzer {
public:
    explicit DTCAnalyzer(std::uint16_t target_ecu = 0)
        : target_ecu_(target_ecu) {
        
        DiagnosticSessionManager::Options opts = DiagnosticSessionManager::Options::defaults();
        
        manager_ = std::make_unique<DiagnosticSessionManager>(opts);
        
        manager_->on_event([this](DiagnosticEvent event,
                                   const DiagnosticSessionState& state,
                                   const RequestResponsePair* pair) {
            if (event == DiagnosticEvent::DTCsRead && pair) {
                process_dtc_response(state.gateway_address, pair);
            } else if (event == DiagnosticEvent::DTCsCleared) {
                handle_dtc_clear(state.gateway_address);
            }
        });
    }
    
    void process(std::span<const std::byte> data) {
        manager_->process_doip_raw(data);
    }
    
    const std::map<std::uint32_t, DTCInfo>& get_dtcs() const {
        return dtcs_;
    }
    
    std::vector<DTCInfo> get_sorted_dtcs() const {
        std::vector<DTCInfo> result;
        for (const auto& [code, info] : dtcs_) {
            result.push_back(info);
        }
        
        // Sort by category, then by confirmed status, then by code
        std::sort(result.begin(), result.end(), [](const DTCInfo& a, const DTCInfo& b) {
            auto cat_a = categorize_dtc(a.dtc_code);
            auto cat_b = categorize_dtc(b.dtc_code);
            if (cat_a != cat_b) return cat_a < cat_b;
            if (a.is_confirmed() != b.is_confirmed()) return a.is_confirmed() > b.is_confirmed();
            return a.dtc_code < b.dtc_code;
        });
        
        return result;
    }
    
    void print_summary() const {
        auto dtcs = get_sorted_dtcs();
        
        std::cout << "\n========== DTC Analysis Summary ==========\n\n";
        
        if (dtcs.empty()) {
            std::cout << "No DTCs found.\n";
            return;
        }
        
        // Count by category and status
        std::map<DTCCategory, std::uint32_t> by_category;
        std::uint32_t confirmed = 0, pending = 0, warning = 0;
        
        for (const auto& dtc : dtcs) {
            by_category[categorize_dtc(dtc.dtc_code)]++;
            if (dtc.is_confirmed()) confirmed++;
            if (dtc.is_pending()) pending++;
            if (dtc.is_warning_indicator_on()) warning++;
        }
        
        std::cout << "Total DTCs: " << dtcs.size() << "\n";
        std::cout << "  Confirmed: " << confirmed << "\n";
        std::cout << "  Pending: " << pending << "\n";
        std::cout << "  Warning indicator: " << warning << "\n\n";
        
        std::cout << "By Category:\n";
        for (const auto& [cat, count] : by_category) {
            std::cout << "  " << category_string(cat) << ": " << count << "\n";
        }
        
        std::cout << "\nDetailed DTC List:\n";
        std::cout << std::setfill('-') << std::setw(70) << "-" << "\n";
        std::cout << std::setfill(' ');
        std::cout << std::left << std::setw(10) << "DTC"
                  << std::setw(12) << "Category"
                  << std::setw(8) << "Status"
                  << std::setw(8) << "ECU"
                  << std::setw(8) << "Count"
                  << "Description\n";
        std::cout << std::setfill('-') << std::setw(70) << "-" << "\n";
        std::cout << std::setfill(' ');
        
        for (const auto& dtc : dtcs) {
            std::cout << std::left << std::setw(10) << format_dtc(dtc.dtc_code)
                      << std::setw(12) << category_string(categorize_dtc(dtc.dtc_code))
                      << std::setw(8) << dtc.status_string()
                      << "0x" << std::hex << std::setw(4) << std::setfill('0') 
                      << dtc.ecu_address << std::dec << std::setfill(' ')
                      << "  " << std::setw(6) << dtc.occurrence_count
                      << lookup_dtc_description(dtc.dtc_code) << "\n";
        }
        
        std::cout << std::setfill('-') << std::setw(70) << "-" << "\n";
        std::cout << "\nStatus Legend: F=Failed, C=ThisCycle, P=Pending, "
                  << "D=Confirmed, S=SinceClear, W=Warning\n";
        std::cout << "\n===========================================\n";
    }
    
private:
    void process_dtc_response(std::uint16_t ecu, const RequestResponsePair* pair) {
        if (!pair->is_complete() || !pair->response) {
            return;
        }
        
        // Note: In real implementation, parse the UDS response data
        // For this example, we show the structure
        std::cout << "[INFO] DTC response received from ECU 0x" 
                  << std::hex << ecu << std::dec << "\n";
    }
    
    void handle_dtc_clear(std::uint16_t ecu) {
        std::cout << "[INFO] DTCs cleared on ECU 0x" << std::hex << ecu 
                  << std::dec << "\n";
        cleared_ecus_.insert(ecu);
    }
    
    std::string lookup_dtc_description(std::uint32_t dtc) const {
        // Sample DTC descriptions - in real app, load from database
        static const std::map<std::uint32_t, std::string> descriptions = {
            {0x010100, "Mass Air Flow Sensor A Circuit Range"},
            {0x013000, "Ignition Coil A Primary Control Circuit"},
            {0x017100, "System Too Lean Bank 1"},
            {0x043000, "Idle Air Control System RPM Higher Than Expected"},
            {0x050000, "Vehicle Speed Sensor A Malfunction"},
            {0x060000, "Catalyst System Efficiency Below Threshold"},
            {0x0C0100, "Lost Communication With TCM"},
            {0x0C0200, "Lost Communication With ABS Module"},
        };
        
        auto it = descriptions.find(dtc);
        return it != descriptions.end() ? it->second : "";
    }
    
    std::uint16_t target_ecu_;
    std::unique_ptr<DiagnosticSessionManager> manager_;
    std::map<std::uint32_t, DTCInfo> dtcs_;
    std::set<std::uint16_t> cleared_ecus_;
};

// =============================================================================
// Main Application
// =============================================================================

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options] <pcap-file|interface>\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -e <addr>  Target ECU address (hex)\n";
    std::cerr << "  -h         Show this help\n\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << program << " diagnostic_capture.pcap\n";
    std::cerr << "  " << program << " -e 0x1234 eth0\n";
}

int main(int argc, char* argv[]) {
    std::string source;
    std::uint16_t target_ecu = 0;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            target_ecu = static_cast<std::uint16_t>(
                std::stoul(argv[++i], nullptr, 0));
        } else if (argv[i][0] != '-') {
            source = argv[i];
        }
    }
    
    if (source.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::cout << "𓆓 Wadjet-Link DTC Analyzer\n";
    std::cout << "===========================\n";
    std::cout << "Source: " << source << "\n";
    if (target_ecu != 0) {
        std::cout << "Target ECU: 0x" << std::hex << target_ecu << std::dec << "\n";
    }
    std::cout << "\nAnalyzing...\n";
    
    DTCAnalyzer analyzer(target_ecu);
    
    // Note: In real implementation, read and process PCAP or live capture
    std::cerr << "\nPCAP/live processing not implemented in this example.\n";
    std::cerr << "Use the full wadjet replay functionality.\n";
    
    analyzer.print_summary();
    
    return 0;
}

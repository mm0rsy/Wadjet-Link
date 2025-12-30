/// @file report.cpp
/// @brief Report generator implementations
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/scenario/report.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef __linux__
#include <unistd.h>
#endif

namespace wadjet::scenario {

namespace {

std::string get_hostname() {
#ifdef __linux__
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return hostname;
    }
#endif
    return "localhost";
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);
    
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

std::string escape_xml(const std::string& str) {
    std::string result;
    result.reserve(str.size() + str.size() / 10);  // ~10% extra for escapes
    
    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c;        break;
        }
    }
    
    return result;
}

std::string escape_json(const std::string& str) {
    std::string result;
    result.reserve(str.size() + str.size() / 10);  // ~10% extra for escapes
    
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c >= 0 && c < 32) {
                    std::ostringstream ss;
                    ss << "\\u" << std::hex << std::setfill('0') 
                       << std::setw(4) << static_cast<int>(c);
                    result += ss.str();
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

double duration_seconds(Duration d) {
    return static_cast<double>(d.count()) / 1000.0;
}

}  // anonymous namespace

// =============================================================================
// JUnit XML Report Generator
// =============================================================================

void JUnitXmlReportGenerator::generate(std::ostream& out,
                                        const BatchResult& results,
                                        const ReportOptions& opts) {
    std::string hostname = opts.hostname.empty() ? get_hostname() : opts.hostname;
    std::string timestamp = get_timestamp();
    
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<testsuites name=\"" << escape_xml(opts.suite_name) << "\""
        << " tests=\"" << results.total() << "\""
        << " failures=\"" << results.failed << "\""
        << " errors=\"0\""
        << " skipped=\"" << results.skipped << "\""
        << " time=\"" << duration_seconds(results.total_elapsed) << "\">\n";
    
    for (const auto& scenario_result : results.results) {
        std::size_t test_count = scenario_result.expect_results.empty() ? 
            1 : scenario_result.expect_results.size();
        std::size_t failures = scenario_result.failed_count();
        
        out << "  <testsuite name=\"" << escape_xml(scenario_result.scenario_name) << "\""
            << " tests=\"" << test_count << "\""
            << " failures=\"" << failures << "\""
            << " errors=\"0\""
            << " skipped=\"0\"";
        
        if (opts.include_timestamps) {
            out << " timestamp=\"" << timestamp << "\"";
        }
        
        out << " hostname=\"" << escape_xml(hostname) << "\""
            << " time=\"" << duration_seconds(scenario_result.total_elapsed) << "\">\n";
        
        if (scenario_result.expect_results.empty()) {
            // Single test case for scenario without expectations
            out << "    <testcase name=\"" << escape_xml(scenario_result.scenario_name) << "\""
                << " classname=\"" << escape_xml(opts.suite_name) << "\""
                << " time=\"" << duration_seconds(scenario_result.total_elapsed) << "\"";
            
            if (!scenario_result.passed) {
                out << ">\n";
                out << "      <failure message=\"" 
                    << escape_xml(scenario_result.error_message) << "\"/>\n";
                out << "    </testcase>\n";
            } else {
                out << "/>\n";
            }
        } else {
            // One test case per expectation
            for (const auto& expect : scenario_result.expect_results) {
                out << "    <testcase name=\"" << escape_xml(expect.description) << "\""
                    << " classname=\"" << escape_xml(scenario_result.scenario_name) << "\""
                    << " time=\"" << duration_seconds(expect.elapsed) << "\"";
                
                if (!expect.passed) {
                    out << ">\n";
                    out << "      <failure message=\"" 
                        << escape_xml(expect.failure_reason) << "\">"
                        << "Matched " << expect.packets_matched << " packets"
                        << "</failure>\n";
                    out << "    </testcase>\n";
                } else {
                    out << "/>\n";
                }
            }
        }
        
        // System output
        if (opts.include_pcap_paths && scenario_result.pcap_file) {
            out << "    <system-out>PCAP saved: " 
                << escape_xml(*scenario_result.pcap_file) << "</system-out>\n";
        }
        
        out << "  </testsuite>\n";
    }
    
    out << "</testsuites>\n";
}

void JUnitXmlReportGenerator::generate(std::ostream& out,
                                        const ScenarioResult& result,
                                        const ReportOptions& opts) {
    BatchResult batch;
    batch.results.push_back(result);
    batch.total_elapsed = result.total_elapsed;
    batch.passed = result.passed ? 1 : 0;
    batch.failed = result.passed ? 0 : 1;
    generate(out, batch, opts);
}

// =============================================================================
// JSON Report Generator
// =============================================================================

void JsonReportGenerator::generate(std::ostream& out,
                                    const BatchResult& results,
                                    const ReportOptions& opts) {
    std::string indent = opts.pretty_print ? "  " : "";
    std::string newline = opts.pretty_print ? "\n" : "";
    
    out << "{" << newline;
    out << indent << "\"suite\": \"" << escape_json(opts.suite_name) << "\"," << newline;
    out << indent << "\"timestamp\": \"" << get_timestamp() << "\"," << newline;
    out << indent << "\"summary\": {" << newline;
    out << indent << indent << "\"total\": " << results.total() << "," << newline;
    out << indent << indent << "\"passed\": " << results.passed << "," << newline;
    out << indent << indent << "\"failed\": " << results.failed << "," << newline;
    out << indent << indent << "\"skipped\": " << results.skipped << "," << newline;
    out << indent << indent << "\"duration_ms\": " << results.total_elapsed.count() << newline;
    out << indent << "}," << newline;
    out << indent << "\"scenarios\": [" << newline;
    
    bool first_scenario = true;
    for (const auto& scenario_result : results.results) {
        if (!first_scenario) {
            out << "," << newline;
        }
        first_scenario = false;
        
        out << indent << indent << "{" << newline;
        out << indent << indent << indent << "\"name\": \"" 
            << escape_json(scenario_result.scenario_name) << "\"," << newline;
        out << indent << indent << indent << "\"passed\": " 
            << (scenario_result.passed ? "true" : "false") << "," << newline;
        out << indent << indent << indent << "\"duration_ms\": " 
            << scenario_result.total_elapsed.count() << "," << newline;
        
        if (!scenario_result.error_message.empty()) {
            out << indent << indent << indent << "\"error\": \"" 
                << escape_json(scenario_result.error_message) << "\"," << newline;
        }
        
        if (opts.include_pcap_paths && scenario_result.pcap_file) {
            out << indent << indent << indent << "\"pcap_file\": \"" 
                << escape_json(*scenario_result.pcap_file) << "\"," << newline;
        }
        
        out << indent << indent << indent << "\"expectations\": [" << newline;
        
        bool first_expect = true;
        for (const auto& expect : scenario_result.expect_results) {
            if (!first_expect) {
                out << "," << newline;
            }
            first_expect = false;
            
            out << indent << indent << indent << indent << "{" << newline;
            out << indent << indent << indent << indent << indent 
                << "\"description\": \"" << escape_json(expect.description) << "\"," << newline;
            out << indent << indent << indent << indent << indent 
                << "\"passed\": " << (expect.passed ? "true" : "false") << "," << newline;
            out << indent << indent << indent << indent << indent 
                << "\"packets_matched\": " << expect.packets_matched << "," << newline;
            out << indent << indent << indent << indent << indent 
                << "\"duration_ms\": " << expect.elapsed.count();
            
            if (!expect.passed && !expect.failure_reason.empty()) {
                out << "," << newline;
                out << indent << indent << indent << indent << indent 
                    << "\"failure_reason\": \"" << escape_json(expect.failure_reason) << "\"";
            }
            
            out << newline;
            out << indent << indent << indent << indent << "}";
        }
        
        out << newline << indent << indent << indent << "]" << newline;
        out << indent << indent << "}";
    }
    
    out << newline << indent << "]" << newline;
    out << "}" << newline;
}

void JsonReportGenerator::generate(std::ostream& out,
                                    const ScenarioResult& result,
                                    const ReportOptions& opts) {
    BatchResult batch;
    batch.results.push_back(result);
    batch.total_elapsed = result.total_elapsed;
    batch.passed = result.passed ? 1 : 0;
    batch.failed = result.passed ? 0 : 1;
    generate(out, batch, opts);
}

// =============================================================================
// Text Report Generator
// =============================================================================

void TextReportGenerator::generate(std::ostream& out,
                                    const BatchResult& results,
                                    const ReportOptions& opts) {
    out << "=== " << opts.suite_name << " Test Results ===" << "\n\n";
    
    for (const auto& scenario_result : results.results) {
        std::string status = scenario_result.passed ? "[PASS]" : "[FAIL]";
        out << status << " " << scenario_result.scenario_name 
            << " (" << scenario_result.total_elapsed.count() << "ms)\n";
        
        if (!scenario_result.error_message.empty()) {
            out << "  Error: " << scenario_result.error_message << "\n";
        }
        
        for (const auto& expect : scenario_result.expect_results) {
            std::string expect_status = expect.passed ? "  [PASS]" : "  [FAIL]";
            out << expect_status << " " << expect.description 
                << " (" << expect.packets_matched << " packets, "
                << expect.elapsed.count() << "ms)\n";
            
            if (!expect.passed && !expect.failure_reason.empty()) {
                out << "    Reason: " << expect.failure_reason << "\n";
            }
        }
        
        if (opts.include_pcap_paths && scenario_result.pcap_file) {
            out << "  PCAP: " << *scenario_result.pcap_file << "\n";
        }
        
        out << "\n";
    }
    
    out << "---\n";
    out << "Total: " << results.total() 
        << " | Passed: " << results.passed 
        << " | Failed: " << results.failed 
        << " | Skipped: " << results.skipped << "\n";
    out << "Duration: " << results.total_elapsed.count() << "ms\n";
}

void TextReportGenerator::generate(std::ostream& out,
                                    const ScenarioResult& result,
                                    const ReportOptions& opts) {
    BatchResult batch;
    batch.results.push_back(result);
    batch.total_elapsed = result.total_elapsed;
    batch.passed = result.passed ? 1 : 0;
    batch.failed = result.passed ? 0 : 1;
    generate(out, batch, opts);
}

// =============================================================================
// TAP Report Generator
// =============================================================================

void TapReportGenerator::generate(std::ostream& out,
                                   const BatchResult& results,
                                   const ReportOptions& /*opts*/) {
    std::size_t total_tests = 0;
    for (const auto& scenario : results.results) {
        total_tests += scenario.expect_results.empty() ? 
            1 : scenario.expect_results.size();
    }
    
    out << "TAP version 13\n";
    out << "1.." << total_tests << "\n";
    
    std::size_t test_num = 1;
    for (const auto& scenario_result : results.results) {
        if (scenario_result.expect_results.empty()) {
            // Single test for scenario
            if (scenario_result.passed) {
                out << "ok " << test_num << " - " << scenario_result.scenario_name << "\n";
            } else {
                out << "not ok " << test_num << " - " << scenario_result.scenario_name << "\n";
                if (!scenario_result.error_message.empty()) {
                    out << "  ---\n";
                    out << "  message: " << scenario_result.error_message << "\n";
                    out << "  ...\n";
                }
            }
            ++test_num;
        } else {
            // One test per expectation
            for (const auto& expect : scenario_result.expect_results) {
                std::string name = scenario_result.scenario_name + ": " + expect.description;
                
                if (expect.passed) {
                    out << "ok " << test_num << " - " << name << "\n";
                } else {
                    out << "not ok " << test_num << " - " << name << "\n";
                    out << "  ---\n";
                    out << "  message: " << expect.failure_reason << "\n";
                    out << "  packets_matched: " << expect.packets_matched << "\n";
                    out << "  ...\n";
                }
                ++test_num;
            }
        }
    }
}

void TapReportGenerator::generate(std::ostream& out,
                                   const ScenarioResult& result,
                                   const ReportOptions& opts) {
    BatchResult batch;
    batch.results.push_back(result);
    batch.total_elapsed = result.total_elapsed;
    batch.passed = result.passed ? 1 : 0;
    batch.failed = result.passed ? 0 : 1;
    generate(out, batch, opts);
}

// =============================================================================
// Factory and Convenience Functions
// =============================================================================

std::unique_ptr<IReportGenerator> create_report_generator(ReportFormat format) {
    switch (format) {
        case ReportFormat::JUnitXML:
            return std::make_unique<JUnitXmlReportGenerator>();
        case ReportFormat::JSON:
            return std::make_unique<JsonReportGenerator>();
        case ReportFormat::Text:
            return std::make_unique<TextReportGenerator>();
        case ReportFormat::TAP:
            return std::make_unique<TapReportGenerator>();
    }
    return std::make_unique<TextReportGenerator>();
}

void generate_report(const std::filesystem::path& path,
                     const BatchResult& results,
                     ReportFormat format,
                     const ReportOptions& opts) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open report file: " + path.string());
    }
    
    auto generator = create_report_generator(format);
    generator->generate(file, results, opts);
}

std::string generate_report_string(const BatchResult& results,
                                    ReportFormat format,
                                    const ReportOptions& opts) {
    std::ostringstream ss;
    auto generator = create_report_generator(format);
    generator->generate(ss, results, opts);
    return ss.str();
}

}  // namespace wadjet::scenario

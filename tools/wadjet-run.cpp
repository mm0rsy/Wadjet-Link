/// @file wadjet-run.cpp
/// @brief CLI tool for running Wadjet test scenarios
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// Usage:
///   wadjet-run [options] <scenario-file>...
///   wadjet-run [options] --dir <directory>
///
/// Options:
///   -h, --help              Show this help message
///   -v, --verbose           Enable verbose output
///   -q, --quiet             Suppress all output except errors
///   --dry-run               Parse and validate scenarios without running
///   --stop-on-failure       Stop at first failed expectation
///   --timeout <ms>          Global timeout in milliseconds (default: 60000)
///   --output <file>         Write report to file
///   --format <fmt>          Report format: junit, json, text, tap (default: text)
///   --pcap-dir <dir>        Directory for failure pcap files (default: .)
///   --no-pcap               Don't save pcap files on failure
///   --tag <tag>             Only run scenarios with this tag (repeatable)
///   --dir <directory>       Run all scenarios in directory
///   --list                  List scenarios without running them

#include "wadjet/scenario/parser.hpp"
#include "wadjet/scenario/runner.hpp"
#include "wadjet/scenario/report.hpp"
#include "wadjet/version.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::vector<std::filesystem::path> files;
    std::filesystem::path directory;
    std::filesystem::path output_file;
    wadjet::scenario::ReportFormat report_format{wadjet::scenario::ReportFormat::Text};
    std::string pcap_dir{"."};
    std::vector<std::string> tags;
    std::chrono::milliseconds timeout{60000};
    bool verbose{false};
    bool quiet{false};
    bool dry_run{false};
    bool stop_on_failure{false};
    bool save_pcap{true};
    bool list_only{false};
    bool show_help{false};
    bool show_version{false};
};

void print_usage(const char* program) {
    std::cout << R"(
𓆓 Wadjet-Link Scenario Runner

Usage:
  )" << program << R"( [options] <scenario-file>...
  )" << program << R"( [options] --dir <directory>

Options:
  -h, --help              Show this help message
  -V, --version           Show version information
  -v, --verbose           Enable verbose output
  -q, --quiet             Suppress all output except errors
  --dry-run               Parse and validate scenarios without running
  --stop-on-failure       Stop at first failed expectation
  --timeout <ms>          Global timeout in milliseconds (default: 60000)
  -o, --output <file>     Write report to file
  -f, --format <fmt>      Report format: junit, json, text, tap (default: text)
  --pcap-dir <dir>        Directory for failure pcap files (default: .)
  --no-pcap               Don't save pcap files on failure
  -t, --tag <tag>         Only run scenarios with this tag (repeatable)
  -d, --dir <directory>   Run all scenarios in directory
  -l, --list              List scenarios without running them

Examples:
  # Run a single scenario
  )" << program << R"( test.yaml

  # Run all scenarios in a directory
  )" << program << R"( --dir scenarios/

  # Run scenarios with specific tag and generate JUnit report
  )" << program << R"( --tag smoke -f junit -o results.xml scenarios/

  # Dry run to validate scenario files
  )" << program << R"( --dry-run --dir scenarios/
)";
}

void print_version() {
    std::cout << "wadjet-run version " << wadjet::version_string() << "\n";
    std::cout << "Part of Wadjet-Link - Automotive Ethernet validation framework\n";
}

wadjet::scenario::ReportFormat parse_format(const std::string& str) {
    if (str == "junit" || str == "xml") {
        return wadjet::scenario::ReportFormat::JUnitXML;
    }
    if (str == "json") {
        return wadjet::scenario::ReportFormat::JSON;
    }
    if (str == "tap") {
        return wadjet::scenario::ReportFormat::TAP;
    }
    return wadjet::scenario::ReportFormat::Text;
}

Options parse_args(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
        } else if (arg == "-V" || arg == "--version") {
            opts.show_version = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            opts.quiet = true;
        } else if (arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "--stop-on-failure") {
            opts.stop_on_failure = true;
        } else if (arg == "--no-pcap") {
            opts.save_pcap = false;
        } else if (arg == "-l" || arg == "--list") {
            opts.list_only = true;
        } else if ((arg == "--timeout") && i + 1 < argc) {
            opts.timeout = std::chrono::milliseconds(std::stoll(argv[++i]));
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            opts.output_file = argv[++i];
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            opts.report_format = parse_format(argv[++i]);
        } else if (arg == "--pcap-dir" && i + 1 < argc) {
            opts.pcap_dir = argv[++i];
        } else if ((arg == "-t" || arg == "--tag") && i + 1 < argc) {
            opts.tags.push_back(argv[++i]);
        } else if ((arg == "-d" || arg == "--dir") && i + 1 < argc) {
            opts.directory = argv[++i];
        } else if (arg[0] != '-') {
            opts.files.emplace_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            std::exit(1);
        }
    }
    
    return opts;
}

void list_scenarios(const std::vector<wadjet::scenario::Scenario>& scenarios) {
    std::cout << "Found " << scenarios.size() << " scenario(s):\n\n";
    
    for (const auto& scenario : scenarios) {
        std::cout << "  " << scenario.name << "\n";
        if (!scenario.description.empty()) {
            std::cout << "    Description: " << scenario.description << "\n";
        }
        if (!scenario.tags.empty()) {
            std::cout << "    Tags: ";
            for (std::size_t i = 0; i < scenario.tags.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << scenario.tags[i];
            }
            std::cout << "\n";
        }
        std::cout << "    Steps: " << scenario.steps.size() << "\n";
        std::cout << "    Expectations: " << scenario.get_expectations().size() << "\n";
        std::cout << "\n";
    }
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    Options opts = parse_args(argc, argv);
    
    if (opts.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (opts.show_version) {
        print_version();
        return 0;
    }
    
    // Collect scenario files
    std::vector<std::filesystem::path> scenario_files;
    
    if (!opts.directory.empty()) {
        if (!std::filesystem::exists(opts.directory)) {
            std::cerr << "Error: Directory not found: " << opts.directory << "\n";
            return 1;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(opts.directory)) {
            if (!entry.is_regular_file()) continue;
            
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            
            if (ext == ".yaml" || ext == ".yml" || ext == ".json") {
                scenario_files.push_back(entry.path());
            }
        }
        
        std::sort(scenario_files.begin(), scenario_files.end());
    }
    
    scenario_files.insert(scenario_files.end(), 
                          opts.files.begin(), opts.files.end());
    
    if (scenario_files.empty()) {
        std::cerr << "Error: No scenario files specified.\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }
    
    // Parse all scenarios
    std::vector<wadjet::scenario::Scenario> scenarios;
    bool parse_errors = false;
    
    for (const auto& file : scenario_files) {
        auto result = wadjet::scenario::parse_scenario_file(file);
        if (!result) {
            std::cerr << "Error parsing " << file << ": " 
                      << result.error().to_string() << "\n";
            parse_errors = true;
            continue;
        }
        
        // Tag filtering
        if (!opts.tags.empty()) {
            bool has_tag = false;
            for (const auto& tag : opts.tags) {
                if (std::find(result->tags.begin(), result->tags.end(), tag) 
                    != result->tags.end()) {
                    has_tag = true;
                    break;
                }
            }
            if (!has_tag) {
                if (opts.verbose) {
                    std::cout << "Skipping " << file << " (no matching tags)\n";
                }
                continue;
            }
        }
        
        scenarios.push_back(std::move(*result));
    }
    
    if (parse_errors && scenarios.empty()) {
        return 1;
    }
    
    // List mode
    if (opts.list_only) {
        list_scenarios(scenarios);
        return 0;
    }
    
    // Configure runner
    wadjet::scenario::RunnerOptions runner_opts;
    runner_opts.verbose = opts.verbose;
    runner_opts.dry_run = opts.dry_run;
    runner_opts.stop_on_first_failure = opts.stop_on_failure;
    runner_opts.save_pcap_on_failure = opts.save_pcap;
    runner_opts.pcap_output_dir = opts.pcap_dir;
    runner_opts.global_timeout = opts.timeout;
    
    wadjet::scenario::ScenarioRunner runner(runner_opts);
    
    // Set up callbacks for progress output
    if (!opts.quiet) {
        wadjet::scenario::RunnerCallbacks callbacks;
        
        callbacks.on_scenario_start = [&](const std::string& name) {
            if (opts.verbose) {
                std::cout << "Running: " << name << "\n";
            }
        };
        
        callbacks.on_scenario_end = [&](const wadjet::scenario::ScenarioResult& result) {
            if (!opts.verbose) {
                std::string status = result.passed ? "[PASS]" : "[FAIL]";
                std::cout << status << " " << result.scenario_name << "\n";
            }
        };
        
        callbacks.on_expect_result = [&](const wadjet::scenario::ExpectResult& result) {
            if (opts.verbose) {
                std::string status = result.passed ? "  [PASS]" : "  [FAIL]";
                std::cout << status << " " << result.description << "\n";
            }
        };
        
        callbacks.on_log = [&](const std::string& msg) {
            if (opts.verbose) {
                std::cout << msg << "\n";
            }
        };
        
        runner.set_callbacks(callbacks);
    }
    
    // Run scenarios
    auto results = runner.run_all(scenarios);
    
    // Build batch result
    wadjet::scenario::BatchResult batch;
    batch.results = std::move(results);
    
    for (const auto& r : batch.results) {
        if (r.passed) {
            ++batch.passed;
        } else {
            ++batch.failed;
        }
        batch.total_elapsed += r.total_elapsed;
    }
    
    // Generate report
    if (!opts.output_file.empty()) {
        try {
            wadjet::scenario::generate_report(
                opts.output_file, batch, opts.report_format);
            
            if (!opts.quiet) {
                std::cout << "\nReport written to: " << opts.output_file << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error writing report: " << e.what() << "\n";
        }
    }
    
    // Print summary
    if (!opts.quiet) {
        std::cout << "\n";
        std::cout << "---\n";
        std::cout << "Total: " << batch.total() 
                  << " | Passed: " << batch.passed 
                  << " | Failed: " << batch.failed << "\n";
        std::cout << "Duration: " << batch.total_elapsed.count() << "ms\n";
    }
    
    return batch.failed > 0 ? 1 : 0;
}

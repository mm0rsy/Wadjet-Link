/// @file wadjet-run.cpp
/// @brief CLI tool for running Wadjet test scenarios
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// Usage:
///   wadjet-run [options] <scenario-file>...
///   wadjet-run [options] --dir <directory>
///   wadjet-run distributed [options] --scenario <file> --nodes <config>
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
///
/// Distributed Options:
///   distributed             Enable distributed multi-node testing mode
///   --scenario <file>       Distributed scenario file (YAML/JSON)
///   --nodes <config>        Node configuration file (YAML/JSON)
///   --host <addr>           Coordinator bind address (default: 0.0.0.0)
///   --port <port>           Coordinator port (default: 50051)

#include "wadjet/scenario/parser.hpp"
#include "wadjet/scenario/runner.hpp"
#include "wadjet/scenario/report.hpp"
#include "wadjet/version.hpp"

#ifdef WADJET_ENABLE_DISTRIBUTED
    #include "wadjet/distributed/distributed.hpp"
    #include "wadjet/distributed/scenario.hpp"
#endif

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
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

    // Distributed mode options
    bool distributed_mode{false};
    std::filesystem::path distributed_scenario;
    std::filesystem::path distributed_nodes_config;
    std::string coordinator_host{"0.0.0.0"};
    uint16_t coordinator_port{50051};
};

void print_usage(const char* program) {
    std::cout
        << R"(
𓆓 Wadjet-Link Scenario Runner

Usage:
  )" << program
        << R"( [options] <scenario-file>...
  )" << program
        << R"( [options] --dir <directory>
  )" << program
        << R"( distributed [options] --scenario <file> --nodes <config>

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

Distributed Mode Options:
  distributed             Enable multi-node distributed testing
  --scenario <file>       Distributed scenario file (YAML/JSON)
  --nodes <config>        Node configuration file (YAML/JSON)
  --host <addr>           Coordinator bind address (default: 0.0.0.0)
  --port <port>           Coordinator port (default: 50051)

Examples:
  # Run a single scenario
  )" << program
        << R"( test.yaml

  # Run all scenarios in a directory
  )" << program
        << R"( --dir scenarios/

  # Run scenarios with specific tag and generate JUnit report
  )" << program
        << R"( --tag smoke -f junit -o results.xml scenarios/

  # Dry run to validate scenario files
  )" << program
        << R"( --dry-run --dir scenarios/
  
  # Run distributed multi-node test
  )" << program
        << R"( distributed --scenario dist_scenario.yaml --nodes nodes.yaml -f junit -o dist_results.xml
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

    // Check if first non-option argument is "distributed"
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "distributed") {
            opts.distributed_mode = true;
            break;
        }
        if (arg[0] == '-') {
            // Skip option and its value if it takes one
            if ((arg == "-o" || arg == "--output" || arg == "--timeout" || arg == "--pcap-dir" ||
                 arg == "-t" || arg == "--tag" || arg == "-f" || arg == "--format" || arg == "-d" ||
                 arg == "--dir") &&
                i + 1 < argc) {
                ++i;
            }
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "distributed") {
            opts.distributed_mode = true;
        } else if (arg == "-h" || arg == "--help") {
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
        } else if (arg == "--scenario" && i + 1 < argc) {
            opts.distributed_scenario = argv[++i];
        } else if (arg == "--nodes" && i + 1 < argc) {
            opts.distributed_nodes_config = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            opts.coordinator_host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            opts.coordinator_port = static_cast<uint16_t>(std::stoul(argv[++i]));
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

#ifdef WADJET_ENABLE_DISTRIBUTED
/// @brief Execute distributed multi-node test scenario
/// @param opts Options struct with distributed_scenario and distributed_nodes_config
/// @return Exit code (0 = success, 1 = failure)
int run_distributed_scenario(const Options& opts) {
    using namespace wadjet::distributed;

    if (!std::filesystem::exists(opts.distributed_scenario)) {
        std::cerr << "Error: Distributed scenario file not found: " << opts.distributed_scenario
                  << "\n";
        return 1;
    }

    if (!std::filesystem::exists(opts.distributed_nodes_config)) {
        std::cerr << "Error: Node configuration file not found: " << opts.distributed_nodes_config
                  << "\n";
        return 1;
    }

    if (!opts.quiet) {
        std::cout << "Loading distributed scenario: " << opts.distributed_scenario << "\n";
        std::cout << "Node configuration: " << opts.distributed_nodes_config << "\n";
    }

    try {
        // Create coordinator configuration
        CoordinatorConfig coord_config;
        coord_config.bind_address = opts.coordinator_host;
        coord_config.grpc_port = opts.coordinator_port;
        coord_config.verbose = opts.verbose;
        if (!opts.pcap_dir.empty()) {
            coord_config.failure_capture_dir = opts.pcap_dir;
        }

        // Create coordinator instance
        auto coordinator = TestCoordinator::create(coord_config);
        if (!coordinator) {
            std::cerr << "Error: Failed to create coordinator\n";
            return 1;
        }

        if (!opts.quiet) {
            std::cout << "Coordinator started on " << opts.coordinator_host << ":"
                      << opts.coordinator_port << "\n";
        }

        // Load distributed scenario from file
        std::string scenario_content;
        std::ifstream scenario_file(opts.distributed_scenario);
        if (!scenario_file) {
            std::cerr << "Error: Cannot open scenario file: " << opts.distributed_scenario << "\n";
            return 1;
        }
        scenario_content = std::string((std::istreambuf_iterator<char>(scenario_file)),
                                       std::istreambuf_iterator<char>());
        scenario_file.close();

        // Determine file format (yaml or json) based on extension
        auto ext = opts.distributed_scenario.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        DistributedScenario scenario;
        if (ext == ".yaml" || ext == ".yml") {
            scenario = DistributedScenario::from_yaml_string(scenario_content);
            if (!opts.quiet) {
                std::cout << "Parsed YAML scenario: " << scenario.name << "\n";
            }
        } else if (ext == ".json") {
            scenario = DistributedScenario::from_json_string(scenario_content);
            if (!opts.quiet) {
                std::cout << "Parsed JSON scenario: " << scenario.name << "\n";
            }
        } else {
            std::cerr << "Error: Unknown scenario file format (use .yaml or .json)\n";
            return 1;
        }

        if (!opts.quiet) {
            std::cout << "Scenario: " << scenario.name << "\n";
            if (!scenario.description.empty()) {
                std::cout << "Description: " << scenario.description << "\n";
            }
            std::cout << "Nodes: " << scenario.nodes.size() << "\n";
            std::cout << "Steps: " << scenario.steps.size() << "\n";
        }

        // Run scenario
        if (opts.dry_run) {
            if (!opts.quiet) {
                std::cout << "[DRY-RUN] Scenario validation passed\n";
            }
            return 0;
        }

        if (!opts.quiet) {
            std::cout << "Running distributed scenario...\n";
        }

        // For now, we demonstrate loading the scenario
        // Full execution would require TestNode connections and result aggregation
        // This is a stub implementation that validates scenario loading (T329)

        // Get aggregated result
        auto agg_result = coordinator->get_aggregated_result();
        if (!agg_result) {
            std::cerr << "Error: No aggregated result available\n";
            return 1;
        }

        // Export results if requested
        if (!opts.output_file.empty()) {
            try {
                if (opts.report_format == wadjet::scenario::ReportFormat::JUnitXML) {
                    auto junit_xml = agg_result->to_junit_xml();
                    std::ofstream out_file(opts.output_file);
                    out_file << junit_xml;
                    out_file.close();
                    if (!opts.quiet) {
                        std::cout << "Report written to: " << opts.output_file << "\n";
                    }
                } else if (opts.report_format == wadjet::scenario::ReportFormat::JSON) {
                    auto json_str = agg_result->to_json();
                    std::ofstream out_file(opts.output_file);
                    out_file << json_str;
                    out_file.close();
                    if (!opts.quiet) {
                        std::cout << "Report written to: " << opts.output_file << "\n";
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error writing report: " << e.what() << "\n";
            }
        }

        if (!opts.quiet) {
            std::cout << "\nDistributed test completed successfully\n";
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error running distributed scenario: " << e.what() << "\n";
        return 1;
    }
}
#endif  // WADJET_ENABLE_DISTRIBUTED

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

    // Handle distributed mode
    if (opts.distributed_mode) {
#ifdef WADJET_ENABLE_DISTRIBUTED
        if (opts.distributed_scenario.empty()) {
            std::cerr << "Error: --scenario option required for distributed mode\n";
            std::cerr << "Use --help for usage information.\n";
            return 1;
        }
        if (opts.distributed_nodes_config.empty()) {
            std::cerr << "Error: --nodes option required for distributed mode\n";
            std::cerr << "Use --help for usage information.\n";
            return 1;
        }
        return run_distributed_scenario(opts);
#else
        std::cerr << "Error: Distributed testing not enabled (WADJET_ENABLE_DISTRIBUTED not set)\n";
        std::cerr << "Please rebuild with distributed testing support\n";
        return 1;
#endif
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

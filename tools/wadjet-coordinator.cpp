/**
 * @file tools/wadjet-coordinator.cpp
 * @brief Command-line tool for Wadjet-Link distributed test coordinator
 *
 * T121-T124: CLI tool for:
 * - T121: Initialization and startup of test coordinator
 * - T122: Loading node configuration from YAML/JSON files
 * - T123: Loading and executing test scenarios
 * - T124: Output directory management and JUnit XML report generation
 *
 * Usage:
 *   wadjet-coordinator [OPTIONS]
 *
 *   OPTIONS:
 *     --help                Show help message
 *     --version             Show version
 *     --config FILE         Load node configuration from YAML/JSON file
 *     --scenario FILE       Load test scenario from YAML/JSON file
 *     --output-dir DIR      Directory for test outputs (PCAP, reports)
 *     --junit-report FILE   Save JUnit XML report to file
 *     --bind ADDR           Bind address (default: 0.0.0.0)
 *     --port PORT           gRPC port (default: 50051)
 *     --timeout MS          Node heartbeat timeout (default: 5000)
 *     --verbose             Enable verbose logging
 *     --debug               Enable debug logging
 */

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <chrono>
#include <thread>
#include <cstring>

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/scenario.hpp"
#include "wadjet/distributed/result_aggregation.hpp"
#include "wadjet/common/logger.hpp"

namespace fs = std::filesystem;
using namespace wadjet::distributed;
using wadjet::logger::Logger;

/**
 * @brief Command-line options for coordinator tool
 */
struct CoordinatorOptions {
    std::string version = "1.0.0";
    bool show_help = false;
    bool show_version = false;
    std::string config_file;
    std::string scenario_file;
    std::string output_dir = "./wadjet_output";
    std::string junit_report;
    std::string bind_address = "0.0.0.0";
    uint16_t port = 50051;
    uint32_t heartbeat_timeout_ms = 5000;
    bool verbose = false;
    bool debug = false;
};

/**
 * @brief Print usage information
 */
void print_help(const char* program_name) {
    std::cout << "Wadjet-Link Distributed Test Coordinator\n\n"
              << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "OPTIONS:\n"
              << "  --help                Show this help message\n"
              << "  --version             Show version information\n"
              << "  --config FILE         Load node configuration from YAML/JSON file\n"
              << "  --scenario FILE       Load and run test scenario from YAML/JSON file\n"
              << "  --output-dir DIR      Directory for outputs (PCAP, logs, reports)\n"
              << "                        (default: ./wadjet_output)\n"
              << "  --junit-report FILE   Save JUnit XML test report to file\n"
              << "  --bind ADDR           Bind address for gRPC server\n"
              << "                        (default: 0.0.0.0)\n"
              << "  --port PORT           gRPC server port (default: 50051)\n"
              << "  --timeout MS          Node heartbeat timeout in milliseconds\n"
              << "                        (default: 5000)\n"
              << "  --verbose             Enable verbose logging\n"
              << "  --debug               Enable debug logging\n\n"
              << "EXAMPLES:\n"
              << "  # Start coordinator and wait for nodes\n"
              << "  wadjet-coordinator --bind 192.168.1.100 --port 50051\n\n"
              << "  # Load node config and run scenario\n"
              << "  wadjet-coordinator --config nodes.yaml --scenario test.yaml \\\n"
              << "    --output-dir ./results --junit-report results.xml\n\n"
              << "  # Enable debug logging\n"
              << "  wadjet-coordinator --scenario test.yaml --debug\n";
}

/**
 * @brief Print version information
 */
void print_version() {
    std::cout << "wadjet-coordinator version 1.0.0\n"
              << "Wadjet-Link Distributed Testing Framework\n"
              << "License: Polyform Noncommercial 1.0.0\n";
}

/**
 * @brief Parse command-line arguments
 */
CoordinatorOptions parse_arguments(int argc, char* argv[]) {
    CoordinatorOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
        } else if (arg == "--version" || arg == "-v") {
            opts.show_version = true;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                opts.config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path\n";
                exit(1);
            }
        } else if (arg == "--scenario") {
            if (i + 1 < argc) {
                opts.scenario_file = argv[++i];
            } else {
                std::cerr << "Error: --scenario requires a file path\n";
                exit(1);
            }
        } else if (arg == "--output-dir") {
            if (i + 1 < argc) {
                opts.output_dir = argv[++i];
            } else {
                std::cerr << "Error: --output-dir requires a directory path\n";
                exit(1);
            }
        } else if (arg == "--junit-report") {
            if (i + 1 < argc) {
                opts.junit_report = argv[++i];
            } else {
                std::cerr << "Error: --junit-report requires a file path\n";
                exit(1);
            }
        } else if (arg == "--bind") {
            if (i + 1 < argc) {
                opts.bind_address = argv[++i];
            } else {
                std::cerr << "Error: --bind requires an address\n";
                exit(1);
            }
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                try {
                    opts.port = static_cast<uint16_t>(std::stoul(argv[++i]));
                } catch (const std::exception& e) {
                    std::cerr << "Error: --port requires a valid port number: " << e.what() << "\n";
                    exit(1);
                }
            } else {
                std::cerr << "Error: --port requires a port number\n";
                exit(1);
            }
        } else if (arg == "--timeout") {
            if (i + 1 < argc) {
                try {
                    opts.heartbeat_timeout_ms = static_cast<uint32_t>(std::stoul(argv[++i]));
                } catch (const std::exception& e) {
                    std::cerr << "Error: --timeout requires a valid number: " << e.what() << "\n";
                    exit(1);
                }
            } else {
                std::cerr << "Error: --timeout requires a time in milliseconds\n";
                exit(1);
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--debug") {
            opts.debug = true;
            opts.verbose = true;
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information\n";
            exit(1);
        }
    }
    
    return opts;
}

/**
 * @brief Create and configure the test coordinator
 *
 * T121: Initialize coordinator with configuration
 */
std::unique_ptr<TestCoordinator> create_coordinator(const CoordinatorOptions& opts) {
    CoordinatorConfig config;
    config.bind_address = opts.bind_address;
    config.grpc_port = opts.port;
    config.heartbeat_timeout = std::chrono::milliseconds(opts.heartbeat_timeout_ms);
    
    // Load from config file if provided
    if (!opts.config_file.empty()) {
        if (!fs::exists(opts.config_file)) {
            std::cerr << "Error: Config file not found: " << opts.config_file << "\n";
            return nullptr;
        }
        config.config_path = opts.config_file;
    }
    
    auto coordinator_result = TestCoordinator::create(config);
    if (!coordinator_result) {
        std::cerr << "Error: Failed to create coordinator: " << coordinator_result.error() << "\n";
        return nullptr;
    }
    
    return std::move(coordinator_result.value());
}

/**
 * @brief Load and run a test scenario
 *
 * T123: Execute scenario on all registered nodes
 */
bool run_scenario(TestCoordinator* coordinator, const CoordinatorOptions& opts) {
    if (!coordinator) {
        std::cerr << "Error: Coordinator is null\n";
        return false;
    }
    
    // Load scenario from file
    auto scenario_result = DistributedScenario::load_from_file(opts.scenario_file);
    if (!scenario_result) {
        std::cerr << "Error: Failed to load scenario: " << scenario_result.error() << "\n";
        return false;
    }
    
    auto scenario = std::move(scenario_result.value());
    std::cout << "Loaded scenario: " << scenario->name() << "\n";
    std::cout << "Nodes: " << scenario->node_count() << "\n";
    std::cout << "Steps: " << scenario->step_count() << "\n";
    
    // Run the scenario
    auto run_result = coordinator->run_scenario(scenario);
    if (!run_result) {
        std::cerr << "Error: Scenario execution failed: " << run_result.error() << "\n";
        return false;
    }
    
    return true;
}

/**
 * @brief Save test results
 *
 * T124: Generate JUnit XML and other reports
 */
bool save_results(TestCoordinator* coordinator, const CoordinatorOptions& opts) {
    if (!coordinator) {
        std::cerr << "Error: Coordinator is null\n";
        return false;
    }
    
    // Create output directory if it doesn't exist
    if (!fs::exists(opts.output_dir)) {
        std::error_code ec;
        fs::create_directories(opts.output_dir, ec);
        if (ec) {
            std::cerr << "Error: Failed to create output directory: " << ec.message() << "\n";
            return false;
        }
    }
    
    // Get aggregated results
    auto results = coordinator->get_results();
    if (!results) {
        std::cerr << "Warning: No test results to save\n";
        return true;  // Not a critical error
    }
    
    // Save JUnit XML report if requested
    if (!opts.junit_report.empty()) {
        fs::path report_path = fs::path(opts.output_dir) / opts.junit_report;
        auto save_result = coordinator->export_junit(report_path.string());
        if (!save_result) {
            std::cerr << "Error: Failed to save JUnit report: " << save_result.error() << "\n";
            return false;
        }
        std::cout << "Saved JUnit report to: " << report_path.string() << "\n";
    }
    
    // Save JSON results
    fs::path json_path = fs::path(opts.output_dir) / "results.json";
    auto json_result = results->to_json();
    if (json_result) {
        std::ofstream json_file(json_path);
        json_file << json_result.value();
        std::cout << "Saved JSON results to: " << json_path.string() << "\n";
    }
    
    // Save HTML report
    fs::path html_path = fs::path(opts.output_dir) / "report.html";
    auto html_result = results->to_html_report();
    if (html_result) {
        std::ofstream html_file(html_path);
        html_file << html_result.value();
        std::cout << "Saved HTML report to: " << html_path.string() << "\n";
    }
    
    return true;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Parse command-line arguments
    auto opts = parse_arguments(argc, argv);
    
    // Handle help
    if (opts.show_help) {
        print_help(argv[0]);
        return 0;
    }
    
    // Handle version
    if (opts.show_version) {
        print_version();
        return 0;
    }
    
    // Configure logging
    if (opts.debug) {
        Logger::set_level(Logger::Level::DEBUG);
    } else if (opts.verbose) {
        Logger::set_level(Logger::Level::INFO);
    } else {
        Logger::set_level(Logger::Level::WARN);
    }
    
    std::cout << "Wadjet-Link Distributed Test Coordinator\n"
              << "========================================\n\n";
    
    // T121: Create coordinator
    std::cout << "Starting coordinator on " << opts.bind_address << ":" << opts.port << "...\n";
    auto coordinator = create_coordinator(opts);
    if (!coordinator) {
        std::cerr << "Fatal: Could not create coordinator\n";
        return 1;
    }
    
    // Start the coordinator
    auto start_result = coordinator->start();
    if (!start_result) {
        std::cerr << "Fatal: Failed to start coordinator: " << start_result.error() << "\n";
        return 1;
    }
    
    std::cout << "Coordinator started successfully\n";
    std::cout << "Listening for node connections...\n\n";
    
    // T123: Run scenario if provided
    if (!opts.scenario_file.empty()) {
        if (!fs::exists(opts.scenario_file)) {
            std::cerr << "Error: Scenario file not found: " << opts.scenario_file << "\n";
            coordinator->stop();
            return 1;
        }
        
        std::cout << "Running scenario: " << opts.scenario_file << "\n\n";
        
        if (!run_scenario(coordinator.get(), opts)) {
            std::cerr << "Scenario execution failed\n";
            coordinator->stop();
            return 1;
        }
        
        std::cout << "\nScenario execution completed\n";
    } else {
        // Interactive mode: wait for external test harness
        std::cout << "\nCoordinator ready. Connect nodes and trigger tests externally.\n";
        std::cout << "Press Ctrl+C to stop.\n";
    }
    
    // T124: Save results
    if (!opts.scenario_file.empty() || !opts.junit_report.empty()) {
        std::cout << "\nGenerating test reports...\n";
        if (!save_results(coordinator.get(), opts)) {
            std::cerr << "Failed to save results\n";
            coordinator->stop();
            return 1;
        }
    }
    
    // Stop the coordinator
    std::cout << "\nShutting down coordinator...\n";
    coordinator->stop();
    
    std::cout << "Coordinator stopped\n";
    return 0;
}

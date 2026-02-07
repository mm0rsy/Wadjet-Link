/**
 * @file tools/wadjet-node.cpp
 * @brief Command-line tool for Wadjet-Link distributed test node
 *
 * T125-T127: CLI tool for:
 * - T125: Initialization of test node and coordinator connection
 * - T126: Loading node configuration from YAML/JSON files
 * - T127: Clock synchronization verification before test execution
 *
 * Usage:
 *   wadjet-node [OPTIONS]
 *
 *   OPTIONS:
 *     --help                Show help message
 *     --version             Show version
 *     --node-id ID          Unique node identifier (required)
 *     --config FILE         Load node configuration from YAML/JSON file
 *     --coordinator ADDR    Coordinator address (default: localhost)
 *     --coordinator-port P  Coordinator port (default: 50051)
 *     --check-clock         Verify clock sync before running tests
 *     --interfaces IFACE    Network interfaces for capture (comma-separated)
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
#include <sstream>

#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/timestamp_normalizer.hpp"
#include "wadjet/distributed/types.hpp"
#include "wadjet/common/logger.hpp"

namespace fs = std::filesystem;
using namespace wadjet::distributed;
using wadjet::logger::Logger;

/**
 * @brief Command-line options for node tool
 */
struct NodeOptions {
    std::string version = "1.0.0";
    bool show_help = false;
    bool show_version = false;
    std::string node_id;
    std::string config_file;
    std::string coordinator_address = "localhost";
    uint16_t coordinator_port = 50051;
    std::vector<std::string> capture_interfaces;
    bool check_clock = false;
    bool verbose = false;
    bool debug = false;
};

/**
 * @brief Print usage information
 */
void print_help(const char* program_name) {
    std::cout << "Wadjet-Link Distributed Test Node\n\n"
              << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "OPTIONS:\n"
              << "  --help                Show this help message\n"
              << "  --version             Show version information\n"
              << "  --node-id ID          Unique node identifier (required)\n"
              << "  --config FILE         Load node configuration from YAML/JSON file\n"
              << "  --coordinator ADDR    Coordinator address (default: localhost)\n"
              << "  --coordinator-port P  Coordinator gRPC port (default: 50051)\n"
              << "  --check-clock         Verify clock synchronization before tests\n"
              << "  --interfaces IFACE    Network interfaces for packet capture\n"
              << "                        (comma-separated, e.g., eth0,eth1)\n"
              << "  --verbose             Enable verbose logging\n"
              << "  --debug               Enable debug logging\n\n"
              << "EXAMPLES:\n"
              << "  # Start node with basic config\n"
              << "  wadjet-node --node-id node1 --coordinator 192.168.1.100\n\n"
              << "  # Load from config file with clock verification\n"
              << "  wadjet-node --node-id node1 --config node1.yaml --check-clock\n\n"
              << "  # Specify capture interfaces explicitly\n"
              << "  wadjet-node --node-id node1 --interfaces eth0,eth1 --debug\n";
}

/**
 * @brief Print version information
 */
void print_version() {
    std::cout << "wadjet-node version 1.0.0\n"
              << "Wadjet-Link Distributed Testing Framework\n"
              << "License: Polyform Noncommercial 1.0.0\n";
}

/**
 * @brief Split comma-separated string into vector
 */
std::vector<std::string> split_interfaces(const std::string& interfaces_str) {
    std::vector<std::string> interfaces;
    std::stringstream ss(interfaces_str);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) {
            interfaces.push_back(item);
        }
    }
    
    return interfaces;
}

/**
 * @brief Parse command-line arguments
 */
NodeOptions parse_arguments(int argc, char* argv[]) {
    NodeOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
        } else if (arg == "--version" || arg == "-v") {
            opts.show_version = true;
        } else if (arg == "--node-id") {
            if (i + 1 < argc) {
                opts.node_id = argv[++i];
            } else {
                std::cerr << "Error: --node-id requires a node identifier\n";
                exit(1);
            }
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                opts.config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path\n";
                exit(1);
            }
        } else if (arg == "--coordinator") {
            if (i + 1 < argc) {
                opts.coordinator_address = argv[++i];
            } else {
                std::cerr << "Error: --coordinator requires an address\n";
                exit(1);
            }
        } else if (arg == "--coordinator-port") {
            if (i + 1 < argc) {
                try {
                    opts.coordinator_port = static_cast<uint16_t>(std::stoul(argv[++i]));
                } catch (const std::exception& e) {
                    std::cerr << "Error: --coordinator-port requires a valid port: " << e.what() << "\n";
                    exit(1);
                }
            } else {
                std::cerr << "Error: --coordinator-port requires a port number\n";
                exit(1);
            }
        } else if (arg == "--check-clock") {
            opts.check_clock = true;
        } else if (arg == "--interfaces") {
            if (i + 1 < argc) {
                opts.capture_interfaces = split_interfaces(argv[++i]);
            } else {
                std::cerr << "Error: --interfaces requires interface names\n";
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
 * @brief Create and configure the test node
 *
 * T125: Initialize node with configuration
 */
std::unique_ptr<TestNode> create_node(const NodeOptions& opts) {
    NodeConfig config;
    config.node_id = opts.node_id;
    config.coordinator_address = opts.coordinator_address;
    config.coordinator_port = opts.coordinator_port;
    
    if (!opts.capture_interfaces.empty()) {
        config.capture_interfaces = opts.capture_interfaces;
    }
    
    // Load from config file if provided (T126)
    if (!opts.config_file.empty()) {
        if (!fs::exists(opts.config_file)) {
            std::cerr << "Error: Config file not found: " << opts.config_file << "\n";
            return nullptr;
        }
        config.config_path = opts.config_file;
    }
    
    auto node_result = TestNode::create(config);
    if (!node_result) {
        std::cerr << "Error: Failed to create node: " << node_result.error() << "\n";
        return nullptr;
    }
    
    return std::move(node_result.value());
}

/**
 * @brief Check clock synchronization status
 *
 * T127: Verify clock sync before test execution
 */
bool check_clock_synchronization(TestNode* node) {
    if (!node) {
        std::cerr << "Error: Node is null\n";
        return false;
    }
    
    std::cout << "Checking clock synchronization...\n";
    
    auto clock_status = node->report_clock_status();
    
    std::cout << "Clock status:\n";
    std::cout << "  Synchronized: " << (clock_status.is_synchronized ? "Yes" : "No") << "\n";
    
    switch (clock_status.method) {
        case ClockSyncMethod::GPTP:
            std::cout << "  Method: gPTP (IEEE 802.1AS)\n";
            break;
        case ClockSyncMethod::NTP:
            std::cout << "  Method: NTP\n";
            break;
        case ClockSyncMethod::None:
            std::cout << "  Method: None (system clock)\n";
            break;
        case ClockSyncMethod::Unknown:
            std::cout << "  Method: Unknown\n";
            break;
    }
    
    if (clock_status.is_synchronized) {
        std::cout << "  Offset: " << clock_status.estimated_offset_ns << " ns\n";
        std::cout << "  Max Error: " << clock_status.max_error_ns << " ns\n";
        if (!clock_status.grandmaster_id.empty()) {
            std::cout << "  Grandmaster: " << clock_status.grandmaster_id << "\n";
        }
    }
    
    if (!clock_status.is_synchronized) {
        std::cerr << "\nWarning: Clock is not synchronized!\n"
                  << "Distributed test assertions may have inconsistent timestamps.\n"
                  << "Recommendation: Ensure gPTP or NTP is configured on this node.\n";
        return false;
    }
    
    return true;
}

/**
 * @brief Report node health status
 */
void report_node_health(TestNode* node) {
    if (!node) {
        return;
    }
    
    auto health = node->report_health();
    
    std::cout << "Node health: ";
    switch (health) {
        case NodeHealthStatus::Healthy:
            std::cout << "HEALTHY\n";
            break;
        case NodeHealthStatus::Degraded:
            std::cout << "DEGRADED\n";
            break;
        case NodeHealthStatus::Unhealthy:
            std::cout << "UNHEALTHY\n";
            break;
        case NodeHealthStatus::Disconnected:
            std::cout << "DISCONNECTED\n";
            break;
        default:
            std::cout << "UNKNOWN\n";
            break;
    }
}

/**
 * @brief Wait for coordinator to send commands
 */
void run_command_loop(TestNode* node) {
    if (!node) {
        std::cerr << "Error: Node is null\n";
        return;
    }
    
    std::cout << "\nNode is ready for distributed tests.\n";
    std::cout << "Waiting for test commands from coordinator...\n";
    std::cout << "Press Ctrl+C to stop.\n\n";
    
    // Simple keep-alive loop
    // In a real implementation, this would process gRPC commands from coordinator
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Check if coordinator is still online
        if (!node->is_coordinator_online()) {
            std::cout << "Coordinator is offline. Exiting...\n";
            break;
        }
    }
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
    
    // Validate required arguments
    if (opts.node_id.empty()) {
        std::cerr << "Error: --node-id is required\n";
        std::cerr << "Use --help for usage information\n";
        return 1;
    }
    
    // Configure logging
    if (opts.debug) {
        Logger::set_level(Logger::Level::DEBUG);
    } else if (opts.verbose) {
        Logger::set_level(Logger::Level::INFO);
    } else {
        Logger::set_level(Logger::Level::WARN);
    }
    
    std::cout << "Wadjet-Link Distributed Test Node\n"
              << "==================================\n\n";
    
    std::cout << "Node ID: " << opts.node_id << "\n";
    std::cout << "Coordinator: " << opts.coordinator_address << ":"
              << opts.coordinator_port << "\n\n";
    
    // T125: Create node
    std::cout << "Initializing node...\n";
    auto node = create_node(opts);
    if (!node) {
        std::cerr << "Fatal: Could not create node\n";
        return 1;
    }
    
    // Connect to coordinator
    std::cout << "Connecting to coordinator...\n";
    auto connect_result = node->connect();
    if (!connect_result) {
        std::cerr << "Fatal: Failed to connect to coordinator: " << connect_result.error() << "\n";
        return 1;
    }
    
    std::cout << "Connected to coordinator\n\n";
    
    // T127: Check clock synchronization if requested
    if (opts.check_clock) {
        std::cout << "\n";
        if (!check_clock_synchronization(node.get())) {
            std::cerr << "\nFatal: Clock synchronization check failed\n";
            node->disconnect();
            return 1;
        }
        std::cout << "\n";
    }
    
    // Report node health
    report_node_health(node.get());
    
    // Run command loop
    std::cout << "\n";
    run_command_loop(node.get());
    
    // Disconnect from coordinator
    std::cout << "Disconnecting from coordinator...\n";
    node->disconnect();
    
    std::cout << "Node stopped\n";
    return 0;
}

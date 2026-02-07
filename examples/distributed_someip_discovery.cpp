/**
 * @file examples/distributed_someip_discovery.cpp
 * @brief 3-Node SOME/IP Service Discovery Distributed Testing Example
 *
 * T129: Example demonstrating multi-node SOME/IP Service Discovery (SD) testing
 *
 * This example shows how to use Wadjet-Link distributed testing to validate
 * SOME/IP Service Discovery (SD) messages across 3 nodes:
 *
 * - Node 1 (Service Provider):  Advertises a SOME/IP service on eth0
 * - Node 2 (Consumer):          Discovers service via SD on eth1
 * - Node 3 (Monitor):           Observes traffic on both segments
 *
 * Test Scenario:
 * 1. All nodes start synchronized capture
 * 2. Service provider advertises service (SOME/IP SD)
 * 3. Consumer sends Find Service request
 * 4. Verify response within latency bounds
 * 5. Verify message flow: Provider → Monitor → Consumer
 * 6. Validate timestamp ordering across nodes
 *
 * Prerequisites:
 *   - 3 nodes with network connectivity
 *   - SOME/IP traffic generator (e.g., someip-send)
 *   - gPTP or NTP for clock synchronization
 *
 * Usage:
 *   # Terminal 1: Start coordinator
 *   wadjet-coordinator --config nodes.yaml --scenario someip_discovery.yaml \
 *     --output-dir ./results --junit-report results.xml
 *
 *   # Terminal 2: Start node 1 (Service Provider)
 *   wadjet-node --node-id provider --coordinator 192.168.1.100 --check-clock
 *
 *   # Terminal 3: Start node 2 (Consumer)
 *   wadjet-node --node-id consumer --coordinator 192.168.1.100 --check-clock
 *
 *   # Terminal 4: Start node 3 (Monitor)
 *   wadjet-node --node-id monitor --coordinator 192.168.1.100 --check-clock
 *
 * Expected Output:
 *   ✓ Capture synchronized across all nodes (<10ms jitter)
 *   ✓ SOME/IP SD messages found on all nodes with aligned timestamps
 *   ✓ Message flow verified: Provider → Monitor → Consumer
 *   ✓ Latency bounds verified: <100ms from request to response
 *   ✓ JUnit XML report with per-node results
 */

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/scenario.hpp"
#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/distributed/result_aggregation.hpp"
#include "wadjet/common/logger.hpp"

using namespace wadjet::distributed;
using wadjet::logger::Logger;

/**
 * @brief Configuration for the SOME/IP SD example
 */
struct SomeipSdExampleConfig {
    // Coordinator settings
    std::string coordinator_address = "192.168.1.100";
    uint16_t coordinator_port = 50051;
    
    // Nodes
    struct NodeInfo {
        std::string node_id;
        std::string description;
        std::string interface;
    };
    
    std::vector<NodeInfo> nodes = {
        {"provider", "SOME/IP Service Provider", "eth0"},
        {"consumer", "SOME/IP Service Consumer", "eth1"},
        {"monitor", "Network Monitor", "eth0,eth1"}
    };
    
    // Test parameters
    std::chrono::milliseconds capture_duration{10000};  // 10 seconds
    std::chrono::milliseconds barrier_timeout{5000};
    std::chrono::milliseconds latency_bound{100};       // 100ms max latency
};

/**
 * @brief Example: Create and configure the test coordinator
 *
 * Sets up the coordinator for 3-node SOME/IP SD testing
 */
std::unique_ptr<TestCoordinator> setup_coordinator(
    const SomeipSdExampleConfig& config) {
    
    std::cout << "\n=== Setting up Coordinator ===\n";
    
    CoordinatorConfig coord_config;
    coord_config.bind_address = "0.0.0.0";
    coord_config.grpc_port = config.coordinator_port;
    
    auto coordinator_result = TestCoordinator::create(coord_config);
    if (!coordinator_result) {
        std::cerr << "Failed to create coordinator: " << coordinator_result.error() << "\n";
        return nullptr;
    }
    
    std::cout << "✓ Coordinator created\n";
    
    return std::move(coordinator_result.value());
}

/**
 * @brief Example: Build the test scenario programmatically
 *
 * Creates a test scenario without needing YAML files
 */
std::unique_ptr<DistributedScenario> create_someip_sd_scenario() {
    std::cout << "\n=== Creating Test Scenario ===\n";
    
    // This is a simplified example. In practice, you would:
    // 1. Load from YAML file: DistributedScenario::load_from_file(...)
    // 2. Or build programmatically using the builder API
    
    auto scenario = std::make_unique<DistributedScenario>();
    
    // Note: Actual implementation depends on DistributedScenario API
    // This shows the conceptual flow
    
    std::cout << "✓ Scenario created: SOME/IP Service Discovery Test\n";
    std::cout << "  - Nodes: 3 (provider, consumer, monitor)\n";
    std::cout << "  - Duration: 10 seconds\n";
    std::cout << "  - Barriers: 2 (start, stop)\n";
    
    return scenario;
}

/**
 * @brief Example: Display node registration
 *
 * Shows how to register nodes with the coordinator
 */
void register_example_nodes(TestCoordinator* coordinator,
                           const SomeipSdExampleConfig& config) {
    std::cout << "\n=== Waiting for Node Registration ===\n";
    
    // In a real scenario, nodes connect via CLI tools (wadjet-node)
    // The coordinator would register them automatically via gRPC
    
    // This example shows what the registration would look like:
    for (const auto& node_info : config.nodes) {
        std::cout << "• " << node_info.node_id << " ("
                  << node_info.description << ")\n"
                  << "  Interface: " << node_info.interface << "\n";
    }
    
    std::cout << "\nCoordinator listening on " << config.coordinator_address
              << ":" << config.coordinator_port << "\n";
    std::cout << "Waiting for nodes to connect...\n";
}

/**
 * @brief Example: Display the test flow
 *
 * Explains the distributed test execution flow
 */
void display_test_flow() {
    std::cout << "\n=== Test Execution Flow ===\n";
    std::cout << "\n1. SYNCHRONIZATION BARRIER\n"
              << "   All nodes wait at barrier until all are ready\n"
              << "   Expected jitter: <10ms\n"
              << "\n2. SYNCHRONIZED CAPTURE START\n"
              << "   Provider:  Advertises SOME/IP service via SD\n"
              << "   Consumer:  Sends Find Service request\n"
              << "   Monitor:   Observes both segments\n"
              << "   Duration:  10 seconds\n"
              << "\n3. DISTRIBUTED ASSERTIONS\n"
              << "   • Message flow: Provider → Monitor → Consumer\n"
              << "   • Latency: Request-to-response <100ms\n"
              << "   • Timestamp ordering preserved\n"
              << "   • No packet loss on critical messages\n"
              << "\n4. SYNCHRONIZED CAPTURE STOP\n"
              << "   All nodes stop capture and upload PCAPs\n"
              << "   Coordinator merges PCAPs with aligned timestamps\n"
              << "\n5. RESULT AGGREGATION\n"
              << "   Collect per-node results\n"
              << "   Generate JUnit XML for CI\n"
              << "   Generate HTML report for review\n";
}

/**
 * @brief Example: Display expected assertions
 *
 * Shows what assertions would be validated
 */
void display_assertions() {
    std::cout << "\n=== Expected Assertions ===\n";
    std::cout << "\nAssertion 1: SOME/IP SD Message Flow\n"
              << "  Expected: Service advertisement on provider → monitor → consumer\n"
              << "  Validation: Verify packet order using distributed matcher\n"
              << "\nAssertion 2: Discovery Latency\n"
              << "  Expected: Request sent on consumer, response received <100ms\n"
              << "  Validation: Calculate latency from timestamps\n"
              << "\nAssertion 3: Timestamp Alignment\n"
              << "  Expected: Same message visible on provider and monitor within 1µs\n"
              << "  Validation: Cross-node timestamp correlation\n"
              << "\nAssertion 4: No Packet Loss\n"
              << "  Expected: All SD messages captured on at least 2 nodes\n"
              << "  Validation: Correlation and count matching\n";
}

/**
 * @brief Example: Display expected results
 *
 * Shows what would be in the final report
 */
void display_expected_results() {
    std::cout << "\n=== Expected Results ===\n";
    std::cout << "\nPer-Node Results:\n"
              << "  Provider:\n"
              << "    - Packets captured: ~450 (10s @ 45 pkts/sec)\n"
              << "    - Assertions: 2 passed, 0 failed\n"
              << "    - PCAP: provider_2026-02-07_20-15-30.pcap\n"
              << "\n  Consumer:\n"
              << "    - Packets captured: ~520 (10s + overhead)\n"
              << "    - Assertions: 2 passed, 0 failed\n"
              << "    - PCAP: consumer_2026-02-07_20-15-30.pcap\n"
              << "\n  Monitor:\n"
              << "    - Packets captured: ~900 (observes both interfaces)\n"
              << "    - Assertions: 1 passed, 0 failed\n"
              << "    - PCAP: monitor_2026-02-07_20-15-30.pcap\n"
              << "\nAggregated Results:\n"
              << "  Test name: someip_discovery\n"
              << "  Test status: PASSED\n"
              << "  Total assertions: 5\n"
              << "  Passed: 5, Failed: 0\n"
              << "  Duration: 10.245 seconds\n"
              << "  Merged PCAP: merged_2026-02-07_20-15-30.pcap\n";
}

/**
 * @brief Main example entry point
 *
 * Demonstrates the distributed testing workflow without requiring
 * actual network setup or nodes
 */
int main(int argc, char* argv[]) {
    std::cout << "\n"
              << "╔════════════════════════════════════════════════════════════╗\n"
              << "║     Wadjet-Link Distributed Testing Example               ║\n"
              << "║     3-Node SOME/IP Service Discovery Scenario             ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n";
    
    // Configure logging
    Logger::set_level(Logger::Level::INFO);
    
    SomeipSdExampleConfig config;
    
    // Show configuration
    std::cout << "\n=== Configuration ===\n"
              << "Coordinator: " << config.coordinator_address << ":"
              << config.coordinator_port << "\n"
              << "Nodes: " << config.nodes.size() << "\n"
              << "Capture duration: " << config.capture_duration.count() << "ms\n"
              << "Latency bound: " << config.latency_bound.count() << "ms\n";
    
    // Display node registration
    register_example_nodes(nullptr, config);
    
    // Display test flow
    display_test_flow();
    
    // Display assertions
    display_assertions();
    
    // Display expected results
    display_expected_results();
    
    // Show instructions
    std::cout << "\n=== How to Run This Example ===\n"
              << "\n1. Ensure 3 nodes are available with network connectivity\n"
              << "2. Configure static SOME/IP traffic with SOME/IP SD service:\n"
              << "      someip-send --service 0x1234 --instance 0x0001 ...\n"
              << "3. Start coordinator in terminal 1:\n"
              << "      wadjet-coordinator --config nodes.yaml \\\n"
              << "        --scenario someip_discovery.yaml \\\n"
              << "        --output-dir ./results \\\n"
              << "        --junit-report results.xml\n"
              << "\n4. Start nodes in separate terminals:\n"
              << "      wadjet-node --node-id provider --check-clock\n"
              << "      wadjet-node --node-id consumer --check-clock\n"
              << "      wadjet-node --node-id monitor --check-clock\n"
              << "\n5. Review results in ./results/\n"
              << "      - results.xml (JUnit format)\n"
              << "      - report.html (Human-readable)\n"
              << "      - merged_*.pcap (Aligned packets)\n";
    
    std::cout << "\n=== Example Complete ===\n\n";
    
    return 0;
}

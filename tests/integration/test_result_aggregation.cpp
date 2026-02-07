#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>
#include "wadjet/distributed/result_aggregation.hpp"
#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"

using namespace wadjet::distributed;

/**
 * T098: Integration test for complete result aggregation flow
 * 
 * Tests:
 * 1. Multi-node test execution with result collection
 * 2. PCAP attachment on failure
 * 3. Failure capture directory creation (T095)
 * 4. Full result aggregation and export
 */
class ResultAggregationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test coordinator
        CoordinatorConfig config;
        config.grpc_port = 50051;
        config.enable_partial_results = true;
        
        auto coordinator_result = TestCoordinator::create(config);
        ASSERT_TRUE(coordinator_result.is_ok());
        coordinator_ = std::move(coordinator_result.unwrap());
    }
    
    void TearDown() override {
        if (coordinator_ && coordinator_->is_running()) {
            coordinator_->stop();
        }
    }
    
    std::unique_ptr<TestCoordinator> coordinator_;
};

/**
 * T090, T091: Test collecting and exporting results to JUnit XML
 */
TEST_F(ResultAggregationIntegrationTest, CollectAndExportJunitResults) {
    // Simulate a 3-node test execution
    AggregatedResult result;
    result.test_name = "integration_test_3_nodes";
    result.test_start_time_ns = 1675000000000000000LL;
    result.test_end_time_ns = 1675000030000000000LL;
    result.global_metadata = {
        {"test_scenario", "multi_node_latency"},
        {"gPTP_sync", "SYNCHRONIZED"}
    };
    
    // Node 1: All assertions passed
    {
        NodeResult node1;
        node1.node_id = "ecu-1";
        node1.node_name = "ECU-1";
        node1.healthy = true;
        node1.passed_count = 5;
        node1.failed_count = 0;
        node1.has_capture = true;
        node1.pcap_file_path = "/tmp/test_captures/ecu-1.pcap";
        node1.start_time_ns = 1675000000000000000LL;
        node1.end_time_ns = 1675000010000000000LL;
        node1.total_duration = std::chrono::nanoseconds(10000000000LL);
        
        for (int i = 0; i < 5; i++) {
            node1.assertions.push_back(AssertionResult{
                .assertion_id = "ASSERT_NODE1_" + std::to_string(i),
                .test_name = "latency_check",
                .passed = true,
                .expression = "latency < 100ms",
                .failure_message = "",
                .timestamp_ns = 1675000000000000000LL + (i * 1000000000LL),
                .duration = std::chrono::milliseconds(10 + i),
                .context = {}
            });
        }
        
        result.node_results.push_back(node1);
    }
    
    // Node 2: Some failures
    {
        NodeResult node2;
        node2.node_id = "ecu-2";
        node2.node_name = "ECU-2";
        node2.healthy = true;
        node2.passed_count = 3;
        node2.failed_count = 2;
        node2.has_capture = true;
        node2.pcap_file_path = "/tmp/test_captures/ecu-2.pcap";
        node2.failure_capture_dir = "/tmp/test_captures/failures/ecu-2";
        node2.start_time_ns = 1675000000000000000LL;
        node2.end_time_ns = 1675000015000000000LL;
        node2.total_duration = std::chrono::nanoseconds(15000000000LL);
        
        for (int i = 0; i < 3; i++) {
            node2.assertions.push_back(AssertionResult{
                .assertion_id = "ASSERT_NODE2_PASS_" + std::to_string(i),
                .test_name = "latency_check",
                .passed = true,
                .expression = "latency < 100ms",
                .failure_message = "",
                .timestamp_ns = 1675000000000000000LL + (i * 1000000000LL),
                .duration = std::chrono::milliseconds(15 + i),
                .context = {}
            });
        }
        
        node2.assertions.push_back(AssertionResult{
            .assertion_id = "ASSERT_NODE2_FAIL_1",
            .test_name = "latency_check",
            .passed = false,
            .expression = "latency < 100ms",
            .failure_message = "Latency spike detected: 150ms",
            .timestamp_ns = 1675000003000000000LL,
            .duration = std::chrono::milliseconds(25),
            .context = {"Frame at offset 1024 in capture"}
        });
        
        node2.assertions.push_back(AssertionResult{
            .assertion_id = "ASSERT_NODE2_FAIL_2",
            .test_name = "packet_ordering",
            .passed = false,
            .expression = "packet_sequence_valid",
            .failure_message = "Out-of-order packets detected",
            .timestamp_ns = 1675000010000000000LL,
            .duration = std::chrono::milliseconds(18),
            .context = {"Packet sequence: 1,3,2,4,5", "Expected: 1,2,3,4,5"}
        });
        
        result.node_results.push_back(node2);
    }
    
    // Node 3: All passed
    {
        NodeResult node3;
        node3.node_id = "ecu-3";
        node3.node_name = "ECU-3";
        node3.healthy = true;
        node3.passed_count = 5;
        node3.failed_count = 0;
        node3.has_capture = true;
        node3.pcap_file_path = "/tmp/test_captures/ecu-3.pcap";
        node3.start_time_ns = 1675000000000000000LL;
        node3.end_time_ns = 1675000008000000000LL;
        node3.total_duration = std::chrono::nanoseconds(8000000000LL);
        
        for (int i = 0; i < 5; i++) {
            node3.assertions.push_back(AssertionResult{
                .assertion_id = "ASSERT_NODE3_" + std::to_string(i),
                .test_name = "latency_check",
                .passed = true,
                .expression = "latency < 100ms",
                .failure_message = "",
                .timestamp_ns = 1675000000000000000LL + (i * 1000000000LL),
                .duration = std::chrono::milliseconds(8 + i),
                .context = {}
            });
        }
        
        result.node_results.push_back(node3);
    }
    
    result.failure_captures_dir = "/tmp/test_captures/failures";
    result.has_failure_captures = true;
    
    // Verify aggregation
    EXPECT_FALSE(result.all_passed());
    EXPECT_EQ(result.total_passed(), 13);
    EXPECT_EQ(result.total_failed(), 2);
    
    auto failed_nodes = result.failed_nodes();
    EXPECT_EQ(failed_nodes.size(), 1);
    EXPECT_EQ(failed_nodes[0], "ecu-2");
    
    auto failed_assertions = result.failed_assertions();
    EXPECT_EQ(failed_assertions.size(), 2);
}

/**
 * T091, T093: Test JUnit XML and HTML report generation
 */
TEST_F(ResultAggregationIntegrationTest, GenerateReports) {
    AggregatedResult result;
    result.test_name = "report_generation_test";
    result.test_start_time_ns = 1675000000000000000LL;
    result.test_end_time_ns = 1675000005000000000LL;
    result.failure_captures_dir = "/tmp/captures/failures";
    result.has_failure_captures = false;
    result.global_metadata = {
        {"test_id", "TES-001"},
        {"test_version", "1.0.0"}
    };
    
    // Add single node with mixed results
    NodeResult node;
    node.node_id = "node-test";
    node.node_name = "Test-Node";
    node.healthy = true;
    node.passed_count = 2;
    node.failed_count = 1;
    node.has_capture = true;
    node.pcap_file_path = "/captures/node-test.pcap";
    node.start_time_ns = result.test_start_time_ns;
    node.end_time_ns = result.test_end_time_ns;
    node.total_duration = std::chrono::nanoseconds(5000000000LL);
    
    node.assertions.push_back(AssertionResult{
        .assertion_id = "TEST_001",
        .test_name = "basic_connectivity",
        .passed = true,
        .expression = "node_reachable",
        .failure_message = "",
        .timestamp_ns = result.test_start_time_ns,
        .duration = std::chrono::milliseconds(100),
        .context = {}
    });
    
    node.assertions.push_back(AssertionResult{
        .assertion_id = "TEST_002",
        .test_name = "latency_test",
        .passed = true,
        .expression = "latency < 50ms",
        .failure_message = "",
        .timestamp_ns = result.test_start_time_ns + 1000000000LL,
        .duration = std::chrono::milliseconds(50),
        .context = {}
    });
    
    node.assertions.push_back(AssertionResult{
        .assertion_id = "TEST_003",
        .test_name = "throughput_test",
        .passed = false,
        .expression = "throughput > 1000Mbps",
        .failure_message = "Throughput was only 900Mbps",
        .timestamp_ns = result.test_start_time_ns + 2000000000LL,
        .duration = std::chrono::milliseconds(200),
        .context = {"Measurement period: 10s", "Network congestion observed"}
    });
    
    result.node_results.push_back(node);
    
    // Generate JUnit XML
    auto junit_xml = result.to_junit_xml();
    EXPECT_GT(junit_xml.length(), 0);
    EXPECT_NE(junit_xml.find("<?xml"), std::string::npos);
    EXPECT_NE(junit_xml.find("<testsuite"), std::string::npos);
    EXPECT_NE(junit_xml.find("tests=\"3\""), std::string::npos);
    EXPECT_NE(junit_xml.find("failures=\"1\""), std::string::npos);
    EXPECT_NE(junit_xml.find("throughput > 1000Mbps"), std::string::npos);
    
    // Generate HTML report
    auto html_report = result.to_html_report();
    EXPECT_GT(html_report.length(), 0);
    EXPECT_NE(html_report.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(html_report.find("report_generation_test"), std::string::npos);
    EXPECT_NE(html_report.find("Test-Node"), std::string::npos);
    EXPECT_NE(html_report.find("Failed Assertions"), std::string::npos);
    EXPECT_NE(html_report.find("Throughput was only 900Mbps"), std::string::npos);
}

/**
 * T092: Test JSON serialization/deserialization roundtrip
 */
TEST_F(ResultAggregationIntegrationTest, JsonRoundtrip) {
    AggregatedResult original;
    original.test_name = "json_roundtrip_test";
    original.test_start_time_ns = 1675000000000000000LL;
    original.test_end_time_ns = 1675000010000000000LL;
    original.failure_captures_dir = "/tmp/failures";
    original.has_failure_captures = true;
    original.global_metadata = {
        {"test_env", "staging"},
        {"test_branch", "develop"}
    };
    
    // Add node with mixed results
    NodeResult node;
    node.node_id = "node-json-test";
    node.node_name = "JSON-Test-Node";
    node.healthy = true;
    node.passed_count = 3;
    node.failed_count = 1;
    node.start_time_ns = original.test_start_time_ns;
    node.end_time_ns = original.test_end_time_ns;
    node.metadata = {
        {"kernel_version", "5.10.0"}
    };
    
    node.assertions.push_back(AssertionResult{
        .assertion_id = "JSON_TEST_001",
        .test_name = "test_1",
        .passed = true,
        .expression = "expr1",
        .failure_message = "",
        .timestamp_ns = original.test_start_time_ns,
        .duration = std::chrono::milliseconds(50),
        .context = {}
    });
    
    node.assertions.push_back(AssertionResult{
        .assertion_id = "JSON_TEST_002",
        .test_name = "test_2",
        .passed = false,
        .expression = "expr2",
        .failure_message = "Failed as expected",
        .timestamp_ns = original.test_start_time_ns + 1000000000LL,
        .duration = std::chrono::milliseconds(75),
        .context = {"context_line_1", "context_line_2"}
    });
    
    original.node_results.push_back(node);
    
    // Serialize to JSON
    auto json_obj = original.to_json();
    std::string json_str = json_obj.dump(2);
    
    EXPECT_GT(json_str.length(), 0);
    
    // Deserialize from JSON
    auto json_parsed = nlohmann::json::parse(json_str);
    auto result = AggregatedResult::from_json(json_parsed);
    
    ASSERT_TRUE(result.is_ok());
    auto deserialized = result.unwrap();
    
    // Verify all data survived roundtrip
    EXPECT_EQ(deserialized.test_name, original.test_name);
    EXPECT_EQ(deserialized.test_start_time_ns, original.test_start_time_ns);
    EXPECT_EQ(deserialized.test_end_time_ns, original.test_end_time_ns);
    EXPECT_EQ(deserialized.failure_captures_dir, original.failure_captures_dir);
    EXPECT_EQ(deserialized.has_failure_captures, original.has_failure_captures);
    EXPECT_EQ(deserialized.global_metadata.size(), original.global_metadata.size());
    EXPECT_EQ(deserialized.node_results.size(), original.node_results.size());
    
    EXPECT_EQ(deserialized.total_passed(), original.total_passed());
    EXPECT_EQ(deserialized.total_failed(), original.total_failed());
    
    // Verify node data
    ASSERT_EQ(deserialized.node_results.size(), 1);
    auto& deserialized_node = deserialized.node_results[0];
    EXPECT_EQ(deserialized_node.node_id, "node-json-test");
    EXPECT_EQ(deserialized_node.node_name, "JSON-Test-Node");
    EXPECT_EQ(deserialized_node.passed_count, 3);
    EXPECT_EQ(deserialized_node.failed_count, 1);
    EXPECT_EQ(deserialized_node.assertions.size(), 2);
    
    // Verify assertion context preserved
    auto& failed_assertion = deserialized_node.assertions[1];
    EXPECT_EQ(failed_assertion.context.size(), 2);
    EXPECT_EQ(failed_assertion.context[0], "context_line_1");
}

/**
 * T094, T095: Test failure capture handling
 */
TEST_F(ResultAggregationIntegrationTest, FailureCaptureTracking) {
    AggregatedResult result;
    result.test_name = "failure_capture_test";
    result.test_start_time_ns = 1675000000000000000LL;
    result.test_end_time_ns = 1675000010000000000LL;
    result.failure_captures_dir = "/tmp/test_captures/failures";
    result.has_failure_captures = true;
    
    // Node with failure and associated capture
    NodeResult node_with_failure;
    node_with_failure.node_id = "node-fail";
    node_with_failure.node_name = "Failing-Node";
    node_with_failure.healthy = true;
    node_with_failure.passed_count = 1;
    node_with_failure.failed_count = 1;
    node_with_failure.has_capture = true;
    node_with_failure.pcap_file_path = "/tmp/test_captures/node-fail.pcap";
    node_with_failure.failure_capture_dir = "/tmp/test_captures/failures/node-fail";  // T095
    node_with_failure.start_time_ns = result.test_start_time_ns;
    node_with_failure.end_time_ns = result.test_end_time_ns;
    
    node_with_failure.assertions.push_back(AssertionResult{
        .assertion_id = "FAIL_CAPTURE_001",
        .test_name = "test_1",
        .passed = true,
        .expression = "expr1",
        .failure_message = "",
        .timestamp_ns = result.test_start_time_ns,
        .duration = std::chrono::milliseconds(50),
        .context = {}
    });
    
    node_with_failure.assertions.push_back(AssertionResult{
        .assertion_id = "FAIL_CAPTURE_002",
        .test_name = "test_2",
        .passed = false,
        .expression = "expr2",
        .failure_message = "Frame analysis failed",
        .timestamp_ns = result.test_start_time_ns + 5000000000LL,
        .duration = std::chrono::milliseconds(100),
        .context = {"Corrupted frame at offset 2048"}
    });
    
    result.node_results.push_back(node_with_failure);
    
    // Verify capture tracking
    EXPECT_TRUE(result.has_failure_captures);
    EXPECT_FALSE(result.failure_captures_dir.empty());
    
    auto failed_nodes = result.failed_nodes();
    EXPECT_EQ(failed_nodes.size(), 1);
    
    auto failed = result.failed_assertions();
    EXPECT_EQ(failed.size(), 1);
    
    // Verify we can access failure capture directory
    for (const auto& node : result.node_results) {
        if (!node.all_passed()) {
            EXPECT_FALSE(node.failure_capture_dir.empty());
            EXPECT_NE(node.failure_capture_dir.find("/failures/"), std::string::npos);
        }
    }
}

/**
 * Large-scale scenario: 10 nodes, 100+ assertions
 */
TEST_F(ResultAggregationIntegrationTest, LargeScaleAggregation) {
    AggregatedResult result;
    result.test_name = "large_scale_test";
    result.test_start_time_ns = 1675000000000000000LL;
    result.test_end_time_ns = 1675000060000000000LL;  // 60 second test
    result.global_metadata = {
        {"num_nodes", "10"},
        {"num_assertions", "100"}
    };
    
    int total_assertions = 0;
    for (int node_idx = 0; node_idx < 10; node_idx++) {
        NodeResult node;
        node.node_id = "node-" + std::to_string(node_idx);
        node.node_name = "ECU-" + std::to_string(node_idx);
        node.healthy = true;
        node.start_time_ns = result.test_start_time_ns;
        node.end_time_ns = result.test_end_time_ns;
        node.total_duration = std::chrono::nanoseconds(60000000000LL);
        
        // Add 10 assertions per node (100 total)
        for (int i = 0; i < 10; i++) {
            bool passed = (node_idx + i) % 7 != 0;  // Some failures for variety
            node.assertions.push_back(AssertionResult{
                .assertion_id = "ASSERT_" + std::to_string(node_idx) + "_" + std::to_string(i),
                .test_name = "test_" + std::to_string(i),
                .passed = passed,
                .expression = "check_" + std::to_string(i),
                .failure_message = passed ? "" : "Assertion failed",
                .timestamp_ns = result.test_start_time_ns + (i * 6000000000LL),
                .duration = std::chrono::milliseconds(10 + i),
                .context = {}
            });
            
            if (passed) {
                node.passed_count++;
            } else {
                node.failed_count++;
            }
            
            total_assertions++;
        }
        
        result.node_results.push_back(node);
    }
    
    // Verify aggregation
    EXPECT_EQ(result.total_passed() + result.total_failed(), total_assertions);
    EXPECT_GT(result.total_failed(), 0);  // Should have some failures
    EXPECT_GT(result.total_passed(), 0);  // Should have some passes
    
    // Generate reports should complete quickly even with large data
    auto junit_xml = result.to_junit_xml();
    EXPECT_GT(junit_xml.length(), 1000);
    
    auto html_report = result.to_html_report();
    EXPECT_GT(html_report.length(), 1000);
    
    auto json_obj = result.to_json();
    EXPECT_EQ(json_obj["node_results"].size(), 10);
}

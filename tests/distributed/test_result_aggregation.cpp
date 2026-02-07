#include "wadjet/distributed/result_aggregation.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace wadjet::distributed;

// ==================== AssertionResult Tests ====================

class AssertionResultTest : public ::testing::Test {
protected:
    AssertionResult create_passed_assertion() {
        return AssertionResult{.assertion_id = "ASSERT_001",
                               .test_name = "test_network_latency",
                               .passed = true,
                               .expression = "latency_ms < 100",
                               .failure_message = "",
                               .timestamp_ns = 1675000000000000000LL,
                               .duration = std::chrono::milliseconds(42),
                               .context = {}};
    }

    AssertionResult create_failed_assertion() {
        return AssertionResult{.assertion_id = "ASSERT_002",
                               .test_name = "test_packet_loss",
                               .passed = false,
                               .expression = "packet_loss == 0",
                               .failure_message = "Expected 0 lost packets, got 5",
                               .timestamp_ns = 1675000000050000000LL,
                               .duration = std::chrono::milliseconds(28),
                               .context = {"stack_frame_1", "stack_frame_2"}};
    }
};

TEST_F(AssertionResultTest, ToJsonConversionPassed) {
    auto assertion = create_passed_assertion();
    auto json_obj = assertion.to_json();

    EXPECT_EQ(json_obj["assertion_id"], "ASSERT_001");
    EXPECT_EQ(json_obj["test_name"], "test_network_latency");
    EXPECT_TRUE(json_obj["passed"]);
    EXPECT_EQ(json_obj["expression"], "latency_ms < 100");
}

TEST_F(AssertionResultTest, ToJsonConversionFailed) {
    auto assertion = create_failed_assertion();
    auto json_obj = assertion.to_json();

    EXPECT_EQ(json_obj["assertion_id"], "ASSERT_002");
    EXPECT_FALSE(json_obj["passed"]);
    EXPECT_EQ(json_obj["failure_message"], "Expected 0 lost packets, got 5");
}

TEST_F(AssertionResultTest, RoundTripJsonConversion) {
    auto original = create_passed_assertion();
    auto json_obj = original.to_json();

    auto result = AssertionResult::from_json(json_obj);
    ASSERT_TRUE(result.is_ok());

    auto converted = result.unwrap();
    EXPECT_EQ(converted.assertion_id, original.assertion_id);
    EXPECT_EQ(converted.test_name, original.test_name);
    EXPECT_EQ(converted.passed, original.passed);
    EXPECT_EQ(converted.expression, original.expression);
    EXPECT_EQ(converted.timestamp_ns, original.timestamp_ns);
}

TEST_F(AssertionResultTest, JsonConversionWithContext) {
    auto assertion = create_failed_assertion();
    auto json_obj = assertion.to_json();

    auto result = AssertionResult::from_json(json_obj);
    ASSERT_TRUE(result.is_ok());

    auto converted = result.unwrap();
    EXPECT_EQ(converted.context.size(), 2);
    EXPECT_EQ(converted.context[0], "stack_frame_1");
}

TEST_F(AssertionResultTest, InvalidJsonHandling) {
    json invalid_json = {{"invalid_field", "value"}};

    auto result = AssertionResult::from_json(invalid_json);
    EXPECT_TRUE(result.is_err());
}

// ==================== NodeResult Tests ====================

class NodeResultTest : public ::testing::Test {
protected:
    NodeResult create_healthy_node() {
        NodeResult result;
        result.node_id = "node-1";
        result.node_name = "ECU-1";
        result.healthy = true;
        result.passed_count = 5;
        result.failed_count = 0;
        result.total_duration = std::chrono::nanoseconds(5000000000LL);
        result.pcap_file_path = "/captures/node-1.pcap";
        result.has_capture = true;
        result.start_time_ns = 1675000000000000000LL;
        result.end_time_ns = 1675000005000000000LL;

        result.assertions.push_back(AssertionResult{.assertion_id = "ASSERT_001",
                                                    .test_name = "test_1",
                                                    .passed = true,
                                                    .expression = "expr1",
                                                    .failure_message = "",
                                                    .timestamp_ns = 1675000000000000000LL,
                                                    .duration = std::chrono::milliseconds(100),
                                                    .context = {}});

        return result;
    }

    NodeResult create_unhealthy_node() {
        NodeResult result;
        result.node_id = "node-2";
        result.node_name = "ECU-2";
        result.healthy = false;
        result.passed_count = 2;
        result.failed_count = 3;
        result.error_message = "Network timeout";
        result.start_time_ns = 1675000000000000000LL;
        result.end_time_ns = 1675000002000000000LL;

        return result;
    }
};

TEST_F(NodeResultTest, HealthyNodeProperties) {
    auto node = create_healthy_node();

    EXPECT_TRUE(node.healthy);
    EXPECT_EQ(node.total_assertions(), 1);
    EXPECT_TRUE(node.all_passed());
    EXPECT_TRUE(node.has_capture);
}

TEST_F(NodeResultTest, UnhealthyNodeProperties) {
    auto node = create_unhealthy_node();

    EXPECT_FALSE(node.healthy);
    EXPECT_EQ(node.total_assertions(), 5);
    EXPECT_FALSE(node.all_passed());
    EXPECT_EQ(node.failed_count, 3);
}

TEST_F(NodeResultTest, ToJsonConversion) {
    auto node = create_healthy_node();
    auto json_obj = node.to_json();

    EXPECT_EQ(json_obj["node_id"], "node-1");
    EXPECT_EQ(json_obj["node_name"], "ECU-1");
    EXPECT_TRUE(json_obj["healthy"]);
    EXPECT_EQ(json_obj["passed_count"], 1);
    EXPECT_EQ(json_obj["failed_count"], 0);
}

TEST_F(NodeResultTest, RoundTripJsonConversion) {
    auto original = create_healthy_node();
    auto json_obj = original.to_json();

    auto result = NodeResult::from_json(json_obj);
    ASSERT_TRUE(result.is_ok());

    auto converted = result.unwrap();
    EXPECT_EQ(converted.node_id, original.node_id);
    EXPECT_EQ(converted.node_name, original.node_name);
    EXPECT_EQ(converted.passed_count, original.passed_count);
    EXPECT_EQ(converted.assertions.size(), original.assertions.size());
}

// ==================== AggregatedResult Tests ====================

class AggregatedResultTest : public ::testing::Test {
protected:
    AggregatedResult create_distributed_test_result() {
        AggregatedResult result;
        result.test_name = "distributed_network_test";
        result.test_start_time_ns = 1675000000000000000LL;
        result.test_end_time_ns = 1675000010000000000LL;
        result.failure_captures_dir = "/captures/failures";
        result.has_failure_captures = false;
        result.global_metadata = {{"gPTP_sync_status", "SYNCHRONIZED"},
                                  {"network_topology", "3-node_ring"}};

        // Node 1: All passed
        NodeResult node1;
        node1.node_id = "node-1";
        node1.node_name = "ECU-1";
        node1.healthy = true;
        node1.passed_count = 3;
        node1.failed_count = 0;
        node1.has_capture = true;
        node1.pcap_file_path = "/captures/node-1.pcap";
        result.node_results.push_back(node1);

        // Node 2: Some failures
        NodeResult node2;
        node2.node_id = "node-2";
        node2.node_name = "ECU-2";
        node2.healthy = true;
        node2.passed_count = 2;
        node2.failed_count = 1;
        node2.has_capture = true;
        node2.pcap_file_path = "/captures/node-2.pcap";
        node2.assertions.push_back(AssertionResult{.assertion_id = "ASSERT_FAIL_001",
                                                   .test_name = "latency_test",
                                                   .passed = false,
                                                   .expression = "latency < 50ms",
                                                   .failure_message = "Latency was 75ms",
                                                   .timestamp_ns = 1675000005000000000LL,
                                                   .duration = std::chrono::milliseconds(15),
                                                   .context = {}});
        result.node_results.push_back(node2);

        return result;
    }
};

TEST_F(AggregatedResultTest, TotalPassedCount) {
    auto result = create_distributed_test_result();
    EXPECT_EQ(result.total_passed(), 5);
}

TEST_F(AggregatedResultTest, TotalFailedCount) {
    auto result = create_distributed_test_result();
    EXPECT_EQ(result.total_failed(), 1);
}

TEST_F(AggregatedResultTest, AllPassedFlag) {
    auto result = create_distributed_test_result();
    EXPECT_FALSE(result.all_passed());
}

TEST_F(AggregatedResultTest, FailedNodes) {
    auto result = create_distributed_test_result();
    auto failed = result.failed_nodes();

    ASSERT_EQ(failed.size(), 1);
    EXPECT_EQ(failed[0], "node-2");
}

TEST_F(AggregatedResultTest, FailedAssertions) {
    auto result = create_distributed_test_result();
    auto failed = result.failed_assertions();

    ASSERT_EQ(failed.size(), 1);
    EXPECT_EQ(failed[0].second.assertion_id, "ASSERT_FAIL_001");
}

TEST_F(AggregatedResultTest, ToJsonConversion) {
    auto result = create_distributed_test_result();
    auto json_obj = result.to_json();

    EXPECT_EQ(json_obj["test_name"], "distributed_network_test");
    EXPECT_EQ(json_obj["node_results"].size(), 2);
    EXPECT_TRUE(json_obj["global_metadata"].contains("gPTP_sync_status"));
}

TEST_F(AggregatedResultTest, RoundTripJsonConversion) {
    auto original = create_distributed_test_result();
    auto json_obj = original.to_json();

    auto result = AggregatedResult::from_json(json_obj);
    ASSERT_TRUE(result.is_ok());

    auto converted = result.unwrap();
    EXPECT_EQ(converted.test_name, original.test_name);
    EXPECT_EQ(converted.node_results.size(), original.node_results.size());
    EXPECT_EQ(converted.total_passed(), original.total_passed());
}

// ==================== JUnit XML Generation Tests ====================

TEST_F(AggregatedResultTest, JunitXmlGeneration) {
    auto result = create_distributed_test_result();
    auto xml = result.to_junit_xml();

    EXPECT_NE(xml.find("<?xml version"), std::string::npos);
    EXPECT_NE(xml.find("<testsuite"), std::string::npos);
    EXPECT_NE(xml.find("distributed_network_test"), std::string::npos);
    EXPECT_NE(xml.find("tests=\"6\""), std::string::npos);
    EXPECT_NE(xml.find("failures=\"1\""), std::string::npos);
}

TEST_F(AggregatedResultTest, JunitXmlContainsFailures) {
    auto result = create_distributed_test_result();
    auto xml = result.to_junit_xml();

    EXPECT_NE(xml.find("<failure"), std::string::npos);
    EXPECT_NE(xml.find("Latency was 75ms"), std::string::npos);
}

TEST_F(AggregatedResultTest, JunitXmlContainsNodeClassnames) {
    auto result = create_distributed_test_result();
    auto xml = result.to_junit_xml();

    EXPECT_NE(xml.find("classname=\"ECU-1\""), std::string::npos);
    EXPECT_NE(xml.find("classname=\"ECU-2\""), std::string::npos);
}

// ==================== HTML Report Generation Tests ====================

TEST_F(AggregatedResultTest, HtmlReportGeneration) {
    auto result = create_distributed_test_result();
    auto html = result.to_html_report();

    EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(html.find("distributed_network_test"), std::string::npos);
    EXPECT_NE(html.find("<table>"), std::string::npos);
}

TEST_F(AggregatedResultTest, HtmlReportContainsSummary) {
    auto result = create_distributed_test_result();
    auto html = result.to_html_report();

    EXPECT_NE(html.find("Total Assertions"), std::string::npos);
    EXPECT_NE(html.find("passed"), std::string::npos);
}

TEST_F(AggregatedResultTest, HtmlReportNodeResults) {
    auto result = create_distributed_test_result();
    auto html = result.to_html_report();

    EXPECT_NE(html.find("ECU-1"), std::string::npos);
    EXPECT_NE(html.find("ECU-2"), std::string::npos);
    EXPECT_NE(html.find("PASS"), std::string::npos);
    EXPECT_NE(html.find("FAIL"), std::string::npos);
}

TEST_F(AggregatedResultTest, HtmlReportFailedAssertions) {
    auto result = create_distributed_test_result();
    auto html = result.to_html_report();

    // Should contain failed assertions section if any failed
    EXPECT_NE(html.find("Failed Assertions"), std::string::npos);
    EXPECT_NE(html.find("ASSERT_FAIL_001"), std::string::npos);
}

TEST_F(AggregatedResultTest, HtmlReportWithCustomCss) {
    auto result = create_distributed_test_result();
    auto html = result.to_html_report("custom_style.css");

    EXPECT_NE(html.find("custom_style.css"), std::string::npos);
}

// ==================== AllPassedScenario Test ====================

TEST(AggregatedResultAllPassedTest, AllAssertionsPassed) {
    AggregatedResult result;
    result.test_name = "successful_test";
    result.test_start_time_ns = 1675000000000000000LL;
    result.test_end_time_ns = 1675000005000000000LL;

    NodeResult node;
    node.node_id = "node-1";
    node.node_name = "ECU-1";
    node.healthy = true;
    node.passed_count = 10;
    node.failed_count = 0;
    result.node_results.push_back(node);

    EXPECT_TRUE(result.all_passed());
    EXPECT_EQ(result.total_failed(), 0);
    EXPECT_EQ(result.total_passed(), 10);
}

// ==================== Empty Result Test ====================

TEST(AggregatedResultEmptyTest, EmptyNodeResults) {
    AggregatedResult result;
    result.test_name = "empty_test";
    result.test_start_time_ns = 1675000000000000000LL;
    result.test_end_time_ns = 1675000005000000000LL;

    EXPECT_TRUE(result.all_passed());
    EXPECT_EQ(result.total_passed(), 0);
    EXPECT_EQ(result.total_failed(), 0);
    EXPECT_TRUE(result.failed_nodes().empty());
    EXPECT_TRUE(result.failed_assertions().empty());
}

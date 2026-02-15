#include "wadjet/distributed/scenario_adapter.hpp"
#include "wadjet/scenario/scenario_types.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed {

class ScenarioAdapterTest : public ::testing::Test {
protected:
    ScenarioAdapter adapter{"test-node"};
};

// T326: Test ScenarioAdapter creation
TEST_F(ScenarioAdapterTest, AdapterCreatedWithDefaultNode) {
    ScenarioAdapter test_adapter("my-node");
    // Constructor completes successfully
    EXPECT_TRUE(true);
}

// T326: Test empty scenario conversion
TEST_F(ScenarioAdapterTest, ConvertEmptyScenario) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "empty_test";
    m4_scenario.description = "Test empty scenario";
    m4_scenario.version = "1.0";

    auto m14_scenario = adapter.adapt(m4_scenario, "node-1");

    EXPECT_EQ(m14_scenario->name, "empty_test");
    EXPECT_EQ(m14_scenario->description, "Test empty scenario");
    EXPECT_EQ(m14_scenario->version, "1.0");
    EXPECT_EQ(m14_scenario->steps().size(), 0);
}

// T326: Test WaitStep conversion
TEST_F(ScenarioAdapterTest, ConvertWaitStep) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "wait_test";

    wadjet::scenario::WaitStep wait_step;
    wait_step.duration = std::chrono::milliseconds(1000);
    m4_scenario.steps.push_back(wait_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "node-1");

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].type, StepType::WAIT);
    EXPECT_EQ(m14_scenario->steps()[0].target_nodes, std::vector<std::string>{"node-1"});

    auto* wait_config = std::get_if<WaitStepConfig>(&m14_scenario->steps()[0].config);
    EXPECT_NE(wait_config, nullptr);
    EXPECT_EQ(wait_config->duration.count(), 1000);
}

// T326: Test LogStep conversion
TEST_F(ScenarioAdapterTest, ConvertLogStep) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "log_test";

    wadjet::scenario::LogStep log_step;
    log_step.message = "Test message";
    log_step.level = "info";
    m4_scenario.steps.push_back(log_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "node-1");

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].type, StepType::LOG);

    auto* log_config = std::get_if<LogStepConfig>(&m14_scenario->steps()[0].config);
    EXPECT_NE(log_config, nullptr);
    EXPECT_EQ(log_config->message, "Test message");
    EXPECT_EQ(log_config->level, "info");
}

// T327: Test SendStep conversion (T327 - new SendStep support)
TEST_F(ScenarioAdapterTest, ConvertSendStepWithPcap) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "send_test";

    wadjet::scenario::SendStep send_step;
    send_step.interface = "eth0";
    send_step.pcap_file = "test.pcap";
    send_step.delay = std::chrono::milliseconds(500);
    m4_scenario.steps.push_back(send_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "sender-node");

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].type, StepType::SEND);
    EXPECT_EQ(m14_scenario->steps()[0].target_nodes, std::vector<std::string>{"sender-node"});

    auto* send_config = std::get_if<SendStepConfig>(&m14_scenario->steps()[0].config);
    EXPECT_NE(send_config, nullptr);
    EXPECT_EQ(send_config->interface, "eth0");
    EXPECT_TRUE(send_config->pcap_file.has_value());
    EXPECT_EQ(send_config->pcap_file.value(), "test.pcap");
    EXPECT_EQ(send_config->delay_before_ms.count(), 500);
}

// T327: Test SendStep conversion with raw data
TEST_F(ScenarioAdapterTest, ConvertSendStepWithRawData) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "send_raw_test";

    wadjet::scenario::SendStep send_step;
    send_step.interface = "eth1";
    send_step.raw_data = std::vector<std::uint8_t>{0xAA, 0xBB, 0xCC, 0xDD};
    m4_scenario.steps.push_back(send_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "node-2");

    EXPECT_EQ(m14_scenario->steps().size(), 1);

    auto* send_config = std::get_if<SendStepConfig>(&m14_scenario->steps()[0].config);
    EXPECT_NE(send_config, nullptr);
    EXPECT_TRUE(send_config->raw_data.has_value());
    EXPECT_EQ(send_config->raw_data.value(), std::vector<std::uint8_t>{0xAA, 0xBB, 0xCC, 0xDD});
}

// T326: Test CaptureStep conversion
TEST_F(ScenarioAdapterTest, ConvertCaptureStep) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "capture_test";

    wadjet::scenario::CaptureStep capture_step;
    capture_step.config.interface = "eth0";
    capture_step.config.filter = "tcp port 80";
    capture_step.config.timeout = std::chrono::milliseconds(3000);
    m4_scenario.steps.push_back(capture_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "receiver-node");

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].type, StepType::CAPTURE);

    auto* capture_config = std::get_if<CaptureStepConfig>(&m14_scenario->steps()[0].config);
    EXPECT_NE(capture_config, nullptr);
    EXPECT_EQ(capture_config->interface, "eth0");
    EXPECT_EQ(capture_config->bpf_filter, "tcp port 80");
    EXPECT_EQ(capture_config->duration_ms.count(), 3000);
}

// T326: Test ExpectStep conversion
TEST_F(ScenarioAdapterTest, ConvertExpectStep) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "expect_test";

    wadjet::scenario::ExpectStep expect_step;
    expect_step.description = "UDP packets on port 5000";
    expect_step.within = std::chrono::milliseconds(2000);
    expect_step.count = wadjet::scenario::CountExpression{wadjet::scenario::CompareOp::GreaterEqual, 3};
    expect_step.required = true;
    m4_scenario.steps.push_back(expect_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "observer-node");

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].type, StepType::EXPECT);

    auto* expect_config = std::get_if<ExpectStepConfig>(&m14_scenario->steps()[0].config);
    EXPECT_NE(expect_config, nullptr);
    EXPECT_EQ(expect_config->assertion_type, "packet_match");
    EXPECT_EQ(expect_config->timeout_ms.count(), 2000);
    EXPECT_FALSE(expect_config->should_fail);  // required=true → should_fail=false
}

// T326: Test complex multi-step scenario
TEST_F(ScenarioAdapterTest, ConvertComplexScenario) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "complex_test";
    m4_scenario.description = "Multi-step scenario";
    m4_scenario.version = "1.0";
    m4_scenario.timeout = std::chrono::milliseconds(10000);

    // Capture step
    wadjet::scenario::CaptureStep capture_step;
    capture_step.config.interface = "eth0";
    m4_scenario.steps.push_back(capture_step);

    // Wait step
    wadjet::scenario::WaitStep wait_step;
    wait_step.duration = std::chrono::milliseconds(500);
    m4_scenario.steps.push_back(wait_step);

    // Send step
    wadjet::scenario::SendStep send_step;
    send_step.interface = "eth0";
    send_step.pcap_file = "packet.pcap";
    m4_scenario.steps.push_back(send_step);

    // Expect step
    wadjet::scenario::ExpectStep expect_step;
    expect_step.description = "Test packets";
    m4_scenario.steps.push_back(expect_step);

    auto m14_scenario = adapter.adapt(m4_scenario, "test-node");

    EXPECT_EQ(m14_scenario->name, "complex_test");
    EXPECT_EQ(m14_scenario->steps().size(), 4);
    EXPECT_EQ(m14_scenario->steps()[0].type, StepType::CAPTURE);
    EXPECT_EQ(m14_scenario->steps()[1].type, StepType::WAIT);
    EXPECT_EQ(m14_scenario->steps()[2].type, StepType::SEND);
    EXPECT_EQ(m14_scenario->steps()[3].type, StepType::EXPECT);
}

// T326: Test scenario with default node
TEST_F(ScenarioAdapterTest, ConvertWithDefaultNode) {
    ScenarioAdapter my_adapter("default-node-id");

    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "default_test";

    wadjet::scenario::WaitStep wait_step;
    wait_step.duration = std::chrono::milliseconds(100);
    m4_scenario.steps.push_back(wait_step);

    // Adapt without specifying node - should use default
    auto m14_scenario = my_adapter.adapt(m4_scenario);

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].target_nodes,
              std::vector<std::string>{"default-node-id"});
}

// T326: Test scenario with explicit node override
TEST_F(ScenarioAdapterTest, ConvertWithNodeOverride) {
    ScenarioAdapter my_adapter("default-node");

    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "override_test";

    wadjet::scenario::WaitStep wait_step;
    wait_step.duration = std::chrono::milliseconds(100);
    m4_scenario.steps.push_back(wait_step);

    // Adapt with explicit node override
    auto m14_scenario = my_adapter.adapt(m4_scenario, "override-node");

    EXPECT_EQ(m14_scenario->steps().size(), 1);
    EXPECT_EQ(m14_scenario->steps()[0].target_nodes,
              std::vector<std::string>{"override-node"});
}

// T326: Test adapt_with_nodes for multi-node mapping
TEST_F(ScenarioAdapterTest, AdaptWithMultipleNodeAssignments) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "multi_node_test";

    wadjet::scenario::WaitStep wait_step;
    wait_step.duration = std::chrono::milliseconds(100);
    m4_scenario.steps.push_back(wait_step);

    std::unordered_map<std::string, std::string> assignments;
    assignments["sender"] = "node-a";
    assignments["receiver"] = "node-b";
    assignments["observer"] = "node-c";

    auto m14_scenario = adapter.adapt_with_nodes(m4_scenario, assignments);

    EXPECT_EQ(m14_scenario->name, "multi_node_test");
    EXPECT_EQ(m14_scenario->steps().size(), 1);
    // Should assign to first node (sender preferred, then any)
    EXPECT_EQ(m14_scenario->steps()[0].target_nodes.size(), 1);
}

// T328: Test scenario metadata preservation
TEST_F(ScenarioAdapterTest, PreservesScenarioMetadata) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "metadata_test";
    m4_scenario.description = "Testing metadata preservation";
    m4_scenario.version = "2.1";
    m4_scenario.tags = {"smoke", "critical", "performance"};

    auto m14_scenario = adapter.adapt(m4_scenario, "node-1");

    EXPECT_EQ(m14_scenario->name, "metadata_test");
    EXPECT_EQ(m14_scenario->description, "Testing metadata preservation");
    EXPECT_EQ(m14_scenario->version, "2.1");
    EXPECT_EQ(m14_scenario->tags, std::vector<std::string>{"smoke", "critical", "performance"});
}

// T328: Test step IDs are unique
TEST_F(ScenarioAdapterTest, GeneratesUniqueStepIds) {
    wadjet::scenario::Scenario m4_scenario;
    m4_scenario.name = "unique_ids_test";

    for (int i = 0; i < 3; ++i) {
        wadjet::scenario::WaitStep wait_step;
        wait_step.duration = std::chrono::milliseconds(100);
        m4_scenario.steps.push_back(wait_step);
    }

    auto m14_scenario = adapter.adapt(m4_scenario, "node-1");

    EXPECT_EQ(m14_scenario->steps().size(), 3);

    // Check all step IDs are unique
    std::set<std::string> ids;
    for (const auto& step : m14_scenario->steps()) {
        ids.insert(step.step_id);
    }
    EXPECT_EQ(ids.size(), 3);
}

}  // namespace wadjet::distributed

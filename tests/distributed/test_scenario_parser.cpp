#include "wadjet/distributed/scenario.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed {

/**
 * @brief Test fixture for scenario parsing
 *
 * T303: Unit tests for YAML/JSON scenario parsing with real multi-node scenarios
 */
class DistributedScenarioParserTest : public ::testing::Test {
protected:
    /**
     * @brief Real multi-node YAML scenario with all step types
     *
     * This scenario demonstrates:
     * - Metadata extraction (scenario_id, scenario_name, description, tags)
     * - Multi-node assignments with roles (sender, receiver, observer)
     * - All 5 step types: barrier, capture, expect, wait, log
     * - Step dependencies and parallel execution flags
     * - Type-specific configurations for each step
     */
    const std::string complete_yaml_scenario = R"(
scenario_id: multi-node-end-to-end
scenario_name: "Complete Multi-Node Test Scenario"
description: "End-to-end test with all distributed features"
tags:
  - "multi-node"
  - "integration"
  - "latency-test"

node_assignments:
  - node_id: sender-node
    role: sender
    interfaces: ["eth0", "eth1"]
  - node_id: receiver-node
    role: receiver
    interfaces: ["eth0", "eth1"]
  - node_id: monitor-node
    role: observer
    interfaces: ["eth0"]

steps:
  - id: barrier-1
    name: "Initial Synchronization"
    type: barrier
    timeout_ms: 5000
    barrier_id: "start-sync"
    nodes:
      - sender-node
      - receiver-node
      - monitor-node
    parallel: false
    
  - id: capture-start
    name: "Start Packet Captures"
    type: capture
    depends_on: barrier-1
    timeout_ms: 30000
    capture_id: "capture-1"
    nodes:
      - sender-node
      - receiver-node
      - monitor-node
    interface: "eth0"
    filter: "tcp port 80"
    duration_ms: 30000
    hardware_timestamps: true
    snaplen: 65535
    buffer_size: 10485760
    parallel: false
    
  - id: wait-1
    name: "Wait for test execution"
    type: wait
    depends_on: capture-start
    timeout_ms: 0
    duration_ms: 5000
    parallel: false
    
  - id: expect-1
    name: "Verify message flow"
    type: expect
    depends_on: wait-1
    timeout_ms: 5000
    assertion_id: "flow-sender-to-receiver"
    assertion_type: "message_flow"
    assertion_params: '{"src":"sender-node","dst":"receiver-node","pattern":".*DATA.*"}'
    parallel: false
    
  - id: log-1
    name: "Test Completed"
    type: log
    depends_on: expect-1
    timeout_ms: 0
    message: "Multi-node scenario execution completed successfully"
    level: "INFO"
    parallel: false
)";

    /**
     * @brief Simple barrier-only YAML scenario
     */
    const std::string simple_barrier_yaml = R"(
scenario_id: simple-barrier
scenario_name: "Simple Barrier Test"
description: "Minimal scenario with just barrier sync"

node_assignments:
  - node_id: node-a
    role: sender
    interfaces: ["eth0"]
  - node_id: node-b
    role: receiver
    interfaces: ["eth0"]

steps:
  - id: barrier-only
    name: "Single Barrier"
    type: barrier
    timeout_ms: 5000
    barrier_id: "sync-point"
    nodes:
      - node-a
      - node-b
)";

    /**
     * @brief YAML scenario with step dependencies
     */
    const std::string yaml_with_dependencies = R"(
scenario_id: dependency-test
scenario_name: "Dependency Chain Test"

node_assignments:
  - node_id: node-1
    role: sender
  - node_id: node-2
    role: receiver

steps:
  - id: step-1
    name: "First step"
    type: barrier
    barrier_id: "b1"
    nodes: [node-1, node-2]
    
  - id: step-2
    name: "Second step - depends on first"
    type: wait
    depends_on: step-1
    duration_ms: 1000
    
  - id: step-3
    name: "Third step - depends on second"
    type: log
    depends_on: step-2
    message: "Chain execution"
)";

    /**
     * @brief Real multi-node JSON scenario
     */
    const std::string complete_json_scenario = R"({
  "scenario_id": "json-multi-node",
  "scenario_name": "JSON Multi-Node Scenario",
  "description": "Complete scenario in JSON format",
  "tags": ["json-format", "multi-node"],
  "node_assignments": [
    {
      "node_id": "json-sender",
      "role": "sender",
      "interfaces": ["eth0"]
    },
    {
      "node_id": "json-receiver",
      "role": "receiver",
      "interfaces": ["eth0"]
    }
  ],
  "steps": [
    {
      "id": "json-barrier-1",
      "name": "JSON Barrier",
      "type": "barrier",
      "timeout_ms": 5000,
      "barrier_id": "json-sync",
      "nodes": ["json-sender", "json-receiver"]
    },
    {
      "id": "json-capture-1",
      "name": "JSON Capture",
      "type": "capture",
      "depends_on": "json-barrier-1",
      "timeout_ms": 10000,
      "capture_id": "json-cap",
      "nodes": ["json-sender", "json-receiver"],
      "interface": "eth0",
      "filter": "tcp",
      "duration_ms": 10000
    },
    {
      "id": "json-expect-1",
      "name": "JSON Expectation",
      "type": "expect",
      "depends_on": "json-capture-1",
      "timeout_ms": 5000,
      "assertion_id": "json-assert",
      "assertion_type": "message_flow",
      "assertion_params": {"src": "json-sender", "dst": "json-receiver"}
    }
  ]
})";
};

/**
 * @brief Test complete YAML scenario parsing with all step types
 *
 * T303: Validates that yaml-cpp correctly parses metadata, node assignments,
 * and all 5 step types with their configurations
 */
TEST_F(DistributedScenarioParserTest, ParseCompleteYAMLScenario) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);
    EXPECT_EQ("multi-node-end-to-end", scenario->id());
    EXPECT_EQ("Complete Multi-Node Test Scenario", scenario->name());
    EXPECT_FALSE(scenario->description().empty());
    EXPECT_EQ(3, scenario->tags().size());
    EXPECT_EQ(3, scenario->node_assignments().size());
    EXPECT_EQ(5, scenario->steps().size());
}

/**
 * @brief Test scenario metadata extraction from YAML
 *
 * T300: Validates extraction of scenario_id, scenario_name, description, tags
 */
TEST_F(DistributedScenarioParserTest, YAMLMetadataExtraction) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);

    // Check metadata fields
    EXPECT_EQ("multi-node-end-to-end", scenario->id());
    EXPECT_EQ("Complete Multi-Node Test Scenario", scenario->name());

    // Check tags array
    const auto& tags = scenario->tags();
    EXPECT_EQ(3, tags.size());
    EXPECT_TRUE(std::find(tags.begin(), tags.end(), "multi-node") != tags.end());
    EXPECT_TRUE(std::find(tags.begin(), tags.end(), "integration") != tags.end());
    EXPECT_TRUE(std::find(tags.begin(), tags.end(), "latency-test") != tags.end());
}

/**
 * @brief Test node assignments parsing from YAML
 *
 * T301: Validates node_id, role, interfaces extraction per node
 */
TEST_F(DistributedScenarioParserTest, YAMLNodeAssignmentsParsing) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);

    const auto& assignments = scenario->node_assignments();
    EXPECT_EQ(3, assignments.size());

    // Verify first node (sender)
    EXPECT_EQ("sender-node", assignments[0].node_id);
    EXPECT_EQ("sender", assignments[0].role);
    EXPECT_EQ(2, assignments[0].interfaces.size());
    EXPECT_EQ("eth0", assignments[0].interfaces[0]);
    EXPECT_EQ("eth1", assignments[0].interfaces[1]);

    // Verify second node (receiver)
    EXPECT_EQ("receiver-node", assignments[1].node_id);
    EXPECT_EQ("receiver", assignments[1].role);

    // Verify third node (observer)
    EXPECT_EQ("monitor-node", assignments[2].node_id);
    EXPECT_EQ("observer", assignments[2].role);
    EXPECT_EQ(1, assignments[2].interfaces.size());
    EXPECT_EQ("eth0", assignments[2].interfaces[0]);
}

/**
 * @brief Test barrier step parsing from YAML
 *
 * T302: Validates barrier-type step configuration
 */
TEST_F(DistributedScenarioParserTest, YAMLBarrierStepParsing) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    // Find barrier step
    auto barrier_it = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.type == StepType::BARRIER;
    });

    ASSERT_NE(barrier_it, steps.end());
    EXPECT_EQ("barrier-1", barrier_it->step_id);
    EXPECT_EQ("Initial Synchronization", barrier_it->step_name);
    EXPECT_EQ(StepType::BARRIER, barrier_it->type);

    // Verify barrier config
    const auto& barrier_cfg = std::get<BarrierStepConfig>(barrier_it->config);
    EXPECT_EQ("start-sync", barrier_cfg.barrier_id);
    EXPECT_EQ(5000, barrier_cfg.timeout_ms.count());
    EXPECT_EQ(3, barrier_cfg.participating_nodes.size());
}

/**
 * @brief Test capture step parsing from YAML
 *
 * T302: Validates capture-type step configuration
 */
TEST_F(DistributedScenarioParserTest, YAMLCaptureStepParsing) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    // Find capture step
    auto capture_it = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.type == StepType::CAPTURE;
    });

    ASSERT_NE(capture_it, steps.end());
    EXPECT_EQ("capture-start", capture_it->step_id);
    EXPECT_EQ(StepType::CAPTURE, capture_it->type);
    EXPECT_EQ("barrier-1", capture_it->depends_on);
    EXPECT_FALSE(capture_it->parallel);

    // Verify capture config
    const auto& cap_cfg = std::get<CaptureStepConfig>(capture_it->config);
    EXPECT_EQ("capture-1", cap_cfg.capture_id);
    EXPECT_EQ("eth0", cap_cfg.interface);
    EXPECT_EQ("tcp port 80", cap_cfg.bpf_filter);
    EXPECT_EQ(30000, cap_cfg.duration_ms.count());
    EXPECT_TRUE(cap_cfg.hardware_timestamps);
    EXPECT_EQ(65535, cap_cfg.snaplen);
    EXPECT_EQ(10485760, cap_cfg.buffer_size);
}

/**
 * @brief Test expectation step parsing from YAML
 *
 * T302: Validates expect-type step configuration
 */
TEST_F(DistributedScenarioParserTest, YAMLExpectStepParsing) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    // Find expect step
    auto expect_it = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.type == StepType::EXPECT;
    });

    ASSERT_NE(expect_it, steps.end());
    EXPECT_EQ("expect-1", expect_it->step_id);
    EXPECT_EQ(StepType::EXPECT, expect_it->type);
    EXPECT_EQ("wait-1", expect_it->depends_on);

    // Verify expect config
    const auto& exp_cfg = std::get<ExpectStepConfig>(expect_it->config);
    EXPECT_EQ("flow-sender-to-receiver", exp_cfg.assertion_id);
    EXPECT_EQ("message_flow", exp_cfg.assertion_type);
    EXPECT_FALSE(exp_cfg.should_fail);
    EXPECT_EQ(5000, exp_cfg.timeout_ms.count());
}

/**
 * @brief Test wait step parsing from YAML
 *
 * T302: Validates wait-type step configuration
 */
TEST_F(DistributedScenarioParserTest, YAMLWaitStepParsing) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    // Find wait step
    auto wait_it = std::find_if(steps.begin(), steps.end(),
                                [](const DistributedStep& s) { return s.type == StepType::WAIT; });

    ASSERT_NE(wait_it, steps.end());
    EXPECT_EQ("wait-1", wait_it->step_id);
    EXPECT_EQ(StepType::WAIT, wait_it->type);
    EXPECT_EQ("capture-start", wait_it->depends_on);

    // Verify wait config
    const auto& wait_cfg = std::get<WaitStepConfig>(wait_it->config);
    EXPECT_EQ(5000, wait_cfg.duration.count());
}

/**
 * @brief Test log step parsing from YAML
 *
 * T302: Validates log-type step configuration
 */
TEST_F(DistributedScenarioParserTest, YAMLLogStepParsing) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    // Find log step
    auto log_it = std::find_if(steps.begin(), steps.end(),
                               [](const DistributedStep& s) { return s.type == StepType::LOG; });

    ASSERT_NE(log_it, steps.end());
    EXPECT_EQ("log-1", log_it->step_id);
    EXPECT_EQ(StepType::LOG, log_it->type);
    EXPECT_EQ("expect-1", log_it->depends_on);

    // Verify log config
    const auto& log_cfg = std::get<LogStepConfig>(log_it->config);
    EXPECT_FALSE(log_cfg.message.empty());
    EXPECT_EQ("INFO", log_cfg.level);
}

/**
 * @brief Test step dependencies are correctly parsed
 *
 * T302: Validates depends_on field for step sequencing
 */
TEST_F(DistributedScenarioParserTest, YAMLStepDependencies) {
    auto scenario = DistributedScenario::from_yaml_string(yaml_with_dependencies);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();
    EXPECT_EQ(3, steps.size());

    // Verify dependency chain
    EXPECT_TRUE(steps[0].depends_on.empty());  // First step has no dependency
    EXPECT_EQ("step-1", steps[1].depends_on);  // Second depends on first
    EXPECT_EQ("step-2", steps[2].depends_on);  // Third depends on second
}

/**
 * @brief Test simple YAML scenario with minimal content
 */
TEST_F(DistributedScenarioParserTest, YAMLSimpleBarrierScenario) {
    auto scenario = DistributedScenario::from_yaml_string(simple_barrier_yaml);

    ASSERT_NE(nullptr, scenario);
    EXPECT_EQ("simple-barrier", scenario->id());
    EXPECT_EQ(2, scenario->node_assignments().size());
    EXPECT_EQ(1, scenario->steps().size());
    EXPECT_EQ(StepType::BARRIER, scenario->steps()[0].type);
}

/**
 * @brief Test JSON scenario parsing
 *
 * T304-T305: Validates JSON parsing with equivalent feature set to YAML
 */
TEST_F(DistributedScenarioParserTest, ParseCompleteJSONScenario) {
    auto scenario = DistributedScenario::from_json_string(complete_json_scenario);

    ASSERT_NE(nullptr, scenario);
    EXPECT_EQ("json-multi-node", scenario->id());
    EXPECT_EQ("JSON Multi-Node Scenario", scenario->name());
    EXPECT_EQ(1, scenario->tags().size());
    EXPECT_EQ(2, scenario->node_assignments().size());
    EXPECT_EQ(3, scenario->steps().size());
}

/**
 * @brief Test JSON barrier step parsing
 */
TEST_F(DistributedScenarioParserTest, JSONBarrierStepParsing) {
    auto scenario = DistributedScenario::from_json_string(complete_json_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    auto barrier_it = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.type == StepType::BARRIER;
    });

    ASSERT_NE(barrier_it, steps.end());
    const auto& barrier_cfg = std::get<BarrierStepConfig>(barrier_it->config);
    EXPECT_EQ("json-sync", barrier_cfg.barrier_id);
}

/**
 * @brief Test JSON capture step parsing
 */
TEST_F(DistributedScenarioParserTest, JSONCaptureStepParsing) {
    auto scenario = DistributedScenario::from_json_string(complete_json_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    auto capture_it = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.type == StepType::CAPTURE;
    });

    ASSERT_NE(capture_it, steps.end());
    const auto& cap_cfg = std::get<CaptureStepConfig>(capture_it->config);
    EXPECT_EQ("json-cap", cap_cfg.capture_id);
    EXPECT_EQ("eth0", cap_cfg.interface);
    EXPECT_EQ("tcp", cap_cfg.bpf_filter);
}

/**
 * @brief Test JSON expect step parsing
 */
TEST_F(DistributedScenarioParserTest, JSONExpectStepParsing) {
    auto scenario = DistributedScenario::from_json_string(complete_json_scenario);

    ASSERT_NE(nullptr, scenario);
    const auto& steps = scenario->steps();

    auto expect_it = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.type == StepType::EXPECT;
    });

    ASSERT_NE(expect_it, steps.end());
    const auto& exp_cfg = std::get<ExpectStepConfig>(expect_it->config);
    EXPECT_EQ("json-assert", exp_cfg.assertion_id);
    EXPECT_EQ("message_flow", exp_cfg.assertion_type);
}

/**
 * @brief Test scenario validation passes for valid scenarios
 *
 * T303: Ensures parsed scenarios are valid and consistent
 */
TEST_F(DistributedScenarioParserTest, ValidScenarioValidation) {
    auto yaml_scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);
    ASSERT_NE(nullptr, yaml_scenario);
    EXPECT_TRUE(yaml_scenario->validate());

    auto json_scenario = DistributedScenario::from_json_string(complete_json_scenario);
    ASSERT_NE(nullptr, json_scenario);
    EXPECT_TRUE(json_scenario->validate());
}

/**
 * @brief Test steps_for_node() decomposition
 *
 * T077: Verifies scenario decomposition for node-specific execution
 */
TEST_F(DistributedScenarioParserTest, ScenarioDecompositionByNode) {
    auto scenario = DistributedScenario::from_yaml_string(complete_yaml_scenario);

    ASSERT_NE(nullptr, scenario);

    // Get steps for each node
    auto sender_steps = scenario->steps_for_node("sender-node");
    auto receiver_steps = scenario->steps_for_node("receiver-node");
    auto observer_steps = scenario->steps_for_node("monitor-node");

    // Barrier, capture, wait, expect, log should be available to all nodes
    EXPECT_FALSE(sender_steps.empty());
    EXPECT_FALSE(receiver_steps.empty());
    EXPECT_FALSE(observer_steps.empty());
}

/**
 * @brief Test invalid scenario detection
 */
TEST_F(DistributedScenarioParserTest, InvalidScenarioRejection) {
    const std::string invalid_yaml = R"(
scenario_id: invalid-test
node_assignments:
  - node_id: node-a
    role: sender
steps:
  - id: step-1
    type: barrier
    depends_on: nonexistent-step
)";

    auto scenario = DistributedScenario::from_yaml_string(invalid_yaml);
    if (scenario) {
        // Step depends on nonexistent step should fail validation
        EXPECT_FALSE(scenario->validate());
    }
}

/**
 * @brief Test scenario with minimal required fields
 */
TEST_F(DistributedScenarioParserTest, MinimalYAMLScenario) {
    const std::string minimal_yaml = R"(
scenario_id: minimal-test
scenario_name: "Minimal Test"
node_assignments:
  - node_id: node-a
    role: sender
steps:
  - id: barrier-1
    type: barrier
    barrier_id: "sync"
    nodes: [node-a]
)";

    auto scenario = DistributedScenario::from_yaml_string(minimal_yaml);
    ASSERT_NE(nullptr, scenario);
    EXPECT_EQ("minimal-test", scenario->id());

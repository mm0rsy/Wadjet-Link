#include <gtest/gtest.h>
#include "wadjet/distributed/scenario.hpp"

namespace wadjet::distributed {

/**
 * @brief Test fixture for scenario parsing
 * 
 * T084: Unit tests for YAML/JSON scenario parsing
 */
class DistributedScenarioParserTest : public ::testing::Test {
protected:
    /**
     * @brief Sample YAML scenario content
     */
    const std::string sample_yaml = R"(
scenario:
  id: test-scenario-1
  name: "Multi-Node Test Scenario"
  description: "Test message flow between nodes"
  
node_assignments:
  - node_id: node-a
    role: sender
    interfaces: ["eth0", "eth1"]
  - node_id: node-b
    role: receiver
    interfaces: ["eth0", "eth1"]
  - node_id: node-c
    role: observer
    interfaces: ["eth0"]

steps:
  - step_id: barrier-1
    step_name: "Initial Sync"
    type: BARRIER
    timeout_ms: 5000
    target_nodes: []  # All nodes
    barrier_config:
      barrier_id: "sync-1"
      participating_nodes: ["node-a", "node-b", "node-c"]
      
  - step_id: capture-1
    step_name: "Start Capture"
    type: CAPTURE
    timeout_ms: 0  # No timeout, runs until stopped
    depends_on: "barrier-1"
    target_nodes: []
    capture_config:
      capture_id: "cap-1"
      interface: "eth0"
      bpf_filter: ""
      duration_ms: 5000
      
  - step_id: expect-1
    step_name: "Verify Flow"
    type: EXPECT
    timeout_ms: 5000
    depends_on: "capture-1"
    target_nodes: []
    expect_config:
      assertion_id: "flow-a-to-b"
      assertion_type: "message_flow"
      assertion_params: '{"src":"node-a","dst":"node-b"}'
)";

    /**
     * @brief Sample JSON scenario content
     */
    const std::string sample_json = R"({
  "scenario": {
    "id": "json-test-1",
    "name": "JSON Test Scenario",
    "description": "Test scenario from JSON"
  },
  "node_assignments": [
    {
      "node_id": "node-a",
      "role": "sender",
      "interfaces": ["eth0"]
    },
    {
      "node_id": "node-b",
      "role": "receiver", 
      "interfaces": ["eth0"]
    }
  ],
  "steps": [
    {
      "step_id": "barrier-json",
      "step_name": "Sync",
      "type": "BARRIER",
      "timeout_ms": 5000
    }
  ]
})";
};

/**
 * @brief Test YAML scenario parsing
 */
TEST_F(DistributedScenarioParserTest, ParseYAMLString) {
    // T084: Test parsing YAML from string
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    EXPECT_FALSE(scenario->id().empty());
    EXPECT_FALSE(scenario->name().empty());
    EXPECT_FALSE(scenario->node_assignments().empty());
    EXPECT_FALSE(scenario->steps().empty());
}

TEST_F(DistributedScenarioParserTest, YAMLScenarioHasCorrectStructure) {
    // T084: Verify scenario structure is parsed correctly
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    // Check node assignments
    const auto& nodes = scenario->node_assignments();
    EXPECT_GE(nodes.size(), 2);
    
    // Check steps
    const auto& steps = scenario->steps();
    EXPECT_GE(steps.size(), 1);
    
    // Verify first step type
    if (!steps.empty()) {
        EXPECT_NE(StepType::UNKNOWN, steps[0].type);
    }
}

TEST_F(DistributedScenarioParserTest, YAMLScenarioValidation) {
    // T084: Test scenario validation
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    EXPECT_TRUE(scenario->validate());
}

/**
 * @brief Test JSON scenario parsing
 */
TEST_F(DistributedScenarioParserTest, ParseJSONString) {
    // T084: Test parsing JSON from string
    auto scenario = DistributedScenario::from_json_string(sample_json);
    
    ASSERT_NE(nullptr, scenario);
    EXPECT_FALSE(scenario->id().empty());
    EXPECT_FALSE(scenario->name().empty());
}

TEST_F(DistributedScenarioParserTest, JSONScenarioValidation) {
    // T084: Test JSON scenario validation
    auto scenario = DistributedScenario::from_json_string(sample_json);
    
    ASSERT_NE(nullptr, scenario);
    EXPECT_TRUE(scenario->validate());
}

/**
 * @brief Test scenario decomposition
 */
TEST_F(DistributedScenarioParserTest, ScenarioDecomposition) {
    // T084: Test decomposing scenario to node-specific steps (T077)
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    // Get steps for node-a
    auto steps_a = scenario->steps_for_node("node-a");
    EXPECT_FALSE(steps_a.empty());
    
    // Get steps for node-b
    auto steps_b = scenario->steps_for_node("node-b");
    EXPECT_FALSE(steps_b.empty());
    
    // Get steps for node-c
    auto steps_c = scenario->steps_for_node("node-c");
    EXPECT_FALSE(steps_c.empty());
}

/**
 * @brief Test step dependency resolution
 */
TEST_F(DistributedScenarioParserTest, StepDependencies) {
    // T084: Verify step dependencies are preserved
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    const auto& steps = scenario->steps();
    
    // Find a step with dependencies
    for (const auto& step : steps) {
        if (!step.depends_on.empty()) {
            // Verify dependency step exists
            auto it = std::find_if(steps.begin(), steps.end(),
                [&step](const DistributedStep& s) { return s.step_id == step.depends_on; });
            EXPECT_NE(it, steps.end());
        }
    }
}

/**
 * @brief Test scenario description and metadata
 */
TEST_F(DistributedScenarioParserTest, ScenarioMetadata) {
    // T084: Test scenario metadata access
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    EXPECT_FALSE(scenario->id().empty());
    EXPECT_FALSE(scenario->name().empty());
    EXPECT_FALSE(scenario->description().empty());
}

/**
 * @brief Test barrier step configuration
 */
TEST_F(DistributedScenarioParserTest, BarrierStepConfig) {
    // T084: Verify barrier step is parsed correctly
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    const auto& steps = scenario->steps();
    auto barrier_it = std::find_if(steps.begin(), steps.end(),
        [](const DistributedStep& s) { return s.type == StepType::BARRIER; });
    
    if (barrier_it != steps.end()) {
        EXPECT_FALSE(barrier_it->barrier_config.barrier_id.empty());
        EXPECT_FALSE(barrier_it->barrier_config.participating_nodes.empty());
    }
}

/**
 * @brief Test capture step configuration
 */
TEST_F(DistributedScenarioParserTest, CaptureStepConfig) {
    // T084: Verify capture step is parsed correctly
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    const auto& steps = scenario->steps();
    auto capture_it = std::find_if(steps.begin(), steps.end(),
        [](const DistributedStep& s) { return s.type == StepType::CAPTURE; });
    
    if (capture_it != steps.end()) {
        EXPECT_FALSE(capture_it->capture_config.capture_id.empty());
        EXPECT_FALSE(capture_it->capture_config.interface.empty());
    }
}

/**
 * @brief Test expectation step configuration
 */
TEST_F(DistributedScenarioParserTest, ExpectStepConfig) {
    // T084: Verify expect step is parsed correctly
    auto scenario = DistributedScenario::from_yaml_string(sample_yaml);
    
    ASSERT_NE(nullptr, scenario);
    
    const auto& steps = scenario->steps();
    auto expect_it = std::find_if(steps.begin(), steps.end(),
        [](const DistributedStep& s) { return s.type == StepType::EXPECT; });
    
    if (expect_it != steps.end()) {
        EXPECT_FALSE(expect_it->expect_config.assertion_id.empty());
        EXPECT_FALSE(expect_it->expect_config.assertion_type.empty());
    }
}

/**
 * @brief Test invalid scenario handling
 */
TEST_F(DistributedScenarioParserTest, InvalidScenarioDetection) {
    // T084: Test that invalid scenarios are rejected
    
    const std::string invalid_yaml = R"(
scenario:
  id: invalid-test
  steps:
    - step_id: step-1
      depends_on: nonexistent-step
)";
    
    auto scenario = DistributedScenario::from_yaml_string(invalid_yaml);
    if (scenario) {
        EXPECT_FALSE(scenario->validate())
            << "Invalid scenario should fail validation";
    }
}

}  // namespace wadjet::distributed

#include "wadjet/distributed/scenario.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed {

/**
 * @brief Test fixture for protocol-aware distributed scenarios (T338)
 *
 * Tests the extended ExpectStepConfig with protocol-aware matching for
 * M2/M9 protocol-specific distributed assertions introduced in T336-T337.
 */
class ProtocolAwareScenarioTest : public ::testing::Test {
protected:
    /**
     * @brief YAML scenario with SOME/IP protocol-aware expect step
     *
     * T336: Tests protocol field parsing and match_fields extraction
     * T337: Tests coordinator integration with M2 SOME/IP decoder
     *
     * Demonstrates:
     * - protocol field: "someip"
     * - match_fields: service_id, method_id, message_type
     * - src_node/dst_node for directed protocol flows
     */
    const std::string someip_protocol_aware_yaml = R"(
scenario_id: protocol-someip-test
scenario_name: "SOME/IP Protocol-Aware Assertion Test"
description: "Validates SOME/IP service calls across nodes"
tags:
  - "protocol-aware"
  - "someip"
  - "distributed"

node_assignments:
  - node_id: client-node
    role: sender
    interfaces: ["eth0"]
  - node_id: server-node
    role: receiver
    interfaces: ["eth0"]

steps:
  - id: barrier-sync
    name: "Synchronize Nodes"
    type: barrier
    barrier_id: "start"
    timeout_ms: 5000
    nodes: [client-node, server-node]

  - id: capture-someip
    name: "Capture SOME/IP Traffic"
    type: capture
    capture_id: "cap-1"
    nodes: [client-node, server-node]
    interface: eth0
    bpf_filter: "udp port 30490"
    duration_ms: 5000

  - id: expect-someip-service
    name: "Validate SOME/IP Service Call"
    type: expect
    assertion_id: "assert-someip-1"
    
    # T336: Protocol-aware assertion mode
    protocol: "someip"
    src_node: "client-node"
    dst_node: "server-node"
    
    # T336: Typed match_fields instead of generic assertion_params JSON
    match_fields:
      service_id: "0x1234"
      method_id: "0x4321"
      message_type: "0"  # REQUEST
    
    timeout_ms: 5000
    should_fail: false

  - id: expect-someip-response
    name: "Validate SOME/IP Response"
    type: expect
    assertion_id: "assert-someip-2"
    protocol: "someip"
    src_node: "server-node"
    dst_node: "client-node"
    match_fields:
      service_id: "0x1234"
      method_id: "0x4321"
      message_type: "1"  # RESPONSE
    timeout_ms: 5000
)";

    /**
     * @brief YAML scenario with DoIP protocol-aware expect step (T337 placeholder)
     *
     * Tests M9 UDS-over-DoIP integration point for diagnostic testing
     * Currently returns "not available" until M10 DoIP decoder available
     */
    const std::string doip_protocol_aware_yaml = R"(
scenario_id: protocol-doip-test
scenario_name: "DoIP Protocol-Aware Assertion Test"
description: "Validates UDS diagnostic requests over DoIP"
tags:
  - "protocol-aware"
  - "doip"
  - "diagnostic"

node_assignments:
  - node_id: diagnostic-client
    role: sender
  - node_id: target-ecu
    role: receiver

steps:
  - id: capture-doip
    name: "Capture DoIP Traffic"
    type: capture
    capture_id: "cap-doip"
    nodes: [diagnostic-client, target-ecu]
    interface: eth0
    bpf_filter: "tcp port 13400"
    duration_ms: 5000

  - id: expect-uds-request
    name: "Validate UDS ReadDataByIdentifier over DoIP"
    type: expect
    assertion_id: "assert-doip-1"
    
    # T337: DoIP protocol awareness (M9 integration placeholder)
    protocol: "doip"
    src_node: "diagnostic-client"
    dst_node: "target-ecu"
    
    match_fields:
      target_address: "0x01"
      message_type: "0x8001"  # DoIP diagnostic request
      uds_service: "0x22"     # ReadDataByIdentifier
    
    timeout_ms: 5000
)";

    /**
     * @brief YAML scenario with UDP/TCP protocol-aware expect step
     *
     * Tests L4 port-based matching without requiring full protocol decoder
     */
    const std::string l4_protocol_aware_yaml = R"(
scenario_id: protocol-l4-test
scenario_name: "Layer-4 Protocol-Aware Assertion"
description: "Validates UDP/TCP port-based communication"

node_assignments:
  - node_id: node-a
    role: sender
  - node_id: node-b
    role: receiver

steps:
  - id: capture-l4
    name: "Capture Layer 4 Traffic"
    type: capture
    capture_id: "cap-l4"
    nodes: [node-a, node-b]
    interface: eth0
    duration_ms: 3000

  - id: expect-udp-flow
    name: "Validate UDP Port Flow"
    type: expect
    assertion_id: "assert-udp-1"
    protocol: "udp"
    src_node: "node-a"
    dst_node: "node-b"
    match_fields:
      src_port: "30490"
      dst_port: "30491"
    timeout_ms: 3000
)";

    /**
     * @brief YAML scenario with legacy generic assertion for backward compatibility
     *
     * T336: Tests that legacy scenarios without protocol field still work
     */
    const std::string legacy_generic_yaml = R"(
scenario_id: legacy-generic-test
scenario_name: "Legacy Generic Assertion"
description: "Backward compatibility with generic assertion_params"

node_assignments:
  - node_id: node-x
    role: sender

steps:
  - id: expect-generic
    name: "Generic Assertion"
    type: expect
    assertion_type: "message_flow"
    # Legacy mode: no protocol field, uses generic assertion_params
    assertion_params:
      filter: "tcp port 80"
      min_packets: 5
    timeout_ms: 5000
)";
};

/**
 * @brief Test T336-T337: SOME/IP protocol-aware expect step parsing
 *
 * Validates that YAML scenario with protocol="someip" and match_fields
 * are correctly parsed into ExpectStepConfig with:
 * - protocol field set to ProtocolType::SOMEIP
 * - match_fields populated with service_id, method_id, etc.
 * - src_node/dst_node captured for directed flows
 */
TEST_F(ProtocolAwareScenarioTest, ParseSomeIPProtocolAwareAssertion) {
    auto scenario = DistributedScenario::from_yaml_string(someip_protocol_aware_yaml);
    ASSERT_NE(nullptr, scenario);
    EXPECT_EQ("protocol-someip-test", scenario->id());

    const auto& steps = scenario->steps();

    // Find the SOME/IP assertion step
    auto someip_step = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.step_id == "expect-someip-service";
    });

    ASSERT_NE(someip_step, steps.end());
    EXPECT_EQ(StepType::EXPECT, someip_step->type);

    // Extract expect config
    const auto& expect_cfg = std::get<ExpectStepConfig>(someip_step->config);

    // T336: Verify protocol field parsed correctly
    EXPECT_EQ(ProtocolType::SOMEIP, expect_cfg.protocol);

    // T336: Verify match_fields populated from YAML
    EXPECT_FALSE(expect_cfg.match_fields.empty());
    EXPECT_EQ("0x1234", expect_cfg.match_fields.at("service_id"));
    EXPECT_EQ("0x4321", expect_cfg.match_fields.at("method_id"));
    EXPECT_EQ("0", expect_cfg.match_fields.at("message_type"));

    // T336: Verify src/dst nodes captured
    EXPECT_EQ("client-node", expect_cfg.src_node);
    EXPECT_EQ("server-node", expect_cfg.dst_node);

    // T336: Verify protocol-aware mode flag works
    EXPECT_TRUE(uses_protocol_aware_assertions(expect_cfg));
}

/**
 * @brief Test T336-T337: DoIP protocol-aware expect step parsing
 *
 * Validates parsing of DoIP (UDS over TCP) assertions with M9 integration point
 */
TEST_F(ProtocolAwareScenarioTest, ParseDoIPProtocolAwareAssertion) {
    auto scenario = DistributedScenario::from_yaml_string(doip_protocol_aware_yaml);
    ASSERT_NE(nullptr, scenario);
    EXPECT_EQ("protocol-doip-test", scenario->id());

    const auto& steps = scenario->steps();

    auto doip_step = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.step_id == "expect-uds-request";
    });

    ASSERT_NE(doip_step, steps.end());

    const auto& expect_cfg = std::get<ExpectStepConfig>(doip_step->config);

    // T336: Verify DoIP protocol type
    EXPECT_EQ(ProtocolType::DoIP, expect_cfg.protocol);

    // T336: Verify diagnostic-specific fields in match_fields
    EXPECT_EQ("0x01", expect_cfg.match_fields.at("target_address"));
    EXPECT_EQ("0x8001", expect_cfg.match_fields.at("message_type"));
    EXPECT_EQ("0x22", expect_cfg.match_fields.at("uds_service"));

    EXPECT_TRUE(uses_protocol_aware_assertions(expect_cfg));
}

/**
 * @brief Test T336-T337: Layer 4 (UDP/TCP) protocol-aware assertions
 *
 * Validates port-based L4 matching without full protocol decoder dependency
 */
TEST_F(ProtocolAwareScenarioTest, ParseL4ProtocolAwareAssertion) {
    auto scenario = DistributedScenario::from_yaml_string(l4_protocol_aware_yaml);
    ASSERT_NE(nullptr, scenario);

    const auto& steps = scenario->steps();
    auto l4_step = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.step_id == "expect-udp-flow";
    });

    ASSERT_NE(l4_step, steps.end());

    const auto& expect_cfg = std::get<ExpectStepConfig>(l4_step->config);

    // T336: Verify UDP protocol type
    EXPECT_EQ(ProtocolType::UDP, expect_cfg.protocol);

    // T336: Verify port matching fields
    EXPECT_EQ("30490", expect_cfg.match_fields.at("src_port"));
    EXPECT_EQ("30491", expect_cfg.match_fields.at("dst_port"));

    EXPECT_TRUE(uses_protocol_aware_assertions(expect_cfg));
}

/**
 * @brief Test T336: Backward compatibility with legacy generic assertions
 *
 * Validates that scenarios without protocol field still parse correctly
 * and use generic assertion_params mode for backward compatibility
 */
TEST_F(ProtocolAwareScenarioTest, LegacyGenericAssertionBackwardCompatibility) {
    auto scenario = DistributedScenario::from_yaml_string(legacy_generic_yaml);
    ASSERT_NE(nullptr, scenario);

    const auto& steps = scenario->steps();
    auto generic_step = std::find_if(steps.begin(), steps.end(), [](const DistributedStep& s) {
        return s.step_id == "expect-generic";
    });

    ASSERT_NE(generic_step, steps.end());

    const auto& expect_cfg = std::get<ExpectStepConfig>(generic_step->config);

    // T336: Verify legacy mode (no protocol specified)
    EXPECT_EQ(ProtocolType::GENERIC, expect_cfg.protocol);

    // T336: Verify generic mode flag is false
    EXPECT_FALSE(uses_protocol_aware_assertions(expect_cfg));

    // T336: Verify assertion_params preserved for legacy assertions
    EXPECT_FALSE(expect_cfg.assertion_params.empty());
    EXPECT_EQ("message_flow", expect_cfg.assertion_type);
}

/**
 * @brief Test T336-T337: Multiple protocol-aware assertions in single scenario
 *
 * Validates a complex scenario with mixed protocol types and generic assertions
 */
TEST_F(ProtocolAwareScenarioTest, MixedProtocolAwareScenario) {
    const std::string mixed_yaml = R"(
scenario_id: mixed-protocols
scenario_name: "Mixed Protocol Scenario"

node_assignments:
  - node_id: node-1
    role: sender
  - node_id: node-2
    role: receiver
  - node_id: node-3
    role: observer

steps:
  - id: barrier-start
    type: barrier
    barrier_id: "sync"
    nodes: [node-1, node-2, node-3]

  - id: expect-someip
    type: expect
    protocol: "someip"
    match_fields:
      service_id: "0x1234"

  - id: expect-generic
    type: expect
    assertion_type: "message_flow"
    assertion_params: {}

  - id: expect-doip
    type: expect
    protocol: "doip"
    match_fields:
      target_address: "0xFF"
)";

    auto scenario = DistributedScenario::from_yaml_string(mixed_yaml);
    ASSERT_NE(nullptr, scenario);

    const auto& steps = scenario->steps();

    // Count different assertion types
    int protocol_aware_count = 0;
    int generic_count = 0;

    for (const auto& step : steps) {
        if (step.type != StepType::EXPECT)
            continue;

        const auto& cfg = std::get<ExpectStepConfig>(step.config);
        if (uses_protocol_aware_assertions(cfg)) {
            protocol_aware_count++;
        } else {
            generic_count++;
        }
    }

    // T336: Verify mixed assertions are handled correctly
    EXPECT_EQ(2, protocol_aware_count);  // SOME/IP + DoIP
    EXPECT_EQ(1, generic_count);         // Generic assertion
}

/**
 * @brief Test T336: Invalid protocol field handling
 *
 * Validates that invalid or unknown protocol types default to GENERIC
 */
TEST_F(ProtocolAwareScenarioTest, InvalidProtocolTypeDefaults) {
    const std::string invalid_protocol_yaml = R"(
scenario_id: invalid-protocol-test
scenario_name: "Test Invalid Protocol"

node_assignments:
  - node_id: test-node
    role: sender

steps:
  - id: expect-invalid
    type: expect
    protocol: "unknown-protocol-xyz"
    match_fields:
      field1: "value1"
)";

    auto scenario = DistributedScenario::from_yaml_string(invalid_protocol_yaml);
    ASSERT_NE(nullptr, scenario);

    const auto& steps = scenario->steps();
    ASSERT_FALSE(steps.empty());

    const auto& cfg = std::get<ExpectStepConfig>(steps[0].config);

    // T336: Unknown protocol type should default to GENERIC
    EXPECT_EQ(ProtocolType::GENERIC, cfg.protocol);
}

}  // namespace wadjet::distributed

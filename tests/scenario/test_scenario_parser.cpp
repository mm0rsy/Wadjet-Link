/// @file test_scenario_parser.cpp
/// @brief Unit tests for scenario YAML and JSON parsers

#include "wadjet/scenario/parser.hpp"
#include "wadjet/scenario/scenario_types.hpp"

#include <gtest/gtest.h>

using namespace wadjet::scenario;

// =============================================================================
// CountExpression Tests
// =============================================================================

class CountExpressionTest : public ::testing::Test {};

TEST_F(CountExpressionTest, ParseSimpleNumber) {
    auto expr = CountExpression::parse("5");
    ASSERT_TRUE(expr.has_value());
    EXPECT_EQ(expr->op, CompareOp::GreaterEqual);
    EXPECT_EQ(expr->value, 5);
}

TEST_F(CountExpressionTest, ParseGreaterEqual) {
    auto expr = CountExpression::parse(">= 3");
    ASSERT_TRUE(expr.has_value());
    EXPECT_EQ(expr->op, CompareOp::GreaterEqual);
    EXPECT_EQ(expr->value, 3);
}

TEST_F(CountExpressionTest, ParseEqual) {
    auto expr = CountExpression::parse("== 10");
    ASSERT_TRUE(expr.has_value());
    EXPECT_EQ(expr->op, CompareOp::Equal);
    EXPECT_EQ(expr->value, 10);
}

TEST_F(CountExpressionTest, ParseLessThan) {
    auto expr = CountExpression::parse("< 100");
    ASSERT_TRUE(expr.has_value());
    EXPECT_EQ(expr->op, CompareOp::LessThan);
    EXPECT_EQ(expr->value, 100);
}

TEST_F(CountExpressionTest, ParseNotEqual) {
    auto expr = CountExpression::parse("!= 0");
    ASSERT_TRUE(expr.has_value());
    EXPECT_EQ(expr->op, CompareOp::NotEqual);
    EXPECT_EQ(expr->value, 0);
}

TEST_F(CountExpressionTest, EvaluateGreaterEqual) {
    CountExpression expr{CompareOp::GreaterEqual, 3};
    EXPECT_FALSE(expr.evaluate(2));
    EXPECT_TRUE(expr.evaluate(3));
    EXPECT_TRUE(expr.evaluate(4));
}

TEST_F(CountExpressionTest, EvaluateEqual) {
    CountExpression expr{CompareOp::Equal, 5};
    EXPECT_FALSE(expr.evaluate(4));
    EXPECT_TRUE(expr.evaluate(5));
    EXPECT_FALSE(expr.evaluate(6));
}

TEST_F(CountExpressionTest, EvaluateLessThan) {
    CountExpression expr{CompareOp::LessThan, 10};
    EXPECT_TRUE(expr.evaluate(5));
    EXPECT_FALSE(expr.evaluate(10));
    EXPECT_FALSE(expr.evaluate(15));
}

// =============================================================================
// YAML Parser Tests
// =============================================================================

class YamlParserTest : public ::testing::Test {};

TEST_F(YamlParserTest, ParseMinimalScenario) {
    const char* yaml = R"(
name: Test Scenario
steps: []
)";

    auto result = parse_yaml(yaml);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->name, "Test Scenario");
    EXPECT_TRUE(result->steps.empty());
}

TEST_F(YamlParserTest, ParseScenarioWithMetadata) {
    const char* yaml = R"(
name: Full Test
description: A comprehensive test scenario
version: "2.0"
timeout: 60s
tags:
  - smoke
  - integration
steps: []
)";

    auto result = parse_yaml(yaml);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->name, "Full Test");
    EXPECT_EQ(result->description, "A comprehensive test scenario");
    EXPECT_EQ(result->version, "2.0");
    EXPECT_EQ(result->timeout, Duration{60000});
    ASSERT_EQ(result->tags.size(), 2);
    EXPECT_EQ(result->tags[0], "smoke");
    EXPECT_EQ(result->tags[1], "integration");
}

TEST_F(YamlParserTest, ParseCaptureStep) {
    const char* yaml = R"(
name: Capture Test
steps:
  - capture:
      interface: eth0
      filter: "udp port 30490"
      timeout: 5s
      promiscuous: true
)";

    auto result = parse_yaml(yaml);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result->steps.size(), 1);

    auto* capture = std::get_if<CaptureStep>(&result->steps[0]);
    ASSERT_NE(capture, nullptr);
    EXPECT_EQ(capture->config.interface, "eth0");
    EXPECT_EQ(capture->config.filter, "udp port 30490");
    EXPECT_EQ(capture->config.timeout, Duration{5000});
    EXPECT_TRUE(capture->config.promiscuous);
}

TEST_F(YamlParserTest, ParseWaitStep) {
    const char* yaml = R"(
name: Wait Test
steps:
  - wait: 500ms
)";

    auto result = parse_yaml(yaml);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result->steps.size(), 1);

    auto* wait = std::get_if<WaitStep>(&result->steps[0]);
    ASSERT_NE(wait, nullptr);
    EXPECT_EQ(wait->duration, Duration{500});
}

TEST_F(YamlParserTest, ParseExpectStepWithSomeIP) {
    const char* yaml = R"(
name: SOME/IP Test
steps:
  - expect:
      description: "Check service offer"
      someip:
        service: 0x1234
        method: 0x0001
        type: request
      within: 2s
      count: ">= 1"
)";

    auto result = parse_yaml(yaml);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result->steps.size(), 1);

    auto* expect = std::get_if<ExpectStep>(&result->steps[0]);
    ASSERT_NE(expect, nullptr);
    EXPECT_EQ(expect->description, "Check service offer");
    EXPECT_EQ(expect->within, Duration{2000});

    ASSERT_TRUE(expect->someip.has_value());
    EXPECT_EQ(expect->someip->service_id, 0x1234);
    EXPECT_EQ(expect->someip->method_id, 0x0001);
    EXPECT_EQ(expect->someip->message_type, SomeIpMessageTypeExpect::Request);

    EXPECT_EQ(expect->count.op, CompareOp::GreaterEqual);
    EXPECT_EQ(expect->count.value, 1);
}

TEST_F(YamlParserTest, ParseInvalidYaml) {
    const char* yaml = R"(
name: [invalid
  not: valid: yaml
)";

    auto result = parse_yaml(yaml);
    EXPECT_TRUE(result.is_err());
}

// =============================================================================
// JSON Parser Tests
// =============================================================================

class JsonParserTest : public ::testing::Test {};

TEST_F(JsonParserTest, ParseMinimalScenario) {
    const char* json = R"({
        "name": "Test Scenario",
        "steps": []
    })";

    auto result = parse_json(json);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->name, "Test Scenario");
    EXPECT_TRUE(result->steps.empty());
}

TEST_F(JsonParserTest, ParseScenarioWithMetadata) {
    const char* json = R"({
        "name": "Full Test",
        "description": "A comprehensive test scenario",
        "version": "2.0",
        "timeout": 60000,
        "tags": ["smoke", "integration"],
        "steps": []
    })";

    auto result = parse_json(json);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->name, "Full Test");
    EXPECT_EQ(result->description, "A comprehensive test scenario");
    EXPECT_EQ(result->version, "2.0");
    EXPECT_EQ(result->timeout, Duration{60000});
    ASSERT_EQ(result->tags.size(), 2);
}

TEST_F(JsonParserTest, ParseCaptureStep) {
    const char* json = R"({
        "name": "Capture Test",
        "steps": [
            {
                "capture": {
                    "interface": "eth0",
                    "filter": "tcp port 13400"
                }
            }
        ]
    })";

    auto result = parse_json(json);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result->steps.size(), 1);

    auto* capture = std::get_if<CaptureStep>(&result->steps[0]);
    ASSERT_NE(capture, nullptr);
    EXPECT_EQ(capture->config.interface, "eth0");
    EXPECT_EQ(capture->config.filter, "tcp port 13400");
}

TEST_F(JsonParserTest, ParseExpectStepWithUDP) {
    const char* json = R"({
        "name": "UDP Test",
        "steps": [
            {
                "expect": {
                    "udp": {
                        "dst_port": 30490
                    },
                    "within_ms": 1000,
                    "count": ">= 5"
                }
            }
        ]
    })";

    auto result = parse_json(json);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result->steps.size(), 1);

    auto* expect = std::get_if<ExpectStep>(&result->steps[0]);
    ASSERT_NE(expect, nullptr);
    ASSERT_TRUE(expect->udp.has_value());
    EXPECT_EQ(expect->udp->dst_port, 30490);
    EXPECT_EQ(expect->within, Duration{1000});
    EXPECT_EQ(expect->count.op, CompareOp::GreaterEqual);
    EXPECT_EQ(expect->count.value, 5);
}

TEST_F(JsonParserTest, ParseInvalidJson) {
    const char* json = R"({ invalid json })";

    auto result = parse_json(json);
    EXPECT_TRUE(result.is_err());
}

// =============================================================================
// Scenario Helpers Tests
// =============================================================================

class ScenarioHelpersTest : public ::testing::Test {};

TEST_F(ScenarioHelpersTest, HasExpectations) {
    Scenario scenario;
    scenario.name = "Test";

    EXPECT_FALSE(scenario.has_expectations());

    scenario.steps.push_back(CaptureStep{});
    EXPECT_FALSE(scenario.has_expectations());

    scenario.steps.push_back(ExpectStep{});
    EXPECT_TRUE(scenario.has_expectations());
}

TEST_F(ScenarioHelpersTest, GetExpectations) {
    Scenario scenario;
    scenario.name = "Test";
    scenario.steps.push_back(CaptureStep{});
    scenario.steps.push_back(ExpectStep{});
    scenario.steps.push_back(WaitStep{Duration{100}});
    scenario.steps.push_back(ExpectStep{});

    auto expectations = scenario.get_expectations();
    EXPECT_EQ(expectations.size(), 2);
}

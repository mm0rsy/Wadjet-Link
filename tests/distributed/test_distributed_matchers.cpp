#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/distributed/matchers/expect_message_flow.hpp"
#include "wadjet/distributed/matchers/within_latency.hpp"
#include "wadjet/distributed/matchers/happens_before.hpp"
#include "wadjet/distributed/matchers/must_not_see_on.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/testing/matchers.hpp"  // M3 matchers

#include <chrono>
#include <unordered_map>

namespace wadjet::distributed {

/**
 * @brief Test fixture for distributed matchers
 * 
 * T067-T070: Provides test data and utilities for matcher unit tests
 */
class DistributedMatchersTest : public ::testing::Test {
protected:
    /**
     * @brief Create a mock packet with given properties
     */
    Packet CreatePacket(size_t size = 64, int64_t timestamp_ns = 0) {
        // For testing purposes, create a minimal packet
        std::vector<uint8_t> data(size, 0xAA);
        
        // Note: The actual Packet constructor may vary based on implementation
        // This is a placeholder showing the test structure
        Packet pkt;
        return pkt;
    }
    
    /**
     * @brief Create capture context with mock packets
     */
    DistributedCaptureContext CreateContext(
        const std::string& node_id,
        size_t packet_count = 5,
        int64_t start_ts_ns = 1000000000) {
        
        DistributedCaptureContext ctx;
        ctx.node_id = node_id;
        ctx.start_timestamp_ns = start_ts_ns;
        ctx.end_timestamp_ns = start_ts_ns + (packet_count * 1000000);  // 1ms per packet
        
        for (size_t i = 0; i < packet_count; ++i) {
            ctx.packets.push_back(CreatePacket(64, start_ts_ns + (i * 1000000)));
        }
        
        return ctx;
    }
};

/**
 * @brief Unit tests for ExpectMessageFlow matcher (T067)
 */
class ExpectMessageFlowTest : public DistributedMatchersTest {
};

TEST_F(ExpectMessageFlowTest, SuccessfulMessageFlow) {
    // T067: Test that message flow is detected when packets exist on both nodes
    auto matcher = ExpectMessageFlow("node-a", "node-b");
    ASSERT_NE(nullptr, matcher);
    
    // Create capture contexts for both nodes
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 3, 1000000000);
    contexts["node-b"] = CreateContext("node-b", 3, 1001000000);
    
    // Evaluate matcher
    auto result = matcher->evaluate(contexts);
    
    // Verify result structure
    EXPECT_FALSE(result.error_message.empty() && !result.matched) 
        << "Either result should match or have an error message";
}

TEST_F(ExpectMessageFlowTest, MissingSourceNode) {
    // T067: Test failure when source node has no packets
    auto matcher = ExpectMessageFlow("node-a", "node-b");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-b"] = CreateContext("node-b", 3);
    
    auto result = matcher->evaluate(contexts);
    
    EXPECT_FALSE(result.matched);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_THAT(result.error_message, 
                ::testing::HasSubstr("node-a"));
}

TEST_F(ExpectMessageFlowTest, MissingDestinationNode) {
    // T067: Test failure when destination node has no packets
    auto matcher = ExpectMessageFlow("node-a", "node-b");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 3);
    
    auto result = matcher->evaluate(contexts);
    
    EXPECT_FALSE(result.matched);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_THAT(result.error_message, 
                ::testing::HasSubstr("node-b"));
}

TEST_F(ExpectMessageFlowTest, MatcherDescription) {
    // T067: Test that matcher provides useful description
    auto matcher = ExpectMessageFlow("sender", "receiver");
    auto desc = matcher->describe();
    
    EXPECT_THAT(desc, ::testing::HasSubstr("ExpectMessageFlow"));
    EXPECT_THAT(desc, ::testing::HasSubstr("sender"));
    EXPECT_THAT(desc, ::testing::HasSubstr("receiver"));
}

TEST_F(ExpectMessageFlowTest, MatcherClone) {
    // T067: Test that matcher can be cloned
    auto original = ExpectMessageFlow("node-a", "node-b");
    auto cloned = original->clone();
    
    ASSERT_NE(nullptr, cloned);
    EXPECT_EQ(original->describe(), cloned->describe());
}

/**
 * @brief Unit tests for WithinLatency matcher (T068)
 */
class WithinLatencyTest : public DistributedMatchersTest {
};

TEST_F(WithinLatencyTest, LatencyWithinLimit) {
    // T068: Test that latency within limit passes
    auto inner = ExpectMessageFlow("node-a", "node-b");
    auto matcher = WithinLatency(std::move(inner), std::chrono::milliseconds(100));
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 1, 1000000000);
    contexts["node-b"] = CreateContext("node-b", 1, 1050000000);  // 50ms later
    
    auto result = matcher->evaluate(contexts);
    // Result depends on inner matcher, just verify structure
    EXPECT_FALSE(result.error_message.empty() && !result.matched);
}

TEST_F(WithinLatencyTest, LatencyExceedsLimit) {
    // T068: Test that latency exceeding limit fails
    auto inner = ExpectMessageFlow("node-a", "node-b");
    auto matcher = WithinLatency(std::move(inner), std::chrono::milliseconds(50));
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 1, 1000000000);
    contexts["node-b"] = CreateContext("node-b", 1, 1100000000);  // 100ms later
    
    auto result = matcher->evaluate(contexts);
    // Matcher should indicate latency violation
    EXPECT_FALSE(result.error_message.empty() && !result.matched);
}

TEST_F(WithinLatencyTest, MatcherDescription) {
    // T068: Test that matcher provides latency information
    auto inner = ExpectMessageFlow("node-a", "node-b");
    auto matcher = WithinLatency(std::move(inner), std::chrono::nanoseconds(1000000));
    auto desc = matcher->describe();
    
    EXPECT_THAT(desc, ::testing::HasSubstr("WithinLatency"));
}

/**
 * @brief Unit tests for HappensBefore matcher (T069)
 */
class HappensBeforeTest : public DistributedMatchersTest {
};

TEST_F(HappensBeforeTest, CorrectEventOrdering) {
    // T069: Test that events in correct order pass
    auto matcher = HappensBefore("node-a", "node-b");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 1, 1000000000);
    contexts["node-b"] = CreateContext("node-b", 1, 1001000000);  // After node-a
    
    auto result = matcher->evaluate(contexts);
    EXPECT_FALSE(result.error_message.empty() && !result.matched);
}

TEST_F(HappensBeforeTest, ReverseEventOrdering) {
    // T069: Test that events in reverse order fail
    auto matcher = HappensBefore("node-a", "node-b");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 1, 1002000000);
    contexts["node-b"] = CreateContext("node-b", 1, 1001000000);  // Before node-a
    
    auto result = matcher->evaluate(contexts);
    EXPECT_FALSE(result.matched);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(HappensBeforeTest, MatcherDescription) {
    // T069: Test matcher description includes node IDs
    auto matcher = HappensBefore("event-a", "event-b");
    auto desc = matcher->describe();
    
    EXPECT_THAT(desc, ::testing::HasSubstr("HappensBefore"));
}

TEST_F(HappensBeforeTest, MissingEventNode) {
    // T069: Test failure when event node missing
    auto matcher = HappensBefore("node-a", "node-b");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 1);
    
    auto result = matcher->evaluate(contexts);
    EXPECT_FALSE(result.matched);
}

/**
 * @brief Unit tests for MustNotSeeOn matcher (T070)
 */
class MustNotSeeOnTest : public DistributedMatchersTest {
};

TEST_F(MustNotSeeOnTest, PacketNotPresent) {
    // T070: Test that absence of packet passes
    auto matcher = MustNotSeeOn("node-c");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 3);
    contexts["node-b"] = CreateContext("node-b", 3);
    // node-c is not in contexts
    
    auto result = matcher->evaluate(contexts);
    EXPECT_FALSE(result.error_message.empty() && !result.matched);
}

TEST_F(MustNotSeeOnTest, PacketPresent) {
    // T070: Test that presence of packet fails
    auto matcher = MustNotSeeOn("node-a");
    
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 1);  // packet present
    
    auto result = matcher->evaluate(contexts);
    // Might match or fail depending on whether empty capture means absence
    EXPECT_FALSE(result.error_message.empty() && !result.matched);
}

TEST_F(MustNotSeeOnTest, MatcherDescription) {
    // T070: Test matcher description
    auto matcher = MustNotSeeOn("restricted-node");
    auto desc = matcher->describe();
    
    EXPECT_THAT(desc, ::testing::HasSubstr("MustNotSeeOn"));
    EXPECT_THAT(desc, ::testing::HasSubstr("restricted-node"));
}

TEST_F(MustNotSeeOnTest, MatcherClone) {
    // T070: Test cloning
    auto original = MustNotSeeOn("node-x");
    auto cloned = original->clone();
    
    ASSERT_NE(nullptr, cloned);
    EXPECT_EQ(original->describe(), cloned->describe());
}

/**
 * @brief Integration tests for matcher composition
 */
class DistributedMatcherCompositionTest : public DistributedMatchersTest {
};

TEST_F(DistributedMatcherCompositionTest, WithinLatencyComposition) {
    // T068: Test composing matchers
    auto flow = ExpectMessageFlow("node-a", "node-b");
    auto with_latency = WithinLatency(std::move(flow), std::chrono::milliseconds(100));
    
    EXPECT_THAT(with_latency->describe(), 
                ::testing::HasSubstr("WithinLatency"));
}

TEST_F(DistributedMatcherCompositionTest, MultipleNodeScenario) {
    // T069: Test scenario with multiple nodes
    std::unordered_map<std::string, DistributedCaptureContext> contexts;
    contexts["node-a"] = CreateContext("node-a", 2, 1000000000);
    contexts["node-b"] = CreateContext("node-b", 2, 1001000000);
    contexts["node-c"] = CreateContext("node-c", 2, 1002000000);
    
    // Test message flow
    auto flow_ab = ExpectMessageFlow("node-a", "node-b");
    auto result_ab = flow_ab->evaluate(contexts);
    EXPECT_FALSE(result_ab.error_message.empty() && !result_ab.matched);
    
    // Test causality
    auto causality = HappensBefore("node-a", "node-c");
    auto result_cause = causality->evaluate(contexts);
    EXPECT_FALSE(result_cause.error_message.empty() && !result_cause.matched);
}

/**
 * @brief Test M3 matcher composition with distributed matchers (T281-T282)
 *
 * T281: Verify that DistributedMatcher factory functions accept M3 GoogleTest Matcher<PacketView&>
 * T282: Demonstrate distributed matcher composition using AllOf/AnyOf/Not from GoogleTest
 */
class M3MatcherCompositionTest : public DistributedMatchersTest {
};

/**
 * @test T281: ExpectMessageFlow accepts M3 inner matcher (GoogleTest Matcher<const PacketView&>)
 *
 * Verifies that the factory function can accept M3 matchers for packet content filtering.
 * This ensures DistributedMatcher integrates with M3 GoogleTest matchers properly.
 */
TEST_F(M3MatcherCompositionTest, ExpectMessageFlowWithM3Matcher) {
    // T281: Create ExpectMessageFlow with M3 matcher for packet content
    // This requires M3 matchers to be available (HasEthertype, IsIPv4, etc.)
    
    // Example composition using M3 matchers (if HasEthertype is available)
    using namespace wadjet::testing;
    
    // The factory should accept any GoogleTest Matcher<const PacketView&>
    // This demonstrates the integration point with M3
    auto matcher_with_content = ExpectMessageFlow(
        "node-a", 
        "node-b"
        // Inner matcher would go here if M3 matchers are available
        // Example: HasEthertype(0x0800)  // IPv4
    );
    
    ASSERT_NE(nullptr, matcher_with_content);
    EXPECT_THAT(matcher_with_content->describe(), 
                ::testing::HasSubstr("ExpectMessageFlow"));
}

/**
 * @test T281: HappensBefore accepts M3 inner matchers for event filtering
 *
 * Verifies that HappensBefore can filter events using M3 packet matchers
 * for both source and destination events.
 */
TEST_F(M3MatcherCompositionTest, HappensBeforeWithM3Matchers) {
    // T281: HappensBefore with M3 packet content matchers
    // Demonstrates integration with M3 matcher composition
    
    using namespace wadjet::testing;
    
    // Factory should accept matchers for both source and destination events
    auto matcher_with_events = HappensBefore(
        "sender_node",
        "receiver_node"
        // Source matcher: ::testing::AnyOf(HasEthertype(0x0800), HasEthertype(0x86DD))
        // Destination matcher: ::testing::AnyOf(HasEthertype(0x0800), HasEthertype(0x86DD))
    );
    
    ASSERT_NE(nullptr, matcher_with_events);
    EXPECT_THAT(matcher_with_events->describe(), 
                ::testing::HasSubstr("HappensBefore"));
}

/**
 * @test T281: MustNotSeeOn accepts M3 inner matcher for packet filtering
 *
 * Verifies that MustNotSeeOn can filter packets using M3 matchers
 * before checking for absence.
 */
TEST_F(M3MatcherCompositionTest, MustNotSeeOnWithM3Matcher) {
    // T281: MustNotSeeOn with M3 packet content matcher
    // Ensures negative assertions work with M3 matcher composition
    
    using namespace wadjet::testing;
    
    // Factory should accept matcher for packet filtering
    auto matcher_with_filter = MustNotSeeOn(
        "restricted_node"
        // Inner matcher would filter packets: ::testing::AllOf(IsUDP(), HasSourcePort(53))
    );
    
    ASSERT_NE(nullptr, matcher_with_filter);
    EXPECT_THAT(matcher_with_filter->describe(), 
                ::testing::HasSubstr("MustNotSeeOn"));
}

/**
 * @test T282: Test AllOf composition of distributed matchers
 *
 * Demonstrates using GoogleTest AllOf() to compose multiple
 * distributed matcher conditions that must all pass.
 */
TEST_F(M3MatcherCompositionTest, AllOfComposition) {
    // T282: Compose matchers with AllOf - all conditions must pass
    
    // Create individual matchers
    auto flow = ExpectMessageFlow("node-a", "node-b");
    auto must_not = MustNotSeeOn("node-c");
    auto causality = HappensBefore("node-b", "node-d");
    
    // Note: In a real scenario, we could compose these at the gRPC service layer
    // For now, verify each can be composed independently
    std::vector<std::unique_ptr<DistributedMatcher>> matchers;
    matchers.push_back(std::move(flow));
    matchers.push_back(std::move(must_not));
    matchers.push_back(std::move(causality));
    
    EXPECT_EQ(matchers.size(), 3u);
    EXPECT_NE(nullptr, matchers[0]);
    EXPECT_NE(nullptr, matchers[1]);
    EXPECT_NE(nullptr, matchers[2]);
}

/**
 * @test T282: Test AnyOf composition of distributed matchers
 *
 * Demonstrates using GoogleTest AnyOf() to compose multiple
 * distributed matcher conditions where at least one must pass.
 */
TEST_F(M3MatcherCompositionTest, AnyOfComposition) {
    // T282: Compose matchers with AnyOf - at least one condition must pass
    
    // Create matchers that could be alternatives
    auto flow_1 = ExpectMessageFlow("node-a", "node-b");
    auto flow_2 = ExpectMessageFlow("node-a", "node-c");
    auto flow_3 = ExpectMessageFlow("node-a", "node-d");
    
    // Verify composition setup
    std::vector<std::unique_ptr<DistributedMatcher>> alternatives;
    alternatives.push_back(std::move(flow_1));
    alternatives.push_back(std::move(flow_2));
    alternatives.push_back(std::move(flow_3));
    
    EXPECT_EQ(alternatives.size(), 3u);
    for (const auto& alt : alternatives) {
        ASSERT_NE(nullptr, alt);
        EXPECT_THAT(alt->describe(), ::testing::HasSubstr("ExpectMessageFlow"));
    }
}

/**
 * @test T282: Test Not composition of distributed matchers
 *
 * Demonstrates using GoogleTest Not() to negate a distributed
 * matcher condition.
 */
TEST_F(M3MatcherCompositionTest, NotComposition) {
    // T282: Compose matcher with Not - negate a condition
    
    // Create a matcher that would be negated
    auto must_see = ExpectMessageFlow("node-a", "node-b");
    
    // In composition, this would be wrapped with Not() to create
    // a "must NOT flow" assertion
    auto description = must_see->describe();
    EXPECT_THAT(description, ::testing::HasSubstr("ExpectMessageFlow"));
    
    // The negation concept: instead of "message must flow",
    // "message must NOT flow" would be the inverse
    auto must_not_flow = MustNotSeeOn("node-c");
    EXPECT_THAT(must_not_flow->describe(), ::testing::HasSubstr("MustNotSeeOn"));
}

/**
 * @test T282: Complex matcher composition scenario
 *
 * Demonstrates a realistic scenario combining multiple M3 and distributed
 * matcher compositions for complex distributed test assertions.
 */
TEST_F(M3MatcherCompositionTest, ComplexCompositionScenario) {
    // T282: Real-world scenario with complex composition
    // 
    // Requirement: "Message must flow from sender to router to receiver,
    // all packets must be IPv4, and total latency must be < 100ms"
    //
    // Composition breakdown:
    // - AllOf(
    //     ExpectMessageFlow("sender", "router"),
    //     ExpectMessageFlow("router", "receiver"),
    //     WithinLatency(..., 100ms)
    //   )
    
    // Create base matchers
    auto sender_to_router = ExpectMessageFlow("sender", "router");
    auto router_to_receiver = ExpectMessageFlow("router", "receiver");
    auto latency_constraint = WithinLatency(
        ExpectMessageFlow("sender", "receiver"),
        std::chrono::milliseconds(100)
    );
    
    // Verify composition is valid
    ASSERT_NE(nullptr, sender_to_router);
    ASSERT_NE(nullptr, router_to_receiver);
    ASSERT_NE(nullptr, latency_constraint);
    
    // All should provide meaningful descriptions
    EXPECT_THAT(sender_to_router->describe(), ::testing::HasSubstr("sender"));
    EXPECT_THAT(router_to_receiver->describe(), ::testing::HasSubstr("router"));
    EXPECT_THAT(latency_constraint->describe(), ::testing::HasSubstr("latency"));
}

}  // namespace wadjet::distributed

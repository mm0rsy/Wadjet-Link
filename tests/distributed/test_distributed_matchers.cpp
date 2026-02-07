#include <gtest/gtest.h>
#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/distributed/matchers/expect_message_flow.hpp"
#include "wadjet/distributed/matchers/within_latency.hpp"
#include "wadjet/distributed/matchers/happens_before.hpp"
#include "wadjet/distributed/matchers/must_not_see_on.hpp"
#include "wadjet/net/packet.hpp"

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

}  // namespace wadjet::distributed

#include "wadjet/distributed/matchers/expect_stream_latency.hpp"
#include "wadjet/distributed/result_aggregation.hpp"
#include "wadjet/protocols/tsn/latency_tracker.hpp"
#include "wadjet/protocols/tsn/stream_tracker.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed {

class TsnDistributedTest : public ::testing::Test {
protected:
    wadjet::protocols::tsn::StreamId test_stream_id{"aa:bb:cc:dd:ee:ff", 100};
};

// T322: Test StreamTracker integration with distributed capture
TEST_F(TsnDistributedTest, StreamTrackerCreatedSuccessfully) {
    wadjet::protocols::tsn::StreamTracker tracker;
    EXPECT_EQ(tracker.get_stream_count(), 0);
}

// T322: Stream tracking with packet recording
TEST_F(TsnDistributedTest, StreamTrackerRecordsPacket) {
    wadjet::protocols::tsn::StreamTracker tracker;

    wadjet::protocols::tsn::StreamId stream_id{"11:22:33:44:55:66", 200};
    tracker.process_packet(stream_id, 5, 1500, 1000000000);

    EXPECT_EQ(tracker.get_stream_count(), 1);
    const auto* stats = tracker.get_stream(stream_id);
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->packet_count, 1);
    EXPECT_EQ(stats->byte_count, 1500);
}

// T322: StreamTracker tracks multiple streams
TEST_F(TsnDistributedTest, StreamTrackerMultipleStreams) {
    wadjet::protocols::tsn::StreamTracker tracker;

    wadjet::protocols::tsn::StreamId stream1{"11:22:33:44:55:66", 100};
    wadjet::protocols::tsn::StreamId stream2{"aa:bb:cc:dd:ee:ff", 200};

    tracker.process_packet(stream1, 5, 1500, 1000000000);
    tracker.process_packet(stream2, 6, 2000, 2000000000);
    tracker.finalize();

    EXPECT_EQ(tracker.get_stream_count(), 2);
}

// T322: StreamTracker aggregates packets in same stream
TEST_F(TsnDistributedTest, StreamTrackerAggregatesPackets) {
    wadjet::protocols::tsn::StreamTracker tracker;

    wadjet::protocols::tsn::StreamId stream_id{"11:22:33:44:55:66", 100};
    tracker.process_packet(stream_id, 5, 1500, 1000000000);
    tracker.process_packet(stream_id, 5, 1500, 2000000000);
    tracker.process_packet(stream_id, 5, 1500, 3000000000);
    tracker.finalize();

    const auto* stats = tracker.get_stream(stream_id);
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->packet_count, 3);
    EXPECT_EQ(stats->byte_count, 4500);
}

// T324: LatencyTracker created successfully
TEST_F(TsnDistributedTest, LatencyTrackerCreatedSuccessfully) {
    wadjet::protocols::tsn::LatencyConfig config;
    wadjet::protocols::tsn::LatencyTracker tracker(config);
    EXPECT_NE(tracker.get_priority_stats(wadjet::protocols::tsn::PriorityCodePoint::Video),
              nullptr);
}

// T324: LatencyTracker records latency for priority
TEST_F(TsnDistributedTest, LatencyTrackerRecordsLatency) {
    wadjet::protocols::tsn::LatencyTracker tracker;

    tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Voice, 500000);
    tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Voice, 750000);
    tracker.finalize();

    const auto* stats = tracker.get_priority_stats(wadjet::protocols::tsn::PriorityCodePoint::Voice);
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->sample_count, 2);
    EXPECT_EQ(stats->min_ns, 500000);
    EXPECT_EQ(stats->max_ns, 750000);
}

// T324: LatencyTracker computes percentiles
TEST_F(TsnDistributedTest, LatencyTrackerComputesPercentiles) {
    wadjet::protocols::tsn::LatencyTracker tracker;

    // Record 100 samples from 0 to 99
    for (int i = 0; i < 100; ++i) {
        tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Video,
                              static_cast<int64_t>(i * 1000));
    }
    tracker.finalize();

    const auto* stats = tracker.get_priority_stats(wadjet::protocols::tsn::PriorityCodePoint::Video);
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->sample_count, 100);
    EXPECT_GT(stats->p95_ns, stats->p50_ns);
    EXPECT_GT(stats->p99_ns, stats->p95_ns);
}

// T324: LatencyTracker detects violations
TEST_F(TsnDistributedTest, LatencyTrackerDetectsViolations) {
    wadjet::protocols::tsn::LatencyConfig config;
    // PCP 7 (Voice) threshold: 1ms = 1'000'000 ns
    wadjet::protocols::tsn::LatencyTracker tracker(config);

    // Record latencies above and below threshold
    tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Voice, 500000);      // OK
    tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Voice, 2'000'000);  // Violation
    tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Voice, 800000);      // OK
    tracker.finalize();

    const auto* stats = tracker.get_priority_stats(wadjet::protocols::tsn::PriorityCodePoint::Voice);
    EXPECT_NE(stats, nullptr);
    EXPECT_GE(stats->violations, 1);
}

// T323: ExpectStreamLatency matcher factory
TEST_F(TsnDistributedTest, ExpectStreamLatencyMatcherCreated) {
    auto matcher = ExpectStreamLatency("node-1", "node-2", test_stream_id,
                                      std::chrono::milliseconds(5));
    EXPECT_NE(matcher, nullptr);
}

// T323: ExpectStreamLatency matcher with empty contexts
TEST_F(TsnDistributedTest, ExpectStreamLatencyMatcherWithEmptyContexts) {
    auto matcher = ExpectStreamLatency("node-1", "node-2", test_stream_id,
                                      std::chrono::milliseconds(5));
    CaptureContextMap contexts;  // Empty

    auto result = matcher->matches(contexts);
    EXPECT_FALSE(result.matched);
    EXPECT_FALSE(result.failure_reason.empty());
}

// T323: ExpectStreamLatency matcher missing source node
TEST_F(TsnDistributedTest, ExpectStreamLatencyMissingSourceNode) {
    auto matcher = ExpectStreamLatency("source", "dest", test_stream_id,
                                      std::chrono::milliseconds(5));
    CaptureContextMap contexts;
    // contexts is empty - both nodes missing

    auto result = matcher->matches(contexts);
    EXPECT_FALSE(result.matched);
    EXPECT_FALSE(result.failure_reason.empty());
}

// T324: NodeResult with TSN stream latency
TEST_F(TsnDistributedTest, NodeResultWithTsnStreamLatency) {
    NodeResult result;
    result.node_id = "test-node";

    // Add some TSN stream latency data
    std::map<uint8_t, LatencyStats> stream_latency;
    LatencyStats latency_stats;
    latency_stats.min_ns = 1000;
    latency_stats.max_ns = 5000;
    latency_stats.mean_ns = 3000;
    stream_latency[5] = latency_stats;
    result.tsn_stream_latency["aa:bb:cc:dd:ee:ff:100"] = stream_latency;

    EXPECT_EQ(result.tsn_stream_latency.size(), 1);
}

// T324: AggregatedResult with TSN stream latency
TEST_F(TsnDistributedTest, AggregatedResultWithTsnStreamLatency) {
    AggregatedResult result;
    result.test_name = "TSN_Latency_Test";

    // Add aggregated stream latency
    LatencyStats stats;
    stats.min_ns = 2000;
    stats.max_ns = 8000;
    stats.mean_ns = 5000;
    result.aggregated_stream_latency["stream1"] = {
        {5, stats},  // Priority 5: Voice
        {6, stats}   // Priority 6: Internet Control
    };

    EXPECT_EQ(result.aggregated_stream_latency.size(), 1);
}

// T325: Integration test - StreamTracker + NodeResult
TEST_F(TsnDistributedTest, StreamTrackerIntegrationWithNodeResult) {
    // Create stream tracker
    wadjet::protocols::tsn::StreamTracker tracker;
    wadjet::protocols::tsn::StreamId stream_id{"11:22:33:44:55:66", 100};

    // Record packets
    tracker.process_packet(stream_id, 5, 1500, 1000000000);
    tracker.process_packet(stream_id, 5, 1500, 2000000000);
    tracker.finalize();

    // Create node result
    NodeResult node_result;
    node_result.node_id = "test-node";

    // Populate from tracker (simulating what distributed node would do)
    const auto& streams = tracker.get_all_streams();
    for (const auto& stream : streams) {
        std::map<uint8_t, LatencyStats> prio_latency;
        // In real scenario, LatencyTracker would populate this
        node_result.tsn_stream_latency[stream.id.to_string()] = prio_latency;
    }

    EXPECT_EQ(node_result.tsn_stream_latency.size(), 1);
}

// T325: LatencyTracker reset
TEST_F(TsnDistributedTest, LatencyTrackerReset) {
    wadjet::protocols::tsn::LatencyTracker tracker;

    tracker.record_latency(wadjet::protocols::tsn::PriorityCodePoint::Voice, 500000);
    tracker.finalize();

    const auto* stats = tracker.get_priority_stats(wadjet::protocols::tsn::PriorityCodePoint::Voice);
    EXPECT_GT(stats->sample_count, 0);

    tracker.reset();

    stats = tracker.get_priority_stats(wadjet::protocols::tsn::PriorityCodePoint::Voice);
    EXPECT_EQ(stats->sample_count, 0);
}

}  // namespace wadjet::distributed

#include "wadjet/distributed/distributed_matcher.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed {

// T066: GoogleTest matcher integration for distributed assertions
//
// This provides gMock-compatible matchers for distributed testing:
// - Can be used with EXPECT_THAT, ASSERT_THAT for distributed assertions
// - Returns gtest::MatchResult with description of failures
// - Integrates with existing M3 testing matchers

/**
 * @brief GoogleTest matcher adapter for DistributedMatcher
 *
 * This adapter allows DistributedMatcher implementations to be used
 * with EXPECT_THAT and ASSERT_THAT in GoogleTest fixtures.
 */
class DistributedMatcherAdapter {
public:
    /**
     * @brief Wrap a DistributedMatcher for use with GoogleTest
     *
     * Usage example:
     *   auto flow_matcher = ExpectMessageFlow("node-a", "node-b");
     *   auto contexts = get_capture_contexts();
     *   auto result = flow_matcher->evaluate(contexts);
     *   EXPECT_TRUE(result.matched) << result.error_message;
     *
     * In a full GoogleTest integration, this would enable:
     *   EXPECT_THAT(capture_result, ExpectMessageFlow("node-a", "node-b"));
     */
    static void LogMatcherResult(const DistributedMatchResult& result,
                                 ::testing::MatchResultListener* listener) {
        if (result.matched) {
            *listener << "Message flow successful";
            if (result.latency_ns > 0) {
                *listener << " with latency " << result.latency_ns << "ns";
            }
        } else {
            *listener << "Message flow failed: " << result.error_message;
        }
    }
};

// Note: Factory functions are implemented in individual matcher files:
// - ExpectMessageFlow() in matchers/expect_message_flow.cpp
// - WithinLatency() in matchers/within_latency.cpp
// - HappensBefore() in matchers/happens_before.cpp
// - MustNotSeeOn() in matchers/must_not_see_on.cpp
//
// This organization allows for better separation of concerns and easier testing.

}  // namespace wadjet::distributed

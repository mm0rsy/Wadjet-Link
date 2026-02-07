#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/timestamp_normalizer.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed::testing {

/**
 * T294: Test that test initialization fails when no clock sync is detected
 */
class CoordinatorClockSyncTest : public ::testing::Test {
protected:
    CoordinatorConfig config_;

    void SetUp() override {
        config_.bind_address = "127.0.0.1";
        config_.grpc_port = 50051;
    }
};

/**
 * T294: Test synchronize_capture_start fails without clock sync
 */
TEST_F(CoordinatorClockSyncTest, SyncFailsWithoutClockSync) {
    // Create coordinator
    auto coordinator_result = TestCoordinator::create(config_);
    ASSERT_TRUE(coordinator_result.is_ok());
    auto coordinator = std::move(coordinator_result.unwrap());

    // Start coordinator
    auto start_result = coordinator->start();
    ASSERT_TRUE(start_result.is_ok());

    // Create a test node
    NodeInfo node;
    node.id = "test-node-1";
    node.hostname = "127.0.0.1";
    node.grpc_port = 50052;

    // Register node
    auto reg_result = coordinator->register_node(node);
    ASSERT_TRUE(reg_result.is_ok());

    // Try to synchronize capture without clock sync
    // This should fail with NO_CLOCK_SYNC error if system has no sync
    auto sync_status = TimestampNormalizer::detect_sync_status();

    auto sync_result =
        coordinator->synchronize_capture_start({node.id}, std::chrono::milliseconds(5000));

    if (!sync_status.is_synchronized && sync_status.method == ClockSyncMethod::None) {
        // If system has no sync, expect failure
        ASSERT_FALSE(sync_result.is_ok());
        EXPECT_EQ(sync_result.unwrap_err().code, "NO_CLOCK_SYNC");
    } else {
        // If system has sync, synchronization attempt should work
        // (though barrier wait may timeout if node isn't actually running)
        // The key is that we don't get NO_CLOCK_SYNC error
        if (!sync_result.is_ok()) {
            EXPECT_NE(sync_result.unwrap_err().code, "NO_CLOCK_SYNC");
        }
    }

    coordinator->stop();
}

/**
 * T294: Test that clock sync status is properly detected
 */
TEST_F(CoordinatorClockSyncTest, ClockSyncStatusDetection) {
    auto sync_status = TimestampNormalizer::detect_sync_status();

    // Should have a valid method
    EXPECT_NE(sync_status.method, ClockSyncMethod::Unknown);

    // max_error should be reasonable
    if (sync_status.is_synchronized) {
        EXPECT_GT(sync_status.max_error_ns, 0);
        // Max error should be less than 10 seconds
        EXPECT_LT(sync_status.max_error_ns, 10'000'000'000LL);
    }
}

/**
 * T294: Test synchronized timestamp generation
 */
TEST_F(CoordinatorClockSyncTest, TimestampGeneration) {
    auto timestamp1 = TimestampNormalizer::now_utc_ns();
    auto timestamp2 = TimestampNormalizer::now_utc_ns();

    // Timestamps should be monotonically increasing
    EXPECT_LE(timestamp1, timestamp2);

    // Timestamps should be in reasonable range (after 2020)
    // 2020-01-01 is approximately 1577836800 seconds = 1577836800000000000 ns
    EXPECT_GT(timestamp1, 1577836800000000000LL);

    // Time delta should be small (less than 1 second)
    EXPECT_LT(timestamp2 - timestamp1, 1'000'000'000LL);
}

}  // namespace wadjet::distributed::testing

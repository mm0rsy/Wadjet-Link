#include <gtest/gtest.h>
#include "wadjet/distributed/timestamp_normalizer.hpp"

#include <thread>
#include <chrono>

namespace wadjet::distributed {

class TimestampNormalizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Detect current sync status for tests
        status_ = TimestampNormalizer::detect_sync_status();
    }
    
    ClockSyncStatus status_;
};

// Test T007 & T008: Clock synchronization status detection
TEST_F(TimestampNormalizerTest, DetectSyncStatusReturnsValidStatus) {
    auto status = TimestampNormalizer::detect_sync_status();
    
    // Should always detect some method (at least "None")
    EXPECT_TRUE(status.method != ClockSyncMethod::Unknown);
    
    // If synchronized, should have positive max error
    if (status.is_synchronized) {
        EXPECT_GT(status.max_error_ns, 0);
    }
}

TEST_F(TimestampNormalizerTest, DetectSyncStatusMethodIsValid) {
    auto status = TimestampNormalizer::detect_sync_status();
    
    // Should be one of the known methods
    EXPECT_TRUE(status.method == ClockSyncMethod::None ||
                status.method == ClockSyncMethod::NTP ||
                status.method == ClockSyncMethod::GPTP);
}

// Test T009: Now UTC nanoseconds
TEST_F(TimestampNormalizerTest, NowUtcNsReturnsMonotonicIncreasingValue) {
    int64_t ts1 = TimestampNormalizer::now_utc_ns();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    int64_t ts2 = TimestampNormalizer::now_utc_ns();
    
    EXPECT_LT(ts1, ts2);
}

TEST_F(TimestampNormalizerTest, NowUtcNsReturnsReasonableValue) {
    int64_t ts = TimestampNormalizer::now_utc_ns();
    
    // Should be around current time (within 1 second tolerance)
    // Unix timestamp in nanoseconds for 2024 should be ~1.7e18
    EXPECT_GT(ts, 1700000000000000000LL);  // 2024 start
    EXPECT_LT(ts, 2000000000000000000LL);  // Before 2033
}

// Test T009: Hardware to UTC conversion
TEST_F(TimestampNormalizerTest, HardwareToUtcConvertsTimespec) {
    struct timespec ts = {
        .tv_sec = 1234567890,
        .tv_nsec = 123456789
    };
    
    int64_t ns = TimestampNormalizer::hardware_to_utc(ts);
    
    // Result should be seconds * 1e9 + nanoseconds
    EXPECT_EQ(ns, 1234567890000000000LL + 123456789);
}

TEST_F(TimestampNormalizerTest, HardwareToUtcZeroTime) {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};
    EXPECT_EQ(TimestampNormalizer::hardware_to_utc(ts), 0);
}

// Test constructor
TEST_F(TimestampNormalizerTest, ConstructorStoresStatus) {
    ClockSyncStatus custom_status;
    custom_status.method = ClockSyncMethod::GPTP;
    custom_status.is_synchronized = true;
    custom_status.max_error_ns = 5000;
    
    TimestampNormalizer normalizer(custom_status);
    
    EXPECT_EQ(normalizer.status().method, ClockSyncMethod::GPTP);
    EXPECT_TRUE(normalizer.status().is_synchronized);
    EXPECT_EQ(normalizer.status().max_error_ns, 5000);
}

// Test within_drift
TEST_F(TimestampNormalizerTest, WithinDriftReturnsTrueForCloseTimestamps) {
    TimestampNormalizer normalizer(status_);
    
    int64_t ts1 = 1000000;
    int64_t ts2 = 1000100;  // 100 ns apart
    
    EXPECT_TRUE(normalizer.within_drift(ts1, ts2, std::chrono::nanoseconds(1000)));
}

TEST_F(TimestampNormalizerTest, WithinDriftReturnsFalseForDistantTimestamps) {
    TimestampNormalizer normalizer(status_);
    
    int64_t ts1 = 1000000;
    int64_t ts2 = 2000000;  // 1 microsecond apart
    
    EXPECT_FALSE(normalizer.within_drift(ts1, ts2, std::chrono::nanoseconds(100)));
}

TEST_F(TimestampNormalizerTest, WithinDriftWorksReversed) {
    TimestampNormalizer normalizer(status_);
    
    int64_t ts1 = 2000000;
    int64_t ts2 = 1000000;  // Reversed order
    
    EXPECT_TRUE(normalizer.within_drift(ts1, ts2, std::chrono::nanoseconds(1000000)));
}

// Test estimated_precision
TEST_F(TimestampNormalizerTest, EstimatedPrecisionGPTP) {
    ClockSyncStatus gptp_status;
    gptp_status.method = ClockSyncMethod::GPTP;
    TimestampNormalizer normalizer(gptp_status);
    
    auto precision = normalizer.estimated_precision();
    // gPTP should be high precision (1 microsecond)
    EXPECT_EQ(precision.count(), 1000);
}

TEST_F(TimestampNormalizerTest, EstimatedPrecisionNTP) {
    ClockSyncStatus ntp_status;
    ntp_status.method = ClockSyncMethod::NTP;
    TimestampNormalizer normalizer(ntp_status);
    
    auto precision = normalizer.estimated_precision();
    // NTP should be millisecond precision
    EXPECT_EQ(precision.count(), 10000000);
}

TEST_F(TimestampNormalizerTest, EstimatedPrecisionNone) {
    ClockSyncStatus none_status;
    none_status.method = ClockSyncMethod::None;
    TimestampNormalizer normalizer(none_status);
    
    auto precision = normalizer.estimated_precision();
    // No sync should be 100ms precision
    EXPECT_EQ(precision.count(), 100000000);
}

// Test status getter
TEST_F(TimestampNormalizerTest, StatusGetterReturnsStoredStatus) {
    ClockSyncStatus test_status;
    test_status.method = ClockSyncMethod::NTP;
    test_status.is_synchronized = true;
    test_status.max_error_ns = 50000;
    test_status.grandmaster_id = "test_gm";
    
    TimestampNormalizer normalizer(test_status);
    const auto& returned_status = normalizer.status();
    
    EXPECT_EQ(returned_status.method, ClockSyncMethod::NTP);
    EXPECT_TRUE(returned_status.is_synchronized);
    EXPECT_EQ(returned_status.max_error_ns, 50000);
    EXPECT_EQ(returned_status.grandmaster_id, "test_gm");
}

}  // namespace wadjet::distributed

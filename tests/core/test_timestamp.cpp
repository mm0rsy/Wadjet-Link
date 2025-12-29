#include <gtest/gtest.h>
#include <wadjet/core/timestamp.hpp>

#include <chrono>
#include <thread>

namespace wadjet::test {

TEST(Timestamp, Now) {
    auto ts = Timestamp::now();
    EXPECT_GT(ts.seconds(), 0);
}

TEST(Timestamp, FromUnix) {
    auto ts = Timestamp::from_unix(1234567890, 123456789);
    EXPECT_EQ(ts.seconds(), 1234567890);
    EXPECT_EQ(ts.nanoseconds(), 123456789);
}

TEST(Timestamp, FromUnixUsec) {
    auto ts = Timestamp::from_unix_usec(1234567890, 123456);
    EXPECT_EQ(ts.seconds(), 1234567890);
    EXPECT_EQ(ts.microseconds(), 123456);
}

TEST(Timestamp, Microseconds) {
    auto ts = Timestamp::from_unix(0, 123456789);
    EXPECT_EQ(ts.microseconds(), 123456);  // 123456789 / 1000
}

TEST(Timestamp, ToString) {
    auto ts = Timestamp::from_unix(0, 0);
    auto str = ts.to_string();
    EXPECT_FALSE(str.empty());
    EXPECT_TRUE(str.find("1970") != std::string::npos);  // Unix epoch
}

TEST(Timestamp, Comparison) {
    auto ts1 = Timestamp::from_unix(100, 0);
    auto ts2 = Timestamp::from_unix(100, 0);
    auto ts3 = Timestamp::from_unix(200, 0);

    EXPECT_EQ(ts1, ts2);
    EXPECT_NE(ts1, ts3);
    EXPECT_LT(ts1, ts3);
    EXPECT_GT(ts3, ts1);
}

TEST(Timestamp, Duration) {
    auto ts1 = Timestamp::from_unix(100, 0);
    auto ts2 = Timestamp::from_unix(200, 0);

    auto diff = ts2 - ts1;
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(diff).count(), 100);
}

TEST(Timestamp, AddDuration) {
    auto ts = Timestamp::from_unix(100, 0);
    auto ts2 = ts + std::chrono::seconds(50);
    EXPECT_EQ(ts2.seconds(), 150);
}

}  // namespace wadjet::test

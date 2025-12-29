#include <gtest/gtest.h>
#include <wadjet/io/frame_filter.hpp>

namespace wadjet::test {

TEST(FrameFilter, CompileValidFilter) {
    auto result = io::FrameFilter::compile("udp port 30490");
    EXPECT_TRUE(result.is_ok()) << result.error().message;
}

TEST(FrameFilter, CompileComplexFilter) {
    auto result = io::FrameFilter::compile("tcp port 80 or udp port 53");
    EXPECT_TRUE(result.is_ok()) << result.error().message;
}

TEST(FrameFilter, CompileEmptyFilter) {
    auto result = io::FrameFilter::compile("");
    EXPECT_TRUE(result.is_ok()) << result.error().message;
}

TEST(FrameFilter, CompileInvalidFilter) {
    auto result = io::FrameFilter::compile("this is not a valid filter !@#$");
    EXPECT_FALSE(result.is_ok());
}

TEST(FrameFilter, AcceptAll) {
    auto filter = io::FrameFilter::accept_all();
    // Should accept any data
    std::uint8_t dummy[64] = {};
    EXPECT_TRUE(filter.matches(dummy, sizeof(dummy)));
}

TEST(FrameFilter, Expression) {
    auto result = io::FrameFilter::compile("udp port 30490");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->expression(), "udp port 30490");
}

}  // namespace wadjet::test

#include "wadjet/protocols/ipv4.hpp"
#include <gtest/gtest.h>
#include <vector>

using namespace wadjet::protocols::ipv4;

TEST(IPv4OptionsTest, ParseNopAndEol) {
    // Options: NOP, NOP, EOL
    std::vector<std::byte> raw = {std::byte{1}, std::byte{1}, std::byte{0}};
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 3);
    EXPECT_EQ(res.options[0].type, 1);
    EXPECT_EQ(res.options[1].type, 1);
    EXPECT_EQ(res.options[2].type, 0);
}

TEST(IPv4OptionsTest, ParseRecordRoute) {
    // Option kind=7, length=7, data=4 bytes (example)
    std::vector<std::byte> raw = {std::byte{7}, std::byte{7}, std::byte{4}, std::byte{0}, std::byte{127}, std::byte{1}};
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 1);
    EXPECT_EQ(res.options[0].type, 7);
    EXPECT_EQ(res.options[0].data.size(), 4);
}

TEST(IPv4OptionsTest, ParseTimestamp) {
    std::vector<std::byte> raw = {std::byte{68}, std::byte{10}, std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}};
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 1);
    EXPECT_EQ(res.options[0].type, 68);
    EXPECT_EQ(res.options[0].data.size(), 8);
}

TEST(IPv4OptionsTest, MalformedOption) {
    std::vector<std::byte> raw = {std::byte{68}, std::byte{10}, std::byte{0}, std::byte{1}}; // truncated
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_TRUE(res.malformed);
}


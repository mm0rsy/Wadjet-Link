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
    // Option kind=7, length=7, data=5 bytes (7 - 2 for kind and length)
    std::vector<std::byte> raw = {std::byte{7},   std::byte{7}, std::byte{4}, std::byte{0},
                                  std::byte{127}, std::byte{1}, std::byte{2}};
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 1);
    EXPECT_EQ(res.options[0].type, 7);
    EXPECT_EQ(res.options[0].data.size(), 5);
}

TEST(IPv4OptionsTest, ParseTimestamp) {
    std::vector<std::byte> raw = {std::byte{68}, std::byte{10}, std::byte{0}, std::byte{1},
                                  std::byte{2},  std::byte{3},  std::byte{4}, std::byte{5},
                                  std::byte{6},  std::byte{7}};
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 1);
    EXPECT_EQ(res.options[0].type, 68);
    EXPECT_EQ(res.options[0].data.size(), 8);
}

TEST(IPv4OptionsTest, MalformedOption) {
    std::vector<std::byte> raw = {std::byte{68}, std::byte{10}, std::byte{0},
                                  std::byte{1}};  // truncated
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_TRUE(res.malformed);
}

TEST(IPv4OptionsTest, ParseRouterAlert) {
    // Router Alert option: type=148, length=4, value=0x0000 (Router shall examine packet)
    // RFC 2113: Option format is [type=148][length=4][value_high][value_low]
    std::vector<std::byte> raw = {
        std::byte{148},  // Type: Router Alert
        std::byte{4},    // Length: 4 bytes total
        std::byte{0},    // Value high byte
        std::byte{0}     // Value low byte (0x0000 = Router shall examine packet)
    };
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 1);
    EXPECT_EQ(res.options[0].type, 148);       // ROUTER_ALERT
    EXPECT_EQ(res.options[0].data.size(), 2);  // 2 bytes of value data
    // Value should be 0x0000
    EXPECT_EQ(res.options[0].data[0], 0);
    EXPECT_EQ(res.options[0].data[1], 0);
}

TEST(IPv4OptionsTest, ParseRouterAlertNonZero) {
    // Router Alert with non-zero value (reserved for future use)
    std::vector<std::byte> raw = {
        std::byte{148},  // Type: Router Alert
        std::byte{4},    // Length: 4 bytes total
        std::byte{0},    // Value high byte
        std::byte{1}     // Value low byte (0x0001 = RSVP reserved)
    };
    auto res = IPv4Header::parseIpv4Options(raw);
    ASSERT_FALSE(res.malformed);
    ASSERT_EQ(res.options.size(), 1);
    EXPECT_EQ(res.options[0].type, static_cast<std::uint8_t>(IPv4Header::OptionType::ROUTER_ALERT));
    EXPECT_EQ(res.options[0].data.size(), 2);
    // Value should be 0x0001
    EXPECT_EQ(res.options[0].data[0], 0);
    EXPECT_EQ(res.options[0].data[1], 1);
}

TEST(IPv4OptionsTest, OptionTypeEnumValues) {
    // Verify all required option types are defined per FR-001
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::EOL), 0);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::NOP), 1);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::RECORD_ROUTE), 7);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::TIMESTAMP), 68);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::SECURITY), 130);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::LOOSE_SOURCE_ROUTE), 131);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::STREAM_ID), 136);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::STRICT_SOURCE_ROUTE), 137);
    EXPECT_EQ(static_cast<std::uint8_t>(IPv4Header::OptionType::ROUTER_ALERT), 148);
}

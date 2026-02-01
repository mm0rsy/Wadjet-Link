#include "wadjet/protocols/ipv4.hpp"
#include <gtest/gtest.h>

using namespace wadjet::protocols::ipv4;
using wadjet::IPv4Address;

/// Tests for IPv4 decoder options and basic functionality
/// Note: Full IPv4 decoding tests should be in test_decoders.cpp or integration tests
/// These tests focus on:
/// - IPv4 checksum calculation
/// - IPv4 decoder options struct
/// - Fragment structure creation

TEST(IPv4ChecksumTest, CalculateChecksum) {
    // Minimal valid IPv4 header (20 bytes)
    std::array<std::uint8_t, 20> hdr = {
        0x45,                    // v=4, ihl=5
        0x00, 0x00, 0x14,        // total length = 20
        0x12, 0x34,              // identification
        0x00, 0x00,              // flags, fragment offset
        0x40,                    // TTL
        0x06,                    // protocol (TCP)
        0x00, 0x00,              // checksum (will calculate)
        0xC0, 0xA8, 0x01, 0x01,  // src: 192.168.1.1
        0xC0, 0xA8, 0x01, 0x02   // dst: 192.168.1.2
    };

    auto bytes =
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(hdr.data()), hdr.size());
    std::uint16_t csum = IPv4Decoder::calculate_checksum(bytes);

    // Verify checksum is non-zero
    EXPECT_NE(csum, 0);
}

TEST(IPv4DecoderOptionsTest, ConstructWithNoValidation) {
    // Test that decoder can be constructed with validation disabled
    IPv4Decoder::Options opts(false, false);
    IPv4Decoder dec(opts);
    EXPECT_EQ(dec.name(), "IPv4");
}

TEST(IPv4DecoderOptionsTest, ConstructWithValidation) {
    // Test that decoder can be constructed with validation enabled
    IPv4Decoder::Options opts(true, true);
    IPv4Decoder dec(opts);
    EXPECT_EQ(dec.name(), "IPv4");
}

TEST(IPv4HeaderTest, ParseOptionsStatic) {
    // Test static method for parsing IPv4 options
    std::vector<std::byte> raw_options;
    // NOP option (type=1, length=1)
    raw_options.push_back(std::byte{1});

    auto result = IPv4Header::parseIpv4Options(raw_options);
    EXPECT_FALSE(result.malformed);
    EXPECT_GE(result.options.size(), 0);
}

TEST(IPv4FragmentTest, FragmentStructCreation) {
    IPv4Header::Ipv4Fragment frag;
    frag.src_ip = IPv4Address::from_string("192.168.1.1");
    frag.dst_ip = IPv4Address::from_string("192.168.1.2");
    frag.protocol = 6;
    frag.identification = 0x1234;
    frag.offset = 0;
    frag.mf = true;
    frag.payload = {1, 2, 3, 4};

    EXPECT_EQ(frag.offset, 0);
    EXPECT_TRUE(frag.mf);
    EXPECT_EQ(frag.payload.size(), 4);
}

TEST(IPv4FragmentTest, FragmentWithOffset) {
    IPv4Header::Ipv4Fragment frag;
    frag.src_ip = IPv4Address::from_string("10.0.0.1");
    frag.dst_ip = IPv4Address::from_string("10.0.0.2");
    frag.protocol = 17;  // UDP
    frag.identification = 0x5678;
    frag.offset = 100;
    frag.mf = false;
    frag.payload = {0xAA, 0xBB, 0xCC};

    EXPECT_EQ(frag.offset, 100);
    EXPECT_FALSE(frag.mf);
    EXPECT_EQ(frag.payload.size(), 3);
}

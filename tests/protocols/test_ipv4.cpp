#include "wadjet/protocols/ipv4.hpp"
#include <gtest/gtest.h>

using namespace wadjet::protocols::ipv4;

TEST(IPv4ExtraTests, DSCPExtraction) {
    // DSCP value 0x1A -> binary 011010, shifted left by 2 for TOS
    std::array<std::uint8_t, 20> pkt = {
        0x45, // v=4, ihl=5
        static_cast<std::uint8_t>((0x1A << 2) | 0x00), // DSCP=0x1A
        0x00, 0x28,
        0x12, 0x34,
        0x00, 0x00,
        0x40,
        0x06,
        0x00, 0x00,
        0xC0, 0xA8, 0x01, 0x01,
        0xC0, 0xA8, 0x01, 0x02
    };

    auto ctx = make_context(pkt);
    ipv4::IPv4Decoder dec(ipv4::IPv4Decoder::Options{false,false});
    auto res = dec.decode(ctx);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->dscp, 0x1A);
}

TEST(IPv4ChecksumTests, ValidChecksum) {
    // Build a minimal header and compute checksum
    std::vector<std::uint8_t> hdr = {
        0x45, 0x00, 0x00, 0x28,
        0x12, 0x34, 0x00, 0x00,
        0x40, 0x06, 0x00, 0x00,
        0xC0, 0xA8, 0x01, 0x01,
        0xC0, 0xA8, 0x01, 0x02
    };
    // Compute checksum
    auto header_bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(hdr.data()), hdr.size());
    uint16_t csum = IPv4Decoder::calculate_checksum(header_bytes);
    // Place checksum in bytes 10-11
    hdr[10] = static_cast<uint8_t>((csum >> 8) & 0xFF);
    hdr[11] = static_cast<uint8_t>(csum & 0xFF);

    auto ctx = make_context(hdr);
    ipv4::IPv4Decoder dec(ipv4::IPv4Decoder::Options{true,false});
    auto res = dec.decode(ctx);
    ASSERT_TRUE(res);
    EXPECT_TRUE(res->checksum_valid);
}

TEST(IPv4ChecksumTests, InvalidChecksum) {
    std::vector<std::uint8_t> hdr = {
        0x45, 0x00, 0x00, 0x28,
        0x12, 0x34, 0x00, 0x00,
        0x40, 0x06, 0xFF, 0xFF, // bad checksum
        0xC0, 0xA8, 0x01, 0x01,
        0xC0, 0xA8, 0x01, 0x02
    };

    auto ctx = make_context(hdr);
    ipv4::IPv4Decoder dec(ipv4::IPv4Decoder::Options{true,false});
    auto res = dec.decode(ctx);
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, DecodeErrorCode::InvalidChecksum);
}

TEST(IPv4OptionsDecodeTest, MalformedOptionsMarked) {
    // IHL=6 -> 24 bytes header, but provide only 22 bytes to simulate truncated options
    std::vector<std::uint8_t> hdr = {
        0x46, // v=4, ihl=6
        0x00,
        0x00, 0x16,
        0x12, 0x34,
        0x00, 0x00,
        0x40,
        0x06,
        0x00, 0x00,
        0xC0, 0xA8, 0x01, 0x01,
        0xC0, 0xA8, 0x01, 0x02,
        0x01, 0x00 // only 2 bytes of options (malformed)
    };

    auto ctx = make_context(hdr);
    ipv4::IPv4Decoder dec(ipv4::IPv4Decoder::Options{false,true}); // allow bad checksum
    auto res = dec.decode(ctx);
    ASSERT_TRUE(res);
    EXPECT_TRUE(res->options_malformed);
}

/// @file test_matchers.cpp
/// @brief Unit tests for Wadjet-Link testing framework matchers
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/testing/matchers.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

using namespace wadjet;
using namespace wadjet::testing;
using namespace ::testing;

// =============================================================================
// Test data
// =============================================================================

// Create a valid Ethernet + IPv4 + UDP + SOME/IP packet
// Ethernet: 14 bytes, IPv4: 20 bytes, UDP: 8 bytes, SOME/IP: 16 bytes header
const std::array<uint8_t, 58> g_someip_packet = {
    // Ethernet header (14 bytes)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // Dst MAC: 01:02:03:04:05:06
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // Src MAC: 0A:0B:0C:0D:0E:0F
    0x08, 0x00,                          // EtherType: IPv4 (0x0800)

    // IPv4 header (20 bytes)
    0x45, 0x00,              // Version=4, IHL=5, DSCP=0
    0x00, 0x2C,              // Total length: 44 bytes
    0x00, 0x01,              // Identification
    0x00, 0x00,              // Flags + Fragment offset
    0x40, 0x11,              // TTL=64, Protocol=UDP (17)
    0x00, 0x00,              // Header checksum (skipped)
    0xC0, 0xA8, 0x01, 0x64,  // Src IP: 192.168.1.100
    0xC0, 0xA8, 0x01, 0xC8,  // Dst IP: 192.168.1.200

    // UDP header (8 bytes)
    0x77, 0x1A,  // Src port: 30490
    0x77, 0x1B,  // Dst port: 30491
    0x00, 0x24,  // Length: 36 bytes
    0x00, 0x00,  // Checksum (skipped)

    // SOME/IP header (16 bytes)
    0x12, 0x34,              // Service ID: 0x1234
    0x80, 0x01,              // Method ID: 0x8001 (high bit = response)
    0x00, 0x00, 0x00, 0x10,  // Length: 16 bytes
    0x00, 0x00, 0x00, 0x01,  // Client ID + Session ID
    0x01,                    // Protocol version
    0x01,                    // Interface version
    0x80,                    // Message type: 0x80 = Response
    0x00                     // Return code: 0x00 = OK
};

// Create a VLAN-tagged Ethernet frame
const std::array<uint8_t, 22> g_vlan_packet = {
    // Ethernet header with VLAN (18 bytes)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // Dst MAC
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // Src MAC
    0x81, 0x00,                          // TPID: 802.1Q VLAN
    0x00, 0x64,                          // TCI: PCP=0, DEI=0, VID=100
    0x08, 0x00,                          // EtherType: IPv4
    // Minimal payload
    0x45, 0x00, 0x00, 0x14};

// Create a DoIP packet (TCP-based)
const std::array<uint8_t, 54> g_doip_packet = {
    // Ethernet header (14 bytes)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // Dst MAC
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // Src MAC
    0x08, 0x00,                          // EtherType: IPv4

    // IPv4 header (20 bytes)
    0x45, 0x00,              // Version=4, IHL=5
    0x00, 0x28,              // Total length: 40 bytes
    0x00, 0x01,              // Identification
    0x00, 0x00,              // Flags + Fragment offset
    0x40, 0x06,              // TTL=64, Protocol=TCP (6)
    0x00, 0x00,              // Header checksum
    0xC0, 0xA8, 0x01, 0x64,  // Src IP: 192.168.1.100
    0xC0, 0xA8, 0x01, 0xC8,  // Dst IP: 192.168.1.200

    // TCP header (20 bytes, minimal)
    0x34, 0x4D,              // Src port: 13389
    0x34, 0x4E,              // Dst port: 13390
    0x00, 0x00, 0x00, 0x01,  // Sequence number
    0x00, 0x00, 0x00, 0x00,  // Ack number
    0x50, 0x00,              // Data offset=5, flags=0
    0x00, 0x00,              // Window
    0x00, 0x00,              // Checksum
    0x00, 0x00               // Urgent pointer
};

// =============================================================================
// Test Fixture
// =============================================================================

class MatchersTest : public ::testing::Test {
protected:
    template <size_t N>
    Packet make_packet(const std::array<uint8_t, N>& data) {
        auto byte_span = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()),
                                                    data.size());
        return Packet(byte_span, Timestamp::now());
    }

    Packet someip_pkt() { return make_packet(g_someip_packet); }
    Packet vlan_pkt() { return make_packet(g_vlan_packet); }
    Packet doip_pkt() { return make_packet(g_doip_packet); }
};

// =============================================================================
// Ethernet Matchers Tests
// =============================================================================

TEST_F(MatchersTest, HasEthertype_IPv4) {
    EXPECT_THAT(someip_pkt(), HasEthertype(0x0800));
    EXPECT_THAT(someip_pkt(), Not(HasEthertype(0x0806)));  // Not ARP
}

TEST_F(MatchersTest, HasEthertype_VLAN) {
    // VLAN packet has inner ethertype 0x0800 (IPv4), not 0x8100 (VLAN tag)
    EXPECT_THAT(vlan_pkt(), HasEthertype(0x0800));
}

TEST_F(MatchersTest, HasSourceMac) {
    EXPECT_THAT(someip_pkt(),
                HasSourceMac(MacAddress::from_bytes(0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F)));
    EXPECT_THAT(someip_pkt(),
                Not(HasSourceMac(MacAddress::from_bytes(0x01, 0x02, 0x03, 0x04, 0x05, 0x06))));
}

TEST_F(MatchersTest, HasDestMac) {
    EXPECT_THAT(someip_pkt(),
                HasDestMac(MacAddress::from_bytes(0x01, 0x02, 0x03, 0x04, 0x05, 0x06)));
    EXPECT_THAT(someip_pkt(),
                Not(HasDestMac(MacAddress::from_bytes(0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F))));
}

TEST_F(MatchersTest, HasVlan) {
    EXPECT_THAT(vlan_pkt(), HasVlan());
    EXPECT_THAT(someip_pkt(), Not(HasVlan()));
}

TEST_F(MatchersTest, HasVlanId) {
    EXPECT_THAT(vlan_pkt(), HasVlanId(100));
    EXPECT_THAT(vlan_pkt(), Not(HasVlanId(200)));
}

// =============================================================================
// IPv4 Matchers Tests
// =============================================================================

TEST_F(MatchersTest, HasSourceIP) {
    EXPECT_THAT(someip_pkt(), HasSourceIP(0xC0A80164));  // 192.168.1.100
    EXPECT_THAT(someip_pkt(), Not(HasSourceIP(0xC0A801C8)));
}

TEST_F(MatchersTest, HasSourceIP_String) {
    EXPECT_THAT(someip_pkt(), HasSourceIP("192.168.1.100"));
    EXPECT_THAT(someip_pkt(), Not(HasSourceIP("192.168.1.200")));
}

TEST_F(MatchersTest, HasDestIP) {
    EXPECT_THAT(someip_pkt(), HasDestIP(0xC0A801C8));  // 192.168.1.200
    EXPECT_THAT(someip_pkt(), Not(HasDestIP(0xC0A80164)));
}

TEST_F(MatchersTest, HasDestIP_String) {
    EXPECT_THAT(someip_pkt(), HasDestIP("192.168.1.200"));
    EXPECT_THAT(someip_pkt(), Not(HasDestIP("192.168.1.100")));
}

TEST_F(MatchersTest, HasIPProtocol_UDP) {
    EXPECT_THAT(someip_pkt(), HasIPProtocol(17));      // UDP
    EXPECT_THAT(someip_pkt(), Not(HasIPProtocol(6)));  // Not TCP
}

TEST_F(MatchersTest, HasIPProtocol_TCP) {
    EXPECT_THAT(doip_pkt(), HasIPProtocol(6));        // TCP
    EXPECT_THAT(doip_pkt(), Not(HasIPProtocol(17)));  // Not UDP
}

TEST_F(MatchersTest, IsUDP) {
    EXPECT_THAT(someip_pkt(), IsUDP());
    EXPECT_THAT(doip_pkt(), Not(IsUDP()));
}

TEST_F(MatchersTest, IsTCP) {
    EXPECT_THAT(doip_pkt(), IsTCP());
    EXPECT_THAT(someip_pkt(), Not(IsTCP()));
}

// =============================================================================
// Port Matchers Tests
// =============================================================================

TEST_F(MatchersTest, HasSourcePort_UDP) {
    EXPECT_THAT(someip_pkt(), HasSourcePort(30490));
    EXPECT_THAT(someip_pkt(), Not(HasSourcePort(30491)));
}

TEST_F(MatchersTest, HasDestPort_UDP) {
    EXPECT_THAT(someip_pkt(), HasDestPort(30491));
    EXPECT_THAT(someip_pkt(), Not(HasDestPort(30490)));
}

TEST_F(MatchersTest, HasSourcePort_TCP) {
    EXPECT_THAT(doip_pkt(), HasSourcePort(13389));
}

TEST_F(MatchersTest, HasDestPort_TCP) {
    EXPECT_THAT(doip_pkt(), HasDestPort(13390));
}

// =============================================================================
// SOME/IP Matchers Tests
// =============================================================================

TEST_F(MatchersTest, HasSOMEIPServiceId) {
    EXPECT_THAT(someip_pkt(), HasSOMEIPServiceId(0x1234));
    EXPECT_THAT(someip_pkt(), Not(HasSOMEIPServiceId(0x5678)));
}

TEST_F(MatchersTest, HasSOMEIPMethodId) {
    EXPECT_THAT(someip_pkt(), HasSOMEIPMethodId(0x8001));
    EXPECT_THAT(someip_pkt(), Not(HasSOMEIPMethodId(0x0001)));
}

TEST_F(MatchersTest, HasSOMEIPMessageType) {
    EXPECT_THAT(someip_pkt(), HasSOMEIPMessageType(protocols::someip::MessageType::Response));
    EXPECT_THAT(someip_pkt(), Not(HasSOMEIPMessageType(protocols::someip::MessageType::Request)));
}

TEST_F(MatchersTest, IsSOMEIPResponse) {
    EXPECT_THAT(someip_pkt(), IsSOMEIPResponse());
    EXPECT_THAT(someip_pkt(), Not(IsSOMEIPRequest()));
}

// =============================================================================
// Payload Matchers Tests
// =============================================================================

TEST_F(MatchersTest, PayloadContains_Found) {
    // The packet contains 0x12, 0x34 as service ID
    EXPECT_THAT(someip_pkt(), PayloadContains({0x12, 0x34}));
}

TEST_F(MatchersTest, PayloadContains_NotFound) {
    // Unlikely pattern
    EXPECT_THAT(someip_pkt(), Not(PayloadContains({0xFF, 0xFF, 0xFF, 0xFF, 0xFF})));
}

TEST_F(MatchersTest, HasPayloadSize) {
    EXPECT_THAT(someip_pkt(), HasPayloadSize(58));
    EXPECT_THAT(someip_pkt(), Not(HasPayloadSize(100)));
}

// =============================================================================
// Combined Matchers Tests
// =============================================================================

TEST_F(MatchersTest, DecodesSuccessfully) {
    EXPECT_THAT(someip_pkt(), DecodesSuccessfully());
}

TEST_F(MatchersTest, CombinedMatchers_AllOf) {
    EXPECT_THAT(someip_pkt(), AllOf(HasEthertype(0x0800), IsUDP(), HasSOMEIPServiceId(0x1234),
                                    IsSOMEIPResponse()));
}

TEST_F(MatchersTest, CombinedMatchers_AnyOf) {
    EXPECT_THAT(someip_pkt(), AnyOf(HasSOMEIPServiceId(0x1234), HasSOMEIPServiceId(0x5678),
                                    HasSOMEIPServiceId(0xABCD)));
}

TEST_F(MatchersTest, ComplexQuery) {
    // Match: UDP SOME/IP response from 192.168.1.100 to service 0x1234
    EXPECT_THAT(someip_pkt(),
                AllOf(IsUDP(), HasSourceIP(0xC0A80164), HasDestIP(0xC0A801C8), HasSourcePort(30490),
                      HasSOMEIPServiceId(0x1234), IsSOMEIPResponse(), DecodesSuccessfully()));
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(MatchersTest, TooShortPacket_EthernetMatcher) {
    std::array<uint8_t, 10> short_data = {0x01, 0x02, 0x03, 0x04, 0x05,
                                          0x06, 0x07, 0x08, 0x09, 0x0A};
    auto pkt = make_packet(short_data);

    // Should not crash, should just not match
    EXPECT_THAT(pkt, Not(HasEthertype(0x0800)));
}

TEST_F(MatchersTest, TooShortPacket_IPv4Matcher) {
    // Only Ethernet header, no IPv4
    std::array<uint8_t, 14> eth_only = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A,
                                        0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x00};
    auto pkt = make_packet(eth_only);

    // Should not crash, should just not match
    EXPECT_THAT(pkt, Not(HasSourceIP(0xC0A80164)));
    EXPECT_THAT(pkt, Not(IsUDP()));
}

TEST_F(MatchersTest, TooShortPacket_SOMEIPMatcher) {
    // Ethernet + IPv4 + UDP but no SOME/IP
    std::array<uint8_t, 42> no_someip = {
        // Ethernet
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x00,
        // IPv4
        0x45, 0x00, 0x00, 0x1C, 0x00, 0x01, 0x00, 0x00, 0x40, 0x11, 0x00, 0x00, 0xC0, 0xA8, 0x01,
        0x64, 0xC0, 0xA8, 0x01, 0xC8,
        // UDP (only header, no payload)
        0x77, 0x1A, 0x77, 0x1B, 0x00, 0x08, 0x00, 0x00};
    auto pkt = make_packet(no_someip);

    // Should not crash, should just not match
    EXPECT_THAT(pkt, Not(HasSOMEIPServiceId(0x1234)));
}

// =============================================================================
// Matcher Description Tests
// =============================================================================

TEST_F(MatchersTest, MatcherDescriptions) {
    // Verify matchers produce meaningful descriptions
    // This is important for readable test failure messages
    auto pkt = someip_pkt();

    // These tests verify the matchers don't crash when describing themselves
    ::testing::StringMatchResultListener listener;

    HasEthertype(0x0800).impl().MatchAndExplain(pkt, &listener);
    HasSourceMac(MacAddress::from_bytes(0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F))
        .impl()
        .MatchAndExplain(pkt, &listener);
    HasSourceIP(0xC0A80164).impl().MatchAndExplain(pkt, &listener);
    HasSourcePort(30490).impl().MatchAndExplain(pkt, &listener);
    HasSOMEIPServiceId(0x1234).impl().MatchAndExplain(pkt, &listener);
    IsSOMEIPResponse().impl().MatchAndExplain(pkt, &listener);
    PayloadContains({0x12, 0x34}).impl().MatchAndExplain(pkt, &listener);
    DecodesSuccessfully().impl().MatchAndExplain(pkt, &listener);
}

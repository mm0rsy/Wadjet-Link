/// @file test_generators.cpp
/// @brief Tests for property-based testing generators
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/testing/generators.hpp"
#include "wadjet/testing/testing.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>

using namespace wadjet;
using namespace wadjet::testing;
using namespace wadjet::testing::generators;

// =============================================================================
// Random Number Generator Tests
// =============================================================================

class RandomTest : public ::testing::Test {
protected:
    Random rng_{42};  // Fixed seed for reproducibility
};

TEST_F(RandomTest, SeedIsReproducible) {
    Random rng1(12345);
    Random rng2(12345);

    EXPECT_EQ(rng1.u32(), rng2.u32());
    EXPECT_EQ(rng1.u16(), rng2.u16());
    EXPECT_EQ(rng1.u8(), rng2.u8());
}

TEST_F(RandomTest, DifferentSeedsProduceDifferentValues) {
    Random rng1(111);
    Random rng2(222);

    // Very unlikely to be equal with different seeds
    EXPECT_NE(rng1.u32(), rng2.u32());
}

TEST_F(RandomTest, IntegerInRange) {
    for (int i = 0; i < 100; ++i) {
        auto val = rng_.integer(10, 20);
        EXPECT_GE(val, 10);
        EXPECT_LE(val, 20);
    }
}

TEST_F(RandomTest, BytesGeneratesCorrectSize) {
    auto bytes = rng_.bytes(100);
    EXPECT_EQ(bytes.size(), 100u);
}

TEST_F(RandomTest, MacAddressIsUnicast) {
    for (int i = 0; i < 100; ++i) {
        auto mac = rng_.mac_address();
        // Unicast MAC has LSB of first byte = 0
        EXPECT_EQ(mac[0] & 0x01, 0);
    }
}

TEST_F(RandomTest, PortInRange) {
    for (int i = 0; i < 100; ++i) {
        auto port = rng_.port();
        EXPECT_GE(port, 1024);
        EXPECT_LE(port, 65535);
    }
}

TEST_F(RandomTest, ChanceWorksReasonably) {
    int true_count = 0;
    int total = 10000;

    for (int i = 0; i < total; ++i) {
        if (rng_.chance(0.5)) {
            ++true_count;
        }
    }

    // Should be roughly 50% (within 10%)
    double ratio = static_cast<double>(true_count) / total;
    EXPECT_GT(ratio, 0.4);
    EXPECT_LT(ratio, 0.6);
}

TEST_F(RandomTest, PickSelectsFromContainer) {
    std::vector<int> values = {1, 2, 3, 4, 5};
    std::set<int> picked;

    for (int i = 0; i < 100; ++i) {
        picked.insert(rng_.pick(values));
    }

    // Should have picked multiple different values
    EXPECT_GT(picked.size(), 1u);
    // All picked values should be from original set
    for (int v : picked) {
        EXPECT_TRUE(std::find(values.begin(), values.end(), v) != values.end());
    }
}

// =============================================================================
// Ethernet Builder Tests
// =============================================================================

class EthernetBuilderTest : public ::testing::Test {
protected:
    Random rng_{42};
};

TEST_F(EthernetBuilderTest, BuildsValidEthernetFrame) {
    auto frame = EthernetBuilder(rng_).with_ethertype(0x0800).build();

    // Minimum Ethernet header is 14 bytes
    EXPECT_GE(frame.size(), 14u);
    // Check ethertype
    EXPECT_EQ(frame[12], 0x08);
    EXPECT_EQ(frame[13], 0x00);
}

TEST_F(EthernetBuilderTest, SetsCustomMac) {
    std::array<std::uint8_t, 6> dst = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::array<std::uint8_t, 6> src = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    auto frame = EthernetBuilder(rng_).with_dst_mac(dst).with_src_mac(src).build();

    EXPECT_EQ(frame[0], 0xAA);
    EXPECT_EQ(frame[5], 0xFF);
    EXPECT_EQ(frame[6], 0x11);
    EXPECT_EQ(frame[11], 0x66);
}

TEST_F(EthernetBuilderTest, AddsVlanTag) {
    auto frame = EthernetBuilder(rng_)
                     .with_vlan(100, 3)  // VLAN ID 100, PCP 3
                     .with_ethertype(0x0800)
                     .build();

    // Should have VLAN tag inserted
    EXPECT_EQ(frame[12], 0x81);  // TPID high
    EXPECT_EQ(frame[13], 0x00);  // TPID low
    // VLAN TCI
    std::uint16_t tci = static_cast<std::uint16_t>((frame[14] << 8) | frame[15]);
    EXPECT_EQ(tci & 0x0FFF, 100);      // VID
    EXPECT_EQ((tci >> 13) & 0x07, 3);  // PCP
    // Inner ethertype
    EXPECT_EQ(frame[16], 0x08);
    EXPECT_EQ(frame[17], 0x00);
}

TEST_F(EthernetBuilderTest, AddsPayload) {
    std::vector<std::uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    auto frame = EthernetBuilder(rng_).with_payload(payload).build();

    EXPECT_EQ(frame.size(), 14u + 4u);
    EXPECT_EQ(frame[14], 0xDE);
    EXPECT_EQ(frame[17], 0xEF);
}

TEST_F(EthernetBuilderTest, BuildPacketCreatesPacketObject) {
    auto packet = EthernetBuilder(rng_).with_ethertype(0x0800).build_packet();

    EXPECT_GE(packet.size(), 14u);
}

// =============================================================================
// IPv4 Builder Tests
// =============================================================================

class IPv4BuilderTest : public ::testing::Test {
protected:
    Random rng_{42};
};

TEST_F(IPv4BuilderTest, BuildsValidIPv4Header) {
    auto header = IPv4Builder(rng_)
                      .with_protocol(17)  // UDP
                      .build();

    // Minimum IPv4 header is 20 bytes
    EXPECT_GE(header.size(), 20u);
    // Version + IHL
    EXPECT_EQ(header[0], 0x45);
    // Protocol
    EXPECT_EQ(header[9], 17);
}

TEST_F(IPv4BuilderTest, SetsCustomIPs) {
    auto header = IPv4Builder(rng_)
                      .with_src_ip(0x0A000001)  // 10.0.0.1
                      .with_dst_ip(0x0A000002)  // 10.0.0.2
                      .build();

    EXPECT_EQ(header[12], 0x0A);  // Src IP
    EXPECT_EQ(header[15], 0x01);
    EXPECT_EQ(header[16], 0x0A);  // Dst IP
    EXPECT_EQ(header[19], 0x02);
}

TEST_F(IPv4BuilderTest, CalculatesChecksum) {
    auto header = IPv4Builder(rng_).build();

    // Verify checksum by recalculating
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < 20; i += 2) {
        sum += static_cast<std::uint32_t>((header[i] << 8) | header[i + 1]);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    EXPECT_EQ(sum, 0xFFFFu);  // Valid checksum
}

TEST_F(IPv4BuilderTest, SetsCorrectTotalLength) {
    std::vector<std::uint8_t> payload(100, 0xAA);
    auto header = IPv4Builder(rng_).with_payload(payload).build();

    std::uint16_t total_len = static_cast<std::uint16_t>((header[2] << 8) | header[3]);
    EXPECT_EQ(total_len, 120u);  // 20 header + 100 payload
}

// =============================================================================
// UDP Builder Tests
// =============================================================================

class UDPBuilderTest : public ::testing::Test {
protected:
    Random rng_{42};
};

TEST_F(UDPBuilderTest, BuildsValidUDPHeader) {
    auto datagram = UDPBuilder(rng_).with_src_port(12345).with_dst_port(54321).build();

    EXPECT_GE(datagram.size(), 8u);
    // Source port
    EXPECT_EQ((datagram[0] << 8) | datagram[1], 12345);
    // Destination port
    EXPECT_EQ((datagram[2] << 8) | datagram[3], 54321);
}

TEST_F(UDPBuilderTest, SetsCorrectLength) {
    std::vector<std::uint8_t> payload(50, 0xBB);
    auto datagram = UDPBuilder(rng_).with_payload(payload).build();

    std::uint16_t length = static_cast<std::uint16_t>((datagram[4] << 8) | datagram[5]);
    EXPECT_EQ(length, 58u);  // 8 header + 50 payload
}

// =============================================================================
// TCP Builder Tests
// =============================================================================

class TCPBuilderTest : public ::testing::Test {
protected:
    Random rng_{42};
};

TEST_F(TCPBuilderTest, BuildsValidTCPHeader) {
    auto segment =
        TCPBuilder(rng_).with_src_port(80).with_dst_port(443).with_seq(1000).with_ack(2000).build();

    EXPECT_GE(segment.size(), 20u);
    // Source port
    EXPECT_EQ((segment[0] << 8) | segment[1], 80);
    // Destination port
    EXPECT_EQ((segment[2] << 8) | segment[3], 443);
}

TEST_F(TCPBuilderTest, SetsFlags) {
    auto segment = TCPBuilder(rng_).with_syn().with_ack_flag().build();

    // Flags at byte 13
    EXPECT_EQ(segment[13] & 0x12, 0x12);  // SYN + ACK
}

TEST_F(TCPBuilderTest, CombinesMultipleFlags) {
    auto segment = TCPBuilder(rng_).with_syn().with_fin().with_psh().build();

    EXPECT_EQ(segment[13] & 0x0B, 0x0B);  // SYN + FIN + PSH
}

// =============================================================================
// SOME/IP Builder Tests
// =============================================================================

class SOMEIPBuilderTest : public ::testing::Test {
protected:
    Random rng_{42};
};

TEST_F(SOMEIPBuilderTest, BuildsValidSOMEIPHeader) {
    auto message = SOMEIPBuilder(rng_).with_service_id(0x1234).with_method_id(0x5678).build();

    EXPECT_GE(message.size(), 16u);
    // Service ID
    EXPECT_EQ((message[0] << 8) | message[1], 0x1234);
    // Method ID
    EXPECT_EQ((message[2] << 8) | message[3], 0x5678);
}

TEST_F(SOMEIPBuilderTest, SetsMessageType) {
    auto message =
        SOMEIPBuilder(rng_).with_message_type(protocols::someip::MessageType::Response).build();

    // Message type at byte 14
    EXPECT_EQ(message[14], 0x80);  // Response
}

TEST_F(SOMEIPBuilderTest, SetsCorrectLength) {
    std::vector<std::uint8_t> payload(20, 0xCC);
    auto message = SOMEIPBuilder(rng_).with_payload(payload).build();

    std::uint32_t length = static_cast<std::uint32_t>((message[4] << 24) | (message[5] << 16) |
                                                      (message[6] << 8) | message[7]);
    EXPECT_EQ(length, 28u);  // 8 (header after length) + 20 payload
}

TEST_F(SOMEIPBuilderTest, RandomMessageTypeIsValid) {
    std::set<std::uint8_t> types_seen;
    Random rng(999);

    for (int i = 0; i < 100; ++i) {
        auto message = SOMEIPBuilder(rng).with_random_message_type().build();
        types_seen.insert(message[14]);
    }

    // Should have seen multiple message types
    EXPECT_GT(types_seen.size(), 1u);
}

// =============================================================================
// DoIP Builder Tests
// =============================================================================

class DoIPBuilderTest : public ::testing::Test {
protected:
    Random rng_{42};
};

TEST_F(DoIPBuilderTest, BuildsValidDoIPHeader) {
    auto message = DoIPBuilder(rng_)
                       .with_protocol_version(0x02)
                       .with_payload_type(protocols::doip::PayloadType::DiagnosticMessage)
                       .build();

    EXPECT_GE(message.size(), 8u);
    // Protocol version
    EXPECT_EQ(message[0], 0x02);
    // Inverse version
    EXPECT_EQ(message[1], 0xFD);
}

TEST_F(DoIPBuilderTest, BuildsDiagnosticMessage) {
    auto message = DoIPBuilder(rng_)
                       .as_diagnostic_message(0x0E80, 0x1234)
                       .with_payload({0x22, 0xF1, 0x90})  // UDS Read Data By ID
                       .build();

    // Payload type should be diagnostic message (0x8001)
    EXPECT_EQ((message[2] << 8) | message[3], 0x8001);
    // Source address
    EXPECT_EQ((message[8] << 8) | message[9], 0x0E80);
    // Target address
    EXPECT_EQ((message[10] << 8) | message[11], 0x1234);
}

// =============================================================================
// Packet Generator Tests
// =============================================================================

class PacketGeneratorTest : public ::testing::Test {
protected:
    PacketGenerator gen_{42};
};

TEST_F(PacketGeneratorTest, GeneratesUDPPacket) {
    auto packet = gen_.udp_packet(10);

    EXPECT_GT(packet.size(), 0u);
    EXPECT_THAT(packet, IsUDP());
}

TEST_F(PacketGeneratorTest, GeneratesTCPPacket) {
    auto packet = gen_.tcp_packet(10);

    EXPECT_GT(packet.size(), 0u);
    EXPECT_THAT(packet, IsTCP());
}

TEST_F(PacketGeneratorTest, GeneratesSOMEIPPacket) {
    auto packet = gen_.someip_udp_packet(5);

    EXPECT_GT(packet.size(), 0u);
    EXPECT_THAT(packet, IsUDP());
}

TEST_F(PacketGeneratorTest, GeneratesSOMEIPServicePacket) {
    auto packet = gen_.someip_service_packet(0xABCD, protocols::someip::MessageType::Request);

    EXPECT_THAT(packet, HasSOMEIPServiceId(0xABCD));
    EXPECT_THAT(packet, IsSOMEIPRequest());
}

TEST_F(PacketGeneratorTest, GeneratesDoIPDiagnosticPacket) {
    auto packet = gen_.doip_diagnostic_packet(0x0E80, 0x1234, {0x10, 0x01});

    EXPECT_GT(packet.size(), 0u);
    EXPECT_THAT(packet, IsTCP());
}

TEST_F(PacketGeneratorTest, GeneratesTruncatedPacket) {
    auto packet = gen_.truncated_packet(10);

    EXPECT_LE(packet.size(), 10u);
}

TEST_F(PacketGeneratorTest, GeneratesRandomBytes) {
    auto packet = gen_.random_bytes(50);

    EXPECT_EQ(packet.size(), 50u);
}

TEST_F(PacketGeneratorTest, SeedProducesReproduciblePackets) {
    PacketGenerator gen1(12345);
    PacketGenerator gen2(12345);

    auto pkt1 = gen1.udp_packet(10);
    auto pkt2 = gen2.udp_packet(10);

    EXPECT_EQ(pkt1.size(), pkt2.size());
    auto data1 = pkt1.data();
    auto data2 = pkt2.data();
    EXPECT_TRUE(std::equal(data1.begin(), data1.end(), data2.begin()));
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST(PropertyBasedTest, AllGeneratedUDPPacketsAreUDP) {
    PacketGenerator gen(std::random_device{}());

    for (std::size_t i = 0; i < 100; ++i) {
        auto packet = gen.udp_packet(i % 50);
        EXPECT_THAT(packet, IsUDP()) << "Failed at iteration " << i;
    }
}

TEST(PropertyBasedTest, AllGeneratedTCPPacketsAreTCP) {
    PacketGenerator gen(std::random_device{}());

    for (std::size_t i = 0; i < 100; ++i) {
        auto packet = gen.tcp_packet(i % 50);
        EXPECT_THAT(packet, IsTCP()) << "Failed at iteration " << i;
    }
}

TEST(PropertyBasedTest, SOMEIPServiceIdIsPreserved) {
    PacketGenerator gen(std::random_device{}());

    for (int i = 0; i < 100; ++i) {
        std::uint16_t service_id = static_cast<std::uint16_t>(i * 17 + 1);
        auto packet = gen.someip_service_packet(service_id);
        EXPECT_THAT(packet, HasSOMEIPServiceId(service_id))
            << "Service ID not preserved at iteration " << i;
    }
}

TEST(PropertyBasedTest, GeneratedPacketsAreDecodable) {
    PacketGenerator gen(std::random_device{}());
    protocols::ProtocolDispatcher dispatcher;

    for (int i = 0; i < 50; ++i) {
        auto packet = gen.udp_packet(10);
        auto result = dispatcher.decode(packet.data());

        EXPECT_TRUE(result.has_layer<protocols::ethernet::EthernetHeader>());
        EXPECT_TRUE(result.has_layer<protocols::ipv4::IPv4Header>());
        EXPECT_TRUE(result.has_layer<protocols::udp::UdpHeader>());
    }
}

#include <gtest/gtest.h>
#include <wadjet/net/packet.hpp>
#include <wadjet/net/packet_view.hpp>

namespace wadjet::test {

// Sample Ethernet frame with IPv4/UDP payload
// Ethernet: dst=ff:ff:ff:ff:ff:ff, src=00:11:22:33:44:55, type=0x0800 (IPv4)
// IPv4: src=192.168.1.1, dst=192.168.1.255, protocol=17 (UDP)
// UDP: src_port=12345, dst_port=30490
constexpr std::uint8_t SAMPLE_UDP_FRAME[] = {
    // Ethernet header (14 bytes)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // dst MAC (broadcast)
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // src MAC
    0x08, 0x00,                          // EtherType: IPv4

    // IPv4 header (20 bytes)
    0x45,                    // Version (4) + IHL (5)
    0x00,                    // DSCP + ECN
    0x00, 0x1c,              // Total length (28)
    0x00, 0x00,              // Identification
    0x40, 0x00,              // Flags (DF) + Fragment offset
    0x40,                    // TTL (64)
    0x11,                    // Protocol: UDP
    0x00, 0x00,              // Checksum (not calculated)
    0xc0, 0xa8, 0x01, 0x01,  // src IP: 192.168.1.1
    0xc0, 0xa8, 0x01, 0xff,  // dst IP: 192.168.1.255

    // UDP header (8 bytes)
    0x30, 0x39,  // src port: 12345
    0x77, 0x1a,  // dst port: 30490
    0x00, 0x08,  // length: 8 (header only, no payload)
    0x00, 0x00,  // checksum (not calculated)
};

class PacketViewTest : public ::testing::Test {
protected:
    PacketView view{std::span<const std::uint8_t>(SAMPLE_UDP_FRAME), Timestamp::now()};
};

TEST_F(PacketViewTest, Size) {
    EXPECT_EQ(view.size(), sizeof(SAMPLE_UDP_FRAME));
}

TEST_F(PacketViewTest, EthernetHeader) {
    auto eth = view.ethernet_header();
    ASSERT_TRUE(eth.has_value());

    EXPECT_TRUE(eth->dst.is_broadcast());
    EXPECT_EQ(eth->src.bytes[0], 0x00);
    EXPECT_EQ(eth->src.bytes[1], 0x11);
    EXPECT_EQ(eth->ether_type, EtherType::IPv4);
}

TEST_F(PacketViewTest, NoVlan) {
    EXPECT_FALSE(view.has_vlan());
    EXPECT_FALSE(view.vlan_tag().has_value());
}

TEST_F(PacketViewTest, IPv4Header) {
    auto ip = view.ipv4_header();
    ASSERT_TRUE(ip.has_value());

    EXPECT_EQ(ip->version, 4);
    EXPECT_EQ(ip->ihl, 5);
    EXPECT_EQ(ip->protocol, IpProtocol::UDP);
    EXPECT_EQ(ip->ttl, 64);
    EXPECT_TRUE(ip->dont_fragment);

    EXPECT_EQ(ip->src.bytes[0], 192);
    EXPECT_EQ(ip->src.bytes[1], 168);
    EXPECT_EQ(ip->src.bytes[2], 1);
    EXPECT_EQ(ip->src.bytes[3], 1);

    EXPECT_EQ(ip->dst.bytes[0], 192);
    EXPECT_EQ(ip->dst.bytes[1], 168);
    EXPECT_EQ(ip->dst.bytes[2], 1);
    EXPECT_EQ(ip->dst.bytes[3], 255);
}

TEST_F(PacketViewTest, UdpHeader) {
    auto udp = view.udp_header();
    ASSERT_TRUE(udp.has_value());

    EXPECT_EQ(udp->src_port, 12345);
    EXPECT_EQ(udp->dst_port, 30490);
    EXPECT_EQ(udp->length, 8);
}

TEST_F(PacketViewTest, NoTcpHeader) {
    auto tcp = view.tcp_header();
    EXPECT_FALSE(tcp.has_value());
}

TEST_F(PacketViewTest, L3Offset) {
    EXPECT_EQ(view.l3_offset(), ETHERNET_HEADER_SIZE);
}

TEST_F(PacketViewTest, L4Offset) {
    EXPECT_EQ(view.l4_offset(), ETHERNET_HEADER_SIZE + 20);  // 14 + 20
}

TEST_F(PacketViewTest, Subview) {
    auto sub = view.subview(14);  // Skip Ethernet header
    EXPECT_EQ(sub.size(), view.size() - 14);

    auto sub2 = view.subview(14, 20);  // Just IP header
    EXPECT_EQ(sub2.size(), 20);
}

TEST_F(PacketViewTest, EmptyPacket) {
    PacketView empty(ByteSpan{});
    EXPECT_TRUE(empty.empty());
    EXPECT_FALSE(empty.ethernet_header().has_value());
    EXPECT_FALSE(empty.ipv4_header().has_value());
}

// ============================================================================
// Packet Tests
// ============================================================================

TEST(Packet, DefaultConstructor) {
    Packet pkt;
    EXPECT_TRUE(pkt.empty());
    EXPECT_EQ(pkt.size(), 0);
}

TEST(Packet, ConstructWithCapacity) {
    Packet pkt(1500);
    EXPECT_TRUE(pkt.empty());
    EXPECT_GE(pkt.capacity(), 1500);
}

TEST(Packet, ConstructFromSpan) {
    Packet pkt{std::span<const std::uint8_t>(SAMPLE_UDP_FRAME, sizeof(SAMPLE_UDP_FRAME))};
    EXPECT_EQ(pkt.size(), sizeof(SAMPLE_UDP_FRAME));
}

TEST(Packet, ConstructFromView) {
    PacketView view{std::span<const std::uint8_t>(SAMPLE_UDP_FRAME, sizeof(SAMPLE_UDP_FRAME))};
    Packet pkt{view};
    EXPECT_EQ(pkt.size(), view.size());
}

TEST(Packet, ViewConversion) {
    Packet pkt{std::span<const std::uint8_t>(SAMPLE_UDP_FRAME, sizeof(SAMPLE_UDP_FRAME))};
    auto view = pkt.view();

    auto eth = view.ethernet_header();
    ASSERT_TRUE(eth.has_value());
    EXPECT_TRUE(eth->dst.is_broadcast());
}

TEST(Packet, Resize) {
    Packet pkt;
    pkt.resize(100);
    EXPECT_EQ(pkt.size(), 100);
}

TEST(Packet, Clear) {
    Packet pkt{std::span<const std::uint8_t>(SAMPLE_UDP_FRAME, sizeof(SAMPLE_UDP_FRAME))};
    pkt.clear();
    EXPECT_TRUE(pkt.empty());
}

TEST(Packet, Timestamp) {
    auto ts = Timestamp::now();
    Packet pkt(ByteSpan{}, ts);
    EXPECT_EQ(pkt.timestamp(), ts);

    auto new_ts = Timestamp::from_unix(12345, 0);
    pkt.set_timestamp(new_ts);
    EXPECT_EQ(pkt.timestamp(), new_ts);
}

}  // namespace wadjet::test

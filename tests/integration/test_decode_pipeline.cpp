/// @file test_decode_pipeline.cpp
/// @brief Integration tests for the full decode pipeline

#include "wadjet/net/packet.hpp"
#include "wadjet/net/packet_view.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/udp.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;

namespace {

/// Helper to build packet data
class PacketBuilder {
public:
    // Add Ethernet header
    PacketBuilder& ethernet(const std::array<std::uint8_t, 6>& dst,
                            const std::array<std::uint8_t, 6>& src, std::uint16_t ethertype) {
        data_.insert(data_.end(), dst.begin(), dst.end());
        data_.insert(data_.end(), src.begin(), src.end());
        data_.push_back(static_cast<std::uint8_t>(ethertype >> 8));
        data_.push_back(static_cast<std::uint8_t>(ethertype & 0xFF));
        return *this;
    }

    // Add Ethernet header with default MACs
    PacketBuilder& ethernet_ipv4() {
        return ethernet({0x00, 0x11, 0x22, 0x33, 0x44, 0x55}, {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB},
                        0x0800  // IPv4
        );
    }

    // Add IPv4 header (20 bytes, no options)
    PacketBuilder& ipv4(const std::array<std::uint8_t, 4>& src,
                        const std::array<std::uint8_t, 4>& dst, std::uint8_t protocol,
                        std::uint16_t payload_len) {
        std::uint16_t total_len = 20 + payload_len;

        data_.push_back(0x45);  // Version 4, IHL 5
        data_.push_back(0x00);  // DSCP/ECN
        data_.push_back(static_cast<std::uint8_t>(total_len >> 8));
        data_.push_back(static_cast<std::uint8_t>(total_len & 0xFF));
        data_.push_back(0x00);  // ID high
        data_.push_back(0x01);  // ID low
        data_.push_back(0x40);  // Don't fragment
        data_.push_back(0x00);  // Fragment offset
        data_.push_back(64);    // TTL
        data_.push_back(protocol);
        data_.push_back(0x00);  // Checksum (will be invalid, but we disable validation)
        data_.push_back(0x00);
        data_.insert(data_.end(), src.begin(), src.end());
        data_.insert(data_.end(), dst.begin(), dst.end());
        return *this;
    }

    // Add UDP header
    PacketBuilder& udp(std::uint16_t src_port, std::uint16_t dst_port, std::uint16_t payload_len) {
        std::uint16_t length = 8 + payload_len;

        data_.push_back(static_cast<std::uint8_t>(src_port >> 8));
        data_.push_back(static_cast<std::uint8_t>(src_port & 0xFF));
        data_.push_back(static_cast<std::uint8_t>(dst_port >> 8));
        data_.push_back(static_cast<std::uint8_t>(dst_port & 0xFF));
        data_.push_back(static_cast<std::uint8_t>(length >> 8));
        data_.push_back(static_cast<std::uint8_t>(length & 0xFF));
        data_.push_back(0x00);  // Checksum
        data_.push_back(0x00);
        return *this;
    }

    // Add TCP header (20 bytes minimum)
    PacketBuilder& tcp(std::uint16_t src_port, std::uint16_t dst_port, std::uint32_t seq,
                       std::uint32_t ack, std::uint8_t flags) {
        data_.push_back(static_cast<std::uint8_t>(src_port >> 8));
        data_.push_back(static_cast<std::uint8_t>(src_port & 0xFF));
        data_.push_back(static_cast<std::uint8_t>(dst_port >> 8));
        data_.push_back(static_cast<std::uint8_t>(dst_port & 0xFF));

        // Sequence number
        data_.push_back(static_cast<std::uint8_t>(seq >> 24));
        data_.push_back(static_cast<std::uint8_t>(seq >> 16));
        data_.push_back(static_cast<std::uint8_t>(seq >> 8));
        data_.push_back(static_cast<std::uint8_t>(seq & 0xFF));

        // Ack number
        data_.push_back(static_cast<std::uint8_t>(ack >> 24));
        data_.push_back(static_cast<std::uint8_t>(ack >> 16));
        data_.push_back(static_cast<std::uint8_t>(ack >> 8));
        data_.push_back(static_cast<std::uint8_t>(ack & 0xFF));

        data_.push_back(0x50);  // Data offset 5 (20 bytes)
        data_.push_back(flags);
        data_.push_back(0xFF);  // Window high
        data_.push_back(0xFF);  // Window low
        data_.push_back(0x00);  // Checksum
        data_.push_back(0x00);
        data_.push_back(0x00);  // Urgent pointer
        data_.push_back(0x00);
        return *this;
    }

    // Add SOME/IP header
    PacketBuilder& someip(std::uint16_t service_id, std::uint16_t method_id,
                          std::uint16_t client_id, std::uint16_t session_id,
                          someip::MessageType msg_type, std::uint16_t payload_len) {
        // Service ID and Method ID
        data_.push_back(static_cast<std::uint8_t>(service_id >> 8));
        data_.push_back(static_cast<std::uint8_t>(service_id & 0xFF));
        data_.push_back(static_cast<std::uint8_t>(method_id >> 8));
        data_.push_back(static_cast<std::uint8_t>(method_id & 0xFF));

        // Length (8 bytes header remainder + payload)
        std::uint32_t length = 8 + payload_len;
        data_.push_back(static_cast<std::uint8_t>(length >> 24));
        data_.push_back(static_cast<std::uint8_t>(length >> 16));
        data_.push_back(static_cast<std::uint8_t>(length >> 8));
        data_.push_back(static_cast<std::uint8_t>(length & 0xFF));

        // Client ID and Session ID
        data_.push_back(static_cast<std::uint8_t>(client_id >> 8));
        data_.push_back(static_cast<std::uint8_t>(client_id & 0xFF));
        data_.push_back(static_cast<std::uint8_t>(session_id >> 8));
        data_.push_back(static_cast<std::uint8_t>(session_id & 0xFF));

        // Protocol version, Interface version, Message type, Return code
        data_.push_back(0x01);  // Protocol version
        data_.push_back(0x01);  // Interface version
        data_.push_back(static_cast<std::uint8_t>(msg_type));
        data_.push_back(0x00);  // Return code OK
        return *this;
    }

    // Add DoIP header
    PacketBuilder& doip(std::uint8_t version, doip::PayloadType type, std::uint32_t payload_len) {
        data_.push_back(version);
        data_.push_back(~version);  // Inverse version
        data_.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(type) >> 8));
        data_.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(type) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>(payload_len >> 24));
        data_.push_back(static_cast<std::uint8_t>(payload_len >> 16));
        data_.push_back(static_cast<std::uint8_t>(payload_len >> 8));
        data_.push_back(static_cast<std::uint8_t>(payload_len & 0xFF));
        return *this;
    }

    // Add raw payload
    PacketBuilder& payload(const std::vector<std::uint8_t>& bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
        return *this;
    }

    PacketBuilder& payload(std::initializer_list<std::uint8_t> bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
        return *this;
    }

    // Build as byte span
    std::span<const std::byte> build() const {
        return std::span<const std::byte>(reinterpret_cast<const std::byte*>(data_.data()),
                                          data_.size());
    }

    const std::vector<std::uint8_t>& data() const { return data_; }

private:
    std::vector<std::uint8_t> data_;
};

}  // namespace

//==============================================================================
// Integration Test: Full Decode Pipeline
//==============================================================================

class DecodePipelineTest : public ::testing::Test {
protected:
    ProtocolDispatcher dispatcher;
};

TEST_F(DecodePipelineTest, DecodeEthernetIPv4UdpSomeIP) {
    // Build: Ethernet + IPv4 + UDP + SOME/IP
    // SOME/IP header is 16 bytes, payload is 8 bytes
    PacketBuilder builder;
    builder.ethernet_ipv4()
        .ipv4({192, 168, 1, 10}, {192, 168, 1, 20}, 17, 8 + 16 + 8)  // UDP + SOMEIP + payload
        .udp(30490, 30490, 16 + 8)                                   // SOME/IP-SD port
        .someip(0x1234, 0x8001, 0x0001, 0x0001, someip::MessageType::Request, 8)
        .payload({0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE});
    auto packet = builder.build();

    auto result = dispatcher.decode(packet);

    EXPECT_TRUE(result.complete) << "Decode should complete successfully";
    EXPECT_FALSE(result.error.has_value()) << "Should have no errors";
    EXPECT_GE(result.layers.size(), 4u) << "Should have at least 4 layers";

    // Verify Ethernet layer
    ASSERT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    auto* eth = result.get_layer<ethernet::EthernetHeader>();
    EXPECT_EQ(eth->ethertype, 0x0800);

    // Verify IPv4 layer
    ASSERT_TRUE(result.has_layer<ipv4::IPv4Header>());
    auto* ip = result.get_layer<ipv4::IPv4Header>();
    EXPECT_EQ(ip->src_ip.to_string(), "192.168.1.10");
    EXPECT_EQ(ip->dst_ip.to_string(), "192.168.1.20");
    EXPECT_EQ(ip->protocol, 17);  // UDP

    // Verify UDP layer
    ASSERT_TRUE(result.has_layer<udp::UdpHeader>());
    auto* udp_hdr = result.get_layer<udp::UdpHeader>();
    EXPECT_EQ(udp_hdr->src_port, 30490);
    EXPECT_EQ(udp_hdr->dst_port, 30490);

    // Verify SOME/IP layer
    ASSERT_TRUE(result.has_layer<someip::SomeIpHeader>());
    auto* someip_hdr = result.get_layer<someip::SomeIpHeader>();
    EXPECT_EQ(someip_hdr->service_id, 0x1234);
    EXPECT_EQ(someip_hdr->method_id, 0x8001);
}

TEST_F(DecodePipelineTest, DecodeEthernetIPv4TcpDoIP) {
    // Build: Ethernet + IPv4 + TCP + DoIP diagnostic message
    PacketBuilder builder;
    builder.ethernet_ipv4()
        .ipv4({10, 0, 0, 1}, {10, 0, 0, 2}, 6, 20 + 8 + 5)  // TCP + DoIP + payload
        .tcp(12345, 13400, 1000, 0, 0x18)                   // DoIP port, PSH+ACK
        .doip(0x02, doip::PayloadType::DiagnosticMessage, 5)
        .payload({0x0E, 0x80, 0x10, 0x01, 0x3E});  // Source addr, target addr, UDS data
    auto packet = builder.build();

    auto result = dispatcher.decode(packet);

    EXPECT_TRUE(result.complete);
    EXPECT_GE(result.layers.size(), 4u);

    // Verify all layers present
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<ipv4::IPv4Header>());
    EXPECT_TRUE(result.has_layer<tcp::TcpHeader>());
    EXPECT_TRUE(result.has_layer<doip::DoIPHeader>());

    // Verify DoIP
    auto* doip_hdr = result.get_layer<doip::DoIPHeader>();
    ASSERT_NE(doip_hdr, nullptr);
    EXPECT_EQ(doip_hdr->payload_type, doip::PayloadType::DiagnosticMessage);
}

TEST_F(DecodePipelineTest, DecodeStopsOnMalformedIPv4) {
    // Build packet with invalid IPv4 version
    PacketBuilder eth_builder;
    eth_builder.ethernet_ipv4();
    auto eth_data = eth_builder.data();

    // Corrupt IPv4 version (byte 14 should be 0x45, change to 0x65 = version 6)
    std::vector<std::uint8_t> corrupted(eth_data.begin(), eth_data.end());
    corrupted.push_back(0x65);  // Bad version
    for (int i = 0; i < 19; i++)
        corrupted.push_back(0x00);  // Rest of IPv4 header

    auto result = dispatcher.decode(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(corrupted.data()), corrupted.size()));

    // Should decode Ethernet but fail on IPv4
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_FALSE(result.has_layer<ipv4::IPv4Header>());
    EXPECT_TRUE(result.error.has_value());
}

TEST_F(DecodePipelineTest, DecodeVlanTaggedPacket) {
    // Build VLAN-tagged packet
    std::vector<std::uint8_t> data = {// Dst MAC
                                      0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                      // Src MAC
                                      0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
                                      // VLAN tag (0x8100)
                                      0x81, 0x00,
                                      // TCI: PCP=5, DEI=0, VID=100
                                      0xA0, 0x64,
                                      // Ethertype: IPv4
                                      0x08, 0x00,
                                      // Minimal IPv4 header (20 bytes)
                                      0x45, 0x00, 0x00, 0x1C, 0x00, 0x01, 0x40, 0x00, 0x40, 0x11,
                                      0x00, 0x00, 0xC0, 0xA8, 0x01, 0x01,  // 192.168.1.1
                                      0xC0, 0xA8, 0x01, 0x02,              // 192.168.1.2
                                                                           // UDP header
                                      0x13, 0x88, 0x13, 0x88,              // Port 5000
                                      0x00, 0x08, 0x00, 0x00};

    auto result = dispatcher.decode(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()));

    EXPECT_TRUE(result.complete);

    auto* eth = result.get_layer<ethernet::EthernetHeader>();
    ASSERT_NE(eth, nullptr);
    EXPECT_TRUE(eth->has_vlan());
    ASSERT_TRUE(eth->vlan.has_value());
    EXPECT_EQ(eth->vlan->vid(), 100);
    EXPECT_EQ(eth->vlan->pcp(), 5);
}

TEST_F(DecodePipelineTest, PayloadRemainsAfterDecode) {
    // Build packet with known payload
    PacketBuilder builder;
    builder.ethernet_ipv4()
        .ipv4({1, 2, 3, 4}, {5, 6, 7, 8}, 17, 8 + 4)
        .udp(1234, 5678, 4)
        .payload({0x01, 0x02, 0x03, 0x04});
    auto packet = builder.build();

    auto result = dispatcher.decode(packet);

    EXPECT_TRUE(result.complete);
    EXPECT_EQ(result.payload.size(), 4u);
    EXPECT_EQ(static_cast<std::uint8_t>(result.payload[0]), 0x01);
    EXPECT_EQ(static_cast<std::uint8_t>(result.payload[3]), 0x04);
}

//==============================================================================
// Integration Test: Decode Multiple Packets
//==============================================================================

class PacketViewToDispatcherTest : public ::testing::Test {
protected:
    ProtocolDispatcher dispatcher;
};

TEST_F(PacketViewToDispatcherTest, DecodeFromRawBytes) {
    PacketBuilder builder;
    builder.ethernet_ipv4().ipv4({192, 168, 0, 1}, {192, 168, 0, 2}, 17, 8).udp(12345, 54321, 0);
    auto packet = builder.build();

    // Decode raw bytes
    auto result = dispatcher.decode(packet);

    EXPECT_TRUE(result.complete);
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<ipv4::IPv4Header>());
    EXPECT_TRUE(result.has_layer<udp::UdpHeader>());
}

//==============================================================================
// Integration Test: Multiple Packets Decode
//==============================================================================

TEST_F(DecodePipelineTest, DecodeMultiplePacketsConsistently) {
    // Verify dispatcher works correctly for multiple sequential decodes
    PacketBuilder builder1;
    builder1.ethernet_ipv4().ipv4({10, 0, 0, 1}, {10, 0, 0, 2}, 17, 8).udp(1000, 2000, 0);
    auto pkt1 = builder1.build();

    PacketBuilder builder2;
    builder2.ethernet_ipv4()
        .ipv4({10, 0, 0, 3}, {10, 0, 0, 4}, 6, 20)
        .tcp(3000, 4000, 100, 200, 0x02);
    auto pkt2 = builder2.build();

    PacketBuilder builder3;
    builder3.ethernet_ipv4()
        .ipv4({10, 0, 0, 5}, {10, 0, 0, 6}, 17, 8 + 16)
        .udp(30490, 30490, 16)
        .someip(0xABCD, 0x0001, 0x0001, 0x0001, someip::MessageType::Notification, 0);
    auto pkt3 = builder3.build();

    // Decode all packets
    auto r1 = dispatcher.decode(pkt1);
    auto r2 = dispatcher.decode(pkt2);
    auto r3 = dispatcher.decode(pkt3);

    // Verify each decoded correctly
    EXPECT_TRUE(r1.complete);
    EXPECT_TRUE(r1.has_layer<udp::UdpHeader>());
    EXPECT_FALSE(r1.has_layer<tcp::TcpHeader>());

    EXPECT_TRUE(r2.complete);
    EXPECT_TRUE(r2.has_layer<tcp::TcpHeader>());
    EXPECT_FALSE(r2.has_layer<udp::UdpHeader>());

    EXPECT_TRUE(r3.complete);
    EXPECT_TRUE(r3.has_layer<someip::SomeIpHeader>());

    auto* someip_hdr = r3.get_layer<someip::SomeIpHeader>();
    EXPECT_EQ(someip_hdr->service_id, 0xABCD);
}

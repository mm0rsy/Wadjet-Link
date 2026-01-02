/// @file test_dds.cpp
/// @brief Unit tests for DDS/RTPS protocol decoder

#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/dds/rtps_messages.hpp"
#include "wadjet/protocols/dds/rtps_types.hpp"
#include "wadjet/protocols/dds/discovery.hpp"
#include "wadjet/protocols/dispatcher.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <vector>

namespace wadjet::protocols::dds {
namespace {

// Helper to create byte vector from initializer list
template <typename... Args>
std::vector<std::byte> make_bytes(Args... args) {
    return {static_cast<std::byte>(args)...};
}

// Helper to create a basic RTPS header (20 bytes)
// Format: "RTPS" + version(2) + vendor(2) + guidPrefix(12)
std::vector<std::byte> make_rtps_header(std::uint8_t major_ver = 2, std::uint8_t minor_ver = 4,
                                        [[maybe_unused]] VendorId vendor = VendorId::Eprosima) {
    auto hdr = make_bytes(
        // Magic: "RTPS"
        'R', 'T', 'P', 'S',
        // Protocol version
        major_ver, minor_ver,
        // Vendor ID (eProsima Fast DDS = 0x0101)
        0x01, 0x01,
        // GUID Prefix (12 bytes)
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C
    );
    
    return hdr;
}

// Helper to create a DATA submessage
std::vector<std::byte> make_data_submessage(
    std::uint16_t extra_flags = 0,
    std::uint16_t writer_sn_low = 1) {
    
    // Submessage header (4 bytes)
    // DATA = 0x15, flags = 0x05 (little-endian + data present)
    // Length = 24 (minimum: 20 header + 4 padding)
    std::vector<std::byte> submsg = make_bytes(
        0x15,       // DATA submessage kind
        0x05,       // Flags: endian=little, dataPresent=true
        0x18, 0x00  // Length = 24 (little-endian)
    );
    
    // DATA body (20 bytes minimum)
    auto body = make_bytes(
        // Extra flags (2 bytes)
        static_cast<uint8_t>(extra_flags & 0xFF),
        static_cast<uint8_t>((extra_flags >> 8) & 0xFF),
        // Octets to inline QoS (2 bytes)
        0x10, 0x00,  // 16 bytes offset
        // Reader Entity ID (4 bytes)
        0x00, 0x00, 0x01, 0x04,  // Reader, builtin
        // Writer Entity ID (4 bytes)
        0x00, 0x00, 0x02, 0x03,  // Writer, builtin
        // Writer Sequence Number (8 bytes)
        0x00, 0x00, 0x00, 0x00,  // high
        static_cast<uint8_t>(writer_sn_low), 0x00, 0x00, 0x00,  // low
        // Serialized data (4 bytes padding)
        0x00, 0x00, 0x00, 0x00
    );
    
    submsg.insert(submsg.end(), body.begin(), body.end());
    return submsg;
}

// Helper to create a HEARTBEAT submessage
std::vector<std::byte> make_heartbeat_submessage(
    std::uint32_t first_sn = 1,
    std::uint32_t last_sn = 10,
    std::int32_t count = 5) {
    
    // Submessage header
    std::vector<std::byte> submsg = make_bytes(
        0x07,       // HEARTBEAT submessage kind
        0x01,       // Flags: endian=little
        0x1C, 0x00  // Length = 28
    );
    
    // HEARTBEAT body (28 bytes)
    auto body = make_bytes(
        // Reader Entity ID (4 bytes)
        0x00, 0x00, 0x01, 0x04,
        // Writer Entity ID (4 bytes)
        0x00, 0x00, 0x02, 0x03,
        // First Sequence Number (8 bytes)
        0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>(first_sn), 0x00, 0x00, 0x00,
        // Last Sequence Number (8 bytes)
        0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>(last_sn), 0x00, 0x00, 0x00,
        // Count (4 bytes)
        static_cast<uint8_t>(count), 0x00, 0x00, 0x00
    );
    
    submsg.insert(submsg.end(), body.begin(), body.end());
    return submsg;
}

// Helper to create INFO_TS submessage
std::vector<std::byte> make_info_ts_submessage(
    std::int32_t seconds = 1000,
    std::uint32_t fraction = 500000) {
    
    // Submessage header
    std::vector<std::byte> submsg = make_bytes(
        0x09,       // INFO_TS submessage kind
        0x01,       // Flags: endian=little (timestamp present)
        0x08, 0x00  // Length = 8
    );
    
    // Timestamp (8 bytes)
    auto body = make_bytes(
        static_cast<uint8_t>(seconds & 0xFF),
        static_cast<uint8_t>((seconds >> 8) & 0xFF),
        static_cast<uint8_t>((seconds >> 16) & 0xFF),
        static_cast<uint8_t>((seconds >> 24) & 0xFF),
        static_cast<uint8_t>(fraction & 0xFF),
        static_cast<uint8_t>((fraction >> 8) & 0xFF),
        static_cast<uint8_t>((fraction >> 16) & 0xFF),
        static_cast<uint8_t>((fraction >> 24) & 0xFF)
    );
    
    submsg.insert(submsg.end(), body.begin(), body.end());
    return submsg;
}

// Helper to create INFO_DST submessage
std::vector<std::byte> make_info_dst_submessage() {
    // Submessage header
    std::vector<std::byte> submsg = make_bytes(
        0x0E,       // INFO_DST submessage kind
        0x01,       // Flags: endian=little
        0x0C, 0x00  // Length = 12
    );
    
    // GUID Prefix (12 bytes)
    auto body = make_bytes(
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC
    );
    
    submsg.insert(submsg.end(), body.begin(), body.end());
    return submsg;
}

// Create a complete RTPS message with submessages
std::vector<std::byte> make_rtps_message_with_data() {
    auto msg = make_rtps_header();
    
    // Add INFO_TS submessage
    auto info_ts = make_info_ts_submessage();
    msg.insert(msg.end(), info_ts.begin(), info_ts.end());
    
    // Add DATA submessage
    auto data = make_data_submessage();
    msg.insert(msg.end(), data.begin(), data.end());
    
    return msg;
}

std::vector<std::byte> make_rtps_message_with_heartbeat() {
    auto msg = make_rtps_header();
    
    // Add HEARTBEAT submessage
    auto hb = make_heartbeat_submessage();
    msg.insert(msg.end(), hb.begin(), hb.end());
    
    return msg;
}

class RtpsDecoderTest : public ::testing::Test {
protected:
    RtpsDecoder decoder_;
    DecodeContext ctx_;

    void SetUp() override {
        ctx_.original_offset = 0;
    }

    void set_data(const std::vector<std::byte>& data) {
        data_ = data;
        ctx_.data = std::span<const std::byte>(data_);
    }

private:
    std::vector<std::byte> data_;
};

// =============================================================================
// Basic Header Tests
// =============================================================================

TEST_F(RtpsDecoderTest, DecodeMinimalHeader) {
    auto msg = make_rtps_header();
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    EXPECT_EQ(header.version.major, 2);
    EXPECT_EQ(header.version.minor, 4);
    EXPECT_EQ(header.vendor_id.to_enum(), VendorId::RTI);
}

TEST_F(RtpsDecoderTest, DecodeHeaderWithSubmessages) {
    auto msg = make_rtps_message_with_data();
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    EXPECT_EQ(header.version.major, 2);
    EXPECT_GE(header.submessages.size(), 1);
}

TEST_F(RtpsDecoderTest, RejectInvalidMagic) {
    auto msg = make_bytes(
        'R', 'T', 'P', 'X',  // Invalid magic
        0x02, 0x04,
        0x01, 0x01,
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C
    );
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(RtpsDecoderTest, RejectTooSmallBuffer) {
    auto msg = make_bytes('R', 'T', 'P', 'S', 0x02, 0x04);  // Only 6 bytes
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

// =============================================================================
// Submessage Parsing Tests
// =============================================================================

TEST_F(RtpsDecoderTest, ParseDataSubmessage) {
    auto msg = make_rtps_header();
    auto data_submsg = make_data_submessage();
    msg.insert(msg.end(), data_submsg.begin(), data_submsg.end());
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    ASSERT_EQ(header.submessages.size(), 1);
    EXPECT_EQ(header.submessages[0].header.kind, SubmessageKind::DATA);
    EXPECT_TRUE(header.submessages[0].header.flags.is_little_endian());
}

TEST_F(RtpsDecoderTest, ParseHeartbeatSubmessage) {
    auto msg = make_rtps_message_with_heartbeat();
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    ASSERT_EQ(header.submessages.size(), 1);
    EXPECT_EQ(header.submessages[0].header.kind, SubmessageKind::HEARTBEAT);
    
    // Check body was parsed
    auto* hb = std::get_if<HeartbeatSubmessage>(&header.submessages[0].body);
    ASSERT_NE(hb, nullptr);
    EXPECT_EQ(hb->first_sn.value(), 1);
    EXPECT_EQ(hb->last_sn.value(), 10);
    EXPECT_EQ(hb->count.value, 5);
}

TEST_F(RtpsDecoderTest, ParseMultipleSubmessages) {
    auto msg = make_rtps_header();
    
    // Add INFO_DST
    auto info_dst = make_info_dst_submessage();
    msg.insert(msg.end(), info_dst.begin(), info_dst.end());
    
    // Add HEARTBEAT
    auto hb = make_heartbeat_submessage();
    msg.insert(msg.end(), hb.begin(), hb.end());
    
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    ASSERT_EQ(header.submessages.size(), 2);
    EXPECT_EQ(header.submessages[0].header.kind, SubmessageKind::INFO_DST);
    EXPECT_EQ(header.submessages[1].header.kind, SubmessageKind::HEARTBEAT);
}

// =============================================================================
// Type Tests
// =============================================================================

TEST(RtpsTypesTest, GuidPrefixToString) {
    GuidPrefix prefix;
    prefix.value = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};

    std::string str = prefix.to_string();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("01"), std::string::npos);
}

TEST(RtpsTypesTest, EntityIdComparison) {
    EntityId id1, id2;
    id1.entity_key = {0x00, 0x00, 0x01};
    id1.kind = EntityKind::BuiltinParticipant;

    id2.entity_key = {0x00, 0x00, 0x01};
    id2.kind = EntityKind::BuiltinParticipant;

    EXPECT_EQ(id1, id2);

    id2.kind = EntityKind::BuiltinWriterNoKey;
    EXPECT_NE(id1, id2);
}

TEST(RtpsTypesTest, SequenceNumberValue) {
    SequenceNumber sn;
    sn.high = 0;
    sn.low = 42;
    EXPECT_EQ(sn.value(), 42);
    
    sn.high = 1;
    sn.low = 0;
    EXPECT_EQ(sn.value(), 0x100000000LL);
}

TEST(RtpsTypesTest, SequenceNumberComparison) {
    SequenceNumber sn1{0, 100};
    SequenceNumber sn2{0, 100};
    SequenceNumber sn3{0, 101};
    
    EXPECT_EQ(sn1, sn2);
    EXPECT_NE(sn1, sn3);
    EXPECT_LT(sn1, sn3);
}

TEST(RtpsTypesTest, VendorIdIdentification) {
    VendorIdValue v;
    v.bytes[0] = 0x01;
    v.bytes[1] = 0x0F;
    EXPECT_EQ(v.to_enum(), VendorId::Eprosima);

    v.bytes[0] = 0x01;
    v.bytes[1] = 0x01;
    EXPECT_EQ(v.to_enum(), VendorId::RTI);

    v.bytes[0] = 0x01;
    v.bytes[1] = 0x20;
    EXPECT_EQ(v.to_enum(), VendorId::Eclipse);
}

TEST(RtpsTypesTest, TimeArithmetic) {
    Time t1{100, 0x80000000};  // 100.5 seconds
    Time t2{50, 0x40000000};   // 50.25 seconds
    
    // Time subtraction
    Duration d = t1 - t2;
    EXPECT_EQ(d.seconds, 50);
    EXPECT_EQ(d.fraction, 0x40000000);  // 0.25
}

TEST(RtpsTypesTest, DurationInfinite) {
    Duration inf = Duration::infinite();
    EXPECT_TRUE(inf.is_infinite());
    
    Duration finite{100, 0};
    EXPECT_FALSE(finite.is_infinite());
}

TEST(RtpsTypesTest, LocatorIPv4Address) {
    Locator loc;
    loc.kind = LocatorKind::UDPv4;
    loc.port = 7400;
    // IPv4 address is in last 4 bytes: 192.168.1.100
    loc.address.fill(0);
    loc.address[12] = 192;
    loc.address[13] = 168;
    loc.address[14] = 1;
    loc.address[15] = 100;
    
    std::string str = loc.to_string();
    EXPECT_NE(str.find("7400"), std::string::npos);
}

TEST(RtpsTypesTest, PortCalculation) {
    // Domain 0, participant 0 should give well-known ports
    EXPECT_EQ(discovery_multicast_port(0), 7400);
    EXPECT_EQ(discovery_unicast_port(0, 0), 7410);
    EXPECT_EQ(user_multicast_port(0), 7401);
    EXPECT_EQ(user_unicast_port(0, 0), 7411);
}

TEST(RtpsTypesTest, RtpsPortDetection) {
    EXPECT_TRUE(is_likely_rtps_port(7400));
    EXPECT_TRUE(is_likely_rtps_port(7410));
    EXPECT_TRUE(is_likely_rtps_port(7500));
    EXPECT_FALSE(is_likely_rtps_port(80));
    EXPECT_FALSE(is_likely_rtps_port(443));
    EXPECT_FALSE(is_likely_rtps_port(13400));  // DoIP port
}

// =============================================================================
// QoS Types Tests
// =============================================================================

TEST(DiscoveryTypesTest, DurabilityKindStrings) {
    EXPECT_EQ(durability_kind_string(DurabilityKind::Volatile), "VOLATILE");
    EXPECT_EQ(durability_kind_string(DurabilityKind::TransientLocal), "TRANSIENT_LOCAL");
    EXPECT_EQ(durability_kind_string(DurabilityKind::Persistent), "PERSISTENT");
}

TEST(DiscoveryTypesTest, ReliabilityKindStrings) {
    EXPECT_EQ(reliability_kind_string(ReliabilityKind::BestEffort), "BEST_EFFORT");
    EXPECT_EQ(reliability_kind_string(ReliabilityKind::Reliable), "RELIABLE");
}

// =============================================================================
// Protocol Dispatcher Integration Tests
// =============================================================================

class RtpsDispatcherTest : public ::testing::Test {
protected:
    ProtocolDispatcher dispatcher_;
    
    // Helper to create a full Ethernet + IP + UDP + RTPS packet
    std::vector<std::byte> make_rtps_packet() {
        std::vector<std::byte> packet;
        
        // Ethernet header (14 bytes)
        auto eth = make_bytes(
            // Destination MAC
            0x01, 0x00, 0x5E, 0x00, 0x01, 0x81,
            // Source MAC
            0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
            // EtherType (IPv4 = 0x0800)
            0x08, 0x00
        );
        packet.insert(packet.end(), eth.begin(), eth.end());
        
        // IPv4 header (20 bytes, minimal)
        auto ip_header = make_bytes(
            0x45,       // Version=4, IHL=5
            0x00,       // DSCP/ECN
            0x00, 0x50, // Total length (80 bytes)
            0x00, 0x00, // Identification
            0x40, 0x00, // Flags + Fragment offset (Don't fragment)
            0x40,       // TTL
            0x11,       // Protocol (UDP = 17)
            0x00, 0x00, // Checksum (not checked)
            // Source IP: 192.168.1.100
            192, 168, 1, 100,
            // Dest IP: 239.255.0.1 (multicast)
            239, 255, 0, 1
        );
        packet.insert(packet.end(), ip_header.begin(), ip_header.end());
        
        // UDP header (8 bytes)
        auto udp = make_bytes(
            0x1C, 0xF8, // Source port: 7416
            0x1C, 0xE8, // Dest port: 7400 (RTPS discovery)
            0x00, 0x3C, // Length: 60 bytes
            0x00, 0x00  // Checksum (optional in IPv4)
        );
        packet.insert(packet.end(), udp.begin(), udp.end());
        
        // RTPS message
        auto rtps = make_rtps_header();
        auto hb = make_heartbeat_submessage();
        rtps.insert(rtps.end(), hb.begin(), hb.end());
        packet.insert(packet.end(), rtps.begin(), rtps.end());
        
        return packet;
    }
};

TEST_F(RtpsDispatcherTest, DecodeRtpsPacket) {
    auto packet = make_rtps_packet();
    
    auto result = dispatcher_.decode(packet);
    EXPECT_TRUE(result.complete);
    
    // Should have Ethernet, IPv4, UDP, and RTPS layers
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<ipv4::IPv4Header>());
    EXPECT_TRUE(result.has_layer<udp::UdpHeader>());
    EXPECT_TRUE(result.has_layer<RtpsHeader>());
    
    // Verify RTPS content
    const auto* rtps = result.get_layer<RtpsHeader>();
    ASSERT_NE(rtps, nullptr);
    EXPECT_EQ(rtps->version.major, 2);
    EXPECT_GE(rtps->submessages.size(), 1);
}

// =============================================================================
// Submessage Kind String Tests
// =============================================================================

TEST(SubmessageKindTest, StringConversion) {
    EXPECT_EQ(submessage_kind_string(SubmessageKind::DATA), "DATA");
    EXPECT_EQ(submessage_kind_string(SubmessageKind::HEARTBEAT), "HEARTBEAT");
    EXPECT_EQ(submessage_kind_string(SubmessageKind::ACKNACK), "ACKNACK");
    EXPECT_EQ(submessage_kind_string(SubmessageKind::GAP), "GAP");
    EXPECT_EQ(submessage_kind_string(SubmessageKind::INFO_TS), "INFO_TS");
    EXPECT_EQ(submessage_kind_string(SubmessageKind::INFO_DST), "INFO_DST");
    EXPECT_EQ(submessage_kind_string(SubmessageKind::INFO_SRC), "INFO_SRC");
}

// =============================================================================
// Header to_string Tests
// =============================================================================

TEST_F(RtpsDecoderTest, HeaderToString) {
    auto msg = make_rtps_header();
    set_data(msg);
    
    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    std::string str = result->to_string();
    EXPECT_NE(str.find("RTPS"), std::string::npos);
    EXPECT_NE(str.find("v2.4"), std::string::npos);
}

}  // anonymous namespace
}  // namespace wadjet::protocols::dds

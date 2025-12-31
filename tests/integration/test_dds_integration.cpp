/// @file test_dds_integration.cpp
/// @brief Integration tests for DDS/RTPS protocol decoder
/// @details Tests complete RTPS message decoding, discovery sequences,
///          data exchange patterns, and multi-vendor interoperability

#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/dds/rtps_types.hpp"
#include "wadjet/protocols/dds/rtps_messages.hpp"
#include "wadjet/protocols/dds/discovery.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/udp.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::dds;

namespace {

// =============================================================================
// Test Packet Builders
// =============================================================================

class RtpsPacketBuilder {
public:
    RtpsPacketBuilder& set_version(std::uint8_t major, std::uint8_t minor) {
        version_major_ = major;
        version_minor_ = minor;
        return *this;
    }
    
    RtpsPacketBuilder& set_vendor(VendorId vendor) {
        vendor_ = vendor;
        return *this;
    }
    
    RtpsPacketBuilder& set_guid_prefix(const std::array<std::uint8_t, 12>& prefix) {
        guid_prefix_ = prefix;
        return *this;
    }
    
    RtpsPacketBuilder& add_data_submessage(
        const EntityId& reader_id,
        const EntityId& writer_id,
        std::int64_t sequence_number,
        std::vector<std::uint8_t> payload) {
        
        Submessage submsg;
        submsg.writer_id = writer_id;
        submsg.reader_id = reader_id;
        submsg.sequence_number = sequence_number;
        submsg.payload = std::move(payload);
        submsg.kind = SubmessageKind::DATA;
        pending_submessages_.push_back(submsg);
        return *this;
    }
    
    RtpsPacketBuilder& add_heartbeat_submessage(
        const EntityId& reader_id,
        const EntityId& writer_id,
        std::int64_t first_sn,
        std::int64_t last_sn,
        std::int32_t count) {
        
        Submessage submsg;
        submsg.writer_id = writer_id;
        submsg.reader_id = reader_id;
        submsg.first_sn = first_sn;
        submsg.last_sn = last_sn;
        submsg.count = count;
        submsg.kind = SubmessageKind::HEARTBEAT;
        pending_submessages_.push_back(submsg);
        return *this;
    }
    
    RtpsPacketBuilder& add_acknack_submessage(
        const EntityId& reader_id,
        const EntityId& writer_id,
        std::int64_t base_sn,
        std::int32_t count) {
        
        Submessage submsg;
        submsg.reader_id = reader_id;
        submsg.writer_id = writer_id;
        submsg.base_sn = base_sn;
        submsg.count = count;
        submsg.kind = SubmessageKind::ACKNACK;
        pending_submessages_.push_back(submsg);
        return *this;
    }
    
    RtpsPacketBuilder& add_info_ts_submessage(std::int32_t seconds, std::uint32_t fraction) {
        Submessage submsg;
        submsg.ts_seconds = seconds;
        submsg.ts_fraction = fraction;
        submsg.kind = SubmessageKind::INFO_TS;
        pending_submessages_.push_back(submsg);
        return *this;
    }
    
    RtpsPacketBuilder& add_info_dst_submessage(const std::array<std::uint8_t, 12>& prefix) {
        Submessage submsg;
        submsg.dst_prefix = prefix;
        submsg.kind = SubmessageKind::INFO_DST;
        pending_submessages_.push_back(submsg);
        return *this;
    }
    
    RtpsPacketBuilder& add_gap_submessage(
        const EntityId& reader_id,
        const EntityId& writer_id,
        std::int64_t gap_start,
        std::int64_t gap_end) {
        
        Submessage submsg;
        submsg.reader_id = reader_id;
        submsg.writer_id = writer_id;
        submsg.gap_start = gap_start;
        submsg.gap_end = gap_end;
        submsg.kind = SubmessageKind::GAP;
        pending_submessages_.push_back(submsg);
        return *this;
    }
    
    std::vector<std::uint8_t> build() {
        std::vector<std::uint8_t> packet;
        
        // RTPS header (20 bytes)
        packet.push_back('R');
        packet.push_back('T');
        packet.push_back('P');
        packet.push_back('S');
        packet.push_back(version_major_);
        packet.push_back(version_minor_);
        
        auto vendor_val = static_cast<std::uint16_t>(vendor_);
        packet.push_back(static_cast<std::uint8_t>(vendor_val >> 8));
        packet.push_back(static_cast<std::uint8_t>(vendor_val & 0xFF));
        
        for (auto b : guid_prefix_) {
            packet.push_back(b);
        }
        
        // Add submessages
        for (const auto& submsg : pending_submessages_) {
            add_submessage_to_packet(packet, submsg);
        }
        
        return packet;
    }

private:
    struct Submessage {
        SubmessageKind kind;
        EntityId reader_id;
        EntityId writer_id;
        std::int64_t sequence_number = 0;
        std::int64_t first_sn = 0;
        std::int64_t last_sn = 0;
        std::int64_t base_sn = 0;
        std::int64_t gap_start = 0;
        std::int64_t gap_end = 0;
        std::int32_t count = 0;
        std::int32_t ts_seconds = 0;
        std::uint32_t ts_fraction = 0;
        std::array<std::uint8_t, 12> dst_prefix{};
        std::vector<std::uint8_t> payload;
    };
    
    void add_submessage_to_packet(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        switch (submsg.kind) {
            case SubmessageKind::DATA:
                add_data_submessage_bytes(packet, submsg);
                break;
            case SubmessageKind::HEARTBEAT:
                add_heartbeat_submessage_bytes(packet, submsg);
                break;
            case SubmessageKind::ACKNACK:
                add_acknack_submessage_bytes(packet, submsg);
                break;
            case SubmessageKind::INFO_TS:
                add_info_ts_submessage_bytes(packet, submsg);
                break;
            case SubmessageKind::INFO_DST:
                add_info_dst_submessage_bytes(packet, submsg);
                break;
            case SubmessageKind::GAP:
                add_gap_submessage_bytes(packet, submsg);
                break;
            default:
                break;
        }
    }
    
    void add_data_submessage_bytes(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        // DATA submessage: header(4) + body(20) + payload
        std::uint16_t body_len = 20 + static_cast<std::uint16_t>(submsg.payload.size());
        
        packet.push_back(static_cast<std::uint8_t>(SubmessageKind::DATA));
        packet.push_back(0x05);  // Flags: endian=little, data present
        packet.push_back(static_cast<std::uint8_t>(body_len & 0xFF));
        packet.push_back(static_cast<std::uint8_t>(body_len >> 8));
        
        // Extra flags + octets to inline QoS
        packet.push_back(0x00);
        packet.push_back(0x00);
        packet.push_back(0x10);
        packet.push_back(0x00);
        
        // Reader entity ID
        for (auto b : submsg.reader_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.reader_id.entity_kind));
        
        // Writer entity ID
        for (auto b : submsg.writer_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.writer_id.entity_kind));
        
        // Sequence number
        auto sn = submsg.sequence_number;
        auto high = static_cast<std::int32_t>(sn >> 32);
        auto low = static_cast<std::uint32_t>(sn & 0xFFFFFFFF);
        
        packet.push_back(static_cast<std::uint8_t>(high & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((high >> 8) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((high >> 16) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((high >> 24) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>(low & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((low >> 8) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((low >> 16) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((low >> 24) & 0xFF));
        
        // Payload
        for (auto b : submsg.payload) {
            packet.push_back(b);
        }
    }
    
    void add_heartbeat_submessage_bytes(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        packet.push_back(static_cast<std::uint8_t>(SubmessageKind::HEARTBEAT));
        packet.push_back(0x01);  // Flags: endian=little
        packet.push_back(0x1C);  // Length = 28
        packet.push_back(0x00);
        
        // Reader/Writer entity IDs
        for (auto b : submsg.reader_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.reader_id.entity_kind));
        for (auto b : submsg.writer_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.writer_id.entity_kind));
        
        // First SN
        auto first_high = static_cast<std::int32_t>(submsg.first_sn >> 32);
        auto first_low = static_cast<std::uint32_t>(submsg.first_sn & 0xFFFFFFFF);
        add_int32_le(packet, first_high);
        add_uint32_le(packet, first_low);
        
        // Last SN
        auto last_high = static_cast<std::int32_t>(submsg.last_sn >> 32);
        auto last_low = static_cast<std::uint32_t>(submsg.last_sn & 0xFFFFFFFF);
        add_int32_le(packet, last_high);
        add_uint32_le(packet, last_low);
        
        // Count
        add_int32_le(packet, submsg.count);
    }
    
    void add_acknack_submessage_bytes(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        packet.push_back(static_cast<std::uint8_t>(SubmessageKind::ACKNACK));
        packet.push_back(0x01);  // Flags: endian=little
        packet.push_back(0x18);  // Length = 24
        packet.push_back(0x00);
        
        // Reader/Writer entity IDs
        for (auto b : submsg.reader_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.reader_id.entity_kind));
        for (auto b : submsg.writer_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.writer_id.entity_kind));
        
        // Base SN
        auto base_high = static_cast<std::int32_t>(submsg.base_sn >> 32);
        auto base_low = static_cast<std::uint32_t>(submsg.base_sn & 0xFFFFFFFF);
        add_int32_le(packet, base_high);
        add_uint32_le(packet, base_low);
        
        // Num bits = 0 (all acknowledged)
        add_uint32_le(packet, 0);
        
        // Count
        add_int32_le(packet, submsg.count);
    }
    
    void add_info_ts_submessage_bytes(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        packet.push_back(static_cast<std::uint8_t>(SubmessageKind::INFO_TS));
        packet.push_back(0x01);  // Flags: endian=little, timestamp present
        packet.push_back(0x08);  // Length = 8
        packet.push_back(0x00);
        
        add_int32_le(packet, submsg.ts_seconds);
        add_uint32_le(packet, submsg.ts_fraction);
    }
    
    void add_info_dst_submessage_bytes(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        packet.push_back(static_cast<std::uint8_t>(SubmessageKind::INFO_DST));
        packet.push_back(0x01);  // Flags: endian=little
        packet.push_back(0x0C);  // Length = 12
        packet.push_back(0x00);
        
        for (auto b : submsg.dst_prefix) {
            packet.push_back(b);
        }
    }
    
    void add_gap_submessage_bytes(std::vector<std::uint8_t>& packet, const Submessage& submsg) {
        packet.push_back(static_cast<std::uint8_t>(SubmessageKind::GAP));
        packet.push_back(0x01);  // Flags: endian=little
        packet.push_back(0x1C);  // Length = 28
        packet.push_back(0x00);
        
        // Reader/Writer entity IDs
        for (auto b : submsg.reader_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.reader_id.entity_kind));
        for (auto b : submsg.writer_id.entity_key) packet.push_back(b);
        packet.push_back(static_cast<std::uint8_t>(submsg.writer_id.entity_kind));
        
        // Gap start
        auto start_high = static_cast<std::int32_t>(submsg.gap_start >> 32);
        auto start_low = static_cast<std::uint32_t>(submsg.gap_start & 0xFFFFFFFF);
        add_int32_le(packet, start_high);
        add_uint32_le(packet, start_low);
        
        // Gap list base (same as gap_end)
        auto end_high = static_cast<std::int32_t>(submsg.gap_end >> 32);
        auto end_low = static_cast<std::uint32_t>(submsg.gap_end & 0xFFFFFFFF);
        add_int32_le(packet, end_high);
        add_uint32_le(packet, end_low);
        
        // Num bits = 0
        add_uint32_le(packet, 0);
    }
    
    void add_int32_le(std::vector<std::uint8_t>& packet, std::int32_t val) {
        packet.push_back(static_cast<std::uint8_t>(val & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((val >> 16) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((val >> 24) & 0xFF));
    }
    
    void add_uint32_le(std::vector<std::uint8_t>& packet, std::uint32_t val) {
        packet.push_back(static_cast<std::uint8_t>(val & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((val >> 16) & 0xFF));
        packet.push_back(static_cast<std::uint8_t>((val >> 24) & 0xFF));
    }
    
    std::uint8_t version_major_ = 2;
    std::uint8_t version_minor_ = 4;
    VendorId vendor_ = VendorId::FastDDS;
    std::array<std::uint8_t, 12> guid_prefix_ = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
    };
    std::vector<Submessage> pending_submessages_;
};

// Helper to create EntityId
EntityId make_entity_id(std::uint8_t k0, std::uint8_t k1, std::uint8_t k2, EntityKind kind) {
    EntityId id;
    id.entity_key = {k0, k1, k2};
    id.entity_kind = kind;
    return id;
}

}  // namespace

// =============================================================================
// Full RTPS Message Decode Tests
// =============================================================================

class DdsIntegrationTest : public ::testing::Test {
protected:
    RtpsDecoder decoder_;
    
    DecodeContext make_context(const std::vector<std::uint8_t>& data) {
        data_ = data;
        byte_data_.clear();
        for (auto b : data_) {
            byte_data_.push_back(static_cast<std::byte>(b));
        }
        
        DecodeContext ctx;
        ctx.data = std::span<const std::byte>(byte_data_);
        ctx.original_offset = 0;
        ctx.timestamp = Timestamp::now();
        return ctx;
    }
    
private:
    std::vector<std::uint8_t> data_;
    std::vector<std::byte> byte_data_;
};

TEST_F(DdsIntegrationTest, DecodeCompleteRtpsMessageWithMultipleSubmessages) {
    auto reader = make_entity_id(0x00, 0x00, 0x01, EntityKind::BuiltinReader);
    auto writer = make_entity_id(0x00, 0x00, 0x02, EntityKind::BuiltinWriter);
    
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(VendorId::FastDDS)
        .add_info_ts_submessage(1234567890, 500000000)
        .add_data_submessage(reader, writer, 1, {0x01, 0x02, 0x03, 0x04})
        .add_heartbeat_submessage(reader, writer, 1, 10, 5)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    const auto& header = *result;
    
    EXPECT_EQ(header.version.major, 2);
    EXPECT_EQ(header.version.minor, 4);
    EXPECT_EQ(header.vendor_id.to_vendor(), VendorId::FastDDS);
    ASSERT_EQ(header.submessages.size(), 3);
    
    EXPECT_EQ(header.submessages[0].header.kind, SubmessageKind::INFO_TS);
    EXPECT_EQ(header.submessages[1].header.kind, SubmessageKind::DATA);
    EXPECT_EQ(header.submessages[2].header.kind, SubmessageKind::HEARTBEAT);
}

TEST_F(DdsIntegrationTest, DecodeDataExchangePattern_ReliableWriter) {
    // Simulate reliable writer data exchange:
    // Writer sends DATA, Reader sends ACKNACK, Writer sends HEARTBEAT
    
    auto reader = make_entity_id(0x00, 0x00, 0x10, EntityKind::UserReader);
    auto writer = make_entity_id(0x00, 0x00, 0x20, EntityKind::UserWriter);
    
    std::array<std::uint8_t, 12> writer_prefix = {
        0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };
    
    // Writer sends DATA with sequence number 1
    auto data_packet = RtpsPacketBuilder()
        .set_vendor(VendorId::FastDDS)
        .set_guid_prefix(writer_prefix)
        .add_info_ts_submessage(1000, 0)
        .add_data_submessage(reader, writer, 1, {0xDE, 0xAD, 0xBE, 0xEF})
        .build();
    
    auto ctx1 = make_context(data_packet);
    auto result1 = decoder_.decode(ctx1);
    ASSERT_TRUE(result1.is_ok());
    ASSERT_GE(result1->submessages.size(), 2);
    
    // Verify DATA submessage
    const auto* data_body = std::get_if<DataSubmessage>(&result1->submessages[1].body);
    ASSERT_NE(data_body, nullptr);
    EXPECT_EQ(data_body->writer_sn.value(), 1);
    
    // Reader sends ACKNACK acknowledging sequence 1
    std::array<std::uint8_t, 12> reader_prefix = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
        0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC
    };
    
    auto ack_packet = RtpsPacketBuilder()
        .set_vendor(VendorId::FastDDS)
        .set_guid_prefix(reader_prefix)
        .add_info_dst_submessage(writer_prefix)
        .add_acknack_submessage(reader, writer, 2, 1)  // Base SN=2 (all before acknowledged)
        .build();
    
    auto ctx2 = make_context(ack_packet);
    auto result2 = decoder_.decode(ctx2);
    ASSERT_TRUE(result2.is_ok());
    
    // Writer sends HEARTBEAT
    auto hb_packet = RtpsPacketBuilder()
        .set_vendor(VendorId::FastDDS)
        .set_guid_prefix(writer_prefix)
        .add_heartbeat_submessage(reader, writer, 1, 1, 2)
        .build();
    
    auto ctx3 = make_context(hb_packet);
    auto result3 = decoder_.decode(ctx3);
    ASSERT_TRUE(result3.is_ok());
    
    const auto* hb_body = std::get_if<HeartbeatSubmessage>(&result3->submessages[0].body);
    ASSERT_NE(hb_body, nullptr);
    EXPECT_EQ(hb_body->first_sn.value(), 1);
    EXPECT_EQ(hb_body->last_sn.value(), 1);
}

TEST_F(DdsIntegrationTest, DecodeDataExchangePattern_BestEffort) {
    // Best-effort: just DATA messages, no acknowledgment
    auto reader = make_entity_id(0x00, 0x00, 0x10, EntityKind::UserReader);
    auto writer = make_entity_id(0x00, 0x00, 0x20, EntityKind::UserWriter);
    
    // Send multiple DATA messages
    for (int seq = 1; seq <= 5; ++seq) {
        auto packet = RtpsPacketBuilder()
            .set_vendor(VendorId::CycloneDDS)
            .add_data_submessage(reader, writer, seq, 
                {static_cast<std::uint8_t>(seq), 0x00, 0x00, 0x00})
            .build();
        
        auto ctx = make_context(packet);
        auto result = decoder_.decode(ctx);
        ASSERT_TRUE(result.is_ok());
        
        const auto* data_body = std::get_if<DataSubmessage>(&result->submessages[0].body);
        ASSERT_NE(data_body, nullptr);
        EXPECT_EQ(data_body->writer_sn.value(), seq);
    }
}

TEST_F(DdsIntegrationTest, DecodeGapMessage_MissedSamples) {
    // Writer indicates samples 5-9 are no longer available
    auto reader = make_entity_id(0x00, 0x00, 0x10, EntityKind::UserReader);
    auto writer = make_entity_id(0x00, 0x00, 0x20, EntityKind::UserWriter);
    
    auto packet = RtpsPacketBuilder()
        .set_vendor(VendorId::RTI)
        .add_gap_submessage(reader, writer, 5, 10)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    ASSERT_TRUE(result.is_ok());
    
    const auto* gap_body = std::get_if<GapSubmessage>(&result->submessages[0].body);
    ASSERT_NE(gap_body, nullptr);
    EXPECT_EQ(gap_body->gap_start.value(), 5);
}

// =============================================================================
// Discovery Sequence Validation Tests
// =============================================================================

TEST_F(DdsIntegrationTest, DiscoverySequence_ParticipantAnnouncement) {
    // SPDP uses well-known reader/writer endpoints
    auto spdp_reader = make_entity_id(0x00, 0x01, 0x00, EntityKind::BuiltinReader);
    auto spdp_writer = make_entity_id(0x00, 0x01, 0x00, EntityKind::BuiltinWriter);
    
    // Create SPDP announcement with participant data
    // This is a simplified version - real SPDP has parameter list format
    auto packet = RtpsPacketBuilder()
        .set_vendor(VendorId::FastDDS)
        .add_info_ts_submessage(static_cast<int32_t>(std::time(nullptr)), 0)
        .add_data_submessage(spdp_reader, spdp_writer, 1, 
            // Simplified participant data
            {0x00, 0x03, 0x00, 0x00,  // PID_PARTICIPANT_GUID header
             0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
             0x09, 0x0A, 0x0B, 0x0C, 0x00, 0x00, 0x01, 0xC1})
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    ASSERT_TRUE(result.is_ok());
    
    // Verify structure
    EXPECT_GE(result->submessages.size(), 2);
    EXPECT_EQ(result->submessages[0].header.kind, SubmessageKind::INFO_TS);
    EXPECT_EQ(result->submessages[1].header.kind, SubmessageKind::DATA);
}

TEST_F(DdsIntegrationTest, DiscoverySequence_EndpointAnnouncement) {
    // SEDP publication announcement
    auto sedp_pub_reader = make_entity_id(0x00, 0x00, 0x03, EntityKind::BuiltinReader);
    auto sedp_pub_writer = make_entity_id(0x00, 0x00, 0x03, EntityKind::BuiltinWriter);
    
    auto packet = RtpsPacketBuilder()
        .set_vendor(VendorId::CycloneDDS)
        .add_data_submessage(sedp_pub_reader, sedp_pub_writer, 1,
            // Simplified endpoint data with topic name
            {0x05, 0x00, 0x10, 0x00,  // PID_TOPIC_NAME header (length 16)
             'H', 'e', 'l', 'l', 'o', 'T', 'o', 'p',
             'i', 'c', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->submessages[0].header.kind, SubmessageKind::DATA);
}

// =============================================================================
// Multi-Vendor Interoperability Tests
// =============================================================================

TEST_F(DdsIntegrationTest, MultiVendor_FastDDS) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(VendorId::FastDDS)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->vendor_id.to_vendor(), VendorId::FastDDS);
    EXPECT_EQ(to_string(result->vendor_id.to_vendor()), "eProsima Fast DDS");
}

TEST_F(DdsIntegrationTest, MultiVendor_RTIConnext) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(VendorId::RTI)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->vendor_id.to_vendor(), VendorId::RTI);
    EXPECT_EQ(to_string(result->vendor_id.to_vendor()), "RTI Connext DDS");
}

TEST_F(DdsIntegrationTest, MultiVendor_CycloneDDS) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(VendorId::CycloneDDS)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->vendor_id.to_vendor(), VendorId::CycloneDDS);
    EXPECT_EQ(to_string(result->vendor_id.to_vendor()), "Eclipse CycloneDDS");
}

TEST_F(DdsIntegrationTest, MultiVendor_OpenDDS) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(VendorId::OpenDDS)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->vendor_id.to_vendor(), VendorId::OpenDDS);
    EXPECT_EQ(to_string(result->vendor_id.to_vendor()), "OCI OpenDDS");
}

TEST_F(DdsIntegrationTest, MultiVendor_UnknownVendor) {
    // Test with an unknown vendor ID
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(static_cast<VendorId>(0x9999))
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->vendor_id.to_vendor(), VendorId::Unknown);
}

TEST_F(DdsIntegrationTest, MultiVendor_CrossVendorCommunication) {
    // Simulate FastDDS writer sending to CycloneDDS reader
    auto reader = make_entity_id(0x00, 0x00, 0x01, EntityKind::UserReader);
    auto writer = make_entity_id(0x00, 0x00, 0x02, EntityKind::UserWriter);
    
    std::array<std::uint8_t, 12> fast_dds_prefix = {
        0x01, 0x0F, 0xAA, 0xBB, 0xCC, 0xDD,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66
    };
    
    std::array<std::uint8_t, 12> cyclone_prefix = {
        0x01, 0x05, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0xAA
    };
    
    // FastDDS sends DATA to CycloneDDS
    auto data_packet = RtpsPacketBuilder()
        .set_vendor(VendorId::FastDDS)
        .set_guid_prefix(fast_dds_prefix)
        .add_info_dst_submessage(cyclone_prefix)
        .add_data_submessage(reader, writer, 100, {0x48, 0x65, 0x6C, 0x6C, 0x6F})  // "Hello"
        .build();
    
    auto ctx1 = make_context(data_packet);
    auto result1 = decoder_.decode(ctx1);
    ASSERT_TRUE(result1.is_ok());
    EXPECT_EQ(result1->vendor_id.to_vendor(), VendorId::FastDDS);
    
    // CycloneDDS sends ACKNACK back
    auto ack_packet = RtpsPacketBuilder()
        .set_vendor(VendorId::CycloneDDS)
        .set_guid_prefix(cyclone_prefix)
        .add_info_dst_submessage(fast_dds_prefix)
        .add_acknack_submessage(reader, writer, 101, 1)
        .build();
    
    auto ctx2 = make_context(ack_packet);
    auto result2 = decoder_.decode(ctx2);
    ASSERT_TRUE(result2.is_ok());
    EXPECT_EQ(result2->vendor_id.to_vendor(), VendorId::CycloneDDS);
}

// =============================================================================
// Version Compatibility Tests
// =============================================================================

TEST_F(DdsIntegrationTest, Version_RTPS_2_2) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 2)
        .set_vendor(VendorId::RTI)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->version.major, 2);
    EXPECT_EQ(result->version.minor, 2);
}

TEST_F(DdsIntegrationTest, Version_RTPS_2_3) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 3)
        .set_vendor(VendorId::OpenDDS)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->version.major, 2);
    EXPECT_EQ(result->version.minor, 3);
}

TEST_F(DdsIntegrationTest, Version_RTPS_2_4) {
    auto packet = RtpsPacketBuilder()
        .set_version(2, 4)
        .set_vendor(VendorId::FastDDS)
        .build();
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->version.major, 2);
    EXPECT_EQ(result->version.minor, 4);
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(DdsIntegrationTest, ErrorHandling_TruncatedHeader) {
    std::vector<std::uint8_t> truncated = {'R', 'T', 'P', 'S', 0x02, 0x04};
    
    auto ctx = make_context(truncated);
    auto result = decoder_.decode(ctx);
    
    EXPECT_FALSE(result.is_ok());
}

TEST_F(DdsIntegrationTest, ErrorHandling_InvalidMagic) {
    auto packet = RtpsPacketBuilder().build();
    packet[2] = 'X';  // Corrupt magic
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    EXPECT_FALSE(result.is_ok());
}

TEST_F(DdsIntegrationTest, ErrorHandling_TruncatedSubmessage) {
    auto packet = RtpsPacketBuilder()
        .add_data_submessage(
            make_entity_id(0, 0, 1, EntityKind::UserReader),
            make_entity_id(0, 0, 2, EntityKind::UserWriter),
            1, {0x01, 0x02, 0x03, 0x04})
        .build();
    
    // Truncate to cut off submessage
    packet.resize(30);
    
    auto ctx = make_context(packet);
    auto result = decoder_.decode(ctx);
    
    // Should either fail or succeed with partial submessage depending on implementation
    // The key is it shouldn't crash
    [[maybe_unused]] auto ok = result.is_ok();
}

}  // namespace

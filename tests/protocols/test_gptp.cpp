/// @file test_gptp.cpp
/// @brief Unit tests for gPTP (IEEE 802.1AS) protocol decoder

#include "wadjet/protocols/gptp/gptp.hpp"
#include "wadjet/protocols/dispatcher.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <vector>

namespace wadjet::protocols::gptp {
namespace {

// Helper to create byte vector from initializer list
template <typename... Args>
std::vector<std::byte> make_bytes(Args... args) {
    return {static_cast<std::byte>(args)...};
}

// Helper to create a basic gPTP Sync header (34 bytes)
std::vector<std::byte> make_sync_header(std::uint16_t seq_id = 0x1234) {
    return make_bytes(
        // Byte 0: transportSpecific (1 = gPTP) << 4 | messageType (0 = Sync)
        0x10,
        // Byte 1: version (2)
        0x02,
        // Bytes 2-3: messageLength (44 = 34 header + 10 body)
        0x00, 0x2C,
        // Byte 4: domainNumber
        0x00,
        // Byte 5: minorSdoId
        0x00,
        // Bytes 6-7: flags (two-step = 0x0200)
        0x02, 0x00,
        // Bytes 8-15: correctionField (0)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Bytes 16-19: messageTypeSpecific (0)
        0x00, 0x00, 0x00, 0x00,
        // Bytes 20-29: sourcePortIdentity (clock ID + port)
        0x00, 0x11, 0x22, 0xFF, 0xFE, 0x33, 0x44, 0x55,  // clock identity
        0x00, 0x01,  // port number
        // Bytes 30-31: sequenceId
        static_cast<uint8_t>((seq_id >> 8) & 0xFF),
        static_cast<uint8_t>(seq_id & 0xFF),
        // Byte 32: controlField
        0x00,
        // Byte 33: logMessageInterval
        0xFD  // -3 = 125ms
    );
}

// Helper to add Sync body (10 bytes timestamp)
std::vector<std::byte> make_sync_message(std::uint16_t seq_id = 0x1234) {
    auto header = make_sync_header(seq_id);
    // Add timestamp body (10 bytes, all zeros for two-step)
    for (int i = 0; i < 10; ++i) {
        header.push_back(std::byte{0x00});
    }
    return header;
}

// Helper to make Pdelay_Req header
std::vector<std::byte> make_pdelay_req_header() {
    return make_bytes(
        // Byte 0: transportSpecific (1 = gPTP) << 4 | messageType (2 = Pdelay_Req)
        0x12,
        // Byte 1: version
        0x02,
        // Bytes 2-3: messageLength (54)
        0x00, 0x36,
        // Byte 4: domainNumber
        0x00,
        // Byte 5: minorSdoId
        0x00,
        // Bytes 6-7: flags
        0x00, 0x00,
        // Bytes 8-15: correctionField
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Bytes 16-19: messageTypeSpecific
        0x00, 0x00, 0x00, 0x00,
        // Bytes 20-29: sourcePortIdentity
        0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0xDD, 0xEE, 0xFF,
        0x00, 0x02,
        // Bytes 30-31: sequenceId
        0x00, 0x42,
        // Byte 32: controlField
        0x05,
        // Byte 33: logMessageInterval
        0x7F
    );
}

// Helper to make Pdelay_Req message (54 bytes total)
std::vector<std::byte> make_pdelay_req_message() {
    auto header = make_pdelay_req_header();
    // Add body (10 bytes timestamp + 10 bytes reserved)
    for (int i = 0; i < 20; ++i) {
        header.push_back(std::byte{0x00});
    }
    return header;
}

class GptpDecoderTest : public ::testing::Test {
protected:
    GptpDecoder decoder_;
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

// Test basic Sync message decoding
TEST_F(GptpDecoderTest, DecodeSyncMessage) {
    auto msg = make_sync_message();
    set_data(msg);

    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    EXPECT_EQ(header.transport_specific, TransportSpecific::IEEE_802_1AS);
    EXPECT_EQ(header.message_type, MessageType::Sync);
    EXPECT_EQ(header.version_ptp, 2);
    EXPECT_EQ(header.message_length, 44);
    EXPECT_EQ(header.domain_number, 0);
    EXPECT_TRUE(header.flags.two_step);
    EXPECT_EQ(header.sequence_id, 0x1234);
    EXPECT_EQ(header.source_port_identity.port_number, 1);
    EXPECT_EQ(header.log_message_interval.value, -3);
}

// Test Pdelay_Req message decoding
TEST_F(GptpDecoderTest, DecodePdelayReqMessage) {
    auto msg = make_pdelay_req_message();
    set_data(msg);

    auto result = decoder_.decode(ctx_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& header = *result;
    EXPECT_EQ(header.transport_specific, TransportSpecific::IEEE_802_1AS);
    EXPECT_EQ(header.message_type, MessageType::Pdelay_Req);
    EXPECT_EQ(header.message_length, 54);
    EXPECT_EQ(header.sequence_id, 0x42);
    EXPECT_EQ(header.source_port_identity.port_number, 2);
}

// Test buffer too small error
TEST_F(GptpDecoderTest, BufferTooSmall) {
    auto msg = make_bytes(0x10, 0x02);  // Only 2 bytes
    set_data(msg);

    auto result = decoder_.decode(ctx_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

// Test invalid message length
TEST_F(GptpDecoderTest, InvalidMessageLength) {
    auto msg = make_sync_header();
    // Modify message length to be larger than available data
    msg[2] = std::byte{0x00};
    msg[3] = std::byte{0xFF};  // 255 bytes, but we only have 34
    set_data(msg);

    auto result = decoder_.decode(ctx_);
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidLength);
}

// Test message type is event
TEST_F(GptpDecoderTest, MessageTypeIsEvent) {
    EXPECT_TRUE(is_event_message(MessageType::Sync));
    EXPECT_TRUE(is_event_message(MessageType::Pdelay_Req));
    EXPECT_TRUE(is_event_message(MessageType::Pdelay_Resp));
    EXPECT_FALSE(is_event_message(MessageType::Follow_Up));
    EXPECT_FALSE(is_event_message(MessageType::Announce));
}

// Test message type string conversion
TEST_F(GptpDecoderTest, MessageTypeString) {
    EXPECT_EQ(message_type_string(MessageType::Sync), "Sync");
    EXPECT_EQ(message_type_string(MessageType::Follow_Up), "Follow_Up");
    EXPECT_EQ(message_type_string(MessageType::Pdelay_Req), "Pdelay_Req");
    EXPECT_EQ(message_type_string(MessageType::Announce), "Announce");
}

// Test clock identity operations
TEST(ClockIdentityTest, FromMac) {
    MacAddress mac = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    ClockIdentity clock = ClockIdentity::from_mac(mac);
    
    EXPECT_EQ(clock.bytes[0], 0x00);
    EXPECT_EQ(clock.bytes[1], 0x11);
    EXPECT_EQ(clock.bytes[2], 0x22);
    EXPECT_EQ(clock.bytes[3], 0xFF);
    EXPECT_EQ(clock.bytes[4], 0xFE);
    EXPECT_EQ(clock.bytes[5], 0x33);
    EXPECT_EQ(clock.bytes[6], 0x44);
    EXPECT_EQ(clock.bytes[7], 0x55);
}

TEST(ClockIdentityTest, ToString) {
    ClockIdentity clock;
    clock.bytes = {0x00, 0x11, 0x22, 0xFF, 0xFE, 0x33, 0x44, 0x55};
    
    EXPECT_EQ(clock.to_string(), "00:11:22:ff:fe:33:44:55");
}

TEST(ClockIdentityTest, IsZero) {
    ClockIdentity zero;
    EXPECT_TRUE(zero.is_zero());
    
    ClockIdentity nonzero;
    nonzero.bytes[0] = 0x01;
    EXPECT_FALSE(nonzero.is_zero());
}

// Test port identity operations
TEST(PortIdentityTest, ToString) {
    PortIdentity port;
    port.clock_identity.bytes = {0x00, 0x11, 0x22, 0xFF, 0xFE, 0x33, 0x44, 0x55};
    port.port_number = 1;
    
    EXPECT_EQ(port.to_string(), "00:11:22:ff:fe:33:44:55-1");
}

// Test timestamp operations
TEST(GptpTimestampTest, ToNanoseconds) {
    GptpTimestamp ts(1000, 500000000);  // 1000.5 seconds
    
    auto ns = ts.to_nanoseconds();
    EXPECT_EQ(ns, 1000500000000ULL);
}

TEST(GptpTimestampTest, ToSecondsDouble) {
    GptpTimestamp ts(1000, 500000000);  // 1000.5 seconds
    
    auto secs = ts.to_seconds_double();
    EXPECT_DOUBLE_EQ(secs, 1000.5);
}

TEST(GptpTimestampTest, Subtraction) {
    GptpTimestamp t1(1000, 0);
    GptpTimestamp t2(999, 500000000);  // 999.5 seconds
    
    auto diff = t1 - t2;  // Should be 500ms = 500,000,000 ns
    EXPECT_EQ(diff, 500000000LL);
}

// Test scaled nanoseconds
TEST(ScaledNanosecondsTest, ToNanoseconds) {
    // Value of 65536 (2^16) represents 1 nanosecond
    ScaledNanoseconds sn(65536LL);
    EXPECT_EQ(sn.to_nanoseconds(), 1);
    
    // Value of 655360 (10 * 2^16) represents 10 nanoseconds
    ScaledNanoseconds sn2(655360LL);
    EXPECT_EQ(sn2.to_nanoseconds(), 10);
}

TEST(ScaledNanosecondsTest, IsZero) {
    ScaledNanoseconds zero;
    EXPECT_TRUE(zero.is_zero());
    
    ScaledNanoseconds nonzero(1);
    EXPECT_FALSE(nonzero.is_zero());
}

// Test log interval
TEST(LogIntervalTest, ToSeconds) {
    LogInterval log0(0);  // 2^0 = 1 second
    EXPECT_DOUBLE_EQ(log0.to_seconds(), 1.0);
    
    LogInterval logMinus3(-3);  // 2^-3 = 0.125 seconds
    EXPECT_DOUBLE_EQ(logMinus3.to_seconds(), 0.125);
    
    LogInterval log2(2);  // 2^2 = 4 seconds
    EXPECT_DOUBLE_EQ(log2.to_seconds(), 4.0);
}

// Test flags parsing
TEST(GptpFlagsTest, FromRaw) {
    // Test two-step flag (bit 1 of first byte)
    auto flags = GptpFlags::from_raw(0x0200);
    EXPECT_TRUE(flags.two_step);
    EXPECT_FALSE(flags.unicast);
    
    // Test unicast flag (bit 2 of first byte)
    flags = GptpFlags::from_raw(0x0400);
    EXPECT_FALSE(flags.two_step);
    EXPECT_TRUE(flags.unicast);
}

// Test peer delay calculation helper
TEST(PeerDelayTest, Calculate) {
    // Simulate peer delay measurement
    GptpTimestamp t1(100, 0);         // Pdelay_Req sent
    GptpTimestamp t2(100, 1000);      // Pdelay_Req received (1000ns later)
    GptpTimestamp t3(100, 2000);      // Pdelay_Resp sent (1000ns turnaround)
    GptpTimestamp t4(100, 3000);      // Pdelay_Resp received (1000ns propagation again)
    
    auto delay = calculate_peer_delay(t1, t2, t3, t4);
    // peerDelay = [(t4-t1) - (t3-t2)] / 2 = [3000 - 1000] / 2 = 1000
    EXPECT_EQ(delay, 1000);
}

// Test with protocol dispatcher
TEST(GptpDispatcherTest, DecodeGptpPacket) {
    // Create a full Ethernet + gPTP frame
    std::vector<std::byte> frame;
    
    // Ethernet header (14 bytes)
    // Destination MAC (gPTP multicast)
    frame.insert(frame.end(), {
        std::byte{0x01}, std::byte{0x80}, std::byte{0xC2}, 
        std::byte{0x00}, std::byte{0x00}, std::byte{0x0E}
    });
    // Source MAC
    frame.insert(frame.end(), {
        std::byte{0x00}, std::byte{0x11}, std::byte{0x22}, 
        std::byte{0x33}, std::byte{0x44}, std::byte{0x55}
    });
    // EtherType (0x88F7 = PTP)
    frame.insert(frame.end(), {std::byte{0x88}, std::byte{0xF7}});
    
    // Add gPTP Sync message
    auto gptp_msg = make_sync_message();
    frame.insert(frame.end(), gptp_msg.begin(), gptp_msg.end());
    
    // Decode with dispatcher
    ProtocolDispatcher dispatcher;
    auto result = dispatcher.decode(std::span<const std::byte>(frame));
    
    EXPECT_TRUE(result.complete);
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<gptp::GptpHeader>());
    
    const auto* gptp_hdr = result.get_layer<gptp::GptpHeader>();
    ASSERT_NE(gptp_hdr, nullptr);
    EXPECT_EQ(gptp_hdr->message_type, MessageType::Sync);
}

// Test gPTP filter functions
TEST(GptpFilterTest, GptpMessage) {
    // Create test result
    DecodeStackResult result;
    GptpHeader gptp;
    gptp.message_type = MessageType::Sync;
    gptp.domain_number = 0;
    result.layers.emplace_back(gptp);
    
    auto sync_filter = gptp_message(MessageType::Sync);
    auto follow_up_filter = gptp_message(MessageType::Follow_Up);
    
    EXPECT_TRUE(sync_filter(result));
    EXPECT_FALSE(follow_up_filter(result));
}

TEST(GptpFilterTest, GptpDomain) {
    DecodeStackResult result;
    GptpHeader gptp;
    gptp.domain_number = 5;
    result.layers.emplace_back(gptp);
    
    auto domain5_filter = gptp_domain(5);
    auto domain0_filter = gptp_domain(0);
    
    EXPECT_TRUE(domain5_filter(result));
    EXPECT_FALSE(domain0_filter(result));
}

}  // namespace
}  // namespace wadjet::protocols::gptp

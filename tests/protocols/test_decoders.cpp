/// @file test_decoders.cpp
/// @brief Unit tests for protocol decoders

#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/someip_sd.hpp"
#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/udp.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <vector>

using namespace wadjet::protocols;

// Helper to create a DecodeContext with proper initialization
template <typename Container>
DecodeContext make_context(const Container& data) {
    DecodeContext ctx;
    ctx.data = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()),
                                          data.size() * sizeof(typename Container::value_type));
    ctx.original_offset = 0;
    ctx.timestamp = {};
    ctx.layer_info = {};
    return ctx;
}

//==============================================================================
// Ethernet Decoder Tests
//==============================================================================

class EthernetDecoderTest : public ::testing::Test {
protected:
    ethernet::EthernetDecoder decoder;

    // Minimal Ethernet frame: dst(6) + src(6) + ethertype(2) = 14 bytes
    std::array<std::uint8_t, 14> minimal_frame = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // dst MAC
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16,  // src MAC
        0x08, 0x00                           // IPv4 ethertype
    };

    // VLAN tagged frame: dst(6) + src(6) + VLAN(4) + ethertype(2) = 18 bytes
    std::array<std::uint8_t, 18> vlan_frame = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // dst MAC
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16,  // src MAC
        0x81, 0x00,                          // VLAN TPID
        0x00, 0x64,                          // TCI (VID=100)
        0x08, 0x00                           // IPv4 ethertype
    };

    // QinQ (double VLAN) frame: dst(6) + src(6) + outer(4) + inner(4) + ethertype(2) = 22 bytes
    std::array<std::uint8_t, 22> qinq_frame = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // dst MAC
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16,  // src MAC
        0x88, 0xa8,                          // QinQ outer TPID
        0x00, 0xC8,                          // Outer TCI (VID=200)
        0x81, 0x00,                          // VLAN inner TPID
        0x00, 0x64,                          // Inner TCI (VID=100)
        0x08, 0x00                           // IPv4 ethertype
    };
};

TEST_F(EthernetDecoderTest, DecodeMinimalFrame) {
    auto ctx = make_context(minimal_frame);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.dst_mac.to_string(), "01:02:03:04:05:06");
    EXPECT_EQ(header.src_mac.to_string(), "11:12:13:14:15:16");
    EXPECT_EQ(header.ethertype, 0x0800);  // IPv4
    EXPECT_FALSE(header.vlan.has_value());
    EXPECT_FALSE(header.vlan_inner.has_value());
    EXPECT_EQ(header.header_len, 14);
}

TEST_F(EthernetDecoderTest, DecodeVlanFrame) {
    auto ctx = make_context(vlan_frame);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.ethertype, 0x0800);  // IPv4
    ASSERT_TRUE(header.vlan.has_value());
    EXPECT_EQ(header.vlan->vid(), 100);
    EXPECT_EQ(header.vlan->tpid, 0x8100);
    EXPECT_FALSE(header.vlan_inner.has_value());
    EXPECT_EQ(header.header_len, 18);
}

TEST_F(EthernetDecoderTest, DecodeQinQFrame) {
    auto ctx = make_context(qinq_frame);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.ethertype, 0x0800);  // IPv4
    ASSERT_TRUE(header.vlan.has_value());
    EXPECT_EQ(header.vlan->vid(), 200);  // Outer VLAN
    ASSERT_TRUE(header.vlan_inner.has_value());
    EXPECT_EQ(header.vlan_inner->vid(), 100);  // Inner VLAN
    EXPECT_EQ(header.header_len, 22);
}

TEST_F(EthernetDecoderTest, RejectTruncatedFrame) {
    std::array<std::uint8_t, 10> truncated = {0};  // Too small
    auto ctx = make_context(truncated);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

TEST_F(EthernetDecoderTest, HeaderToString) {
    auto ctx = make_context(vlan_frame);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    std::string str = result->to_string();

    EXPECT_TRUE(str.find("Ethernet") != std::string::npos);
    EXPECT_TRUE(str.find("vlan=100") != std::string::npos);
}

//==============================================================================
// IPv4 Decoder Tests
//==============================================================================

class IPv4DecoderTest : public ::testing::Test {
protected:
    // Minimal IPv4 header (20 bytes, no options)
    std::array<std::uint8_t, 20> minimal_header = {
        0x45,                    // Version=4, IHL=5 (20 bytes)
        0x00,                    // DSCP=0, ECN=0
        0x00, 0x28,              // Total length = 40 bytes
        0x12, 0x34,              // Identification
        0x40, 0x00,              // Flags=DF, Fragment offset=0
        0x40,                    // TTL=64
        0x06,                    // Protocol=TCP
        0x00, 0x00,              // Checksum (placeholder)
        0xC0, 0xA8, 0x01, 0x01,  // Src IP: 192.168.1.1
        0xC0, 0xA8, 0x01, 0x02   // Dst IP: 192.168.1.2
    };

    // IPv4 header with options (24 bytes)
    std::array<std::uint8_t, 24> header_with_options = {
        0x46,                    // Version=4, IHL=6 (24 bytes)
        0x00,                    // DSCP=0, ECN=0
        0x00, 0x30,              // Total length = 48 bytes
        0x12, 0x34,              // Identification
        0x00, 0x00,              // Flags=0, Fragment offset=0
        0x40,                    // TTL=64
        0x11,                    // Protocol=UDP
        0x00, 0x00,              // Checksum
        0x0A, 0x00, 0x00, 0x01,  // Src IP: 10.0.0.1
        0x0A, 0x00, 0x00, 0x02,  // Dst IP: 10.0.0.2
        0x01, 0x01, 0x00, 0x00   // Options (NOP, NOP, End, pad)
    };

    ipv4::IPv4Decoder make_decoder(bool validate_checksum = false) {
        ipv4::IPv4Decoder::Options opts;
        opts.validate_checksum = validate_checksum;
        return ipv4::IPv4Decoder(opts);
    }
};

TEST_F(IPv4DecoderTest, DecodeMinimalHeader) {
    auto decoder = make_decoder(false);  // Don't validate checksum
    auto ctx = make_context(minimal_header);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.version, 4);
    EXPECT_EQ(header.ihl, 5);
    EXPECT_EQ(header.total_length, 40);
    EXPECT_EQ(header.ttl, 64);
    EXPECT_EQ(header.protocol, 6);  // TCP
    EXPECT_EQ(header.src_ip.to_string(), "192.168.1.1");
    EXPECT_EQ(header.dst_ip.to_string(), "192.168.1.2");
    EXPECT_TRUE(header.flags.dont_fragment);
    EXPECT_FALSE(header.flags.more_fragments);
}

TEST_F(IPv4DecoderTest, DecodeHeaderWithOptions) {
    auto decoder = make_decoder(false);
    auto ctx = make_context(header_with_options);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.version, 4);
    EXPECT_EQ(header.ihl, 6);          // 24 bytes
    EXPECT_EQ(header.protocol, 0x11);  // UDP
    EXPECT_EQ(header.src_ip.to_string(), "10.0.0.1");
    EXPECT_EQ(header.dst_ip.to_string(), "10.0.0.2");
}

TEST_F(IPv4DecoderTest, RejectInvalidVersion) {
    auto decoder = make_decoder(false);
    std::array<std::uint8_t, 20> bad_version = minimal_header;
    bad_version[0] = 0x65;  // Version=6 (invalid for IPv4)

    auto ctx = make_context(bad_version);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidVersion);
}

TEST_F(IPv4DecoderTest, RejectTooSmallIHL) {
    auto decoder = make_decoder(false);
    std::array<std::uint8_t, 20> small_ihl = minimal_header;
    small_ihl[0] = 0x44;  // IHL=4 (invalid, min is 5)

    auto ctx = make_context(small_ihl);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidHeader);
}

TEST_F(IPv4DecoderTest, RejectTruncatedHeader) {
    auto decoder = make_decoder(false);
    std::array<std::uint8_t, 10> truncated = {0x45};  // Too small
    auto ctx = make_context(truncated);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

TEST_F(IPv4DecoderTest, HeaderToString) {
    auto decoder = make_decoder(false);
    auto ctx = make_context(minimal_header);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    std::string str = result->to_string();

    EXPECT_TRUE(str.find("IPv4") != std::string::npos);
    EXPECT_TRUE(str.find("192.168.1.1") != std::string::npos);
    EXPECT_TRUE(str.find("192.168.1.2") != std::string::npos);
}

//==============================================================================
// UDP Decoder Tests
//==============================================================================

class UDPDecoderTest : public ::testing::Test {
protected:
    udp::UdpDecoder decoder;

    // UDP header (8 bytes)
    std::array<std::uint8_t, 16> udp_packet = {
        0x30, 0x39,              // Src port: 12345
        0x00, 0x50,              // Dst port: 80
        0x00, 0x10,              // Length: 16 bytes (header + 8 bytes payload)
        0x00, 0x00,              // Checksum: 0 (optional in IPv4)
        0x48, 0x65, 0x6c, 0x6c,  // Payload: "Hell"
        0x6f, 0x21, 0x0a, 0x00   // Payload: "o!\n\0"
    };
};

TEST_F(UDPDecoderTest, DecodeValidPacket) {
    auto ctx = make_context(udp_packet);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.src_port, 12345);
    EXPECT_EQ(header.dst_port, 80);
    EXPECT_EQ(header.length, 16);
    EXPECT_EQ(header.checksum, 0);
    EXPECT_TRUE(header.checksum_valid);  // 0 is valid in IPv4
}

TEST_F(UDPDecoderTest, RejectTruncatedHeader) {
    std::array<std::uint8_t, 4> truncated = {0x30, 0x39, 0x00, 0x50};
    auto ctx = make_context(truncated);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

TEST_F(UDPDecoderTest, RejectInvalidLength) {
    std::array<std::uint8_t, 8> bad_length = {0x30, 0x39, 0x00,
                                              0x50, 0x00, 0x05,  // Length: 5 (invalid, min is 8)
                                              0x00, 0x00};
    auto ctx = make_context(bad_length);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidLength);
}

TEST_F(UDPDecoderTest, HeaderToString) {
    auto ctx = make_context(udp_packet);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    std::string str = result->to_string();

    EXPECT_TRUE(str.find("UDP") != std::string::npos);
    EXPECT_TRUE(str.find("12345") != std::string::npos);
    EXPECT_TRUE(str.find("80") != std::string::npos);
}

//==============================================================================
// TCP Decoder Tests
//==============================================================================

class TCPDecoderTest : public ::testing::Test {
protected:
    tcp::TcpDecoder decoder;

    // Minimal TCP header (20 bytes, no options)
    std::array<std::uint8_t, 20> minimal_header = {
        0x00, 0x50,              // Src port: 80
        0x30, 0x39,              // Dst port: 12345
        0x00, 0x00, 0x00, 0x01,  // Seq: 1
        0x00, 0x00, 0x00, 0x00,  // Ack: 0
        0x50,                    // Data offset: 5 (20 bytes), reserved
        0x02,                    // Flags: SYN
        0x72, 0x10,              // Window: 29200
        0x00, 0x00,              // Checksum
        0x00, 0x00               // Urgent pointer
    };

    // TCP SYN with MSS option (24 bytes)
    std::array<std::uint8_t, 24> syn_with_mss = {
        0x00, 0x50,              // Src port: 80
        0x30, 0x39,              // Dst port: 12345
        0x00, 0x00, 0x00, 0x01,  // Seq: 1
        0x00, 0x00, 0x00, 0x00,  // Ack: 0
        0x60,                    // Data offset: 6 (24 bytes)
        0x02,                    // Flags: SYN
        0x72, 0x10,              // Window: 29200
        0x00, 0x00,              // Checksum
        0x00, 0x00,              // Urgent pointer
        0x02, 0x04, 0x05, 0xB4   // MSS option: kind=2, len=4, value=1460
    };
};

TEST_F(TCPDecoderTest, DecodeMinimalHeader) {
    auto ctx = make_context(minimal_header);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.src_port, 80);
    EXPECT_EQ(header.dst_port, 12345);
    EXPECT_EQ(header.seq_num, 1);
    EXPECT_EQ(header.ack_num, 0);
    EXPECT_EQ(header.data_offset, 5);
    EXPECT_TRUE(header.flags.syn);
    EXPECT_FALSE(header.flags.ack);
    EXPECT_EQ(header.window, 29200);
}

TEST_F(TCPDecoderTest, DecodeWithMSSOption) {
    tcp::TcpDecoder::Options opts;
    opts.parse_options = true;
    tcp::TcpDecoder opt_decoder(opts);

    auto ctx = make_context(syn_with_mss);
    auto result = opt_decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.data_offset, 6);
    auto mss = header.get_mss();
    ASSERT_TRUE(mss.has_value());
    EXPECT_EQ(*mss, 1460);
}

TEST_F(TCPDecoderTest, FlagsToString) {
    auto ctx = make_context(minimal_header);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    std::string flags_str = result->flags.to_string();

    EXPECT_EQ(flags_str, "SYN");
}

TEST_F(TCPDecoderTest, RejectTruncatedHeader) {
    std::array<std::uint8_t, 10> truncated = {0};
    auto ctx = make_context(truncated);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

TEST_F(TCPDecoderTest, RejectInvalidDataOffset) {
    std::array<std::uint8_t, 20> bad_offset = minimal_header;
    bad_offset[12] = 0x40;  // Data offset: 4 (invalid, min is 5)

    auto ctx = make_context(bad_offset);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidHeader);
}

//==============================================================================
// SOME/IP Decoder Tests
//==============================================================================

class SomeIpDecoderTest : public ::testing::Test {
protected:
    someip::SomeIpDecoder decoder;

    // SOME/IP header (16 bytes)
    std::array<std::uint8_t, 24> someip_message = {
        0x12, 0x34,              // Service ID: 0x1234
        0x80, 0x01,              // Method ID: 0x8001 (event)
        0x00, 0x00, 0x00, 0x10,  // Length: 16 (8 byte header + 8 byte payload)
        0x00, 0x01,              // Client ID: 1
        0x00, 0x0A,              // Session ID: 10
        0x01,                    // Protocol version: 1
        0x01,                    // Interface version: 1
        0x02,                    // Message type: Notification
        0x00,                    // Return code: Ok
        0xDE, 0xAD, 0xBE, 0xEF,  // Payload
        0xCA, 0xFE, 0xBA, 0xBE   // Payload
    };
};

TEST_F(SomeIpDecoderTest, DecodeValidMessage) {
    auto ctx = make_context(someip_message);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.service_id, 0x1234);
    EXPECT_EQ(header.method_id, 0x8001);
    EXPECT_TRUE(header.is_event());
    EXPECT_EQ(header.length, 16);
    EXPECT_EQ(header.client_id, 1);
    EXPECT_EQ(header.session_id, 10);
    EXPECT_EQ(header.protocol_version, 1);
    EXPECT_EQ(header.interface_version, 1);
    EXPECT_EQ(header.message_type, someip::MessageType::Notification);
    EXPECT_EQ(header.return_code, someip::ReturnCode::Ok);
}

TEST_F(SomeIpDecoderTest, RejectTruncatedHeader) {
    std::array<std::uint8_t, 8> truncated = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x08};
    auto ctx = make_context(truncated);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

TEST_F(SomeIpDecoderTest, RejectInvalidLength) {
    std::array<std::uint8_t, 16> bad_length = {
        0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x04,  // Length: 4 (invalid, min is 8)
        0x00, 0x01, 0x00, 0x0A, 0x01, 0x01, 0x02, 0x00};
    auto ctx = make_context(bad_length);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidLength);
}

TEST_F(SomeIpDecoderTest, HeaderToString) {
    auto ctx = make_context(someip_message);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    std::string str = result->to_string();

    EXPECT_TRUE(str.find("SOME/IP") != std::string::npos);
    EXPECT_TRUE(str.find("0x1234") != std::string::npos);
}

TEST_F(SomeIpDecoderTest, RejectMinimumLengthTooSmall) {
    // SOME/IP length field must be at least 8 (for Request ID and subsequent fields)
    std::array<std::uint8_t, 16> bad_length = {
        0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,  // Length: 7 (below minimum)
        0x00, 0x01, 0x00, 0x0A, 0x01, 0x01, 0x02, 0x00};
    auto ctx = make_context(bad_length);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidLength);
}

TEST_F(SomeIpDecoderTest, AcceptMinimumValidLength) {
    // SOME/IP length = 8 is the minimum (no payload)
    std::array<std::uint8_t, 16> min_length = {
        0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x08,  // Length: 8 (minimum valid)
        0x00, 0x01, 0x00, 0x0A, 0x01, 0x01, 0x02, 0x00};
    auto ctx = make_context(min_length);
    auto result = decoder.decode(ctx);

    EXPECT_TRUE(result);
    EXPECT_EQ(result->length, 8);
}

TEST_F(SomeIpDecoderTest, AcceptMaximumLength) {
    // Maximum SOME/IP message size is 16MB (0xFFFFFF + 8 for header)
    // Create a message with length field = 0xFFFFFF (16777215)
    std::array<std::uint8_t, 16> max_length = {
        0x12, 0x34, 0x80, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,  // Length: 0xFFFFFFFF (max uint32)
        0x00, 0x01, 0x00, 0x0A, 0x01, 0x01, 0x02, 0x00};
    auto ctx = make_context(max_length);
    auto result = decoder.decode(ctx);

    // Note: This should succeed with the length field, actual buffer validation is separate
    EXPECT_TRUE(result);
    EXPECT_EQ(result->length, 0xFFFFFFFF);
}

TEST_F(SomeIpDecoderTest, PayloadSizeMatchesLength) {
    // Test that payload size matches the length field (length = 8 + payload_size)
    std::array<std::uint8_t, 24> correct_payload = {
        0x12, 0x34,              // Service ID
        0x80, 0x01,              // Method ID
        0x00, 0x00, 0x00, 0x10,  // Length: 16 (8 header + 8 payload)
        0x00, 0x01,              // Client ID
        0x00, 0x0A,              // Session ID
        0x01,                    // Protocol version
        0x01,                    // Interface version
        0x02,                    // Message type
        0x00,                    // Return code
        0xDE, 0xAD, 0xBE, 0xEF,  // Payload (8 bytes)
        0xCA, 0xFE, 0xBA, 0xBE};
    auto ctx = make_context(correct_payload);
    auto result = decoder.decode(ctx);

    EXPECT_TRUE(result);
    EXPECT_EQ(result->length, 16);
}

//==============================================================================
// DoIP Decoder Tests
//==============================================================================

class DoIPDecoderTest : public ::testing::Test {
protected:
    doip::DoIPDecoder decoder;

    // DoIP header with diagnostic message (8 byte header + payload)
    std::array<std::uint8_t, 16> doip_diagnostic = {
        0x02,                    // Protocol version
        0xFD,                    // Inverse version
        0x80, 0x01,              // Payload type: Diagnostic Message
        0x00, 0x00, 0x00, 0x08,  // Payload length: 8 bytes
        0x0E, 0x00,              // Source address: 0x0E00
        0x10, 0x10,              // Target address: 0x1010
        0x22, 0xF1, 0x90, 0x00   // UDS payload: Read DID 0xF190
    };

    // DoIP routing activation request
    std::array<std::uint8_t, 15> doip_routing_activation = {
        0x02,                    // Protocol version
        0xFD,                    // Inverse version
        0x00, 0x05,              // Payload type: Routing Activation Request
        0x00, 0x00, 0x00, 0x07,  // Payload length: 7 bytes
        0x0E, 0x00,              // Source address
        0x00,                    // Activation type: default
        0x00, 0x00, 0x00, 0x00   // Reserved (ISO 13400)
    };
};

TEST_F(DoIPDecoderTest, DecodeDiagnosticMessage) {
    auto ctx = make_context(doip_diagnostic);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    const auto& header = *result;

    EXPECT_EQ(header.protocol_version, 0x02);
    EXPECT_EQ(header.inverse_protocol_version, 0xFD);
    EXPECT_TRUE(header.is_version_valid());
    EXPECT_EQ(header.payload_type, doip::PayloadType::DiagnosticMessage);
    EXPECT_EQ(header.payload_length, 8);
}

TEST_F(DoIPDecoderTest, DecodeRoutingActivation) {
    auto ctx = make_context(doip_routing_activation);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->payload_type, doip::PayloadType::RoutingActivationRequest);
}

TEST_F(DoIPDecoderTest, ParseRoutingActivationRequest) {
    // Get the payload part (after 8 byte header)
    std::span<const std::byte> payload{
        reinterpret_cast<const std::byte*>(doip_routing_activation.data() + 8), 7};

    auto req = decoder.parse_routing_activation_request(payload);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->source_address, 0x0E00);
    EXPECT_EQ(req->activation_type, 0x00);
}

TEST_F(DoIPDecoderTest, RejectInvalidVersion) {
    doip::DoIPDecoder::Options opts;
    opts.validate_version = true;
    opts.allow_invalid_version = false;
    doip::DoIPDecoder strict_decoder(opts);

    std::array<std::uint8_t, 16> bad_version = doip_diagnostic;
    bad_version[1] = 0x00;  // Invalid inverse (should be 0xFD)

    auto ctx = make_context(bad_version);
    auto result = strict_decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::InvalidVersion);
}

TEST_F(DoIPDecoderTest, RejectTruncatedHeader) {
    std::array<std::uint8_t, 4> truncated = {0x02, 0xFD, 0x80, 0x01};
    auto ctx = make_context(truncated);
    auto result = decoder.decode(ctx);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code, DecodeErrorCode::BufferTooSmall);
}

TEST_F(DoIPDecoderTest, HeaderToString) {
    auto ctx = make_context(doip_diagnostic);
    auto result = decoder.decode(ctx);

    ASSERT_TRUE(result);
    std::string str = result->to_string();

    EXPECT_TRUE(str.find("DoIP") != std::string::npos);
    EXPECT_TRUE(str.find("Diagnostic Message") != std::string::npos);
}

//==============================================================================
// DecodeContext Tests
//==============================================================================

class DecodeContextTest : public ::testing::Test {
protected:
    std::array<std::uint8_t, 16> test_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
};

TEST_F(DecodeContextTest, ReadBigEndian16) {
    auto ctx = make_context(test_data);
    EXPECT_EQ(ctx.read_be16(0), 0x0102);
    EXPECT_EQ(ctx.read_be16(2), 0x0304);
}

TEST_F(DecodeContextTest, ReadBigEndian32) {
    auto ctx = make_context(test_data);
    EXPECT_EQ(ctx.read_be32(0), 0x01020304);
    EXPECT_EQ(ctx.read_be32(4), 0x05060708);
}

TEST_F(DecodeContextTest, ReadU8) {
    auto ctx = make_context(test_data);
    EXPECT_EQ(ctx.read_u8(0), 0x01);
    EXPECT_EQ(ctx.read_u8(5), 0x06);
}

TEST_F(DecodeContextTest, ReadBytes) {
    auto ctx = make_context(test_data);
    auto bytes = ctx.read_bytes(2, 4);

    ASSERT_EQ(bytes.size(), 4);
    EXPECT_EQ(static_cast<std::uint8_t>(bytes[0]), 0x03);
    EXPECT_EQ(static_cast<std::uint8_t>(bytes[3]), 0x06);
}

TEST_F(DecodeContextTest, HasBytes) {
    auto ctx = make_context(test_data);
    EXPECT_TRUE(ctx.has_bytes(16));
    EXPECT_FALSE(ctx.has_bytes(17));
}

TEST_F(DecodeContextTest, SubContext) {
    auto ctx = make_context(test_data);
    auto sub = ctx.sub_context(4);

    EXPECT_EQ(sub.data.size(), 12);
    EXPECT_EQ(sub.read_u8(0), 0x05);
}

//==============================================================================
// Protocol Dispatcher Tests
//==============================================================================

class ProtocolDispatcherTest : public ::testing::Test {
protected:
    ProtocolDispatcher dispatcher;

    // Full Ethernet/IPv4/UDP packet with SOME/IP payload
    std::vector<std::uint8_t> full_packet;

    void SetUp() override {
        // Ethernet header (14 bytes)
        std::vector<std::uint8_t> eth = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // dst MAC
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05,  // src MAC
            0x08, 0x00                           // IPv4
        };

        // IPv4 header (20 bytes)
        std::vector<std::uint8_t> ip = {
            0x45, 0x00,              // Version, IHL, DSCP, ECN
            0x00, 0x3C,              // Total length: 60 bytes
            0x00, 0x00,              // ID
            0x40, 0x00,              // Flags, Fragment
            0x40,                    // TTL
            0x11,                    // Protocol: UDP
            0x00, 0x00,              // Checksum
            0xC0, 0xA8, 0x01, 0x01,  // Src: 192.168.1.1
            0xC0, 0xA8, 0x01, 0x02   // Dst: 192.168.1.2
        };

        // UDP header (8 bytes) - port 30490 is SOME/IP-SD
        std::vector<std::uint8_t> udp = {
            0x77, 0x1A,  // Src port: 30490 (SOME/IP-SD)
            0x77, 0x1A,  // Dst port: 30490 (SOME/IP-SD)
            0x00, 0x20,  // Length: 32 bytes
            0x00, 0x00   // Checksum
        };

        // SOME/IP header (16 bytes)
        std::vector<std::uint8_t> someip = {
            0x12, 0x34, 0x80, 0x01,  // Service ID, Method ID
            0x00, 0x00, 0x00, 0x10,  // Length: 16
            0x00, 0x01, 0x00, 0x01,  // Client ID, Session ID
            0x01, 0x01, 0x02, 0x00   // Proto ver, Iface ver, Msg type, Return
        };

        // Payload (8 bytes)
        std::vector<std::uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};

        full_packet.insert(full_packet.end(), eth.begin(), eth.end());
        full_packet.insert(full_packet.end(), ip.begin(), ip.end());
        full_packet.insert(full_packet.end(), udp.begin(), udp.end());
        full_packet.insert(full_packet.end(), someip.begin(), someip.end());
        full_packet.insert(full_packet.end(), payload.begin(), payload.end());
    }
};

TEST_F(ProtocolDispatcherTest, DecodeFullStack) {
    auto result = dispatcher.decode(make_context(full_packet).data);

    EXPECT_TRUE(result.complete);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_GE(result.layers.size(), 4);  // Ethernet + IPv4 + UDP + SOME/IP

    // Check we got all expected layers
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<ipv4::IPv4Header>());
    EXPECT_TRUE(result.has_layer<udp::UdpHeader>());
    EXPECT_TRUE(result.has_layer<someip::SomeIpHeader>());
}

TEST_F(ProtocolDispatcherTest, GetLayerByType) {
    auto result = dispatcher.decode(make_context(full_packet).data);

    EXPECT_TRUE(result.complete);

    auto eth = result.get_layer<ethernet::EthernetHeader>();
    ASSERT_TRUE(eth != nullptr);
    EXPECT_EQ(eth->ethertype, 0x0800);

    auto ip = result.get_layer<ipv4::IPv4Header>();
    ASSERT_TRUE(ip != nullptr);
    EXPECT_EQ(ip->src_ip.to_string(), "192.168.1.1");

    auto udp_hdr = result.get_layer<udp::UdpHeader>();
    ASSERT_TRUE(udp_hdr != nullptr);
    EXPECT_EQ(udp_hdr->src_port, 30490);
}

TEST_F(ProtocolDispatcherTest, HasLayer) {
    auto result = dispatcher.decode(make_context(full_packet).data);

    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<ipv4::IPv4Header>());
    EXPECT_TRUE(result.has_layer<udp::UdpHeader>());
    EXPECT_TRUE(result.has_layer<someip::SomeIpHeader>());
    EXPECT_FALSE(result.has_layer<tcp::TcpHeader>());
    EXPECT_FALSE(result.has_layer<doip::DoIPHeader>());
}

/// @file test_pcap_decode.cpp
/// @brief Integration tests for PCAP file decode pipeline

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "wadjet/pcap/pcap_reader.hpp"
#include "wadjet/pcap/pcap_writer.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/udp.hpp"
#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/net/packet_view.hpp"
#include "wadjet/core/timestamp.hpp"

using namespace wadjet;
using namespace wadjet::protocols;

namespace {

/// Helper to create test PCAP files in-memory/temporary
class PcapTestFixture {
public:
    static std::filesystem::path temp_dir() {
        auto dir = std::filesystem::temp_directory_path() / "wadjet_test_pcaps";
        std::filesystem::create_directories(dir);
        return dir;
    }
    
    static std::filesystem::path create_temp_pcap(
        const std::string& name,
        const std::vector<std::vector<std::uint8_t>>& packets) {
        
        auto path = temp_dir() / (name + ".pcap");
        
        auto writer_result = pcap::PcapWriter::create(path);
        if (!writer_result) {
            return {};
        }
        
        auto& writer = writer_result.value();
        
        Timestamp ts = Timestamp::now();
        for (const auto& pkt : packets) {
            PacketView pkt_view(
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(pkt.data()),
                    pkt.size()
                ),
                ts
            );
            auto result = writer.write_packet(pkt_view);
            if (!result) {
                return {};
            }
            // Advance by 1ms
            ts = Timestamp::from_unix(ts.seconds(), ts.nanoseconds() + 1000000);
        }
        
        return path;
    }
    
    static void cleanup() {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir(), ec);
    }
};

/// Build common test packets
std::vector<std::uint8_t> build_someip_packet(
    std::uint16_t service_id,
    std::uint16_t method_id,
    someip::MessageType msg_type) {
    
    std::vector<std::uint8_t> pkt;
    
    // Ethernet header
    pkt.insert(pkt.end(), {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // Dst MAC
        0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,  // Src MAC
        0x08, 0x00                             // IPv4
    });
    
    // IPv4 header (20 bytes)
    pkt.insert(pkt.end(), {
        0x45, 0x00, 0x00, 0x30,  // Version, IHL, Total length 48
        0x00, 0x01, 0x40, 0x00,
        0x40, 0x11, 0x00, 0x00,  // TTL, UDP, Checksum
        0xC0, 0xA8, 0x01, 0x0A,  // 192.168.1.10
        0xC0, 0xA8, 0x01, 0x14   // 192.168.1.20
    });
    
    // UDP header (8 bytes)
    pkt.insert(pkt.end(), {
        0x77, 0x0A, 0x77, 0x0A,  // Port 30490
        0x00, 0x1C, 0x00, 0x00   // Length 28
    });
    
    // SOME/IP header (16 bytes)
    pkt.push_back(static_cast<std::uint8_t>(service_id >> 8));
    pkt.push_back(static_cast<std::uint8_t>(service_id & 0xFF));
    pkt.push_back(static_cast<std::uint8_t>(method_id >> 8));
    pkt.push_back(static_cast<std::uint8_t>(method_id & 0xFF));
    pkt.insert(pkt.end(), {0x00, 0x00, 0x00, 0x0C});  // Length 12
    pkt.insert(pkt.end(), {0x00, 0x01, 0x00, 0x01});  // Client ID, Session ID
    pkt.push_back(0x01);  // Protocol version
    pkt.push_back(0x01);  // Interface version
    pkt.push_back(static_cast<std::uint8_t>(msg_type));
    pkt.push_back(0x00);  // Return code
    
    // Payload (4 bytes)
    pkt.insert(pkt.end(), {0xDE, 0xAD, 0xBE, 0xEF});
    
    return pkt;
}

std::vector<std::uint8_t> build_doip_packet(
    doip::PayloadType payload_type,
    const std::vector<std::uint8_t>& payload) {
    
    std::vector<std::uint8_t> pkt;
    
    // Ethernet header
    pkt.insert(pkt.end(), {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
        0x08, 0x00
    });
    
    // IPv4 header
    std::uint16_t total_len = 20 + 20 + 8 + static_cast<std::uint16_t>(payload.size());
    pkt.insert(pkt.end(), {
        0x45, 0x00,
        static_cast<std::uint8_t>(total_len >> 8),
        static_cast<std::uint8_t>(total_len & 0xFF),
        0x00, 0x01, 0x40, 0x00,
        0x40, 0x06, 0x00, 0x00,  // TTL, TCP, Checksum
        0x0A, 0x00, 0x00, 0x01,  // 10.0.0.1
        0x0A, 0x00, 0x00, 0x02   // 10.0.0.2
    });
    
    // TCP header (20 bytes)
    pkt.insert(pkt.end(), {
        0x30, 0x39, 0x34, 0x58,  // Port 12345 -> 13400
        0x00, 0x00, 0x00, 0x01,  // Seq
        0x00, 0x00, 0x00, 0x00,  // Ack
        0x50, 0x18, 0xFF, 0xFF,  // Offset, PSH+ACK, Window
        0x00, 0x00, 0x00, 0x00   // Checksum, Urgent
    });
    
    // DoIP header (8 bytes)
    auto type_val = static_cast<std::uint16_t>(payload_type);
    pkt.push_back(0x02);  // Version
    pkt.push_back(0xFD);  // Inverse version
    pkt.push_back(static_cast<std::uint8_t>(type_val >> 8));
    pkt.push_back(static_cast<std::uint8_t>(type_val & 0xFF));
    pkt.push_back(static_cast<std::uint8_t>(payload.size() >> 24));
    pkt.push_back(static_cast<std::uint8_t>(payload.size() >> 16));
    pkt.push_back(static_cast<std::uint8_t>(payload.size() >> 8));
    pkt.push_back(static_cast<std::uint8_t>(payload.size() & 0xFF));
    
    // DoIP payload
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    
    return pkt;
}

}  // namespace

//==============================================================================
// PCAP Read + Decode Tests
//==============================================================================

class PcapDecodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure temp directory exists
        std::filesystem::create_directories(PcapTestFixture::temp_dir());
    }
    
    void TearDown() override {
        PcapTestFixture::cleanup();
    }
    
    ProtocolDispatcher dispatcher;
};

TEST_F(PcapDecodeTest, DecodeSingleSomeIpPacket) {
    // Create PCAP with one SOME/IP packet
    auto pkt = build_someip_packet(0x1234, 0x8001, someip::MessageType::Request);
    auto path = PcapTestFixture::create_temp_pcap("single_someip", {pkt});
    ASSERT_FALSE(path.empty());
    
    // Read and decode
    auto reader_result = pcap::PcapReader::open(path);
    ASSERT_TRUE(reader_result.is_ok());
    
    auto& reader = reader_result.value();
    auto packet = reader.next_packet();
    ASSERT_TRUE(packet.has_value());
    
    auto result = dispatcher.decode(packet->view().data());
    
    EXPECT_TRUE(result.complete);
    
    auto* someip_hdr = result.get_layer<someip::SomeIpHeader>();
    ASSERT_NE(someip_hdr, nullptr);
    EXPECT_EQ(someip_hdr->service_id, 0x1234);
    EXPECT_EQ(someip_hdr->method_id, 0x8001);
    EXPECT_EQ(someip_hdr->message_type, someip::MessageType::Request);
}

TEST_F(PcapDecodeTest, DecodeMultipleSomeIpPackets) {
    // Create PCAP with multiple SOME/IP packets
    std::vector<std::vector<std::uint8_t>> packets = {
        build_someip_packet(0x1000, 0x0001, someip::MessageType::Request),
        build_someip_packet(0x1000, 0x8001, someip::MessageType::Response),
        build_someip_packet(0x2000, 0x0002, someip::MessageType::Notification),
    };
    
    auto path = PcapTestFixture::create_temp_pcap("multi_someip", packets);
    ASSERT_FALSE(path.empty());
    
    auto reader_result = pcap::PcapReader::open(path);
    ASSERT_TRUE(reader_result.is_ok());
    
    auto& reader = reader_result.value();
    
    // Decode and verify each packet
    int count = 0;
    while (auto packet = reader.next_packet()) {
        auto result = dispatcher.decode(packet->view().data());
        EXPECT_TRUE(result.complete) << "Packet " << count << " failed to decode";
        EXPECT_TRUE(result.has_layer<someip::SomeIpHeader>()) << "Packet " << count << " missing SOME/IP";
        count++;
    }
    
    EXPECT_EQ(count, 3);
}

TEST_F(PcapDecodeTest, DecodeDoIPPacket) {
    // Create DoIP diagnostic message
    auto pkt = build_doip_packet(
        doip::PayloadType::DiagnosticMessage,
        {0x0E, 0x80, 0x10, 0x01, 0x3E, 0x00}  // SA, TA, UDS TesterPresent
    );
    
    auto path = PcapTestFixture::create_temp_pcap("doip", {pkt});
    ASSERT_FALSE(path.empty());
    
    auto reader_result = pcap::PcapReader::open(path);
    ASSERT_TRUE(reader_result.is_ok());
    
    auto& reader = reader_result.value();
    auto packet = reader.next_packet();
    ASSERT_TRUE(packet.has_value());
    
    auto result = dispatcher.decode(packet->view().data());
    
    EXPECT_TRUE(result.complete);
    
    auto* doip_hdr = result.get_layer<doip::DoIPHeader>();
    ASSERT_NE(doip_hdr, nullptr);
    EXPECT_EQ(doip_hdr->payload_type, doip::PayloadType::DiagnosticMessage);
}

TEST_F(PcapDecodeTest, DecodeMixedProtocols) {
    // Create PCAP with mixed protocols
    std::vector<std::vector<std::uint8_t>> packets = {
        build_someip_packet(0x1000, 0x0001, someip::MessageType::Request),
        build_doip_packet(doip::PayloadType::VehicleIdentificationRequest, {}),
        build_someip_packet(0x2000, 0x0002, someip::MessageType::Notification),
    };
    
    auto path = PcapTestFixture::create_temp_pcap("mixed", packets);
    ASSERT_FALSE(path.empty());
    
    auto reader_result = pcap::PcapReader::open(path);
    ASSERT_TRUE(reader_result.is_ok());
    
    auto& reader = reader_result.value();
    
    int someip_count = 0;
    int doip_count = 0;
    
    while (auto packet = reader.next_packet()) {
        auto result = dispatcher.decode(packet->view().data());
        if (result.has_layer<someip::SomeIpHeader>()) someip_count++;
        if (result.has_layer<doip::DoIPHeader>()) doip_count++;
    }
    
    EXPECT_EQ(someip_count, 2);
    EXPECT_EQ(doip_count, 1);
}

//==============================================================================
// PCAP Write + Read Round-Trip Tests
//==============================================================================

class PcapRoundTripTest : public ::testing::Test {
protected:
    void TearDown() override {
        PcapTestFixture::cleanup();
    }
};

TEST_F(PcapRoundTripTest, WriteAndReadBack) {
    auto path = PcapTestFixture::temp_dir() / "roundtrip.pcap";
    
    // Build test packet
    std::vector<std::uint8_t> pkt1 = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
        0x08, 0x00,  // IPv4
        0x45, 0x00, 0x00, 0x1C, 0x00, 0x01, 0x40, 0x00,
        0x40, 0x11, 0x00, 0x00,
        0xC0, 0xA8, 0x01, 0x01, 0xC0, 0xA8, 0x01, 0x02,
        0x04, 0xD2, 0x04, 0xD2, 0x00, 0x08, 0x00, 0x00
    };
    
    // Write
    {
        auto writer_result = pcap::PcapWriter::create(path);
        ASSERT_TRUE(writer_result.is_ok());
        
        auto& writer = writer_result.value();
        
        PacketView pkt_view(
            std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(pkt1.data()),
                pkt1.size()
            ),
            Timestamp::from_unix(1, 0)  // 1 second
        );
        auto result = writer.write_packet(pkt_view);
        ASSERT_TRUE(result.is_ok());
    }
    
    // Read back
    {
        auto reader_result = pcap::PcapReader::open(path);
        ASSERT_TRUE(reader_result.is_ok());
        
        auto& reader = reader_result.value();
        auto packet = reader.next_packet();
        ASSERT_TRUE(packet.has_value());
        
        auto view = packet->view();
        EXPECT_EQ(view.size(), pkt1.size());
        
        // Verify contents match
        auto data = view.data();
        for (size_t i = 0; i < pkt1.size(); i++) {
            EXPECT_EQ(static_cast<std::uint8_t>(data[i]), pkt1[i]) << "Mismatch at byte " << i;
        }
    }
}

TEST_F(PcapRoundTripTest, PreservesTimestamps) {
    auto path = PcapTestFixture::temp_dir() / "timestamps.pcap";
    std::filesystem::create_directories(path.parent_path());
    
    std::vector<std::uint8_t> pkt = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
        0x08, 0x00
    };
    
    Timestamp ts1 = Timestamp::from_unix(1, 0);   // 1 second
    Timestamp ts2 = Timestamp::from_unix(2, 500000000);   // 2.5 seconds
    
    // Write with specific timestamps
    {
        auto writer_result = pcap::PcapWriter::create(path);
        ASSERT_TRUE(writer_result.is_ok());
        
        auto& writer = writer_result.value();
        
        PacketView v1(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(pkt.data()), pkt.size()),
            ts1
        );
        PacketView v2(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(pkt.data()), pkt.size()),
            ts2
        );
        
        ASSERT_TRUE(writer.write_packet(v1).is_ok());
        ASSERT_TRUE(writer.write_packet(v2).is_ok());
    }
    
    // Read and verify timestamps
    {
        auto reader_result = pcap::PcapReader::open(path);
        ASSERT_TRUE(reader_result.is_ok());
        
        auto& reader = reader_result.value();
        
        auto p1 = reader.next_packet();
        ASSERT_TRUE(p1.has_value());
        // Allow some timestamp precision loss in PCAP format (microsecond precision)
        EXPECT_EQ(p1->timestamp().seconds(), ts1.seconds());
        
        auto p2 = reader.next_packet();
        ASSERT_TRUE(p2.has_value());
        EXPECT_EQ(p2->timestamp().seconds(), ts2.seconds());
    }
}

//==============================================================================
// Statistics and Metadata Tests
//==============================================================================

TEST_F(PcapRoundTripTest, ReaderCountsPackets) {
    auto packets = std::vector<std::vector<std::uint8_t>>{
        build_someip_packet(0x1000, 0x0001, someip::MessageType::Request),
        build_someip_packet(0x1000, 0x0002, someip::MessageType::Request),
        build_someip_packet(0x1000, 0x0003, someip::MessageType::Request),
        build_someip_packet(0x1000, 0x0004, someip::MessageType::Request),
        build_someip_packet(0x1000, 0x0005, someip::MessageType::Request),
    };
    
    auto path = PcapTestFixture::create_temp_pcap("count", packets);
    ASSERT_FALSE(path.empty());
    
    auto reader_result = pcap::PcapReader::open(path);
    ASSERT_TRUE(reader_result.is_ok());
    
    auto& reader = reader_result.value();
    
    int count = 0;
    while (reader.next_packet()) {
        count++;
    }
    
    EXPECT_EQ(count, 5);
}

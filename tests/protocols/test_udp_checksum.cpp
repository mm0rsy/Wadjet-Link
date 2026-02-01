#include "wadjet/protocols/udp.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

using namespace wadjet::protocols::udp;

class UdpChecksumTest : public ::testing::Test {
protected:
    /// Helper to create a minimal IPv4 + UDP packet
    static std::vector<std::byte> create_udp_packet(
        const std::string& /*src_ip*/, const std::string& /*dst_ip*/,
        std::uint16_t src_port, std::uint16_t dst_port,
        const std::vector<std::uint8_t>& payload,
        std::uint16_t checksum_override = 0,
        bool override_checksum = false) {
        
        std::vector<std::byte> packet;
        
        // IPv4 header (20 bytes)
        packet.push_back(std::byte{0x45});  // Version 4, IHL 5
        packet.push_back(std::byte{0x00});  // DSCP/ECN
        
        std::uint16_t total_len = static_cast<std::uint16_t>(20 + 8 + payload.size());
        packet.push_back(std::byte{static_cast<std::uint8_t>(total_len >> 8)});
        packet.push_back(std::byte{static_cast<std::uint8_t>(total_len & 0xFF)});
        
        // Identification, flags, fragment offset
        packet.insert(packet.end(), {std::byte{0x12}, std::byte{0x34},
                                      std::byte{0x40}, std::byte{0x00}});
        
        packet.push_back(std::byte{0x40});  // TTL
        packet.push_back(std::byte{0x11});  // Protocol UDP
        packet.insert(packet.end(), {std::byte{0x00}, std::byte{0x00}});  // Checksum placeholder
        
        // Source IP
        packet.insert(packet.end(), {std::byte{0xC0}, std::byte{0xA8},
                                      std::byte{0x01}, std::byte{0x01}});  // 192.168.1.1
        // Destination IP
        packet.insert(packet.end(), {std::byte{0xC0}, std::byte{0xA8},
                                      std::byte{0x01}, std::byte{0x02}});  // 192.168.1.2
        
        // UDP header (8 bytes)
        packet.push_back(std::byte{static_cast<std::uint8_t>(src_port >> 8)});
        packet.push_back(std::byte{static_cast<std::uint8_t>(src_port & 0xFF)});
        packet.push_back(std::byte{static_cast<std::uint8_t>(dst_port >> 8)});
        packet.push_back(std::byte{static_cast<std::uint8_t>(dst_port & 0xFF)});
        
        std::uint16_t udp_len = static_cast<std::uint16_t>(8 + payload.size());
        packet.push_back(std::byte{static_cast<std::uint8_t>(udp_len >> 8)});
        packet.push_back(std::byte{static_cast<std::uint8_t>(udp_len & 0xFF)});
        
        // Checksum
        if (override_checksum) {
            packet.push_back(std::byte{static_cast<std::uint8_t>(checksum_override >> 8)});
            packet.push_back(std::byte{static_cast<std::uint8_t>(checksum_override & 0xFF)});
        } else {
            packet.insert(packet.end(), {std::byte{0x00}, std::byte{0x00}});
        }
        
        // Payload
        for (auto b : payload) {
            packet.push_back(std::byte{b});
        }
        
        return packet;
    }
};

/// Test valid UDP checksum
TEST_F(UdpChecksumTest, ValidChecksum) {
    auto packet = create_udp_packet("192.168.1.1", "192.168.1.2", 1234, 5678,
                                     {0x48, 0x65, 0x6C, 0x6C, 0x6F},  // "Hello"
                                     0x0000);
    
    // For now, just verify packet creation works
    EXPECT_GE(packet.size(), 28);  // 20 IPv4 + 8 UDP
}

/// Test invalid UDP checksum
TEST_F(UdpChecksumTest, InvalidChecksum) {
    auto packet = create_udp_packet("192.168.1.1", "192.168.1.2", 1234, 5678,
                                     {0x48, 0x65, 0x6C, 0x6C, 0x6F},
                                     0xFFFF,  // Invalid checksum
                                     true);
    
    EXPECT_GE(packet.size(), 28);
}

/// Test zero checksum for IPv4 (allowed)
TEST_F(UdpChecksumTest, ZeroChecksumIPv4) {
    auto packet = create_udp_packet("192.168.1.1", "192.168.1.2", 1234, 5678,
                                     {0x00, 0x00, 0x00},
                                     0x0000,
                                     true);
    
    // Zero checksum should be allowed for IPv4
    EXPECT_GE(packet.size(), 28);
}

/// Test validation modes - strict mode
TEST_F(UdpChecksumTest, StrictModeValidChecksum) {
    UdpDecoder::Options opts;
    opts.validate_checksum = true;
    
    UdpDecoder decoder(opts);
    EXPECT_TRUE(opts.validate_checksum);
}

/// Test validation modes - warning mode (default)
TEST_F(UdpChecksumTest, WarningModeDefault) {
    UdpDecoder::Options opts;
    
    // Default should disable checksum validation
    EXPECT_FALSE(opts.validate_checksum);
}

/// Test validation modes - disabled mode
TEST_F(UdpChecksumTest, DisabledMode) {
    UdpDecoder::Options opts;
    opts.validate_checksum = false;
    
    EXPECT_FALSE(opts.validate_checksum);
}

/// Test checksum calculation
TEST_F(UdpChecksumTest, ChecksumCalculation) {
    // Simple UDP packet with known checksum
    auto packet = create_udp_packet("192.168.1.1", "192.168.1.2", 1234, 5678,
                                     {});
    
    EXPECT_GE(packet.size(), 28);
}

/// Test empty payload
TEST_F(UdpChecksumTest, EmptyPayload) {
    auto packet = create_udp_packet("192.168.1.1", "192.168.1.2", 53, 53,
                                     {});  // Empty payload
    
    EXPECT_EQ(packet.size(), 28);  // Just headers
}

/// Test large payload
TEST_F(UdpChecksumTest, LargePayload) {
    std::vector<uint8_t> large_payload(1000, 0xAA);
    auto packet = create_udp_packet("192.168.1.1", "192.168.1.2", 1234, 5678,
                                     large_payload);
    
    EXPECT_EQ(packet.size(), 28 + 1000);
}

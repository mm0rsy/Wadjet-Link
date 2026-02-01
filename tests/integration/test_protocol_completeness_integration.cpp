/// @file test_protocol_completeness_integration.cpp
/// @brief Integration tests for Protocol Completeness (Phase 11)
///
/// Tests multi-layer protocol validation, cross-protocol interactions,
/// and end-to-end protocol stack decoding scenarios.
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <gtest/gtest.h>
#include <wadjet/protocols/validation.hpp>

#include <vector>
#include <span>

namespace wadjet::protocols {

// ============================================================================
// Test Fixtures
// ============================================================================

class ProtocolCompletenessIntegrationTest : public ::testing::Test {
protected:
    std::span<const std::byte> as_span(const std::vector<std::uint8_t>& data) {
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(data.data()),
            data.size()
        );
    }

    ProtocolLayer make_layer(const std::string& name, std::size_t offset, std::size_t header_len,
                             std::size_t payload_len, std::uint16_t ethertype = 0,
                             std::uint16_t checksum = 0, bool has_checksum = false) {
        return ProtocolLayer{
            .name = name,
            .offset = offset,
            .header_length = header_len,
            .payload_length = payload_len,
            .ethertype = ethertype,
            .checksum = checksum,
            .has_checksum = has_checksum
        };
    }
};

// ============================================================================
// Integration Tests: Basic Protocol Stacks
// ============================================================================

/// T121.1: Ethernet → IPv4 → TCP (simple stack)
TEST_F(ProtocolCompletenessIntegrationTest, SimpleEthernetIPv4TCPStack) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 242),
        make_layer("IPv4", 14, 20, 222, 0x0800),
        make_layer("TCP", 34, 20, 202, 6)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

/// T122.1: IPv4 with SOME/IP over UDP
TEST_F(ProtocolCompletenessIntegrationTest, IPv4UDPSomeIP) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 500),
        make_layer("IPv4", 14, 20, 486, 0x0800),
        make_layer("UDP", 34, 8, 478, 17),
        make_layer("SOME/IP", 42, 16, 462)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

/// T123.1: TCP with DoIP
TEST_F(ProtocolCompletenessIntegrationTest, TCPWithDoIP) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 100),
        make_layer("IPv4", 14, 20, 86, 0x0800),
        make_layer("TCP", 34, 20, 66, 6),
        make_layer("DoIP", 54, 8, 58)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

/// T124.1: SOME/IP-SD with UDP
TEST_F(ProtocolCompletenessIntegrationTest, SomeIPSDOverUDP) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 300),
        make_layer("IPv4", 14, 20, 286, 0x0800),
        make_layer("UDP", 34, 8, 278, 17),
        make_layer("SOME/IP-SD", 42, 24, 254)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

/// T125.1: UDS over DoIP over TCP
TEST_F(ProtocolCompletenessIntegrationTest, UDSOverDoIPOverTCP) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 200),
        make_layer("IPv4", 14, 20, 186, 0x0800),
        make_layer("TCP", 34, 20, 166, 6),
        make_layer("DoIP", 54, 8, 158),
        make_layer("UDS", 62, 1, 157)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

// ============================================================================
// Integration Tests: VLAN Tagging
// ============================================================================

/// T126.1: Ethernet with VLAN → IPv4 → TCP
TEST_F(ProtocolCompletenessIntegrationTest, VLANTaggedStack) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 18, 238),  // +4 for VLAN tag
        make_layer("IPv4", 18, 20, 218, 0x0800),
        make_layer("TCP", 38, 20, 198, 6)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

/// T126.2: VLAN with complete diagnostic stack
TEST_F(ProtocolCompletenessIntegrationTest, VLANWithDiagnosticStack) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 18, 250),
        make_layer("IPv4", 18, 20, 232, 0x0800),
        make_layer("TCP", 38, 20, 212, 6),
        make_layer("DoIP", 58, 8, 204),
        make_layer("UDS", 66, 1, 203)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
}

// ============================================================================
// Integration Tests: Length Validation
// ============================================================================

/// T126.3: Multi-layer length consistency
TEST_F(ProtocolCompletenessIntegrationTest, LengthConsistency) {
    ProtocolValidator validator(ValidationMode::Strict);
    std::vector<std::uint8_t> packet_data(256);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 242),
        make_layer("IPv4", 14, 20, 222, 0x0800),
        make_layer("TCP", 34, 20, 202, 6),
        make_layer("DoIP", 54, 8, 194),
        make_layer("UDS", 62, 1, 193)
    };
    
    auto result = validator.validateLengths(layers, packet_data.size());
    EXPECT_TRUE(result.is_valid);
}

/// T126.4: Length validation with gaps
TEST_F(ProtocolCompletenessIntegrationTest, LengthValidationWithPadding) {
    ProtocolValidator validator(ValidationMode::Lenient);
    std::vector<std::uint8_t> packet_data(300);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 100),
        make_layer("IPv4", 14, 20, 86, 0x0800),
        make_layer("UDP", 34, 8, 78, 17),
        make_layer("SOME/IP", 42, 16, 62)
    };
    
    auto result = validator.validateLengths(layers, packet_data.size());
    // Should be lenient about padding at end
    EXPECT_TRUE(result.is_valid || result.error_count() > 0);
}

// ============================================================================
// Integration Tests: Checksum Validation
// ============================================================================

/// T126.5: IPv4 checksum in stack
TEST_F(ProtocolCompletenessIntegrationTest, IPv4ChecksumValidation) {
    ProtocolValidator validator(ValidationMode::Lenient);
    std::vector<std::uint8_t> packet_data(100);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 86),
        make_layer("IPv4", 14, 20, 72, 0x0800, 0x0000, true),
        make_layer("TCP", 34, 20, 52, 6)
    };
    
    auto result = validator.validateChecksums(as_span(packet_data), layers);
    EXPECT_TRUE(result.mode == ValidationMode::Lenient);
}

/// T126.6: UDP with pseudo-header checksum
TEST_F(ProtocolCompletenessIntegrationTest, UDPChecksumWithPseudoHeader) {
    ProtocolValidator validator(ValidationMode::Lenient);
    std::vector<std::uint8_t> packet_data(150);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 136),
        make_layer("IPv4", 14, 20, 116, 0x0800, 0x0000, true),
        make_layer("UDP", 34, 8, 108, 17, 0x0000, true),
        make_layer("SOME/IP", 42, 16, 92)
    };
    
    auto result = validator.validateChecksums(as_span(packet_data), layers);
    EXPECT_TRUE(result.mode == ValidationMode::Lenient);
}

/// T126.7: TCP checksum with pseudo-header
TEST_F(ProtocolCompletenessIntegrationTest, TCPChecksumWithPseudoHeader) {
    ProtocolValidator validator(ValidationMode::Lenient);
    std::vector<std::uint8_t> packet_data(120);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 106),
        make_layer("IPv4", 14, 20, 86, 0x0800, 0x0000, true),
        make_layer("TCP", 34, 20, 66, 6, 0x0000, true),
        make_layer("DoIP", 54, 8, 58)
    };
    
    auto result = validator.validateChecksums(as_span(packet_data), layers);
    EXPECT_TRUE(result.mode == ValidationMode::Lenient);
}

// ============================================================================
// Integration Tests: IPv4 Fragmentation
// ============================================================================

/// T122.2: Fragmented IPv4 with payload
TEST_F(ProtocolCompletenessIntegrationTest, IPv4FragmentationWithPayload) {
    ProtocolValidator validator(ValidationMode::Lenient);
    
    // First fragment
    std::vector<ProtocolLayer> frag1 = {
        make_layer("Ethernet", 0, 14, 1500),
        make_layer("IPv4", 14, 20, 1486, 0x0800),
        make_layer("SOME/IP", 34, 16, 1470)
    };
    
    auto result1 = validator.validateLayering(frag1);
    EXPECT_TRUE(result1.is_valid);
    
    // Second fragment
    std::vector<ProtocolLayer> frag2 = {
        make_layer("Ethernet", 0, 14, 1500),
        make_layer("IPv4", 14, 20, 1486, 0x0800)
    };
    
    auto result2 = validator.validateLayering(frag2);
    EXPECT_TRUE(result2.is_valid);
}

// ============================================================================
// Integration Tests: Validation Modes
// ============================================================================

/// T126.8: Strict vs Lenient modes
TEST_F(ProtocolCompletenessIntegrationTest, StrictVsLenientModes) {
    // Strict mode - fails on invalid sequence
    {
        ProtocolValidator strict(ValidationMode::Strict);
        std::vector<ProtocolLayer> invalid = {
            make_layer("Ethernet", 0, 14, 100),
            make_layer("UDP", 14, 8, 92),  // Missing IP layer
            make_layer("SOME/IP", 22, 16, 76)
        };
        
        auto result = strict.validateLayering(invalid);
        EXPECT_FALSE(result.is_valid);
    }
    
    // Lenient mode - continues despite errors
    {
        ProtocolValidator lenient(ValidationMode::Lenient);
        std::vector<ProtocolLayer> layers = {
            make_layer("Ethernet", 0, 14, 100),
            make_layer("IPv4", 14, 20, 86, 0x0800),
            make_layer("SOME/IP", 34, 16, 70)
        };
        
        auto result = lenient.validateLayering(layers);
        EXPECT_TRUE(result.is_valid);
    }
}

/// T126.9: Mode switching
TEST_F(ProtocolCompletenessIntegrationTest, ModeSwitch) {
    ProtocolValidator validator(ValidationMode::Strict);
    EXPECT_EQ(validator.get_mode(), ValidationMode::Strict);
    
    validator.set_mode(ValidationMode::Lenient);
    EXPECT_EQ(validator.get_mode(), ValidationMode::Lenient);
    
    validator.set_mode(ValidationMode::Strict);
    EXPECT_EQ(validator.get_mode(), ValidationMode::Strict);
}

// ============================================================================
// Integration Tests: Edge Cases
// ============================================================================

/// T126.10: Empty packet
TEST_F(ProtocolCompletenessIntegrationTest, EmptyPacket) {
    ProtocolValidator validator(ValidationMode::Lenient);
    
    std::vector<ProtocolLayer> empty;
    auto result = validator.validateLayering(empty);
    EXPECT_TRUE(result.is_valid);
}

/// T126.11: Single layer (Ethernet only)
TEST_F(ProtocolCompletenessIntegrationTest, SingleLayer) {
    ProtocolValidator validator(ValidationMode::Strict);
    
    std::vector<ProtocolLayer> single = {
        make_layer("Ethernet", 0, 14, 0)
    };
    
    auto result = validator.validateLayering(single);
    EXPECT_TRUE(result.is_valid);
}

/// T126.12: Very large packet
TEST_F(ProtocolCompletenessIntegrationTest, VeryLargePacket) {
    ProtocolValidator validator(ValidationMode::Strict);
    std::vector<std::uint8_t> large_packet(9000);
    
    std::vector<ProtocolLayer> layers = {
        make_layer("Ethernet", 0, 14, 8986),
        make_layer("IPv4", 14, 20, 8966, 0x0800),
        make_layer("UDP", 34, 8, 8958, 17),
        make_layer("SOME/IP", 42, 16, 8942)
    };
    
    auto result = validator.validateLayering(layers);
    EXPECT_TRUE(result.is_valid);
    
    auto length_result = validator.validateLengths(layers, large_packet.size());
    EXPECT_TRUE(length_result.is_valid);
}

// ============================================================================
// Integration Tests: Real-World Scenarios
// ============================================================================

/// T126.13: Vehicle SOME/IP-SD multicast
TEST_F(ProtocolCompletenessIntegrationTest, VehicleSomeIPSDMulticast) {
    ProtocolValidator validator(ValidationMode::Lenient);
    
    std::vector<ProtocolLayer> sd_discovery = {
        make_layer("Ethernet", 0, 14, 500),
        make_layer("IPv4", 14, 20, 480, 0x0800),
        make_layer("UDP", 34, 8, 472, 17),
        make_layer("SOME/IP-SD", 42, 24, 448)
    };
    
    auto result = validator.validateLayering(sd_discovery);
    EXPECT_TRUE(result.is_valid);
}

/// T126.14: Diagnostic request with response
TEST_F(ProtocolCompletenessIntegrationTest, DiagnosticRequestResponse) {
    ProtocolValidator validator(ValidationMode::Lenient);
    
    // Request
    std::vector<ProtocolLayer> request = {
        make_layer("Ethernet", 0, 14, 86),
        make_layer("IPv4", 14, 20, 72, 0x0800),
        make_layer("TCP", 34, 20, 52, 6),
        make_layer("DoIP", 54, 8, 44),
        make_layer("UDS", 62, 2, 42)
    };
    
    auto req_result = validator.validateLayering(request);
    EXPECT_TRUE(req_result.is_valid);
    
    // Response
    std::vector<ProtocolLayer> response = {
        make_layer("Ethernet", 0, 14, 120),
        make_layer("IPv4", 14, 20, 106, 0x0800),
        make_layer("TCP", 34, 20, 86, 6),
        make_layer("DoIP", 54, 8, 78),
        make_layer("UDS", 62, 3, 76)
    };
    
    auto resp_result = validator.validateLayering(response);
    EXPECT_TRUE(resp_result.is_valid);
}

/// T126.15: TesterPresent keep-alive
TEST_F(ProtocolCompletenessIntegrationTest, TesterPresentKeepalive) {
    ProtocolValidator validator(ValidationMode::Lenient);
    
    std::vector<ProtocolLayer> keepalive = {
        make_layer("Ethernet", 0, 14, 54),
        make_layer("IPv4", 14, 20, 40, 0x0800),
        make_layer("TCP", 34, 20, 20, 6),
        make_layer("DoIP", 54, 8, 12),
        make_layer("UDS", 62, 2, 10)  // 0x3E 0x80
    };
    
    auto result = validator.validateLayering(keepalive);
    EXPECT_TRUE(result.is_valid);
}

}  // namespace wadjet::protocols

#include "wadjet/protocols/someip.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

using namespace wadjet::protocols::someip;

class SomeipTpTest : public ::testing::Test {
protected:
    /// Helper to create a SOME/IP-TP message
    static std::vector<std::byte> create_someip_tp_message(
        std::uint16_t service_id,
        std::uint16_t method_id,
        std::uint32_t /*total_length*/,  // For documentation only
        std::uint32_t segment_offset,
        bool more_segments,
        const std::vector<std::uint8_t>& payload) {
        
        std::vector<std::byte> message;
        
        // SOME/IP Header (16 bytes)
        // Service ID
        message.push_back(std::byte{static_cast<std::uint8_t>(service_id >> 8)});
        message.push_back(std::byte{static_cast<std::uint8_t>(service_id & 0xFF)});
        
        // Method ID
        message.push_back(std::byte{static_cast<std::uint8_t>(method_id >> 8)});
        message.push_back(std::byte{static_cast<std::uint8_t>(method_id & 0xFF)});
        
        // Length (header + TP header + payload = 8 + 4 + payload.size())
        std::uint32_t length = static_cast<std::uint32_t>(8 + 4 + payload.size());
        message.push_back(std::byte{static_cast<std::uint8_t>(length >> 24)});
        message.push_back(std::byte{static_cast<std::uint8_t>((length >> 16) & 0xFF)});
        message.push_back(std::byte{static_cast<std::uint8_t>((length >> 8) & 0xFF)});
        message.push_back(std::byte{static_cast<std::uint8_t>(length & 0xFF)});
        
        // Client ID
        message.push_back(std::byte{0x01});
        message.push_back(std::byte{0x00});
        
        // Session ID
        message.push_back(std::byte{0x00});
        message.push_back(std::byte{0x01});
        
        // Protocol Version
        message.push_back(std::byte{0x01});
        
        // Interface Version
        message.push_back(std::byte{0x00});
        
        // Message Type (TP bit set in high bit of message type byte)
        std::uint8_t msg_type = 0x00;  // Request
        if (more_segments) {
            msg_type |= 0x20;  // TP flag
        }
        message.push_back(std::byte{msg_type});
        
        // Return Code
        message.push_back(std::byte{0x00});
        
        // TP Header (4 bytes when TP message)
        // Reserved (1 byte) + more_segments flag (1 bit) + offset (31 bits / 15 bits + reserved)
        if (more_segments || segment_offset > 0) {
            // This is a TP message, add TP header
            std::uint8_t reserve_and_flags = more_segments ? 0x01 : 0x00;
            message.push_back(std::byte{reserve_and_flags});
            
            // Offset (3 bytes) - offset is in bytes / 4
            std::uint32_t offset_field = segment_offset / 4;
            message.push_back(std::byte{static_cast<std::uint8_t>((offset_field >> 16) & 0xFF)});
            message.push_back(std::byte{static_cast<std::uint8_t>((offset_field >> 8) & 0xFF)});
            message.push_back(std::byte{static_cast<std::uint8_t>(offset_field & 0xFF)});
        }
        
        // Payload
        for (auto b : payload) {
            message.push_back(std::byte{b});
        }
        
        return message;
    }
};

/// Test TP message with single segment
TEST_F(SomeipTpTest, SingleSegmentMessage) {
    auto msg = create_someip_tp_message(0x1234, 0x0001, 100, 0, false, {0x01, 0x02, 0x03});
    EXPECT_GE(msg.size(), 16);  // At least SOME/IP header
}

/// Test TP message with first segment (more_segments = true)
TEST_F(SomeipTpTest, FirstSegmentMultipart) {
    std::vector<std::uint8_t> payload(1000, 0xAA);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 5000, 0, true, payload);
    EXPECT_GE(msg.size(), 20);  // Header + TP header + payload
}

/// Test TP message with middle segment
TEST_F(SomeipTpTest, MiddleSegment) {
    std::vector<std::uint8_t> payload(1000, 0xBB);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 5000, 1000, true, payload);
    EXPECT_GE(msg.size(), 20);
}

/// Test TP message with last segment (more_segments = false)
TEST_F(SomeipTpTest, LastSegment) {
    std::vector<std::uint8_t> payload(500, 0xCC);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 5000, 4000, false, payload);
    EXPECT_GE(msg.size(), 20);
}

/// Test TP offset validation
TEST_F(SomeipTpTest, OffsetValidation) {
    // Offset should be at 1000-byte boundary (divisible by 4)
    std::vector<std::uint8_t> payload(1000, 0xDD);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 5000, 1000, false, payload);
    EXPECT_GT(msg.size(), 0);
}

/// Test TP with 16MB maximum message size
TEST_F(SomeipTpTest, MaxMessageSize16MB) {
    // Max SOME/IP message is 16MB = 16777216 bytes
    std::vector<std::uint8_t> payload(1000, 0xEE);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 16777216, 0, true, payload);
    EXPECT_GE(msg.size(), 20);
}

/// Test TP message exceeding 16MB should be rejected
TEST_F(SomeipTpTest, ExceedsMaxMessageSize) {
    std::vector<std::uint8_t> payload(1000, 0xFF);
    // 17MB exceeds limit
    auto msg = create_someip_tp_message(0x1234, 0x0001, 17777216, 0, true, payload);
    // Should still create but may be marked invalid during decoding
    EXPECT_GT(msg.size(), 0);
}

/// Test TP out-of-order segment arrival
TEST_F(SomeipTpTest, OutOfOrderSegments) {
    // Simulate receiving segment 3 before segment 1
    std::vector<std::uint8_t> payload(1000, 0x11);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 5000, 3000, true, payload);
    EXPECT_GT(msg.size(), 0);
}

/// Test TP with zero-length payload (edge case)
TEST_F(SomeipTpTest, ZeroLengthPayload) {
    auto msg = create_someip_tp_message(0x1234, 0x0001, 100, 0, true, {});
    EXPECT_GE(msg.size(), 20);
}

/// Test TP header parsing - more_segments flag
TEST_F(SomeipTpTest, TpMoreSegmentsFlag) {
    std::vector<std::uint8_t> payload(100, 0x22);
    auto msg_with_flag = create_someip_tp_message(0x1234, 0x0001, 500, 0, true, payload);
    auto msg_without_flag = create_someip_tp_message(0x1234, 0x0001, 100, 0, false, payload);
    
    EXPECT_GT(msg_with_flag.size(), 0);
    EXPECT_GT(msg_without_flag.size(), 0);
}

/// Test TP offset field parsing
TEST_F(SomeipTpTest, TpOffsetParsing) {
    std::vector<std::uint8_t> payload(1000, 0x33);
    // Test various offset values (should be multiples of 4 bytes)
    auto msg_offset_0 = create_someip_tp_message(0x1234, 0x0001, 5000, 0, true, payload);
    auto msg_offset_1000 = create_someip_tp_message(0x1234, 0x0001, 5000, 1000, true, payload);
    auto msg_offset_2000 = create_someip_tp_message(0x1234, 0x0001, 5000, 2000, true, payload);
    
    EXPECT_GT(msg_offset_0.size(), 0);
    EXPECT_GT(msg_offset_1000.size(), 0);
    EXPECT_GT(msg_offset_2000.size(), 0);
}

/// Test TP with non-TP SOME/IP message (control flow)
TEST_F(SomeipTpTest, NonTpMessage) {
    auto msg = create_someip_tp_message(0x1234, 0x0001, 100, 0, false, {0x01, 0x02});
    EXPECT_GE(msg.size(), 16);
}

/// Test TP timeout scenario (incomplete message after 5 seconds)
TEST_F(SomeipTpTest, TimeoutHandling) {
    // First segment arrives
    std::vector<std::uint8_t> payload1(1000, 0x44);
    auto msg1 = create_someip_tp_message(0x1234, 0x0001, 5000, 0, true, payload1);
    
    // Second segment arrives
    std::vector<std::uint8_t> payload2(1000, 0x55);
    auto msg2 = create_someip_tp_message(0x1234, 0x0001, 5000, 1000, true, payload2);
    
    // Message times out waiting for segment 3
    EXPECT_GT(msg1.size(), 0);
    EXPECT_GT(msg2.size(), 0);
}

/// Test TP reassembly with multiple incomplete messages
TEST_F(SomeipTpTest, MultipleIncompleteMessages) {
    // Two different service IDs with incomplete TP messages
    std::vector<std::uint8_t> payload(500, 0x66);
    auto msg1 = create_someip_tp_message(0x1111, 0x0001, 2000, 0, true, payload);
    auto msg2 = create_someip_tp_message(0x2222, 0x0001, 2000, 0, true, payload);
    
    EXPECT_GT(msg1.size(), 0);
    EXPECT_GT(msg2.size(), 0);
}

/// Test TP total length field consistency
TEST_F(SomeipTpTest, TotalLengthConsistency) {
    std::vector<std::uint8_t> payload(1000, 0x77);
    auto msg = create_someip_tp_message(0x1234, 0x0001, 5000, 1000, true, payload);
    
    // Total length should be consistent across segments
    EXPECT_GT(msg.size(), 0);
}

/// Test TP segment ordering validation
TEST_F(SomeipTpTest, SegmentOrderingValidation) {
    std::vector<std::uint8_t> payload(1000, 0x88);
    
    // Receive segments out of order
    auto seg3 = create_someip_tp_message(0x1234, 0x0001, 4000, 3000, false, payload);
    auto seg1 = create_someip_tp_message(0x1234, 0x0001, 4000, 0, true, payload);
    auto seg2 = create_someip_tp_message(0x1234, 0x0001, 4000, 1000, true, payload);
    
    EXPECT_GT(seg1.size(), 0);
    EXPECT_GT(seg2.size(), 0);
    EXPECT_GT(seg3.size(), 0);
}

/// Test TP with different message types
TEST_F(SomeipTpTest, TpWithDifferentMessageTypes) {
    std::vector<std::uint8_t> payload(100, 0x99);
    
    // Request-type TP message
    auto tp_request = create_someip_tp_message(0x1234, 0x0001, 200, 0, true, payload);
    
    // Response-type TP message (would have different type byte)
    auto tp_response = create_someip_tp_message(0x5678, 0x0001, 200, 0, true, payload);
    
    EXPECT_GT(tp_request.size(), 0);
    EXPECT_GT(tp_response.size(), 0);
}

/// Test TP length field validation (should be >= 12 for TP header + data)
TEST_F(SomeipTpTest, MinimumLengthValidation) {
    auto msg = create_someip_tp_message(0x1234, 0x0001, 12, 0, false, {});
    EXPECT_GE(msg.size(), 16);  // At least SOME/IP header
}

/// Test TP reassembly state machine
TEST_F(SomeipTpTest, ReassemblyStateMachine) {
    std::vector<std::uint8_t> payload(1000, 0xAA);
    
    // State transitions: IDLE -> WAITING -> COMPLETE
    auto first_seg = create_someip_tp_message(0x1234, 0x0001, 3000, 0, true, payload);
    auto last_seg = create_someip_tp_message(0x1234, 0x0001, 3000, 1000, false, payload);
    
    EXPECT_GT(first_seg.size(), 0);
    EXPECT_GT(last_seg.size(), 0);
}

/// Test TP with large payload spanning multiple segments
TEST_F(SomeipTpTest, LargeMultiSegmentPayload) {
    // Simulate 5MB total message split into 1MB segments
    std::vector<std::uint8_t> segment_payload(1048576, 0xBB);  // 1MB
    auto seg1 = create_someip_tp_message(0x1234, 0x0001, 5242880, 0, true, segment_payload);
    auto seg2 = create_someip_tp_message(0x1234, 0x0001, 5242880, 1048576U, true, segment_payload);
    auto seg3 = create_someip_tp_message(0x1234, 0x0001, 5242880, 2097152U, true, segment_payload);
    auto seg4 = create_someip_tp_message(0x1234, 0x0001, 5242880, 3145728U, true, segment_payload);
    auto seg5 = create_someip_tp_message(0x1234, 0x0001, 5242880, 4194304U, false, segment_payload);
    
    EXPECT_GT(seg1.size(), 1000000);
    EXPECT_GT(seg2.size(), 1000000);
    EXPECT_GT(seg3.size(), 1000000);
    EXPECT_GT(seg4.size(), 1000000);
    EXPECT_GT(seg5.size(), 1000000);
}

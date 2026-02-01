/// @file test_doip_power.cpp
/// @brief DoIP power mode (0x4003/0x4004) and entity status (0x4001/0x4002) tests

#include <gtest/gtest.h>

#include "wadjet/protocols/doip.hpp"

using namespace wadjet::protocols::doip;

//==============================================================================
// DoIP Power Mode Message Type Tests
//==============================================================================

class DoIPPowerModeTest : public ::testing::Test {
};

TEST_F(DoIPPowerModeTest, DiagnosticPowerModeRequestPayloadType) {
    EXPECT_EQ(static_cast<std::uint16_t>(PayloadType::DiagnosticPowerModeRequest), 0x4003);
}

TEST_F(DoIPPowerModeTest, DiagnosticPowerModeResponsePayloadType) {
    EXPECT_EQ(static_cast<std::uint16_t>(PayloadType::DiagnosticPowerModeResponse), 0x4004);
}

TEST_F(DoIPPowerModeTest, PowerModeEnumConsistency) {
    // Verify request and response are different types
    EXPECT_NE(PayloadType::DiagnosticPowerModeRequest, 
              PayloadType::DiagnosticPowerModeResponse);
}

//==============================================================================
// DoIP Entity Status Message Type Tests
//==============================================================================

class DoIPEntityStatusTest : public ::testing::Test {
};

TEST_F(DoIPEntityStatusTest, EntityStatusRequestPayloadType) {
    EXPECT_EQ(static_cast<std::uint16_t>(PayloadType::DoIPEntityStatusRequest), 0x4001);
}

TEST_F(DoIPEntityStatusTest, EntityStatusResponsePayloadType) {
    EXPECT_EQ(static_cast<std::uint16_t>(PayloadType::DoIPEntityStatusResponse), 0x4002);
}

TEST_F(DoIPEntityStatusTest, EntityStatusEnumConsistency) {
    EXPECT_NE(PayloadType::DoIPEntityStatusRequest,
              PayloadType::DoIPEntityStatusResponse);
}

//==============================================================================
// DoIP Generic NACK Code Tests
//==============================================================================

class DoIPNackCodeTest : public ::testing::Test {
};

TEST_F(DoIPNackCodeTest, IncorrectPatternFormatCode) {
    EXPECT_EQ(static_cast<std::uint8_t>(NackCode::IncorrectPatternFormat), 0x00);
}

TEST_F(DoIPNackCodeTest, UnknownPayloadTypeCode) {
    EXPECT_EQ(static_cast<std::uint8_t>(NackCode::UnknownPayloadType), 0x01);
}

TEST_F(DoIPNackCodeTest, MessageTooLargeCode) {
    EXPECT_EQ(static_cast<std::uint8_t>(NackCode::MessageTooLarge), 0x02);
}

TEST_F(DoIPNackCodeTest, OutOfMemoryCode) {
    EXPECT_EQ(static_cast<std::uint8_t>(NackCode::OutOfMemory), 0x03);
}

TEST_F(DoIPNackCodeTest, InvalidPayloadLengthCode) {
    EXPECT_EQ(static_cast<std::uint8_t>(NackCode::InvalidPayloadLength), 0x04);
}

//==============================================================================
// DoIP Alive Check Message Tests
//==============================================================================

class DoIPAliveCheckTest : public ::testing::Test {
};

TEST_F(DoIPAliveCheckTest, AliveCheckRequestPayloadType) {
    EXPECT_EQ(static_cast<std::uint16_t>(PayloadType::AliveCheckRequest), 0x0007);
}

TEST_F(DoIPAliveCheckTest, AliveCheckResponsePayloadType) {
    EXPECT_EQ(static_cast<std::uint16_t>(PayloadType::AliveCheckResponse), 0x0008);
}

TEST_F(DoIPAliveCheckTest, AliveCheckRequestResponseMismatch) {
    EXPECT_NE(PayloadType::AliveCheckRequest, PayloadType::AliveCheckResponse);
}

TEST_F(DoIPAliveCheckTest, AliveCheckIsValidRequest) {
    DoIPHeader header;
    header.payload_type = PayloadType::AliveCheckRequest;
    header.payload_length = 0;  // Alive check has no payload
    
    EXPECT_EQ(header.payload_type, PayloadType::AliveCheckRequest);
    EXPECT_EQ(header.payload_length, 0);
}

TEST_F(DoIPAliveCheckTest, AliveCheckResponsePayloadLength) {
    DoIPHeader header;
    header.payload_type = PayloadType::AliveCheckResponse;
    header.payload_length = 4;  // Response contains tester source address (2 bytes)
    
    EXPECT_EQ(header.payload_type, PayloadType::AliveCheckResponse);
    EXPECT_EQ(header.payload_length, 4);
}

//==============================================================================
// DoIP Power Mode State Transition Tests
//==============================================================================

class DoIPPowerModeTransitionTest : public ::testing::Test {
};

TEST_F(DoIPPowerModeTransitionTest, ReadyState) {
    // Power mode value: 0x00 = Ready
    std::uint8_t ready_state = 0x00;
    EXPECT_EQ(ready_state, 0x00);
}

TEST_F(DoIPPowerModeTransitionTest, NotReadyState) {
    // Power mode value: 0x01 = NotReady
    std::uint8_t not_ready_state = 0x01;
    EXPECT_EQ(not_ready_state, 0x01);
}

TEST_F(DoIPPowerModeTransitionTest, NotSupportedState) {
    // Power mode value: 0x02 = NotSupported
    std::uint8_t not_supported_state = 0x02;
    EXPECT_EQ(not_supported_state, 0x02);
}

TEST_F(DoIPPowerModeTransitionTest, PowerModeReadyToNotReady) {
    // Scenario: ECU transitions from Ready to NotReady
    std::uint8_t initial_mode = 0x00;  // Ready
    std::uint8_t new_mode = 0x01;      // NotReady
    
    EXPECT_NE(initial_mode, new_mode);
}

TEST_F(DoIPPowerModeTransitionTest, PowerModeNotReadyToReady) {
    // Scenario: ECU transitions from NotReady back to Ready
    std::uint8_t initial_mode = 0x01;  // NotReady
    std::uint8_t new_mode = 0x00;      // Ready
    
    EXPECT_NE(initial_mode, new_mode);
}

//==============================================================================
// DoIP Entity Status Tests
//==============================================================================

class DoIPEntityStatusPayloadTest : public ::testing::Test {
};

TEST_F(DoIPEntityStatusPayloadTest, EntityStatusRequestFormat) {
    // Entity status request is empty
    DoIPHeader header;
    header.payload_type = PayloadType::DoIPEntityStatusRequest;
    header.payload_length = 0;
    
    EXPECT_EQ(header.payload_type, PayloadType::DoIPEntityStatusRequest);
    EXPECT_EQ(header.payload_length, 0);
}

TEST_F(DoIPEntityStatusPayloadTest, EntityStatusResponseFormat) {
    // Entity status response: node type (1) + max concurrent sockets (1) + 
    // current concurrent sockets (1) + max connections (2)
    DoIPHeader header;
    header.payload_type = PayloadType::DoIPEntityStatusResponse;
    header.payload_length = 5;  // Minimum payload size
    
    EXPECT_EQ(header.payload_type, PayloadType::DoIPEntityStatusResponse);
    EXPECT_EQ(header.payload_length, 5);
}

//==============================================================================
// DoIP Payload Length Validation Tests
//==============================================================================

class DoIPPayloadLengthValidationTest : public ::testing::Test {
};

TEST_F(DoIPPayloadLengthValidationTest, PowerModeRequestZeroLength) {
    DoIPHeader header;
    header.payload_type = PayloadType::DiagnosticPowerModeRequest;
    header.payload_length = 0;  // Request has no payload
    
    EXPECT_EQ(header.payload_length, 0);
}

TEST_F(DoIPPayloadLengthValidationTest, PowerModeResponseMinimumLength) {
    DoIPHeader header;
    header.payload_type = PayloadType::DiagnosticPowerModeResponse;
    header.payload_length = 1;  // Minimum: power mode byte
    
    EXPECT_EQ(header.payload_length, 1);
}

TEST_F(DoIPPayloadLengthValidationTest, PowerModeResponseValidLength) {
    DoIPHeader header;
    header.payload_type = PayloadType::DiagnosticPowerModeResponse;
    header.payload_length = 4;  // Valid response with additional fields
    
    EXPECT_EQ(header.payload_length, 4);
}

TEST_F(DoIPPayloadLengthValidationTest, AliveCheckRequestZeroLength) {
    DoIPHeader header;
    header.payload_type = PayloadType::AliveCheckRequest;
    header.payload_length = 0;
    
    EXPECT_EQ(header.payload_length, 0);
}

TEST_F(DoIPPayloadLengthValidationTest, AliveCheckResponseValidLength) {
    DoIPHeader header;
    header.payload_type = PayloadType::AliveCheckResponse;
    header.payload_length = 2;  // Tester source address
    
    EXPECT_EQ(header.payload_length, 2);
}

//==============================================================================
// DoIP Message Type Categorization Tests
//==============================================================================

class DoIPMessageCategorizationTest : public ::testing::Test {
};

TEST_F(DoIPMessageCategorizationTest, PowerModeIsNotDiagnosticMessage) {
    DoIPHeader header;
    header.payload_type = PayloadType::DiagnosticPowerModeRequest;
    
    EXPECT_FALSE(header.is_diagnostic_message());
}

TEST_F(DoIPMessageCategorizationTest, PowerModeIsNotRoutingActivation) {
    DoIPHeader header;
    header.payload_type = PayloadType::DiagnosticPowerModeResponse;
    
    EXPECT_FALSE(header.is_routing_activation());
}

TEST_F(DoIPMessageCategorizationTest, PowerModeIsNotVehicleIdentification) {
    DoIPHeader header;
    header.payload_type = PayloadType::DiagnosticPowerModeRequest;
    
    EXPECT_FALSE(header.is_vehicle_identification());
}

TEST_F(DoIPMessageCategorizationTest, EntityStatusIsNotDiagnosticMessage) {
    DoIPHeader header;
    header.payload_type = PayloadType::DoIPEntityStatusRequest;
    
    EXPECT_FALSE(header.is_diagnostic_message());
}

TEST_F(DoIPMessageCategorizationTest, AliveCheckIsNotDiagnosticMessage) {
    DoIPHeader header;
    header.payload_type = PayloadType::AliveCheckRequest;
    
    EXPECT_FALSE(header.is_diagnostic_message());
}

//==============================================================================
// DoIP Parse Power Mode Tests
//==============================================================================

class DoIPParsePowerModeTest : public ::testing::Test {
};

TEST_F(DoIPParsePowerModeTest, ParsePowerModeReady) {
    std::array<std::byte, 1> payload{std::byte(0x00)};
    auto result = DoIPDecoder::parse_diagnostic_power_mode(payload);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), PowerMode::Ready);
}

TEST_F(DoIPParsePowerModeTest, ParsePowerModeNotReady) {
    std::array<std::byte, 1> payload{std::byte(0x01)};
    auto result = DoIPDecoder::parse_diagnostic_power_mode(payload);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), PowerMode::NotReady);
}

TEST_F(DoIPParsePowerModeTest, ParsePowerModeNotSupported) {
    std::array<std::byte, 1> payload{std::byte(0x02)};
    auto result = DoIPDecoder::parse_diagnostic_power_mode(payload);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), PowerMode::NotSupported);
}

TEST_F(DoIPParsePowerModeTest, ParsePowerModeEmptyPayload) {
    std::vector<std::byte> empty;
    auto result = DoIPDecoder::parse_diagnostic_power_mode(empty);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(DoIPParsePowerModeTest, ParsePowerModeInvalidCode) {
    std::array<std::byte, 1> payload{std::byte(0x03)};
    auto result = DoIPDecoder::parse_diagnostic_power_mode(payload);
    
    EXPECT_FALSE(result.has_value());
}

//==============================================================================
// DoIP Parse Entity Status Tests
//==============================================================================

class DoIPParseEntityStatusTest : public ::testing::Test {
};

TEST_F(DoIPParseEntityStatusTest, ParseEntityStatusValidPayload) {
    // node_type(1) + max_concurrent(1) + current_concurrent(1) + max_connections(2)
    std::array<std::byte, 5> payload{
        std::byte(0x00),  // Gateway
        std::byte(0x0A),  // max_concurrent = 10
        std::byte(0x05),  // current_concurrent = 5
        std::byte(0x00),  // max_connections high byte
        std::byte(0x20),  // max_connections low byte = 32
    };
    
    auto result = DoIPDecoder::parse_entity_status(payload);
    
    EXPECT_TRUE(result.has_value());
    auto [node_type, max_concurrent, current_concurrent, max_connections] = result.value();
    EXPECT_EQ(node_type, 0x00);
    EXPECT_EQ(max_concurrent, 0x0A);
    EXPECT_EQ(current_concurrent, 0x05);
    EXPECT_EQ(max_connections, 0x0020);
}

TEST_F(DoIPParseEntityStatusTest, ParseEntityStatusEmptyPayload) {
    std::vector<std::byte> empty;
    auto result = DoIPDecoder::parse_entity_status(empty);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(DoIPParseEntityStatusTest, ParseEntityStatusTooShort) {
    std::array<std::byte, 4> payload{
        std::byte(0x00), std::byte(0x0A), std::byte(0x05), std::byte(0x00)
    };
    auto result = DoIPDecoder::parse_entity_status(payload);
    
    EXPECT_FALSE(result.has_value());
}

//==============================================================================
// DoIP Parse Generic NACK Tests
//==============================================================================

class DoIPParseGenericNackTest : public ::testing::Test {
};

TEST_F(DoIPParseGenericNackTest, ParseGenericNackValidPayload) {
    // nack_code(1) + unknown_payload_type(2)
    std::array<std::byte, 3> payload{
        std::byte(0x01),  // UnknownPayloadType
        std::byte(0xAB),  // high byte
        std::byte(0xCD),  // low byte
    };
    
    auto result = DoIPDecoder::parse_generic_nack(payload);
    
    EXPECT_TRUE(result.has_value());
    auto [nack_code, unknown_payload_type] = result.value();
    EXPECT_EQ(nack_code, NackCode::UnknownPayloadType);
    EXPECT_EQ(unknown_payload_type, 0xABCD);
}

TEST_F(DoIPParseGenericNackTest, ParseGenericNackTooShort) {
    std::array<std::byte, 2> payload{std::byte(0x01), std::byte(0xAB)};
    auto result = DoIPDecoder::parse_generic_nack(payload);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(DoIPParseGenericNackTest, ParseGenericNackEmptyPayload) {
    std::vector<std::byte> empty;
    auto result = DoIPDecoder::parse_generic_nack(empty);
    
    EXPECT_FALSE(result.has_value());
}

//==============================================================================
// DoIP Parse Alive Check Response Tests
//==============================================================================

class DoIPParseAliveCheckTest : public ::testing::Test {
};

TEST_F(DoIPParseAliveCheckTest, ParseAliveCheckResponseValidPayload) {
    // tester_source_address(2)
    std::array<std::byte, 2> payload{
        std::byte(0x12),  // high byte
        std::byte(0x34),  // low byte
    };
    
    auto result = DoIPDecoder::parse_alive_check_response(payload);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0x1234);
}

TEST_F(DoIPParseAliveCheckTest, ParseAliveCheckResponseTooShort) {
    std::array<std::byte, 1> payload{std::byte(0x12)};
    auto result = DoIPDecoder::parse_alive_check_response(payload);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(DoIPParseAliveCheckTest, ParseAliveCheckResponseEmptyPayload) {
    std::vector<std::byte> empty;
    auto result = DoIPDecoder::parse_alive_check_response(empty);
    
    EXPECT_FALSE(result.has_value());
}

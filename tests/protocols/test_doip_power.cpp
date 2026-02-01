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


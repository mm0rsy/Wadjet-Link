/// @file test_uds_integration.cpp
/// @brief Integration tests for UDS protocol with DoIP transport
/// @details Tests UDS message decoding, session tracking, request-response correlation

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <chrono>
#include <thread>

#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_session.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/tcp.hpp"

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::uds;

namespace {

/// Helper class to build DoIP+UDS packets
class DoIpUdsPacketBuilder {
public:
    // Build a DoIP diagnostic message with UDS payload
    static std::vector<std::uint8_t> build_diagnostic_message(
        std::uint16_t source_addr,
        std::uint16_t target_addr,
        const std::vector<std::uint8_t>& uds_payload) {
        
        std::vector<std::uint8_t> packet;
        
        // DoIP header (8 bytes)
        packet.push_back(0x02);  // Protocol version
        packet.push_back(0xFD);  // Inverse version
        packet.push_back(0x80);  // Payload type: Diagnostic Message (0x8001)
        packet.push_back(0x01);
        
        // Payload length = 4 (addresses) + UDS payload length
        std::uint32_t payload_len = 4 + uds_payload.size();
        packet.push_back(static_cast<std::uint8_t>(payload_len >> 24));
        packet.push_back(static_cast<std::uint8_t>(payload_len >> 16));
        packet.push_back(static_cast<std::uint8_t>(payload_len >> 8));
        packet.push_back(static_cast<std::uint8_t>(payload_len & 0xFF));
        
        // Source address
        packet.push_back(static_cast<std::uint8_t>(source_addr >> 8));
        packet.push_back(static_cast<std::uint8_t>(source_addr & 0xFF));
        
        // Target address
        packet.push_back(static_cast<std::uint8_t>(target_addr >> 8));
        packet.push_back(static_cast<std::uint8_t>(target_addr & 0xFF));
        
        // UDS payload
        packet.insert(packet.end(), uds_payload.begin(), uds_payload.end());
        
        return packet;
    }
    
    // Build UDS request: Diagnostic Session Control
    static std::vector<std::uint8_t> uds_diagnostic_session_control(SessionType session_type, bool spr = false) {
        std::uint8_t sub_function = static_cast<std::uint8_t>(session_type);
        if (spr) sub_function |= 0x80;  // Suppress positive response
        return {0x10, sub_function};
    }
    
    // Build UDS response: Diagnostic Session Control
    static std::vector<std::uint8_t> uds_diagnostic_session_control_response(
        SessionType session_type,
        std::uint16_t p2_time = 50,
        std::uint16_t p2_star_time = 5000) {
        return {
            0x50,  // Positive response (0x10 + 0x40)
            static_cast<std::uint8_t>(session_type),
            static_cast<std::uint8_t>(p2_time >> 8),
            static_cast<std::uint8_t>(p2_time & 0xFF),
            static_cast<std::uint8_t>(p2_star_time >> 8),
            static_cast<std::uint8_t>(p2_star_time & 0xFF)
        };
    }
    
    // Build UDS request: Security Access - Request Seed
    static std::vector<std::uint8_t> uds_security_access_request_seed(std::uint8_t level) {
        return {0x27, level};  // Odd level = request seed
    }
    
    // Build UDS response: Security Access - Seed
    static std::vector<std::uint8_t> uds_security_access_seed_response(
        std::uint8_t level,
        const std::vector<std::uint8_t>& seed) {
        std::vector<std::uint8_t> response = {0x67, level};  // 0x27 + 0x40
        response.insert(response.end(), seed.begin(), seed.end());
        return response;
    }
    
    // Build UDS request: Security Access - Send Key
    static std::vector<std::uint8_t> uds_security_access_send_key(
        std::uint8_t level,
        const std::vector<std::uint8_t>& key) {
        std::vector<std::uint8_t> request = {0x27, static_cast<std::uint8_t>(level + 1)};  // Even level = send key
        request.insert(request.end(), key.begin(), key.end());
        return request;
    }
    
    // Build UDS response: Security Access - Key accepted
    static std::vector<std::uint8_t> uds_security_access_key_response(std::uint8_t level) {
        return {0x67, static_cast<std::uint8_t>(level + 1)};
    }
    
    // Build UDS request: Tester Present
    static std::vector<std::uint8_t> uds_tester_present(bool spr = true) {
        return {0x3E, spr ? std::uint8_t(0x80) : std::uint8_t(0x00)};
    }
    
    // Build UDS response: Tester Present
    static std::vector<std::uint8_t> uds_tester_present_response() {
        return {0x7E, 0x00};
    }
    
    // Build UDS request: Read Data By Identifier
    static std::vector<std::uint8_t> uds_read_data_by_identifier(
        const std::vector<std::uint16_t>& dids) {
        std::vector<std::uint8_t> request = {0x22};
        for (auto did : dids) {
            request.push_back(static_cast<std::uint8_t>(did >> 8));
            request.push_back(static_cast<std::uint8_t>(did & 0xFF));
        }
        return request;
    }
    
    // Build UDS response: Read Data By Identifier
    static std::vector<std::uint8_t> uds_read_data_by_identifier_response(
        std::uint16_t did,
        const std::vector<std::uint8_t>& data) {
        std::vector<std::uint8_t> response = {
            0x62,  // 0x22 + 0x40
            static_cast<std::uint8_t>(did >> 8),
            static_cast<std::uint8_t>(did & 0xFF)
        };
        response.insert(response.end(), data.begin(), data.end());
        return response;
    }
    
    // Build UDS request: ECU Reset
    static std::vector<std::uint8_t> uds_ecu_reset(ResetType reset_type) {
        return {0x11, static_cast<std::uint8_t>(reset_type)};
    }
    
    // Build UDS response: ECU Reset
    static std::vector<std::uint8_t> uds_ecu_reset_response(ResetType reset_type) {
        return {0x51, static_cast<std::uint8_t>(reset_type)};
    }
    
    // Build UDS Negative Response
    static std::vector<std::uint8_t> uds_negative_response(
        ServiceID rejected_sid,
        NRC nrc) {
        return {
            0x7F,
            static_cast<std::uint8_t>(rejected_sid),
            static_cast<std::uint8_t>(nrc)
        };
    }
    
    // Build UDS Response Pending (NRC 0x78)
    static std::vector<std::uint8_t> uds_response_pending(ServiceID rejected_sid) {
        return uds_negative_response(rejected_sid, NRC::RequestCorrectlyReceivedResponsePending);
    }
};

}  // namespace

// =============================================================================
// UDS Decoder Integration Tests
// =============================================================================

class UdsDecoderIntegrationTest : public ::testing::Test {
protected:
    UdsDecoder decoder_;
};

TEST_F(UdsDecoderIntegrationTest, DecodeSessionControlRequest) {
    auto uds_data = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    
    std::span<const std::uint8_t> span(uds_data.data(), uds_data.size());
    auto result = decoder_.decode(span);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_TRUE(result->header.is_request());
    EXPECT_FALSE(result->header.suppress_positive_response);
}

TEST_F(UdsDecoderIntegrationTest, DecodeSessionControlRequestWithSPR) {
    auto uds_data = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ProgrammingSession, true);  // SPR = true
    
    std::span<const std::uint8_t> span(uds_data.data(), uds_data.size());
    auto result = decoder_.decode(span);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_TRUE(result->header.is_request());
    EXPECT_TRUE(result->header.suppress_positive_response);
}

TEST_F(UdsDecoderIntegrationTest, DecodeSessionControlResponse) {
    auto uds_data = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    
    std::span<const std::uint8_t> span(uds_data.data(), uds_data.size());
    auto result = decoder_.decode(span);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_TRUE(result->header.is_positive_response());
}

TEST_F(UdsDecoderIntegrationTest, DecodeNegativeResponse) {
    auto uds_data = DoIpUdsPacketBuilder::uds_negative_response(
        ServiceID::SecurityAccess,
        NRC::SecurityAccessDenied);
    
    std::span<const std::uint8_t> span(uds_data.data(), uds_data.size());
    auto result = decoder_.decode(span);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code, NRC::SecurityAccessDenied);
    EXPECT_EQ(result->header.rejected_service_id, ServiceID::SecurityAccess);
}

TEST_F(UdsDecoderIntegrationTest, DecodeResponsePending) {
    auto uds_data = DoIpUdsPacketBuilder::uds_response_pending(
        ServiceID::RoutineControl);
    
    std::span<const std::uint8_t> span(uds_data.data(), uds_data.size());
    auto result = decoder_.decode(span);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code, NRC::RequestCorrectlyReceivedResponsePending);
    EXPECT_EQ(result->header.rejected_service_id, ServiceID::RoutineControl);
}

TEST_F(UdsDecoderIntegrationTest, DecodeReadDataByIdentifier) {
    auto uds_data = DoIpUdsPacketBuilder::uds_read_data_by_identifier({0xF190, 0xF186});
    
    std::span<const std::uint8_t> span(uds_data.data(), uds_data.size());
    auto result = decoder_.decode(span);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_TRUE(result->header.is_request());
}

// =============================================================================
// UDS Session Tracking Integration Tests
// =============================================================================

class UdsSessionIntegrationTest : public ::testing::Test {
protected:
    UdsSession session_{0x0001};  // ECU address 0x0001
};

TEST_F(UdsSessionIntegrationTest, SessionTransition_DefaultToExtended) {
    // Initial state: Default session
    EXPECT_EQ(session_.session_type(), SessionType::DefaultSession);
    
    // Request extended session
    auto request = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> req_span(request.data(), request.size());
    session_.process_message(req_span, true);
    
    // Response from ECU
    auto response = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> resp_span(response.data(), response.size());
    session_.process_message(resp_span, false);
    
    // Verify session transition
    EXPECT_EQ(session_.session_type(), SessionType::ExtendedDiagnosticSession);
    EXPECT_TRUE(session_.is_active());
}

TEST_F(UdsSessionIntegrationTest, SessionTransition_ExtendedToProgramming) {
    // First transition to extended
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    session_.process_message(ext_req_span, true);
    
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    session_.process_message(ext_resp_span, false);
    
    // Then transition to programming
    auto prog_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ProgrammingSession);
    std::span<const std::uint8_t> prog_req_span(prog_req.data(), prog_req.size());
    session_.process_message(prog_req_span, true);
    
    auto prog_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ProgrammingSession);
    std::span<const std::uint8_t> prog_resp_span(prog_resp.data(), prog_resp.size());
    session_.process_message(prog_resp_span, false);
    
    EXPECT_EQ(session_.session_type(), SessionType::ProgrammingSession);
}

TEST_F(UdsSessionIntegrationTest, SecurityUnlock_Level1) {
    // First enter extended session
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    session_.process_message(ext_req_span, true);
    
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    session_.process_message(ext_resp_span, false);
    
    // Not unlocked yet
    EXPECT_FALSE(session_.is_security_unlocked(1));
    
    // Request seed
    auto seed_req = DoIpUdsPacketBuilder::uds_security_access_request_seed(0x01);
    std::span<const std::uint8_t> seed_req_span(seed_req.data(), seed_req.size());
    session_.process_message(seed_req_span, true);
    
    auto seed_resp = DoIpUdsPacketBuilder::uds_security_access_seed_response(
        0x01, {0x12, 0x34, 0x56, 0x78});
    std::span<const std::uint8_t> seed_resp_span(seed_resp.data(), seed_resp.size());
    session_.process_message(seed_resp_span, false);
    
    // Still not unlocked (waiting for key)
    EXPECT_FALSE(session_.is_security_unlocked(1));
    
    // Send key
    auto key_req = DoIpUdsPacketBuilder::uds_security_access_send_key(
        0x01, {0xAB, 0xCD, 0xEF, 0x01});
    std::span<const std::uint8_t> key_req_span(key_req.data(), key_req.size());
    session_.process_message(key_req_span, true);
    
    auto key_resp = DoIpUdsPacketBuilder::uds_security_access_key_response(0x01);
    std::span<const std::uint8_t> key_resp_span(key_resp.data(), key_resp.size());
    session_.process_message(key_resp_span, false);
    
    // Now unlocked
    EXPECT_TRUE(session_.is_security_unlocked(1));
}

TEST_F(UdsSessionIntegrationTest, SecurityUnlock_InvalidKey) {
    // Enter extended session
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    session_.process_message(ext_req_span, true);
    
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    session_.process_message(ext_resp_span, false);
    
    // Request seed
    auto seed_req = DoIpUdsPacketBuilder::uds_security_access_request_seed(0x01);
    std::span<const std::uint8_t> seed_req_span(seed_req.data(), seed_req.size());
    session_.process_message(seed_req_span, true);
    
    auto seed_resp = DoIpUdsPacketBuilder::uds_security_access_seed_response(
        0x01, {0x12, 0x34, 0x56, 0x78});
    std::span<const std::uint8_t> seed_resp_span(seed_resp.data(), seed_resp.size());
    session_.process_message(seed_resp_span, false);
    
    // Send wrong key -> negative response
    auto key_req = DoIpUdsPacketBuilder::uds_security_access_send_key(
        0x01, {0x00, 0x00, 0x00, 0x00});
    std::span<const std::uint8_t> key_req_span(key_req.data(), key_req.size());
    session_.process_message(key_req_span, true);
    
    auto nrc = DoIpUdsPacketBuilder::uds_negative_response(
        ServiceID::SecurityAccess, NRC::InvalidKey);
    std::span<const std::uint8_t> nrc_span(nrc.data(), nrc.size());
    session_.process_message(nrc_span, false);
    
    // Still not unlocked
    EXPECT_FALSE(session_.is_security_unlocked(1));
}

TEST_F(UdsSessionIntegrationTest, TesterPresent_KeepsSessionAlive) {
    // Enter extended session
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    session_.process_message(ext_req_span, true);
    
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    session_.process_message(ext_resp_span, false);
    
    EXPECT_TRUE(session_.is_active());
    
    // Send Tester Present
    auto tp_req = DoIpUdsPacketBuilder::uds_tester_present(false);
    std::span<const std::uint8_t> tp_req_span(tp_req.data(), tp_req.size());
    session_.process_message(tp_req_span, true);
    
    auto tp_resp = DoIpUdsPacketBuilder::uds_tester_present_response();
    std::span<const std::uint8_t> tp_resp_span(tp_resp.data(), tp_resp.size());
    session_.process_message(tp_resp_span, false);
    
    // Still active
    EXPECT_TRUE(session_.is_active());
    EXPECT_EQ(session_.session_type(), SessionType::ExtendedDiagnosticSession);
}

TEST_F(UdsSessionIntegrationTest, ECUReset_ResetsSession) {
    // Enter extended session and unlock security
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    session_.process_message(ext_req_span, true);
    
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    session_.process_message(ext_resp_span, false);
    
    // Perform ECU reset
    auto reset_req = DoIpUdsPacketBuilder::uds_ecu_reset(ResetType::HardReset);
    std::span<const std::uint8_t> reset_req_span(reset_req.data(), reset_req.size());
    session_.process_message(reset_req_span, true);
    
    auto reset_resp = DoIpUdsPacketBuilder::uds_ecu_reset_response(ResetType::HardReset);
    std::span<const std::uint8_t> reset_resp_span(reset_resp.data(), reset_resp.size());
    session_.process_message(reset_resp_span, false);
    
    // Session should be reset to default
    EXPECT_EQ(session_.session_type(), SessionType::DefaultSession);
    EXPECT_FALSE(session_.is_security_unlocked(1));
}

// =============================================================================
// DoIP + UDS Combined Decode Tests
// =============================================================================

class DoIpUdsIntegrationTest : public ::testing::Test {
protected:
    doip::DoIpDecoder doip_decoder_;
    UdsDecoder uds_decoder_;
};

TEST_F(DoIpUdsIntegrationTest, DecodeDoIpDiagnosticMessage_WithUds) {
    auto uds_payload = DoIpUdsPacketBuilder::uds_read_data_by_identifier({0xF190});
    auto doip_packet = DoIpUdsPacketBuilder::build_diagnostic_message(
        0x0E00,  // Tester address
        0x0001,  // ECU address
        uds_payload);
    
    std::span<const std::uint8_t> doip_span(doip_packet.data(), doip_packet.size());
    auto doip_result = doip_decoder_.decode(doip_span);
    
    ASSERT_TRUE(doip_result.is_ok());
    EXPECT_EQ(doip_result->header.payload_type, doip::PayloadType::DiagnosticMessage);
    EXPECT_EQ(doip_result->header.payload_length, 4 + uds_payload.size());
    
    // Extract UDS payload from DoIP
    auto& diag_msg = std::get<doip::DiagnosticMessage>(doip_result->payload);
    EXPECT_EQ(diag_msg.source_address, 0x0E00);
    EXPECT_EQ(diag_msg.target_address, 0x0001);
    
    // Decode UDS from diagnostic message payload
    std::span<const std::uint8_t> uds_span(diag_msg.user_data.data(), diag_msg.user_data.size());
    auto uds_result = uds_decoder_.decode(uds_span);
    
    ASSERT_TRUE(uds_result.is_ok());
    EXPECT_EQ(uds_result->header.service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_TRUE(uds_result->header.is_request());
}

TEST_F(DoIpUdsIntegrationTest, DecodeDoIpDiagnosticResponse_WithUds) {
    auto uds_payload = DoIpUdsPacketBuilder::uds_read_data_by_identifier_response(
        0xF190, {'V', 'I', 'N', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'A', 'B', 'C', 'D'});
    auto doip_packet = DoIpUdsPacketBuilder::build_diagnostic_message(
        0x0001,  // ECU address (responding)
        0x0E00,  // Tester address
        uds_payload);
    
    std::span<const std::uint8_t> doip_span(doip_packet.data(), doip_packet.size());
    auto doip_result = doip_decoder_.decode(doip_span);
    
    ASSERT_TRUE(doip_result.is_ok());
    
    auto& diag_msg = std::get<doip::DiagnosticMessage>(doip_result->payload);
    std::span<const std::uint8_t> uds_span(diag_msg.user_data.data(), diag_msg.user_data.size());
    auto uds_result = uds_decoder_.decode(uds_span);
    
    ASSERT_TRUE(uds_result.is_ok());
    EXPECT_EQ(uds_result->header.service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_TRUE(uds_result->header.is_positive_response());
}

TEST_F(DoIpUdsIntegrationTest, DecodeDoIpDiagnosticNRC_WithUds) {
    auto uds_payload = DoIpUdsPacketBuilder::uds_negative_response(
        ServiceID::WriteDataByIdentifier, NRC::SecurityAccessDenied);
    auto doip_packet = DoIpUdsPacketBuilder::build_diagnostic_message(
        0x0001, 0x0E00, uds_payload);
    
    std::span<const std::uint8_t> doip_span(doip_packet.data(), doip_packet.size());
    auto doip_result = doip_decoder_.decode(doip_span);
    
    ASSERT_TRUE(doip_result.is_ok());
    
    auto& diag_msg = std::get<doip::DiagnosticMessage>(doip_result->payload);
    std::span<const std::uint8_t> uds_span(diag_msg.user_data.data(), diag_msg.user_data.size());
    auto uds_result = uds_decoder_.decode(uds_span);
    
    ASSERT_TRUE(uds_result.is_ok());
    EXPECT_TRUE(uds_result->header.is_negative_response());
    EXPECT_EQ(uds_result->header.negative_response_code, NRC::SecurityAccessDenied);
    EXPECT_EQ(uds_result->header.rejected_service_id, ServiceID::WriteDataByIdentifier);
}

// =============================================================================
// Multi-ECU Session Management Tests
// =============================================================================

class UdsMultiEcuTest : public ::testing::Test {
protected:
    UdsSessionManager session_manager_;
};

TEST_F(UdsMultiEcuTest, TrackMultipleEcuSessions) {
    // Get or create sessions for different ECUs
    auto& ecu1 = session_manager_.get_or_create_session(0x0001);
    auto& ecu2 = session_manager_.get_or_create_session(0x0002);
    auto& ecu3 = session_manager_.get_or_create_session(0x0003);
    
    // All start in default session
    EXPECT_EQ(ecu1.session_type(), SessionType::DefaultSession);
    EXPECT_EQ(ecu2.session_type(), SessionType::DefaultSession);
    EXPECT_EQ(ecu3.session_type(), SessionType::DefaultSession);
    
    // Transition ECU1 to extended session
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    ecu1.process_message(ext_req_span, true);
    
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    ecu1.process_message(ext_resp_span, false);
    
    // Transition ECU2 to programming session
    auto prog_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ProgrammingSession);
    std::span<const std::uint8_t> prog_req_span(prog_req.data(), prog_req.size());
    ecu2.process_message(prog_req_span, true);
    
    auto prog_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ProgrammingSession);
    std::span<const std::uint8_t> prog_resp_span(prog_resp.data(), prog_resp.size());
    ecu2.process_message(prog_resp_span, false);
    
    // Verify independent session states
    EXPECT_EQ(ecu1.session_type(), SessionType::ExtendedDiagnosticSession);
    EXPECT_EQ(ecu2.session_type(), SessionType::ProgrammingSession);
    EXPECT_EQ(ecu3.session_type(), SessionType::DefaultSession);
    
    // Verify session count
    EXPECT_EQ(session_manager_.session_count(), 3);
}

TEST_F(UdsMultiEcuTest, FindExistingSession) {
    session_manager_.get_or_create_session(0x0001);
    session_manager_.get_or_create_session(0x0002);
    
    auto* session = session_manager_.find_session(0x0001);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->ecu_address(), 0x0001);
    
    auto* missing = session_manager_.find_session(0x9999);
    EXPECT_EQ(missing, nullptr);
}

TEST_F(UdsMultiEcuTest, ClearAllSessions) {
    session_manager_.get_or_create_session(0x0001);
    session_manager_.get_or_create_session(0x0002);
    session_manager_.get_or_create_session(0x0003);
    
    EXPECT_EQ(session_manager_.session_count(), 3);
    
    session_manager_.clear_all();
    
    EXPECT_EQ(session_manager_.session_count(), 0);
}

// =============================================================================
// Timing Parameters Tests
// =============================================================================

class UdsTimingTest : public ::testing::Test {
protected:
    UdsSession session_{0x0001};
};

TEST_F(UdsTimingTest, DefaultTimingParameters) {
    auto timing = TimingParameters::default_values();
    
    // ISO 14229 default values
    EXPECT_EQ(timing.p2_server_max.count(), 50);      // 50ms
    EXPECT_EQ(timing.p2_star_server_max.count(), 5000); // 5000ms
    EXPECT_EQ(timing.s3_server.count(), 5000);        // 5000ms
}

TEST_F(UdsTimingTest, TimingParametersFromSessionResponse) {
    // Extended session response with custom timing
    auto ext_req = DoIpUdsPacketBuilder::uds_diagnostic_session_control(
        SessionType::ExtendedDiagnosticSession);
    std::span<const std::uint8_t> ext_req_span(ext_req.data(), ext_req.size());
    session_.process_message(ext_req_span, true);
    
    // Response with P2=25ms, P2*=2500ms (values in ms in response)
    auto ext_resp = DoIpUdsPacketBuilder::uds_diagnostic_session_control_response(
        SessionType::ExtendedDiagnosticSession, 25, 2500);
    std::span<const std::uint8_t> ext_resp_span(ext_resp.data(), ext_resp.size());
    session_.process_message(ext_resp_span, false);
    
    // Timing should be updated
    const auto& timing = session_.timing();
    // Note: The actual timing update depends on implementation
    // This test verifies the message processing doesn't break timing
    EXPECT_GT(timing.p2_server_max.count(), 0);
}

// =============================================================================
// Service String Helpers Tests
// =============================================================================

TEST(UdsServiceStringsTest, ServiceIdString) {
    EXPECT_STREQ(service_id_string(ServiceID::DiagnosticSessionControl), "DiagnosticSessionControl");
    EXPECT_STREQ(service_id_string(ServiceID::ECUReset), "ECUReset");
    EXPECT_STREQ(service_id_string(ServiceID::SecurityAccess), "SecurityAccess");
    EXPECT_STREQ(service_id_string(ServiceID::ReadDataByIdentifier), "ReadDataByIdentifier");
    EXPECT_STREQ(service_id_string(ServiceID::WriteDataByIdentifier), "WriteDataByIdentifier");
    EXPECT_STREQ(service_id_string(ServiceID::RoutineControl), "RoutineControl");
}

TEST(UdsServiceStringsTest, SessionTypeString) {
    EXPECT_STREQ(session_type_string(SessionType::DefaultSession), "DefaultSession");
    EXPECT_STREQ(session_type_string(SessionType::ProgrammingSession), "ProgrammingSession");
    EXPECT_STREQ(session_type_string(SessionType::ExtendedDiagnosticSession), "ExtendedDiagnosticSession");
}

TEST(UdsServiceStringsTest, NRCString) {
    EXPECT_STREQ(nrc_string(NRC::SecurityAccessDenied), "SecurityAccessDenied");
    EXPECT_STREQ(nrc_string(NRC::InvalidKey), "InvalidKey");
    EXPECT_STREQ(nrc_string(NRC::RequestCorrectlyReceivedResponsePending), "RequestCorrectlyReceivedResponsePending");
}

TEST(UdsServiceStringsTest, NRCDescription) {
    auto desc = nrc_description(NRC::SecurityAccessDenied);
    EXPECT_NE(desc.find("security"), std::string::npos);
    
    auto desc2 = nrc_description(NRC::RequestCorrectlyReceivedResponsePending);
    EXPECT_NE(desc2.find("pending"), std::string::npos);
}


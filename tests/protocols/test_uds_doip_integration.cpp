/// @file test_diagnostic_integration.cpp
/// @brief Tests for UDS over DoIP diagnostic integration

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "wadjet/protocols/diagnostic.hpp"
#include "wadjet/testing/diagnostic_matchers.hpp"

namespace wadjet::protocols::diagnostic {
namespace {

using namespace ::testing;
using namespace wadjet::testing::diagnostic;
using namespace std::chrono_literals;

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Create a DoIP diagnostic message packet
std::vector<std::byte> create_doip_diagnostic_message(
    LogicalAddress source, LogicalAddress target,
    std::span<const std::byte> uds_data) {
    std::vector<std::byte> packet;
    
    // DoIP header (8 bytes)
    packet.push_back(std::byte{0x02});  // Protocol version
    packet.push_back(std::byte{0xFD});  // Inverse version
    packet.push_back(std::byte{0x80});  // Payload type high (DiagnosticMessage)
    packet.push_back(std::byte{0x01});  // Payload type low
    
    // Payload length (4 + UDS length)
    std::uint32_t payload_len = 4 + static_cast<std::uint32_t>(uds_data.size());
    packet.push_back(static_cast<std::byte>((payload_len >> 24) & 0xFF));
    packet.push_back(static_cast<std::byte>((payload_len >> 16) & 0xFF));
    packet.push_back(static_cast<std::byte>((payload_len >> 8) & 0xFF));
    packet.push_back(static_cast<std::byte>(payload_len & 0xFF));
    
    // Source address (2 bytes, big-endian)
    packet.push_back(static_cast<std::byte>((source >> 8) & 0xFF));
    packet.push_back(static_cast<std::byte>(source & 0xFF));
    
    // Target address (2 bytes, big-endian)
    packet.push_back(static_cast<std::byte>((target >> 8) & 0xFF));
    packet.push_back(static_cast<std::byte>(target & 0xFF));
    
    // UDS data
    for (auto b : uds_data) {
        packet.push_back(b);
    }
    
    return packet;
}

/// @brief Create UDS DiagnosticSessionControl request
std::vector<std::byte> create_dsc_request(uds::SessionType session) {
    return {
        std::byte{0x10},  // SID: DiagnosticSessionControl
        static_cast<std::byte>(session)
    };
}

/// @brief Create UDS DiagnosticSessionControl positive response
std::vector<std::byte> create_dsc_response(uds::SessionType session,
                                            std::uint16_t p2_ms = 50,
                                            std::uint16_t p2_star_10ms = 500) {
    return {
        std::byte{0x50},  // SID + 0x40
        static_cast<std::byte>(session),
        static_cast<std::byte>((p2_ms >> 8) & 0xFF),
        static_cast<std::byte>(p2_ms & 0xFF),
        static_cast<std::byte>((p2_star_10ms >> 8) & 0xFF),
        static_cast<std::byte>(p2_star_10ms & 0xFF)
    };
}

/// @brief Create UDS SecurityAccess seed request
std::vector<std::byte> create_security_access_seed_request(std::uint8_t level) {
    return {
        std::byte{0x27},  // SID: SecurityAccess
        static_cast<std::byte>(level)  // Odd = request seed
    };
}

/// @brief Create UDS SecurityAccess seed response
std::vector<std::byte> create_security_access_seed_response(
    std::uint8_t level, const std::vector<std::byte>& seed) {
    std::vector<std::byte> data;
    data.push_back(std::byte{0x67});  // SID + 0x40
    data.push_back(static_cast<std::byte>(level));
    for (auto b : seed) {
        data.push_back(b);
    }
    return data;
}

/// @brief Create UDS SecurityAccess key request
std::vector<std::byte> create_security_access_key_request(
    std::uint8_t level, const std::vector<std::byte>& key) {
    std::vector<std::byte> data;
    data.push_back(std::byte{0x27});  // SID: SecurityAccess
    data.push_back(static_cast<std::byte>(level + 1));  // Even = send key
    for (auto b : key) {
        data.push_back(b);
    }
    return data;
}

/// @brief Create UDS SecurityAccess key accepted response
std::vector<std::byte> create_security_access_key_response(std::uint8_t level) {
    return {
        std::byte{0x67},  // SID + 0x40
        static_cast<std::byte>(level + 1)
    };
}

/// @brief Create UDS negative response
std::vector<std::byte> create_negative_response(uds::ServiceID rejected_sid,
                                                 uds::NRC nrc) {
    return {
        std::byte{0x7F},
        static_cast<std::byte>(rejected_sid),
        static_cast<std::byte>(nrc)
    };
}

/// @brief Create UDS TesterPresent request
std::vector<std::byte> create_tester_present_request(bool suppress = false) {
    return {
        std::byte{0x3E},  // SID: TesterPresent
        static_cast<std::byte>(suppress ? 0x80 : 0x00)
    };
}

/// @brief Create UDS TesterPresent response
std::vector<std::byte> create_tester_present_response() {
    return {
        std::byte{0x7E},  // SID + 0x40
        std::byte{0x00}
    };
}

// =============================================================================
// Request Correlator Tests
// =============================================================================

class RequestCorrelatorTest : public Test {
protected:
    RequestCorrelator correlator_;
    
    static constexpr LogicalAddress TESTER = 0x0E00;
    static constexpr LogicalAddress ECU = 0x0001;
};

TEST_F(RequestCorrelatorTest, RecordAndMatchSimpleRequest) {
    // Create request
    uds::UdsHeader req_header;
    req_header.service_id = uds::ServiceID::DiagnosticSessionControl;
    req_header.sub_function = 0x01;
    req_header.direction = uds::MessageDirection::Request;
    
    TransportInfo req_transport;
    req_transport.source_address = TESTER;
    req_transport.target_address = ECU;
    req_transport.timestamp = std::chrono::steady_clock::now();
    
    correlator_.record_request(req_header, uds::DiagnosticSessionControlRequest{
        uds::SessionType::DefaultSession
    }, req_transport);
    
    EXPECT_EQ(correlator_.pending_count(TESTER, ECU), 1);
    
    // Create response
    uds::UdsHeader resp_header;
    resp_header.service_id = uds::ServiceID::DiagnosticSessionControl;
    resp_header.direction = uds::MessageDirection::PositiveResponse;
    
    TransportInfo resp_transport;
    resp_transport.source_address = ECU;
    resp_transport.target_address = TESTER;
    resp_transport.timestamp = std::chrono::steady_clock::now() + 10ms;
    
    auto pair = correlator_.process_response(resp_header,
        uds::DiagnosticSessionControlResponse{
            uds::SessionType::DefaultSession, 50, 500
        }, resp_transport);
    
    ASSERT_NE(pair, nullptr);
    EXPECT_THAT(*pair, IsComplete());
    EXPECT_THAT(*pair, IsPositiveResponse());
    EXPECT_THAT(*pair, HasRequestService(uds::ServiceID::DiagnosticSessionControl));
}

TEST_F(RequestCorrelatorTest, MatchNegativeResponse) {
    // Record request
    uds::UdsHeader req_header;
    req_header.service_id = uds::ServiceID::SecurityAccess;
    req_header.sub_function = 0x01;
    req_header.direction = uds::MessageDirection::Request;
    
    TransportInfo transport;
    transport.source_address = TESTER;
    transport.target_address = ECU;
    transport.timestamp = std::chrono::steady_clock::now();
    
    correlator_.record_request(req_header, uds::SecurityAccessRequest{1, false, {}}, transport);
    
    // Create negative response
    uds::UdsHeader resp_header;
    resp_header.service_id = uds::ServiceID::SecurityAccess;
    resp_header.direction = uds::MessageDirection::NegativeResponse;
    resp_header.negative_response_code = static_cast<std::uint8_t>(uds::NRC::SecurityAccessDenied);
    
    TransportInfo resp_transport;
    resp_transport.source_address = ECU;
    resp_transport.target_address = TESTER;
    resp_transport.timestamp = std::chrono::steady_clock::now() + 20ms;
    
    auto pair = correlator_.process_response(resp_header,
        uds::NegativeResponseMessage{
            uds::ServiceID::SecurityAccess,
            uds::NRC::SecurityAccessDenied
        }, resp_transport);
    
    ASSERT_NE(pair, nullptr);
    EXPECT_THAT(*pair, IsNegativeResponse());
    EXPECT_THAT(*pair, HasNRC(static_cast<std::uint8_t>(uds::NRC::SecurityAccessDenied)));
}

TEST_F(RequestCorrelatorTest, HandleResponsePending) {
    // Record request
    uds::UdsHeader req_header;
    req_header.service_id = uds::ServiceID::RoutineControl;
    req_header.direction = uds::MessageDirection::Request;
    
    TransportInfo transport;
    transport.source_address = TESTER;
    transport.target_address = ECU;
    transport.timestamp = std::chrono::steady_clock::now();
    
    correlator_.record_request(req_header, uds::RoutineControlRequest{}, transport);
    
    // First response: ResponsePending
    uds::UdsHeader pending_header;
    pending_header.service_id = uds::ServiceID::RoutineControl;
    pending_header.direction = uds::MessageDirection::NegativeResponse;
    pending_header.negative_response_code = static_cast<std::uint8_t>(uds::NRC::RequestCorrectlyReceivedResponsePending);
    
    TransportInfo pending_transport;
    pending_transport.source_address = ECU;
    pending_transport.target_address = TESTER;
    pending_transport.timestamp = std::chrono::steady_clock::now() + 40ms;
    
    auto pending_pair = correlator_.process_response(pending_header,
        uds::NegativeResponseMessage{
            uds::ServiceID::RoutineControl,
            uds::NRC::RequestCorrectlyReceivedResponsePending
        }, pending_transport);
    
    // ResponsePending should not complete the pair
    EXPECT_EQ(pending_pair, nullptr);
    EXPECT_EQ(correlator_.pending_count(TESTER, ECU), 1);
    
    // Final response
    uds::UdsHeader final_header;
    final_header.service_id = uds::ServiceID::RoutineControl;
    final_header.direction = uds::MessageDirection::PositiveResponse;
    
    TransportInfo final_transport;
    final_transport.source_address = ECU;
    final_transport.target_address = TESTER;
    final_transport.timestamp = std::chrono::steady_clock::now() + 2000ms;
    
    auto pair = correlator_.process_response(final_header,
        uds::RoutineControlResponse{}, final_transport);
    
    ASSERT_NE(pair, nullptr);
    EXPECT_THAT(*pair, IsComplete());
    EXPECT_THAT(*pair, HadPendingResponse());
    EXPECT_THAT(*pair, HasPendingCount(1));
}

TEST_F(RequestCorrelatorTest, TimeoutDetection) {
    RequestCorrelator::Options opts = RequestCorrelator::Options::defaults();
    opts.timing.p2_server_max = 10ms;  // Short timeout for test
    RequestCorrelator fast_correlator(opts);
    
    // Record request
    uds::UdsHeader req_header;
    req_header.service_id = uds::ServiceID::ReadDataByIdentifier;
    req_header.direction = uds::MessageDirection::Request;
    
    TransportInfo transport;
    transport.source_address = TESTER;
    transport.target_address = ECU;
    transport.timestamp = std::chrono::steady_clock::now() - 100ms;  // Already old
    
    fast_correlator.record_request(req_header, uds::ReadDataByIdentifierRequest{}, transport);
    
    // Check timeouts
    auto timeout_count = fast_correlator.check_timeouts();
    
    EXPECT_EQ(timeout_count, 1);
    EXPECT_EQ(fast_correlator.pending_count(TESTER, ECU), 0);
    
    auto stats = fast_correlator.statistics();
    EXPECT_EQ(stats.pending_timeouts, 1);
}

TEST_F(RequestCorrelatorTest, Statistics) {
    // Record multiple requests
    for (int i = 0; i < 5; ++i) {
        uds::UdsHeader req_header;
        req_header.service_id = uds::ServiceID::ReadDataByIdentifier;
        req_header.direction = uds::MessageDirection::Request;
        
        TransportInfo transport;
        transport.source_address = TESTER;
        transport.target_address = ECU;
        transport.timestamp = std::chrono::steady_clock::now();
        
        correlator_.record_request(req_header, uds::ReadDataByIdentifierRequest{}, transport);
    }
    
    // Match some responses
    for (int i = 0; i < 3; ++i) {
        uds::UdsHeader resp_header;
        resp_header.service_id = uds::ServiceID::ReadDataByIdentifier;
        resp_header.direction = uds::MessageDirection::PositiveResponse;
        
        TransportInfo transport;
        transport.source_address = ECU;
        transport.target_address = TESTER;
        transport.timestamp = std::chrono::steady_clock::now();
        
        correlator_.process_response(resp_header, uds::ReadDataByIdentifierResponse{}, transport);
    }
    
    auto stats = correlator_.statistics();
    EXPECT_EQ(stats.requests_recorded, 5);
    EXPECT_EQ(stats.responses_matched, 3);
    EXPECT_NEAR(stats.match_rate(), 0.6, 0.01);
}

// =============================================================================
// Diagnostic Session Manager Tests
// =============================================================================

class DiagnosticSessionManagerTest : public Test {
protected:
    DiagnosticSessionManager manager_;
    
    static constexpr LogicalAddress TESTER = 0x0E00;
    static constexpr LogicalAddress ECU = 0x0001;
    
    std::vector<DiagnosticEvent> received_events_;
    
    void SetUp() override {
        manager_.on_event([this](DiagnosticEvent event, 
                                  [[maybe_unused]] const auto& state, 
                                  [[maybe_unused]] auto* pair) {
            received_events_.push_back(event);
        });
    }
};

TEST_F(DiagnosticSessionManagerTest, ProcessDiagnosticSessionControlSequence) {
    // Send DSC request
    auto req_uds = create_dsc_request(uds::SessionType::ExtendedDiagnosticSession);
    auto req_packet = create_doip_diagnostic_message(TESTER, ECU, req_uds);
    
    EXPECT_TRUE(manager_.process_doip_raw(req_packet, std::chrono::steady_clock::now()));
    
    // Send DSC response
    auto resp_uds = create_dsc_response(uds::SessionType::ExtendedDiagnosticSession, 50, 500);
    auto resp_packet = create_doip_diagnostic_message(ECU, TESTER, resp_uds);
    
    EXPECT_TRUE(manager_.process_doip_raw(resp_packet, std::chrono::steady_clock::now()));
    
    // Check session state
    auto* state = manager_.get_session_state(ECU);
    ASSERT_NE(state, nullptr);
    
    EXPECT_THAT(*state, IsSessionActive());
    EXPECT_THAT(*state, HasSessionType(uds::SessionType::ExtendedDiagnosticSession));
    EXPECT_THAT(*state, IsInExtendedSession());
    
    // Verify timing was updated
    EXPECT_EQ(state->timing.p2_server_max, 50ms);
    EXPECT_EQ(state->timing.p2_star_server_max, 5000ms);  // 500 * 10
}

TEST_F(DiagnosticSessionManagerTest, TrackSecurityAccessSequence) {
    // First establish extended session
    auto dsc_req = create_dsc_request(uds::SessionType::ExtendedDiagnosticSession);
    auto dsc_resp = create_dsc_response(uds::SessionType::ExtendedDiagnosticSession);
    
    manager_.process_doip_raw(
        create_doip_diagnostic_message(TESTER, ECU, dsc_req),
        std::chrono::steady_clock::now());
    manager_.process_doip_raw(
        create_doip_diagnostic_message(ECU, TESTER, dsc_resp),
        std::chrono::steady_clock::now());
    
    // Security Access - Request Seed
    auto seed_req = create_security_access_seed_request(0x01);
    manager_.process_doip_raw(
        create_doip_diagnostic_message(TESTER, ECU, seed_req),
        std::chrono::steady_clock::now());
    
    // Security Access - Seed Response
    std::vector<std::byte> seed = {std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    auto seed_resp = create_security_access_seed_response(0x01, seed);
    manager_.process_doip_raw(
        create_doip_diagnostic_message(ECU, TESTER, seed_resp),
        std::chrono::steady_clock::now());
    
    // Security Access - Send Key
    std::vector<std::byte> key = {std::byte{0xAB}, std::byte{0xCD}, std::byte{0xEF}, std::byte{0x01}};
    auto key_req = create_security_access_key_request(0x01, key);
    manager_.process_doip_raw(
        create_doip_diagnostic_message(TESTER, ECU, key_req),
        std::chrono::steady_clock::now());
    
    // Security Access - Key Accepted
    auto key_resp = create_security_access_key_response(0x01);
    manager_.process_doip_raw(
        create_doip_diagnostic_message(ECU, TESTER, key_resp),
        std::chrono::steady_clock::now());
    
    // Check security state
    auto* state = manager_.get_session_state(ECU);
    ASSERT_NE(state, nullptr);
    
    EXPECT_THAT(*state, HasSecurityLevel(1));
    EXPECT_THAT(*state, IsSecurityUnlocked());
    
    // Verify SecurityUnlocked event was emitted
    EXPECT_THAT(received_events_, Contains(DiagnosticEvent::SecurityUnlocked));
}

TEST_F(DiagnosticSessionManagerTest, HandleNegativeResponse) {
    // Send request
    auto req_uds = create_dsc_request(uds::SessionType::ProgrammingSession);
    manager_.process_doip_raw(
        create_doip_diagnostic_message(TESTER, ECU, req_uds),
        std::chrono::steady_clock::now());
    
    // Send negative response (conditions not correct)
    auto neg_resp = create_negative_response(
        uds::ServiceID::DiagnosticSessionControl,
        uds::NRC::ConditionsNotCorrect);
    manager_.process_doip_raw(
        create_doip_diagnostic_message(ECU, TESTER, neg_resp),
        std::chrono::steady_clock::now());
    
    // Check state (should not change to programming)
    auto* state = manager_.get_session_state(ECU);
    ASSERT_NE(state, nullptr);
    
    EXPECT_NE(state->session_type, uds::SessionType::ProgrammingSession);
    EXPECT_EQ(state->negative_responses, 1);
    
    // Verify NegativeResponse event
    EXPECT_THAT(received_events_, Contains(DiagnosticEvent::NegativeResponse));
}

TEST_F(DiagnosticSessionManagerTest, TesterPresentKeepsSessionAlive) {
    // Start session
    auto dsc_req = create_dsc_request(uds::SessionType::ExtendedDiagnosticSession);
    auto dsc_resp = create_dsc_response(uds::SessionType::ExtendedDiagnosticSession);
    
    manager_.process_doip_raw(
        create_doip_diagnostic_message(TESTER, ECU, dsc_req),
        std::chrono::steady_clock::now());
    manager_.process_doip_raw(
        create_doip_diagnostic_message(ECU, TESTER, dsc_resp),
        std::chrono::steady_clock::now());
    
    auto* state = manager_.get_session_state(ECU);
    auto first_activity = state->last_activity;
    
    // Wait a bit
    std::this_thread::sleep_for(10ms);
    
    // Send TesterPresent
    auto tp_req = create_tester_present_request();
    manager_.process_doip_raw(
        create_doip_diagnostic_message(TESTER, ECU, tp_req),
        std::chrono::steady_clock::now());
    
    auto tp_resp = create_tester_present_response();
    manager_.process_doip_raw(
        create_doip_diagnostic_message(ECU, TESTER, tp_resp),
        std::chrono::steady_clock::now());
    
    // Activity should be updated
    state = manager_.get_session_state(ECU);
    EXPECT_GT(state->last_activity, first_activity);
}

TEST_F(DiagnosticSessionManagerTest, Statistics) {
    // Process some packets
    for (int i = 0; i < 3; ++i) {
        auto req = create_dsc_request(uds::SessionType::DefaultSession);
        auto resp = create_dsc_response(uds::SessionType::DefaultSession);
        
        manager_.process_doip_raw(
            create_doip_diagnostic_message(TESTER, ECU, req),
            std::chrono::steady_clock::now());
        manager_.process_doip_raw(
            create_doip_diagnostic_message(ECU, TESTER, resp),
            std::chrono::steady_clock::now());
    }
    
    auto stats = manager_.statistics();
    EXPECT_EQ(stats.doip_packets_processed, 6);
    EXPECT_EQ(stats.diagnostic_messages, 6);
    EXPECT_EQ(stats.uds_requests, 3);
    EXPECT_EQ(stats.uds_responses, 3);
}

TEST_F(DiagnosticSessionManagerTest, MultipleECUs) {
    constexpr LogicalAddress ECU1 = 0x0001;
    constexpr LogicalAddress ECU2 = 0x0002;
    constexpr LogicalAddress ECU3 = 0x0003;
    
    // Start sessions on multiple ECUs
    for (auto ecu : {ECU1, ECU2, ECU3}) {
        auto req = create_dsc_request(uds::SessionType::ExtendedDiagnosticSession);
        auto resp = create_dsc_response(uds::SessionType::ExtendedDiagnosticSession);
        
        manager_.process_doip_raw(
            create_doip_diagnostic_message(TESTER, ecu, req),
            std::chrono::steady_clock::now());
        manager_.process_doip_raw(
            create_doip_diagnostic_message(ecu, TESTER, resp),
            std::chrono::steady_clock::now());
    }
    
    auto tracked = manager_.get_tracked_ecus();
    EXPECT_EQ(tracked.size(), 3);
    EXPECT_THAT(tracked, UnorderedElementsAre(ECU1, ECU2, ECU3));
    
    for (auto ecu : {ECU1, ECU2, ECU3}) {
        EXPECT_TRUE(manager_.is_tracking(ecu));
        auto* state = manager_.get_session_state(ecu);
        ASSERT_NE(state, nullptr);
        EXPECT_THAT(*state, IsInExtendedSession());
    }
}

// =============================================================================
// UDS over DoIP Decoder Tests
// =============================================================================

class UdsOverDoipDecoderTest : public Test {
protected:
    UdsOverDoipDecoder decoder_;
    
    static constexpr LogicalAddress TESTER = 0x0E00;
    static constexpr LogicalAddress ECU = 0x0001;
};

TEST_F(UdsOverDoipDecoderTest, DecodeSimpleRequest) {
    auto uds_data = create_dsc_request(uds::SessionType::ExtendedDiagnosticSession);
    auto packet = create_doip_diagnostic_message(TESTER, ECU, uds_data);
    
    auto result = decoder_.decode(packet);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->source_address, TESTER);
    EXPECT_EQ(result->target_address, ECU);
    EXPECT_EQ(result->service_id(), uds::ServiceID::DiagnosticSessionControl);
    EXPECT_EQ(result->direction, MessageDirection::Request);
}

TEST_F(UdsOverDoipDecoderTest, DecodePositiveResponse) {
    auto uds_data = create_dsc_response(uds::SessionType::ExtendedDiagnosticSession);
    auto packet = create_doip_diagnostic_message(ECU, TESTER, uds_data);
    
    auto result = decoder_.decode(packet);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->source_address, ECU);
    EXPECT_EQ(result->target_address, TESTER);
    EXPECT_TRUE(result->is_positive_response());
    EXPECT_EQ(result->direction, MessageDirection::Response);
}

TEST_F(UdsOverDoipDecoderTest, DecodeNegativeResponse) {
    auto uds_data = create_negative_response(
        uds::ServiceID::SecurityAccess,
        uds::NRC::InvalidKey);
    auto packet = create_doip_diagnostic_message(ECU, TESTER, uds_data);
    
    auto result = decoder_.decode(packet);
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->is_negative_response());
    EXPECT_EQ(result->direction, MessageDirection::Response);
}

TEST_F(UdsOverDoipDecoderTest, RejectNonDiagnosticMessage) {
    // Create a non-diagnostic DoIP packet (vehicle identification)
    std::vector<std::byte> packet = {
        std::byte{0x02}, std::byte{0xFD},  // Version
        std::byte{0x00}, std::byte{0x01},  // VehicleIdentificationRequest
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}  // Length 0
    };
    
    auto result = decoder_.decode(packet);
    
    ASSERT_FALSE(result.is_ok());
    EXPECT_EQ(result.error().code, UdsOverDoipError::Code::NotDiagnosticMessage);
}

TEST_F(UdsOverDoipDecoderTest, LooksLikeDiagnosticMessage) {
    auto uds_data = create_dsc_request(uds::SessionType::DefaultSession);
    auto packet = create_doip_diagnostic_message(TESTER, ECU, uds_data);
    
    EXPECT_TRUE(UdsOverDoipDecoder::looks_like_diagnostic_message(packet));
    
    // Non-diagnostic message
    std::vector<std::byte> other = {
        std::byte{0x02}, std::byte{0xFD},
        std::byte{0x00}, std::byte{0x01},  // Not diagnostic
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}
    };
    
    EXPECT_FALSE(UdsOverDoipDecoder::looks_like_diagnostic_message(other));
}

// =============================================================================
// Timing Validation Tests
// =============================================================================

class TimingValidationTest : public Test {
protected:
    DiagnosticTiming timing_{DiagnosticTiming::defaults()};
};

TEST_F(TimingValidationTest, WithinP2) {
    EXPECT_TRUE(timing_.within_p2(40ms));
    EXPECT_TRUE(timing_.within_p2(50ms));
    EXPECT_FALSE(timing_.within_p2(60ms));
}

TEST_F(TimingValidationTest, WithinP2Star) {
    EXPECT_TRUE(timing_.within_p2_star(4000ms));
    EXPECT_TRUE(timing_.within_p2_star(5000ms));
    EXPECT_FALSE(timing_.within_p2_star(6000ms));
}

TEST_F(TimingValidationTest, CustomTiming) {
    DiagnosticTiming custom;
    custom.p2_server_max = 100ms;
    custom.p2_star_server_max = 10000ms;
    
    EXPECT_TRUE(custom.within_p2(90ms));
    EXPECT_FALSE(custom.within_p2(110ms));
    
    EXPECT_TRUE(custom.within_p2_star(9000ms));
    EXPECT_FALSE(custom.within_p2_star(11000ms));
}

}  // namespace
}  // namespace wadjet::protocols::diagnostic

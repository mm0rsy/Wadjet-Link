/// @file test_uds.cpp
/// @brief Unit tests for UDS (ISO 14229) protocol decoder

#include "wadjet/protocols/uds/uds.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace wadjet::protocols::uds {
namespace {

// Helper to create byte vector from initializer list
template <typename... Args>
std::vector<std::byte> make_bytes(Args... args) {
    return {static_cast<std::byte>(args)...};
}

// ===========================================================================
// UdsDecoder Tests
// ===========================================================================

class UdsDecoderTest : public ::testing::Test {
protected:
    UdsDecoder decoder_;

    std::span<const std::byte> to_span(const std::vector<std::byte>& vec) {
        return {vec.data(), vec.size()};
    }
};

// ---------------------------------------------------------------------------
// Basic Request/Response Detection
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, DetectRequestMessage) {
    auto data = make_bytes(0x10, 0x01);  // DiagnosticSessionControl request
    EXPECT_TRUE(UdsDecoder::is_request(to_span(data)));
    EXPECT_FALSE(UdsDecoder::is_positive_response(to_span(data)));
    EXPECT_FALSE(UdsDecoder::is_negative_response(to_span(data)));
}

TEST_F(UdsDecoderTest, DetectPositiveResponse) {
    auto data = make_bytes(0x50, 0x01, 0x00, 0x19, 0x01, 0xF4);  // DiagSessionControl response
    EXPECT_FALSE(UdsDecoder::is_request(to_span(data)));
    EXPECT_TRUE(UdsDecoder::is_positive_response(to_span(data)));
    EXPECT_FALSE(UdsDecoder::is_negative_response(to_span(data)));
}

TEST_F(UdsDecoderTest, DetectNegativeResponse) {
    auto data = make_bytes(0x7F, 0x10, 0x12);  // Negative response to 0x10
    EXPECT_FALSE(UdsDecoder::is_request(to_span(data)));
    EXPECT_FALSE(UdsDecoder::is_positive_response(to_span(data)));
    EXPECT_TRUE(UdsDecoder::is_negative_response(to_span(data)));
}

TEST_F(UdsDecoderTest, LooksLikeUdsWithValidServices) {
    EXPECT_TRUE(UdsDecoder::looks_like_uds(to_span(make_bytes(0x10, 0x01))));        // Request
    EXPECT_TRUE(UdsDecoder::looks_like_uds(to_span(make_bytes(0x50, 0x01))));        // Response
    EXPECT_TRUE(UdsDecoder::looks_like_uds(to_span(make_bytes(0x22, 0xF1, 0x90))));  // ReadDID
    EXPECT_TRUE(UdsDecoder::looks_like_uds(to_span(make_bytes(0x7F, 0x22, 0x31))));  // NRC
}

TEST_F(UdsDecoderTest, LooksLikeUdsRejectsInvalid) {
    EXPECT_FALSE(UdsDecoder::looks_like_uds(to_span(make_bytes())));      // Empty
    EXPECT_FALSE(UdsDecoder::looks_like_uds(to_span(make_bytes(0x00))));  // Invalid SID
    EXPECT_FALSE(UdsDecoder::looks_like_uds(to_span(make_bytes(0xFF))));  // Invalid SID
}

// ---------------------------------------------------------------------------
// DiagnosticSessionControl (0x10)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, DiagnosticSessionControlRequest) {
    auto data = make_bytes(0x10, 0x03);  // Extended session request
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_EQ(result->header.direction, MessageDirection::Request);
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x03));
    EXPECT_FALSE(result->header.suppress_positive_response);

    auto* req = result->as<DiagnosticSessionControlRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->session_type, SessionType::ExtendedDiagnosticSession);
}

TEST_F(UdsDecoderTest, DiagnosticSessionControlRequestWithSuppressResponse) {
    auto data = make_bytes(0x10, 0x83);  // Extended session + suppress response
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x03));
    EXPECT_TRUE(result->header.suppress_positive_response);
}

TEST_F(UdsDecoderTest, DiagnosticSessionControlPositiveResponse) {
    // Response: 0x50 + session type + P2 timing (2 bytes) + P2* timing (2 bytes)
    auto data = make_bytes(0x50, 0x03, 0x00, 0x19, 0x01, 0xF4);
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_EQ(result->header.direction, MessageDirection::PositiveResponse);

    auto* resp = result->as<DiagnosticSessionControlResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->session_type, SessionType::ExtendedDiagnosticSession);
    EXPECT_EQ(resp->p2_server_max_ms, 25);        // 0x0019 = 25ms
    EXPECT_EQ(resp->p2_star_server_max_ms, 500);  // 0x01F4 = 500 (5000ms)
    EXPECT_EQ(resp->p2_star_ms(), 5000);
}

// ---------------------------------------------------------------------------
// ECUReset (0x11)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, ECUResetRequestHardReset) {
    auto data = make_bytes(0x11, 0x01);  // Hard reset
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ECUReset);

    auto* req = result->as<ECUResetRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->reset_type, ResetType::HardReset);
}

TEST_F(UdsDecoderTest, ECUResetRequestSoftReset) {
    auto data = make_bytes(0x11, 0x03);  // Soft reset
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<ECUResetRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->reset_type, ResetType::SoftReset);
}

TEST_F(UdsDecoderTest, ECUResetPositiveResponse) {
    auto data = make_bytes(0x51, 0x01);  // Hard reset response
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.direction, MessageDirection::PositiveResponse);

    auto* resp = result->as<ECUResetResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->reset_type, ResetType::HardReset);
}

// ---------------------------------------------------------------------------
// SecurityAccess (0x27)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, SecurityAccessRequestSeed) {
    auto data = make_bytes(0x27, 0x01);  // Request seed for level 1
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::SecurityAccess);

    auto* req = result->as<SecurityAccessRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->access_type, 0x01);
    EXPECT_TRUE(req->is_request_seed());
    EXPECT_FALSE(req->is_send_key());
    EXPECT_EQ(req->security_level(), 1);
}

TEST_F(UdsDecoderTest, SecurityAccessSendKey) {
    // Send key for level 1: 0x27 0x02 + key bytes
    auto data = make_bytes(0x27, 0x02, 0xDE, 0xAD, 0xBE, 0xEF);
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<SecurityAccessRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->access_type, 0x02);
    EXPECT_FALSE(req->is_request_seed());
    EXPECT_TRUE(req->is_send_key());
    EXPECT_EQ(req->security_level(), 1);
    EXPECT_EQ(req->security_key.size(), 4);
    EXPECT_EQ(req->security_key[0], 0xDE);
}

TEST_F(UdsDecoderTest, SecurityAccessResponseWithSeed) {
    auto data = make_bytes(0x67, 0x01, 0x12, 0x34, 0x56, 0x78);  // Seed response
    auto result = decoder_.decode(to_span(data));

    auto* resp = result->as<SecurityAccessResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->access_type, 0x01);
    EXPECT_EQ(resp->security_seed.size(), 4);
    EXPECT_EQ(resp->security_seed[0], 0x12);
    EXPECT_FALSE(resp->is_already_unlocked());
}

TEST_F(UdsDecoderTest, SecurityAccessAlreadyUnlocked) {
    auto data = make_bytes(0x67, 0x01, 0x00, 0x00, 0x00, 0x00);  // Zero seed = unlocked
    auto result = decoder_.decode(to_span(data));

    auto* resp = result->as<SecurityAccessResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_TRUE(resp->is_already_unlocked());
}

// ---------------------------------------------------------------------------
// TesterPresent (0x3E)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, TesterPresentRequest) {
    auto data = make_bytes(0x3E, 0x00);
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::TesterPresent);

    auto* req = result->as<TesterPresentRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->sub_function, 0x00);
    EXPECT_FALSE(req->suppress_positive_response);
}

TEST_F(UdsDecoderTest, TesterPresentRequestSuppressResponse) {
    auto data = make_bytes(0x3E, 0x80);  // Suppress positive response
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<TesterPresentRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_TRUE(req->suppress_positive_response);
}

TEST_F(UdsDecoderTest, TesterPresentResponse) {
    auto data = make_bytes(0x7E, 0x00);
    auto result = decoder_.decode(to_span(data));

    EXPECT_EQ(result->header.direction, MessageDirection::PositiveResponse);

    auto* resp = result->as<TesterPresentResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->sub_function, 0x00);
}

// ---------------------------------------------------------------------------
// ReadDataByIdentifier (0x22)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, ReadDataByIdentifierRequestSingleDID) {
    auto data = make_bytes(0x22, 0xF1, 0x90);  // Read VIN (DID 0xF190)
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ReadDataByIdentifier);

    auto* req = result->as<ReadDataByIdentifierRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->data_identifiers.size(), 1);
    EXPECT_EQ(req->data_identifiers[0], DID::VIN);
}

TEST_F(UdsDecoderTest, ReadDataByIdentifierRequestMultipleDIDs) {
    // Read VIN (0xF190) and ECU serial (0xF18C)
    auto data = make_bytes(0x22, 0xF1, 0x90, 0xF1, 0x8C);
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<ReadDataByIdentifierRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->data_identifiers.size(), 2);
    EXPECT_EQ(req->data_identifiers[0].value, 0xF190);
    EXPECT_EQ(req->data_identifiers[1].value, 0xF18C);
}

TEST_F(UdsDecoderTest, ReadDataByIdentifierResponse) {
    // Response: 0x62 + DID (2 bytes) + data
    auto data = make_bytes(0x62, 0xF1, 0x90, 'W', 'A', 'U', 'Z', 'Z', 'Z');
    auto result = decoder_.decode(to_span(data));

    EXPECT_EQ(result->header.direction, MessageDirection::PositiveResponse);

    auto* resp = result->as<ReadDataByIdentifierResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->records.size(), 1);
    EXPECT_EQ(resp->records[0].did.value, 0xF190);
    EXPECT_EQ(resp->records[0].data.size(), 6);
}

// ---------------------------------------------------------------------------
// WriteDataByIdentifier (0x2E)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, WriteDataByIdentifierRequest) {
    // Write to DID 0xF199 with some data
    auto data = make_bytes(0x2E, 0xF1, 0x99, 0x01, 0x02, 0x03);
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());

    auto* req = result->as<WriteDataByIdentifierRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->data_identifier.value, 0xF199);
    EXPECT_EQ(req->data_record.size(), 3);
}

TEST_F(UdsDecoderTest, WriteDataByIdentifierResponse) {
    auto data = make_bytes(0x6E, 0xF1, 0x99);
    auto result = decoder_.decode(to_span(data));

    auto* resp = result->as<WriteDataByIdentifierResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->data_identifier.value, 0xF199);
}

// ---------------------------------------------------------------------------
// RoutineControl (0x31)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, RoutineControlStartRoutine) {
    // Start routine 0xFF00 (erase memory)
    auto data = make_bytes(0x31, 0x01, 0xFF, 0x00);
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());

    auto* req = result->as<RoutineControlRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->routine_control_type, RoutineControlType::StartRoutine);
    EXPECT_EQ(req->routine_identifier.value, 0xFF00);
}

TEST_F(UdsDecoderTest, RoutineControlWithOptions) {
    // Start routine with option record
    auto data = make_bytes(0x31, 0x01, 0xFF, 0x01, 0x44, 0x00, 0x00, 0x00);
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<RoutineControlRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->routine_option_record.size(), 4);
}

TEST_F(UdsDecoderTest, RoutineControlRequestResults) {
    auto data = make_bytes(0x31, 0x03, 0xFF, 0x00);  // Request routine results
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<RoutineControlRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->routine_control_type, RoutineControlType::RequestRoutineResults);
}

// ---------------------------------------------------------------------------
// RequestDownload (0x34)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, RequestDownloadBasic) {
    // Format: 0x34 + dataFormatId + addressAndLengthFormat + address + size
    // 4 bytes address, 4 bytes size
    auto data = make_bytes(0x34, 0x00, 0x44, 0x00, 0x10, 0x00, 0x00,  // Address 0x00100000
                           0x00, 0x00, 0x40, 0x00);                   // Size 0x00004000
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());

    auto* req = result->as<RequestDownloadRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_TRUE(req->data_format.is_uncompressed());
    EXPECT_TRUE(req->data_format.is_unencrypted());
    EXPECT_EQ(req->address_and_length_format.memory_address_length, 4);
    EXPECT_EQ(req->address_and_length_format.memory_size_length, 4);
    EXPECT_EQ(req->memory_address, 0x00100000);
    EXPECT_EQ(req->memory_size, 0x00004000);
}

TEST_F(UdsDecoderTest, RequestDownloadResponse) {
    // Response: 0x74 + lengthFormat + maxBlockLength
    auto data = make_bytes(0x74, 0x20, 0x10, 0x00);  // 2 bytes for maxBlockLength
    auto result = decoder_.decode(to_span(data));

    auto* resp = result->as<RequestDownloadResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->max_number_of_block_length, 0x1000);
}

// ---------------------------------------------------------------------------
// TransferData (0x36)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, TransferDataRequest) {
    auto data = make_bytes(0x36, 0x01, 0xAA, 0xBB, 0xCC, 0xDD);
    auto result = decoder_.decode(to_span(data));

    auto* req = result->as<TransferDataRequest>();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->block_sequence_counter, 0x01);
    EXPECT_EQ(req->transfer_request_parameter_record.size(), 4);
}

TEST_F(UdsDecoderTest, TransferDataResponse) {
    auto data = make_bytes(0x76, 0x01);
    auto result = decoder_.decode(to_span(data));

    auto* resp = result->as<TransferDataResponse>();
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->block_sequence_counter, 0x01);
}

// ---------------------------------------------------------------------------
// RequestTransferExit (0x37)
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, RequestTransferExitRequest) {
    auto data = make_bytes(0x37);
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::RequestTransferExit);

    auto* req = result->as<RequestTransferExitRequest>();
    ASSERT_NE(req, nullptr);
}

TEST_F(UdsDecoderTest, RequestTransferExitResponse) {
    auto data = make_bytes(0x77);
    auto result = decoder_.decode(to_span(data));

    auto* resp = result->as<RequestTransferExitResponse>();
    ASSERT_NE(resp, nullptr);
}

// ---------------------------------------------------------------------------
// Negative Responses
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, NegativeResponseBasic) {
    auto data = make_bytes(0x7F, 0x22, 0x31);  // Request out of range for ReadDID
    auto result = decoder_.decode(to_span(data));

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.direction, MessageDirection::NegativeResponse);
    EXPECT_EQ(result->header.rejected_service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_EQ(result->header.negative_response_code, std::optional<std::uint8_t>(0x31));

    auto* nrc = result->as<NegativeResponseMessage>();
    ASSERT_NE(nrc, nullptr);
    EXPECT_EQ(nrc->rejected_service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_EQ(nrc->negative_response_code, NRC::RequestOutOfRange);
}

TEST_F(UdsDecoderTest, NegativeResponsePending) {
    auto data = make_bytes(0x7F, 0x10, 0x78);  // Response pending
    auto result = decoder_.decode(to_span(data));

    auto* nrc = result->as<NegativeResponseMessage>();
    ASSERT_NE(nrc, nullptr);
    EXPECT_TRUE(nrc->is_response_pending());
    EXPECT_TRUE(nrc->is_temporary());
}

TEST_F(UdsDecoderTest, NegativeResponseSecurityDenied) {
    auto data = make_bytes(0x7F, 0x22, 0x33);  // Security access denied
    auto result = decoder_.decode(to_span(data));

    auto* nrc = result->as<NegativeResponseMessage>();
    ASSERT_NE(nrc, nullptr);
    EXPECT_EQ(nrc->negative_response_code, NRC::SecurityAccessDenied);
    EXPECT_FALSE(nrc->is_temporary());
}

// ---------------------------------------------------------------------------
// Error Handling
// ---------------------------------------------------------------------------

TEST_F(UdsDecoderTest, DecodeEmptyBuffer) {
    auto data = make_bytes();
    auto result = decoder_.decode(to_span(data));

    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error().code, UdsDecodeError::Code::MessageTooShort);
}

TEST_F(UdsDecoderTest, NegativeResponseTooShort) {
    auto data = make_bytes(0x7F, 0x22);  // Missing NRC byte
    auto result = decoder_.decode(to_span(data));

    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error().code, UdsDecodeError::Code::MessageTooShort);
}

// ===========================================================================
// UDS Types Tests
// ===========================================================================

TEST(UdsTypesTest, ServiceIdStrings) {
    EXPECT_EQ(service_id_string(ServiceID::DiagnosticSessionControl), "DiagnosticSessionControl");
    EXPECT_EQ(service_id_string(ServiceID::ECUReset), "ECUReset");
    EXPECT_EQ(service_id_string(ServiceID::SecurityAccess), "SecurityAccess");
    EXPECT_EQ(service_id_string(ServiceID::ReadDataByIdentifier), "ReadDataByIdentifier");
    EXPECT_EQ(service_id_string(ServiceID::RoutineControl), "RoutineControl");
}

TEST(UdsTypesTest, SessionTypeStrings) {
    EXPECT_EQ(session_type_string(SessionType::DefaultSession), "DefaultSession");
    EXPECT_EQ(session_type_string(SessionType::ProgrammingSession), "ProgrammingSession");
    EXPECT_EQ(session_type_string(SessionType::ExtendedDiagnosticSession),
              "ExtendedDiagnosticSession");
}

TEST(UdsTypesTest, IsResponseSid) {
    EXPECT_FALSE(is_response_sid(0x10));  // Request
    EXPECT_TRUE(is_response_sid(0x50));   // Response
    EXPECT_FALSE(is_response_sid(0x7F));  // Negative response
}

TEST(UdsTypesTest, SidConversion) {
    EXPECT_EQ(request_to_response_sid(0x10), 0x50);
    EXPECT_EQ(response_to_request_sid(0x50), 0x10);
}

TEST(UdsTypesTest, DataIdentifierCommon) {
    EXPECT_EQ(DID::VIN.value, 0xF190);
    EXPECT_EQ(DID::ECUSerialNumber.value, 0xF18C);
    EXPECT_TRUE(DID::VIN.is_identification());
}

TEST(UdsTypesTest, DataIdentifierOemRange) {
    DataIdentifier oem_did{0xF150};
    EXPECT_TRUE(oem_did.is_oem_specific());

    DataIdentifier standard_did{0x1000};
    EXPECT_FALSE(standard_did.is_oem_specific());
}

TEST(UdsTypesTest, RoutineIdentifierOemRange) {
    RoutineIdentifier oem_routine{0xF123};
    EXPECT_TRUE(oem_routine.is_oem_specific());

    EXPECT_EQ(RoutineID::EraseMemory.value, 0xFF00);
}

TEST(UdsTypesTest, DTCConversion) {
    DTC dtc(0xB1, 0x23, 0x45);
    EXPECT_EQ(dtc.to_value(), 0xB12345);

    auto dtc2 = DTC::from_value(0xC01234);
    EXPECT_EQ(dtc2.high_byte, 0xC0);
    EXPECT_EQ(dtc2.middle_byte, 0x12);
    EXPECT_EQ(dtc2.low_byte, 0x34);
}

TEST(UdsTypesTest, DTCStatusMask) {
    auto mask = DTCStatusMask::from_byte(0x09);  // test_failed + confirmed_dtc
    EXPECT_TRUE(mask.test_failed);
    EXPECT_FALSE(mask.pending_dtc);
    EXPECT_TRUE(mask.confirmed_dtc);

    EXPECT_EQ(mask.to_byte(), 0x09);
}

// ===========================================================================
// NRC Tests
// ===========================================================================

TEST(NrcTest, NrcStrings) {
    EXPECT_EQ(nrc_string(NRC::ServiceNotSupported), "ServiceNotSupported");
    EXPECT_EQ(nrc_string(NRC::SecurityAccessDenied), "SecurityAccessDenied");
    EXPECT_EQ(nrc_string(NRC::RequestOutOfRange), "RequestOutOfRange");
}

TEST(NrcTest, NrcClassification) {
    EXPECT_EQ(classify_nrc(NRC::ServiceNotSupported), NRCCategory::ServiceError);
    EXPECT_EQ(classify_nrc(NRC::BusyRepeatRequest), NRCCategory::Busy);
    EXPECT_EQ(classify_nrc(NRC::SecurityAccessDenied), NRCCategory::SecurityError);
    EXPECT_EQ(classify_nrc(NRC::RequestOutOfRange), NRCCategory::ParameterError);
    EXPECT_EQ(classify_nrc(NRC::UploadDownloadNotAccepted), NRCCategory::TransferError);
    EXPECT_EQ(classify_nrc(NRC::RequestCorrectlyReceivedResponsePending),
              NRCCategory::ResponsePending);
}

TEST(NrcTest, TemporaryNrc) {
    EXPECT_TRUE(is_temporary_nrc(NRC::BusyRepeatRequest));
    EXPECT_TRUE(is_temporary_nrc(NRC::RequestCorrectlyReceivedResponsePending));
    EXPECT_TRUE(is_temporary_nrc(NRC::RequiredTimeDelayNotExpired));
    EXPECT_FALSE(is_temporary_nrc(NRC::ServiceNotSupported));
    EXPECT_FALSE(is_temporary_nrc(NRC::SecurityAccessDenied));
}

TEST(NrcTest, SecurityNrc) {
    EXPECT_TRUE(is_security_nrc(NRC::SecurityAccessDenied));
    EXPECT_TRUE(is_security_nrc(NRC::InvalidKey));
    EXPECT_TRUE(is_security_nrc(NRC::ExceededNumberOfAttempts));
    EXPECT_FALSE(is_security_nrc(NRC::ServiceNotSupported));
}

TEST(NrcTest, SessionNrc) {
    EXPECT_TRUE(is_session_nrc(NRC::ServiceNotSupportedInActiveSession));
    EXPECT_TRUE(is_session_nrc(NRC::SubFunctionNotSupportedInActiveSession));
    EXPECT_FALSE(is_session_nrc(NRC::ServiceNotSupported));
}

TEST(NrcTest, OemSpecificNrc) {
    EXPECT_TRUE(is_oem_specific_nrc(0xF0));
    EXPECT_TRUE(is_oem_specific_nrc(0xFE));
    EXPECT_FALSE(is_oem_specific_nrc(0xFF));  // Reserved by ISO
    EXPECT_FALSE(is_oem_specific_nrc(0x10));
}

// ===========================================================================
// Service Helper Tests
// ===========================================================================

TEST(ServiceHelpersTest, ServiceHasSubFunction) {
    EXPECT_TRUE(service_has_sub_function(ServiceID::DiagnosticSessionControl));
    EXPECT_TRUE(service_has_sub_function(ServiceID::ECUReset));
    EXPECT_TRUE(service_has_sub_function(ServiceID::SecurityAccess));
    EXPECT_TRUE(service_has_sub_function(ServiceID::TesterPresent));
    EXPECT_TRUE(service_has_sub_function(ServiceID::RoutineControl));

    EXPECT_FALSE(service_has_sub_function(ServiceID::ReadDataByIdentifier));
    EXPECT_FALSE(service_has_sub_function(ServiceID::WriteDataByIdentifier));
    EXPECT_FALSE(service_has_sub_function(ServiceID::TransferData));
}

TEST(ServiceHelpersTest, ExtractSubFunction) {
    EXPECT_EQ(extract_sub_function(0x03), 0x03);
    EXPECT_EQ(extract_sub_function(0x83), 0x03);  // With suppress bit
    EXPECT_EQ(extract_sub_function(0x7F), 0x7F);
}

TEST(ServiceHelpersTest, ExtractSuppressPositiveResponse) {
    EXPECT_FALSE(extract_suppress_positive_response(0x03));
    EXPECT_TRUE(extract_suppress_positive_response(0x83));
    EXPECT_TRUE(extract_suppress_positive_response(0x80));
}

TEST(ServiceHelpersTest, MakeSubFunctionByte) {
    EXPECT_EQ(make_sub_function_byte(0x03, false), 0x03);
    EXPECT_EQ(make_sub_function_byte(0x03, true), 0x83);
    EXPECT_EQ(make_sub_function_byte(0x01, true), 0x81);
}

TEST(ServiceHelpersTest, SecurityAccessHelpers) {
    EXPECT_TRUE(is_request_seed(0x01));
    EXPECT_FALSE(is_request_seed(0x02));
    EXPECT_TRUE(is_request_seed(0x03));
    EXPECT_FALSE(is_request_seed(0x04));

    EXPECT_EQ(get_security_level(0x01), 1);
    EXPECT_EQ(get_security_level(0x02), 1);
    EXPECT_EQ(get_security_level(0x03), 2);
    EXPECT_EQ(get_security_level(0x04), 2);
}

}  // namespace
}  // namespace wadjet::protocols::uds

/// @file test_uds_regression.cpp
/// @brief Regression tests for UDS protocol using known byte sequences
/// @details Tests UDS decoding against known ECU patterns and edge cases

#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_session.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::uds;

namespace {

// =============================================================================
// Known byte sequences for regression testing
// =============================================================================

/// DiagnosticSessionControl - Default to Extended session
namespace dsc_default_to_extended {
// Request: 0x10 0x03 (DSC to Extended session)
const std::vector<std::uint8_t> request = {0x10, 0x03};

// Positive response: 0x50 0x03 <P2> <P2*>
const std::vector<std::uint8_t> response = {
    0x50, 0x03,  // DSC response + Extended session
    0x00, 0x19,  // P2 server max = 25ms (0x0019)
    0x01, 0xF4   // P2* server max = 500ms (0x01F4)
};

// NRC: conditions not correct
const std::vector<std::uint8_t> nrc_conditions = {0x7F, 0x10, 0x22};
}  // namespace dsc_default_to_extended

/// SecurityAccess - Level 1 unlock sequence
namespace sa_level1_unlock {
// Request seed (level 1)
const std::vector<std::uint8_t> request_seed = {0x27, 0x01};

// Seed response
const std::vector<std::uint8_t> seed_response = {
    0x67, 0x01,             // SA response + level 1
    0x12, 0x34, 0x56, 0x78  // 4-byte seed
};

// Send key (level 2 = level 1 + 1)
const std::vector<std::uint8_t> send_key = {
    0x27, 0x02,             // SA send key
    0xAB, 0xCD, 0xEF, 0x01  // 4-byte key
};

// Key accepted
const std::vector<std::uint8_t> key_accepted = {0x67, 0x02};

// Invalid key NRC
const std::vector<std::uint8_t> invalid_key = {0x7F, 0x27, 0x35};

// Exceeded attempts NRC
const std::vector<std::uint8_t> exceeded_attempts = {0x7F, 0x27, 0x36};

// Time delay not expired NRC
const std::vector<std::uint8_t> time_delay = {0x7F, 0x27, 0x37};
}  // namespace sa_level1_unlock

/// ReadDataByIdentifier - VIN request
namespace rdbi_vin {
// Request: Read VIN (DID 0xF190)
const std::vector<std::uint8_t> request = {0x22, 0xF1, 0x90};

// Response with VIN
const std::vector<std::uint8_t> response = {0x62, 0xF1, 0x90,  // RDBI response + DID
                                            'W',  'A',  'U',  'Z', 'Z', 'Z', '8', 'V', '5',
                                            'K',  'A',  '0',  '1', '2', '3', '4', '5'};

// Request out of range (DID not supported)
const std::vector<std::uint8_t> nrc_out_of_range = {0x7F, 0x22, 0x31};
}  // namespace rdbi_vin

/// ReadDataByIdentifier - Multiple DIDs
namespace rdbi_multi {
// Request: Read multiple DIDs
const std::vector<std::uint8_t> request = {
    0x22, 0xF1, 0x90,  // VIN
    0xF1, 0x86,        // Active diagnostic session
    0xF1, 0x8C         // ECU serial number
};
}  // namespace rdbi_multi

/// WriteDataByIdentifier
namespace wdbi {
// Write programming date
const std::vector<std::uint8_t> write_date = {
    0x2E, 0xF1, 0x99,  // WDBI + Programming date DID
    0x24, 0x01, 0x15   // Date: 2024-01-15 (BCD)
};

// Positive response
const std::vector<std::uint8_t> response = {0x6E, 0xF1, 0x99};

// Security access denied
const std::vector<std::uint8_t> nrc_security = {0x7F, 0x2E, 0x33};
}  // namespace wdbi

/// ECUReset
namespace ecu_reset {
// Hard reset
const std::vector<std::uint8_t> hard_reset = {0x11, 0x01};

// Key off/on reset
const std::vector<std::uint8_t> key_off_on = {0x11, 0x02};

// Soft reset
const std::vector<std::uint8_t> soft_reset = {0x11, 0x03};

// Positive response
const std::vector<std::uint8_t> response = {0x51, 0x01};

// Sub-function not supported
const std::vector<std::uint8_t> nrc_sub_not_supported = {0x7F, 0x11, 0x12};
}  // namespace ecu_reset

/// TesterPresent
namespace tester_present {
// Without SPR
const std::vector<std::uint8_t> request = {0x3E, 0x00};

// With SPR (suppress positive response)
const std::vector<std::uint8_t> request_spr = {0x3E, 0x80};

// Response
const std::vector<std::uint8_t> response = {0x7E, 0x00};
}  // namespace tester_present

/// RoutineControl
namespace routine_control {
// Start routine (erase memory)
const std::vector<std::uint8_t> start_erase = {
    0x31, 0x01,              // RC + Start
    0xFF, 0x00,              // Routine ID: Erase memory
    0x44,                    // Address format
    0x00, 0x01, 0x00, 0x00,  // Start address
    0x00, 0x00, 0x10, 0x00   // Length
};

// Request results
const std::vector<std::uint8_t> request_results = {
    0x31, 0x03,  // RC + Request results
    0xFF, 0x00   // Routine ID
};

// Stop routine
const std::vector<std::uint8_t> stop_routine = {
    0x31, 0x02,  // RC + Stop
    0xFF, 0x00   // Routine ID
};

// Positive response - routine complete
const std::vector<std::uint8_t> response_complete = {
    0x71, 0x01,  // RC response + Start
    0xFF, 0x00,  // Routine ID
    0x00         // Status: complete
};
}  // namespace routine_control

/// RequestDownload
namespace request_download {
// Download request
const std::vector<std::uint8_t> request = {
    0x34,                    // RequestDownload
    0x00,                    // Data format (uncompressed, unencrypted)
    0x44,                    // Address and length format
    0x00, 0x01, 0x00, 0x00,  // Memory address
    0x00, 0x00, 0x10, 0x00   // Memory size
};

// Positive response
const std::vector<std::uint8_t> response = {
    0x74,       // Positive response
    0x20,       // Length format
    0x00, 0x02  // Max block length = 512
};
}  // namespace request_download

/// TransferData
namespace transfer_data {
// Transfer block 1
const std::vector<std::uint8_t> block1 = {
    0x36, 0x01,                         // TransferData + block counter
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05  // Data
};

// Positive response
const std::vector<std::uint8_t> response = {0x76, 0x01};

// Wrong block sequence
const std::vector<std::uint8_t> nrc_sequence = {0x7F, 0x36, 0x73};
}  // namespace transfer_data

/// Response pending (NRC 0x78)
namespace response_pending {
const std::vector<std::uint8_t> for_routine = {0x7F, 0x31, 0x78};
const std::vector<std::uint8_t> for_security = {0x7F, 0x27, 0x78};
const std::vector<std::uint8_t> for_download = {0x7F, 0x34, 0x78};
}  // namespace response_pending

}  // namespace

// =============================================================================
// Regression Tests
// =============================================================================

class UdsRegressionTest : public ::testing::Test {
protected:
    UdsDecoder decoder_;
};

TEST_F(UdsRegressionTest, DSC_DefaultToExtended_Request) {
    std::span<const std::uint8_t> span(dsc_default_to_extended::request);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_TRUE(result->header.is_request());
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x03));  // Extended session
}

TEST_F(UdsRegressionTest, DSC_DefaultToExtended_Response) {
    std::span<const std::uint8_t> span(dsc_default_to_extended::response);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::DiagnosticSessionControl);
    EXPECT_TRUE(result->header.is_positive_response());
}

TEST_F(UdsRegressionTest, DSC_ConditionsNotCorrect) {
    std::span<const std::uint8_t> span(dsc_default_to_extended::nrc_conditions);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code,
              static_cast<std::uint8_t>(NRC::ConditionsNotCorrect));
    EXPECT_EQ(result->header.rejected_service_id, ServiceID::DiagnosticSessionControl);
}

TEST_F(UdsRegressionTest, SA_RequestSeed) {
    std::span<const std::uint8_t> span(sa_level1_unlock::request_seed);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::SecurityAccess);
    EXPECT_TRUE(result->header.is_request());
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x01));
}

TEST_F(UdsRegressionTest, SA_SeedResponse) {
    std::span<const std::uint8_t> span(sa_level1_unlock::seed_response);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::SecurityAccess);
    EXPECT_TRUE(result->header.is_positive_response());
}

TEST_F(UdsRegressionTest, SA_InvalidKey) {
    std::span<const std::uint8_t> span(sa_level1_unlock::invalid_key);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code, static_cast<std::uint8_t>(NRC::InvalidKey));
}

TEST_F(UdsRegressionTest, SA_ExceededAttempts) {
    std::span<const std::uint8_t> span(sa_level1_unlock::exceeded_attempts);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code,
              static_cast<std::uint8_t>(NRC::ExceededNumberOfAttempts));
}

TEST_F(UdsRegressionTest, SA_TimeDelayNotExpired) {
    std::span<const std::uint8_t> span(sa_level1_unlock::time_delay);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code,
              static_cast<std::uint8_t>(NRC::RequiredTimeDelayNotExpired));
}

TEST_F(UdsRegressionTest, RDBI_VIN_Request) {
    std::span<const std::uint8_t> span(rdbi_vin::request);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_TRUE(result->header.is_request());
}

TEST_F(UdsRegressionTest, RDBI_VIN_Response) {
    std::span<const std::uint8_t> span(rdbi_vin::response);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_TRUE(result->header.is_positive_response());
}

TEST_F(UdsRegressionTest, RDBI_MultiDID) {
    std::span<const std::uint8_t> span(rdbi_multi::request);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ReadDataByIdentifier);
    EXPECT_TRUE(result->header.is_request());
}

TEST_F(UdsRegressionTest, WDBI_WriteDate) {
    std::span<const std::uint8_t> span(wdbi::write_date);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::WriteDataByIdentifier);
    EXPECT_TRUE(result->header.is_request());
}

TEST_F(UdsRegressionTest, WDBI_SecurityDenied) {
    std::span<const std::uint8_t> span(wdbi::nrc_security);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code,
              static_cast<std::uint8_t>(NRC::SecurityAccessDenied));
}

TEST_F(UdsRegressionTest, ECUReset_Hard) {
    std::span<const std::uint8_t> span(ecu_reset::hard_reset);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ECUReset);
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x01));
}

TEST_F(UdsRegressionTest, ECUReset_Soft) {
    std::span<const std::uint8_t> span(ecu_reset::soft_reset);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ECUReset);
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x03));
}

TEST_F(UdsRegressionTest, TesterPresent_Request) {
    std::span<const std::uint8_t> span(tester_present::request);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::TesterPresent);
    EXPECT_FALSE(result->header.suppress_positive_response);
}

TEST_F(UdsRegressionTest, TesterPresent_WithSPR) {
    std::span<const std::uint8_t> span(tester_present::request_spr);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::TesterPresent);
    EXPECT_TRUE(result->header.suppress_positive_response);
}

TEST_F(UdsRegressionTest, RoutineControl_StartErase) {
    std::span<const std::uint8_t> span(routine_control::start_erase);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::RoutineControl);
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x01));  // Start
}

TEST_F(UdsRegressionTest, RoutineControl_Stop) {
    std::span<const std::uint8_t> span(routine_control::stop_routine);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::RoutineControl);
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x02));  // Stop
}

TEST_F(UdsRegressionTest, RoutineControl_RequestResults) {
    std::span<const std::uint8_t> span(routine_control::request_results);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::RoutineControl);
    EXPECT_EQ(result->header.sub_function, std::optional<std::uint8_t>(0x03));  // Request results
}

TEST_F(UdsRegressionTest, RequestDownload) {
    std::span<const std::uint8_t> span(request_download::request);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::RequestDownload);
    EXPECT_TRUE(result->header.is_request());
}

TEST_F(UdsRegressionTest, TransferData_Block1) {
    std::span<const std::uint8_t> span(transfer_data::block1);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::TransferData);
    EXPECT_TRUE(result->header.is_request());
}

TEST_F(UdsRegressionTest, TransferData_WrongSequence) {
    std::span<const std::uint8_t> span(transfer_data::nrc_sequence);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code,
              static_cast<std::uint8_t>(NRC::WrongBlockSequenceCounter));
}

TEST_F(UdsRegressionTest, ResponsePending_ForRoutine) {
    std::span<const std::uint8_t> span(response_pending::for_routine);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
    EXPECT_EQ(result->header.negative_response_code,
              static_cast<std::uint8_t>(NRC::RequestCorrectlyReceivedResponsePending));
    EXPECT_EQ(result->header.rejected_service_id, ServiceID::RoutineControl);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(UdsRegressionTest, MinimalRequest) {
    // Single byte request (e.g., ClearDTC with no parameters)
    std::vector<std::uint8_t> data = {0x14};  // ClearDiagnosticInformation
    std::span<const std::uint8_t> span(data);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result->header.service_id, ServiceID::ClearDiagnosticInformation);
}

TEST_F(UdsRegressionTest, EmptyData) {
    std::vector<std::uint8_t> data;
    std::span<const std::uint8_t> span(data);
    auto result = decoder_.decode(span);

    EXPECT_FALSE(result.is_ok());
}

TEST_F(UdsRegressionTest, UnknownServiceID) {
    std::vector<std::uint8_t> data = {0xBA};  // Unknown service
    std::span<const std::uint8_t> span(data);
    auto result = decoder_.decode(span);

    // Should still decode, but service_id may be Unknown
    ASSERT_TRUE(result.is_ok());
}

TEST_F(UdsRegressionTest, NRC_MinimalLength) {
    std::vector<std::uint8_t> data = {0x7F, 0x10, 0x11};  // Minimal NRC
    std::span<const std::uint8_t> span(data);
    auto result = decoder_.decode(span);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result->header.is_negative_response());
}

TEST_F(UdsRegressionTest, AllStandardNRCs) {
    // Test all standard NRC codes decode correctly
    struct NrcTestCase {
        std::uint8_t code;
        NRC expected;
    };

    std::vector<NrcTestCase> test_cases = {
        {0x10, NRC::GeneralReject},
        {0x11, NRC::ServiceNotSupported},
        {0x12, NRC::SubFunctionNotSupported},
        {0x13, NRC::IncorrectMessageLengthOrInvalidFormat},
        {0x14, NRC::ResponseTooLong},
        {0x21, NRC::BusyRepeatRequest},
        {0x22, NRC::ConditionsNotCorrect},
        {0x24, NRC::RequestSequenceError},
        {0x31, NRC::RequestOutOfRange},
        {0x33, NRC::SecurityAccessDenied},
        {0x35, NRC::InvalidKey},
        {0x36, NRC::ExceededNumberOfAttempts},
        {0x37, NRC::RequiredTimeDelayNotExpired},
        {0x70, NRC::UploadDownloadNotAccepted},
        {0x71, NRC::TransferDataSuspended},
        {0x72, NRC::GeneralProgrammingFailure},
        {0x73, NRC::WrongBlockSequenceCounter},
        {0x78, NRC::RequestCorrectlyReceivedResponsePending},
        {0x7E, NRC::SubFunctionNotSupportedInActiveSession},
        {0x7F, NRC::ServiceNotSupportedInActiveSession},
    };

    for (const auto& tc : test_cases) {
        std::vector<std::uint8_t> data = {0x7F, 0x22, tc.code};
        std::span<const std::uint8_t> span(data);
        auto result = decoder_.decode(span);

        ASSERT_TRUE(result.is_ok()) << "Failed for NRC 0x" << std::hex << static_cast<int>(tc.code);
        EXPECT_EQ(result->header.negative_response_code,
                  std::optional<std::uint8_t>(static_cast<std::uint8_t>(tc.expected)))
            << "Mismatch for NRC 0x" << std::hex << static_cast<int>(tc.code);
    }
}

// =============================================================================
// Session Tracking Regression Tests
// =============================================================================

class UdsSessionRegressionTest : public ::testing::Test {
protected:
    UdsSession session_{0x0001};
};

TEST_F(UdsSessionRegressionTest, FullUnlockSequence) {
    // 1. Enter extended session
    std::span<const std::uint8_t> dsc_req(dsc_default_to_extended::request);
    session_.process_message(dsc_req, true);

    std::span<const std::uint8_t> dsc_resp(dsc_default_to_extended::response);
    session_.process_message(dsc_resp, false);

    EXPECT_EQ(session_.session_type(), SessionType::ExtendedDiagnosticSession);

    // 2. Request seed
    std::span<const std::uint8_t> seed_req(sa_level1_unlock::request_seed);
    session_.process_message(seed_req, true);

    std::span<const std::uint8_t> seed_resp(sa_level1_unlock::seed_response);
    session_.process_message(seed_resp, false);

    EXPECT_FALSE(session_.is_security_unlocked(1));  // Not yet unlocked

    // 3. Send key
    std::span<const std::uint8_t> key_req(sa_level1_unlock::send_key);
    session_.process_message(key_req, true);

    std::span<const std::uint8_t> key_resp(sa_level1_unlock::key_accepted);
    session_.process_message(key_resp, false);

    EXPECT_TRUE(session_.is_security_unlocked(1));  // Now unlocked
}

TEST_F(UdsSessionRegressionTest, TesterPresentKeepsSession) {
    // Enter extended session
    std::span<const std::uint8_t> dsc_req(dsc_default_to_extended::request);
    session_.process_message(dsc_req, true);

    std::span<const std::uint8_t> dsc_resp(dsc_default_to_extended::response);
    session_.process_message(dsc_resp, false);

    // Send multiple tester present
    for (int i = 0; i < 5; ++i) {
        std::span<const std::uint8_t> tp_req(tester_present::request);
        session_.process_message(tp_req, true);

        std::span<const std::uint8_t> tp_resp(tester_present::response);
        session_.process_message(tp_resp, false);
    }

    // Session should still be active
    EXPECT_EQ(session_.session_type(), SessionType::ExtendedDiagnosticSession);
    EXPECT_TRUE(session_.is_active());
}

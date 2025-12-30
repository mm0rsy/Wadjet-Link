#pragma once

/// @file uds_services.hpp
/// @brief UDS (ISO 14229) service-specific structures
///
/// This file contains request/response structures for all UDS services
/// as defined in ISO 14229-1.

#include "uds_types.hpp"
#include "uds_nrc.hpp"

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace wadjet::protocols::uds {

// =============================================================================
// Forward declarations
// =============================================================================

struct DiagnosticSessionControlRequest;
struct DiagnosticSessionControlResponse;
struct ECUResetRequest;
struct ECUResetResponse;
struct SecurityAccessRequest;
struct SecurityAccessResponse;
struct CommunicationControlRequest;
struct CommunicationControlResponse;
struct TesterPresentRequest;
struct TesterPresentResponse;
struct ControlDTCSettingRequest;
struct ControlDTCSettingResponse;
struct ReadDataByIdentifierRequest;
struct ReadDataByIdentifierResponse;
struct WriteDataByIdentifierRequest;
struct WriteDataByIdentifierResponse;
struct ReadDTCInformationRequest;
struct ReadDTCInformationResponse;
struct ClearDiagnosticInformationRequest;
struct ClearDiagnosticInformationResponse;
struct RoutineControlRequest;
struct RoutineControlResponse;
struct InputOutputControlByIdentifierRequest;
struct InputOutputControlByIdentifierResponse;
struct RequestDownloadRequest;
struct RequestDownloadResponse;
struct RequestUploadRequest;
struct RequestUploadResponse;
struct TransferDataRequest;
struct TransferDataResponse;
struct RequestTransferExitRequest;
struct RequestTransferExitResponse;
struct ReadMemoryByAddressRequest;
struct ReadMemoryByAddressResponse;
struct WriteMemoryByAddressRequest;
struct WriteMemoryByAddressResponse;
struct NegativeResponseMessage;

// =============================================================================
// Service Message Variant
// =============================================================================

/// @brief Variant type for all UDS service messages
using UdsServiceMessage = std::variant<
    std::monostate,  // Unknown/unparsed
    DiagnosticSessionControlRequest,
    DiagnosticSessionControlResponse,
    ECUResetRequest,
    ECUResetResponse,
    SecurityAccessRequest,
    SecurityAccessResponse,
    CommunicationControlRequest,
    CommunicationControlResponse,
    TesterPresentRequest,
    TesterPresentResponse,
    ControlDTCSettingRequest,
    ControlDTCSettingResponse,
    ReadDataByIdentifierRequest,
    ReadDataByIdentifierResponse,
    WriteDataByIdentifierRequest,
    WriteDataByIdentifierResponse,
    ReadDTCInformationRequest,
    ReadDTCInformationResponse,
    ClearDiagnosticInformationRequest,
    ClearDiagnosticInformationResponse,
    RoutineControlRequest,
    RoutineControlResponse,
    InputOutputControlByIdentifierRequest,
    InputOutputControlByIdentifierResponse,
    RequestDownloadRequest,
    RequestDownloadResponse,
    RequestUploadRequest,
    RequestUploadResponse,
    TransferDataRequest,
    TransferDataResponse,
    RequestTransferExitRequest,
    RequestTransferExitResponse,
    ReadMemoryByAddressRequest,
    ReadMemoryByAddressResponse,
    WriteMemoryByAddressRequest,
    WriteMemoryByAddressResponse,
    NegativeResponseMessage
>;

// =============================================================================
// Diagnostic Session Control (0x10)
// =============================================================================

/// @brief DiagnosticSessionControl Request (0x10)
struct DiagnosticSessionControlRequest {
    SessionType session_type = SessionType::DefaultSession;
    bool suppress_positive_response = false;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::DiagnosticSessionControl;
    }

    /// @brief Minimum message size (SID + sub-function)
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief DiagnosticSessionControl Positive Response
struct DiagnosticSessionControlResponse {
    SessionType session_type = SessionType::DefaultSession;
    std::uint16_t p2_server_max_ms = 0;      ///< P2 timing in milliseconds
    std::uint16_t p2_star_server_max_ms = 0; ///< P2* timing in 10ms units

    /// @brief Get P2* in milliseconds
    [[nodiscard]] std::uint32_t p2_star_ms() const {
        return static_cast<std::uint32_t>(p2_star_server_max_ms) * 10;
    }

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::DiagnosticSessionControl;
    }
};

// =============================================================================
// ECU Reset (0x11)
// =============================================================================

/// @brief ECUReset Request (0x11)
struct ECUResetRequest {
    ResetType reset_type = ResetType::HardReset;
    bool suppress_positive_response = false;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ECUReset;
    }

    /// @brief Minimum message size (SID + sub-function)
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief ECUReset Positive Response
struct ECUResetResponse {
    ResetType reset_type = ResetType::HardReset;
    std::optional<std::uint8_t> power_down_time;  ///< For rapid shutdown, time in seconds

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ECUReset;
    }
};

// =============================================================================
// Security Access (0x27)
// =============================================================================

/// @brief SecurityAccess Request (0x27)
struct SecurityAccessRequest {
    std::uint8_t access_type = 0x01;  ///< Odd = requestSeed, even = sendKey
    bool suppress_positive_response = false;
    std::vector<std::uint8_t> security_key;  ///< Key for sendKey requests

    /// @brief Check if this is a seed request
    [[nodiscard]] bool is_request_seed() const {
        return (access_type & 0x01) != 0;
    }

    /// @brief Check if this is a key send
    [[nodiscard]] bool is_send_key() const {
        return (access_type & 0x01) == 0;
    }

    /// @brief Get security level (1-33)
    [[nodiscard]] std::uint8_t security_level() const {
        return static_cast<std::uint8_t>((access_type + 1) / 2);
    }

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::SecurityAccess;
    }

    /// @brief Minimum message size (SID + sub-function)
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief SecurityAccess Positive Response
struct SecurityAccessResponse {
    std::uint8_t access_type = 0x01;
    std::vector<std::uint8_t> security_seed;  ///< Seed for requestSeed responses

    /// @brief Check if seed is zero (already unlocked)
    [[nodiscard]] bool is_already_unlocked() const {
        for (auto b : security_seed) {
            if (b != 0) return false;
        }
        return true;
    }

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::SecurityAccess;
    }
};

// =============================================================================
// Communication Control (0x28)
// =============================================================================

/// @brief CommunicationControl Request (0x28)
struct CommunicationControlRequest {
    CommunicationControlType control_type = CommunicationControlType::EnableRxAndTx;
    CommunicationType communication_type;
    bool suppress_positive_response = false;
    std::optional<std::uint8_t> node_identification_number;  ///< For subnet

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::CommunicationControl;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 3; }
};

/// @brief CommunicationControl Positive Response
struct CommunicationControlResponse {
    CommunicationControlType control_type = CommunicationControlType::EnableRxAndTx;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::CommunicationControl;
    }
};

// =============================================================================
// Tester Present (0x3E)
// =============================================================================

/// @brief TesterPresent Request (0x3E)
struct TesterPresentRequest {
    std::uint8_t sub_function = 0x00;  ///< Usually 0x00 or 0x80 (suppress response)
    bool suppress_positive_response = false;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::TesterPresent;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief TesterPresent Positive Response
struct TesterPresentResponse {
    std::uint8_t sub_function = 0x00;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::TesterPresent;
    }
};

// =============================================================================
// Control DTC Setting (0x85)
// =============================================================================

/// @brief ControlDTCSetting Request (0x85)
struct ControlDTCSettingRequest {
    ControlDTCSettingType dtc_setting_type = ControlDTCSettingType::On;
    bool suppress_positive_response = false;
    std::vector<std::uint8_t> dtc_setting_control_option_record;  ///< Optional OEM data

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ControlDTCSetting;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief ControlDTCSetting Positive Response
struct ControlDTCSettingResponse {
    ControlDTCSettingType dtc_setting_type = ControlDTCSettingType::On;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ControlDTCSetting;
    }
};

// =============================================================================
// Read Data By Identifier (0x22)
// =============================================================================

/// @brief ReadDataByIdentifier Request (0x22)
struct ReadDataByIdentifierRequest {
    std::vector<DataIdentifier> data_identifiers;  ///< One or more DIDs to read

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ReadDataByIdentifier;
    }

    /// @brief Minimum message size (SID + at least one DID)
    [[nodiscard]] static constexpr std::size_t min_size() { return 3; }
};

/// @brief Single DID data record in response
struct DataRecord {
    DataIdentifier did;
    std::vector<std::uint8_t> data;
};

/// @brief ReadDataByIdentifier Positive Response (0x62)
struct ReadDataByIdentifierResponse {
    std::vector<DataRecord> records;  ///< DID + data pairs

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ReadDataByIdentifier;
    }

    /// @brief Find data for a specific DID
    [[nodiscard]] const DataRecord* find_did(DataIdentifier did) const {
        for (const auto& record : records) {
            if (record.did == did) return &record;
        }
        return nullptr;
    }
};

// =============================================================================
// Write Data By Identifier (0x2E)
// =============================================================================

/// @brief WriteDataByIdentifier Request (0x2E)
struct WriteDataByIdentifierRequest {
    DataIdentifier data_identifier;
    std::vector<std::uint8_t> data_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::WriteDataByIdentifier;
    }

    /// @brief Minimum message size (SID + DID + at least 1 byte data)
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief WriteDataByIdentifier Positive Response
struct WriteDataByIdentifierResponse {
    DataIdentifier data_identifier;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::WriteDataByIdentifier;
    }
};

// =============================================================================
// Read DTC Information (0x19)
// =============================================================================

/// @brief ReadDTCInformation Request (0x19)
struct ReadDTCInformationRequest {
    ReadDTCSubFunction sub_function = ReadDTCSubFunction::ReportDTCByStatusMask;
    bool suppress_positive_response = false;

    // Optional parameters depending on sub-function
    std::optional<std::uint8_t> dtc_status_mask;
    std::optional<DTC> dtc_mask_record;
    std::optional<std::uint8_t> dtc_snapshot_record_number;
    std::optional<std::uint8_t> dtc_extended_data_record_number;
    std::optional<std::uint8_t> dtc_severity_mask;
    std::optional<std::uint8_t> memory_selection;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ReadDTCInformation;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief DTC with status
struct DTCAndStatus {
    DTC dtc;
    DTCStatusMask status;
};

/// @brief ReadDTCInformation Positive Response
struct ReadDTCInformationResponse {
    ReadDTCSubFunction sub_function = ReadDTCSubFunction::ReportDTCByStatusMask;
    std::optional<std::uint8_t> dtc_status_availability_mask;
    std::optional<std::uint8_t> dtc_format_identifier;  ///< 0x01 = ISO 14229-1, 0x02 = ISO 15031-6
    std::optional<std::uint16_t> dtc_count;

    std::vector<DTCAndStatus> dtc_list;
    std::vector<std::uint8_t> dtc_record_data;  ///< For snapshot/extended data

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ReadDTCInformation;
    }
};

// =============================================================================
// Clear Diagnostic Information (0x14)
// =============================================================================

/// @brief ClearDiagnosticInformation Request (0x14)
struct ClearDiagnosticInformationRequest {
    std::uint32_t group_of_dtc = 0xFFFFFF;  ///< 0xFFFFFF = all DTCs

    /// @brief Check if clearing all DTCs
    [[nodiscard]] bool is_clear_all() const {
        return group_of_dtc == 0xFFFFFF;
    }

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ClearDiagnosticInformation;
    }

    /// @brief Minimum message size (SID + 3 byte group)
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief ClearDiagnosticInformation Positive Response
struct ClearDiagnosticInformationResponse {
    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ClearDiagnosticInformation;
    }
};

// =============================================================================
// Routine Control (0x31)
// =============================================================================

/// @brief RoutineControl Request (0x31)
struct RoutineControlRequest {
    RoutineControlType routine_control_type = RoutineControlType::StartRoutine;
    RoutineIdentifier routine_identifier;
    std::vector<std::uint8_t> routine_option_record;
    bool suppress_positive_response = false;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RoutineControl;
    }

    /// @brief Minimum message size (SID + sub + 2-byte routine ID)
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief RoutineControl Positive Response
struct RoutineControlResponse {
    RoutineControlType routine_control_type = RoutineControlType::StartRoutine;
    RoutineIdentifier routine_identifier;
    std::vector<std::uint8_t> routine_info;
    std::vector<std::uint8_t> routine_status_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RoutineControl;
    }
};

// =============================================================================
// Input/Output Control By Identifier (0x2F)
// =============================================================================

/// @brief InputOutputControlByIdentifier Request (0x2F)
struct InputOutputControlByIdentifierRequest {
    DataIdentifier data_identifier;
    IOControlParameter control_parameter = IOControlParameter::ReturnControlToECU;
    std::vector<std::uint8_t> control_state;
    std::vector<std::uint8_t> control_enable_mask;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::InputOutputControlByIdentifier;
    }

    /// @brief Minimum message size (SID + 2-byte DID + control param)
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief InputOutputControlByIdentifier Positive Response
struct InputOutputControlByIdentifierResponse {
    DataIdentifier data_identifier;
    IOControlParameter control_parameter = IOControlParameter::ReturnControlToECU;
    std::vector<std::uint8_t> control_status_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::InputOutputControlByIdentifier;
    }
};

// =============================================================================
// Request Download (0x34)
// =============================================================================

/// @brief RequestDownload Request (0x34)
struct RequestDownloadRequest {
    DataFormatIdentifier data_format;
    AddressAndLengthFormatIdentifier address_and_length_format;
    std::uint64_t memory_address = 0;
    std::uint64_t memory_size = 0;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RequestDownload;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief RequestDownload Positive Response
struct RequestDownloadResponse {
    std::uint8_t length_format_identifier = 0;  ///< Number of bytes for maxNumberOfBlockLength
    std::uint32_t max_number_of_block_length = 0;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RequestDownload;
    }
};

// =============================================================================
// Request Upload (0x35)
// =============================================================================

/// @brief RequestUpload Request (0x35)
struct RequestUploadRequest {
    DataFormatIdentifier data_format;
    AddressAndLengthFormatIdentifier address_and_length_format;
    std::uint64_t memory_address = 0;
    std::uint64_t memory_size = 0;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RequestUpload;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief RequestUpload Positive Response
struct RequestUploadResponse {
    std::uint8_t length_format_identifier = 0;
    std::uint32_t max_number_of_block_length = 0;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RequestUpload;
    }
};

// =============================================================================
// Transfer Data (0x36)
// =============================================================================

/// @brief TransferData Request (0x36)
struct TransferDataRequest {
    std::uint8_t block_sequence_counter = 0;
    std::vector<std::uint8_t> transfer_request_parameter_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::TransferData;
    }

    /// @brief Minimum message size (SID + block counter)
    [[nodiscard]] static constexpr std::size_t min_size() { return 2; }
};

/// @brief TransferData Positive Response
struct TransferDataResponse {
    std::uint8_t block_sequence_counter = 0;
    std::vector<std::uint8_t> transfer_response_parameter_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::TransferData;
    }
};

// =============================================================================
// Request Transfer Exit (0x37)
// =============================================================================

/// @brief RequestTransferExit Request (0x37)
struct RequestTransferExitRequest {
    std::vector<std::uint8_t> transfer_request_parameter_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RequestTransferExit;
    }

    /// @brief Minimum message size (SID only)
    [[nodiscard]] static constexpr std::size_t min_size() { return 1; }
};

/// @brief RequestTransferExit Positive Response
struct RequestTransferExitResponse {
    std::vector<std::uint8_t> transfer_response_parameter_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::RequestTransferExit;
    }
};

// =============================================================================
// Read Memory By Address (0x23)
// =============================================================================

/// @brief ReadMemoryByAddress Request (0x23)
struct ReadMemoryByAddressRequest {
    AddressAndLengthFormatIdentifier address_and_length_format;
    std::uint64_t memory_address = 0;
    std::uint64_t memory_size = 0;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ReadMemoryByAddress;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 4; }
};

/// @brief ReadMemoryByAddress Positive Response
struct ReadMemoryByAddressResponse {
    std::vector<std::uint8_t> data_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::ReadMemoryByAddress;
    }
};

// =============================================================================
// Write Memory By Address (0x3D)
// =============================================================================

/// @brief WriteMemoryByAddress Request (0x3D)
struct WriteMemoryByAddressRequest {
    AddressAndLengthFormatIdentifier address_and_length_format;
    std::uint64_t memory_address = 0;
    std::uint64_t memory_size = 0;
    std::vector<std::uint8_t> data_record;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::WriteMemoryByAddress;
    }

    /// @brief Minimum message size
    [[nodiscard]] static constexpr std::size_t min_size() { return 5; }
};

/// @brief WriteMemoryByAddress Positive Response
struct WriteMemoryByAddressResponse {
    AddressAndLengthFormatIdentifier address_and_length_format;
    std::uint64_t memory_address = 0;
    std::uint64_t memory_size = 0;

    /// @brief Get service ID
    [[nodiscard]] static constexpr ServiceID service_id() {
        return ServiceID::WriteMemoryByAddress;
    }
};

// =============================================================================
// Negative Response (0x7F)
// =============================================================================

/// @brief Negative Response Message
struct NegativeResponseMessage {
    ServiceID rejected_service_id = ServiceID::TesterPresent;
    NRC negative_response_code = NRC::GeneralReject;

    /// @brief Get raw NRC byte value
    [[nodiscard]] std::uint8_t nrc_byte() const {
        return static_cast<std::uint8_t>(negative_response_code);
    }

    /// @brief Check if this is a response pending NRC
    [[nodiscard]] bool is_response_pending() const {
        return negative_response_code == NRC::RequestCorrectlyReceivedResponsePending;
    }

    /// @brief Check if this is a temporary error (can retry)
    [[nodiscard]] bool is_temporary() const {
        return is_temporary_nrc(negative_response_code);
    }

    /// @brief Get NRC description string
    [[nodiscard]] std::string_view nrc_string() const {
        return ::wadjet::protocols::uds::nrc_string(negative_response_code);
    }

    /// @brief Get NRC detailed description
    [[nodiscard]] std::string_view nrc_description() const {
        return ::wadjet::protocols::uds::nrc_description(negative_response_code);
    }

    /// @brief Get service ID (for response SID - always 0x7F)
    [[nodiscard]] static constexpr std::uint8_t response_sid() {
        return NEGATIVE_RESPONSE_SID;
    }

    /// @brief Message size (always 3 bytes)
    [[nodiscard]] static constexpr std::size_t size() { return 3; }
};

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Check if a service has a sub-function byte
[[nodiscard]] constexpr bool service_has_sub_function(ServiceID sid) {
    switch (sid) {
        case ServiceID::DiagnosticSessionControl:
        case ServiceID::ECUReset:
        case ServiceID::SecurityAccess:
        case ServiceID::CommunicationControl:
        case ServiceID::TesterPresent:
        case ServiceID::ControlDTCSetting:
        case ServiceID::ResponseOnEvent:
        case ServiceID::LinkControl:
        case ServiceID::ReadDTCInformation:
        case ServiceID::RoutineControl:
            return true;
        default:
            return false;
    }
}

/// @brief Check if a service supports suppress positive response
[[nodiscard]] constexpr bool service_supports_suppress_response(ServiceID sid) {
    // Most services with sub-function support suppress positive response
    return service_has_sub_function(sid);
}

/// @brief Extract sub-function value (bits 0-6) from sub-function byte
[[nodiscard]] constexpr std::uint8_t extract_sub_function(std::uint8_t byte) {
    return byte & 0x7F;
}

/// @brief Extract suppress positive response bit (bit 7) from sub-function byte
[[nodiscard]] constexpr bool extract_suppress_positive_response(std::uint8_t byte) {
    return (byte & 0x80) != 0;
}

/// @brief Create sub-function byte from value and suppress flag
[[nodiscard]] constexpr std::uint8_t make_sub_function_byte(
    std::uint8_t sub_function, bool suppress_positive_response) {
    return (sub_function & 0x7F) | (suppress_positive_response ? 0x80 : 0x00);
}

}  // namespace wadjet::protocols::uds

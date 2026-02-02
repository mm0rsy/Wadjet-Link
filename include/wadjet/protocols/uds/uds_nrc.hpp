#pragma once

/// @file uds_nrc.hpp
/// @brief UDS (ISO 14229) Negative Response Code definitions
///
/// This file contains all Negative Response Codes (NRCs) as defined in
/// ISO 14229-1 with descriptions and classification helpers.

#include <cstdint>
#include <string_view>

namespace wadjet::protocols::uds {

/// @brief Negative Response Code (NRC)
///
/// ISO 14229-1 defines negative response codes that indicate why a diagnostic
/// service request was rejected.
enum class NRC : std::uint8_t {
    // General Response Codes (0x00-0x21)
    PositiveResponse = 0x00,                       ///< Not an NRC - positive response
    GeneralReject = 0x10,                          ///< Service rejected for unspecified reason
    ServiceNotSupported = 0x11,                    ///< Service ID not supported
    SubFunctionNotSupported = 0x12,                ///< Sub-function not supported
    IncorrectMessageLengthOrInvalidFormat = 0x13,  ///< Invalid message length or format
    ResponseTooLong = 0x14,                        ///< Response would exceed max length
    // 0x15-0x20: Reserved by ISO

    // Busy Response Codes (0x21-0x22)
    BusyRepeatRequest = 0x21,     ///< Server busy, repeat request later
    ConditionsNotCorrect = 0x22,  ///< Preconditions not met

    // 0x23: Reserved by ISO

    // Sequence Error (0x24)
    RequestSequenceError = 0x24,  ///< Request received in wrong sequence

    // No Response Required (0x25)
    NoResponseFromSubnetComponent = 0x25,  ///< Gateway: no response from subnet

    // Failure Response Codes (0x26)
    FailurePreventsExecutionOfRequestedAction = 0x26,  ///< Failure prevents execution

    // 0x27-0x30: Reserved by ISO

    // Request Out of Range (0x31)
    RequestOutOfRange = 0x31,  ///< Parameter value out of range

    // 0x32: Reserved by ISO

    // Security Response Codes (0x33-0x36)
    SecurityAccessDenied = 0x33,  ///< Security access not granted
    // 0x34: Reserved by ISO (was AuthenticationRequired in earlier versions)
    InvalidKey = 0x35,                ///< Security key does not match
    ExceededNumberOfAttempts = 0x36,  ///< Too many failed security attempts

    // Time Delay Response Codes (0x37)
    RequiredTimeDelayNotExpired = 0x37,  ///< Security delay timer active

    // 0x38-0x4F: Reserved by ISO for security

    // Secure Data Transmission (0x50-0x6F - Reserved)

    // Upload/Download Response Codes (0x70-0x77)
    UploadDownloadNotAccepted = 0x70,  ///< Upload/download request rejected
    TransferDataSuspended = 0x71,      ///< Data transfer suspended
    GeneralProgrammingFailure = 0x72,  ///< Programming operation failed
    WrongBlockSequenceCounter = 0x73,  ///< Block sequence error
    // 0x74-0x77: Reserved by ISO

    // Service Execution Response Codes (0x78)
    RequestCorrectlyReceivedResponsePending = 0x78,  ///< Response pending, keep waiting

    // 0x79-0x7D: Reserved by ISO

    // Service Not Supported In Active Session (0x7E)
    SubFunctionNotSupportedInActiveSession = 0x7E,  ///< Sub-function not in current session

    // Service Not Supported In Active Session (0x7F)
    ServiceNotSupportedInActiveSession = 0x7F,  ///< Service not in current session

    // 0x80: Reserved by ISO

    // RPM Response Codes (0x81-0x82)
    RpmTooHigh = 0x81,  ///< Engine RPM too high
    RpmTooLow = 0x82,   ///< Engine RPM too low

    // Engine State Response Codes (0x83-0x84)
    EngineIsRunning = 0x83,     ///< Engine must be stopped
    EngineIsNotRunning = 0x84,  ///< Engine must be running

    // Operating Time Response Codes (0x85-0x86)
    EngineRunTimeTooLow = 0x85,  ///< Engine run time too short
    // 0x86: Reserved by ISO

    // Temperature Response Codes (0x87-0x88)
    TemperatureTooHigh = 0x87,  ///< Temperature too high
    TemperatureTooLow = 0x88,   ///< Temperature too low

    // Speed Response Codes (0x89-0x8A)
    VehicleSpeedTooHigh = 0x89,  ///< Vehicle speed too high
    VehicleSpeedTooLow = 0x8A,   ///< Vehicle speed too low

    // Throttle/Pedal Response Codes (0x8B-0x8C)
    ThrottlePedalTooHigh = 0x8B,  ///< Throttle position too high
    ThrottlePedalTooLow = 0x8C,   ///< Throttle position too low

    // Transmission Response Codes (0x8D-0x8F)
    TransmissionRangeNotInNeutral = 0x8D,  ///< Transmission not in neutral
    TransmissionRangeNotInGear = 0x8E,     ///< Transmission not in gear
    // 0x8F: Reserved by ISO

    // Brake Response Codes (0x90-0x91)
    BrakeSwitchNotClosed = 0x90,   ///< Brake pedal not applied
    ShifterLeverNotInPark = 0x91,  ///< Shifter not in park

    // Torque Converter Response Codes (0x92)
    TorqueConverterClutchLocked = 0x92,  ///< Torque converter clutch locked

    // Voltage Response Codes (0x93-0x94)
    VoltageTooHigh = 0x93,  ///< System voltage too high
    VoltageTooLow = 0x94,   ///< System voltage too low

    // 0x95-0xFF: Reserved for future or OEM-specific use
};

/// @brief Convert NRC to human-readable string
[[nodiscard]] constexpr std::string_view nrc_string(NRC nrc) {
    switch (nrc) {
        case NRC::PositiveResponse:
            return "PositiveResponse";
        case NRC::GeneralReject:
            return "GeneralReject";
        case NRC::ServiceNotSupported:
            return "ServiceNotSupported";
        case NRC::SubFunctionNotSupported:
            return "SubFunctionNotSupported";
        case NRC::IncorrectMessageLengthOrInvalidFormat:
            return "IncorrectMessageLengthOrInvalidFormat";
        case NRC::ResponseTooLong:
            return "ResponseTooLong";
        case NRC::BusyRepeatRequest:
            return "BusyRepeatRequest";
        case NRC::ConditionsNotCorrect:
            return "ConditionsNotCorrect";
        case NRC::RequestSequenceError:
            return "RequestSequenceError";
        case NRC::NoResponseFromSubnetComponent:
            return "NoResponseFromSubnetComponent";
        case NRC::FailurePreventsExecutionOfRequestedAction:
            return "FailurePreventsExecutionOfRequestedAction";
        case NRC::RequestOutOfRange:
            return "RequestOutOfRange";
        case NRC::SecurityAccessDenied:
            return "SecurityAccessDenied";
        case NRC::InvalidKey:
            return "InvalidKey";
        case NRC::ExceededNumberOfAttempts:
            return "ExceededNumberOfAttempts";
        case NRC::RequiredTimeDelayNotExpired:
            return "RequiredTimeDelayNotExpired";
        case NRC::UploadDownloadNotAccepted:
            return "UploadDownloadNotAccepted";
        case NRC::TransferDataSuspended:
            return "TransferDataSuspended";
        case NRC::GeneralProgrammingFailure:
            return "GeneralProgrammingFailure";
        case NRC::WrongBlockSequenceCounter:
            return "WrongBlockSequenceCounter";
        case NRC::RequestCorrectlyReceivedResponsePending:
            return "RequestCorrectlyReceivedResponsePending";
        case NRC::SubFunctionNotSupportedInActiveSession:
            return "SubFunctionNotSupportedInActiveSession";
        case NRC::ServiceNotSupportedInActiveSession:
            return "ServiceNotSupportedInActiveSession";
        case NRC::RpmTooHigh:
            return "RpmTooHigh";
        case NRC::RpmTooLow:
            return "RpmTooLow";
        case NRC::EngineIsRunning:
            return "EngineIsRunning";
        case NRC::EngineIsNotRunning:
            return "EngineIsNotRunning";
        case NRC::EngineRunTimeTooLow:
            return "EngineRunTimeTooLow";
        case NRC::TemperatureTooHigh:
            return "TemperatureTooHigh";
        case NRC::TemperatureTooLow:
            return "TemperatureTooLow";
        case NRC::VehicleSpeedTooHigh:
            return "VehicleSpeedTooHigh";
        case NRC::VehicleSpeedTooLow:
            return "VehicleSpeedTooLow";
        case NRC::ThrottlePedalTooHigh:
            return "ThrottlePedalTooHigh";
        case NRC::ThrottlePedalTooLow:
            return "ThrottlePedalTooLow";
        case NRC::TransmissionRangeNotInNeutral:
            return "TransmissionRangeNotInNeutral";
        case NRC::TransmissionRangeNotInGear:
            return "TransmissionRangeNotInGear";
        case NRC::BrakeSwitchNotClosed:
            return "BrakeSwitchNotClosed";
        case NRC::ShifterLeverNotInPark:
            return "ShifterLeverNotInPark";
        case NRC::TorqueConverterClutchLocked:
            return "TorqueConverterClutchLocked";
        case NRC::VoltageTooHigh:
            return "VoltageTooHigh";
        case NRC::VoltageTooLow:
            return "VoltageTooLow";
        default:
            return "Unknown";
    }
}

/// @brief Convert NRC byte value to human-readable string
[[nodiscard]] constexpr std::string_view nrc_string(std::uint8_t nrc) {
    return nrc_string(static_cast<NRC>(nrc));
}

/// @brief Get detailed description of NRC
[[nodiscard]] constexpr std::string_view nrc_description(NRC nrc) {
    switch (nrc) {
        case NRC::PositiveResponse:
            return "The request was successfully processed";
        case NRC::GeneralReject:
            return "Service request rejected for unspecified reason";
        case NRC::ServiceNotSupported:
            return "The requested service identifier is not supported";
        case NRC::SubFunctionNotSupported:
            return "The requested sub-function is not supported";
        case NRC::IncorrectMessageLengthOrInvalidFormat:
            return "Message length is incorrect or format is invalid";
        case NRC::ResponseTooLong:
            return "Response message would exceed maximum transport layer capacity";
        case NRC::BusyRepeatRequest:
            return "Server is busy, client should repeat the request";
        case NRC::ConditionsNotCorrect:
            return "Conditions are not correct to perform the requested action";
        case NRC::RequestSequenceError:
            return "Message sequence error - request received in wrong order";
        case NRC::NoResponseFromSubnetComponent:
            return "Gateway received no response from the subnet component";
        case NRC::FailurePreventsExecutionOfRequestedAction:
            return "A failure condition prevents execution of the request";
        case NRC::RequestOutOfRange:
            return "Request contains a parameter value that is out of range";
        case NRC::SecurityAccessDenied:
            return "Security access was denied - authentication required";
        case NRC::InvalidKey:
            return "Security key sent by the client does not match";
        case NRC::ExceededNumberOfAttempts:
            return "Number of failed security access attempts exceeded limit";
        case NRC::RequiredTimeDelayNotExpired:
            return "Security access delay timer has not yet expired";
        case NRC::UploadDownloadNotAccepted:
            return "Data transfer request (upload/download) was not accepted";
        case NRC::TransferDataSuspended:
            return "Data transfer has been suspended";
        case NRC::GeneralProgrammingFailure:
            return "A programming operation has failed";
        case NRC::WrongBlockSequenceCounter:
            return "Block sequence counter in request is incorrect";
        case NRC::RequestCorrectlyReceivedResponsePending:
            return "Request received, response pending - continue waiting";
        case NRC::SubFunctionNotSupportedInActiveSession:
            return "Sub-function is not supported in the active diagnostic session";
        case NRC::ServiceNotSupportedInActiveSession:
            return "Service is not supported in the active diagnostic session";
        case NRC::RpmTooHigh:
            return "Engine RPM is too high to perform the requested action";
        case NRC::RpmTooLow:
            return "Engine RPM is too low to perform the requested action";
        case NRC::EngineIsRunning:
            return "Engine is running but must be stopped for this operation";
        case NRC::EngineIsNotRunning:
            return "Engine is not running but must be running for this operation";
        case NRC::EngineRunTimeTooLow:
            return "Engine run time is too low for this operation";
        case NRC::TemperatureTooHigh:
            return "Temperature is too high for this operation";
        case NRC::TemperatureTooLow:
            return "Temperature is too low for this operation";
        case NRC::VehicleSpeedTooHigh:
            return "Vehicle speed is too high for this operation";
        case NRC::VehicleSpeedTooLow:
            return "Vehicle speed is too low for this operation";
        case NRC::ThrottlePedalTooHigh:
            return "Throttle/pedal position is too high for this operation";
        case NRC::ThrottlePedalTooLow:
            return "Throttle/pedal position is too low for this operation";
        case NRC::TransmissionRangeNotInNeutral:
            return "Transmission is not in neutral";
        case NRC::TransmissionRangeNotInGear:
            return "Transmission is not in gear";
        case NRC::BrakeSwitchNotClosed:
            return "Brake switch is not closed (brake pedal not applied)";
        case NRC::ShifterLeverNotInPark:
            return "Shifter lever is not in park position";
        case NRC::TorqueConverterClutchLocked:
            return "Torque converter clutch is locked";
        case NRC::VoltageTooHigh:
            return "System voltage is too high for this operation";
        case NRC::VoltageTooLow:
            return "System voltage is too low for this operation";
        default:
            return "Unknown or reserved negative response code";
    }
}

/// @brief NRC Classification
enum class NRCCategory {
    Success,           ///< Not an error (0x00)
    ServiceError,      ///< Service-related errors (0x10-0x14)
    Busy,              ///< Server busy (0x21)
    ConditionError,    ///< Condition/sequence errors (0x22-0x26)
    ParameterError,    ///< Parameter out of range (0x31)
    SecurityError,     ///< Security-related errors (0x33-0x37)
    TransferError,     ///< Upload/download errors (0x70-0x73)
    ResponsePending,   ///< Response pending (0x78)
    SessionError,      ///< Session-related errors (0x7E-0x7F)
    VehicleCondition,  ///< Vehicle condition errors (0x81-0x94)
    Reserved,          ///< Reserved NRC values
};

/// @brief Classify an NRC into a category
[[nodiscard]] constexpr NRCCategory classify_nrc(NRC nrc) {
    auto value = static_cast<std::uint8_t>(nrc);

    if (value == 0x00)
        return NRCCategory::Success;
    if (value >= 0x10 && value <= 0x14)
        return NRCCategory::ServiceError;
    if (value == 0x21)
        return NRCCategory::Busy;
    if (value >= 0x22 && value <= 0x26)
        return NRCCategory::ConditionError;
    if (value == 0x31)
        return NRCCategory::ParameterError;
    if (value >= 0x33 && value <= 0x37)
        return NRCCategory::SecurityError;
    if (value >= 0x70 && value <= 0x73)
        return NRCCategory::TransferError;
    if (value == 0x78)
        return NRCCategory::ResponsePending;
    if (value >= 0x7E && value <= 0x7F)
        return NRCCategory::SessionError;
    if (value >= 0x81 && value <= 0x94)
        return NRCCategory::VehicleCondition;
    return NRCCategory::Reserved;
}

/// @brief Classify an NRC byte value into a category
[[nodiscard]] constexpr NRCCategory classify_nrc(std::uint8_t nrc) {
    return classify_nrc(static_cast<NRC>(nrc));
}

/// @brief Check if NRC indicates a temporary condition (can be retried)
///
/// Temporary conditions include busy states and response pending that
/// may resolve on retry.
[[nodiscard]] constexpr bool is_temporary_nrc(NRC nrc) {
    switch (nrc) {
        case NRC::BusyRepeatRequest:
        case NRC::RequestCorrectlyReceivedResponsePending:
        case NRC::RequiredTimeDelayNotExpired:
            return true;
        default:
            return false;
    }
}

/// @brief Check if NRC indicates a temporary condition (byte version)
[[nodiscard]] constexpr bool is_temporary_nrc(std::uint8_t nrc) {
    return is_temporary_nrc(static_cast<NRC>(nrc));
}

/// @brief Check if NRC requires security access to resolve
[[nodiscard]] constexpr bool is_security_nrc(NRC nrc) {
    auto category = classify_nrc(nrc);
    return category == NRCCategory::SecurityError;
}

/// @brief Check if NRC requires a different session to resolve
[[nodiscard]] constexpr bool is_session_nrc(NRC nrc) {
    auto category = classify_nrc(nrc);
    return category == NRCCategory::SessionError;
}

/// @brief Check if NRC indicates a programming/transfer error
[[nodiscard]] constexpr bool is_transfer_nrc(NRC nrc) {
    auto category = classify_nrc(nrc);
    return category == NRCCategory::TransferError;
}

/// @brief Check if NRC indicates a vehicle condition issue
[[nodiscard]] constexpr bool is_vehicle_condition_nrc(NRC nrc) {
    auto category = classify_nrc(nrc);
    return category == NRCCategory::VehicleCondition;
}

/// @brief Check if the NRC value is in the OEM-specific range
[[nodiscard]] constexpr bool is_oem_specific_nrc(std::uint8_t nrc) {
    // 0xF0-0xFE are reserved for vehicle manufacturer specific use
    return nrc >= 0xF0 && nrc <= 0xFE;
}

/// @brief Check if the NRC value is in the system supplier specific range
[[nodiscard]] constexpr bool is_supplier_specific_nrc(std::uint8_t nrc) {
    // Certain ranges are reserved for system supplier specific use
    // 0x95-0xEF generally available for supplier use (not ISO-defined)
    return nrc >= 0x95 && nrc <= 0xEF;
}

// Forward declaration for ServiceID (defined in uds_types.hpp)
// Using raw values to avoid circular dependency
namespace detail {

/// @brief Service-specific context for RequestOutOfRange (0x31)
[[nodiscard]] constexpr std::string_view request_out_of_range_context(std::uint8_t service_id) {
    switch (service_id) {
        case 0x22:  // ReadDataByIdentifier
            return "The requested Data Identifier (DID) is not supported or does not exist";
        case 0x23:  // ReadMemoryByAddress
            return "The requested memory address or size is outside the valid range";
        case 0x24:  // ReadScalingDataByIdentifier
            return "The requested scaling DID is not supported";
        case 0x2A:  // ReadDataByPeriodicIdentifier
            return "The requested periodic identifier is not supported";
        case 0x2C:  // DynamicallyDefineDataIdentifier
            return "The DID definition parameters are invalid or out of range";
        case 0x2E:  // WriteDataByIdentifier
            return "The DID value to write is out of the allowed range";
        case 0x2F:  // InputOutputControlByIdentifier
            return "The I/O control parameter or DID is out of range";
        case 0x31:  // RoutineControl
            return "The routine ID or option record parameter is out of range";
        case 0x34:  // RequestDownload
            return "The memory address, size, or format identifier is out of range";
        case 0x35:  // RequestUpload
            return "The memory address, size, or format identifier is out of range";
        case 0x36:  // TransferData
            return "The block sequence counter or data length is invalid";
        case 0x38:  // RequestFileTransfer
            return "The file path, name, or parameters are invalid";
        case 0x3D:  // WriteMemoryByAddress
            return "The memory address or data to write is out of valid range";
        default:
            return "Request contains a parameter value that is out of range";
    }
}

/// @brief Service-specific context for ConditionsNotCorrect (0x22)
[[nodiscard]] constexpr std::string_view conditions_not_correct_context(std::uint8_t service_id) {
    switch (service_id) {
        case 0x10:  // DiagnosticSessionControl
            return "Session transition not allowed due to current vehicle/ECU state";
        case 0x11:  // ECUReset
            return "ECU reset not allowed - vehicle conditions not met (e.g., ignition state)";
        case 0x27:  // SecurityAccess
            return "Security access not allowed - preconditions not met or wrong session";
        case 0x28:  // CommunicationControl
            return "Communication control not allowed in current session or vehicle state";
        case 0x2E:  // WriteDataByIdentifier
            return "DID cannot be written - security level or session requirements not met";
        case 0x2F:  // InputOutputControlByIdentifier
            return "I/O control not allowed - safety conditions or prerequisites not met";
        case 0x31:  // RoutineControl
            return "Routine cannot execute - preconditions not met (session, security, sequence)";
        case 0x34:  // RequestDownload
            return "Download not allowed - programming session not active or security not unlocked";
        case 0x35:  // RequestUpload
            return "Upload not allowed - programming session not active or security not unlocked";
        case 0x36:  // TransferData
            return "Data transfer not allowed - no active download/upload session";
        case 0x37:  // RequestTransferExit
            return "Transfer exit not allowed - transfer not complete or not in progress";
        case 0x85:  // ControlDTCSetting
            return "DTC setting control not allowed in current diagnostic session";
        default:
            return "Conditions are not correct to perform the requested action";
    }
}

/// @brief Service-specific context for SubFunctionNotSupported (0x12)
[[nodiscard]] constexpr std::string_view sub_function_not_supported_context(
    std::uint8_t service_id) {
    switch (service_id) {
        case 0x10:  // DiagnosticSessionControl
            return "The requested diagnostic session type is not supported by this ECU";
        case 0x11:  // ECUReset
            return "The requested reset type is not supported by this ECU";
        case 0x19:  // ReadDTCInformation
            return "The requested DTC report type (sub-function) is not supported";
        case 0x27:  // SecurityAccess
            return "The requested security access type/level is not supported";
        case 0x28:  // CommunicationControl
            return "The requested communication control type is not supported";
        case 0x31:  // RoutineControl
            return "The requested routine control type (start/stop/result) is not supported";
        case 0x3E:  // TesterPresent
            return "The requested sub-function for TesterPresent is not supported";
        case 0x85:  // ControlDTCSetting
            return "The requested DTC setting control type is not supported";
        case 0x86:  // ResponseOnEvent
            return "The requested event type is not supported";
        case 0x87:  // LinkControl
            return "The requested link control mode is not supported";
        default:
            return "The requested sub-function is not supported";
    }
}

/// @brief Service-specific context for SecurityAccessDenied (0x33)
[[nodiscard]] constexpr std::string_view security_access_denied_context(std::uint8_t service_id) {
    switch (service_id) {
        case 0x22:  // ReadDataByIdentifier
            return "Reading this DID requires a higher security level";
        case 0x23:  // ReadMemoryByAddress
            return "Reading this memory region requires security unlock";
        case 0x2E:  // WriteDataByIdentifier
            return "Writing this DID requires security authentication";
        case 0x2F:  // InputOutputControlByIdentifier
            return "I/O control for this identifier requires security access";
        case 0x31:  // RoutineControl
            return "This routine requires security unlock before execution";
        case 0x34:  // RequestDownload
            return "Download to this memory area requires security authentication";
        case 0x35:  // RequestUpload
            return "Upload from this memory area requires security authentication";
        case 0x3D:  // WriteMemoryByAddress
            return "Writing to this memory region requires security unlock";
        default:
            return "Security access was denied - authentication required";
    }
}

/// @brief Service-specific context for ServiceNotSupported (0x11)
[[nodiscard]] constexpr std::string_view service_not_supported_context(std::uint8_t service_id) {
    switch (service_id) {
        case 0x22:
            return "ReadDataByIdentifier service is not implemented in this ECU";
        case 0x23:
            return "ReadMemoryByAddress service is not implemented in this ECU";
        case 0x2E:
            return "WriteDataByIdentifier service is not implemented in this ECU";
        case 0x2F:
            return "InputOutputControlByIdentifier service is not implemented in this ECU";
        case 0x31:
            return "RoutineControl service is not implemented in this ECU";
        case 0x34:
            return "RequestDownload service is not implemented (no flash programming support)";
        case 0x35:
            return "RequestUpload service is not implemented (no memory upload support)";
        case 0x36:
            return "TransferData service is not implemented in this ECU";
        default:
            return "The requested service identifier is not supported";
    }
}

/// @brief Service-specific context for TransferDataSuspended (0x71)
[[nodiscard]] constexpr std::string_view transfer_data_suspended_context(std::uint8_t service_id) {
    switch (service_id) {
        case 0x34:  // RequestDownload
            return "Download operation has been suspended due to an error";
        case 0x35:  // RequestUpload
            return "Upload operation has been suspended due to an error";
        case 0x36:  // TransferData
            return "Data block transfer suspended - verify data integrity";
        case 0x38:  // RequestFileTransfer
            return "File transfer operation has been suspended";
        default:
            return "Data transfer has been suspended";
    }
}

/// @brief Service-specific context for GeneralProgrammingFailure (0x72)
[[nodiscard]] constexpr std::string_view general_programming_failure_context(
    std::uint8_t service_id) {
    switch (service_id) {
        case 0x34:  // RequestDownload
            return "Failed to initialize memory for download (erase/prepare failure)";
        case 0x36:  // TransferData
            return "Failed to program data block to memory (write/flash failure)";
        case 0x37:  // RequestTransferExit
            return "Programming verification failed - data integrity error";
        default:
            return "A programming operation has failed";
    }
}

}  // namespace detail

/// @brief Get service-specific NRC description
///
/// Provides context-aware NRC descriptions based on the rejected service.
/// Some NRC codes have different meanings depending on which service returned them.
///
/// @param service_id The service that was rejected (from negative response)
/// @param nrc The Negative Response Code
/// @return Context-specific description string
[[nodiscard]] constexpr std::string_view service_specific_nrc_description(std::uint8_t service_id,
                                                                          NRC nrc) {
    switch (nrc) {
        case NRC::RequestOutOfRange:
            return detail::request_out_of_range_context(service_id);
        case NRC::ConditionsNotCorrect:
            return detail::conditions_not_correct_context(service_id);
        case NRC::SubFunctionNotSupported:
            return detail::sub_function_not_supported_context(service_id);
        case NRC::SecurityAccessDenied:
            return detail::security_access_denied_context(service_id);
        case NRC::ServiceNotSupported:
            return detail::service_not_supported_context(service_id);
        case NRC::TransferDataSuspended:
            return detail::transfer_data_suspended_context(service_id);
        case NRC::GeneralProgrammingFailure:
            return detail::general_programming_failure_context(service_id);
        default:
            // For NRCs without service-specific context, return generic description
            return nrc_description(nrc);
    }
}

/// @brief Get service-specific NRC description (byte version)
[[nodiscard]] constexpr std::string_view service_specific_nrc_description(std::uint8_t service_id,
                                                                          std::uint8_t nrc) {
    return service_specific_nrc_description(service_id, static_cast<NRC>(nrc));
}

}  // namespace wadjet::protocols::uds

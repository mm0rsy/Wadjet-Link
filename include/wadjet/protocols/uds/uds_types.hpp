#pragma once

/// @file uds_types.hpp
/// @brief UDS (ISO 14229) type definitions
///
/// This file contains the fundamental types used in the Unified Diagnostic
/// Services (UDS) protocol as defined in ISO 14229-1.

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wadjet::protocols::uds {

/// @brief Minimum UDS message size (1 byte for SID)
inline constexpr std::size_t MIN_MESSAGE_SIZE = 1;

/// @brief Maximum UDS message size (single frame, excluding transport overhead)
inline constexpr std::size_t MAX_SINGLE_FRAME_SIZE = 4095;

/// @brief Negative response SID
inline constexpr std::uint8_t NEGATIVE_RESPONSE_SID = 0x7F;

/// @brief Response SID offset (request SID + 0x40)
inline constexpr std::uint8_t RESPONSE_SID_OFFSET = 0x40;

/// @brief UDS Service Identifier (SID)
///
/// Defines all standard UDS services as per ISO 14229-1
enum class ServiceID : std::uint8_t {
    // Diagnostic and Communication Management
    DiagnosticSessionControl = 0x10,  ///< Start diagnostic session
    ECUReset = 0x11,                  ///< Reset ECU
    SecurityAccess = 0x27,            ///< Security unlock
    CommunicationControl = 0x28,      ///< Enable/disable communication
    TesterPresent = 0x3E,             ///< Keep session alive
    AccessTimingParameter = 0x83,     ///< Read/modify timing parameters
    SecuredDataTransmission = 0x84,   ///< Encrypted data transfer
    ControlDTCSetting = 0x85,         ///< Enable/disable DTC setting
    ResponseOnEvent = 0x86,           ///< Event-driven responses
    LinkControl = 0x87,               ///< Control baud rate

    // Data Transmission
    ReadDataByIdentifier = 0x22,             ///< Read DID values
    ReadMemoryByAddress = 0x23,              ///< Read memory
    ReadScalingDataByIdentifier = 0x24,      ///< Read DID with scaling
    ReadDataByPeriodicIdentifier = 0x2A,     ///< Periodic DID read
    DynamicallyDefineDataIdentifier = 0x2C,  ///< Create dynamic DID
    WriteDataByIdentifier = 0x2E,            ///< Write DID value
    WriteMemoryByAddress = 0x3D,             ///< Write memory

    // Stored Data Transmission
    ClearDiagnosticInformation = 0x14,  ///< Clear DTCs
    ReadDTCInformation = 0x19,          ///< Read DTC status

    // Input/Output Control
    InputOutputControlByIdentifier = 0x2F,  ///< Control I/O

    // Routine Control
    RoutineControl = 0x31,  ///< Start/stop/get results of routine

    // Upload/Download
    RequestDownload = 0x34,      ///< Prepare for download
    RequestUpload = 0x35,        ///< Prepare for upload
    TransferData = 0x36,         ///< Transfer data block
    RequestTransferExit = 0x37,  ///< End transfer
    RequestFileTransfer = 0x38,  ///< File operations

    // OEM-specific range: 0xBA-0xBE (manufacturer specific)
    // System supplier specific range: 0x00 (reserved), 0x80-0x82, 0x88-0x9F, 0xA0-0xB9, 0xBF
};

/// @brief Convert ServiceID to human-readable string
[[nodiscard]] constexpr std::string_view service_id_string(ServiceID sid) {
    switch (sid) {
        case ServiceID::DiagnosticSessionControl:
            return "DiagnosticSessionControl";
        case ServiceID::ECUReset:
            return "ECUReset";
        case ServiceID::SecurityAccess:
            return "SecurityAccess";
        case ServiceID::CommunicationControl:
            return "CommunicationControl";
        case ServiceID::TesterPresent:
            return "TesterPresent";
        case ServiceID::AccessTimingParameter:
            return "AccessTimingParameter";
        case ServiceID::SecuredDataTransmission:
            return "SecuredDataTransmission";
        case ServiceID::ControlDTCSetting:
            return "ControlDTCSetting";
        case ServiceID::ResponseOnEvent:
            return "ResponseOnEvent";
        case ServiceID::LinkControl:
            return "LinkControl";
        case ServiceID::ReadDataByIdentifier:
            return "ReadDataByIdentifier";
        case ServiceID::ReadMemoryByAddress:
            return "ReadMemoryByAddress";
        case ServiceID::ReadScalingDataByIdentifier:
            return "ReadScalingDataByIdentifier";
        case ServiceID::ReadDataByPeriodicIdentifier:
            return "ReadDataByPeriodicIdentifier";
        case ServiceID::DynamicallyDefineDataIdentifier:
            return "DynamicallyDefineDataIdentifier";
        case ServiceID::WriteDataByIdentifier:
            return "WriteDataByIdentifier";
        case ServiceID::WriteMemoryByAddress:
            return "WriteMemoryByAddress";
        case ServiceID::ClearDiagnosticInformation:
            return "ClearDiagnosticInformation";
        case ServiceID::ReadDTCInformation:
            return "ReadDTCInformation";
        case ServiceID::InputOutputControlByIdentifier:
            return "InputOutputControlByIdentifier";
        case ServiceID::RoutineControl:
            return "RoutineControl";
        case ServiceID::RequestDownload:
            return "RequestDownload";
        case ServiceID::RequestUpload:
            return "RequestUpload";
        case ServiceID::TransferData:
            return "TransferData";
        case ServiceID::RequestTransferExit:
            return "RequestTransferExit";
        case ServiceID::RequestFileTransfer:
            return "RequestFileTransfer";
        default:
            return "Unknown";
    }
}

/// @brief Check if a SID byte represents a response (bit 6 set)
[[nodiscard]] constexpr bool is_response_sid(std::uint8_t sid) {
    return (sid & 0x40) != 0 && sid != NEGATIVE_RESPONSE_SID;
}

/// @brief Check if a SID byte represents a negative response
[[nodiscard]] constexpr bool is_negative_response_sid(std::uint8_t sid) {
    return sid == NEGATIVE_RESPONSE_SID;
}

/// @brief Get the request SID from a response SID
[[nodiscard]] constexpr std::uint8_t response_to_request_sid(std::uint8_t response_sid) {
    return response_sid - RESPONSE_SID_OFFSET;
}

/// @brief Get the response SID from a request SID
[[nodiscard]] constexpr std::uint8_t request_to_response_sid(std::uint8_t request_sid) {
    return request_sid + RESPONSE_SID_OFFSET;
}

/// @brief Diagnostic Session Type
///
/// Defines the different diagnostic session types
enum class SessionType : std::uint8_t {
    DefaultSession = 0x01,                 ///< Default session (always available)
    ProgrammingSession = 0x02,             ///< ECU programming mode
    ExtendedDiagnosticSession = 0x03,      ///< Extended diagnostics
    SafetySystemDiagnosticSession = 0x04,  ///< Safety system mode
    // 0x05-0x3F: ISO reserved
    // 0x40-0x5F: Vehicle manufacturer specific
    // 0x60-0x7E: System supplier specific
    // 0x7F: Reserved by ISO
};

/// @brief Convert SessionType to human-readable string
[[nodiscard]] constexpr std::string_view session_type_string(SessionType type) {
    switch (type) {
        case SessionType::DefaultSession:
            return "DefaultSession";
        case SessionType::ProgrammingSession:
            return "ProgrammingSession";
        case SessionType::ExtendedDiagnosticSession:
            return "ExtendedDiagnosticSession";
        case SessionType::SafetySystemDiagnosticSession:
            return "SafetySystemDiagnosticSession";
        default:
            if (static_cast<std::uint8_t>(type) >= 0x40 &&
                static_cast<std::uint8_t>(type) <= 0x5F) {
                return "VehicleManufacturerSpecific";
            }
            if (static_cast<std::uint8_t>(type) >= 0x60 &&
                static_cast<std::uint8_t>(type) <= 0x7E) {
                return "SystemSupplierSpecific";
            }
            return "Reserved";
    }
}

/// @brief ECU Reset Type
enum class ResetType : std::uint8_t {
    HardReset = 0x01,                  ///< Complete hardware reset
    KeyOffOnReset = 0x02,              ///< Simulate key off/on
    SoftReset = 0x03,                  ///< Software reset
    EnableRapidPowerShutDown = 0x04,   ///< Enable rapid shutdown
    DisableRapidPowerShutDown = 0x05,  ///< Disable rapid shutdown
    // 0x06-0x3F: ISO reserved
    // 0x40-0x5F: Vehicle manufacturer specific
    // 0x60-0x7E: System supplier specific
};

/// @brief Convert ResetType to human-readable string
[[nodiscard]] constexpr std::string_view reset_type_string(ResetType type) {
    switch (type) {
        case ResetType::HardReset:
            return "HardReset";
        case ResetType::KeyOffOnReset:
            return "KeyOffOnReset";
        case ResetType::SoftReset:
            return "SoftReset";
        case ResetType::EnableRapidPowerShutDown:
            return "EnableRapidPowerShutDown";
        case ResetType::DisableRapidPowerShutDown:
            return "DisableRapidPowerShutDown";
        default:
            return "Unknown";
    }
}

/// @brief Security Access Type
///
/// Odd values are requestSeed, even values are sendKey
enum class SecurityAccessType : std::uint8_t {
    RequestSeed = 0x01,        ///< Request security seed (level 1)
    SendKey = 0x02,            ///< Send security key (level 1)
    RequestSeedLevel2 = 0x03,  ///< Request seed (level 2)
    SendKeyLevel2 = 0x04,      ///< Send key (level 2)
    // Pattern continues: odd=requestSeed, even=sendKey
    // 0x01-0x41: Security levels 1-33
    // 0x61-0x7E: Vehicle manufacturer specific
};

/// @brief Check if security access type is a seed request
[[nodiscard]] constexpr bool is_request_seed(std::uint8_t sub_function) {
    return (sub_function & 0x01) != 0;
}

/// @brief Get security level from security access sub-function
[[nodiscard]] constexpr std::uint8_t get_security_level(std::uint8_t sub_function) {
    return static_cast<std::uint8_t>((sub_function + 1) / 2);
}

/// @brief Routine Control Type
enum class RoutineControlType : std::uint8_t {
    StartRoutine = 0x01,           ///< Start routine execution
    StopRoutine = 0x02,            ///< Stop routine execution
    RequestRoutineResults = 0x03,  ///< Get routine results
};

/// @brief Convert RoutineControlType to human-readable string
[[nodiscard]] constexpr std::string_view routine_control_type_string(RoutineControlType type) {
    switch (type) {
        case RoutineControlType::StartRoutine:
            return "StartRoutine";
        case RoutineControlType::StopRoutine:
            return "StopRoutine";
        case RoutineControlType::RequestRoutineResults:
            return "RequestRoutineResults";
        default:
            return "Unknown";
    }
}

/// @brief Communication Control Type
enum class CommunicationControlType : std::uint8_t {
    EnableRxAndTx = 0x00,         ///< Enable all communication
    EnableRxAndDisableTx = 0x01,  ///< Enable RX, disable TX
    DisableRxAndEnableTx = 0x02,  ///< Disable RX, enable TX
    DisableRxAndTx = 0x03,        ///< Disable all communication
};

/// @brief Convert CommunicationControlType to human-readable string
[[nodiscard]] constexpr std::string_view communication_control_type_string(
    CommunicationControlType type) {
    switch (type) {
        case CommunicationControlType::EnableRxAndTx:
            return "EnableRxAndTx";
        case CommunicationControlType::EnableRxAndDisableTx:
            return "EnableRxAndDisableTx";
        case CommunicationControlType::DisableRxAndEnableTx:
            return "DisableRxAndEnableTx";
        case CommunicationControlType::DisableRxAndTx:
            return "DisableRxAndTx";
        default:
            return "Unknown";
    }
}

/// @brief Communication Type (bit field for CommunicationControl)
struct CommunicationType {
    bool normal_communication = true;  ///< Normal communication messages
    bool network_management = false;   ///< Network management communication

    /// @brief Parse from byte
    static CommunicationType from_byte(std::uint8_t byte) {
        CommunicationType ct;
        ct.normal_communication = (byte & 0x01) != 0;
        ct.network_management = (byte & 0x02) != 0;
        return ct;
    }

    /// @brief Convert to byte
    [[nodiscard]] std::uint8_t to_byte() const {
        std::uint8_t byte = 0;
        if (normal_communication)
            byte |= 0x01;
        if (network_management)
            byte |= 0x02;
        return byte;
    }
};

/// @brief Control DTC Setting Type
enum class ControlDTCSettingType : std::uint8_t {
    On = 0x01,   ///< Enable DTC setting
    Off = 0x02,  ///< Disable DTC setting
    // 0x03-0x3F: ISO reserved
    // 0x40-0x5F: Vehicle manufacturer specific
    // 0x60-0x7E: System supplier specific
};

/// @brief Input/Output Control Parameter
enum class IOControlParameter : std::uint8_t {
    ReturnControlToECU = 0x00,   ///< Return control to ECU
    ResetToDefault = 0x01,       ///< Reset to default values
    FreezeCurrentState = 0x02,   ///< Freeze current state
    ShortTermAdjustment = 0x03,  ///< Temporary adjustment
    // 0x04-0xFF: Reserved or control state
};

/// @brief Convert IOControlParameter to human-readable string
[[nodiscard]] constexpr std::string_view io_control_parameter_string(IOControlParameter param) {
    switch (param) {
        case IOControlParameter::ReturnControlToECU:
            return "ReturnControlToECU";
        case IOControlParameter::ResetToDefault:
            return "ResetToDefault";
        case IOControlParameter::FreezeCurrentState:
            return "FreezeCurrentState";
        case IOControlParameter::ShortTermAdjustment:
            return "ShortTermAdjustment";
        default:
            return "ControlState";
    }
}

/// @brief ReadDTCInformation sub-function
enum class ReadDTCSubFunction : std::uint8_t {
    ReportNumberOfDTCByStatusMask = 0x01,
    ReportDTCByStatusMask = 0x02,
    ReportDTCSnapshotIdentification = 0x03,
    ReportDTCSnapshotRecordByDTCNumber = 0x04,
    ReportDTCStoredDataByRecordNumber = 0x05,
    ReportDTCExtDataRecordByDTCNumber = 0x06,
    ReportNumberOfDTCBySeverityMaskRecord = 0x07,
    ReportDTCBySeverityMaskRecord = 0x08,
    ReportSeverityInformationOfDTC = 0x09,
    ReportSupportedDTC = 0x0A,
    ReportFirstTestFailedDTC = 0x0B,
    ReportFirstConfirmedDTC = 0x0C,
    ReportMostRecentTestFailedDTC = 0x0D,
    ReportMostRecentConfirmedDTC = 0x0E,
    ReportMirrorMemoryDTCByStatusMask = 0x0F,
    ReportMirrorMemoryDTCExtDataRecordByDTCNumber = 0x10,
    ReportNumberOfMirrorMemoryDTCByStatusMask = 0x11,
    ReportNumberOfEmissionsOBDDTCByStatusMask = 0x12,
    ReportEmissionsOBDDTCByStatusMask = 0x13,
    ReportDTCFaultDetectionCounter = 0x14,
    ReportDTCWithPermanentStatus = 0x15,
    ReportDTCExtDataRecordByRecordNumber = 0x16,
    ReportUserDefMemoryDTCByStatusMask = 0x17,
    ReportUserDefMemoryDTCSnapshotRecordByDTCNumber = 0x18,
    ReportUserDefMemoryDTCExtDataRecordByDTCNumber = 0x19,
    ReportWWHOBDDTCByMaskRecord = 0x42,
    ReportWWHOBDDTCWithPermanentStatus = 0x55,
};

/// @brief Data Identifier (DID) - 16-bit identifier for data elements
///
/// Common DIDs defined in ISO 14229-1 Annex C
struct DataIdentifier {
    std::uint16_t value = 0;

    DataIdentifier() = default;
    explicit constexpr DataIdentifier(std::uint16_t v) : value(v) {}

    bool operator==(const DataIdentifier& other) const { return value == other.value; }
    bool operator!=(const DataIdentifier& other) const { return value != other.value; }
    bool operator<(const DataIdentifier& other) const { return value < other.value; }

    /// @brief Check if DID is in OEM-specific range
    [[nodiscard]] constexpr bool is_oem_specific() const {
        return (value >= 0xF100 && value <= 0xF17F) ||  // Vehicle manufacturer specific
               (value >= 0xF180 && value <= 0xF1FF);    // System supplier specific
    }

    /// @brief Check if DID is a standard identification DID
    [[nodiscard]] constexpr bool is_identification() const {
        return value >= 0xF180 && value <= 0xF19F;
    }
};

/// @brief Common Data Identifiers
namespace DID {
/// @brief Vehicle Identification Number
inline constexpr DataIdentifier VIN{0xF190};
/// @brief ECU Manufacturing Date
inline constexpr DataIdentifier ManufacturingDate{0xF18B};
/// @brief ECU Serial Number
inline constexpr DataIdentifier ECUSerialNumber{0xF18C};
/// @brief ECU Hardware Version
inline constexpr DataIdentifier ECUHardwareVersion{0xF191};
/// @brief ECU Software Version
inline constexpr DataIdentifier ECUSoftwareVersion{0xF195};
/// @brief System Supplier ECU Hardware Number
inline constexpr DataIdentifier SystemSupplierECUHardwareNumber{0xF192};
/// @brief System Supplier ECU Software Number
inline constexpr DataIdentifier SystemSupplierECUSoftwareNumber{0xF194};
/// @brief Boot Software Identification
inline constexpr DataIdentifier BootSoftwareIdentification{0xF183};
/// @brief Application Software Identification
inline constexpr DataIdentifier ApplicationSoftwareIdentification{0xF181};
/// @brief Application Data Identification
inline constexpr DataIdentifier ApplicationDataIdentification{0xF182};
/// @brief Active Diagnostic Session
inline constexpr DataIdentifier ActiveDiagnosticSession{0xF186};
/// @brief Vehicle Manufacturer Spare Part Number
inline constexpr DataIdentifier VehicleManufacturerSparePartNumber{0xF187};
/// @brief Vehicle Manufacturer ECU Software Number
inline constexpr DataIdentifier VehicleManufacturerECUSoftwareNumber{0xF188};
/// @brief Vehicle Manufacturer ECU Software Version Number
inline constexpr DataIdentifier VehicleManufacturerECUSoftwareVersionNumber{0xF189};
/// @brief System Name or Engine Type
inline constexpr DataIdentifier SystemNameOrEngineType{0xF197};
}  // namespace DID

/// @brief Routine Identifier - 16-bit identifier for routines
struct RoutineIdentifier {
    std::uint16_t value = 0;

    RoutineIdentifier() = default;
    explicit constexpr RoutineIdentifier(std::uint16_t v) : value(v) {}

    bool operator==(const RoutineIdentifier& other) const { return value == other.value; }
    bool operator!=(const RoutineIdentifier& other) const { return value != other.value; }
    bool operator<(const RoutineIdentifier& other) const { return value < other.value; }

    /// @brief Check if routine ID is in OEM-specific range
    [[nodiscard]] constexpr bool is_oem_specific() const { return value >= 0xF000; }
};

/// @brief Common Routine Identifiers
namespace RoutineID {
/// @brief Erase Memory
inline constexpr RoutineIdentifier EraseMemory{0xFF00};
/// @brief Check Programming Dependencies
inline constexpr RoutineIdentifier CheckProgrammingDependencies{0xFF01};
/// @brief Erase Mirror Memory DTCs
inline constexpr RoutineIdentifier EraseMirrorMemoryDTCs{0xFF02};
}  // namespace RoutineID

/// @brief DTC (Diagnostic Trouble Code) - 24-bit code
struct DTC {
    std::uint8_t high_byte = 0;    ///< DTC high byte (category)
    std::uint8_t middle_byte = 0;  ///< DTC middle byte (component)
    std::uint8_t low_byte = 0;     ///< DTC low byte (failure type)

    DTC() = default;
    DTC(std::uint8_t h, std::uint8_t m, std::uint8_t l)
        : high_byte(h), middle_byte(m), low_byte(l) {}

    /// @brief Create DTC from 24-bit value
    static DTC from_value(std::uint32_t value) {
        return DTC(static_cast<std::uint8_t>((value >> 16) & 0xFF),
                   static_cast<std::uint8_t>((value >> 8) & 0xFF),
                   static_cast<std::uint8_t>(value & 0xFF));
    }

    /// @brief Convert to 24-bit value
    [[nodiscard]] std::uint32_t to_value() const {
        return (static_cast<std::uint32_t>(high_byte) << 16) |
               (static_cast<std::uint32_t>(middle_byte) << 8) |
               static_cast<std::uint32_t>(low_byte);
    }

    /// @brief Convert to string representation (e.g., "P0123")
    [[nodiscard]] std::string to_string() const {
        char buf[6];
        char prefix;

        switch ((high_byte >> 4) & 0x0F) {
            case 0x0:
            case 0x1:
            case 0x2:
            case 0x3:
                prefix = 'P';  // Powertrain
                break;
            case 0x4:
            case 0x5:
                prefix = 'C';  // Chassis
                break;
            case 0x6:
            case 0x7:
                prefix = 'B';  // Body
                break;
            case 0x8:
            case 0x9:
            case 0xA:
            case 0xB:
            case 0xC:
            case 0xD:
            case 0xE:
            case 0xF:
                prefix = 'U';  // Network
                break;
            default:
                prefix = '?';
        }

        std::uint16_t code = ((high_byte & 0x3F) << 8) | middle_byte;
        std::snprintf(buf, sizeof(buf), "%c%04X", prefix, code);
        return buf;
    }

    bool operator==(const DTC& other) const {
        return high_byte == other.high_byte && middle_byte == other.middle_byte &&
               low_byte == other.low_byte;
    }
};

/// @brief DTC Status Mask
struct DTCStatusMask {
    bool test_failed = false;                          ///< Bit 0: testFailed
    bool test_failed_this_operation_cycle = false;     ///< Bit 1: testFailedThisOperationCycle
    bool pending_dtc = false;                          ///< Bit 2: pendingDTC
    bool confirmed_dtc = false;                        ///< Bit 3: confirmedDTC
    bool test_not_completed_since_last_clear = false;  ///< Bit 4: testNotCompletedSinceLastClear
    bool test_failed_since_last_clear = false;         ///< Bit 5: testFailedSinceLastClear
    bool test_not_completed_this_operation_cycle =
        false;                                 ///< Bit 6: testNotCompletedThisOperationCycle
    bool warning_indicator_requested = false;  ///< Bit 7: warningIndicatorRequested

    /// @brief Parse from byte
    static DTCStatusMask from_byte(std::uint8_t byte) {
        DTCStatusMask mask;
        mask.test_failed = (byte & 0x01) != 0;
        mask.test_failed_this_operation_cycle = (byte & 0x02) != 0;
        mask.pending_dtc = (byte & 0x04) != 0;
        mask.confirmed_dtc = (byte & 0x08) != 0;
        mask.test_not_completed_since_last_clear = (byte & 0x10) != 0;
        mask.test_failed_since_last_clear = (byte & 0x20) != 0;
        mask.test_not_completed_this_operation_cycle = (byte & 0x40) != 0;
        mask.warning_indicator_requested = (byte & 0x80) != 0;
        return mask;
    }

    /// @brief Convert to byte
    [[nodiscard]] std::uint8_t to_byte() const {
        std::uint8_t byte = 0;
        if (test_failed)
            byte |= 0x01;
        if (test_failed_this_operation_cycle)
            byte |= 0x02;
        if (pending_dtc)
            byte |= 0x04;
        if (confirmed_dtc)
            byte |= 0x08;
        if (test_not_completed_since_last_clear)
            byte |= 0x10;
        if (test_failed_since_last_clear)
            byte |= 0x20;
        if (test_not_completed_this_operation_cycle)
            byte |= 0x40;
        if (warning_indicator_requested)
            byte |= 0x80;
        return byte;
    }
};

/// @brief Address and Length Format Identifier for memory operations
struct AddressAndLengthFormatIdentifier {
    std::uint8_t memory_size_length = 4;     ///< Number of bytes for memory size (high nibble)
    std::uint8_t memory_address_length = 4;  ///< Number of bytes for memory address (low nibble)

    AddressAndLengthFormatIdentifier() = default;

    /// @brief Parse from byte
    static AddressAndLengthFormatIdentifier from_byte(std::uint8_t byte) {
        AddressAndLengthFormatIdentifier fmt;
        fmt.memory_size_length = (byte >> 4) & 0x0F;
        fmt.memory_address_length = byte & 0x0F;
        return fmt;
    }

    /// @brief Convert to byte
    [[nodiscard]] std::uint8_t to_byte() const {
        return ((memory_size_length & 0x0F) << 4) | (memory_address_length & 0x0F);
    }
};

/// @brief Data Format Identifier for download/upload operations
struct DataFormatIdentifier {
    std::uint8_t compression_method = 0;  ///< Compression method (high nibble)
    std::uint8_t encrypting_method = 0;   ///< Encryption method (low nibble)

    DataFormatIdentifier() = default;

    /// @brief Parse from byte
    static DataFormatIdentifier from_byte(std::uint8_t byte) {
        DataFormatIdentifier fmt;
        fmt.compression_method = (byte >> 4) & 0x0F;
        fmt.encrypting_method = byte & 0x0F;
        return fmt;
    }

    /// @brief Convert to byte
    [[nodiscard]] std::uint8_t to_byte() const {
        return ((compression_method & 0x0F) << 4) | (encrypting_method & 0x0F);
    }

    /// @brief Check if no compression is used
    [[nodiscard]] bool is_uncompressed() const { return compression_method == 0; }

    /// @brief Check if no encryption is used
    [[nodiscard]] bool is_unencrypted() const { return encrypting_method == 0; }
};

/// @brief UDS Message Direction
enum class MessageDirection {
    Request,           ///< Tester to ECU
    PositiveResponse,  ///< ECU positive response to tester
    NegativeResponse,  ///< ECU negative response to tester
};

/// @brief UDS Message Header (decoded)
///
/// This structure represents a decoded UDS message with its key components.
/// The actual wire format varies by transport layer (DoIP, CAN, etc.)
struct UdsHeader : public IDecodedHeader {
    ServiceID service_id = ServiceID::TesterPresent;
    MessageDirection direction = MessageDirection::Request;

    /// @brief For requests/positive responses: sub-function if present
    std::optional<std::uint8_t> sub_function;

    /// @brief Suppress positive response bit (from sub-function)
    bool suppress_positive_response = false;

    /// @brief For negative responses: the rejected service ID
    std::optional<ServiceID> rejected_service_id;

    /// @brief For negative responses: the NRC (value stored, use nrc.hpp for interpretation)
    std::optional<std::uint8_t> negative_response_code;

    /// @brief Service-specific data (after SID and sub-function)
    std::span<const std::byte> service_data;

    /// @brief Original raw message data
    std::span<const std::byte> raw_data;

    /// @brief Total message length including SID
    [[nodiscard]] std::size_t total_length() const { return raw_data.size(); }

    /// @brief Check if this is a request
    [[nodiscard]] bool is_request() const { return direction == MessageDirection::Request; }

    /// @brief Check if this is a positive response
    [[nodiscard]] bool is_positive_response() const {
        return direction == MessageDirection::PositiveResponse;
    }

    /// @brief Check if this is a negative response
    [[nodiscard]] bool is_negative_response() const {
        return direction == MessageDirection::NegativeResponse;
    }

    /// @brief Get protocol type
    [[nodiscard]] std::string_view protocol_name() const override { return "UDS"; }

    /// @brief Get header size (varies by service)
    [[nodiscard]] std::size_t header_size() const override {
        if (direction == MessageDirection::NegativeResponse) {
            return 3;  // 0x7F + rejected SID + NRC
        }
        return sub_function ? 2 : 1;  // SID [+ sub-function]
    }

    /// @brief Check validity
    [[nodiscard]] bool is_valid() const { return raw_data.size() >= MIN_MESSAGE_SIZE; }

    /// @brief Get payload size (service data)
    [[nodiscard]] std::size_t payload_size() const override { return service_data.size(); }

    /// @brief Generate string representation
    [[nodiscard]] std::string to_string() const override;
};

/// @brief Inline implementation of UdsHeader::to_string()
inline std::string UdsHeader::to_string() const {
    std::string result;
    result.reserve(64);

    switch (direction) {
        case MessageDirection::Request:
            result = "UDS Request: ";
            break;
        case MessageDirection::PositiveResponse:
            result = "UDS Response: ";
            break;
        case MessageDirection::NegativeResponse:
            result = "UDS Negative Response: ";
            if (rejected_service_id) {
                result += service_id_string(*rejected_service_id);
                result += " -> NRC=";
                if (negative_response_code) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "0x%02X", *negative_response_code);
                    result += buf;
                }
            }
            return result;
    }

    result += service_id_string(service_id);

    if (sub_function) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), " [0x%02X]", *sub_function);
        result += buf;
        if (suppress_positive_response) {
            result += " (suppressPosRsp)";
        }
    }

    return result;
}

}  // namespace wadjet::protocols::uds

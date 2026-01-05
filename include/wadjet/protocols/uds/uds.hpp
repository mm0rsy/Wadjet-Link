#pragma once

/// @file uds.hpp
/// @brief UDS (ISO 14229) protocol decoder
///
/// This file provides the main UDS header structure and decoder implementation
/// for Unified Diagnostic Services used in automotive diagnostics.
///
/// @uds UDS is the standard diagnostic protocol for automotive ECUs as defined
/// in ISO 14229-1. It provides services for diagnostic session control,
/// security access, data reading/writing, DTC management, and more.
///
/// ## Protocol Overview
///
/// UDS uses a request/response model:
/// - **Request**: Service ID (SID) + optional sub-function + parameters
/// - **Positive Response**: SID + 0x40 + echo of sub-function + response data
/// - **Negative Response**: 0x7F + rejected SID + NRC (Negative Response Code)
///
/// ## Transport Layer
///
/// UDS can be transported over:
/// - DoIP (ISO 13400) - Diagnostics over IP
/// - CAN (ISO 15765) - With ISO-TP segmentation
/// - Direct TCP/UDP connections
///
/// ## Example Usage
///
/// ```cpp
/// #include <wadjet/protocols/uds/uds.hpp>
///
/// using namespace wadjet::protocols::uds;
///
/// // Decode a UDS message
/// UdsDecoder decoder;
/// auto result = decoder.decode(uds_payload);
///
/// if (result) {
///     const auto& header = result.value().header;
///     std::cout << "Service: " << service_id_string(header.service_id) << "\n";
///     if (header.is_negative_response()) {
///         std::cout << "NRC: " << nrc_string(*header.negative_response_code) << "\n";
///     }
/// }
/// ```
///
/// ## Common Services
///
/// | Service | SID | Description |
/// |---------|-----|-------------|
/// | DiagnosticSessionControl | 0x10 | Start/change diagnostic session |
/// | ECUReset | 0x11 | Reset the ECU |
/// | SecurityAccess | 0x27 | Unlock secured operations |
/// | TesterPresent | 0x3E | Keep session alive |
/// | ReadDataByIdentifier | 0x22 | Read data by DID |
/// | WriteDataByIdentifier | 0x2E | Write data by DID |
/// | RoutineControl | 0x31 | Execute routines |
/// | RequestDownload | 0x34 | Begin flash programming |
/// | TransferData | 0x36 | Transfer data blocks |

#include "wadjet/core/result.hpp"
#include "wadjet/protocols/decoder.hpp"
#include "wadjet/protocols/uds/uds_nrc.hpp"
#include "wadjet/protocols/uds/uds_services.hpp"
#include "wadjet/protocols/uds/uds_session.hpp"
#include "wadjet/protocols/uds/uds_types.hpp"

#include <optional>
#include <span>
#include <string>

namespace wadjet::protocols::uds {

/// @brief UDS decode result containing header and parsed service message
struct UdsDecodeResult {
    UdsHeader header;
    UdsServiceMessage message;

    /// @brief Get the service message as a specific type
    template <typename T>
    [[nodiscard]] const T* as() const {
        return std::get_if<T>(&message);
    }

    /// @brief Check if the message is of a specific type
    template <typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(message);
    }
};

/// @brief UDS decoder error type
struct UdsDecodeError {
    enum class Code {
        MessageTooShort,       ///< Message shorter than minimum size
        InvalidServiceId,      ///< Unknown service ID
        InvalidSubFunction,    ///< Invalid sub-function for service
        InvalidMessageLength,  ///< Message length doesn't match service
        MalformedData,         ///< Data cannot be parsed
    };

    Code code;
    std::string message;

    /// @brief Create an error
    static UdsDecodeError make(Code c, std::string msg = {}) { return {c, std::move(msg)}; }
};

/// @brief UDS message decoder
///
/// Decodes UDS messages from raw byte data. The decoder handles:
/// - Request/response differentiation (SID bit 6)
/// - Negative response parsing (0x7F)
/// - Sub-function extraction with suppress positive response bit
/// - Service-specific parameter parsing
class UdsDecoder {
public:
    /// @brief Decode result type
    using Result = wadjet::Result<UdsDecodeResult, UdsDecodeError>;

    /// @brief Decode a UDS message
    ///
    /// @param data Raw UDS message data (starting with SID)
    /// @return Decoded result or error
    [[nodiscard]] Result decode(std::span<const std::byte> data) const;

    /// @brief Decode UDS message from uint8_t span
    [[nodiscard]] Result decode(std::span<const std::uint8_t> data) const {
        return decode(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()),
                                                 data.size()));
    }

    /// @brief Quick check if data looks like a valid UDS message
    [[nodiscard]] static bool looks_like_uds(std::span<const std::byte> data);

    /// @brief Check if data is a request (SID without bit 6 set)
    [[nodiscard]] static bool is_request(std::span<const std::byte> data);

    /// @brief Check if data is a positive response (SID with bit 6 set)
    [[nodiscard]] static bool is_positive_response(std::span<const std::byte> data);

    /// @brief Check if data is a negative response (starts with 0x7F)
    [[nodiscard]] static bool is_negative_response(std::span<const std::byte> data);

private:
    /// @brief Parse a request message
    [[nodiscard]] Result parse_request(std::span<const std::byte> data) const;

    /// @brief Parse a positive response
    [[nodiscard]] Result parse_positive_response(std::span<const std::byte> data) const;

    /// @brief Parse a negative response
    [[nodiscard]] Result parse_negative_response(std::span<const std::byte> data) const;

    // Service-specific parsers
    [[nodiscard]] UdsServiceMessage parse_diagnostic_session_control_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_diagnostic_session_control_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_ecu_reset_request(std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_ecu_reset_response(std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_security_access_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_security_access_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_tester_present_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_tester_present_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_read_data_by_identifier_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_read_data_by_identifier_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_write_data_by_identifier_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_write_data_by_identifier_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_routine_control_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_routine_control_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_request_download_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_request_download_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_transfer_data_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_transfer_data_response(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_request_transfer_exit_request(
        std::span<const std::byte> data) const;
    [[nodiscard]] UdsServiceMessage parse_request_transfer_exit_response(
        std::span<const std::byte> data) const;
};

// =============================================================================
// Inline implementations
// =============================================================================

inline bool UdsDecoder::looks_like_uds(std::span<const std::byte> data) {
    if (data.empty())
        return false;

    auto sid = static_cast<std::uint8_t>(data[0]);

    // Check for negative response
    if (sid == NEGATIVE_RESPONSE_SID) {
        return data.size() >= 3;
    }

    // Check if SID is in valid range (request or response)
    auto base_sid = sid & ~static_cast<std::uint8_t>(RESPONSE_SID_OFFSET);

    // Check against known service IDs
    switch (static_cast<ServiceID>(base_sid)) {
        case ServiceID::DiagnosticSessionControl:
        case ServiceID::ECUReset:
        case ServiceID::SecurityAccess:
        case ServiceID::CommunicationControl:
        case ServiceID::TesterPresent:
        case ServiceID::AccessTimingParameter:
        case ServiceID::SecuredDataTransmission:
        case ServiceID::ControlDTCSetting:
        case ServiceID::ResponseOnEvent:
        case ServiceID::LinkControl:
        case ServiceID::ReadDataByIdentifier:
        case ServiceID::ReadMemoryByAddress:
        case ServiceID::ReadScalingDataByIdentifier:
        case ServiceID::ReadDataByPeriodicIdentifier:
        case ServiceID::DynamicallyDefineDataIdentifier:
        case ServiceID::WriteDataByIdentifier:
        case ServiceID::WriteMemoryByAddress:
        case ServiceID::ClearDiagnosticInformation:
        case ServiceID::ReadDTCInformation:
        case ServiceID::InputOutputControlByIdentifier:
        case ServiceID::RoutineControl:
        case ServiceID::RequestDownload:
        case ServiceID::RequestUpload:
        case ServiceID::TransferData:
        case ServiceID::RequestTransferExit:
        case ServiceID::RequestFileTransfer:
            return true;
        default:
            return false;
    }
}

inline bool UdsDecoder::is_request(std::span<const std::byte> data) {
    if (data.empty())
        return false;
    auto sid = static_cast<std::uint8_t>(data[0]);
    return sid != NEGATIVE_RESPONSE_SID && (sid & 0x40) == 0;
}

inline bool UdsDecoder::is_positive_response(std::span<const std::byte> data) {
    if (data.empty())
        return false;
    auto sid = static_cast<std::uint8_t>(data[0]);
    return sid != NEGATIVE_RESPONSE_SID && (sid & 0x40) != 0;
}

inline bool UdsDecoder::is_negative_response(std::span<const std::byte> data) {
    if (data.empty())
        return false;
    return static_cast<std::uint8_t>(data[0]) == NEGATIVE_RESPONSE_SID;
}

}  // namespace wadjet::protocols::uds

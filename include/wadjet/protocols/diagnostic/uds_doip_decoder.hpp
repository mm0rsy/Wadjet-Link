#pragma once

/// @file uds_doip_decoder.hpp
/// @brief Combined UDS-over-DoIP decoder
///
/// Extracts and decodes UDS messages from DoIP diagnostic message payloads.
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_types.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/uds/uds.hpp"

#include <optional>
#include <span>

namespace wadjet::protocols::diagnostic {

/// @brief Result of decoding UDS from DoIP
struct UdsOverDoipResult {
    // DoIP layer
    doip::DoIPHeader doip_header;
    LogicalAddress source_address{0};
    LogicalAddress target_address{0};

    // UDS layer
    uds::UdsHeader uds_header;
    uds::UdsServiceMessage uds_message;

    // Message direction
    MessageDirection direction{MessageDirection::Unknown};

    /// @brief Get the UDS service message as a specific type
    template <typename T>
    [[nodiscard]] const T* as() const {
        return std::get_if<T>(&uds_message);
    }

    /// @brief Check if the UDS message is of a specific type
    template <typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(uds_message);
    }

    /// @brief Check if this is a positive UDS response
    [[nodiscard]] bool is_positive_response() const {
        return !uds_header.is_negative_response();
    }

    /// @brief Check if this is a negative UDS response
    [[nodiscard]] bool is_negative_response() const {
        return uds_header.is_negative_response();
    }

    /// @brief Get the UDS service ID
    [[nodiscard]] uds::ServiceID service_id() const {
        return uds_header.service_id;
    }
};

/// @brief Error when decoding UDS over DoIP
struct UdsOverDoipError {
    enum class Code {
        DoipDecodeFailed,       ///< Failed to decode DoIP header
        NotDiagnosticMessage,   ///< DoIP message is not a diagnostic message
        PayloadTooShort,        ///< Payload too short for UDS
        UdsDecodeFailed,        ///< Failed to decode UDS message
        InvalidVersion,         ///< Invalid DoIP version
    };

    Code code;
    std::string message;

    [[nodiscard]] static UdsOverDoipError make(Code c, std::string msg = {}) {
        return {c, std::move(msg)};
    }
};

/// @brief Combined UDS-over-DoIP decoder
///
/// Decodes DoIP packets and extracts the embedded UDS messages,
/// providing a single decode result with both layers.
///
/// @example
/// ```cpp
/// UdsOverDoipDecoder decoder;
///
/// // Decode from raw DoIP data
/// auto result = decoder.decode(doip_packet_data);
/// if (result) {
///     std::cout << "Service: " << service_id_string(result->service_id()) << "\n";
///     std::cout << "Source: 0x" << std::hex << result->source_address << "\n";
///     std::cout << "Target: 0x" << std::hex << result->target_address << "\n";
/// }
/// ```
class UdsOverDoipDecoder {
public:
    using Result = wadjet::Result<UdsOverDoipResult, UdsOverDoipError>;

    /// @brief Decode UDS from raw DoIP packet data
    /// @param data Raw DoIP packet (header + payload)
    /// @return Decode result or error
    [[nodiscard]] Result decode(std::span<const std::byte> data) const;

    /// @brief Decode UDS from a parsed DoIP header and payload
    /// @param header Decoded DoIP header
    /// @param payload DoIP payload data
    /// @return Decode result or error
    [[nodiscard]] Result decode(const doip::DoIPHeader& header,
                                std::span<const std::byte> payload) const;

    /// @brief Quick check if DoIP packet contains a diagnostic message
    [[nodiscard]] static bool is_diagnostic_message(const doip::DoIPHeader& header);

    /// @brief Quick check if raw data looks like DoIP diagnostic message
    [[nodiscard]] static bool looks_like_diagnostic_message(std::span<const std::byte> data);

private:
    doip::DoIPDecoder doip_decoder_;
    uds::UdsDecoder uds_decoder_;
};

/// @brief Global UDS-over-DoIP decoder instance
inline const UdsOverDoipDecoder& uds_over_doip_decoder() {
    static UdsOverDoipDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::diagnostic

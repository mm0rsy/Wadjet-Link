#pragma once

/// @file doip.hpp
/// @brief DoIP (Diagnostics over IP) protocol decoder - ISO 13400-2

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::protocols::doip {

/// @brief DoIP header size
inline constexpr std::size_t HEADER_SIZE = 8;

/// @brief DoIP protocol version (ISO 13400-2:2012)
inline constexpr std::uint8_t PROTOCOL_VERSION_2012 = 0x02;

/// @brief DoIP protocol version (ISO 13400-2:2019)
inline constexpr std::uint8_t PROTOCOL_VERSION_2019 = 0x03;

/// @brief Default DoIP protocol version for detection
inline constexpr std::uint8_t PROTOCOL_VERSION_DEFAULT = 0xFF;

/// @brief DoIP payload types
enum class PayloadType : std::uint16_t {
    // Generic DoIP header negative acknowledge
    GenericNack = 0x0000,

    // Vehicle identification
    VehicleIdentificationRequest = 0x0001,
    VehicleIdentificationRequestWithEID = 0x0002,
    VehicleIdentificationRequestWithVIN = 0x0003,
    VehicleAnnouncementOrIdentificationResponse = 0x0004,

    // Routing activation
    RoutingActivationRequest = 0x0005,
    RoutingActivationResponse = 0x0006,

    // Alive check
    AliveCheckRequest = 0x0007,
    AliveCheckResponse = 0x0008,

    // DoIP entity status
    DoIPEntityStatusRequest = 0x4001,
    DoIPEntityStatusResponse = 0x4002,

    // Diagnostic power mode
    DiagnosticPowerModeRequest = 0x4003,
    DiagnosticPowerModeResponse = 0x4004,

    // Diagnostic message
    DiagnosticMessage = 0x8001,
    DiagnosticMessagePositiveAck = 0x8002,
    DiagnosticMessageNegativeAck = 0x8003,
};

/// @brief Convert payload type to string
[[nodiscard]] std::string_view payload_type_string(PayloadType type);

/// @brief Generic NACK codes
enum class NackCode : std::uint8_t {
    IncorrectPatternFormat = 0x00,
    UnknownPayloadType = 0x01,
    MessageTooLarge = 0x02,
    OutOfMemory = 0x03,
    InvalidPayloadLength = 0x04,
};

/// @brief Routing activation response codes
enum class RoutingActivationResponseCode : std::uint8_t {
    UnknownSourceAddress = 0x00,
    NoSocketsAvailable = 0x01,
    DifferentSourceAddress = 0x02,
    AlreadyActive = 0x03,
    MissingAuthentication = 0x04,
    ConfirmationRejected = 0x05,
    UnsupportedActivationType = 0x06,
    TLSRequired = 0x07,
    // 0x08-0x0F reserved
    SuccessfullyActivated = 0x10,
    ActivationRequiresConfirmation = 0x11,
};

/// @brief Diagnostic message ACK codes
enum class DiagnosticAckCode : std::uint8_t {
    Acknowledged = 0x00,
};

/// @brief Diagnostic message NACK codes
enum class DiagnosticNackCode : std::uint8_t {
    InvalidSourceAddress = 0x02,
    UnknownTargetAddress = 0x03,
    DiagnosticMessageTooLarge = 0x04,
    OutOfMemory = 0x05,
    TargetUnreachable = 0x06,
    UnknownNetwork = 0x07,
    TransportProtocolError = 0x08,
};

/// @brief VIN (Vehicle Identification Number) - 17 characters
using VIN = std::array<char, 17>;

/// @brief EID (Entity ID) - 6 bytes (usually MAC address)
using EID = std::array<std::uint8_t, 6>;

/// @brief GID (Group ID) - 6 bytes
using GID = std::array<std::uint8_t, 6>;

/// @brief Decoded DoIP header
struct DoIPHeader : public IDecodedHeader {
    std::uint8_t protocol_version = 0;          ///< Protocol version
    std::uint8_t inverse_protocol_version = 0;  ///< Inverse of protocol version
    PayloadType payload_type = PayloadType::GenericNack;
    std::uint32_t payload_length = 0;

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "DoIP"; }

    [[nodiscard]] std::size_t header_size() const override { return HEADER_SIZE; }

    [[nodiscard]] std::size_t payload_size() const override { return payload_length; }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Validate protocol version (version XOR inverse = 0xFF)
    [[nodiscard]] bool is_version_valid() const {
        return (protocol_version ^ inverse_protocol_version) == 0xFF;
    }

    /// @brief Check if this is a diagnostic message
    [[nodiscard]] bool is_diagnostic_message() const {
        return payload_type == PayloadType::DiagnosticMessage;
    }

    /// @brief Check if this is a routing activation message
    [[nodiscard]] bool is_routing_activation() const {
        return payload_type == PayloadType::RoutingActivationRequest ||
               payload_type == PayloadType::RoutingActivationResponse;
    }

    /// @brief Check if this is a vehicle identification message
    [[nodiscard]] bool is_vehicle_identification() const {
        auto type_val = static_cast<std::uint16_t>(payload_type);
        return type_val >= 0x0001 && type_val <= 0x0004;
    }
};

/// @brief Routing activation request payload
struct RoutingActivationRequest {
    std::uint16_t source_address = 0;           ///< External tester address
    std::uint8_t activation_type = 0;           ///< Activation type
    std::uint32_t reserved = 0;                 ///< Reserved (ISO 13400-2)
    std::optional<std::uint32_t> oem_specific;  ///< OEM-specific data (if present)
};

/// @brief Routing activation response payload
struct RoutingActivationResponse {
    std::uint16_t logical_address = 0;  ///< Tester logical address
    std::uint16_t entity_address = 0;   ///< DoIP entity logical address
    RoutingActivationResponseCode response_code =
        RoutingActivationResponseCode::UnknownSourceAddress;
    std::uint32_t reserved = 0;
    std::optional<std::uint32_t> oem_specific;
};

/// @brief Diagnostic message payload
struct DiagnosticMessagePayload {
    std::uint16_t source_address = 0;      ///< Source logical address
    std::uint16_t target_address = 0;      ///< Target logical address
    std::span<const std::byte> user_data;  ///< UDS/diagnostic data
};

/// @brief Vehicle announcement/identification response
struct VehicleIdentificationResponse {
    VIN vin{};                          ///< Vehicle Identification Number
    std::uint16_t logical_address = 0;  ///< Logical address of DoIP entity
    EID eid{};                          ///< Entity ID (usually MAC)
    GID gid{};                          ///< Group ID
    std::uint8_t further_action = 0;    ///< Further action required
    std::uint8_t sync_status = 0;       ///< VIN/GID sync status (optional)
};

/// @brief DoIP decoder
class DoIPDecoder : public DecoderBase<DoIPDecoder, DoIPHeader> {
public:
    /// @brief Decoder options
    struct Options {
        bool validate_version;       ///< Check version field validity
        bool allow_invalid_version;  ///< Continue even if version invalid
        Options() : validate_version(true), allow_invalid_version(false) {}
    };

    explicit DoIPDecoder(Options opts = Options()) : options_(opts) {}

    [[nodiscard]] std::string_view name() const override { return "DoIP"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        // DoIP typically on port 13400, but we just check size here
        return ctx.has_bytes(HEADER_SIZE);
    }

    /// @brief Decode DoIP header
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

    /// @brief Parse routing activation request from payload
    [[nodiscard]] static std::optional<RoutingActivationRequest> parse_routing_activation_request(
        std::span<const std::byte> payload);

    /// @brief Parse routing activation response from payload
    [[nodiscard]] static std::optional<RoutingActivationResponse> parse_routing_activation_response(
        std::span<const std::byte> payload);

    /// @brief Parse diagnostic message payload
    [[nodiscard]] static std::optional<DiagnosticMessagePayload> parse_diagnostic_message(
        std::span<const std::byte> payload);

    /// @brief Parse vehicle identification response
    [[nodiscard]] static std::optional<VehicleIdentificationResponse>
    parse_vehicle_identification_response(std::span<const std::byte> payload);

private:
    Options options_;
};

/// @brief Global DoIP decoder instance
inline const DoIPDecoder& doip_decoder() {
    static DoIPDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::doip

/// @file doip.cpp
/// @brief DoIP (Diagnostics over IP) decoder implementation

#include "wadjet/protocols/doip.hpp"

#include "wadjet/core/byte_order.hpp"

#include <sstream>
#include <tuple>

namespace wadjet::protocols::doip {

std::string_view payload_type_string(PayloadType type) {
    switch (type) {
        case PayloadType::GenericNack:
            return "Generic NACK";
        case PayloadType::VehicleIdentificationRequest:
            return "Vehicle Identification Request";
        case PayloadType::VehicleIdentificationRequestWithEID:
            return "Vehicle Identification Request (EID)";
        case PayloadType::VehicleIdentificationRequestWithVIN:
            return "Vehicle Identification Request (VIN)";
        case PayloadType::VehicleAnnouncementOrIdentificationResponse:
            return "Vehicle Announcement/Identification Response";
        case PayloadType::RoutingActivationRequest:
            return "Routing Activation Request";
        case PayloadType::RoutingActivationResponse:
            return "Routing Activation Response";
        case PayloadType::AliveCheckRequest:
            return "Alive Check Request";
        case PayloadType::AliveCheckResponse:
            return "Alive Check Response";
        case PayloadType::DoIPEntityStatusRequest:
            return "DoIP Entity Status Request";
        case PayloadType::DoIPEntityStatusResponse:
            return "DoIP Entity Status Response";
        case PayloadType::DiagnosticPowerModeRequest:
            return "Diagnostic Power Mode Request";
        case PayloadType::DiagnosticPowerModeResponse:
            return "Diagnostic Power Mode Response";
        case PayloadType::DiagnosticMessage:
            return "Diagnostic Message";
        case PayloadType::DiagnosticMessagePositiveAck:
            return "Diagnostic Message Positive ACK";
        case PayloadType::DiagnosticMessageNegativeAck:
            return "Diagnostic Message Negative ACK";
        default:
            return "Unknown";
    }
}

std::string DoIPHeader::to_string() const {
    std::ostringstream oss;
    oss << "DoIP [version=0x" << std::hex << static_cast<int>(protocol_version)
        << ", type=" << payload_type_string(payload_type) << " (0x" << std::hex
        << static_cast<std::uint16_t>(payload_type) << "), payload_length=" << std::dec
        << payload_length << "]";
    return oss.str();
}

std::string_view activation_type_string(ActivationType type) {
    switch (type) {
        case ActivationType::Default:
            return "Default";
        case ActivationType::WWHObd:
            return "WWH-OBD";
        case ActivationType::CentralSecurityUnlock:
            return "Central Security Unlock";
        case ActivationType::ReservedForFutureExpansion:
            return "Reserved";
        case ActivationType::ManufacturerSpecificStart:
            return "Manufacturer-Specific (0xE0)";
        case ActivationType::ManufacturerSpecificEnd:
            return "Manufacturer-Specific (0xFE)";
        case ActivationType::InputOutputControlIdentifier:
            return "Input/Output Control";
        default:
            return "Unknown";
    }
}

std::string_view entity_type_string(EntityType type) {
    switch (type) {
        case EntityType::Gateway:
            return "Gateway";
        case EntityType::Node:
            return "Node";
        default:
            return "Unknown";
    }
}

std::string_view power_mode_string(PowerMode mode) {
    switch (mode) {
        case PowerMode::Ready:
            return "Ready";
        case PowerMode::NotReady:
            return "Not Ready";
        case PowerMode::NotSupported:
            return "Not Supported";
        default:
            return "Unknown";
    }
}

DoIPDecoder::Result DoIPDecoder::decode_impl(const DecodeContext& ctx) const {
    if (!ctx.has_bytes(HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "DoIP header requires 8 bytes");
    }

    const auto* data = ctx.data.data();

    DoIPHeader header;
    header.protocol_version = static_cast<std::uint8_t>(data[0]);
    header.inverse_protocol_version = static_cast<std::uint8_t>(data[1]);

    // Validate version field
    if (options_.validate_version && !header.is_version_valid()) {
        if (!options_.allow_invalid_version) {
            return make_error(DecodeErrorCode::InvalidVersion,
                              "DoIP version validation failed (version XOR inverse != 0xFF)");
        }
    }

    // Payload type (big-endian)
    header.payload_type = static_cast<PayloadType>((static_cast<std::uint16_t>(data[2]) << 8) |
                                                   static_cast<std::uint16_t>(data[3]));

    // Payload length (big-endian)
    header.payload_length =
        (static_cast<std::uint32_t>(data[4]) << 24) | (static_cast<std::uint32_t>(data[5]) << 16) |
        (static_cast<std::uint32_t>(data[6]) << 8) | static_cast<std::uint32_t>(data[7]);

    // Check if we have the complete payload
    if (!ctx.has_bytes(HEADER_SIZE + header.payload_length)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "Incomplete DoIP payload");
    }

    // Validate payload length for specific message types
    switch (header.payload_type) {
        case PayloadType::GenericNack:
            // NACK requires 3 bytes minimum (1 for code, 2 for unknown payload type)
            if (header.payload_length < 3) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "GenericNack payload requires at least 3 bytes");
            }
            break;

        case PayloadType::DiagnosticPowerModeResponse:
            // Power mode requires 1 byte
            if (header.payload_length < 1) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "DiagnosticPowerModeResponse requires at least 1 byte");
            }
            break;

        case PayloadType::DoIPEntityStatusResponse:
            // Entity status requires at least 5 bytes
            if (header.payload_length < 5) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "DoIPEntityStatusResponse requires at least 5 bytes");
            }
            break;

        case PayloadType::AliveCheckResponse:
            // Alive check response requires 2 bytes (tester source address)
            if (header.payload_length < 2) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "AliveCheckResponse requires at least 2 bytes");
            }
            break;

        case PayloadType::DiagnosticMessage:
            // Diagnostic message requires at least 4 bytes (source + target addresses)
            if (header.payload_length < 4) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "DiagnosticMessage requires at least 4 bytes");
            }
            break;

        case PayloadType::DiagnosticMessagePositiveAck:
        case PayloadType::DiagnosticMessageNegativeAck:
            // Diagnostic ACK/NACK requires at least 3 bytes (source + target + code)
            if (header.payload_length < 3) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "DiagnosticMessage ACK/NACK requires at least 3 bytes");
            }
            break;

        case PayloadType::RoutingActivationRequest:
            // Routing activation request requires at least 7 bytes
            if (header.payload_length < 7) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "RoutingActivationRequest requires at least 7 bytes");
            }
            break;

        case PayloadType::RoutingActivationResponse:
            // Routing activation response requires at least 9 bytes
            if (header.payload_length < 9) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "RoutingActivationResponse requires at least 9 bytes");
            }
            break;

        case PayloadType::VehicleIdentificationRequest:
        case PayloadType::VehicleIdentificationRequestWithEID:
        case PayloadType::VehicleIdentificationRequestWithVIN:
        case PayloadType::AliveCheckRequest:
        case PayloadType::DiagnosticPowerModeRequest:
        case PayloadType::DoIPEntityStatusRequest:
            // These request messages can have 0 payload length
            break;

        case PayloadType::VehicleAnnouncementOrIdentificationResponse:
            // Vehicle ID response requires at least 32 bytes
            if (header.payload_length < 32) {
                return make_error(DecodeErrorCode::InvalidLength,
                                  "VehicleAnnouncementOrIdentificationResponse requires at least 32 bytes");
            }
            break;

        default:
            // Unknown payload type - don't enforce validation
            break;
    }

    return make_success(std::move(header), ctx.sub_context(HEADER_SIZE));
}

std::optional<RoutingActivationRequest> DoIPDecoder::parse_routing_activation_request(
    std::span<const std::byte> payload) {
    // Minimum size: source_address(2) + activation_type(1) + reserved(4) = 7
    if (payload.size() < 7) {
        return std::nullopt;
    }

    RoutingActivationRequest req;
    const auto* data = payload.data();

    req.source_address = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                                    static_cast<std::uint16_t>(data[1]));

    req.activation_type = static_cast<std::uint8_t>(data[2]);

    req.reserved = (static_cast<std::uint32_t>(data[3]) << 24) |
                   (static_cast<std::uint32_t>(data[4]) << 16) |
                   (static_cast<std::uint32_t>(data[5]) << 8) | static_cast<std::uint32_t>(data[6]);

    // OEM-specific field (optional, 4 bytes)
    if (payload.size() >= 11) {
        req.oem_specific = (static_cast<std::uint32_t>(data[7]) << 24) |
                           (static_cast<std::uint32_t>(data[8]) << 16) |
                           (static_cast<std::uint32_t>(data[9]) << 8) |
                           static_cast<std::uint32_t>(data[10]);
    }

    return req;
}

std::optional<RoutingActivationResponse> DoIPDecoder::parse_routing_activation_response(
    std::span<const std::byte> payload) {
    // Minimum size: logical_address(2) + entity_address(2) + response_code(1) + reserved(4) = 9
    if (payload.size() < 9) {
        return std::nullopt;
    }

    RoutingActivationResponse resp;
    const auto* data = payload.data();

    resp.logical_address = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                                      static_cast<std::uint16_t>(data[1]));

    resp.entity_address = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[2]) << 8) |
                                                     static_cast<std::uint16_t>(data[3]));

    resp.response_code = static_cast<RoutingActivationResponseCode>(data[4]);

    resp.reserved =
        (static_cast<std::uint32_t>(data[5]) << 24) | (static_cast<std::uint32_t>(data[6]) << 16) |
        (static_cast<std::uint32_t>(data[7]) << 8) | static_cast<std::uint32_t>(data[8]);

    // OEM-specific field (optional, 4 bytes)
    if (payload.size() >= 13) {
        resp.oem_specific = (static_cast<std::uint32_t>(data[9]) << 24) |
                            (static_cast<std::uint32_t>(data[10]) << 16) |
                            (static_cast<std::uint32_t>(data[11]) << 8) |
                            static_cast<std::uint32_t>(data[12]);
    }

    return resp;
}

std::optional<DiagnosticMessagePayload> DoIPDecoder::parse_diagnostic_message(
    std::span<const std::byte> payload) {
    // Minimum size: source_address(2) + target_address(2) = 4
    if (payload.size() < 4) {
        return std::nullopt;
    }

    DiagnosticMessagePayload msg;
    const auto* data = payload.data();

    msg.source_address = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                                    static_cast<std::uint16_t>(data[1]));

    msg.target_address = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[2]) << 8) |
                                                    static_cast<std::uint16_t>(data[3]));

    // User data (UDS payload)
    if (payload.size() > 4) {
        msg.user_data = payload.subspan(4);
    }

    return msg;
}

std::optional<VehicleIdentificationResponse> DoIPDecoder::parse_vehicle_identification_response(
    std::span<const std::byte> payload) {
    // VIN(17) + logical_address(2) + EID(6) + GID(6) + further_action(1) = 32
    // Optional: sync_status(1) = 33
    if (payload.size() < 32) {
        return std::nullopt;
    }

    VehicleIdentificationResponse resp;
    const auto* data = payload.data();

    // VIN (17 bytes)
    for (std::size_t i = 0; i < 17; ++i) {
        resp.vin[i] = static_cast<char>(data[i]);
    }

    // Logical address
    resp.logical_address = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[17]) << 8) |
                                                      static_cast<std::uint16_t>(data[18]));

    // EID (6 bytes)
    for (std::size_t i = 0; i < 6; ++i) {
        resp.eid[i] = static_cast<std::uint8_t>(data[19 + i]);
    }

    // GID (6 bytes)
    for (std::size_t i = 0; i < 6; ++i) {
        resp.gid[i] = static_cast<std::uint8_t>(data[25 + i]);
    }

    // Further action required
    resp.further_action = static_cast<std::uint8_t>(data[31]);

    // VIN/GID sync status (optional)
    if (payload.size() >= 33) {
        resp.sync_status = static_cast<std::uint8_t>(data[32]);
    }

    return resp;
}

std::optional<PowerMode> DoIPDecoder::parse_diagnostic_power_mode(
    std::span<const std::byte> payload) {
    // Payload must contain at least 1 byte for power mode
    if (payload.size() < 1) {
        return std::nullopt;
    }

    auto mode = static_cast<PowerMode>(static_cast<std::uint8_t>(payload[0]));

    // Validate power mode is one of the known values
    if (mode != PowerMode::Ready && mode != PowerMode::NotReady && 
        mode != PowerMode::NotSupported) {
        return std::nullopt;
    }

    return mode;
}

std::optional<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t, std::uint16_t>>
DoIPDecoder::parse_entity_status(std::span<const std::byte> payload) {
    // Payload: node_type(1) + max_concurrent(1) + current_concurrent(1) + 
    //          max_connections(2) + reserved(2) = 7 bytes minimum
    // But the actual spec might vary, we accept 5 bytes minimum
    if (payload.size() < 5) {
        return std::nullopt;
    }

    const auto* data = payload.data();

    std::uint8_t node_type = static_cast<std::uint8_t>(data[0]);
    std::uint8_t max_concurrent_sockets = static_cast<std::uint8_t>(data[1]);
    std::uint8_t current_concurrent_sockets = static_cast<std::uint8_t>(data[2]);

    // Max connections (2 bytes, big-endian)
    std::uint16_t max_connections = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[3]) << 8) | 
        static_cast<std::uint16_t>(data[4]));

    return std::make_tuple(node_type, max_concurrent_sockets, 
                          current_concurrent_sockets, max_connections);
}

std::optional<std::tuple<NackCode, std::uint16_t>>
DoIPDecoder::parse_generic_nack(std::span<const std::byte> payload) {
    // Generic NACK payload contains NACK code (1 byte) + unknown payload type (2 bytes)
    if (payload.size() < 3) {
        return std::nullopt;
    }

    const auto* data = payload.data();

    NackCode nack_code = static_cast<NackCode>(static_cast<std::uint8_t>(data[0]));

    std::uint16_t unknown_payload_type = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[1]) << 8) | 
        static_cast<std::uint16_t>(data[2]));

    return std::make_tuple(nack_code, unknown_payload_type);
}

std::optional<std::uint16_t> DoIPDecoder::parse_alive_check_response(
    std::span<const std::byte> payload) {
    // Alive check response: tester source address (2 bytes)
    if (payload.size() < 2) {
        return std::nullopt;
    }

    const auto* data = payload.data();

    std::uint16_t tester_source_address = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8) | 
        static_cast<std::uint16_t>(data[1]));

    return tester_source_address;
}

}  // namespace wadjet::protocols::doip

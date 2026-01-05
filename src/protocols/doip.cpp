/// @file doip.cpp
/// @brief DoIP (Diagnostics over IP) decoder implementation

#include "wadjet/protocols/doip.hpp"

#include "wadjet/core/byte_order.hpp"

#include <format>

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
    return std::format("DoIP [version={:#04x}, type={} ({:#06x}), payload_length={}]",
                       protocol_version, payload_type_string(payload_type),
                       static_cast<std::uint16_t>(payload_type), payload_length);
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

    req.source_address =
        (static_cast<std::uint16_t>(data[0]) << 8) | static_cast<std::uint16_t>(data[1]);

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

    resp.logical_address =
        (static_cast<std::uint16_t>(data[0]) << 8) | static_cast<std::uint16_t>(data[1]);

    resp.entity_address =
        (static_cast<std::uint16_t>(data[2]) << 8) | static_cast<std::uint16_t>(data[3]);

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

    msg.source_address =
        (static_cast<std::uint16_t>(data[0]) << 8) | static_cast<std::uint16_t>(data[1]);

    msg.target_address =
        (static_cast<std::uint16_t>(data[2]) << 8) | static_cast<std::uint16_t>(data[3]);

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
    resp.logical_address =
        (static_cast<std::uint16_t>(data[17]) << 8) | static_cast<std::uint16_t>(data[18]);

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

}  // namespace wadjet::protocols::doip

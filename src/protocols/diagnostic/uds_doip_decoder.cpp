/// @file uds_doip_decoder.cpp
/// @brief Combined UDS-over-DoIP decoder implementation

#include "wadjet/protocols/diagnostic/uds_doip_decoder.hpp"

namespace wadjet::protocols::diagnostic {

UdsOverDoipDecoder::Result UdsOverDoipDecoder::decode(std::span<const std::byte> data) const {
    // First decode DoIP
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;
    ctx.timestamp = {};
    auto doip_result = doip_decoder_.decode_impl(ctx);

    if (!doip_result) {
        return Result::err(UdsOverDoipError::make(UdsOverDoipError::Code::DoipDecodeFailed,
                                                  "Failed to decode DoIP header"));
    }

    const auto& doip_header = doip_result.value();

    // Check if it's a diagnostic message
    if (!is_diagnostic_message(doip_header)) {
        return Result::err(UdsOverDoipError::make(UdsOverDoipError::Code::NotDiagnosticMessage,
                                                  "DoIP message is not a diagnostic message"));
    }

    // Get payload
    if (data.size() < doip::HEADER_SIZE + doip_header.payload_length) {
        return Result::err(UdsOverDoipError::make(UdsOverDoipError::Code::PayloadTooShort,
                                                  "Insufficient data for DoIP payload"));
    }

    auto payload = data.subspan(doip::HEADER_SIZE, doip_header.payload_length);

    return decode(doip_header, payload);
}

UdsOverDoipDecoder::Result UdsOverDoipDecoder::decode(const doip::DoIPHeader& header,
                                                      std::span<const std::byte> payload) const {
    // Check if it's a diagnostic message
    if (!is_diagnostic_message(header)) {
        return Result::err(UdsOverDoipError::make(UdsOverDoipError::Code::NotDiagnosticMessage,
                                                  "DoIP message is not a diagnostic message"));
    }

    // Parse diagnostic message payload (source + target + UDS data)
    // Minimum: 2 bytes source + 2 bytes target + 1 byte SID
    if (payload.size() < 5) {
        return Result::err(UdsOverDoipError::make(UdsOverDoipError::Code::PayloadTooShort,
                                                  "Diagnostic message payload too short"));
    }

    // Extract addresses (big-endian)
    auto source_address = static_cast<LogicalAddress>(
        (static_cast<std::uint16_t>(payload[0]) << 8) | static_cast<std::uint16_t>(payload[1]));

    auto target_address = static_cast<LogicalAddress>(
        (static_cast<std::uint16_t>(payload[2]) << 8) | static_cast<std::uint16_t>(payload[3]));

    // Extract UDS data
    auto uds_data = payload.subspan(4);

    // Decode UDS
    auto uds_result = uds_decoder_.decode(uds_data);
    if (!uds_result) {
        return Result::err(UdsOverDoipError::make(UdsOverDoipError::Code::UdsDecodeFailed,
                                                  "Failed to decode UDS message"));
    }

    const auto& uds_decode = uds_result.value();

    // Determine direction
    MessageDirection direction = MessageDirection::Unknown;
    if (uds::UdsDecoder::is_request(uds_data)) {
        direction = MessageDirection::Request;
    } else if (uds::UdsDecoder::is_positive_response(uds_data) ||
               uds::UdsDecoder::is_negative_response(uds_data)) {
        direction = MessageDirection::Response;
    }

    // Build result
    UdsOverDoipResult result;
    result.doip_header = header;
    result.source_address = source_address;
    result.target_address = target_address;
    result.uds_header = uds_decode.header;
    result.uds_message = uds_decode.message;
    result.direction = direction;

    return Result::ok(result);
}

bool UdsOverDoipDecoder::is_diagnostic_message(const doip::DoIPHeader& header) {
    return header.payload_type == doip::PayloadType::DiagnosticMessage;
}

bool UdsOverDoipDecoder::looks_like_diagnostic_message(std::span<const std::byte> data) {
    if (data.size() < doip::HEADER_SIZE) {
        return false;
    }

    // Check DoIP version field
    auto version = static_cast<std::uint8_t>(data[0]);
    auto inverse = static_cast<std::uint8_t>(data[1]);
    if ((version ^ inverse) != 0xFF) {
        return false;
    }

    // Check payload type (bytes 2-3, big-endian)
    auto payload_type = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[2]) << 8) |
                                                   static_cast<std::uint16_t>(data[3]));

    return payload_type == static_cast<std::uint16_t>(doip::PayloadType::DiagnosticMessage);
}

}  // namespace wadjet::protocols::diagnostic

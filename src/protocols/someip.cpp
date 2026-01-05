/// @file someip.cpp
/// @brief SOME/IP protocol decoder implementation

#include "wadjet/protocols/someip.hpp"

#include <iomanip>
#include <sstream>

namespace wadjet::protocols::someip {

std::string SomeIpHeader::to_string() const {
    std::ostringstream oss;
    oss << "SOME/IP { service=0x" << std::hex << std::setfill('0') << std::setw(4) << service_id
        << ", method=0x" << std::setw(4) << method_id << ", client=0x" << std::setw(4) << client_id
        << ", session=0x" << std::setw(4) << session_id << std::dec
        << ", type=" << message_type_string(message_type)
        << ", ret=" << return_code_string(return_code) << ", len=" << length;

    if (is_service_discovery()) {
        oss << " [SD]";
    }
    oss << " }";
    return oss.str();
}

SomeIpDecoder::Result SomeIpDecoder::decode_impl(const DecodeContext& ctx) const {
    // Check minimum size
    if (!ctx.has_bytes(HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "SOME/IP header too small");
    }

    SomeIpHeader header;

    // Service ID (bytes 0-1)
    header.service_id = ctx.read_be16(0);

    // Method ID (bytes 2-3)
    header.method_id = ctx.read_be16(2);

    // Length (bytes 4-7) - includes 8 bytes from Request ID onwards
    header.length = ctx.read_be32(4);

    // Client ID (bytes 8-9)
    header.client_id = ctx.read_be16(8);

    // Session ID (bytes 10-11)
    header.session_id = ctx.read_be16(10);

    // Protocol version (byte 12)
    header.protocol_version = static_cast<std::uint8_t>(ctx.data[12]);

    // Validate protocol version
    if (options_.validate_protocol_version && header.protocol_version != PROTOCOL_VERSION) {
        if (!options_.allow_invalid_version) {
            return make_error(DecodeErrorCode::InvalidVersion,
                              "SOME/IP protocol version " +
                                  std::to_string(header.protocol_version) + " (expected " +
                                  std::to_string(PROTOCOL_VERSION) + ")");
        }
    }

    // Interface version (byte 13)
    header.interface_version = static_cast<std::uint8_t>(ctx.data[13]);

    // Message type (byte 14)
    header.message_type = static_cast<MessageType>(static_cast<std::uint8_t>(ctx.data[14]));

    // Return code (byte 15)
    header.return_code = static_cast<ReturnCode>(static_cast<std::uint8_t>(ctx.data[15]));

    // Validate length field
    if (header.length < 8) {
        return make_error(DecodeErrorCode::InvalidLength,
                          "SOME/IP length too small: " + std::to_string(header.length));
    }

    // Create context for payload
    auto next_ctx = ctx.sub_context(HEADER_SIZE);

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::someip

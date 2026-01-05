/// @file udp.cpp
/// @brief UDP header decoder implementation

#include "wadjet/protocols/udp.hpp"

#include <sstream>

namespace wadjet::protocols::udp {

std::string UdpHeader::to_string() const {
    std::ostringstream oss;
    oss << "UDP { src_port=" << src_port << ", dst_port=" << dst_port << ", len=" << length;
    if (!checksum_valid) {
        oss << ", CHECKSUM_INVALID";
    }
    oss << " }";
    return oss.str();
}

UdpDecoder::Result UdpDecoder::decode_impl(const DecodeContext& ctx) const {
    // Check minimum size
    if (!ctx.has_bytes(HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "UDP header too small");
    }

    UdpHeader header;

    // Source port (bytes 0-1)
    header.src_port = ctx.read_be16(0);

    // Destination port (bytes 2-3)
    header.dst_port = ctx.read_be16(2);

    // Length (bytes 4-5)
    header.length = ctx.read_be16(4);

    // Checksum (bytes 6-7)
    header.checksum = ctx.read_be16(6);

    // Validate length
    if (header.length < HEADER_SIZE) {
        return make_error(DecodeErrorCode::InvalidLength,
                          "UDP length too small: " + std::to_string(header.length));
    }

    // Note: Full UDP checksum validation requires IP pseudo-header
    // For now, we just mark it as valid if checksum is 0 (optional in UDP over IPv4)
    // or if validation is disabled
    // TODO: Implement full checksum validation with pseudo-header
    (void)options_.validate_checksum;  // Suppress unused warning until implemented
    header.checksum_valid = (header.checksum == 0) || !options_.validate_checksum;

    // Create context for next layer
    auto next_ctx = ctx.sub_context(HEADER_SIZE);
    next_ctx.layer_info.src_port = header.src_port;
    next_ctx.layer_info.dst_port = header.dst_port;

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::udp

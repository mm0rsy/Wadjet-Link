/// @file udp.cpp
/// @brief UDP header decoder implementation

#include "wadjet/protocols/udp.hpp"

#include "wadjet/protocols/common/checksum.hpp"

#include <cstdint>
#include <cstring>
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

std::uint16_t UdpChecksumValidator::calculate_checksum(const void* src_ip, const void* dst_ip,
                                                       const void* udp_data,
                                                       std::size_t udp_length) {
    // Convert pointers to uint32 (assuming 4-byte IPv4 addresses)
    const auto* src_ptr = static_cast<const std::uint8_t*>(src_ip);
    const auto* dst_ptr = static_cast<const std::uint8_t*>(dst_ip);
    const auto* data_ptr = static_cast<const std::uint8_t*>(udp_data);

    std::uint32_t src_addr = (static_cast<std::uint32_t>(src_ptr[0]) << 24) |
                             (static_cast<std::uint32_t>(src_ptr[1]) << 16) |
                             (static_cast<std::uint32_t>(src_ptr[2]) << 8) |
                             static_cast<std::uint32_t>(src_ptr[3]);

    std::uint32_t dst_addr = (static_cast<std::uint32_t>(dst_ptr[0]) << 24) |
                             (static_cast<std::uint32_t>(dst_ptr[1]) << 16) |
                             (static_cast<std::uint32_t>(dst_ptr[2]) << 8) |
                             static_cast<std::uint32_t>(dst_ptr[3]);

    // Convert UDP data to vector for Checksum utility
    std::vector<std::uint8_t> udp_header(data_ptr, data_ptr + std::min(udp_length, size_t(8)));
    std::vector<std::uint8_t> udp_payload(data_ptr + 8, data_ptr + udp_length);

    // Use common checksum function
    return common::Checksum::transport_checksum(udp_header, udp_payload, src_addr, dst_addr, 0x11);
}

bool UdpChecksumValidator::validate_checksum(const void* src_ip, const void* dst_ip,
                                             const void* udp_data, std::size_t udp_length,
                                             std::uint16_t checksum_field) {
    // Zero checksum is special: in IPv4 it means "no checksum"
    if (checksum_field == 0) {
        return true;  // Zero checksum always valid in this context
    }

    // Calculate the checksum
    std::uint16_t calculated = calculate_checksum(src_ip, dst_ip, udp_data, udp_length);

    // In UDP, a calculated checksum of 0 is replaced with 0xFFFF
    if (calculated == 0) {
        calculated = 0xFFFF;
    }

    return calculated == checksum_field;
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

    // Note: Full UDP checksum validation requires IP pseudo-header (src/dst IP addresses)
    // These are not currently passed through DecodeContext.LayerInfo
    // For now, validate checksum only if it's zero (which is allowed in IPv4)
    // TODO: Extend LayerInfo to include src/dst IP for checksum validation
    header.checksum_valid = (header.checksum == 0) || !options_.validate_checksum;

    // Create context for next layer
    auto next_ctx = ctx.sub_context(HEADER_SIZE);
    next_ctx.layer_info.src_port = header.src_port;
    next_ctx.layer_info.dst_port = header.dst_port;

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::udp

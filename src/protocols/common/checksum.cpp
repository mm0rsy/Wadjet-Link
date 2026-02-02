/// @file checksum.cpp
/// @brief Checksum validation utilities for protocol completeness

#include "wadjet/protocols/common/checksum.hpp"

#include <cstdint>
#include <numeric>
#include <span>

namespace wadjet {
namespace protocols {
namespace common {

/// @brief Calculate IPv4 header checksum (RFC 791)
/// @param header IPv4 header bytes (typically 20-60 bytes)
/// @return 16-bit checksum value (0 means valid when re-computed)
std::uint16_t Checksum::ipv4_checksum(const std::vector<std::uint8_t>& header) {
    // IPv4 checksum is one's complement of sum of 16-bit words
    std::uint32_t sum = 0;

    // Process header as 16-bit words
    for (std::size_t i = 0; i < header.size(); i += 2) {
        // Skip checksum field (bytes 10-11)
        if (i == 10)
            continue;

        std::uint16_t word = static_cast<std::uint16_t>(static_cast<std::uint16_t>(header[i]) << 8);
        if (i + 1 < header.size()) {
            word |= static_cast<std::uint16_t>(header[i + 1]);
        }
        sum += word;
    }

    // Add carries back to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Return one's complement
    return ~static_cast<std::uint16_t>(sum);
}

/// @brief Calculate TCP/UDP checksum with pseudo-header (RFC 793, RFC 768)
/// @param header TCP/UDP header bytes
/// @param payload Payload bytes
/// @param src_ip Source IPv4 address (32-bit)
/// @param dst_ip Destination IPv4 address (32-bit)
/// @param protocol IP protocol number (6 for TCP, 17 for UDP)
/// @return 16-bit checksum value (0 means valid when re-computed)
std::uint16_t Checksum::transport_checksum(const std::vector<std::uint8_t>& header,
                                           const std::vector<std::uint8_t>& payload,
                                           std::uint32_t src_ip, std::uint32_t dst_ip,
                                           std::uint8_t protocol) {
    std::uint32_t sum = 0;

    // Pseudo-header (RFC 793)
    // Source IP (4 bytes)
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;

    // Destination IP (4 bytes)
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += dst_ip & 0xFFFF;

    // Protocol (2 bytes, padded with zero)
    sum += static_cast<std::uint16_t>(protocol);

    // Length (2 bytes) = header + payload length
    std::uint16_t length = static_cast<std::uint16_t>(header.size() + payload.size());
    sum += length;

    // Process header + payload as 16-bit words
    auto process_buffer = [&sum](const std::vector<std::uint8_t>& buf) {
        for (std::size_t i = 0; i < buf.size(); i += 2) {
            std::uint16_t word =
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(buf[i]) << 8);
            if (i + 1 < buf.size()) {
                word |= static_cast<std::uint16_t>(buf[i + 1]);
            }
            sum += word;
        }
    };

    process_buffer(header);
    process_buffer(payload);

    // Add carries back to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Return one's complement
    return ~static_cast<std::uint16_t>(sum);
}

}  // namespace common
}  // namespace protocols
}  // namespace wadjet

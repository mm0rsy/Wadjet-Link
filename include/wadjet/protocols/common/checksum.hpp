// Checksum validation utilities for protocol completeness
#pragma once
#include <cstdint>
#include <vector>

namespace wadjet {
namespace protocols {
namespace common {

class Checksum {
public:
    // IPv4 header checksum
    static uint16_t ipv4_checksum(const std::vector<uint8_t>& header);
    // TCP/UDP checksum (with pseudo-header)
    static uint16_t transport_checksum(const std::vector<uint8_t>& header,
                                       const std::vector<uint8_t>& payload, uint32_t src_ip,
                                       uint32_t dst_ip, uint8_t protocol);
};

}  // namespace common
}  // namespace protocols
}  // namespace wadjet

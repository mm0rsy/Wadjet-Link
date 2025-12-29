#pragma once

/// @file pcap_file.hpp
/// @brief PCAP file format structures and constants

#include <cstdint>

namespace wadjet::pcap {

/// @brief PCAP magic numbers
constexpr std::uint32_t PCAP_MAGIC_NATIVE = 0xA1B2C3D4;       ///< Native byte order
constexpr std::uint32_t PCAP_MAGIC_SWAPPED = 0xD4C3B2A1;      ///< Swapped byte order
constexpr std::uint32_t PCAP_MAGIC_NSEC_NATIVE = 0xA1B23C4D;  ///< Nanosecond resolution
constexpr std::uint32_t PCAP_MAGIC_NSEC_SWAPPED = 0x4D3CB2A1;

/// @brief PCAP link layer types
enum class LinkType : std::uint32_t {
    Null = 0,
    Ethernet = 1,
    Raw = 101,
    Linux_SLL = 113,
    Linux_SLL2 = 276,
};

/// @brief PCAP file header (24 bytes)
struct PcapFileHeader {
    std::uint32_t magic_number;   ///< Magic number (byte order indicator)
    std::uint16_t version_major;  ///< Major version (usually 2)
    std::uint16_t version_minor;  ///< Minor version (usually 4)
    std::int32_t thiszone;        ///< GMT to local correction (usually 0)
    std::uint32_t sigfigs;        ///< Accuracy of timestamps (usually 0)
    std::uint32_t snaplen;        ///< Max length of captured packets
    std::uint32_t network;        ///< Link layer type
};

/// @brief PCAP packet header (16 bytes)
struct PcapPacketHeader {
    std::uint32_t ts_sec;    ///< Timestamp seconds
    std::uint32_t ts_usec;   ///< Timestamp microseconds (or nanoseconds)
    std::uint32_t incl_len;  ///< Number of bytes of packet saved in file
    std::uint32_t orig_len;  ///< Actual length of packet
};

static_assert(sizeof(PcapFileHeader) == 24, "PcapFileHeader must be 24 bytes");
static_assert(sizeof(PcapPacketHeader) == 16, "PcapPacketHeader must be 16 bytes");

}  // namespace wadjet::pcap

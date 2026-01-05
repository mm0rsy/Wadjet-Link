#pragma once

/// @file pcapng_file.hpp
/// @brief PCAPNG file format structures and constants (RFC draft-tuexen-opsawg-pcapng)

#include <cstdint>

namespace wadjet::pcap {

/// @brief PCAPNG block types
enum class PcapngBlockType : std::uint32_t {
    SectionHeader = 0x0A0D0D0A,         ///< Section Header Block (SHB)
    InterfaceDescription = 0x00000001,  ///< Interface Description Block (IDB)
    EnhancedPacket = 0x00000006,        ///< Enhanced Packet Block (EPB)
    SimplePacket = 0x00000003,          ///< Simple Packet Block (SPB)
    NameResolution = 0x00000004,        ///< Name Resolution Block (NRB)
    InterfaceStatistics = 0x00000005,   ///< Interface Statistics Block (ISB)
    CustomBlock = 0x00000BAD,           ///< Custom Block
    CustomBlockCopyable = 0x40000BAD,   ///< Custom Block (copyable)
};

/// @brief PCAPNG byte order magic
constexpr std::uint32_t PCAPNG_BYTE_ORDER_MAGIC = 0x1A2B3C4D;

/// @brief PCAPNG option types (common)
enum class PcapngOptionType : std::uint16_t {
    EndOfOpt = 0,  ///< End of options
    Comment = 1,   ///< Comment (UTF-8 string)
    // SHB-specific options
    SHB_Hardware = 2,  ///< Hardware description
    SHB_OS = 3,        ///< Operating system
    SHB_UserAppl = 4,  ///< User application
    // IDB-specific options
    IDB_Name = 2,         ///< Interface name
    IDB_Description = 3,  ///< Interface description
    IDB_IPv4Addr = 4,     ///< IPv4 address
    IDB_IPv6Addr = 5,     ///< IPv6 address
    IDB_MACAddr = 6,      ///< MAC address
    IDB_EUIAddr = 7,      ///< EUI address
    IDB_Speed = 8,        ///< Interface speed (bps)
    IDB_TSResol = 9,      ///< Timestamp resolution
    IDB_TZone = 10,       ///< Time zone
    IDB_Filter = 11,      ///< BPF filter
    IDB_OS = 12,          ///< Operating system
    IDB_FCSLen = 13,      ///< Frame Check Sequence length
    IDB_TSOffset = 14,    ///< Timestamp offset
    IDB_Hardware = 15,    ///< Hardware description
    // EPB-specific options
    EPB_Flags = 2,      ///< Packet flags
    EPB_Hash = 3,       ///< Packet hash
    EPB_DropCount = 4,  ///< Dropped packets count
    EPB_PacketId = 5,   ///< Packet ID
    EPB_Queue = 6,      ///< Queue ID
    EPB_Verdict = 7,    ///< Verdict
    // ISB-specific options
    ISB_StartTime = 2,     ///< Capture start time
    ISB_EndTime = 3,       ///< Capture end time
    ISB_IFRecv = 4,        ///< Packets received
    ISB_IFDrop = 5,        ///< Packets dropped by interface
    ISB_FilterAccept = 6,  ///< Packets accepted by filter
    ISB_OSDrops = 7,       ///< Packets dropped by OS
    ISB_UsrDeliv = 8,      ///< Packets delivered to user
};

/// @brief Section Header Block (SHB) fixed fields
struct PcapngSHB {
    std::uint32_t block_type;        ///< Block type = 0x0A0D0D0A
    std::uint32_t block_total_len;   ///< Total block length
    std::uint32_t byte_order_magic;  ///< Byte order magic = 0x1A2B3C4D
    std::uint16_t major_version;     ///< Major version = 1
    std::uint16_t minor_version;     ///< Minor version = 0
    std::int64_t section_length;     ///< Section length (-1 = unspecified)
    // Options follow, then block_total_len repeated
};

/// @brief Interface Description Block (IDB) fixed fields
struct PcapngIDB {
    std::uint32_t block_type;       ///< Block type = 0x00000001
    std::uint32_t block_total_len;  ///< Total block length
    std::uint16_t link_type;        ///< Link layer type (e.g., LINKTYPE_ETHERNET = 1)
    std::uint16_t reserved;         ///< Reserved = 0
    std::uint32_t snap_len;         ///< Maximum packet length captured
    // Options follow, then block_total_len repeated
};

/// @brief Enhanced Packet Block (EPB) fixed fields
struct PcapngEPB {
    std::uint32_t block_type;       ///< Block type = 0x00000006
    std::uint32_t block_total_len;  ///< Total block length
    std::uint32_t interface_id;     ///< Interface ID (from IDB order)
    std::uint32_t timestamp_high;   ///< High 32 bits of timestamp
    std::uint32_t timestamp_low;    ///< Low 32 bits of timestamp
    std::uint32_t captured_len;     ///< Captured packet length
    std::uint32_t original_len;     ///< Original packet length
    // Packet data follows (padded to 4 bytes), then options, then block_total_len
};

/// @brief Simple Packet Block (SPB) fixed fields
struct PcapngSPB {
    std::uint32_t block_type;       ///< Block type = 0x00000003
    std::uint32_t block_total_len;  ///< Total block length
    std::uint32_t original_len;     ///< Original packet length
    // Packet data follows (padded to 4 bytes), then block_total_len
};

/// @brief Interface Statistics Block (ISB) fixed fields
struct PcapngISB {
    std::uint32_t block_type;       ///< Block type = 0x00000005
    std::uint32_t block_total_len;  ///< Total block length
    std::uint32_t interface_id;     ///< Interface ID
    std::uint32_t timestamp_high;   ///< High 32 bits of timestamp
    std::uint32_t timestamp_low;    ///< Low 32 bits of timestamp
    // Options follow, then block_total_len repeated
};

/// @brief Option header
struct PcapngOption {
    std::uint16_t type;    ///< Option type
    std::uint16_t length;  ///< Option value length (not including padding)
    // Option value follows (padded to 4 bytes)
};

/// @brief Calculate padding to align to 4 bytes
constexpr std::uint32_t pcapng_padding(std::uint32_t len) {
    return (4 - (len & 3)) & 3;
}

/// @brief Default timestamp resolution (nanoseconds)
constexpr std::uint8_t PCAPNG_TSRESOL_NANO = 9;  // 10^-9

/// @brief Default timestamp resolution (microseconds)
constexpr std::uint8_t PCAPNG_TSRESOL_MICRO = 6;  // 10^-6

}  // namespace wadjet::pcap

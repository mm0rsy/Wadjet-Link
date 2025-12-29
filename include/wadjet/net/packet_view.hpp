#pragma once

/// @file packet_view.hpp
/// @brief Zero-copy immutable view into packet data

#include "wadjet/core/byte_order.hpp"
#include "wadjet/core/timestamp.hpp"
#include "wadjet/core/types.hpp"

#include <cstring>
#include <optional>
#include <span>

namespace wadjet {

/// @brief Zero-copy immutable view into raw packet data
///
/// PacketView provides a lightweight, non-owning view into packet data.
/// It does not allocate memory and is designed for high-performance
/// packet inspection.
///
/// @code
/// PacketView view(raw_data, timestamp);
/// auto eth = view.ethernet_header();
/// if (eth) {
///     std::cout << "Src: " << eth->src.to_string() << "\n";
/// }
/// @endcode
class PacketView {
public:
    /// @brief Ethernet header structure
    struct EthernetHeader {
        MacAddress dst;
        MacAddress src;
        EtherType ether_type;
    };

    /// @brief VLAN tag structure (802.1Q)
    struct VlanTag {
        std::uint16_t tci;       ///< Tag Control Information
        std::uint16_t priority;  ///< Priority Code Point (PCP)
        bool dei;                ///< Drop Eligible Indicator
        std::uint16_t vlan_id;   ///< VLAN Identifier
        EtherType ether_type;    ///< Inner EtherType
    };

    /// @brief IPv4 header structure
    struct IPv4Header {
        std::uint8_t version;
        std::uint8_t ihl;  ///< Internet Header Length (in 32-bit words)
        std::uint8_t dscp;
        std::uint8_t ecn;
        std::uint16_t total_length;
        std::uint16_t identification;
        bool dont_fragment;
        bool more_fragments;
        std::uint16_t fragment_offset;
        std::uint8_t ttl;
        IpProtocol protocol;
        std::uint16_t checksum;
        IPv4Address src;
        IPv4Address dst;
    };

    /// @brief UDP header structure
    struct UdpHeader {
        std::uint16_t src_port;
        std::uint16_t dst_port;
        std::uint16_t length;
        std::uint16_t checksum;
    };

    /// @brief TCP header structure
    struct TcpHeader {
        std::uint16_t src_port;
        std::uint16_t dst_port;
        std::uint32_t seq_num;
        std::uint32_t ack_num;
        std::uint8_t data_offset;  ///< Header length in 32-bit words
        std::uint8_t flags;
        std::uint16_t window;
        std::uint16_t checksum;
        std::uint16_t urgent_ptr;
    };

    /// @brief TCP flag constants
    struct TcpFlags {
        static constexpr std::uint8_t FIN = 0x01;
        static constexpr std::uint8_t SYN = 0x02;
        static constexpr std::uint8_t RST = 0x04;
        static constexpr std::uint8_t PSH = 0x08;
        static constexpr std::uint8_t ACK = 0x10;
        static constexpr std::uint8_t URG = 0x20;
    };

    /// @brief Construct a PacketView from raw data
    /// @param data Raw packet data (must remain valid for lifetime of view)
    /// @param ts Capture timestamp
    PacketView(ByteSpan data, Timestamp ts = {}) : data_(data), timestamp_(ts) {}

    /// @brief Construct from uint8_t span (convenience)
    PacketView(std::span<const std::uint8_t> data, Timestamp ts = {})
        : data_(std::as_bytes(data)), timestamp_(ts) {}

    /// @brief Get raw packet data
    [[nodiscard]] ByteSpan data() const { return data_; }

    /// @brief Get packet size in bytes
    [[nodiscard]] std::size_t size() const { return data_.size(); }

    /// @brief Get capture timestamp
    [[nodiscard]] Timestamp timestamp() const { return timestamp_; }

    /// @brief Check if packet is empty
    [[nodiscard]] bool empty() const { return data_.empty(); }

    /// @brief Parse Ethernet header
    [[nodiscard]] std::optional<EthernetHeader> ethernet_header() const;

    /// @brief Check if packet has VLAN tag
    [[nodiscard]] bool has_vlan() const;

    /// @brief Parse VLAN tag (if present)
    [[nodiscard]] std::optional<VlanTag> vlan_tag() const;

    /// @brief Parse IPv4 header
    [[nodiscard]] std::optional<IPv4Header> ipv4_header() const;

    /// @brief Parse UDP header
    [[nodiscard]] std::optional<UdpHeader> udp_header() const;

    /// @brief Parse TCP header
    [[nodiscard]] std::optional<TcpHeader> tcp_header() const;

    /// @brief Get Layer 3 (IP) payload offset
    [[nodiscard]] std::size_t l3_offset() const;

    /// @brief Get Layer 4 (TCP/UDP) payload offset
    [[nodiscard]] std::size_t l4_offset() const;

    /// @brief Get application layer payload
    [[nodiscard]] ByteSpan payload() const;

    /// @brief Get a sub-view starting at offset
    [[nodiscard]] PacketView subview(std::size_t offset) const;

    /// @brief Get a sub-view with offset and length
    [[nodiscard]] PacketView subview(std::size_t offset, std::size_t length) const;

private:
    ByteSpan data_;
    Timestamp timestamp_;
};

}  // namespace wadjet

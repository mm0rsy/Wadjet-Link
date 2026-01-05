#pragma once

/// @file types.hpp
/// @brief Core type definitions for Wadjet-Link

#include "wadjet/core/address.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace wadjet {

/// @brief MAC address (6 bytes)
///
/// Uses CRTP base class for common address operations.
/// Provides MAC-specific functionality like broadcast/multicast detection.
struct MacAddress : public AddressBase<MacAddress, 6> {
    using Base = AddressBase<MacAddress, 6>;
    using Base::Base;  // Inherit constructors

    /// @brief Create MAC address from bytes
    static constexpr MacAddress from_bytes(std::uint8_t b0, std::uint8_t b1, std::uint8_t b2,
                                           std::uint8_t b3, std::uint8_t b4, std::uint8_t b5) {
        MacAddress addr;
        addr.bytes = {b0, b1, b2, b3, b4, b5};
        return addr;
    }

    /// @brief Parse MAC address from string "xx:xx:xx:xx:xx:xx"
    static auto from_string(std::string_view str) -> MacAddress {
        MacAddress addr;
        Base::parse_hex_with_delimiter(str, addr.bytes, ':', 2);
        return addr;
    }

    /// @brief Convert to string "xx:xx:xx:xx:xx:xx"
    [[nodiscard]] auto to_string() const -> std::string { return Base::format_hex(bytes, ':'); }

    /// @brief Check if this is a broadcast address (ff:ff:ff:ff:ff:ff)
    [[nodiscard]] constexpr bool is_broadcast() const {
        return bytes[0] == 0xff && bytes[1] == 0xff && bytes[2] == 0xff && bytes[3] == 0xff &&
               bytes[4] == 0xff && bytes[5] == 0xff;
    }

    /// @brief Check if this is a multicast address (LSB of first byte is 1)
    [[nodiscard]] constexpr bool is_multicast() const { return (bytes[0] & 0x01) != 0; }

    /// @brief Check if this is a locally administered address
    [[nodiscard]] constexpr bool is_local() const { return (bytes[0] & 0x02) != 0; }

    /// @brief Get the OUI (Organizationally Unique Identifier)
    [[nodiscard]] constexpr std::uint32_t oui() const {
        return (static_cast<std::uint32_t>(bytes[0]) << 16) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) | static_cast<std::uint32_t>(bytes[2]);
    }
};

/// @brief IPv4 address (4 bytes)
///
/// Uses CRTP base class for common address operations.
/// Provides IPv4-specific functionality like subnet checking.
struct IPv4Address : public AddressBase<IPv4Address, 4> {
    using Base = AddressBase<IPv4Address, 4>;
    using Base::Base;  // Inherit constructors

    /// @brief Create IPv4 address from bytes
    static constexpr IPv4Address from_bytes(std::uint8_t b0, std::uint8_t b1, std::uint8_t b2,
                                            std::uint8_t b3) {
        IPv4Address addr;
        addr.bytes = {b0, b1, b2, b3};
        return addr;
    }

    /// @brief Create IPv4 address from 32-bit integer (network byte order)
    static constexpr IPv4Address from_uint32(std::uint32_t value) {
        return from_bytes(static_cast<std::uint8_t>((value >> 24) & 0xFF),
                          static_cast<std::uint8_t>((value >> 16) & 0xFF),
                          static_cast<std::uint8_t>((value >> 8) & 0xFF),
                          static_cast<std::uint8_t>(value & 0xFF));
    }

    /// @brief Parse IPv4 address from string "x.x.x.x"
    static auto from_string(std::string_view str) -> IPv4Address {
        IPv4Address addr;
        Base::parse_decimal_with_delimiter(str, addr.bytes, '.');
        return addr;
    }

    /// @brief Convert to string "x.x.x.x"
    [[nodiscard]] auto to_string() const -> std::string { return Base::format_decimal(bytes, '.'); }

    /// @brief Convert to 32-bit integer (network byte order)
    [[nodiscard]] constexpr std::uint32_t to_uint32() const {
        return (static_cast<std::uint32_t>(bytes[0]) << 24) |
               (static_cast<std::uint32_t>(bytes[1]) << 16) |
               (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
    }

    /// @brief Check if this is a loopback address (127.x.x.x)
    [[nodiscard]] constexpr bool is_loopback() const { return bytes[0] == 127; }

    /// @brief Check if this is a broadcast address (255.255.255.255)
    [[nodiscard]] constexpr bool is_broadcast() const {
        return bytes[0] == 255 && bytes[1] == 255 && bytes[2] == 255 && bytes[3] == 255;
    }

    /// @brief Check if this is a multicast address (224.0.0.0 - 239.255.255.255)
    [[nodiscard]] constexpr bool is_multicast() const {
        return (bytes[0] & 0xF0) == 0xE0;  // 224-239
    }

    /// @brief Check if this is a private address (RFC 1918)
    [[nodiscard]] constexpr bool is_private() const {
        // 10.0.0.0/8
        if (bytes[0] == 10)
            return true;
        // 172.16.0.0/12
        if (bytes[0] == 172 && (bytes[1] & 0xF0) == 16)
            return true;
        // 192.168.0.0/16
        if (bytes[0] == 192 && bytes[1] == 168)
            return true;
        return false;
    }

    /// @brief Check if address is in the given subnet
    [[nodiscard]] constexpr bool is_in_subnet(IPv4Address network, std::uint8_t prefix_len) const {
        if (prefix_len > 32)
            return false;
        std::uint32_t mask = prefix_len == 0 ? 0 : (~0U << (32 - prefix_len));
        return (to_uint32() & mask) == (network.to_uint32() & mask);
    }
};

/// @brief Common Ethernet types
enum class EtherType : std::uint16_t {
    IPv4 = 0x0800,
    ARP = 0x0806,
    VLAN = 0x8100,
    QinQ = 0x88A8,
    IPv6 = 0x86DD,
    PTP = 0x88F7,  ///< Precision Time Protocol (gPTP / IEEE 802.1AS)
};

/// @brief IP protocol numbers
enum class IpProtocol : std::uint8_t {
    ICMP = 1,
    TCP = 6,
    UDP = 17,
};

/// @brief Byte span type alias for raw packet data
using ByteSpan = std::span<const std::byte>;
using MutableByteSpan = std::span<std::byte>;

/// @brief Maximum Ethernet frame size (with VLAN tag)
constexpr std::size_t MAX_FRAME_SIZE = 1522;

/// @brief Minimum Ethernet frame size
constexpr std::size_t MIN_FRAME_SIZE = 64;

/// @brief Ethernet header size (without VLAN)
constexpr std::size_t ETHERNET_HEADER_SIZE = 14;

/// @brief VLAN tag size
constexpr std::size_t VLAN_TAG_SIZE = 4;

}  // namespace wadjet

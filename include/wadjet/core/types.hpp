#pragma once

/// @file types.hpp
/// @brief Core type definitions for Wadjet-Link

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace wadjet {

/// @brief MAC address (6 bytes)
struct MacAddress {
    std::array<std::uint8_t, 6> bytes{};

    /// @brief Create MAC address from bytes
    static constexpr MacAddress from_bytes(std::uint8_t b0, std::uint8_t b1,
                                           std::uint8_t b2, std::uint8_t b3,
                                           std::uint8_t b4, std::uint8_t b5) {
        return MacAddress{{b0, b1, b2, b3, b4, b5}};
    }

    /// @brief Parse MAC address from string "xx:xx:xx:xx:xx:xx"
    static auto from_string(std::string_view str) -> MacAddress;

    /// @brief Convert to string "xx:xx:xx:xx:xx:xx"
    [[nodiscard]] auto to_string() const -> std::string;

    /// @brief Check if this is a broadcast address (ff:ff:ff:ff:ff:ff)
    [[nodiscard]] constexpr bool is_broadcast() const {
        return bytes[0] == 0xff && bytes[1] == 0xff && bytes[2] == 0xff &&
               bytes[3] == 0xff && bytes[4] == 0xff && bytes[5] == 0xff;
    }

    /// @brief Check if this is a multicast address
    [[nodiscard]] constexpr bool is_multicast() const {
        return (bytes[0] & 0x01) != 0;
    }

    auto operator<=>(const MacAddress&) const = default;
};

/// @brief IPv4 address (4 bytes)
struct IPv4Address {
    std::array<std::uint8_t, 4> bytes{};

    /// @brief Create IPv4 address from bytes
    static constexpr IPv4Address from_bytes(std::uint8_t b0, std::uint8_t b1,
                                            std::uint8_t b2, std::uint8_t b3) {
        return IPv4Address{{b0, b1, b2, b3}};
    }

    /// @brief Parse IPv4 address from string "x.x.x.x"
    static auto from_string(std::string_view str) -> IPv4Address;

    /// @brief Convert to string "x.x.x.x"
    [[nodiscard]] auto to_string() const -> std::string;

    /// @brief Convert to 32-bit integer (network byte order)
    [[nodiscard]] constexpr std::uint32_t to_uint32() const {
        return (static_cast<std::uint32_t>(bytes[0]) << 24) |
               (static_cast<std::uint32_t>(bytes[1]) << 16) |
               (static_cast<std::uint32_t>(bytes[2]) << 8) |
               static_cast<std::uint32_t>(bytes[3]);
    }

    auto operator<=>(const IPv4Address&) const = default;
};

/// @brief Common Ethernet types
enum class EtherType : std::uint16_t {
    IPv4 = 0x0800,
    ARP = 0x0806,
    VLAN = 0x8100,
    QinQ = 0x88A8,
    IPv6 = 0x86DD,
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

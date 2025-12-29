#pragma once

/// @file byte_order.hpp
/// @brief Network byte order utilities

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>

namespace wadjet {

/// @brief Read a 16-bit value in network byte order (big-endian)
[[nodiscard]] inline constexpr std::uint16_t read_be16(const std::byte* data) {
    return (static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[0])) << 8) |
           static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[1]));
}

/// @brief Read a 32-bit value in network byte order (big-endian)
[[nodiscard]] inline constexpr std::uint32_t read_be32(const std::byte* data) {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[3]));
}

/// @brief Read a 64-bit value in network byte order (big-endian)
[[nodiscard]] inline constexpr std::uint64_t read_be64(const std::byte* data) {
    return (static_cast<std::uint64_t>(read_be32(data)) << 32) |
           static_cast<std::uint64_t>(read_be32(data + 4));
}

/// @brief Write a 16-bit value in network byte order (big-endian)
inline constexpr void write_be16(std::byte* data, std::uint16_t value) {
    data[0] = static_cast<std::byte>(value >> 8);
    data[1] = static_cast<std::byte>(value & 0xFF);
}

/// @brief Write a 32-bit value in network byte order (big-endian)
inline constexpr void write_be32(std::byte* data, std::uint32_t value) {
    data[0] = static_cast<std::byte>(value >> 24);
    data[1] = static_cast<std::byte>((value >> 16) & 0xFF);
    data[2] = static_cast<std::byte>((value >> 8) & 0xFF);
    data[3] = static_cast<std::byte>(value & 0xFF);
}

/// @brief Write a 64-bit value in network byte order (big-endian)
inline constexpr void write_be64(std::byte* data, std::uint64_t value) {
    write_be32(data, static_cast<std::uint32_t>(value >> 32));
    write_be32(data + 4, static_cast<std::uint32_t>(value & 0xFFFFFFFF));
}

/// @brief Read a 16-bit value in little-endian order
[[nodiscard]] inline constexpr std::uint16_t read_le16(const std::byte* data) {
    return static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[0])) |
           (static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[1])) << 8);
}

/// @brief Read a 32-bit value in little-endian order
[[nodiscard]] inline constexpr std::uint32_t read_le32(const std::byte* data) {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[0])) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[3])) << 24);
}

/// @brief Write a 16-bit value in little-endian order
inline constexpr void write_le16(std::byte* data, std::uint16_t value) {
    data[0] = static_cast<std::byte>(value & 0xFF);
    data[1] = static_cast<std::byte>(value >> 8);
}

/// @brief Write a 32-bit value in little-endian order
inline constexpr void write_le32(std::byte* data, std::uint32_t value) {
    data[0] = static_cast<std::byte>(value & 0xFF);
    data[1] = static_cast<std::byte>((value >> 8) & 0xFF);
    data[2] = static_cast<std::byte>((value >> 16) & 0xFF);
    data[3] = static_cast<std::byte>(value >> 24);
}

}  // namespace wadjet

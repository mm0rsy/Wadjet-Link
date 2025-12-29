#pragma once

/// @file address.hpp
/// @brief Generic network address template using CRTP

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace wadjet {

/// @brief CRTP base class for network addresses
///
/// Provides common functionality for fixed-size network addresses
/// using the Curiously Recurring Template Pattern (CRTP).
///
/// @tparam Derived The derived address type
/// @tparam N Size of the address in bytes
template <typename Derived, std::size_t N>
class AddressBase {
public:
    /// @brief Number of bytes in the address
    static constexpr std::size_t size = N;

    /// @brief The underlying storage type
    using StorageType = std::array<std::uint8_t, N>;

    /// @brief Default constructor (zero-initialized)
    constexpr AddressBase() = default;

    /// @brief Construct from byte array
    constexpr explicit AddressBase(const StorageType& data) : bytes(data) {}

    /// @brief Construct from span
    constexpr explicit AddressBase(std::span<const std::uint8_t, N> data) {
        std::copy(data.begin(), data.end(), bytes.begin());
    }

    /// @brief Get a span view of the bytes
    [[nodiscard]] constexpr std::span<const std::uint8_t, N> as_span() const {
        return std::span<const std::uint8_t, N>(bytes);
    }

    /// @brief Get a mutable span view of the bytes
    [[nodiscard]] constexpr std::span<std::uint8_t, N> as_mutable_span() {
        return std::span<std::uint8_t, N>(bytes);
    }

    /// @brief Check if address is all zeros
    [[nodiscard]] constexpr bool is_zero() const {
        return std::all_of(bytes.begin(), bytes.end(),
                          [](std::uint8_t b) { return b == 0; });
    }

    /// @brief Three-way comparison
    constexpr auto operator<=>(const AddressBase&) const = default;

    /// @brief Equality comparison
    constexpr bool operator==(const AddressBase&) const = default;

    /// @brief The raw bytes of the address
    StorageType bytes{};

protected:
    /// @brief Helper to parse hex string with delimiter
    /// @return true on success
    static bool parse_hex_with_delimiter(std::string_view str,
                                         StorageType& out,
                                         char delimiter,
                                         std::size_t chars_per_byte) {
        std::size_t expected_len = N * chars_per_byte + (N - 1);  // bytes + delimiters
        if (str.size() != expected_len) {
            return false;
        }

        std::size_t byte_idx = 0;
        std::size_t str_idx = 0;

        while (byte_idx < N) {
            // Parse hex digits
            std::uint8_t value = 0;
            for (std::size_t i = 0; i < chars_per_byte; ++i) {
                char c = str[str_idx++];
                std::uint8_t nibble = 0;
                if (c >= '0' && c <= '9') {
                    nibble = static_cast<std::uint8_t>(c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    nibble = static_cast<std::uint8_t>(c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    nibble = static_cast<std::uint8_t>(c - 'A' + 10);
                } else {
                    return false;
                }
                value = static_cast<std::uint8_t>((value << 4) | nibble);
            }
            out[byte_idx++] = value;

            // Skip delimiter (except after last byte)
            if (byte_idx < N) {
                if (str[str_idx++] != delimiter) {
                    return false;
                }
            }
        }
        return true;
    }

    /// @brief Helper to parse decimal string with delimiter
    static bool parse_decimal_with_delimiter(std::string_view str,
                                             StorageType& out,
                                             char delimiter) {
        std::size_t byte_idx = 0;
        std::size_t start = 0;

        for (std::size_t i = 0; i <= str.size() && byte_idx < N; ++i) {
            if (i == str.size() || str[i] == delimiter) {
                if (i == start) return false;  // Empty segment

                // Parse decimal number
                std::uint32_t value = 0;
                for (std::size_t j = start; j < i; ++j) {
                    char c = str[j];
                    if (c < '0' || c > '9') return false;
                    value = value * 10 + static_cast<std::uint32_t>(c - '0');
                    if (value > 255) return false;
                }
                out[byte_idx++] = static_cast<std::uint8_t>(value);
                start = i + 1;
            }
        }
        return byte_idx == N;
    }

    /// @brief Helper to format bytes as hex with delimiter
    [[nodiscard]] static std::string format_hex(const StorageType& data,
                                                char delimiter,
                                                bool lowercase = true) {
        static constexpr char hex_lower[] = "0123456789abcdef";
        static constexpr char hex_upper[] = "0123456789ABCDEF";
        const char* hex = lowercase ? hex_lower : hex_upper;

        std::string result;
        result.reserve(N * 3 - 1);

        for (std::size_t i = 0; i < N; ++i) {
            if (i > 0) result += delimiter;
            result += hex[(data[i] >> 4) & 0x0F];
            result += hex[data[i] & 0x0F];
        }
        return result;
    }

    /// @brief Helper to format bytes as decimal with delimiter
    [[nodiscard]] static std::string format_decimal(const StorageType& data,
                                                    char delimiter) {
        std::string result;
        result.reserve(N * 4);  // Max "255." per byte

        for (std::size_t i = 0; i < N; ++i) {
            if (i > 0) result += delimiter;
            result += std::to_string(data[i]);
        }
        return result;
    }
};

}  // namespace wadjet

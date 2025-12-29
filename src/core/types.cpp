#include "wadjet/core/types.hpp"

#include <charconv>
#include <cstdio>
#include <stdexcept>

namespace wadjet {

auto MacAddress::from_string(std::string_view str) -> MacAddress {
    MacAddress mac{};
    unsigned int b[6];

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    if (std::sscanf(str.data(), "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2],
                    &b[3], &b[4], &b[5]) != 6) {
        throw std::invalid_argument("Invalid MAC address format");
    }

    for (int i = 0; i < 6; ++i) {
        mac.bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(b[i]);
    }
    return mac;
}

auto MacAddress::to_string() const -> std::string {
    char buf[18];
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", bytes[0], bytes[1],
                  bytes[2], bytes[3], bytes[4], bytes[5]);
    return std::string(buf);
}

auto IPv4Address::from_string(std::string_view str) -> IPv4Address {
    IPv4Address addr{};
    unsigned int b[4];

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    if (std::sscanf(str.data(), "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]) != 4) {
        throw std::invalid_argument("Invalid IPv4 address format");
    }

    for (int i = 0; i < 4; ++i) {
        if (b[i] > 255) {
            throw std::invalid_argument("Invalid IPv4 address: octet out of range");
        }
        addr.bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(b[i]);
    }
    return addr;
}

auto IPv4Address::to_string() const -> std::string {
    char buf[16];
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
    return std::string(buf);
}

}  // namespace wadjet

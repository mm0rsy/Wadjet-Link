#pragma once

/// @file device.hpp
/// @brief Network device enumeration and information

#include "wadjet/core/result.hpp"
#include "wadjet/core/types.hpp"

#include <string>
#include <vector>

namespace wadjet::io {

/// @brief Network interface information
struct NetworkDevice {
    std::string name;          ///< Interface name (e.g., "eth0")
    std::string description;   ///< Description (if available)
    MacAddress mac;            ///< MAC address
    std::uint32_t index;       ///< Interface index
    std::uint32_t mtu;         ///< Maximum Transmission Unit
    bool is_up;                ///< Interface is up
    bool is_running;           ///< Interface is running
    bool is_loopback;          ///< Loopback interface
    bool supports_promiscuous; ///< Supports promiscuous mode
};

/// @brief Enumerate network devices
/// @return List of available network devices
auto enumerate_devices() -> Result<std::vector<NetworkDevice>>;

/// @brief Get a specific device by name
/// @param name Interface name (e.g., "eth0")
/// @return Device info or error if not found
auto get_device(const std::string& name) -> Result<NetworkDevice>;

/// @brief Get the default network device (first non-loopback, up device)
/// @return Device info or error if none available
auto get_default_device() -> Result<NetworkDevice>;

}  // namespace wadjet::io

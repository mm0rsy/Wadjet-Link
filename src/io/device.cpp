#include "wadjet/io/device.hpp"

#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/if_packet.h>
#include <netinet/ether.h>
#endif

namespace wadjet::io {

auto enumerate_devices() -> Result<std::vector<NetworkDevice>> {
    std::vector<NetworkDevice> devices;

    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        return Result<std::vector<NetworkDevice>>::err(
            Error{errno, "Failed to enumerate network interfaces"});
    }

    // Create a socket for ioctl calls
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        freeifaddrs(ifaddr);
        return Result<std::vector<NetworkDevice>>::err(
            Error{errno, "Failed to create socket"});
    }

    // Track which interfaces we've already processed
    std::vector<std::string> seen_names;

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name == nullptr) {
            continue;
        }

        std::string name(ifa->ifa_name);

        // Skip if already processed
        bool already_seen = false;
        for (const auto& seen : seen_names) {
            if (seen == name) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) {
            continue;
        }
        seen_names.push_back(name);

        NetworkDevice dev{};
        dev.name = name;
        dev.index = if_nametoindex(name.c_str());

        // Get interface flags
        ifreq ifr{};
        std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            dev.is_up = (ifr.ifr_flags & IFF_UP) != 0;
            dev.is_running = (ifr.ifr_flags & IFF_RUNNING) != 0;
            dev.is_loopback = (ifr.ifr_flags & IFF_LOOPBACK) != 0;
            dev.supports_promiscuous = true;  // Most interfaces support this
        }

        // Get MTU
        if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
            dev.mtu = static_cast<std::uint32_t>(ifr.ifr_mtu);
        }

        // Get MAC address
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            std::memcpy(dev.mac.bytes.data(), ifr.ifr_hwaddr.sa_data, 6);
        }

        devices.push_back(std::move(dev));
    }

    close(sock);
    freeifaddrs(ifaddr);

    return Result<std::vector<NetworkDevice>>::ok(std::move(devices));
}

auto get_device(const std::string& name) -> Result<NetworkDevice> {
    auto result = enumerate_devices();
    if (!result) {
        return Result<NetworkDevice>::err(std::move(result).error());
    }

    for (auto& dev : result.value()) {
        if (dev.name == name) {
            return Result<NetworkDevice>::ok(std::move(dev));
        }
    }

    return Result<NetworkDevice>::err(
        Error{-1, "Device not found: " + name});
}

auto get_default_device() -> Result<NetworkDevice> {
    auto result = enumerate_devices();
    if (!result) {
        return Result<NetworkDevice>::err(std::move(result).error());
    }

    for (auto& dev : result.value()) {
        if (!dev.is_loopback && dev.is_up && dev.is_running) {
            return Result<NetworkDevice>::ok(std::move(dev));
        }
    }

    return Result<NetworkDevice>::err(
        Error{-1, "No suitable network device found"});
}

}  // namespace wadjet::io

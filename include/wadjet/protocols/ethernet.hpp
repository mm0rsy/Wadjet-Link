#pragma once

/// @file ethernet.hpp
/// @brief Ethernet frame decoder with VLAN (802.1Q) support

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace wadjet::protocols::ethernet {

/// @brief Minimum Ethernet frame size (without FCS)
inline constexpr std::size_t MIN_FRAME_SIZE = 14;

/// @brief Maximum Ethernet frame size (without FCS)
inline constexpr std::size_t MAX_FRAME_SIZE = 1514;

/// @brief Jumbo frame size limit
inline constexpr std::size_t MAX_JUMBO_FRAME_SIZE = 9014;

/// @brief VLAN tag size (802.1Q)
inline constexpr std::size_t VLAN_TAG_SIZE = 4;

/// @brief Double VLAN tag ethertype (QinQ, 802.1ad)
inline constexpr std::uint16_t ETHERTYPE_QINQ = 0x88A8;

/// @brief VLAN tag information (802.1Q)
struct VlanTag {
    std::uint16_t tpid = 0;          ///< Tag Protocol ID (0x8100 or 0x88A8)
    std::uint16_t tci = 0;           ///< Tag Control Information

    /// @brief Priority Code Point (3 bits, 0-7)
    [[nodiscard]] constexpr std::uint8_t pcp() const {
        return static_cast<std::uint8_t>((tci >> 13) & 0x07);
    }

    /// @brief Drop Eligible Indicator (1 bit)
    [[nodiscard]] constexpr bool dei() const {
        return (tci & 0x1000) != 0;
    }

    /// @brief VLAN Identifier (12 bits, 0-4095)
    [[nodiscard]] constexpr std::uint16_t vid() const {
        return tci & 0x0FFF;
    }

    /// @brief Check if this is a valid VLAN tag
    [[nodiscard]] constexpr bool is_valid() const {
        return tpid == static_cast<std::uint16_t>(EtherType::VLAN) ||
               tpid == ETHERTYPE_QINQ;
    }
};

/// @brief Decoded Ethernet header
struct EthernetHeader : public IDecodedHeader {
    MacAddress dst_mac;              ///< Destination MAC address
    MacAddress src_mac;              ///< Source MAC address
    std::optional<VlanTag> vlan;     ///< Optional VLAN tag (outer)
    std::optional<VlanTag> vlan_inner; ///< Optional inner VLAN tag (QinQ)
    std::uint16_t ethertype = 0;     ///< Ethertype/Length field
    std::size_t header_len = 0;      ///< Total header length parsed

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override {
        return "Ethernet";
    }

    [[nodiscard]] std::size_t header_size() const override {
        return header_len;
    }

    [[nodiscard]] std::size_t payload_size() const override {
        return 0;  // Unknown from Ethernet header alone
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Check if frame has VLAN tag
    [[nodiscard]] bool has_vlan() const { return vlan.has_value(); }

    /// @brief Check if frame has double VLAN tag (QinQ)
    [[nodiscard]] bool has_qinq() const { return vlan_inner.has_value(); }

    /// @brief Get VLAN ID (0 if no VLAN)
    [[nodiscard]] std::uint16_t vlan_id() const {
        return vlan ? vlan->vid() : 0;
    }

    /// @brief Check if ethertype indicates an IP protocol
    [[nodiscard]] bool is_ipv4() const {
        return ethertype == static_cast<std::uint16_t>(EtherType::IPv4);
    }

    [[nodiscard]] bool is_ipv6() const {
        return ethertype == static_cast<std::uint16_t>(EtherType::IPv6);
    }

    [[nodiscard]] bool is_arp() const {
        return ethertype == static_cast<std::uint16_t>(EtherType::ARP);
    }
};

/// @brief Ethernet frame decoder
class EthernetDecoder : public DecoderBase<EthernetDecoder, EthernetHeader> {
public:
    [[nodiscard]] std::string_view name() const override { return "Ethernet"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        // Ethernet is typically the first layer, always try to decode
        return ctx.data.size() >= MIN_FRAME_SIZE;
    }

    /// @brief Decode Ethernet frame
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

private:
    /// @brief Parse VLAN tag at offset
    [[nodiscard]] static std::optional<VlanTag> parse_vlan(
        const DecodeContext& ctx, std::size_t offset);
};

/// @brief Global Ethernet decoder instance
inline const EthernetDecoder& ethernet_decoder() {
    static EthernetDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::ethernet

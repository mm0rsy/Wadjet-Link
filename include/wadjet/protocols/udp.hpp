#pragma once

/// @file udp.hpp
/// @brief UDP header decoder

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <string>

namespace wadjet::protocols::udp {

/// @brief UDP header size (always 8 bytes)
inline constexpr std::size_t HEADER_SIZE = 8;

/// @brief Well-known UDP ports
namespace ports {
inline constexpr std::uint16_t DNS = 53;
inline constexpr std::uint16_t DHCP_SERVER = 67;
inline constexpr std::uint16_t DHCP_CLIENT = 68;
inline constexpr std::uint16_t NTP = 123;
inline constexpr std::uint16_t SOMEIP_SD = 30490;  ///< SOME/IP Service Discovery
inline constexpr std::uint16_t DOIP = 13400;       ///< DoIP
}  // namespace ports

/// @brief Decoded UDP header
struct UdpHeader : public IDecodedHeader {
    std::uint16_t src_port = 0;  ///< Source port
    std::uint16_t dst_port = 0;  ///< Destination port
    std::uint16_t length = 0;    ///< Total datagram length (header + payload)
    std::uint16_t checksum = 0;  ///< UDP checksum
    bool checksum_valid = true;  ///< Whether checksum was validated

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "UDP"; }

    [[nodiscard]] std::size_t header_size() const override { return HEADER_SIZE; }

    [[nodiscard]] std::size_t payload_size() const override {
        if (length >= HEADER_SIZE) {
            return length - HEADER_SIZE;
        }
        return 0;
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Check if this might be SOME/IP-SD
    [[nodiscard]] bool is_someip_sd_port() const {
        return src_port == ports::SOMEIP_SD || dst_port == ports::SOMEIP_SD;
    }

    /// @brief Check if this might be DoIP
    [[nodiscard]] bool is_doip_port() const {
        return src_port == ports::DOIP || dst_port == ports::DOIP;
    }
};

/// @brief UDP header decoder
class UdpDecoder : public DecoderBase<UdpDecoder, UdpHeader> {
public:
    /// @brief Decoder options
    struct Options {
        bool validate_checksum;  ///< Validate UDP checksum (needs pseudo-header)
        Options() : validate_checksum(false) {}
    };

    explicit UdpDecoder(Options opts = Options()) : options_(opts) {}

    [[nodiscard]] std::string_view name() const override { return "UDP"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        // Check IP protocol field
        return ctx.layer_info.ip_protocol == static_cast<std::uint8_t>(IpProtocol::UDP);
    }

    /// @brief Decode UDP header
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

private:
    Options options_;
};

/// @brief Global UDP decoder instance
inline const UdpDecoder& udp_decoder() {
    static UdpDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::udp

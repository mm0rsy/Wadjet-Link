#pragma once

/// @file dispatcher.hpp
/// @brief Protocol dispatcher for chaining decoders

#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/decoder.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/gptp/gptp.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/someip_sd.hpp"
#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/udp.hpp"

#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <variant>
#include <vector>

namespace wadjet::protocols {

/// @brief Type alias for decoded header variant
using DecodedHeaderVariant =
    std::variant<ethernet::EthernetHeader, ipv4::IPv4Header, udp::UdpHeader, tcp::TcpHeader,
                 someip::SomeIpHeader, someip_sd::SomeIpSdHeader, doip::DoIPHeader,
                 gptp::GptpHeader, dds::RtpsHeader>;

/// @brief Result of a full protocol stack decode
struct DecodeStackResult {
    std::vector<DecodedHeaderVariant> layers;  ///< Decoded protocol layers
    std::span<const std::byte> payload;        ///< Final payload after all headers
    std::optional<DecodeError> error;          ///< First error encountered (if any)
    bool complete = true;                      ///< True if decode completed without errors

    /// @brief Check if specific protocol was decoded
    template <typename HeaderT>
    [[nodiscard]] bool has_layer() const {
        for (const auto& layer : layers) {
            if (std::holds_alternative<HeaderT>(layer)) {
                return true;
            }
        }
        return false;
    }

    /// @brief Get specific protocol layer (first match)
    template <typename HeaderT>
    [[nodiscard]] const HeaderT* get_layer() const {
        for (const auto& layer : layers) {
            if (auto* hdr = std::get_if<HeaderT>(&layer)) {
                return hdr;
            }
        }
        return nullptr;
    }

    /// @brief Get all layers of a specific type
    template <typename HeaderT>
    [[nodiscard]] std::vector<const HeaderT*> get_all_layers() const {
        std::vector<const HeaderT*> result;
        for (const auto& layer : layers) {
            if (auto* hdr = std::get_if<HeaderT>(&layer)) {
                result.push_back(hdr);
            }
        }
        return result;
    }
};

/// @brief Protocol dispatcher options
struct DispatcherOptions {
    bool stop_on_error = false;       ///< Stop decoding on first error
    bool decode_application = true;   ///< Decode app-layer (SOME/IP, DoIP)
    std::size_t max_layers = 16;      ///< Maximum number of protocol layers
    bool validate_ipv4_checksum = false; ///< Validate IPv4 header checksum
};

/// @brief Protocol dispatcher - chains decoders based on protocol fields
/// 
/// The dispatcher follows the protocol stack:
///   Ethernet -> VLAN? -> IPv4 -> UDP/TCP -> SOME/IP/DoIP
/// 
/// @example
///   ProtocolDispatcher dispatcher;
///   auto result = dispatcher.decode(packet_data);
///   if (auto* ip = result.get_layer<ipv4::IPv4Header>()) {
///       // Process IP header
///   }
class ProtocolDispatcher {
public:
    explicit ProtocolDispatcher(DispatcherOptions opts = DispatcherOptions{})
        : options_(opts), 
          ipv4_decoder_(ipv4::IPv4Decoder::Options{
              opts.validate_ipv4_checksum,  // validate_checksum
              true                          // allow_bad_checksum
          }) {}

    /// @brief Decode a complete protocol stack starting from Ethernet
    [[nodiscard]] DecodeStackResult decode(std::span<const std::byte> data) const;

    /// @brief Decode starting from a specific layer
    [[nodiscard]] DecodeStackResult decode_from_ip(std::span<const std::byte> data) const;

    /// @brief Get dispatcher options
    [[nodiscard]] const DispatcherOptions& options() const { return options_; }

    /// @brief Set dispatcher options
    void set_options(const DispatcherOptions& opts) { options_ = opts; }

private:
    /// @brief Decode transport layer (UDP/TCP)
    void decode_transport(DecodeStackResult& result,
                         std::span<const std::byte>& data,
                         std::uint8_t ip_protocol) const;

    /// @brief Decode application layer (SOME/IP, DoIP, SOME/IP-SD)
    void decode_application(DecodeStackResult& result,
                           std::span<const std::byte>& data,
                           std::uint16_t src_port,
                           std::uint16_t dst_port) const;

    /// @brief Decode gPTP (IEEE 802.1AS) layer
    void decode_gptp(DecodeStackResult& result, std::span<const std::byte>& data) const;

    /// @brief Decode DDS/RTPS layer
    void decode_rtps(DecodeStackResult& result, std::span<const std::byte>& data) const;

    DispatcherOptions options_;

    // Pre-instantiated decoders
    ethernet::EthernetDecoder eth_decoder_;
    ipv4::IPv4Decoder ipv4_decoder_;
    udp::UdpDecoder udp_decoder_;
    tcp::TcpDecoder tcp_decoder_;
    someip::SomeIpDecoder someip_decoder_;
    someip_sd::SomeIpSdDecoder someip_sd_decoder_;
    doip::DoIPDecoder doip_decoder_;
    gptp::GptpDecoder gptp_decoder_;
    dds::RtpsDecoder rtps_decoder_;
};

/// @brief Global dispatcher instance with default options
[[nodiscard]] inline ProtocolDispatcher& default_dispatcher() {
    static ProtocolDispatcher dispatcher;
    return dispatcher;
}

/// @brief Convenience function to decode a packet
[[nodiscard]] inline DecodeStackResult decode_packet(std::span<const std::byte> data) {
    return default_dispatcher().decode(data);
}

/// @brief Protocol filter predicate type
using ProtocolFilter = std::function<bool(const DecodeStackResult&)>;

/// @brief Create filter for packets containing a specific protocol
template <typename HeaderT>
[[nodiscard]] ProtocolFilter has_protocol() {
    return [](const DecodeStackResult& result) {
        return result.has_layer<HeaderT>();
    };
}

/// @brief Create filter for SOME/IP service ID
[[nodiscard]] inline ProtocolFilter someip_service(std::uint16_t service_id) {
    return [service_id](const DecodeStackResult& result) {
        if (auto* hdr = result.get_layer<someip::SomeIpHeader>()) {
            return hdr->service_id == service_id;
        }
        return false;
    };
}

/// @brief Create filter for SOME/IP method ID
[[nodiscard]] inline ProtocolFilter someip_method(std::uint16_t service_id,
                                                   std::uint16_t method_id) {
    return [service_id, method_id](const DecodeStackResult& result) {
        if (auto* hdr = result.get_layer<someip::SomeIpHeader>()) {
            return hdr->service_id == service_id && hdr->method_id == method_id;
        }
        return false;
    };
}

/// @brief Create filter for DoIP payload type
[[nodiscard]] inline ProtocolFilter doip_type(doip::PayloadType type) {
    return [type](const DecodeStackResult& result) {
        if (auto* hdr = result.get_layer<doip::DoIPHeader>()) {
            return hdr->payload_type == type;
        }
        return false;
    };
}

/// @brief Create filter for gPTP message type
[[nodiscard]] inline ProtocolFilter gptp_message(gptp::MessageType type) {
    return [type](const DecodeStackResult& result) {
        if (auto* hdr = result.get_layer<gptp::GptpHeader>()) {
            return hdr->message_type == type;
        }
        return false;
    };
}

/// @brief Create filter for gPTP domain
[[nodiscard]] inline ProtocolFilter gptp_domain(std::uint8_t domain) {
    return [domain](const DecodeStackResult& result) {
        if (auto* hdr = result.get_layer<gptp::GptpHeader>()) {
            return hdr->domain_number == domain;
        }
        return false;
    };
}

/// @brief Create filter for IP address (source or destination)
[[nodiscard]] inline ProtocolFilter ip_address(const IPv4Address& addr) {
    return [addr](const DecodeStackResult& result) {
        if (auto* hdr = result.get_layer<ipv4::IPv4Header>()) {
            return hdr->src_ip == addr || hdr->dst_ip == addr;
        }
        return false;
    };
}

/// @brief Create filter for IP address from uint32 (network byte order)
[[nodiscard]] inline ProtocolFilter ip_address(std::uint32_t addr) {
    return ip_address(IPv4Address::from_uint32(addr));
}

/// @brief Create filter for port (source or destination)
[[nodiscard]] inline ProtocolFilter port(std::uint16_t port_num) {
    return [port_num](const DecodeStackResult& result) {
        if (auto* udp_hdr = result.get_layer<udp::UdpHeader>()) {
            return udp_hdr->src_port == port_num || udp_hdr->dst_port == port_num;
        }
        if (auto* tcp_hdr = result.get_layer<tcp::TcpHeader>()) {
            return tcp_hdr->src_port == port_num || tcp_hdr->dst_port == port_num;
        }
        return false;
    };
}

/// @brief Combine filters with AND
[[nodiscard]] inline ProtocolFilter operator&&(ProtocolFilter lhs, ProtocolFilter rhs) {
    return [l = std::move(lhs), r = std::move(rhs)](const DecodeStackResult& result) {
        return l(result) && r(result);
    };
}

/// @brief Combine filters with OR
[[nodiscard]] inline ProtocolFilter operator||(ProtocolFilter lhs, ProtocolFilter rhs) {
    return [l = std::move(lhs), r = std::move(rhs)](const DecodeStackResult& result) {
        return l(result) || r(result);
    };
}

/// @brief Negate a filter
[[nodiscard]] inline ProtocolFilter operator!(ProtocolFilter filter) {
    return [f = std::move(filter)](const DecodeStackResult& result) {
        return !f(result);
    };
}

}  // namespace wadjet::protocols

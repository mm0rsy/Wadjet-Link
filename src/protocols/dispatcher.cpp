/// @file dispatcher.cpp
/// @brief Protocol dispatcher implementation

#include "wadjet/protocols/dispatcher.hpp"

#include "wadjet/protocols/dds/rtps_types.hpp"

namespace wadjet::protocols {

DecodeStackResult ProtocolDispatcher::decode(std::span<const std::byte> data) const {
    DecodeStackResult result;
    result.payload = data;

    if (data.empty()) {
        result.error = DecodeError(DecodeErrorCode::BufferTooSmall, 0, "Empty packet data");
        result.complete = false;
        return result;
    }

    // Layer 2: Ethernet
    DecodeContext eth_ctx;
    eth_ctx.data = data;
    auto eth_result = eth_decoder_.decode(eth_ctx);

    if (!eth_result) {
        result.error = eth_result.error();
        result.complete = false;
        if (options_.stop_on_error) {
            return result;
        }
    } else {
        result.layers.emplace_back(std::move(*eth_result));

        // Advance past Ethernet header
        const auto& eth = std::get<ethernet::EthernetHeader>(result.layers.back());
        data = data.subspan(eth.header_size());
        result.payload = data;

        // Check ethertype for next layer
        if (eth.ethertype == static_cast<std::uint16_t>(EtherType::IPv4)) {
            // Layer 3: IPv4
            DecodeContext ip_ctx;
            ip_ctx.data = data;
            ip_ctx.layer_info.ethertype = eth.ethertype;
            auto ip_result = ipv4_decoder_.decode(ip_ctx);

            if (!ip_result) {
                result.error = ip_result.error();
                result.complete = false;
                if (options_.stop_on_error) {
                    return result;
                }
            } else {
                result.layers.emplace_back(std::move(*ip_result));

                const auto& ip = std::get<ipv4::IPv4Header>(result.layers.back());
                data = data.subspan(ip.header_size());
                result.payload = data;

                // Layer 4: Transport (UDP/TCP)
                decode_transport(result, data, ip.protocol);
            }
        } else if (eth.ethertype == static_cast<std::uint16_t>(EtherType::PTP)) {
            // gPTP (IEEE 802.1AS) - directly over Ethernet
            decode_gptp(result, data);
        }
        // Could add IPv6 support here with ETHERTYPE_IPV6
    }

    // Mark as complete if we didn't encounter any errors
    if (!result.error.has_value()) {
        result.complete = true;
    }

    return result;
}

DecodeStackResult ProtocolDispatcher::decode_from_ip(std::span<const std::byte> data) const {
    DecodeStackResult result;
    result.payload = data;

    if (data.empty()) {
        result.error = DecodeError(DecodeErrorCode::BufferTooSmall, 0, "Empty packet data");
        result.complete = false;
        return result;
    }

    // Start from IPv4
    DecodeContext ip_ctx;
    ip_ctx.data = data;
    ip_ctx.layer_info.ethertype = static_cast<std::uint16_t>(EtherType::IPv4);
    auto ip_result = ipv4_decoder_.decode(ip_ctx);

    if (!ip_result) {
        result.error = ip_result.error();
        result.complete = false;
        return result;
    }

    result.layers.emplace_back(std::move(*ip_result));

    const auto& ip = std::get<ipv4::IPv4Header>(result.layers.back());
    data = data.subspan(ip.header_size());
    result.payload = data;

    // Transport layer
    decode_transport(result, data, ip.protocol);

    return result;
}

void ProtocolDispatcher::decode_transport(DecodeStackResult& result,
                                          std::span<const std::byte>& data,
                                          std::uint8_t ip_protocol) const {
    if (result.layers.size() >= options_.max_layers) {
        return;
    }

    std::uint16_t src_port = 0;
    std::uint16_t dst_port = 0;

    if (ip_protocol == static_cast<std::uint8_t>(IpProtocol::UDP)) {
        DecodeContext udp_ctx;
        udp_ctx.data = data;
        udp_ctx.layer_info.ip_protocol = ip_protocol;
        auto udp_result = udp_decoder_.decode(udp_ctx);

        if (!udp_result) {
            result.error = udp_result.error();
            result.complete = false;
            if (options_.stop_on_error) {
                return;
            }
        } else {
            result.layers.emplace_back(std::move(*udp_result));

            const auto& udp_hdr = std::get<udp::UdpHeader>(result.layers.back());
            src_port = udp_hdr.src_port;
            dst_port = udp_hdr.dst_port;
            data = data.subspan(udp_hdr.header_size());
            result.payload = data;

            // Application layer
            if (options_.decode_application) {
                decode_application(result, data, src_port, dst_port);
            }
        }
    } else if (ip_protocol == static_cast<std::uint8_t>(IpProtocol::TCP)) {
        DecodeContext tcp_ctx;
        tcp_ctx.data = data;
        tcp_ctx.layer_info.ip_protocol = ip_protocol;
        auto tcp_result = tcp_decoder_.decode(tcp_ctx);

        if (!tcp_result) {
            result.error = tcp_result.error();
            result.complete = false;
            if (options_.stop_on_error) {
                return;
            }
        } else {
            result.layers.emplace_back(std::move(*tcp_result));

            const auto& tcp_hdr = std::get<tcp::TcpHeader>(result.layers.back());
            src_port = tcp_hdr.src_port;
            dst_port = tcp_hdr.dst_port;
            data = data.subspan(tcp_hdr.header_size());
            result.payload = data;

            // Application layer (if not a pure control packet)
            if (options_.decode_application && !data.empty()) {
                decode_application(result, data, src_port, dst_port);
            }
        }
    }
    // Other protocols (ICMP, etc.) could be added here
}

void ProtocolDispatcher::decode_application(DecodeStackResult& result,
                                            std::span<const std::byte>& data,
                                            std::uint16_t src_port,
                                            std::uint16_t dst_port) const {
    if (data.empty() || result.layers.size() >= options_.max_layers) {
        return;
    }

    // Check for DoIP (port 13400)
    if (src_port == udp::ports::DOIP || dst_port == udp::ports::DOIP) {
        DecodeContext doip_ctx;
        doip_ctx.data = data;
        auto doip_result = doip_decoder_.decode(doip_ctx);

        if (doip_result) {
            result.layers.emplace_back(std::move(*doip_result));

            const auto& doip_hdr = std::get<doip::DoIPHeader>(result.layers.back());
            data = data.subspan(doip_hdr.header_size());
            result.payload = data;
        } else if (!result.error) {
            // Don't overwrite earlier errors
            result.error = doip_result.error();
            result.complete = false;
        }
        return;  // DoIP doesn't have further layers we decode
    }

    // Check for DDS/RTPS (ports 7400-7500 range, typically)
    if (dds::is_likely_rtps_port(src_port) || dds::is_likely_rtps_port(dst_port)) {
        // Verify RTPS magic before committing
        if (data.size() >= 4 && data[0] == std::byte{'R'} && data[1] == std::byte{'T'} &&
            data[2] == std::byte{'P'} && data[3] == std::byte{'S'}) {
            decode_rtps(result, data);
            return;
        }
    }

    // Check for SOME/IP-SD (port 30490)
    if (src_port == udp::ports::SOMEIP_SD || dst_port == udp::ports::SOMEIP_SD) {
        // First decode SOME/IP header
        DecodeContext someip_ctx;
        someip_ctx.data = data;
        auto someip_result = someip_decoder_.decode(someip_ctx);

        if (someip_result) {
            result.layers.emplace_back(std::move(*someip_result));

            const auto& someip_hdr = std::get<someip::SomeIpHeader>(result.layers.back());
            data = data.subspan(someip_hdr.header_size());

            // Check if this is actually SD (service ID 0xFFFF, method ID 0x8100)
            if (someip_hdr.is_service_discovery() &&
                result.layers.size() < options_.max_layers) {
                DecodeContext sd_ctx;
                sd_ctx.data = data;
                auto sd_result = someip_sd_decoder_.decode(sd_ctx);

                if (sd_result) {
                    result.layers.emplace_back(std::move(*sd_result));

                    const auto& sd_hdr = std::get<someip_sd::SomeIpSdHeader>(result.layers.back());
                    data = data.subspan(sd_hdr.header_size() + sd_hdr.payload_size());
                    result.payload = data;
                }
            } else {
                result.payload = data;
            }
        } else if (!result.error) {
            result.error = someip_result.error();
            result.complete = false;
        }
        return;
    }

    // Try generic SOME/IP on any other port (heuristic: check for valid SOME/IP magic)
    // SOME/IP doesn't have a magic number, so we try decoding and check validity
    if (data.size() >= someip::HEADER_SIZE) {
        DecodeContext someip_ctx;
        someip_ctx.data = data;
        auto someip_result = someip_decoder_.decode(someip_ctx);

        if (someip_result) {
            const auto& hdr = *someip_result;
            // Basic sanity check: length should be reasonable
            if (hdr.length >= 8 && hdr.length <= data.size()) {
                result.layers.emplace_back(std::move(*someip_result));

                const auto& someip_hdr = std::get<someip::SomeIpHeader>(result.layers.back());
                data = data.subspan(someip_hdr.header_size());
                result.payload = data;
            }
        }
        // Don't set error for failed heuristic decode
    }
}

void ProtocolDispatcher::decode_gptp(DecodeStackResult& result,
                                     std::span<const std::byte>& data) const {
    if (data.empty() || result.layers.size() >= options_.max_layers) {
        return;
    }

    DecodeContext gptp_ctx;
    gptp_ctx.data = data;
    gptp_ctx.layer_info.ethertype = static_cast<std::uint16_t>(EtherType::PTP);
    auto gptp_result = gptp_decoder_.decode(gptp_ctx);

    if (!gptp_result) {
        result.error = gptp_result.error();
        result.complete = false;
        if (options_.stop_on_error) {
            return;
        }
    } else {
        result.layers.emplace_back(std::move(*gptp_result));

        const auto& gptp_hdr = std::get<gptp::GptpHeader>(result.layers.back());
        // gPTP is typically the final layer - no further payload
        std::size_t consumed = gptp_hdr.message_length;
        if (consumed <= data.size()) {
            data = data.subspan(consumed);
        } else {
            data = {};
        }
        result.payload = data;
    }
}

void ProtocolDispatcher::decode_rtps(DecodeStackResult& result,
                                     std::span<const std::byte>& data) const {
    if (data.empty() || result.layers.size() >= options_.max_layers) {
        return;
    }

    DecodeContext rtps_ctx;
    rtps_ctx.data = data;
    auto rtps_result = rtps_decoder_.decode(rtps_ctx);

    if (!rtps_result) {
        result.error = rtps_result.error();
        result.complete = false;
        if (options_.stop_on_error) {
            return;
        }
    } else {
        result.layers.emplace_back(std::move(*rtps_result));
        // RTPS is the final application layer
        data = {};
        result.payload = data;
    }
}

}  // namespace wadjet::protocols

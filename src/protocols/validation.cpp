/// @file validation.cpp
/// @brief Implementation of cross-protocol validation framework

#include "wadjet/protocols/validation.hpp"

#include <algorithm>
#include <numeric>

namespace wadjet::protocols {

ProtocolValidator::ProtocolValidator(ValidationMode mode) : mode_(mode) {}

ValidationResult ProtocolValidator::validateLayering(
    const std::vector<ProtocolLayer>& layers) const {
    ValidationResult result(mode_);

    // Empty layer list is valid (no layers to validate)
    if (layers.empty()) {
        return result;
    }

    // Track seen protocols to detect unsupported sequences
    bool seen_ipv4 = false;
    bool seen_ipv6 = false;
    bool seen_transport = false;
    bool seen_application = false;

    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];

        // Layer 0 should be Ethernet or another link layer
        if (i == 0) {
            if (layer.name != "Ethernet" && layer.name != "VLAN" && layer.name != "PPP" &&
                layer.name != "Raw") {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "First layer must be link layer (Ethernet, VLAN, PPP, or Raw)");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
        }

        // Validate protocol progression
        if (layer.name == "Ethernet" || layer.name == "VLAN") {
            // Link layers OK
        } else if (layer.name == "IPv4") {
            if (seen_ipv6) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Cannot have IPv4 after IPv6");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            if (seen_transport || seen_application) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Network layer (IPv4) must come before transport layer");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            seen_ipv4 = true;
        } else if (layer.name == "IPv6") {
            if (seen_ipv4) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Cannot mix IPv4 and IPv6");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            if (seen_transport || seen_application) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Network layer (IPv6) must come before transport layer");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            seen_ipv6 = true;
        } else if (layer.name == "UDP" || layer.name == "TCP" || layer.name == "SCTP") {
            if (!seen_ipv4 && !seen_ipv6) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Transport layer must follow network layer (IPv4 or IPv6)");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            if (seen_application) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Cannot have multiple transport layers");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            seen_transport = true;
        } else if (layer.name == "SOME/IP" || layer.name == "SOME/IP-SD" || layer.name == "DoIP" ||
                   layer.name == "UDS" || layer.name == "DDS" || layer.name == "gPTP") {
            // Application layers
            if (layer.name != "gPTP" && !seen_transport && !seen_ipv4 && !seen_ipv6) {
                result.add_error(DecodeErrorCode::InvalidHeader, layer.offset,
                                 "Application layer must follow transport/network layers");
                if (mode_ == ValidationMode::Strict)
                    return result;
            }
            seen_application = true;
        }
    }

    return result;
}

ValidationResult ProtocolValidator::validateLengths(const std::vector<ProtocolLayer>& layers,
                                                    std::size_t total_packet_length) const {
    ValidationResult result(mode_);

    if (layers.empty()) {
        return result;
    }

    // Calculate total expected length from all layers
    std::size_t expected_total = 0;
    std::size_t last_end = 0;

    for (const auto& layer : layers) {
        const std::size_t layer_total = layer.header_length + layer.payload_length;

        // Check for gaps
        if (layer.offset > last_end && layer.offset != last_end) {
            result.add_error(DecodeErrorCode::InvalidLength, layer.offset,
                             "Gap detected between " + std::to_string(last_end) + " and " +
                                 std::to_string(layer.offset) + " in layer " + layer.name);
            if (mode_ == ValidationMode::Strict)
                return result;
        }

        last_end = layer.offset + layer_total;
        expected_total = std::max(expected_total, last_end);

        // Validate that payload_length is reasonable (header should not exceed total)
        if (layer.header_length > layer_total) {
            result.add_error(DecodeErrorCode::InvalidLength, layer.offset,
                             "Header length (" + std::to_string(layer.header_length) +
                                 ") exceeds total layer length (" + std::to_string(layer_total) +
                                 ") in " + layer.name);
            if (mode_ == ValidationMode::Strict)
                return result;
        }
    }

    // Check total packet length
    if (expected_total > total_packet_length) {
        result.add_error(DecodeErrorCode::InvalidLength, 0,
                         "Expected " + std::to_string(expected_total) +
                             " bytes but packet is only " + std::to_string(total_packet_length));
        if (mode_ == ValidationMode::Strict)
            return result;
    }

    // Check IPv4 total length field if present
    for (const auto& layer : layers) {
        if (layer.name == "IPv4") {
            // IPv4 total length is at offset 2-3 (in layer.offset + 2)
            // For now, we assume it matches the provided length
            // This could be enhanced to read the actual value from packet data
        }
    }

    return result;
}

ValidationResult ProtocolValidator::validateChecksums(
    const std::span<const std::byte>& packet_data, const std::vector<ProtocolLayer>& layers) const {
    ValidationResult result(mode_);

    if (packet_data.empty() || layers.empty()) {
        return result;
    }

    // Find specific layers if they exist
    const ProtocolLayer* ipv4_layer = nullptr;
    const ProtocolLayer* udp_layer = nullptr;
    const ProtocolLayer* tcp_layer = nullptr;

    for (const auto& layer : layers) {
        if (layer.name == "IPv4")
            ipv4_layer = &layer;
        else if (layer.name == "UDP")
            udp_layer = &layer;
        else if (layer.name == "TCP")
            tcp_layer = &layer;
    }

    // Validate IPv4 checksum
    if (ipv4_layer != nullptr) {
        if (!validate_ipv4_checksum(packet_data, *ipv4_layer)) {
            result.add_error(DecodeErrorCode::InvalidChecksum, ipv4_layer->offset,
                             "IPv4 header checksum mismatch");
            if (mode_ == ValidationMode::Strict)
                return result;
        }
    }

    // Validate UDP checksum
    if (udp_layer != nullptr) {
        if (!validate_udp_checksum(packet_data, *udp_layer, ipv4_layer)) {
            result.add_error(DecodeErrorCode::InvalidChecksum, udp_layer->offset,
                             "UDP checksum mismatch");
            if (mode_ == ValidationMode::Strict)
                return result;
        }
    }

    // Validate TCP checksum
    if (tcp_layer != nullptr) {
        if (!validate_tcp_checksum(packet_data, *tcp_layer, ipv4_layer)) {
            result.add_error(DecodeErrorCode::InvalidChecksum, tcp_layer->offset,
                             "TCP checksum mismatch");
            if (mode_ == ValidationMode::Strict)
                return result;
        }
    }

    return result;
}

bool ProtocolValidator::validate_ipv4_checksum(const std::span<const std::byte>& packet_data,
                                               const ProtocolLayer& layer) const {
    // IPv4 header is 20 bytes minimum
    if (layer.offset + 20 > packet_data.size()) {
        return false;
    }

    // Extract header (20 bytes)
    auto header_data = packet_data.subspan(layer.offset, 20);

    // Calculate checksum
    std::uint16_t calculated = calculate_ipv4_checksum(header_data);

    // Checksum is at offset 10-11 in the header
    std::uint16_t packet_checksum =
        static_cast<std::uint16_t>((static_cast<std::uint8_t>(header_data[10]) << 8) |
                                   static_cast<std::uint8_t>(header_data[11]));

    return calculated == packet_checksum;
}

bool ProtocolValidator::validate_udp_checksum(const std::span<const std::byte>& packet_data,
                                              const ProtocolLayer& udp_layer,
                                              const ProtocolLayer* ipv4_layer) const {
    // UDP header is 8 bytes minimum
    if (udp_layer.offset + 8 > packet_data.size()) {
        return false;
    }

    // UDP checksum of 0 means no checksum in IPv4
    std::uint16_t packet_checksum = static_cast<std::uint16_t>(
        (static_cast<std::uint8_t>(packet_data[udp_layer.offset + 6]) << 8) |
        static_cast<std::uint8_t>(packet_data[udp_layer.offset + 7]));

    if (packet_checksum == 0 && ipv4_layer != nullptr) {
        // No checksum specified for UDP over IPv4
        return true;
    }

    // Calculate pseudo-header checksum
    if (ipv4_layer == nullptr) {
        return true;  // Can't validate without IP layer
    }

    std::uint16_t calculated =
        calculate_pseudo_checksum(packet_data, *ipv4_layer,
                                  17,  // UDP protocol number
                                  udp_layer.header_length + udp_layer.payload_length);

    return calculated == packet_checksum;
}

bool ProtocolValidator::validate_tcp_checksum(const std::span<const std::byte>& packet_data,
                                              const ProtocolLayer& tcp_layer,
                                              const ProtocolLayer* ipv4_layer) const {
    // TCP header is 20 bytes minimum
    if (tcp_layer.offset + 20 > packet_data.size()) {
        return false;
    }

    if (ipv4_layer == nullptr) {
        return true;  // Can't validate without IP layer
    }

    // TCP checksum is at offset 16-17 in the header
    std::uint16_t packet_checksum = static_cast<std::uint16_t>(
        (static_cast<std::uint8_t>(packet_data[tcp_layer.offset + 16]) << 8) |
        static_cast<std::uint8_t>(packet_data[tcp_layer.offset + 17]));

    // Calculate pseudo-header checksum
    std::uint16_t calculated =
        calculate_pseudo_checksum(packet_data, *ipv4_layer,
                                  6,  // TCP protocol number
                                  tcp_layer.header_length + tcp_layer.payload_length);

    return calculated == packet_checksum;
}

std::uint16_t ProtocolValidator::calculate_ipv4_checksum(
    const std::span<const std::byte>& header_data) {
    if (header_data.size() < 20) {
        return 0;
    }

    // Sum all 16-bit words in the header
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < 20; i += 2) {
        std::uint32_t byte1 = static_cast<std::uint8_t>(header_data[i]);
        std::uint32_t byte2 = static_cast<std::uint8_t>(header_data[i + 1]);
        std::uint16_t word = static_cast<std::uint16_t>((byte1 << 8) | byte2);
        sum += word;
    }

    // Add carry bits
    while ((sum >> 16) > 0) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // One's complement
    return static_cast<std::uint16_t>(~sum);
}

std::uint16_t ProtocolValidator::calculate_pseudo_checksum(
    const std::span<const std::byte>& packet_data, const ProtocolLayer& ipv4_layer,
    std::uint8_t protocol_number, std::size_t transport_length) {
    if (ipv4_layer.offset + 20 > packet_data.size()) {
        return 0;
    }

    std::uint32_t sum = 0;

    // Source IP (offset 12-15 in IPv4 header)
    for (std::size_t i = 0; i < 4; i += 2) {
        std::uint32_t byte1 = static_cast<std::uint8_t>(packet_data[ipv4_layer.offset + 12 + i]);
        std::uint32_t byte2 = static_cast<std::uint8_t>(packet_data[ipv4_layer.offset + 13 + i]);
        std::uint16_t word = static_cast<std::uint16_t>((byte1 << 8) | byte2);
        sum += word;
    }

    // Destination IP (offset 16-19 in IPv4 header)
    for (std::size_t i = 0; i < 4; i += 2) {
        std::uint32_t byte1 = static_cast<std::uint8_t>(packet_data[ipv4_layer.offset + 16 + i]);
        std::uint32_t byte2 = static_cast<std::uint8_t>(packet_data[ipv4_layer.offset + 17 + i]);
        std::uint16_t word = static_cast<std::uint16_t>((byte1 << 8) | byte2);
        sum += word;
    }

    // Protocol number and transport length
    sum += protocol_number;
    sum += static_cast<std::uint16_t>(transport_length & 0xFFFF);
    if (transport_length > 0xFFFF) {
        sum += static_cast<std::uint16_t>(transport_length >> 16);
    }

    // Add carry bits
    while ((sum >> 16) > 0) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<std::uint16_t>(~sum);
}

}  // namespace wadjet::protocols

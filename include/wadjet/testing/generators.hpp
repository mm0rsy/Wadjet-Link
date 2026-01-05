/// @file generators.hpp
/// @brief Property-based testing generators for network packets
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This module provides random packet generators for property-based testing.
/// Generators can create valid or deliberately malformed packets for fuzzing
/// and testing edge cases.
///
/// Example:
/// @code
/// using namespace wadjet::testing::generators;
///
/// PacketGenerator gen(42);  // Seed for reproducibility
///
/// // Generate random valid UDP packets
/// for (int i = 0; i < 100; ++i) {
///     auto packet = gen.udp_packet();
///     EXPECT_THAT(packet, IsUDP());
/// }
///
/// // Generate SOME/IP packets with specific constraints
/// auto someip = gen.someip_packet()
///     .with_service_id(0x1234)
///     .with_message_type(MessageType::REQUEST)
///     .build();
/// @endcode

#pragma once

#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/uds/uds.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <vector>

namespace wadjet::testing::generators {

/// Random number generator wrapper with convenience methods
class Random {
public:
    explicit Random(std::uint64_t seed = std::random_device{}()) : engine_(seed), seed_(seed) {}

    /// Get the seed used (for reproducibility)
    [[nodiscard]] std::uint64_t seed() const { return seed_; }

    /// Generate random integer in range [min, max]
    template <typename T>
    T integer(T min, T max) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(engine_);
    }

    /// Generate random uint8_t
    std::uint8_t u8() { return integer<std::uint8_t>(0, 255); }

    /// Generate random uint16_t
    std::uint16_t u16() { return integer<std::uint16_t>(0, 65535); }

    /// Generate random uint32_t
    std::uint32_t u32() { return integer<std::uint32_t>(0, UINT32_MAX); }

    /// Generate random bytes
    std::vector<std::uint8_t> bytes(std::size_t count) {
        std::vector<std::uint8_t> result(count);
        for (auto& b : result) {
            b = u8();
        }
        return result;
    }

    /// Generate random MAC address
    std::array<std::uint8_t, 6> mac_address() {
        std::array<std::uint8_t, 6> mac;
        for (auto& b : mac) {
            b = u8();
        }
        // Clear multicast bit for unicast address
        mac[0] &= 0xFE;
        return mac;
    }

    /// Generate random IPv4 address
    std::uint32_t ipv4_address() { return u32(); }

    /// Generate random port number (1024-65535 for non-privileged)
    std::uint16_t port() { return integer<std::uint16_t>(1024, 65535); }

    /// Generate random port number (any)
    std::uint16_t any_port() { return u16(); }

    /// Pick random element from container
    template <typename Container>
    auto pick(const Container& c) -> typename Container::value_type {
        auto idx = integer<std::size_t>(0, c.size() - 1);
        return c[idx];
    }

    /// Return true with given probability (0.0 to 1.0)
    bool chance(double probability) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(engine_) < probability;
    }

private:
    std::mt19937_64 engine_;
    std::uint64_t seed_;
};

// Forward declarations
class EthernetBuilder;
class IPv4Builder;
class UDPBuilder;
class TCPBuilder;
class SOMEIPBuilder;
class DoIPBuilder;

/// Builder for Ethernet frames
class EthernetBuilder {
public:
    explicit EthernetBuilder(Random& rng) : rng_(rng) {
        dst_mac_ = rng.mac_address();
        src_mac_ = rng.mac_address();
        ethertype_ = 0x0800;  // IPv4 default
    }

    EthernetBuilder& with_dst_mac(std::array<std::uint8_t, 6> mac) {
        dst_mac_ = mac;
        return *this;
    }

    EthernetBuilder& with_src_mac(std::array<std::uint8_t, 6> mac) {
        src_mac_ = mac;
        return *this;
    }

    EthernetBuilder& with_ethertype(std::uint16_t type) {
        ethertype_ = type;
        return *this;
    }

    EthernetBuilder& with_vlan(std::uint16_t vlan_id, std::uint8_t pcp = 0) {
        has_vlan_ = true;
        vlan_id_ = vlan_id;
        vlan_pcp_ = pcp;
        return *this;
    }

    EthernetBuilder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    EthernetBuilder& with_random_payload(std::size_t size) {
        payload_ = rng_.bytes(size);
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> frame;

        // Destination MAC
        frame.insert(frame.end(), dst_mac_.begin(), dst_mac_.end());
        // Source MAC
        frame.insert(frame.end(), src_mac_.begin(), src_mac_.end());

        if (has_vlan_) {
            // VLAN tag (802.1Q)
            frame.push_back(0x81);
            frame.push_back(0x00);
            // TCI (PCP + DEI + VID)
            std::uint16_t tci = static_cast<std::uint16_t>((vlan_pcp_ << 13) | (vlan_id_ & 0x0FFF));
            frame.push_back(static_cast<std::uint8_t>(tci >> 8));
            frame.push_back(static_cast<std::uint8_t>(tci & 0xFF));
        }

        // Ethertype
        frame.push_back(static_cast<std::uint8_t>(ethertype_ >> 8));
        frame.push_back(static_cast<std::uint8_t>(ethertype_ & 0xFF));

        // Payload
        frame.insert(frame.end(), payload_.begin(), payload_.end());

        return frame;
    }

    [[nodiscard]] Packet build_packet() const {
        auto data = build();
        return Packet(std::move(data));
    }

private:
    Random& rng_;
    std::array<std::uint8_t, 6> dst_mac_;
    std::array<std::uint8_t, 6> src_mac_;
    std::uint16_t ethertype_ = 0x0800;
    bool has_vlan_ = false;
    std::uint16_t vlan_id_ = 0;
    std::uint8_t vlan_pcp_ = 0;
    std::vector<std::uint8_t> payload_;
};

/// Builder for IPv4 packets
class IPv4Builder {
public:
    explicit IPv4Builder(Random& rng) : rng_(rng) {
        src_ip_ = rng.ipv4_address();
        dst_ip_ = rng.ipv4_address();
        protocol_ = 17;  // UDP default
        ttl_ = 64;
    }

    IPv4Builder& with_src_ip(std::uint32_t ip) {
        src_ip_ = ip;
        return *this;
    }
    IPv4Builder& with_dst_ip(std::uint32_t ip) {
        dst_ip_ = ip;
        return *this;
    }
    IPv4Builder& with_protocol(std::uint8_t proto) {
        protocol_ = proto;
        return *this;
    }
    IPv4Builder& with_ttl(std::uint8_t ttl) {
        ttl_ = ttl;
        return *this;
    }
    IPv4Builder& with_id(std::uint16_t id) {
        id_ = id;
        return *this;
    }

    IPv4Builder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> packet;

        // Version (4) + IHL (5 = 20 bytes)
        packet.push_back(0x45);
        // DSCP + ECN
        packet.push_back(0x00);
        // Total length
        std::uint16_t total_len = 20 + static_cast<std::uint16_t>(payload_.size());
        packet.push_back(static_cast<std::uint8_t>(total_len >> 8));
        packet.push_back(static_cast<std::uint8_t>(total_len & 0xFF));
        // Identification
        packet.push_back(static_cast<std::uint8_t>(id_ >> 8));
        packet.push_back(static_cast<std::uint8_t>(id_ & 0xFF));
        // Flags + Fragment offset
        packet.push_back(0x40);  // Don't fragment
        packet.push_back(0x00);
        // TTL
        packet.push_back(ttl_);
        // Protocol
        packet.push_back(protocol_);
        // Checksum placeholder (will calculate)
        packet.push_back(0x00);
        packet.push_back(0x00);
        // Source IP
        packet.push_back(static_cast<std::uint8_t>(src_ip_ >> 24));
        packet.push_back(static_cast<std::uint8_t>(src_ip_ >> 16));
        packet.push_back(static_cast<std::uint8_t>(src_ip_ >> 8));
        packet.push_back(static_cast<std::uint8_t>(src_ip_ & 0xFF));
        // Destination IP
        packet.push_back(static_cast<std::uint8_t>(dst_ip_ >> 24));
        packet.push_back(static_cast<std::uint8_t>(dst_ip_ >> 16));
        packet.push_back(static_cast<std::uint8_t>(dst_ip_ >> 8));
        packet.push_back(static_cast<std::uint8_t>(dst_ip_ & 0xFF));

        // Calculate checksum
        std::uint32_t sum = 0;
        for (std::size_t i = 0; i < 20; i += 2) {
            sum += static_cast<std::uint32_t>((packet[i] << 8) | packet[i + 1]);
        }
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        std::uint16_t checksum = static_cast<std::uint16_t>(~sum);
        packet[10] = static_cast<std::uint8_t>(checksum >> 8);
        packet[11] = static_cast<std::uint8_t>(checksum & 0xFF);

        // Payload
        packet.insert(packet.end(), payload_.begin(), payload_.end());

        return packet;
    }

private:
    Random& rng_;
    std::uint32_t src_ip_;
    std::uint32_t dst_ip_;
    std::uint8_t protocol_ = 17;
    std::uint8_t ttl_ = 64;
    std::uint16_t id_ = 0;
    std::vector<std::uint8_t> payload_;
};

/// Builder for UDP datagrams
class UDPBuilder {
public:
    explicit UDPBuilder(Random& rng) : rng_(rng) {
        src_port_ = rng.port();
        dst_port_ = rng.port();
    }

    UDPBuilder& with_src_port(std::uint16_t port) {
        src_port_ = port;
        return *this;
    }
    UDPBuilder& with_dst_port(std::uint16_t port) {
        dst_port_ = port;
        return *this;
    }

    UDPBuilder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    UDPBuilder& with_random_payload(std::size_t size) {
        payload_ = rng_.bytes(size);
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> datagram;

        // Source port
        datagram.push_back(static_cast<std::uint8_t>(src_port_ >> 8));
        datagram.push_back(static_cast<std::uint8_t>(src_port_ & 0xFF));
        // Destination port
        datagram.push_back(static_cast<std::uint8_t>(dst_port_ >> 8));
        datagram.push_back(static_cast<std::uint8_t>(dst_port_ & 0xFF));
        // Length
        std::uint16_t length = 8 + static_cast<std::uint16_t>(payload_.size());
        datagram.push_back(static_cast<std::uint8_t>(length >> 8));
        datagram.push_back(static_cast<std::uint8_t>(length & 0xFF));
        // Checksum (0 = disabled)
        datagram.push_back(0x00);
        datagram.push_back(0x00);

        // Payload
        datagram.insert(datagram.end(), payload_.begin(), payload_.end());

        return datagram;
    }

private:
    Random& rng_;
    std::uint16_t src_port_;
    std::uint16_t dst_port_;
    std::vector<std::uint8_t> payload_;
};

/// Builder for TCP segments
class TCPBuilder {
public:
    explicit TCPBuilder(Random& rng) : rng_(rng) {
        src_port_ = rng.port();
        dst_port_ = rng.port();
        seq_num_ = rng.u32();
        ack_num_ = rng.u32();
    }

    TCPBuilder& with_src_port(std::uint16_t port) {
        src_port_ = port;
        return *this;
    }
    TCPBuilder& with_dst_port(std::uint16_t port) {
        dst_port_ = port;
        return *this;
    }
    TCPBuilder& with_seq(std::uint32_t seq) {
        seq_num_ = seq;
        return *this;
    }
    TCPBuilder& with_ack(std::uint32_t ack) {
        ack_num_ = ack;
        return *this;
    }
    TCPBuilder& with_flags(std::uint8_t flags) {
        flags_ = flags;
        return *this;
    }
    TCPBuilder& with_syn() {
        flags_ |= 0x02;
        return *this;
    }
    TCPBuilder& with_ack_flag() {
        flags_ |= 0x10;
        return *this;
    }
    TCPBuilder& with_fin() {
        flags_ |= 0x01;
        return *this;
    }
    TCPBuilder& with_rst() {
        flags_ |= 0x04;
        return *this;
    }
    TCPBuilder& with_psh() {
        flags_ |= 0x08;
        return *this;
    }

    TCPBuilder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> segment;

        // Source port
        segment.push_back(static_cast<std::uint8_t>(src_port_ >> 8));
        segment.push_back(static_cast<std::uint8_t>(src_port_ & 0xFF));
        // Destination port
        segment.push_back(static_cast<std::uint8_t>(dst_port_ >> 8));
        segment.push_back(static_cast<std::uint8_t>(dst_port_ & 0xFF));
        // Sequence number
        segment.push_back(static_cast<std::uint8_t>(seq_num_ >> 24));
        segment.push_back(static_cast<std::uint8_t>(seq_num_ >> 16));
        segment.push_back(static_cast<std::uint8_t>(seq_num_ >> 8));
        segment.push_back(static_cast<std::uint8_t>(seq_num_ & 0xFF));
        // Acknowledgment number
        segment.push_back(static_cast<std::uint8_t>(ack_num_ >> 24));
        segment.push_back(static_cast<std::uint8_t>(ack_num_ >> 16));
        segment.push_back(static_cast<std::uint8_t>(ack_num_ >> 8));
        segment.push_back(static_cast<std::uint8_t>(ack_num_ & 0xFF));
        // Data offset (5 = 20 bytes) + reserved
        segment.push_back(0x50);
        // Flags
        segment.push_back(flags_);
        // Window size
        segment.push_back(0xFF);
        segment.push_back(0xFF);
        // Checksum (0 for now)
        segment.push_back(0x00);
        segment.push_back(0x00);
        // Urgent pointer
        segment.push_back(0x00);
        segment.push_back(0x00);

        // Payload
        segment.insert(segment.end(), payload_.begin(), payload_.end());

        return segment;
    }

private:
    Random& rng_;
    std::uint16_t src_port_;
    std::uint16_t dst_port_;
    std::uint32_t seq_num_;
    std::uint32_t ack_num_;
    std::uint8_t flags_ = 0;
    std::vector<std::uint8_t> payload_;
};

/// Builder for SOME/IP messages
class SOMEIPBuilder {
public:
    explicit SOMEIPBuilder(Random& rng) : rng_(rng) {
        service_id_ = rng.u16();
        method_id_ = rng.u16();
        client_id_ = rng.u16();
        session_id_ = rng.u16();
        message_type_ = static_cast<std::uint8_t>(protocols::someip::MessageType::Request);
    }

    SOMEIPBuilder& with_service_id(std::uint16_t id) {
        service_id_ = id;
        return *this;
    }
    SOMEIPBuilder& with_method_id(std::uint16_t id) {
        method_id_ = id;
        return *this;
    }
    SOMEIPBuilder& with_client_id(std::uint16_t id) {
        client_id_ = id;
        return *this;
    }
    SOMEIPBuilder& with_session_id(std::uint16_t id) {
        session_id_ = id;
        return *this;
    }

    SOMEIPBuilder& with_message_type(protocols::someip::MessageType type) {
        message_type_ = static_cast<std::uint8_t>(type);
        return *this;
    }

    SOMEIPBuilder& with_return_code(std::uint8_t code) {
        return_code_ = code;
        return *this;
    }
    SOMEIPBuilder& with_protocol_version(std::uint8_t ver) {
        protocol_version_ = ver;
        return *this;
    }
    SOMEIPBuilder& with_interface_version(std::uint8_t ver) {
        interface_version_ = ver;
        return *this;
    }

    SOMEIPBuilder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    SOMEIPBuilder& with_random_payload(std::size_t size) {
        payload_ = rng_.bytes(size);
        return *this;
    }

    /// Generate a random valid message type
    SOMEIPBuilder& with_random_message_type() {
        static const std::vector<protocols::someip::MessageType> types = {
            protocols::someip::MessageType::Request,
            protocols::someip::MessageType::RequestNoReturn,
            protocols::someip::MessageType::Notification, protocols::someip::MessageType::Response,
            protocols::someip::MessageType::Error};
        message_type_ = static_cast<std::uint8_t>(rng_.pick(types));
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> message;

        // Service ID
        message.push_back(static_cast<std::uint8_t>(service_id_ >> 8));
        message.push_back(static_cast<std::uint8_t>(service_id_ & 0xFF));
        // Method ID
        message.push_back(static_cast<std::uint8_t>(method_id_ >> 8));
        message.push_back(static_cast<std::uint8_t>(method_id_ & 0xFF));
        // Length (header after length field + payload = 8 + payload)
        std::uint32_t length = 8 + static_cast<std::uint32_t>(payload_.size());
        message.push_back(static_cast<std::uint8_t>(length >> 24));
        message.push_back(static_cast<std::uint8_t>(length >> 16));
        message.push_back(static_cast<std::uint8_t>(length >> 8));
        message.push_back(static_cast<std::uint8_t>(length & 0xFF));
        // Client ID
        message.push_back(static_cast<std::uint8_t>(client_id_ >> 8));
        message.push_back(static_cast<std::uint8_t>(client_id_ & 0xFF));
        // Session ID
        message.push_back(static_cast<std::uint8_t>(session_id_ >> 8));
        message.push_back(static_cast<std::uint8_t>(session_id_ & 0xFF));
        // Protocol version
        message.push_back(protocol_version_);
        // Interface version
        message.push_back(interface_version_);
        // Message type
        message.push_back(message_type_);
        // Return code
        message.push_back(return_code_);

        // Payload
        message.insert(message.end(), payload_.begin(), payload_.end());

        return message;
    }

private:
    Random& rng_;
    std::uint16_t service_id_;
    std::uint16_t method_id_;
    std::uint16_t client_id_;
    std::uint16_t session_id_;
    std::uint8_t protocol_version_ = 1;
    std::uint8_t interface_version_ = 1;
    std::uint8_t message_type_;
    std::uint8_t return_code_ = 0;
    std::vector<std::uint8_t> payload_;
};

/// Builder for DoIP messages
class DoIPBuilder {
public:
    explicit DoIPBuilder(Random& rng) : rng_(rng) {
        payload_type_ = protocols::doip::PayloadType::DiagnosticMessage;
    }

    DoIPBuilder& with_protocol_version(std::uint8_t ver) {
        protocol_version_ = ver;
        return *this;
    }

    DoIPBuilder& with_payload_type(protocols::doip::PayloadType type) {
        payload_type_ = type;
        return *this;
    }

    DoIPBuilder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    /// Generate a random valid payload type
    DoIPBuilder& with_random_payload_type() {
        static const std::vector<protocols::doip::PayloadType> types = {
            protocols::doip::PayloadType::VehicleIdentificationRequest,
            protocols::doip::PayloadType::VehicleAnnouncementOrIdentificationResponse,
            protocols::doip::PayloadType::RoutingActivationRequest,
            protocols::doip::PayloadType::RoutingActivationResponse,
            protocols::doip::PayloadType::DiagnosticMessage,
            protocols::doip::PayloadType::DiagnosticMessagePositiveAck,
            protocols::doip::PayloadType::DiagnosticMessageNegativeAck};
        payload_type_ = rng_.pick(types);
        return *this;
    }

    /// Build a diagnostic message with source/target addresses
    DoIPBuilder& as_diagnostic_message(std::uint16_t source, std::uint16_t target) {
        payload_type_ = protocols::doip::PayloadType::DiagnosticMessage;
        diagnostic_source_ = source;
        diagnostic_target_ = target;
        is_diagnostic_ = true;
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> message;

        // Protocol version
        message.push_back(protocol_version_);
        // Inverse protocol version
        message.push_back(~protocol_version_);
        // Payload type
        auto type_val = static_cast<std::uint16_t>(payload_type_);
        message.push_back(static_cast<std::uint8_t>(type_val >> 8));
        message.push_back(static_cast<std::uint8_t>(type_val & 0xFF));

        // Build payload based on type
        std::vector<std::uint8_t> inner_payload;
        if (is_diagnostic_) {
            // Source address
            inner_payload.push_back(static_cast<std::uint8_t>(diagnostic_source_ >> 8));
            inner_payload.push_back(static_cast<std::uint8_t>(diagnostic_source_ & 0xFF));
            // Target address
            inner_payload.push_back(static_cast<std::uint8_t>(diagnostic_target_ >> 8));
            inner_payload.push_back(static_cast<std::uint8_t>(diagnostic_target_ & 0xFF));
            // User data
            inner_payload.insert(inner_payload.end(), payload_.begin(), payload_.end());
        } else {
            inner_payload = payload_;
        }

        // Payload length
        std::uint32_t length = static_cast<std::uint32_t>(inner_payload.size());
        message.push_back(static_cast<std::uint8_t>(length >> 24));
        message.push_back(static_cast<std::uint8_t>(length >> 16));
        message.push_back(static_cast<std::uint8_t>(length >> 8));
        message.push_back(static_cast<std::uint8_t>(length & 0xFF));

        // Payload
        message.insert(message.end(), inner_payload.begin(), inner_payload.end());

        return message;
    }

private:
    Random& rng_;
    std::uint8_t protocol_version_ = 0x02;
    protocols::doip::PayloadType payload_type_;
    std::vector<std::uint8_t> payload_;
    bool is_diagnostic_ = false;
    std::uint16_t diagnostic_source_ = 0;
    std::uint16_t diagnostic_target_ = 0;
};

/// Builder for UDS (ISO 14229) messages
class UDSBuilder {
public:
    explicit UDSBuilder(Random& rng) : rng_(rng) {
        service_id_ = protocols::uds::ServiceID::TesterPresent;
    }

    /// Set the service ID
    UDSBuilder& with_service_id(protocols::uds::ServiceID sid) {
        service_id_ = sid;
        return *this;
    }

    /// Set the sub-function (if applicable)
    UDSBuilder& with_sub_function(std::uint8_t sf) {
        sub_function_ = sf;
        has_sub_function_ = true;
        return *this;
    }

    /// Set suppress positive response flag
    UDSBuilder& with_suppress_positive_response(bool spr = true) {
        suppress_positive_response_ = spr;
        return *this;
    }

    /// Add payload data
    UDSBuilder& with_payload(std::vector<std::uint8_t> payload) {
        payload_ = std::move(payload);
        return *this;
    }

    /// Add random payload
    UDSBuilder& with_random_payload(std::size_t size) {
        payload_ = rng_.bytes(size);
        return *this;
    }

    /// Build as a request
    UDSBuilder& as_request() {
        is_response_ = false;
        is_negative_response_ = false;
        return *this;
    }

    /// Build as a positive response
    UDSBuilder& as_positive_response() {
        is_response_ = true;
        is_negative_response_ = false;
        return *this;
    }

    /// Build as a negative response with specific NRC
    UDSBuilder& as_negative_response(protocols::uds::NRC nrc) {
        is_response_ = true;
        is_negative_response_ = true;
        nrc_ = nrc;
        return *this;
    }

    /// Build Diagnostic Session Control request
    UDSBuilder& as_diagnostic_session_control(
        protocols::uds::SessionType session =
            protocols::uds::SessionType::ExtendedDiagnosticSession) {
        service_id_ = protocols::uds::ServiceID::DiagnosticSessionControl;
        sub_function_ = static_cast<std::uint8_t>(session);
        has_sub_function_ = true;
        return *this;
    }

    /// Build Security Access request seed
    UDSBuilder& as_security_access_request_seed(std::uint8_t level = 0x01) {
        service_id_ = protocols::uds::ServiceID::SecurityAccess;
        sub_function_ = level;  // Odd = request seed
        has_sub_function_ = true;
        return *this;
    }

    /// Build Security Access send key
    UDSBuilder& as_security_access_send_key(std::uint8_t level, std::vector<std::uint8_t> key) {
        service_id_ = protocols::uds::ServiceID::SecurityAccess;
        sub_function_ = level + 1;  // Even = send key
        has_sub_function_ = true;
        payload_ = std::move(key);
        return *this;
    }

    /// Build Read Data By Identifier request
    UDSBuilder& as_read_data_by_identifier(const std::vector<std::uint16_t>& dids) {
        service_id_ = protocols::uds::ServiceID::ReadDataByIdentifier;
        has_sub_function_ = false;
        payload_.clear();
        for (auto did : dids) {
            payload_.push_back(static_cast<std::uint8_t>(did >> 8));
            payload_.push_back(static_cast<std::uint8_t>(did & 0xFF));
        }
        return *this;
    }

    /// Build Write Data By Identifier request
    UDSBuilder& as_write_data_by_identifier(std::uint16_t did, std::vector<std::uint8_t> data) {
        service_id_ = protocols::uds::ServiceID::WriteDataByIdentifier;
        has_sub_function_ = false;
        payload_.clear();
        payload_.push_back(static_cast<std::uint8_t>(did >> 8));
        payload_.push_back(static_cast<std::uint8_t>(did & 0xFF));
        payload_.insert(payload_.end(), data.begin(), data.end());
        return *this;
    }

    /// Build ECU Reset request
    UDSBuilder& as_ecu_reset(
        protocols::uds::ResetType type = protocols::uds::ResetType::HardReset) {
        service_id_ = protocols::uds::ServiceID::ECUReset;
        sub_function_ = static_cast<std::uint8_t>(type);
        has_sub_function_ = true;
        return *this;
    }

    /// Build Tester Present request
    UDSBuilder& as_tester_present(bool suppress_response = true) {
        service_id_ = protocols::uds::ServiceID::TesterPresent;
        sub_function_ = 0x00;
        has_sub_function_ = true;
        suppress_positive_response_ = suppress_response;
        return *this;
    }

    /// Build Routine Control request
    UDSBuilder& as_routine_control(protocols::uds::RoutineControlType control_type,
                                   std::uint16_t routine_id,
                                   std::vector<std::uint8_t> option_record = {}) {
        service_id_ = protocols::uds::ServiceID::RoutineControl;
        sub_function_ = static_cast<std::uint8_t>(control_type);
        has_sub_function_ = true;
        payload_.clear();
        payload_.push_back(static_cast<std::uint8_t>(routine_id >> 8));
        payload_.push_back(static_cast<std::uint8_t>(routine_id & 0xFF));
        payload_.insert(payload_.end(), option_record.begin(), option_record.end());
        return *this;
    }

    /// Generate a random valid service
    UDSBuilder& with_random_service() {
        static const std::vector<protocols::uds::ServiceID> services = {
            protocols::uds::ServiceID::DiagnosticSessionControl,
            protocols::uds::ServiceID::ECUReset,
            protocols::uds::ServiceID::SecurityAccess,
            protocols::uds::ServiceID::TesterPresent,
            protocols::uds::ServiceID::ReadDataByIdentifier,
            protocols::uds::ServiceID::WriteDataByIdentifier,
            protocols::uds::ServiceID::RoutineControl,
            protocols::uds::ServiceID::ReadDTCInformation,
            protocols::uds::ServiceID::ClearDiagnosticInformation,
        };
        service_id_ = rng_.pick(services);
        return *this;
    }

    /// Generate random DID in specified range
    UDSBuilder& with_random_did_range(std::uint16_t min_did = 0xF100,
                                      std::uint16_t max_did = 0xF1FF) {
        auto did = rng_.integer<std::uint16_t>(min_did, max_did);
        service_id_ = protocols::uds::ServiceID::ReadDataByIdentifier;
        has_sub_function_ = false;
        payload_.clear();
        payload_.push_back(static_cast<std::uint8_t>(did >> 8));
        payload_.push_back(static_cast<std::uint8_t>(did & 0xFF));
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> message;

        if (is_negative_response_) {
            // Negative response: 0x7F <rejected-sid> <nrc>
            message.push_back(0x7F);
            message.push_back(static_cast<std::uint8_t>(service_id_));
            message.push_back(static_cast<std::uint8_t>(nrc_));
            return message;
        }

        // Service ID (with +0x40 for positive response)
        std::uint8_t sid = static_cast<std::uint8_t>(service_id_);
        if (is_response_) {
            sid += 0x40;
        }
        message.push_back(sid);

        // Sub-function (if present)
        if (has_sub_function_) {
            std::uint8_t sf = sub_function_;
            if (suppress_positive_response_ && !is_response_) {
                sf |= 0x80;  // Set SPR bit
            }
            message.push_back(sf);
        }

        // Payload
        message.insert(message.end(), payload_.begin(), payload_.end());

        return message;
    }

private:
    Random& rng_;
    protocols::uds::ServiceID service_id_;
    std::uint8_t sub_function_ = 0;
    bool has_sub_function_ = false;
    bool suppress_positive_response_ = false;
    bool is_response_ = false;
    bool is_negative_response_ = false;
    protocols::uds::NRC nrc_ = protocols::uds::NRC::GeneralReject;
    std::vector<std::uint8_t> payload_;
};

/// Main packet generator class
class PacketGenerator {
public:
    explicit PacketGenerator(std::uint64_t seed = std::random_device{}()) : rng_(seed) {}

    /// Get the random generator (for custom generation)
    Random& rng() { return rng_; }

    /// Get seed for reproducibility
    [[nodiscard]] std::uint64_t seed() const { return rng_.seed(); }

    // ==========================================================================
    // Builder factory methods
    // ==========================================================================

    EthernetBuilder ethernet() { return EthernetBuilder(rng_); }
    IPv4Builder ipv4() { return IPv4Builder(rng_); }
    UDPBuilder udp() { return UDPBuilder(rng_); }
    TCPBuilder tcp() { return TCPBuilder(rng_); }
    SOMEIPBuilder someip() { return SOMEIPBuilder(rng_); }
    DoIPBuilder doip() { return DoIPBuilder(rng_); }
    UDSBuilder uds() { return UDSBuilder(rng_); }

    // ==========================================================================
    // Convenience methods for complete packets
    // ==========================================================================

    /// Generate a complete random UDP packet (Ethernet + IPv4 + UDP)
    Packet udp_packet(std::size_t payload_size = 0) {
        auto udp_payload =
            payload_size > 0 ? rng_.bytes(payload_size) : std::vector<std::uint8_t>{};
        auto udp_data = UDPBuilder(rng_).with_payload(udp_payload).build();
        auto ipv4_data = IPv4Builder(rng_).with_protocol(17).with_payload(udp_data).build();
        auto eth_data =
            EthernetBuilder(rng_).with_ethertype(0x0800).with_payload(ipv4_data).build();
        return Packet(std::move(eth_data));
    }

    /// Generate a complete random TCP packet (Ethernet + IPv4 + TCP)
    Packet tcp_packet(std::size_t payload_size = 0) {
        auto tcp_payload =
            payload_size > 0 ? rng_.bytes(payload_size) : std::vector<std::uint8_t>{};
        auto tcp_data = TCPBuilder(rng_).with_payload(tcp_payload).build();
        auto ipv4_data = IPv4Builder(rng_).with_protocol(6).with_payload(tcp_data).build();
        auto eth_data =
            EthernetBuilder(rng_).with_ethertype(0x0800).with_payload(ipv4_data).build();
        return Packet(std::move(eth_data));
    }

    /// Generate a complete SOME/IP over UDP packet
    Packet someip_udp_packet(std::size_t payload_size = 0) {
        auto someip_data = SOMEIPBuilder(rng_)
                               .with_random_message_type()
                               .with_random_payload(payload_size)
                               .build();
        auto udp_data = UDPBuilder(rng_).with_payload(someip_data).build();
        auto ipv4_data = IPv4Builder(rng_).with_protocol(17).with_payload(udp_data).build();
        auto eth_data =
            EthernetBuilder(rng_).with_ethertype(0x0800).with_payload(ipv4_data).build();
        return Packet(std::move(eth_data));
    }

    /// Generate a complete DoIP over TCP packet
    Packet doip_tcp_packet(std::size_t payload_size = 0) {
        auto doip_data = DoIPBuilder(rng_)
                             .with_random_payload_type()
                             .with_payload(rng_.bytes(payload_size))
                             .build();
        auto tcp_data = TCPBuilder(rng_).with_payload(doip_data).build();
        auto ipv4_data = IPv4Builder(rng_).with_protocol(6).with_payload(tcp_data).build();
        auto eth_data =
            EthernetBuilder(rng_).with_ethertype(0x0800).with_payload(ipv4_data).build();
        return Packet(std::move(eth_data));
    }

    /// Generate a complete SOME/IP packet with specific service ID
    Packet someip_service_packet(
        std::uint16_t service_id,
        protocols::someip::MessageType msg_type = protocols::someip::MessageType::Request) {
        auto someip_data =
            SOMEIPBuilder(rng_).with_service_id(service_id).with_message_type(msg_type).build();
        auto udp_data = UDPBuilder(rng_).with_payload(someip_data).build();
        auto ipv4_data = IPv4Builder(rng_).with_protocol(17).with_payload(udp_data).build();
        auto eth_data =
            EthernetBuilder(rng_).with_ethertype(0x0800).with_payload(ipv4_data).build();
        return Packet(std::move(eth_data));
    }

    /// Generate a DoIP diagnostic message packet
    Packet doip_diagnostic_packet(std::uint16_t source, std::uint16_t target,
                                  const std::vector<std::uint8_t>& uds_data = {}) {
        auto doip_data =
            DoIPBuilder(rng_).as_diagnostic_message(source, target).with_payload(uds_data).build();
        auto tcp_data = TCPBuilder(rng_).with_payload(doip_data).build();
        auto ipv4_data = IPv4Builder(rng_).with_protocol(6).with_payload(tcp_data).build();
        auto eth_data =
            EthernetBuilder(rng_).with_ethertype(0x0800).with_payload(ipv4_data).build();
        return Packet(std::move(eth_data));
    }

    // ==========================================================================
    // UDS packet generation methods
    // ==========================================================================

    /// Generate a complete DoIP + UDS packet with random UDS service
    Packet uds_doip_packet(std::uint16_t source = 0x0E00, std::uint16_t target = 0x0001) {
        auto uds_data = UDSBuilder(rng_).with_random_service().build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate DoIP + UDS Diagnostic Session Control request
    Packet uds_session_control_packet(protocols::uds::SessionType session =
                                          protocols::uds::SessionType::ExtendedDiagnosticSession,
                                      std::uint16_t source = 0x0E00,
                                      std::uint16_t target = 0x0001) {
        auto uds_data = UDSBuilder(rng_).as_diagnostic_session_control(session).build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate DoIP + UDS Security Access request seed
    Packet uds_security_request_seed_packet(std::uint8_t level = 0x01,
                                            std::uint16_t source = 0x0E00,
                                            std::uint16_t target = 0x0001) {
        auto uds_data = UDSBuilder(rng_).as_security_access_request_seed(level).build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate DoIP + UDS Read Data By Identifier request
    Packet uds_read_did_packet(const std::vector<std::uint16_t>& dids,
                               std::uint16_t source = 0x0E00, std::uint16_t target = 0x0001) {
        auto uds_data = UDSBuilder(rng_).as_read_data_by_identifier(dids).build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate DoIP + UDS Tester Present request
    Packet uds_tester_present_packet(std::uint16_t source = 0x0E00, std::uint16_t target = 0x0001,
                                     bool suppress_response = true) {
        auto uds_data = UDSBuilder(rng_).as_tester_present(suppress_response).build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate DoIP + UDS ECU Reset request
    Packet uds_ecu_reset_packet(
        protocols::uds::ResetType type = protocols::uds::ResetType::HardReset,
        std::uint16_t source = 0x0E00, std::uint16_t target = 0x0001) {
        auto uds_data = UDSBuilder(rng_).as_ecu_reset(type).build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate DoIP + UDS Negative Response
    Packet uds_negative_response_packet(protocols::uds::ServiceID rejected_service,
                                        protocols::uds::NRC nrc,
                                        std::uint16_t source = 0x0001,    // ECU responding
                                        std::uint16_t target = 0x0E00) {  // To tester
        auto uds_data =
            UDSBuilder(rng_).with_service_id(rejected_service).as_negative_response(nrc).build();
        return doip_diagnostic_packet(source, target, uds_data);
    }

    /// Generate random UDS request (any valid service)
    std::vector<std::uint8_t> random_uds_request() {
        return UDSBuilder(rng_).with_random_service().as_request().build();
    }

    /// Generate random DID in OEM-specific range
    std::vector<std::uint8_t> random_oem_did_request() {
        return UDSBuilder(rng_).with_random_did_range(0xF100, 0xF1FF).as_request().build();
    }

    /// Generate random DID in vehicle identification range
    std::vector<std::uint8_t> random_vehicle_id_request() {
        return UDSBuilder(rng_).with_random_did_range(0xF190, 0xF19F).as_request().build();
    }

    // ==========================================================================
    // Fuzzing / malformed packet generation
    // ==========================================================================

    /// Generate packet with truncated headers
    Packet truncated_packet(std::size_t max_size = 20) {
        auto size = rng_.integer<std::size_t>(1, max_size);
        return Packet(rng_.bytes(size));
    }

    /// Generate packet with invalid checksums
    Packet corrupted_ipv4_packet() {
        auto packet = udp_packet(10);
        auto& data = const_cast<std::vector<std::uint8_t>&>(
            *reinterpret_cast<const std::vector<std::uint8_t>*>(&packet.data()[0]));
        // Corrupt the IP checksum (bytes 24-25 in Ethernet frame)
        if (data.size() > 25) {
            data[24] ^= 0xFF;
            data[25] ^= 0xFF;
        }
        return packet;
    }

    /// Generate completely random bytes (for fuzzing)
    Packet random_bytes(std::size_t size) { return Packet(rng_.bytes(size)); }

private:
    Random rng_;
};

}  // namespace wadjet::testing::generators

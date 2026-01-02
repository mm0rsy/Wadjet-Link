/// @file matchers.hpp
/// @brief gMock-style packet matchers for Wadjet-Link
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This header provides gMock-compatible matchers for protocol assertions
/// in tests. Use these matchers with EXPECT_THAT/ASSERT_THAT for expressive
/// packet validation.
///
/// Example:
/// @code
/// using namespace wadjet::testing;
///
/// auto pkt = capture.next_packet();
/// EXPECT_THAT(pkt, AllOf(
///     HasEthertype(0x0800),         // IPv4
///     IsUDP(),                       // UDP transport
///     HasSOMEIPServiceId(0x1234),   // Specific service
///     IsSOMEIPRequest()             // Request message
/// ));
/// @endcode

#pragma once

#include "wadjet/core/types.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/dds/rtps_messages.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/uds/uds.hpp"

#include <gmock/gmock.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace wadjet::testing {

// =============================================================================
// Helper to extract raw bytes from any packet-like object
// =============================================================================

namespace detail {

/// @brief Extract raw data span from wadjet::Packet
inline std::span<const std::byte> get_packet_data(const wadjet::Packet& pkt) {
    return pkt.data();
}

/// @brief Extract raw data span from span of bytes
inline std::span<const std::byte> get_packet_data(std::span<const std::byte> pkt) {
    return pkt;
}

/// @brief Extract raw data span from span of uint8_t
inline std::span<const std::byte> get_packet_data(std::span<const uint8_t> pkt) {
    return std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(pkt.data()), pkt.size());
}

/// @brief Extract raw data span from vector of bytes
inline std::span<const std::byte> get_packet_data(const std::vector<std::byte>& pkt) {
    return std::span<const std::byte>(pkt);
}

/// @brief Extract raw data span from vector of uint8_t
inline std::span<const std::byte> get_packet_data(const std::vector<uint8_t>& pkt) {
    return std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(pkt.data()), pkt.size());
}

/// @brief Generic fallback for array types
template <std::size_t N>
std::span<const std::byte> get_packet_data(const std::array<uint8_t, N>& pkt) {
    return std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(pkt.data()), pkt.size());
}

/// @brief Decode packet and return result
inline protocols::DecodeStackResult decode(std::span<const std::byte> data) {
    return protocols::decode_packet(data);
}

}  // namespace detail

// =============================================================================
// Ethernet Matchers
// =============================================================================

/// @brief Matcher: packet has specific EtherType
class HasEthertypeMatcher {
public:
    explicit HasEthertypeMatcher(std::uint16_t ethertype) : expected_(ethertype) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ethernet::EthernetHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have Ethernet header";
            }
            return false;
        }

        const auto* eth = result.get_layer<protocols::ethernet::EthernetHeader>();
        if (listener->IsInterested()) {
            *listener << "has ethertype 0x" << std::hex << eth->ethertype;
        }
        return eth->ethertype == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has ethertype 0x" << std::hex << std::setw(4) << std::setfill('0') << expected_;
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have ethertype 0x" << std::hex << std::setw(4) << std::setfill('0')
            << expected_;
    }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasEthertypeMatcher> HasEthertype(std::uint16_t ethertype) {
    return ::testing::MakePolymorphicMatcher(HasEthertypeMatcher(ethertype));
}

/// @brief Matcher: packet has specific source MAC address
class HasSourceMacMatcher {
public:
    explicit HasSourceMacMatcher(const MacAddress& mac) : expected_(mac) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ethernet::EthernetHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have Ethernet header";
            }
            return false;
        }

        const auto* eth = result.get_layer<protocols::ethernet::EthernetHeader>();
        if (listener->IsInterested()) {
            *listener << "has source MAC " << eth->src_mac.to_string();
        }
        return eth->src_mac == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has source MAC " << expected_.to_string();
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have source MAC " << expected_.to_string();
    }

private:
    MacAddress expected_;
};

inline ::testing::PolymorphicMatcher<HasSourceMacMatcher> HasSourceMac(const MacAddress& mac) {
    return ::testing::MakePolymorphicMatcher(HasSourceMacMatcher(mac));
}

/// @brief Matcher: packet has specific destination MAC address
class HasDestMacMatcher {
public:
    explicit HasDestMacMatcher(const MacAddress& mac) : expected_(mac) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ethernet::EthernetHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have Ethernet header";
            }
            return false;
        }

        const auto* eth = result.get_layer<protocols::ethernet::EthernetHeader>();
        if (listener->IsInterested()) {
            *listener << "has destination MAC " << eth->dst_mac.to_string();
        }
        return eth->dst_mac == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has destination MAC " << expected_.to_string();
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have destination MAC " << expected_.to_string();
    }

private:
    MacAddress expected_;
};

inline ::testing::PolymorphicMatcher<HasDestMacMatcher> HasDestMac(const MacAddress& mac) {
    return ::testing::MakePolymorphicMatcher(HasDestMacMatcher(mac));
}

/// @brief Matcher: packet has VLAN tag
class HasVlanMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ethernet::EthernetHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have Ethernet header";
            }
            return false;
        }

        const auto* eth = result.get_layer<protocols::ethernet::EthernetHeader>();
        bool has_vlan = eth->has_vlan();
        if (listener->IsInterested()) {
            *listener << (has_vlan ? "has VLAN tag" : "does not have VLAN tag");
        }
        return has_vlan;
    }

    void DescribeTo(std::ostream* os) const { *os << "has VLAN tag"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "does not have VLAN tag"; }
};

inline ::testing::PolymorphicMatcher<HasVlanMatcher> HasVlan() {
    return ::testing::MakePolymorphicMatcher(HasVlanMatcher());
}

/// @brief Matcher: packet has specific VLAN ID
class HasVlanIdMatcher {
public:
    explicit HasVlanIdMatcher(std::uint16_t vlan_id) : expected_(vlan_id) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);

        // For VLAN, we need to check raw bytes (assuming standard 802.1Q)
        if (data.size() < 16) {  // At least Ethernet header + VLAN tag
            if (listener->IsInterested()) {
                *listener << "packet too short for VLAN";
            }
            return false;
        }

        // Check if VLAN tag is present (ethertype at offset 12-13)
        auto ethertype =
            (static_cast<uint16_t>(static_cast<uint8_t>(data[12])) << 8) |
            static_cast<uint16_t>(static_cast<uint8_t>(data[13]));

        if (ethertype != 0x8100 && ethertype != 0x88A8) {
            if (listener->IsInterested()) {
                *listener << "packet does not have VLAN tag";
            }
            return false;
        }

        // VLAN ID is in bytes 14-15, lower 12 bits
        auto tci =
            (static_cast<uint16_t>(static_cast<uint8_t>(data[14])) << 8) |
            static_cast<uint16_t>(static_cast<uint8_t>(data[15]));
        auto vlan_id = tci & 0x0FFF;

        if (listener->IsInterested()) {
            *listener << "has VLAN ID " << vlan_id;
        }
        return vlan_id == expected_;
    }

    void DescribeTo(std::ostream* os) const { *os << "has VLAN ID " << expected_; }
    void DescribeNegationTo(std::ostream* os) const { *os << "does not have VLAN ID " << expected_; }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasVlanIdMatcher> HasVlanId(std::uint16_t vlan_id) {
    return ::testing::MakePolymorphicMatcher(HasVlanIdMatcher(vlan_id));
}

// =============================================================================
// IPv4 Matchers
// =============================================================================

/// @brief Matcher: packet has specific source IP
class HasSourceIPMatcher {
public:
    explicit HasSourceIPMatcher(const IPv4Address& ip) : expected_(ip) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ipv4::IPv4Header>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have IPv4 header";
            }
            return false;
        }

        const auto* ip = result.get_layer<protocols::ipv4::IPv4Header>();
        if (listener->IsInterested()) {
            *listener << "has source IP " << ip->src_ip.to_string();
        }
        return ip->src_ip == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has source IP " << expected_.to_string();
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have source IP " << expected_.to_string();
    }

private:
    IPv4Address expected_;
};

inline ::testing::PolymorphicMatcher<HasSourceIPMatcher> HasSourceIP(const IPv4Address& ip) {
    return ::testing::MakePolymorphicMatcher(HasSourceIPMatcher(ip));
}

inline ::testing::PolymorphicMatcher<HasSourceIPMatcher> HasSourceIP(const std::string& ip) {
    return ::testing::MakePolymorphicMatcher(HasSourceIPMatcher(IPv4Address::from_string(ip)));
}

inline ::testing::PolymorphicMatcher<HasSourceIPMatcher> HasSourceIP(std::uint32_t ip) {
    return ::testing::MakePolymorphicMatcher(HasSourceIPMatcher(IPv4Address::from_uint32(ip)));
}

/// @brief Matcher: packet has specific destination IP
class HasDestIPMatcher {
public:
    explicit HasDestIPMatcher(const IPv4Address& ip) : expected_(ip) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ipv4::IPv4Header>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have IPv4 header";
            }
            return false;
        }

        const auto* ip = result.get_layer<protocols::ipv4::IPv4Header>();
        if (listener->IsInterested()) {
            *listener << "has destination IP " << ip->dst_ip.to_string();
        }
        return ip->dst_ip == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has destination IP " << expected_.to_string();
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have destination IP " << expected_.to_string();
    }

private:
    IPv4Address expected_;
};

inline ::testing::PolymorphicMatcher<HasDestIPMatcher> HasDestIP(const IPv4Address& ip) {
    return ::testing::MakePolymorphicMatcher(HasDestIPMatcher(ip));
}

inline ::testing::PolymorphicMatcher<HasDestIPMatcher> HasDestIP(const std::string& ip) {
    return ::testing::MakePolymorphicMatcher(HasDestIPMatcher(IPv4Address::from_string(ip)));
}

inline ::testing::PolymorphicMatcher<HasDestIPMatcher> HasDestIP(std::uint32_t ip) {
    return ::testing::MakePolymorphicMatcher(HasDestIPMatcher(IPv4Address::from_uint32(ip)));
}

/// @brief Matcher: packet has specific IP protocol
class HasIPProtocolMatcher {
public:
    explicit HasIPProtocolMatcher(std::uint8_t protocol) : expected_(protocol) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::ipv4::IPv4Header>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have IPv4 header";
            }
            return false;
        }

        const auto* ip = result.get_layer<protocols::ipv4::IPv4Header>();
        if (listener->IsInterested()) {
            *listener << "has IP protocol " << static_cast<int>(ip->protocol);
        }
        return ip->protocol == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has IP protocol " << static_cast<int>(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have IP protocol " << static_cast<int>(expected_);
    }

private:
    std::uint8_t expected_;
};

inline ::testing::PolymorphicMatcher<HasIPProtocolMatcher> HasIPProtocol(std::uint8_t protocol) {
    return ::testing::MakePolymorphicMatcher(HasIPProtocolMatcher(protocol));
}

/// @brief Matcher: packet uses UDP
inline ::testing::PolymorphicMatcher<HasIPProtocolMatcher> IsUDP() {
    return HasIPProtocol(17);  // IPPROTO_UDP
}

/// @brief Matcher: packet uses TCP
inline ::testing::PolymorphicMatcher<HasIPProtocolMatcher> IsTCP() {
    return HasIPProtocol(6);  // IPPROTO_TCP
}

// =============================================================================
// Port Matchers
// =============================================================================

/// @brief Matcher: packet has specific source port (UDP or TCP)
class HasSourcePortMatcher {
public:
    explicit HasSourcePortMatcher(std::uint16_t port) : expected_(port) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (result.has_layer<protocols::udp::UdpHeader>()) {
            const auto* udp = result.get_layer<protocols::udp::UdpHeader>();
            if (listener->IsInterested()) {
                *listener << "has UDP source port " << udp->src_port;
            }
            return udp->src_port == expected_;
        }

        if (result.has_layer<protocols::tcp::TcpHeader>()) {
            const auto* tcp = result.get_layer<protocols::tcp::TcpHeader>();
            if (listener->IsInterested()) {
                *listener << "has TCP source port " << tcp->src_port;
            }
            return tcp->src_port == expected_;
        }

        if (listener->IsInterested()) {
            *listener << "packet does not have UDP or TCP header";
        }
        return false;
    }

    void DescribeTo(std::ostream* os) const { *os << "has source port " << expected_; }
    void DescribeNegationTo(std::ostream* os) const { *os << "does not have source port " << expected_; }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasSourcePortMatcher> HasSourcePort(std::uint16_t port) {
    return ::testing::MakePolymorphicMatcher(HasSourcePortMatcher(port));
}

/// @brief Matcher: packet has specific destination port (UDP or TCP)
class HasDestPortMatcher {
public:
    explicit HasDestPortMatcher(std::uint16_t port) : expected_(port) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (result.has_layer<protocols::udp::UdpHeader>()) {
            const auto* udp = result.get_layer<protocols::udp::UdpHeader>();
            if (listener->IsInterested()) {
                *listener << "has UDP destination port " << udp->dst_port;
            }
            return udp->dst_port == expected_;
        }

        if (result.has_layer<protocols::tcp::TcpHeader>()) {
            const auto* tcp = result.get_layer<protocols::tcp::TcpHeader>();
            if (listener->IsInterested()) {
                *listener << "has TCP destination port " << tcp->dst_port;
            }
            return tcp->dst_port == expected_;
        }

        if (listener->IsInterested()) {
            *listener << "packet does not have UDP or TCP header";
        }
        return false;
    }

    void DescribeTo(std::ostream* os) const { *os << "has destination port " << expected_; }
    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have destination port " << expected_;
    }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasDestPortMatcher> HasDestPort(std::uint16_t port) {
    return ::testing::MakePolymorphicMatcher(HasDestPortMatcher(port));
}

// =============================================================================
// SOME/IP Matchers
// =============================================================================

/// @brief Matcher: packet has specific SOME/IP service ID
class HasSOMEIPServiceIdMatcher {
public:
    explicit HasSOMEIPServiceIdMatcher(std::uint16_t service_id) : expected_(service_id) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::someip::SomeIpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have SOME/IP header";
            }
            return false;
        }

        const auto* someip = result.get_layer<protocols::someip::SomeIpHeader>();
        if (listener->IsInterested()) {
            *listener << "has SOME/IP service ID 0x" << std::hex << someip->service_id;
        }
        return someip->service_id == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has SOME/IP service ID 0x" << std::hex << expected_;
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have SOME/IP service ID 0x" << std::hex << expected_;
    }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasSOMEIPServiceIdMatcher> HasSOMEIPServiceId(
    std::uint16_t service_id) {
    return ::testing::MakePolymorphicMatcher(HasSOMEIPServiceIdMatcher(service_id));
}

/// @brief Matcher: packet has specific SOME/IP method ID
class HasSOMEIPMethodIdMatcher {
public:
    explicit HasSOMEIPMethodIdMatcher(std::uint16_t method_id) : expected_(method_id) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::someip::SomeIpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have SOME/IP header";
            }
            return false;
        }

        const auto* someip = result.get_layer<protocols::someip::SomeIpHeader>();
        if (listener->IsInterested()) {
            *listener << "has SOME/IP method ID 0x" << std::hex << someip->method_id;
        }
        return someip->method_id == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has SOME/IP method ID 0x" << std::hex << expected_;
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have SOME/IP method ID 0x" << std::hex << expected_;
    }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasSOMEIPMethodIdMatcher> HasSOMEIPMethodId(
    std::uint16_t method_id) {
    return ::testing::MakePolymorphicMatcher(HasSOMEIPMethodIdMatcher(method_id));
}

/// @brief Matcher: packet has specific SOME/IP message type
class HasSOMEIPMessageTypeMatcher {
public:
    explicit HasSOMEIPMessageTypeMatcher(protocols::someip::MessageType type) : expected_(type) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::someip::SomeIpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have SOME/IP header";
            }
            return false;
        }

        const auto* someip = result.get_layer<protocols::someip::SomeIpHeader>();
        if (listener->IsInterested()) {
            *listener << "has SOME/IP message type 0x" << std::hex
                      << static_cast<int>(someip->message_type);
        }
        return someip->message_type == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has SOME/IP message type 0x" << std::hex << static_cast<int>(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have SOME/IP message type 0x" << std::hex << static_cast<int>(expected_);
    }

private:
    protocols::someip::MessageType expected_;
};

inline ::testing::PolymorphicMatcher<HasSOMEIPMessageTypeMatcher> HasSOMEIPMessageType(
    protocols::someip::MessageType type) {
    return ::testing::MakePolymorphicMatcher(HasSOMEIPMessageTypeMatcher(type));
}

/// @brief Matcher: packet is a SOME/IP request
inline ::testing::PolymorphicMatcher<HasSOMEIPMessageTypeMatcher> IsSOMEIPRequest() {
    return HasSOMEIPMessageType(protocols::someip::MessageType::Request);
}

/// @brief Matcher: packet is a SOME/IP response
inline ::testing::PolymorphicMatcher<HasSOMEIPMessageTypeMatcher> IsSOMEIPResponse() {
    return HasSOMEIPMessageType(protocols::someip::MessageType::Response);
}

/// @brief Matcher: packet is a SOME/IP notification/event
inline ::testing::PolymorphicMatcher<HasSOMEIPMessageTypeMatcher> IsSOMEIPNotification() {
    return HasSOMEIPMessageType(protocols::someip::MessageType::Notification);
}

/// @brief Matcher: packet is a SOME/IP request without return
inline ::testing::PolymorphicMatcher<HasSOMEIPMessageTypeMatcher> IsSOMEIPRequestNoReturn() {
    return HasSOMEIPMessageType(protocols::someip::MessageType::RequestNoReturn);
}

// =============================================================================
// DoIP Matchers
// =============================================================================

/// @brief Matcher: packet has specific DoIP payload type
class HasDoIPPayloadTypeMatcher {
public:
    explicit HasDoIPPayloadTypeMatcher(protocols::doip::PayloadType payload_type) : expected_(payload_type) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::doip::DoIPHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have DoIP header";
            }
            return false;
        }

        const auto* doip = result.get_layer<protocols::doip::DoIPHeader>();
        if (listener->IsInterested()) {
            *listener << "has DoIP payload type 0x" << std::hex 
                      << static_cast<uint16_t>(doip->payload_type);
        }
        return doip->payload_type == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has DoIP payload type 0x" << std::hex << static_cast<uint16_t>(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have DoIP payload type 0x" << std::hex << static_cast<uint16_t>(expected_);
    }

private:
    protocols::doip::PayloadType expected_;
};

inline ::testing::PolymorphicMatcher<HasDoIPPayloadTypeMatcher> HasDoIPPayloadType(
    protocols::doip::PayloadType payload_type) {
    return ::testing::MakePolymorphicMatcher(HasDoIPPayloadTypeMatcher(payload_type));
}

/// @brief Matcher: packet is a DoIP diagnostic message
inline ::testing::PolymorphicMatcher<HasDoIPPayloadTypeMatcher> IsDoIPDiagnosticMessage() {
    return HasDoIPPayloadType(protocols::doip::PayloadType::DiagnosticMessage);
}

/// @brief Matcher: packet is a DoIP routing activation request
inline ::testing::PolymorphicMatcher<HasDoIPPayloadTypeMatcher> IsDoIPRoutingActivationRequest() {
    return HasDoIPPayloadType(protocols::doip::PayloadType::RoutingActivationRequest);
}

/// @brief Matcher: packet is a DoIP routing activation response
inline ::testing::PolymorphicMatcher<HasDoIPPayloadTypeMatcher> IsDoIPRoutingActivationResponse() {
    return HasDoIPPayloadType(protocols::doip::PayloadType::RoutingActivationResponse);
}

/// @brief Matcher: packet is a DoIP vehicle identification request
inline ::testing::PolymorphicMatcher<HasDoIPPayloadTypeMatcher> IsDoIPVehicleIdentificationRequest() {
    return HasDoIPPayloadType(protocols::doip::PayloadType::VehicleIdentificationRequest);
}

/// @brief Matcher: packet is a DoIP vehicle identification response
inline ::testing::PolymorphicMatcher<HasDoIPPayloadTypeMatcher> IsDoIPVehicleIdentificationResponse() {
    return HasDoIPPayloadType(protocols::doip::PayloadType::VehicleAnnouncementOrIdentificationResponse);
}

// =============================================================================
// gPTP (IEEE 802.1AS) Matchers
// =============================================================================

/// @brief Matcher: packet is a gPTP packet
class IsGptpMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        bool has_gptp = result.has_layer<protocols::gptp::GptpHeader>();
        if (listener->IsInterested()) {
            *listener << (has_gptp ? "is a gPTP packet" : "is not a gPTP packet");
        }
        return has_gptp;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a gPTP packet"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a gPTP packet"; }
};

inline ::testing::PolymorphicMatcher<IsGptpMatcher> IsGptp() {
    return ::testing::MakePolymorphicMatcher(IsGptpMatcher());
}

/// @brief Matcher: packet has specific gPTP message type
class HasGptpMessageTypeMatcher {
public:
    explicit HasGptpMessageTypeMatcher(protocols::gptp::MessageType msg_type)
        : expected_(msg_type) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        if (listener->IsInterested()) {
            *listener << "has gPTP message type "
                      << protocols::gptp::message_type_string(gptp->message_type);
        }
        return gptp->message_type == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has gPTP message type " << protocols::gptp::message_type_string(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have gPTP message type "
            << protocols::gptp::message_type_string(expected_);
    }

private:
    protocols::gptp::MessageType expected_;
};

inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> HasGptpMessageType(
    protocols::gptp::MessageType msg_type) {
    return ::testing::MakePolymorphicMatcher(HasGptpMessageTypeMatcher(msg_type));
}

/// @brief Matcher: packet is a gPTP Sync message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpSync() {
    return HasGptpMessageType(protocols::gptp::MessageType::Sync);
}

/// @brief Matcher: packet is a gPTP Follow_Up message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpFollowUp() {
    return HasGptpMessageType(protocols::gptp::MessageType::Follow_Up);
}

/// @brief Matcher: packet is a gPTP Pdelay_Req message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpPdelayReq() {
    return HasGptpMessageType(protocols::gptp::MessageType::Pdelay_Req);
}

/// @brief Matcher: packet is a gPTP Pdelay_Resp message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpPdelayResp() {
    return HasGptpMessageType(protocols::gptp::MessageType::Pdelay_Resp);
}

/// @brief Matcher: packet is a gPTP Pdelay_Resp_Follow_Up message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpPdelayRespFollowUp() {
    return HasGptpMessageType(protocols::gptp::MessageType::Pdelay_Resp_Follow_Up);
}

/// @brief Matcher: packet is a gPTP Announce message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpAnnounce() {
    return HasGptpMessageType(protocols::gptp::MessageType::Announce);
}

/// @brief Matcher: packet is a gPTP Signaling message
inline ::testing::PolymorphicMatcher<HasGptpMessageTypeMatcher> IsGptpSignaling() {
    return HasGptpMessageType(protocols::gptp::MessageType::Signaling);
}

/// @brief Matcher: packet has specific gPTP domain number
class HasGptpDomainMatcher {
public:
    explicit HasGptpDomainMatcher(std::uint8_t domain) : expected_(domain) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        if (listener->IsInterested()) {
            *listener << "has gPTP domain " << static_cast<int>(gptp->domain_number);
        }
        return gptp->domain_number == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has gPTP domain " << static_cast<int>(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have gPTP domain " << static_cast<int>(expected_);
    }

private:
    std::uint8_t expected_;
};

inline ::testing::PolymorphicMatcher<HasGptpDomainMatcher> HasGptpDomain(std::uint8_t domain) {
    return ::testing::MakePolymorphicMatcher(HasGptpDomainMatcher(domain));
}

/// @brief Matcher: packet has specific gPTP sequence ID
class HasGptpSequenceIdMatcher {
public:
    explicit HasGptpSequenceIdMatcher(std::uint16_t seq_id) : expected_(seq_id) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        if (listener->IsInterested()) {
            *listener << "has gPTP sequence ID " << gptp->sequence_id;
        }
        return gptp->sequence_id == expected_;
    }

    void DescribeTo(std::ostream* os) const { *os << "has gPTP sequence ID " << expected_; }
    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have gPTP sequence ID " << expected_;
    }

private:
    std::uint16_t expected_;
};

inline ::testing::PolymorphicMatcher<HasGptpSequenceIdMatcher> HasGptpSequenceId(
    std::uint16_t seq_id) {
    return ::testing::MakePolymorphicMatcher(HasGptpSequenceIdMatcher(seq_id));
}

/// @brief Matcher: packet is from a specific gPTP port identity
class GptpFromPortMatcher {
public:
    explicit GptpFromPortMatcher(const protocols::gptp::PortIdentity& port) : expected_(port) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        const auto& src = gptp->source_port_identity;

        bool matches = (src.clock_identity == expected_.clock_identity &&
                        src.port_number == expected_.port_number);

        if (listener->IsInterested()) {
            *listener << "from port " << src.clock_identity.to_string() << ":" << src.port_number;
        }
        return matches;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "is from gPTP port " << expected_.clock_identity.to_string() << ":"
            << expected_.port_number;
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "is not from gPTP port " << expected_.clock_identity.to_string() << ":"
            << expected_.port_number;
    }

private:
    protocols::gptp::PortIdentity expected_;
};

inline ::testing::PolymorphicMatcher<GptpFromPortMatcher> GptpFromPort(
    const protocols::gptp::PortIdentity& port) {
    return ::testing::MakePolymorphicMatcher(GptpFromPortMatcher(port));
}

/// @brief Matcher: packet is from a specific gPTP clock identity
class GptpFromClockMatcher {
public:
    explicit GptpFromClockMatcher(const protocols::gptp::ClockIdentity& clock) : expected_(clock) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        bool matches = (gptp->source_port_identity.clock_identity == expected_);

        if (listener->IsInterested()) {
            *listener << "from clock " << gptp->source_port_identity.clock_identity.to_string();
        }
        return matches;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "is from gPTP clock " << expected_.to_string();
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "is not from gPTP clock " << expected_.to_string();
    }

private:
    protocols::gptp::ClockIdentity expected_;
};

inline ::testing::PolymorphicMatcher<GptpFromClockMatcher> GptpFromClock(
    const protocols::gptp::ClockIdentity& clock) {
    return ::testing::MakePolymorphicMatcher(GptpFromClockMatcher(clock));
}

/// @brief Matcher: packet is a gPTP event message (requires timestamping)
class IsGptpEventMessageMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        bool is_event = gptp->is_event();
        if (listener->IsInterested()) {
            *listener << (is_event ? "is an event message" : "is not an event message");
        }
        return is_event;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a gPTP event message"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a gPTP event message"; }
};

inline ::testing::PolymorphicMatcher<IsGptpEventMessageMatcher> IsGptpEventMessage() {
    return ::testing::MakePolymorphicMatcher(IsGptpEventMessageMatcher());
}

/// @brief Matcher: packet is a gPTP two-step message (Follow_Up expected)
class IsGptpTwoStepMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::gptp::GptpHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have gPTP header";
            }
            return false;
        }

        const auto* gptp = result.get_layer<protocols::gptp::GptpHeader>();
        bool is_two_step = gptp->is_two_step();
        if (listener->IsInterested()) {
            *listener << (is_two_step ? "is two-step" : "is not two-step");
        }
        return is_two_step;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a gPTP two-step message"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a gPTP two-step message"; }
};

inline ::testing::PolymorphicMatcher<IsGptpTwoStepMatcher> IsGptpTwoStep() {
    return ::testing::MakePolymorphicMatcher(IsGptpTwoStepMatcher());
}

// =============================================================================
// Payload Matchers
// =============================================================================

/// @brief Matcher: packet payload contains specific bytes
class PayloadContainsMatcher {
public:
    explicit PayloadContainsMatcher(std::vector<std::uint8_t> pattern) : pattern_(std::move(pattern)) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);

        // Search for pattern in raw data
        auto it = std::search(
            reinterpret_cast<const uint8_t*>(data.data()),
            reinterpret_cast<const uint8_t*>(data.data()) + data.size(),
            pattern_.begin(), pattern_.end());

        bool found = it != (reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        if (listener->IsInterested()) {
            *listener << (found ? "payload contains pattern" : "payload does not contain pattern");
        }
        return found;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "payload contains pattern of " << pattern_.size() << " bytes";
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "payload does not contain pattern of " << pattern_.size() << " bytes";
    }

private:
    std::vector<std::uint8_t> pattern_;
};

inline ::testing::PolymorphicMatcher<PayloadContainsMatcher> PayloadContains(
    std::initializer_list<std::uint8_t> pattern) {
    return ::testing::MakePolymorphicMatcher(
        PayloadContainsMatcher(std::vector<std::uint8_t>(pattern)));
}

inline ::testing::PolymorphicMatcher<PayloadContainsMatcher> PayloadContains(
    const std::vector<std::uint8_t>& pattern) {
    return ::testing::MakePolymorphicMatcher(PayloadContainsMatcher(pattern));
}

/// @brief Matcher: packet has specific payload size
class PayloadSizeMatcher {
public:
    explicit PayloadSizeMatcher(std::size_t size) : expected_(size) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);

        if (listener->IsInterested()) {
            *listener << "has payload size " << data.size();
        }
        return data.size() == expected_;
    }

    void DescribeTo(std::ostream* os) const { *os << "has payload size " << expected_; }
    void DescribeNegationTo(std::ostream* os) const { *os << "does not have payload size " << expected_; }

private:
    std::size_t expected_;
};

inline ::testing::PolymorphicMatcher<PayloadSizeMatcher> HasPayloadSize(std::size_t size) {
    return ::testing::MakePolymorphicMatcher(PayloadSizeMatcher(size));
}

// =============================================================================
// Combined/Meta Matchers
// =============================================================================

/// @brief Matcher: packet decodes successfully without errors
class DecodesSuccessfullyMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (result.error.has_value()) {
            if (listener->IsInterested()) {
                *listener << "decode error: " << result.error->message;
            }
            return false;
        }

        if (listener->IsInterested()) {
            *listener << "decoded " << result.layers.size() << " layers successfully";
        }
        return true;
    }

    void DescribeTo(std::ostream* os) const { *os << "decodes successfully"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "fails to decode"; }
};

inline ::testing::PolymorphicMatcher<DecodesSuccessfullyMatcher> DecodesSuccessfully() {
    return ::testing::MakePolymorphicMatcher(DecodesSuccessfullyMatcher());
}

// =============================================================================
// UDS (ISO 14229) Matchers
// =============================================================================

/// @brief Matcher: payload is a UDS request message
class IsUdsRequestMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);

        bool is_request = protocols::uds::UdsDecoder::is_request(span);
        if (listener->IsInterested()) {
            *listener << (is_request ? "is a UDS request" : "is not a UDS request");
        }
        return is_request;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a UDS request"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a UDS request"; }
};

inline ::testing::PolymorphicMatcher<IsUdsRequestMatcher> IsUdsRequest() {
    return ::testing::MakePolymorphicMatcher(IsUdsRequestMatcher());
}

/// @brief Matcher: payload is a UDS positive response message
class IsUdsPositiveResponseMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);

        bool is_response = protocols::uds::UdsDecoder::is_positive_response(span);
        if (listener->IsInterested()) {
            *listener << (is_response ? "is a UDS positive response"
                                      : "is not a UDS positive response");
        }
        return is_response;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a UDS positive response"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a UDS positive response"; }
};

inline ::testing::PolymorphicMatcher<IsUdsPositiveResponseMatcher> IsUdsPositiveResponse() {
    return ::testing::MakePolymorphicMatcher(IsUdsPositiveResponseMatcher());
}

/// @brief Matcher: payload is a UDS response (positive or negative)
class IsUdsResponseMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);

        bool is_response = protocols::uds::UdsDecoder::is_positive_response(span) ||
                           protocols::uds::UdsDecoder::is_negative_response(span);
        if (listener->IsInterested()) {
            *listener << (is_response ? "is a UDS response" : "is not a UDS response");
        }
        return is_response;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a UDS response"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a UDS response"; }
};

inline ::testing::PolymorphicMatcher<IsUdsResponseMatcher> IsUdsResponse() {
    return ::testing::MakePolymorphicMatcher(IsUdsResponseMatcher());
}

/// @brief Matcher: payload is a UDS negative response message
class IsUdsNegativeResponseMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);

        bool is_nrc = protocols::uds::UdsDecoder::is_negative_response(span);
        if (listener->IsInterested()) {
            *listener << (is_nrc ? "is a UDS negative response" : "is not a UDS negative response");
        }
        return is_nrc;
    }

    void DescribeTo(std::ostream* os) const { *os << "is a UDS negative response"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not a UDS negative response"; }
};

inline ::testing::PolymorphicMatcher<IsUdsNegativeResponseMatcher> IsUdsNegativeResponse() {
    return ::testing::MakePolymorphicMatcher(IsUdsNegativeResponseMatcher());
}

/// @brief Matcher: payload has specific UDS service ID
class HasUdsServiceMatcher {
public:
    explicit HasUdsServiceMatcher(protocols::uds::ServiceID service_id) : expected_(service_id) {}

    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);
        protocols::uds::UdsDecoder decoder;
        auto result = decoder.decode(span);

        if (!result.is_ok()) {
            if (listener->IsInterested()) {
                *listener << "failed to decode UDS message";
            }
            return false;
        }

        if (listener->IsInterested()) {
            *listener << "has UDS service "
                      << protocols::uds::service_id_string(result->header.service_id);
        }
        return result->header.service_id == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has UDS service " << protocols::uds::service_id_string(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have UDS service " << protocols::uds::service_id_string(expected_);
    }

private:
    protocols::uds::ServiceID expected_;
};

inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> HasUdsService(
    protocols::uds::ServiceID service_id) {
    return ::testing::MakePolymorphicMatcher(HasUdsServiceMatcher(service_id));
}

/// @brief Convenience: HasUdsService for DiagnosticSessionControl
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsDiagnosticSessionControl() {
    return HasUdsService(protocols::uds::ServiceID::DiagnosticSessionControl);
}

/// @brief Convenience: HasUdsService for ECUReset
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsECUReset() {
    return HasUdsService(protocols::uds::ServiceID::ECUReset);
}

/// @brief Convenience: HasUdsService for SecurityAccess
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsSecurityAccess() {
    return HasUdsService(protocols::uds::ServiceID::SecurityAccess);
}

/// @brief Convenience: HasUdsService for TesterPresent
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsTesterPresent() {
    return HasUdsService(protocols::uds::ServiceID::TesterPresent);
}

/// @brief Convenience: HasUdsService for ReadDataByIdentifier
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsReadDataByIdentifier() {
    return HasUdsService(protocols::uds::ServiceID::ReadDataByIdentifier);
}

/// @brief Convenience: HasUdsService for WriteDataByIdentifier
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsWriteDataByIdentifier() {
    return HasUdsService(protocols::uds::ServiceID::WriteDataByIdentifier);
}

/// @brief Convenience: HasUdsService for RoutineControl
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsRoutineControl() {
    return HasUdsService(protocols::uds::ServiceID::RoutineControl);
}

/// @brief Convenience: HasUdsService for RequestDownload
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsRequestDownload() {
    return HasUdsService(protocols::uds::ServiceID::RequestDownload);
}

/// @brief Convenience: HasUdsService for TransferData
inline ::testing::PolymorphicMatcher<HasUdsServiceMatcher> IsUdsTransferData() {
    return HasUdsService(protocols::uds::ServiceID::TransferData);
}

/// @brief Matcher: UDS message contains specific DID
class HasUdsDIDMatcher {
public:
    explicit HasUdsDIDMatcher(protocols::uds::DataIdentifier did) : expected_(did) {}
    explicit HasUdsDIDMatcher(std::uint16_t did_value) : expected_(did_value) {}

    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);
        protocols::uds::UdsDecoder decoder;
        auto result = decoder.decode(span);

        if (!result.is_ok()) {
            if (listener->IsInterested()) {
                *listener << "failed to decode UDS message";
            }
            return false;
        }

        // Check ReadDataByIdentifier request
        if (auto* req = result->template as<protocols::uds::ReadDataByIdentifierRequest>()) {
            for (const auto& did : req->data_identifiers) {
                if (did == expected_) {
                    if (listener->IsInterested()) {
                        *listener << "contains DID 0x" << std::hex << expected_.value;
                    }
                    return true;
                }
            }
        }

        // Check ReadDataByIdentifier response
        if (auto* resp = result->template as<protocols::uds::ReadDataByIdentifierResponse>()) {
            for (const auto& record : resp->records) {
                if (record.did == expected_) {
                    if (listener->IsInterested()) {
                        *listener << "contains DID 0x" << std::hex << expected_.value;
                    }
                    return true;
                }
            }
        }

        // Check WriteDataByIdentifier request
        if (auto* req = result->template as<protocols::uds::WriteDataByIdentifierRequest>()) {
            if (req->data_identifier == expected_) {
                if (listener->IsInterested()) {
                    *listener << "contains DID 0x" << std::hex << expected_.value;
                }
                return true;
            }
        }

        // Check WriteDataByIdentifier response
        if (auto* resp = result->template as<protocols::uds::WriteDataByIdentifierResponse>()) {
            if (resp->data_identifier == expected_) {
                if (listener->IsInterested()) {
                    *listener << "contains DID 0x" << std::hex << expected_.value;
                }
                return true;
            }
        }

        if (listener->IsInterested()) {
            *listener << "does not contain DID 0x" << std::hex << expected_.value;
        }
        return false;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "contains UDS DID 0x" << std::hex << expected_.value;
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not contain UDS DID 0x" << std::hex << expected_.value;
    }

private:
    protocols::uds::DataIdentifier expected_;
};

inline ::testing::PolymorphicMatcher<HasUdsDIDMatcher> HasUdsDID(
    protocols::uds::DataIdentifier did) {
    return ::testing::MakePolymorphicMatcher(HasUdsDIDMatcher(did));
}

inline ::testing::PolymorphicMatcher<HasUdsDIDMatcher> HasUdsDID(std::uint16_t did_value) {
    return ::testing::MakePolymorphicMatcher(HasUdsDIDMatcher(did_value));
}

/// @brief Convenience: Check for VIN DID (0xF190)
inline ::testing::PolymorphicMatcher<HasUdsDIDMatcher> HasUdsVinDID() {
    return HasUdsDID(protocols::uds::DID::VIN);
}

/// @brief Matcher: UDS negative response has specific NRC
class HasUdsNRCMatcher {
public:
    explicit HasUdsNRCMatcher(protocols::uds::NRC nrc) : expected_(nrc) {}
    explicit HasUdsNRCMatcher(std::uint8_t nrc_value)
        : expected_(static_cast<protocols::uds::NRC>(nrc_value)) {}

    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);
        protocols::uds::UdsDecoder decoder;
        auto result = decoder.decode(span);

        if (!result.is_ok()) {
            if (listener->IsInterested()) {
                *listener << "failed to decode UDS message";
            }
            return false;
        }

        if (!result->header.is_negative_response()) {
            if (listener->IsInterested()) {
                *listener << "message is not a negative response";
            }
            return false;
        }

        auto nrc = static_cast<protocols::uds::NRC>(*result->header.negative_response_code);
        if (listener->IsInterested()) {
            *listener << "has NRC " << protocols::uds::nrc_string(nrc);
        }
        return nrc == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has UDS NRC " << protocols::uds::nrc_string(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have UDS NRC " << protocols::uds::nrc_string(expected_);
    }

private:
    protocols::uds::NRC expected_;
};

inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsNRC(protocols::uds::NRC nrc) {
    return ::testing::MakePolymorphicMatcher(HasUdsNRCMatcher(nrc));
}

inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsNRC(std::uint8_t nrc_value) {
    return ::testing::MakePolymorphicMatcher(HasUdsNRCMatcher(nrc_value));
}

/// @brief Convenience: Check for ServiceNotSupported NRC
inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsServiceNotSupported() {
    return HasUdsNRC(protocols::uds::NRC::ServiceNotSupported);
}

/// @brief Convenience: Check for SecurityAccessDenied NRC
inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsSecurityAccessDenied() {
    return HasUdsNRC(protocols::uds::NRC::SecurityAccessDenied);
}

/// @brief Convenience: Check for RequestOutOfRange NRC
inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsRequestOutOfRange() {
    return HasUdsNRC(protocols::uds::NRC::RequestOutOfRange);
}

/// @brief Convenience: Check for ConditionsNotCorrect NRC
inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsConditionsNotCorrect() {
    return HasUdsNRC(protocols::uds::NRC::ConditionsNotCorrect);
}

/// @brief Convenience: Check for ResponsePending NRC
inline ::testing::PolymorphicMatcher<HasUdsNRCMatcher> HasUdsResponsePending() {
    return HasUdsNRC(protocols::uds::NRC::RequestCorrectlyReceivedResponsePending);
}

/// @brief Matcher: UDS message has specific session type
class HasUdsSessionTypeMatcher {
public:
    explicit HasUdsSessionTypeMatcher(protocols::uds::SessionType session_type)
        : expected_(session_type) {}

    template <typename T>
    bool MatchAndExplain(const T& data, ::testing::MatchResultListener* listener) const {
        auto span = detail::get_packet_data(data);
        protocols::uds::UdsDecoder decoder;
        auto result = decoder.decode(span);

        if (!result.is_ok()) {
            if (listener->IsInterested()) {
                *listener << "failed to decode UDS message";
            }
            return false;
        }

        // Check request
        if (auto* req = result->template as<protocols::uds::DiagnosticSessionControlRequest>()) {
            if (listener->IsInterested()) {
                *listener << "has session type "
                          << protocols::uds::session_type_string(req->session_type);
            }
            return req->session_type == expected_;
        }

        // Check response
        if (auto* resp = result->template as<protocols::uds::DiagnosticSessionControlResponse>()) {
            if (listener->IsInterested()) {
                *listener << "has session type "
                          << protocols::uds::session_type_string(resp->session_type);
            }
            return resp->session_type == expected_;
        }

        if (listener->IsInterested()) {
            *listener << "message is not a DiagnosticSessionControl";
        }
        return false;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has UDS session type " << protocols::uds::session_type_string(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have UDS session type " << protocols::uds::session_type_string(expected_);
    }

private:
    protocols::uds::SessionType expected_;
};

inline ::testing::PolymorphicMatcher<HasUdsSessionTypeMatcher> HasUdsSessionType(
    protocols::uds::SessionType session_type) {
    return ::testing::MakePolymorphicMatcher(HasUdsSessionTypeMatcher(session_type));
}

/// @brief Convenience: Check for default session
inline ::testing::PolymorphicMatcher<HasUdsSessionTypeMatcher> IsUdsDefaultSession() {
    return HasUdsSessionType(protocols::uds::SessionType::DefaultSession);
}

/// @brief Convenience: Check for programming session
inline ::testing::PolymorphicMatcher<HasUdsSessionTypeMatcher> IsUdsProgrammingSession() {
    return HasUdsSessionType(protocols::uds::SessionType::ProgrammingSession);
}

/// @brief Convenience: Check for extended diagnostic session
inline ::testing::PolymorphicMatcher<HasUdsSessionTypeMatcher> IsUdsExtendedSession() {
    return HasUdsSessionType(protocols::uds::SessionType::ExtendedDiagnosticSession);
}

// =============================================================================
// DDS/RTPS Matchers
// =============================================================================

/// @brief Matcher: packet is an RTPS message
class IsRtpsMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        bool has_rtps = result.has_layer<protocols::dds::RtpsHeader>();
        if (listener->IsInterested()) {
            *listener << (has_rtps ? "is an RTPS message" : "is not an RTPS message");
        }
        return has_rtps;
    }

    void DescribeTo(std::ostream* os) const { *os << "is an RTPS message"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not an RTPS message"; }
};

inline ::testing::PolymorphicMatcher<IsRtpsMatcher> IsRtps() {
    return ::testing::MakePolymorphicMatcher(IsRtpsMatcher());
}

/// @brief Alias for IsRtps
inline ::testing::PolymorphicMatcher<IsRtpsMatcher> IsDds() {
    return ::testing::MakePolymorphicMatcher(IsRtpsMatcher());
}

/// @brief Matcher: RTPS message has specific protocol version
class HasRtpsVersionMatcher {
public:
    HasRtpsVersionMatcher(std::uint8_t major, std::uint8_t minor) : major_(major), minor_(minor) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::dds::RtpsHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have RTPS header";
            }
            return false;
        }

        const auto* rtps = result.get_layer<protocols::dds::RtpsHeader>();
        if (listener->IsInterested()) {
            *listener << "has RTPS version " << static_cast<int>(rtps->version.major) << "."
                      << static_cast<int>(rtps->version.minor);
        }
        return rtps->version.major == major_ && rtps->version.minor == minor_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has RTPS version " << static_cast<int>(major_) << "." << static_cast<int>(minor_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have RTPS version " << static_cast<int>(major_) << "."
            << static_cast<int>(minor_);
    }

private:
    std::uint8_t major_;
    std::uint8_t minor_;
};

inline ::testing::PolymorphicMatcher<HasRtpsVersionMatcher> HasRtpsVersion(std::uint8_t major,
                                                                           std::uint8_t minor) {
    return ::testing::MakePolymorphicMatcher(HasRtpsVersionMatcher(major, minor));
}

/// @brief Matcher: RTPS message has specific vendor ID
class HasRtpsVendorMatcher {
public:
    explicit HasRtpsVendorMatcher(protocols::dds::VendorId vendor) : expected_(vendor) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::dds::RtpsHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have RTPS header";
            }
            return false;
        }

        const auto* rtps = result.get_layer<protocols::dds::RtpsHeader>();
        if (listener->IsInterested()) {
            *listener << "has vendor " << rtps->vendor_id.to_string();
        }
        return rtps->vendor_id.to_enum() == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has RTPS vendor " << protocols::dds::vendor_id_string(expected_);
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have RTPS vendor " << protocols::dds::vendor_id_string(expected_);
    }

private:
    protocols::dds::VendorId expected_;
};

inline ::testing::PolymorphicMatcher<HasRtpsVendorMatcher> HasRtpsVendor(
    protocols::dds::VendorId vendor) {
    return ::testing::MakePolymorphicMatcher(HasRtpsVendorMatcher(vendor));
}

/// @brief Convenience: Check for FastDDS vendor
inline ::testing::PolymorphicMatcher<HasRtpsVendorMatcher> IsFromFastDDS() {
    return HasRtpsVendor(protocols::dds::VendorId::Eprosima);
}

/// @brief Convenience: Check for RTI Connext vendor
inline ::testing::PolymorphicMatcher<HasRtpsVendorMatcher> IsFromRTI() {
    return HasRtpsVendor(protocols::dds::VendorId::RTI);
}

/// @brief Convenience: Check for CycloneDDS vendor
inline ::testing::PolymorphicMatcher<HasRtpsVendorMatcher> IsFromCycloneDDS() {
    return HasRtpsVendor(protocols::dds::VendorId::Eclipse);
}

/// @brief Convenience: Check for OpenDDS vendor
inline ::testing::PolymorphicMatcher<HasRtpsVendorMatcher> IsFromOpenDDS() {
    return HasRtpsVendor(protocols::dds::VendorId::OCI);
}

/// @brief Matcher: RTPS message has specific GUID prefix
class HasRtpsGuidPrefixMatcher {
public:
    explicit HasRtpsGuidPrefixMatcher(const protocols::dds::GuidPrefix& prefix)
        : expected_(prefix) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::dds::RtpsHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have RTPS header";
            }
            return false;
        }

        const auto* rtps = result.get_layer<protocols::dds::RtpsHeader>();
        if (listener->IsInterested()) {
            *listener << "has GUID prefix " << rtps->guid_prefix.to_string();
        }
        return rtps->guid_prefix == expected_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has RTPS GUID prefix " << expected_.to_string();
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not have RTPS GUID prefix " << expected_.to_string();
    }

private:
    protocols::dds::GuidPrefix expected_;
};

inline ::testing::PolymorphicMatcher<HasRtpsGuidPrefixMatcher> HasRtpsGuidPrefix(
    const protocols::dds::GuidPrefix& prefix) {
    return ::testing::MakePolymorphicMatcher(HasRtpsGuidPrefixMatcher(prefix));
}

/// @brief Matcher: RTPS message contains specific submessage kind
class HasRtpsSubmessageMatcher {
public:
    explicit HasRtpsSubmessageMatcher(protocols::dds::SubmessageKind kind) : expected_(kind) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::dds::RtpsHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have RTPS header";
            }
            return false;
        }

        const auto* rtps = result.get_layer<protocols::dds::RtpsHeader>();
        for (const auto& submsg : rtps->submessages) {
            if (submsg.header.kind == expected_) {
                if (listener->IsInterested()) {
                    *listener << "contains " << protocols::dds::submessage_kind_string(expected_)
                              << " submessage";
                }
                return true;
            }
        }

        if (listener->IsInterested()) {
            *listener << "does not contain " << protocols::dds::submessage_kind_string(expected_)
                      << " submessage (has " << rtps->submessages.size() << " submessages)";
        }
        return false;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "contains RTPS " << protocols::dds::submessage_kind_string(expected_)
            << " submessage";
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not contain RTPS " << protocols::dds::submessage_kind_string(expected_)
            << " submessage";
    }

private:
    protocols::dds::SubmessageKind expected_;
};

inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsSubmessage(
    protocols::dds::SubmessageKind kind) {
    return ::testing::MakePolymorphicMatcher(HasRtpsSubmessageMatcher(kind));
}

/// @brief Convenience: Check for DATA submessage
inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsData() {
    return HasRtpsSubmessage(protocols::dds::SubmessageKind::DATA);
}

/// @brief Convenience: Check for HEARTBEAT submessage
inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsHeartbeat() {
    return HasRtpsSubmessage(protocols::dds::SubmessageKind::HEARTBEAT);
}

/// @brief Convenience: Check for ACKNACK submessage
inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsAckNack() {
    return HasRtpsSubmessage(protocols::dds::SubmessageKind::ACKNACK);
}

/// @brief Convenience: Check for GAP submessage
inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsGap() {
    return HasRtpsSubmessage(protocols::dds::SubmessageKind::GAP);
}

/// @brief Convenience: Check for INFO_TS submessage
inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsInfoTs() {
    return HasRtpsSubmessage(protocols::dds::SubmessageKind::INFO_TS);
}

/// @brief Convenience: Check for INFO_DST submessage
inline ::testing::PolymorphicMatcher<HasRtpsSubmessageMatcher> HasRtpsInfoDst() {
    return HasRtpsSubmessage(protocols::dds::SubmessageKind::INFO_DST);
}

/// @brief Matcher: RTPS message has at least N submessages
class HasRtpsSubmessageCountMatcher {
public:
    explicit HasRtpsSubmessageCountMatcher(std::size_t min_count) : min_count_(min_count) {}

    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::dds::RtpsHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have RTPS header";
            }
            return false;
        }

        const auto* rtps = result.get_layer<protocols::dds::RtpsHeader>();
        if (listener->IsInterested()) {
            *listener << "has " << rtps->submessages.size() << " submessages";
        }
        return rtps->submessages.size() >= min_count_;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "has at least " << min_count_ << " RTPS submessages";
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "has fewer than " << min_count_ << " RTPS submessages";
    }

private:
    std::size_t min_count_;
};

inline ::testing::PolymorphicMatcher<HasRtpsSubmessageCountMatcher> HasRtpsSubmessageCount(
    std::size_t min_count) {
    return ::testing::MakePolymorphicMatcher(HasRtpsSubmessageCountMatcher(min_count));
}

/// @brief Matcher: RTPS message is discovery traffic (SPDP/SEDP)
class IsRtpsDiscoveryMatcher {
public:
    template <typename T>
    bool MatchAndExplain(const T& pkt, ::testing::MatchResultListener* listener) const {
        auto data = detail::get_packet_data(pkt);
        protocols::DecodeStackResult result = protocols::decode_packet(data);

        if (!result.has_layer<protocols::dds::RtpsHeader>()) {
            if (listener->IsInterested()) {
                *listener << "packet does not have RTPS header";
            }
            return false;
        }

        const auto* rtps = result.get_layer<protocols::dds::RtpsHeader>();

        // Check for builtin entity IDs in DATA submessages
        for (const auto& submsg : rtps->submessages) {
            if (submsg.header.kind == protocols::dds::SubmessageKind::DATA) {
                if (auto* data_submsg = std::get_if<protocols::dds::DataSubmessage>(&submsg.body)) {
                    // Check for SPDP writer (participant announcements)
                    if (data_submsg->writer_id.kind ==
                            protocols::dds::EntityKind::BuiltinWriterWithKey &&
                        data_submsg->writer_id.entity_key ==
                            std::array<std::uint8_t, 3>{0x00, 0x01, 0x00}) {
                        if (listener->IsInterested()) {
                            *listener << "is SPDP participant announcement";
                        }
                        return true;
                    }
                    // Check for SEDP publication writer
                    if (data_submsg->writer_id.kind ==
                            protocols::dds::EntityKind::BuiltinWriterWithKey &&
                        data_submsg->writer_id.entity_key ==
                            std::array<std::uint8_t, 3>{0x00, 0x00, 0x03}) {
                        if (listener->IsInterested()) {
                            *listener << "is SEDP publication announcement";
                        }
                        return true;
                    }
                    // Check for SEDP subscription writer
                    if (data_submsg->writer_id.kind ==
                            protocols::dds::EntityKind::BuiltinWriterWithKey &&
                        data_submsg->writer_id.entity_key ==
                            std::array<std::uint8_t, 3>{0x00, 0x00, 0x04}) {
                        if (listener->IsInterested()) {
                            *listener << "is SEDP subscription announcement";
                        }
                        return true;
                    }
                }
            }
        }

        if (listener->IsInterested()) {
            *listener << "is not discovery traffic";
        }
        return false;
    }

    void DescribeTo(std::ostream* os) const { *os << "is RTPS discovery traffic"; }
    void DescribeNegationTo(std::ostream* os) const { *os << "is not RTPS discovery traffic"; }
};

inline ::testing::PolymorphicMatcher<IsRtpsDiscoveryMatcher> IsRtpsDiscovery() {
    return ::testing::MakePolymorphicMatcher(IsRtpsDiscoveryMatcher());
}

/// @brief Alias for discovery matcher
inline ::testing::PolymorphicMatcher<IsRtpsDiscoveryMatcher> IsSpdpOrSedp() {
    return ::testing::MakePolymorphicMatcher(IsRtpsDiscoveryMatcher());
}

}  // namespace wadjet::testing

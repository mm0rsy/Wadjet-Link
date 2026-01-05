#pragma once

/// @file rtps_types.hpp
/// @brief RTPS (Real-Time Publish-Subscribe) type definitions
///
/// This file contains the fundamental types used in the RTPS wire protocol
/// as defined in the OMG DDS-RTPS specification.
///
/// RTPS is the underlying wire protocol for DDS (Data Distribution Service),
/// used in automotive applications like ROS2 for sensor fusion and control.

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace wadjet::protocols::dds {

// =============================================================================
// Constants
// =============================================================================

/// @brief RTPS magic value "RTPS"
inline constexpr std::array<std::uint8_t, 4> RTPS_MAGIC = {'R', 'T', 'P', 'S'};

/// @brief RTPS header size (20 bytes)
inline constexpr std::size_t RTPS_HEADER_SIZE = 20;

/// @brief Submessage header size (4 bytes)
inline constexpr std::size_t SUBMESSAGE_HEADER_SIZE = 4;

/// @brief GUID size (16 bytes = 12 prefix + 4 entity)
inline constexpr std::size_t GUID_SIZE = 16;

/// @brief GuidPrefix size (12 bytes)
inline constexpr std::size_t GUID_PREFIX_SIZE = 12;

/// @brief EntityId size (4 bytes)
inline constexpr std::size_t ENTITY_ID_SIZE = 4;

/// @brief SequenceNumber size (8 bytes)
inline constexpr std::size_t SEQUENCE_NUMBER_SIZE = 8;

/// @brief Locator size (24 bytes)
inline constexpr std::size_t LOCATOR_SIZE = 24;

/// @brief Default RTPS/DDS multicast port base
inline constexpr std::uint16_t RTPS_PORT_BASE = 7400;

/// @brief Port gain for domain calculation
inline constexpr std::uint16_t RTPS_PORT_DOMAIN_GAIN = 250;

/// @brief Default discovery multicast port offset
inline constexpr std::uint16_t RTPS_DISCOVERY_MULTICAST_PORT_OFFSET = 0;

/// @brief Default discovery unicast port offset
inline constexpr std::uint16_t RTPS_DISCOVERY_UNICAST_PORT_OFFSET = 10;

/// @brief Default user multicast port offset
inline constexpr std::uint16_t RTPS_USER_MULTICAST_PORT_OFFSET = 1;

/// @brief Default user unicast port offset
inline constexpr std::uint16_t RTPS_USER_UNICAST_PORT_OFFSET = 11;

// =============================================================================
// Protocol Version
// =============================================================================

/// @brief RTPS protocol version
struct ProtocolVersion {
    std::uint8_t major = 2;
    std::uint8_t minor = 4;

    [[nodiscard]] bool operator==(const ProtocolVersion& other) const = default;

    [[nodiscard]] std::string to_string() const {
        return std::to_string(major) + "." + std::to_string(minor);
    }

    /// @brief RTPS 2.1 version
    static constexpr ProtocolVersion v2_1() { return {2, 1}; }

    /// @brief RTPS 2.2 version
    static constexpr ProtocolVersion v2_2() { return {2, 2}; }

    /// @brief RTPS 2.3 version
    static constexpr ProtocolVersion v2_3() { return {2, 3}; }

    /// @brief RTPS 2.4 version (latest)
    static constexpr ProtocolVersion v2_4() { return {2, 4}; }
};

// =============================================================================
// Vendor ID
// =============================================================================

/// @brief Known DDS vendor IDs
enum class VendorId : std::uint16_t {
    Unknown = 0x0000,
    RTI = 0x0101,          ///< Real-Time Innovations (RTI Connext)
    ADLink = 0x0102,       ///< ADLink (OpenSplice, Vortex, CycloneDDS)
    OCI = 0x0103,          ///< Object Computing Inc (OpenDDS)
    MilSOFT = 0x0104,      ///< MilSOFT
    Gallium = 0x0105,      ///< Gallium Visual Systems
    TwinOaks = 0x0106,     ///< Twin Oaks Computing (CoreDX)
    Lakota = 0x0107,       ///< Lakota Technical Solutions
    ICOUP = 0x0108,        ///< ICOUP Consulting
    ETRI = 0x0109,         ///< ETRI
    RTI_Micro = 0x010A,    ///< RTI Connext DDS Micro
    ADLink_Lite = 0x010B,  ///< ADLink Vortex Lite
    Technicolor = 0x010C,  ///< Technicolor
    Eprosima = 0x010F,     ///< eProsima (Fast DDS / Fast RTPS)
    Eclipse = 0x0120,      ///< Eclipse Foundation (Cyclone DDS)
};

/// @brief Convert vendor ID to string
[[nodiscard]] constexpr std::string_view vendor_id_string(VendorId id) {
    switch (id) {
        case VendorId::Unknown:
            return "Unknown";
        case VendorId::RTI:
            return "RTI";
        case VendorId::ADLink:
            return "ADLink";
        case VendorId::OCI:
            return "OCI";
        case VendorId::MilSOFT:
            return "MilSOFT";
        case VendorId::Gallium:
            return "Gallium";
        case VendorId::TwinOaks:
            return "TwinOaks";
        case VendorId::Lakota:
            return "Lakota";
        case VendorId::ICOUP:
            return "ICOUP";
        case VendorId::ETRI:
            return "ETRI";
        case VendorId::RTI_Micro:
            return "RTI_Micro";
        case VendorId::ADLink_Lite:
            return "ADLink_Lite";
        case VendorId::Technicolor:
            return "Technicolor";
        case VendorId::Eprosima:
            return "eProsima";
        case VendorId::Eclipse:
            return "Eclipse";
        default:
            return "Unknown";
    }
}

/// @brief Vendor ID structure (2 bytes)
struct VendorIdValue {
    std::uint8_t bytes[2] = {0, 0};

    [[nodiscard]] VendorId to_enum() const {
        auto val = static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
        return static_cast<VendorId>(val);
    }

    [[nodiscard]] std::string to_string() const { return std::string(vendor_id_string(to_enum())); }

    [[nodiscard]] bool operator==(const VendorIdValue& other) const {
        return bytes[0] == other.bytes[0] && bytes[1] == other.bytes[1];
    }
};

// =============================================================================
// GuidPrefix and EntityId
// =============================================================================

/// @brief GUID Prefix (12 bytes) - identifies a participant
struct GuidPrefix {
    std::array<std::uint8_t, GUID_PREFIX_SIZE> value = {};

    [[nodiscard]] bool operator==(const GuidPrefix& other) const { return value == other.value; }

    [[nodiscard]] bool operator<(const GuidPrefix& other) const { return value < other.value; }

    [[nodiscard]] bool is_unknown() const {
        for (auto b : value) {
            if (b != 0)
                return false;
        }
        return true;
    }

    [[nodiscard]] std::string to_string() const {
        std::string result;
        result.reserve(35);
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i > 0 && i % 4 == 0)
                result += ':';
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02x", value[i]);
            result += hex;
        }
        return result;
    }

    /// @brief Unknown/Invalid GUID prefix
    static GuidPrefix unknown() { return {}; }
};

/// @brief Entity kind (part of EntityId)
enum class EntityKind : std::uint8_t {
    // User-defined entities
    UserDefinedUnknown = 0x00,
    UserDefinedWriterWithKey = 0x02,
    UserDefinedWriterNoKey = 0x03,
    UserDefinedReaderNoKey = 0x04,
    UserDefinedReaderWithKey = 0x07,

    // Built-in entities
    BuiltinUnknown = 0xC0,
    BuiltinParticipant = 0xC1,
    BuiltinWriterWithKey = 0xC2,
    BuiltinWriterNoKey = 0xC3,
    BuiltinReaderNoKey = 0xC4,
    BuiltinReaderWithKey = 0xC7,
};

/// @brief Entity ID (4 bytes) - identifies an entity within a participant
struct EntityId {
    std::array<std::uint8_t, 3> entity_key = {};
    EntityKind kind = EntityKind::UserDefinedUnknown;

    [[nodiscard]] bool operator==(const EntityId& other) const {
        return entity_key == other.entity_key && kind == other.kind;
    }

    [[nodiscard]] bool operator<(const EntityId& other) const {
        if (entity_key != other.entity_key)
            return entity_key < other.entity_key;
        return static_cast<std::uint8_t>(kind) < static_cast<std::uint8_t>(other.kind);
    }

    [[nodiscard]] std::string to_string() const {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02x%02x%02x.%02x", entity_key[0], entity_key[1],
                      entity_key[2], static_cast<std::uint8_t>(kind));
        return buf;
    }

    [[nodiscard]] bool is_builtin() const {
        return (static_cast<std::uint8_t>(kind) & 0xC0) == 0xC0;
    }

    /// @brief Unknown entity
    static constexpr EntityId unknown() { return {{0, 0, 0}, EntityKind::UserDefinedUnknown}; }

    /// @brief Participant entity
    static constexpr EntityId participant() { return {{0, 0, 1}, EntityKind::BuiltinParticipant}; }

    /// @brief SPDP built-in participant writer
    static constexpr EntityId spdp_builtin_participant_writer() {
        return {{0, 1, 0}, EntityKind::BuiltinWriterWithKey};
    }

    /// @brief SPDP built-in participant reader
    static constexpr EntityId spdp_builtin_participant_reader() {
        return {{0, 1, 0}, EntityKind::BuiltinReaderWithKey};
    }

    /// @brief SEDP built-in publications writer
    static constexpr EntityId sedp_builtin_publications_writer() {
        return {{0, 0, 3}, EntityKind::BuiltinWriterWithKey};
    }

    /// @brief SEDP built-in publications reader
    static constexpr EntityId sedp_builtin_publications_reader() {
        return {{0, 0, 3}, EntityKind::BuiltinReaderWithKey};
    }

    /// @brief SEDP built-in subscriptions writer
    static constexpr EntityId sedp_builtin_subscriptions_writer() {
        return {{0, 0, 4}, EntityKind::BuiltinWriterWithKey};
    }

    /// @brief SEDP built-in subscriptions reader
    static constexpr EntityId sedp_builtin_subscriptions_reader() {
        return {{0, 0, 4}, EntityKind::BuiltinReaderWithKey};
    }

    /// @brief SEDP built-in topics writer
    static constexpr EntityId sedp_builtin_topics_writer() {
        return {{0, 0, 2}, EntityKind::BuiltinWriterWithKey};
    }

    /// @brief SEDP built-in topics reader
    static constexpr EntityId sedp_builtin_topics_reader() {
        return {{0, 0, 2}, EntityKind::BuiltinReaderWithKey};
    }
};

// =============================================================================
// GUID (Globally Unique Identifier)
// =============================================================================

/// @brief GUID (16 bytes) - globally unique identifier for RTPS entities
struct GUID {
    GuidPrefix prefix;
    EntityId entity_id;

    [[nodiscard]] bool operator==(const GUID& other) const {
        return prefix == other.prefix && entity_id == other.entity_id;
    }

    [[nodiscard]] bool operator<(const GUID& other) const {
        if (prefix != other.prefix)
            return prefix < other.prefix;
        return entity_id < other.entity_id;
    }

    [[nodiscard]] std::string to_string() const {
        return prefix.to_string() + "|" + entity_id.to_string();
    }

    [[nodiscard]] bool is_unknown() const {
        return prefix.is_unknown() && entity_id == EntityId::unknown();
    }

    [[nodiscard]] bool is_builtin() const { return entity_id.is_builtin(); }

    /// @brief Unknown GUID
    static GUID unknown() { return {GuidPrefix::unknown(), EntityId::unknown()}; }
};

// =============================================================================
// Sequence Number
// =============================================================================

/// @brief Sequence number (8 bytes, signed 64-bit)
struct SequenceNumber {
    std::int32_t high = 0;
    std::uint32_t low = 0;

    [[nodiscard]] std::int64_t value() const {
        return (static_cast<std::int64_t>(high) << 32) | low;
    }

    [[nodiscard]] bool operator==(const SequenceNumber& other) const {
        return high == other.high && low == other.low;
    }

    [[nodiscard]] bool operator<(const SequenceNumber& other) const {
        return value() < other.value();
    }

    [[nodiscard]] bool operator<=(const SequenceNumber& other) const {
        return value() <= other.value();
    }

    [[nodiscard]] std::string to_string() const { return std::to_string(value()); }

    /// @brief Minimum sequence number
    static constexpr SequenceNumber min() { return {0, 1}; }

    /// @brief Maximum sequence number
    static constexpr SequenceNumber max() { return {0x7FFFFFFF, 0xFFFFFFFF}; }

    /// @brief Unknown/Invalid sequence number
    static constexpr SequenceNumber unknown() { return {-1, 0}; }
};

// =============================================================================
// Locator
// =============================================================================

/// @brief Locator kind
enum class LocatorKind : std::int32_t {
    Invalid = -1,
    Reserved = 0,
    UDPv4 = 1,
    UDPv6 = 2,
    TCPv4_LAN = 4,
    TCPv4_WAN = 8,
    TCPv6 = 16,
    SHM = 32,  ///< Shared memory
};

/// @brief Locator (24 bytes) - network address for RTPS communication
struct Locator {
    LocatorKind kind = LocatorKind::Invalid;
    std::uint32_t port = 0;
    std::array<std::uint8_t, 16> address = {};  // IPv6 or IPv4-mapped

    [[nodiscard]] bool operator==(const Locator& other) const {
        return kind == other.kind && port == other.port && address == other.address;
    }

    [[nodiscard]] bool is_valid() const { return kind != LocatorKind::Invalid; }

    [[nodiscard]] bool is_udp_v4() const { return kind == LocatorKind::UDPv4; }

    [[nodiscard]] bool is_udp_v6() const { return kind == LocatorKind::UDPv6; }

    /// @brief Get IPv4 address (for UDPv4 locators)
    [[nodiscard]] std::array<std::uint8_t, 4> ipv4_address() const {
        // IPv4 is in the last 4 bytes of the address field
        return {address[12], address[13], address[14], address[15]};
    }

    [[nodiscard]] std::string to_string() const {
        if (kind == LocatorKind::UDPv4) {
            auto ipv4 = ipv4_address();
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d.%d.%d.%d:%u", ipv4[0], ipv4[1], ipv4[2], ipv4[3],
                          port);
            return buf;
        }
        return "Locator{kind=" + std::to_string(static_cast<int>(kind)) +
               ",port=" + std::to_string(port) + "}";
    }

    /// @brief Invalid locator
    static Locator invalid() { return {LocatorKind::Invalid, 0, {}}; }
};

// =============================================================================
// Time
// =============================================================================

/// @brief RTPS Time (8 bytes)
struct Time {
    std::int32_t seconds = 0;
    std::uint32_t fraction = 0;  ///< Fraction in 2^-32 seconds units

    [[nodiscard]] bool operator==(const Time& other) const {
        return seconds == other.seconds && fraction == other.fraction;
    }

    [[nodiscard]] double to_seconds_double() const { return seconds + fraction / 4294967296.0; }

    [[nodiscard]] std::int64_t to_nanoseconds() const {
        return static_cast<std::int64_t>(seconds) * 1'000'000'000LL +
               static_cast<std::int64_t>(fraction * 1000000000.0 / 4294967296.0);
    }

    [[nodiscard]] std::string to_string() const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d.%09lu", seconds,
                      static_cast<unsigned long>(fraction * 1000000000ULL / 4294967296ULL));
        return buf;
    }

    /// @brief Check if this is infinite time/duration
    [[nodiscard]] bool is_infinite() const {
        return seconds == 0x7FFFFFFF && fraction == 0xFFFFFFFF;
    }

    /// @brief Invalid time
    static constexpr Time invalid() { return {-1, 0xFFFFFFFF}; }

    /// @brief Zero time
    static constexpr Time zero() { return {0, 0}; }

    /// @brief Infinite time
    static constexpr Time infinite() { return {0x7FFFFFFF, 0xFFFFFFFF}; }
};

/// @brief Time subtraction operator
[[nodiscard]] inline Time operator-(const Time& a, const Time& b) {
    Time result;
    result.seconds = a.seconds - b.seconds;
    if (a.fraction >= b.fraction) {
        result.fraction = a.fraction - b.fraction;
    } else {
        result.seconds -= 1;
        result.fraction = static_cast<std::uint32_t>(0x100000000ULL + a.fraction - b.fraction);
    }
    return result;
}

// =============================================================================
// Duration
// =============================================================================

/// @brief Duration (same format as Time)
using Duration = Time;

// =============================================================================
// Count
// =============================================================================

/// @brief Count type used in RTPS
struct Count {
    std::int32_t value = 0;

    [[nodiscard]] bool operator==(const Count& other) const { return value == other.value; }
};

// =============================================================================
// BuiltinEndpointSet
// =============================================================================

/// @brief Built-in endpoint set flags
struct BuiltinEndpointSet {
    std::uint32_t value = 0;

    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER = 1 << 0;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR = 1 << 1;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER = 1 << 2;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR = 1 << 3;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER = 1 << 4;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR = 1 << 5;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_TOPIC_ANNOUNCER = 1 << 6;
    static constexpr std::uint32_t DISC_BUILTIN_ENDPOINT_TOPIC_DETECTOR = 1 << 7;
    static constexpr std::uint32_t BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER = 1 << 10;
    static constexpr std::uint32_t BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER = 1 << 11;

    [[nodiscard]] bool has_participant_announcer() const {
        return (value & DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER) != 0;
    }

    [[nodiscard]] bool has_participant_detector() const {
        return (value & DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR) != 0;
    }

    [[nodiscard]] bool has_publication_announcer() const {
        return (value & DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER) != 0;
    }

    [[nodiscard]] bool has_publication_detector() const {
        return (value & DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR) != 0;
    }

    [[nodiscard]] bool has_subscription_announcer() const {
        return (value & DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER) != 0;
    }

    [[nodiscard]] bool has_subscription_detector() const {
        return (value & DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR) != 0;
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Calculate discovery multicast port for a domain
[[nodiscard]] inline std::uint16_t discovery_multicast_port(std::uint16_t domain_id) {
    return static_cast<std::uint16_t>(RTPS_PORT_BASE + RTPS_PORT_DOMAIN_GAIN * domain_id +
                                      RTPS_DISCOVERY_MULTICAST_PORT_OFFSET);
}

/// @brief Calculate discovery unicast port for a domain and participant
[[nodiscard]] inline std::uint16_t discovery_unicast_port(std::uint16_t domain_id,
                                                          std::uint16_t participant_id) {
    return static_cast<std::uint16_t>(RTPS_PORT_BASE + RTPS_PORT_DOMAIN_GAIN * domain_id +
                                      RTPS_DISCOVERY_UNICAST_PORT_OFFSET + participant_id);
}

/// @brief Calculate user multicast port for a domain
[[nodiscard]] inline std::uint16_t user_multicast_port(std::uint16_t domain_id) {
    return static_cast<std::uint16_t>(RTPS_PORT_BASE + RTPS_PORT_DOMAIN_GAIN * domain_id +
                                      RTPS_USER_MULTICAST_PORT_OFFSET);
}

/// @brief Calculate user unicast port for a domain and participant
[[nodiscard]] inline std::uint16_t user_unicast_port(std::uint16_t domain_id,
                                                     std::uint16_t participant_id) {
    return static_cast<std::uint16_t>(RTPS_PORT_BASE + RTPS_PORT_DOMAIN_GAIN * domain_id +
                                      RTPS_USER_UNICAST_PORT_OFFSET + participant_id);
}

/// @brief Check if a port is likely an RTPS discovery port
[[nodiscard]] inline bool is_likely_rtps_port(std::uint16_t port) {
    // Exclude well-known non-RTPS ports
    constexpr std::uint16_t DOIP_PORT = 13400;  // DoIP discovery/diagnostic port
    if (port == DOIP_PORT)
        return false;

    // RTPS ports are typically in the 7400-7900+ range
    // Check for discovery multicast/unicast and user multicast/unicast patterns
    if (port < RTPS_PORT_BASE)
        return false;
    if (port > RTPS_PORT_BASE + RTPS_PORT_DOMAIN_GAIN * 230 + 20)
        return false;

    // Check if it matches the pattern for any reasonable domain
    auto offset = static_cast<std::uint16_t>((port - RTPS_PORT_BASE) % RTPS_PORT_DOMAIN_GAIN);
    return offset == RTPS_DISCOVERY_MULTICAST_PORT_OFFSET ||
           offset >= RTPS_DISCOVERY_UNICAST_PORT_OFFSET;
}

}  // namespace wadjet::protocols::dds

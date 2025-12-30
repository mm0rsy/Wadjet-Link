#pragma once

/// @file gptp_types.hpp
/// @brief gPTP (IEEE 802.1AS) type definitions
/// 
/// This file contains the fundamental types used in the Generalized Precision
/// Time Protocol (gPTP) as defined in IEEE 802.1AS-2020.

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace wadjet::protocols::gptp {

/// @brief gPTP EtherType (same as PTP)
inline constexpr std::uint16_t GPTP_ETHERTYPE = 0x88F7;

/// @brief gPTP header size (34 bytes base header)
inline constexpr std::size_t HEADER_SIZE = 34;

/// @brief gPTP common message header size
inline constexpr std::size_t COMMON_HEADER_SIZE = 34;

/// @brief Timestamp size in gPTP messages (10 bytes: 6 seconds + 4 nanoseconds)
inline constexpr std::size_t TIMESTAMP_SIZE = 10;

/// @brief TLV header size (type + length = 4 bytes)
inline constexpr std::size_t TLV_HEADER_SIZE = 4;

/// @brief Message body sizes (after common header)
inline constexpr std::size_t SYNC_MESSAGE_SIZE = 10;
inline constexpr std::size_t FOLLOW_UP_MESSAGE_SIZE = 10;
inline constexpr std::size_t PDELAY_REQ_MESSAGE_SIZE = 20;
inline constexpr std::size_t PDELAY_RESP_MESSAGE_SIZE = 20;
inline constexpr std::size_t PDELAY_RESP_FOLLOW_UP_MESSAGE_SIZE = 20;
inline constexpr std::size_t ANNOUNCE_MESSAGE_SIZE = 30;
inline constexpr std::size_t SIGNALING_MESSAGE_SIZE = 10;

/// @brief Follow_Up TLV size (without TLV header)
inline constexpr std::size_t FOLLOW_UP_TLV_SIZE = 28;

/// @brief gPTP multicast MAC address for all time-aware systems
inline constexpr std::array<std::uint8_t, 6> GPTP_MULTICAST_MAC = {
    0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E
};

/// @brief gPTP multicast MAC address for Pdelay messages
inline constexpr std::array<std::uint8_t, 6> GPTP_PDELAY_MULTICAST_MAC = {
    0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E
};

/// @brief gPTP protocol version (IEEE 802.1AS)
inline constexpr std::uint8_t GPTP_VERSION_MAJOR = 2;
inline constexpr std::uint8_t GPTP_VERSION_MINOR = 1;

/// @brief PTP/gPTP domain number for automotive profile
inline constexpr std::uint8_t AUTOMOTIVE_DOMAIN = 0;

/// @brief gPTP message types as defined in IEEE 1588-2019 / 802.1AS-2020
enum class MessageType : std::uint8_t {
    Sync = 0x0,                     ///< Sync message
    Delay_Req = 0x1,                ///< Delay_Req message (not used in gPTP)
    Pdelay_Req = 0x2,               ///< Peer delay request
    Pdelay_Resp = 0x3,              ///< Peer delay response
    Reserved_4 = 0x4,               ///< Reserved
    Reserved_5 = 0x5,               ///< Reserved
    Reserved_6 = 0x6,               ///< Reserved
    Reserved_7 = 0x7,               ///< Reserved
    Follow_Up = 0x8,                ///< Follow_Up message
    Delay_Resp = 0x9,               ///< Delay_Resp message (not used in gPTP)
    Pdelay_Resp_Follow_Up = 0xA,    ///< Peer delay response follow up
    Announce = 0xB,                 ///< Announce message
    Signaling = 0xC,                ///< Signaling message
    Management = 0xD,               ///< Management message
    Reserved_E = 0xE,               ///< Reserved
    Reserved_F = 0xF,               ///< Reserved
};

/// @brief Convert message type to human-readable string
[[nodiscard]] constexpr std::string_view message_type_string(MessageType type) {
    switch (type) {
        case MessageType::Sync: return "Sync";
        case MessageType::Delay_Req: return "Delay_Req";
        case MessageType::Pdelay_Req: return "Pdelay_Req";
        case MessageType::Pdelay_Resp: return "Pdelay_Resp";
        case MessageType::Follow_Up: return "Follow_Up";
        case MessageType::Delay_Resp: return "Delay_Resp";
        case MessageType::Pdelay_Resp_Follow_Up: return "Pdelay_Resp_Follow_Up";
        case MessageType::Announce: return "Announce";
        case MessageType::Signaling: return "Signaling";
        case MessageType::Management: return "Management";
        default: return "Reserved";
    }
}

/// @brief Check if message type is an event message (requires timestamp)
[[nodiscard]] constexpr bool is_event_message(MessageType type) {
    return type == MessageType::Sync ||
           type == MessageType::Delay_Req ||
           type == MessageType::Pdelay_Req ||
           type == MessageType::Pdelay_Resp;
}

/// @brief Check if message type is a general message
[[nodiscard]] constexpr bool is_general_message(MessageType type) {
    return !is_event_message(type);
}

/// @brief gPTP transport specific values
enum class TransportSpecific : std::uint8_t {
    IEEE_802_1AS = 0x1,             ///< gPTP (802.1AS) profile
    IEEE_1588 = 0x0,                ///< Standard PTP (IEEE 1588)
};

/// @brief gPTP flags in header
struct GptpFlags {
    // Octet 1 (flags[0])
    bool alternate_master = false;      ///< alternateMasterFlag
    bool two_step = false;              ///< twoStepFlag
    bool unicast = false;               ///< unicastFlag
    bool ptp_profile_specific_1 = false; ///< PTP profile Specific 1
    bool ptp_profile_specific_2 = false; ///< PTP profile Specific 2
    bool reserved = false;              ///< Reserved

    // Octet 2 (flags[1])
    bool leap_61 = false;               ///< leap61
    bool leap_59 = false;               ///< leap59
    bool current_utc_offset_valid = false; ///< currentUtcOffsetValid
    bool ptp_timescale = false;         ///< ptpTimescale
    bool time_traceable = false;        ///< timeTraceable
    bool frequency_traceable = false;   ///< frequencyTraceable

    /// @brief Parse flags from two bytes
    static GptpFlags from_bytes(std::uint8_t octet0, std::uint8_t octet1) {
        GptpFlags flags;
        // Octet 0
        flags.alternate_master = (octet0 & 0x01) != 0;
        flags.two_step = (octet0 & 0x02) != 0;
        flags.unicast = (octet0 & 0x04) != 0;
        flags.ptp_profile_specific_1 = (octet0 & 0x20) != 0;
        flags.ptp_profile_specific_2 = (octet0 & 0x40) != 0;

        // Octet 1
        flags.leap_61 = (octet1 & 0x01) != 0;
        flags.leap_59 = (octet1 & 0x02) != 0;
        flags.current_utc_offset_valid = (octet1 & 0x04) != 0;
        flags.ptp_timescale = (octet1 & 0x08) != 0;
        flags.time_traceable = (octet1 & 0x10) != 0;
        flags.frequency_traceable = (octet1 & 0x20) != 0;

        return flags;
    }

    /// @brief Parse flags from 16-bit raw value (network byte order)
    static GptpFlags from_raw(std::uint16_t raw) {
        auto octet0 = static_cast<std::uint8_t>((raw >> 8) & 0xFF);
        auto octet1 = static_cast<std::uint8_t>(raw & 0xFF);
        return from_bytes(octet0, octet1);
    }

    /// @brief Convert flags back to bytes
    [[nodiscard]] std::pair<std::uint8_t, std::uint8_t> to_bytes() const {
        std::uint8_t octet0 = 0;
        std::uint8_t octet1 = 0;

        if (alternate_master) octet0 |= 0x01;
        if (two_step) octet0 |= 0x02;
        if (unicast) octet0 |= 0x04;
        if (ptp_profile_specific_1) octet0 |= 0x20;
        if (ptp_profile_specific_2) octet0 |= 0x40;

        if (leap_61) octet1 |= 0x01;
        if (leap_59) octet1 |= 0x02;
        if (current_utc_offset_valid) octet1 |= 0x04;
        if (ptp_timescale) octet1 |= 0x08;
        if (time_traceable) octet1 |= 0x10;
        if (frequency_traceable) octet1 |= 0x20;

        return {octet0, octet1};
    }
};

/// @brief Clock identity (8 bytes EUI-64 format)
/// 
/// Typically derived from MAC address by inserting 0xFF 0xFE in the middle.
/// Example: MAC 00:11:22:33:44:55 -> Clock ID 00:11:22:FF:FE:33:44:55
struct ClockIdentity {
    std::array<std::uint8_t, 8> bytes = {};

    ClockIdentity() = default;

    explicit ClockIdentity(const std::array<std::uint8_t, 8>& data) : bytes(data) {}

    /// @brief Create clock identity from MAC address (EUI-48 to EUI-64)
    static ClockIdentity from_mac(const MacAddress& mac) {
        ClockIdentity id;
        id.bytes[0] = mac.bytes[0];
        id.bytes[1] = mac.bytes[1];
        id.bytes[2] = mac.bytes[2];
        id.bytes[3] = 0xFF;
        id.bytes[4] = 0xFE;
        id.bytes[5] = mac.bytes[3];
        id.bytes[6] = mac.bytes[4];
        id.bytes[7] = mac.bytes[5];
        return id;
    }

    /// @brief Parse clock identity from raw bytes
    static ClockIdentity from_bytes(const std::byte* ptr) {
        ClockIdentity id;
        std::memcpy(id.bytes.data(), ptr, 8);
        return id;
    }

    /// @brief Check equality
    bool operator==(const ClockIdentity& other) const {
        return bytes == other.bytes;
    }

    bool operator!=(const ClockIdentity& other) const {
        return !(*this == other);
    }

    /// @brief Convert to string representation
    [[nodiscard]] std::string to_string() const {
        char buf[24];
        std::snprintf(buf, sizeof(buf),
            "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7]);
        return buf;
    }

    /// @brief Check if this is an all-zeros identity
    [[nodiscard]] bool is_zero() const {
        for (auto b : bytes) {
            if (b != 0) return false;
        }
        return true;
    }
};

/// @brief Port identity (clock identity + port number)
struct PortIdentity {
    ClockIdentity clock_identity;   ///< 8-byte clock identity
    std::uint16_t port_number = 0;  ///< Port number (1-based typically)

    PortIdentity() = default;

    PortIdentity(const ClockIdentity& clock, std::uint16_t port)
        : clock_identity(clock), port_number(port) {}

    /// @brief Parse port identity from raw bytes (10 bytes)
    static PortIdentity from_bytes(const std::byte* ptr);

    /// @brief Check equality
    bool operator==(const PortIdentity& other) const {
        return clock_identity == other.clock_identity &&
               port_number == other.port_number;
    }

    bool operator!=(const PortIdentity& other) const {
        return !(*this == other);
    }

    /// @brief Convert to string representation
    [[nodiscard]] std::string to_string() const {
        return clock_identity.to_string() + "-" + std::to_string(port_number);
    }
};

/// @brief gPTP scaled nanoseconds (64-bit signed, scaled by 2^16)
/// 
/// Used for correction fields. The value represents nanoseconds scaled by 2^16.
struct ScaledNanoseconds {
    std::int64_t scaled_ns = 0;  ///< Scaled nanoseconds (value * 2^16)

    ScaledNanoseconds() = default;
    
    explicit ScaledNanoseconds(std::int64_t val) : scaled_ns(val) {}

    /// @brief Parse from 8-byte correction field (scaled nanoseconds * 2^16)
    static ScaledNanoseconds from_correction_field(const std::byte* ptr);

    /// @brief Convert to nanoseconds (integer)
    [[nodiscard]] std::int64_t to_nanoseconds() const {
        return scaled_ns >> 16;
    }

    /// @brief Convert to double (in nanoseconds, with fractional part)
    [[nodiscard]] double to_double() const {
        return static_cast<double>(scaled_ns) / 65536.0;
    }

    /// @brief Check if value is zero
    [[nodiscard]] bool is_zero() const {
        return scaled_ns == 0;
    }
};

/// @brief gPTP timestamp (80-bit: 48-bit seconds + 32-bit nanoseconds)
/// 
/// Represents a precise point in time with sub-nanosecond resolution support.
struct GptpTimestamp {
    std::uint16_t seconds_msb = 0;     ///< Upper 16 bits of seconds
    std::uint32_t seconds_lsb = 0;     ///< Lower 32 bits of seconds
    std::uint32_t nanoseconds = 0;     ///< Nanoseconds (0-999,999,999)

    GptpTimestamp() = default;

    GptpTimestamp(std::uint64_t sec, std::uint32_t nsec)
        : seconds_msb(static_cast<std::uint16_t>((sec >> 32) & 0xFFFF)),
          seconds_lsb(static_cast<std::uint32_t>(sec & 0xFFFFFFFF)),
          nanoseconds(nsec) {}

    /// @brief Get full 48-bit seconds value
    [[nodiscard]] std::uint64_t seconds() const {
        return (static_cast<std::uint64_t>(seconds_msb) << 32) | seconds_lsb;
    }

    /// @brief Parse from 10-byte timestamp (6 bytes seconds + 4 bytes nanoseconds)
    static GptpTimestamp from_bytes(const std::byte* ptr);

    /// @brief Check if timestamp is zero
    [[nodiscard]] bool is_zero() const {
        return seconds_msb == 0 && seconds_lsb == 0 && nanoseconds == 0;
    }

    /// @brief Convert to total nanoseconds (may overflow for large values)
    [[nodiscard]] std::uint64_t to_nanoseconds() const {
        return seconds() * 1'000'000'000ULL + nanoseconds;
    }

    /// @brief Convert to double seconds
    [[nodiscard]] double to_seconds_double() const {
        return static_cast<double>(seconds()) +
               static_cast<double>(nanoseconds) / 1'000'000'000.0;
    }

    /// @brief Subtract two timestamps (a - b) returning signed nanoseconds
    [[nodiscard]] std::int64_t operator-(const GptpTimestamp& other) const {
        auto this_ns = static_cast<std::int64_t>(to_nanoseconds());
        auto other_ns = static_cast<std::int64_t>(other.to_nanoseconds());
        return this_ns - other_ns;
    }

    /// @brief Convert to string representation
    [[nodiscard]] std::string to_string() const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%lu.%09u", 
            static_cast<unsigned long>(seconds()), nanoseconds);
        return buf;
    }
};

/// @brief Clock quality (from Announce messages)
struct ClockQuality {
    std::uint8_t clock_class = 0;           ///< Clock class (248 = default)
    std::uint8_t clock_accuracy = 0;        ///< Clock accuracy enumeration
    std::uint16_t offset_scaled_log_variance = 0; ///< Offset scaled log variance

    /// @brief Parse from 4 bytes
    static ClockQuality from_bytes(const std::byte* ptr);

    /// @brief Convert to string representation
    [[nodiscard]] std::string to_string() const;
};

/// @brief Time interval (scaled nanoseconds for rates)
/// 
/// Represents a time interval in log2 form. For example:
/// - logInterval = 0  means interval = 2^0 = 1 second
/// - logInterval = -3 means interval = 2^-3 = 125 milliseconds
struct LogInterval {
    std::int8_t value = 0;

    LogInterval() = default;
    explicit LogInterval(std::int8_t v) : value(v) {}

    /// @brief Convert to seconds as a double
    [[nodiscard]] double to_seconds() const {
        return std::pow(2.0, static_cast<double>(value));
    }

    /// @brief Convert to milliseconds as a double
    [[nodiscard]] double to_milliseconds() const {
        return to_seconds() * 1000.0;
    }

    /// @brief Convert to string representation
    [[nodiscard]] std::string to_string() const {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "2^%d (%.3f ms)", value, to_milliseconds());
        return buf;
    }
};

/// @brief TLV type enumeration for gPTP TLVs
enum class TlvType : std::uint16_t {
    // Standard TLVs
    Management = 0x0001,
    ManagementErrorStatus = 0x0002,
    ORGANIZATION_EXTENSION = 0x0003,  ///< Organization Extension TLV
    RequestUnicastTransmission = 0x0004,
    GrantUnicastTransmission = 0x0005,
    CancelUnicastTransmission = 0x0006,
    AcknowledgeCancelUnicastTransmission = 0x0007,
    PATH_TRACE = 0x0008,              ///< Path Trace TLV
    AlternateTimeOffsetIndicator = 0x0009,

    // gPTP specific (802.1AS)
    OrganizationExtensionPropagate = 0x4000,
    OrganizationExtensionDoNotPropagate = 0x8000,
    L1Sync = 0x8001,
    PortCommunicationAvailable = 0x8002,
    ProtocolAddress = 0x8003,
    SlaveRxSyncTimingData = 0x8004,
    SlaveRxSyncComputedData = 0x8005,
    SlaveTxEventTimestamps = 0x8006,
    CumulativeScaledRateOffset = 0x8007,

    // Authentication
    Authentication = 0x2000,
    AuthenticationChallenge = 0x2001,
    SecurityAssociationUpdate = 0x2002,
    CumFreqScaleFactorOffset = 0x2003,
};

/// @brief Time source enumeration (for Announce messages)
enum class TimeSource : std::uint8_t {
    AtomicClock = 0x10,        ///< Atomic clock
    GPS = 0x20,                ///< GPS
    TerrestrialRadio = 0x30,   ///< Terrestrial radio
    PTP = 0x40,                ///< PTP (synchronized to another PTP clock)
    NTP = 0x50,                ///< NTP
    HandSet = 0x60,            ///< Hand set
    Other = 0x90,              ///< Other
    InternalOscillator = 0xA0, ///< Internal oscillator
};

/// @brief Convert TLV type to string
[[nodiscard]] std::string_view tlv_type_string(TlvType type);

}  // namespace wadjet::protocols::gptp

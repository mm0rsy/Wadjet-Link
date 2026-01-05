#pragma once

/// @file rtps_messages.hpp
/// @brief RTPS submessage body structures
///
/// This file contains the detailed structures for each RTPS submessage type
/// including DATA, HEARTBEAT, ACKNACK, GAP, and INFO_* submessages.

#include "wadjet/protocols/dds/rtps_types.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wadjet::protocols::dds {

// =============================================================================
// DATA Submessage Flags
// =============================================================================

/// @brief DATA submessage flags
struct DataFlags {
    bool endianness = false;    ///< E: false=big-endian, true=little-endian
    bool inline_qos = false;    ///< Q: inline QoS present
    bool data_present = false;  ///< D: serialized data present
    bool key_present = false;   ///< K: serialized key present

    /// @brief Parse from raw flags byte
    static DataFlags from_byte(std::uint8_t byte) {
        DataFlags f;
        f.endianness = (byte & 0x01) != 0;
        f.inline_qos = (byte & 0x02) != 0;
        f.data_present = (byte & 0x04) != 0;
        f.key_present = (byte & 0x08) != 0;
        return f;
    }
};

// =============================================================================
// DATA Submessage
// =============================================================================

/// @brief DATA submessage body
///
/// The DATA submessage sends application data or discovery information.
/// Layout after submessage header:
/// - 2 bytes: Extra flags
/// - 2 bytes: Octets to inline QoS (if Q flag set)
/// - EntityId: Reader ID
/// - EntityId: Writer ID
/// - SequenceNumber: Writer sequence number
/// - [Inline QoS]: If Q flag set
/// - [Serialized data]: If D flag set
/// - [Serialized key]: If K flag set
struct DataSubmessage {
    std::uint16_t extra_flags = 0;
    std::uint16_t octets_to_inline_qos = 0;
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumber writer_sn;

    DataFlags flags;

    /// @brief Inline QoS parameters (raw, if present)
    std::vector<std::byte> inline_qos;

    /// @brief Serialized payload data
    std::vector<std::byte> serialized_payload;

    /// @brief Check if this is discovery data
    [[nodiscard]] bool is_discovery_data() const { return writer_id.is_builtin(); }

    /// @brief Check if this is user data
    [[nodiscard]] bool is_user_data() const { return !writer_id.is_builtin(); }

    /// @brief Check if payload contains key only
    [[nodiscard]] bool is_key_only() const { return flags.key_present && !flags.data_present; }
};

// =============================================================================
// DATA_FRAG Submessage
// =============================================================================

/// @brief DATA_FRAG submessage body
///
/// Fragmented data for large samples.
struct DataFragSubmessage {
    std::uint16_t extra_flags = 0;
    std::uint16_t octets_to_inline_qos = 0;
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumber writer_sn;
    std::uint32_t fragment_starting_num = 0;
    std::uint16_t fragments_in_submessage = 0;
    std::uint16_t fragment_size = 0;
    std::uint32_t sample_size = 0;

    /// @brief Inline QoS (if present)
    std::vector<std::byte> inline_qos;

    /// @brief Fragment data
    std::vector<std::byte> fragment_data;
};

// =============================================================================
// HEARTBEAT Submessage
// =============================================================================

/// @brief HEARTBEAT submessage flags
struct HeartbeatFlags {
    bool endianness = false;       ///< E: false=big-endian, true=little-endian
    bool final_flag = false;       ///< F: final heartbeat
    bool liveliness_flag = false;  ///< L: liveliness heartbeat

    static HeartbeatFlags from_byte(std::uint8_t byte) {
        HeartbeatFlags f;
        f.endianness = (byte & 0x01) != 0;
        f.final_flag = (byte & 0x02) != 0;
        f.liveliness_flag = (byte & 0x04) != 0;
        return f;
    }
};

/// @brief HEARTBEAT submessage body
///
/// Announces available sequence numbers from a writer.
/// Layout:
/// - EntityId: Reader ID
/// - EntityId: Writer ID
/// - SequenceNumber: First sequence number
/// - SequenceNumber: Last sequence number
/// - Count: Count
struct HeartbeatSubmessage {
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumber first_sn;
    SequenceNumber last_sn;
    Count count;
    HeartbeatFlags flags;

    /// @brief Get the range of available sequence numbers
    [[nodiscard]] std::int64_t sequence_range() const {
        return last_sn.value() - first_sn.value() + 1;
    }
};

// =============================================================================
// HEARTBEAT_FRAG Submessage
// =============================================================================

/// @brief HEARTBEAT_FRAG submessage body
struct HeartbeatFragSubmessage {
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumber writer_sn;
    std::uint32_t last_fragment_num = 0;
    Count count;
};

// =============================================================================
// ACKNACK Submessage
// =============================================================================

/// @brief ACKNACK submessage flags
struct AckNackFlags {
    bool endianness = false;  ///< E: false=big-endian, true=little-endian
    bool final_flag = false;  ///< F: final ACKNACK

    static AckNackFlags from_byte(std::uint8_t byte) {
        AckNackFlags f;
        f.endianness = (byte & 0x01) != 0;
        f.final_flag = (byte & 0x02) != 0;
        return f;
    }
};

/// @brief Sequence number set (bitmap)
struct SequenceNumberSet {
    SequenceNumber base;
    std::uint32_t num_bits = 0;
    std::vector<std::uint32_t> bitmap;

    /// @brief Check if a sequence number is in the set
    [[nodiscard]] bool contains(SequenceNumber sn) const {
        auto offset = sn.value() - base.value();
        if (offset < 0 || offset >= static_cast<std::int64_t>(num_bits)) {
            return false;
        }
        auto word_index = static_cast<std::size_t>(offset / 32);
        auto bit_index = static_cast<std::size_t>(offset % 32);
        if (word_index >= bitmap.size()) {
            return false;
        }
        return (bitmap[word_index] & (1u << (31 - bit_index))) != 0;
    }

    /// @brief Get count of sequence numbers in set
    [[nodiscard]] std::size_t count() const {
        std::size_t c = 0;
        for (std::size_t i = 0; i < num_bits; ++i) {
            auto word_index = i / 32;
            auto bit_index = i % 32;
            if (word_index < bitmap.size()) {
                if ((bitmap[word_index] & (1u << (31 - bit_index))) != 0) {
                    ++c;
                }
            }
        }
        return c;
    }
};

/// @brief ACKNACK submessage body
///
/// Acknowledges received sequence numbers and requests missing ones.
/// Layout:
/// - EntityId: Reader ID
/// - EntityId: Writer ID
/// - SequenceNumberSet: Reader SN state
/// - Count: Count
struct AckNackSubmessage {
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumberSet reader_sn_state;
    Count count;
    AckNackFlags flags;

    /// @brief Get number of missing sequence numbers (NACKs)
    [[nodiscard]] std::size_t missing_count() const { return reader_sn_state.count(); }
};

// =============================================================================
// NACK_FRAG Submessage
// =============================================================================

/// @brief Fragment number set (bitmap)
struct FragmentNumberSet {
    std::uint32_t base = 0;
    std::uint32_t num_bits = 0;
    std::vector<std::uint32_t> bitmap;
};

/// @brief NACK_FRAG submessage body
struct NackFragSubmessage {
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumber writer_sn;
    FragmentNumberSet fragment_number_state;
    Count count;
};

// =============================================================================
// GAP Submessage
// =============================================================================

/// @brief GAP submessage body
///
/// Informs a reader that certain sequence numbers are no longer relevant.
/// Layout:
/// - EntityId: Reader ID
/// - EntityId: Writer ID
/// - SequenceNumber: Gap start
/// - SequenceNumberSet: Gap list
struct GapSubmessage {
    EntityId reader_id;
    EntityId writer_id;
    SequenceNumber gap_start;
    SequenceNumberSet gap_list;

    /// @brief Get the start of the irrelevant range
    [[nodiscard]] SequenceNumber irrelevant_start() const { return gap_start; }
};

// =============================================================================
// INFO_TS Submessage
// =============================================================================

/// @brief INFO_TS submessage flags
struct InfoTimestampFlags {
    bool endianness = false;       ///< E: false=big-endian, true=little-endian
    bool invalidate_flag = false;  ///< T: invalidate timestamp

    static InfoTimestampFlags from_byte(std::uint8_t byte) {
        InfoTimestampFlags f;
        f.endianness = (byte & 0x01) != 0;
        f.invalidate_flag = (byte & 0x02) != 0;
        return f;
    }
};

/// @brief INFO_TS submessage body
///
/// Provides timestamp for subsequent submessages.
struct InfoTimestampSubmessage {
    std::optional<Time> timestamp;
    InfoTimestampFlags flags;

    /// @brief Check if timestamp is valid
    [[nodiscard]] bool has_timestamp() const {
        return timestamp.has_value() && !flags.invalidate_flag;
    }
};

// =============================================================================
// INFO_SRC Submessage
// =============================================================================

/// @brief INFO_SRC submessage body
///
/// Changes the source of subsequent submessages.
struct InfoSourceSubmessage {
    std::uint32_t unused = 0;
    ProtocolVersion version;
    VendorIdValue vendor_id;
    GuidPrefix guid_prefix;
};

// =============================================================================
// INFO_DST Submessage
// =============================================================================

/// @brief INFO_DST submessage body
///
/// Specifies the destination participant for subsequent submessages.
struct InfoDestinationSubmessage {
    GuidPrefix guid_prefix;
};

// =============================================================================
// INFO_REPLY Submessage
// =============================================================================

/// @brief INFO_REPLY submessage flags
struct InfoReplyFlags {
    bool endianness = false;      ///< E: false=big-endian, true=little-endian
    bool multicast_flag = false;  ///< M: multicast locator present

    static InfoReplyFlags from_byte(std::uint8_t byte) {
        InfoReplyFlags f;
        f.endianness = (byte & 0x01) != 0;
        f.multicast_flag = (byte & 0x02) != 0;
        return f;
    }
};

/// @brief Locator list
struct LocatorList {
    std::vector<Locator> locators;

    [[nodiscard]] bool empty() const { return locators.empty(); }
    [[nodiscard]] std::size_t size() const { return locators.size(); }
};

/// @brief INFO_REPLY submessage body
///
/// Provides reply locators for the source.
struct InfoReplySubmessage {
    LocatorList unicast_locator_list;
    LocatorList multicast_locator_list;
    InfoReplyFlags flags;
};

// =============================================================================
// INFO_REPLY_IP4 Submessage
// =============================================================================

/// @brief INFO_REPLY_IP4 submessage body (compact IPv4 version)
struct InfoReplyIp4Submessage {
    Locator unicast_locator;
    std::optional<Locator> multicast_locator;
};

// =============================================================================
// PAD Submessage
// =============================================================================

/// @brief PAD submessage body
///
/// Used for alignment; contains no data.
struct PadSubmessage {
    // No fields - padding only
};

}  // namespace wadjet::protocols::dds

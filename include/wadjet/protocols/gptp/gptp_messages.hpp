#pragma once

/// @file gptp_messages.hpp
/// @brief gPTP message structures and TLV parsing
///
/// This file contains the message-specific structures for gPTP (IEEE 802.1AS)
/// including Sync, Follow_Up, Pdelay, and Announce messages.

#include "wadjet/protocols/gptp/gptp_types.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace wadjet::protocols::gptp {

/// @brief TLV (Type-Length-Value) structure
struct Tlv {
    TlvType type = TlvType::Management;
    std::uint16_t length = 0;
    std::vector<std::byte> value;

    /// @brief Parse TLV from bytes
    /// @param ptr Pointer to TLV data
    /// @param remaining Bytes remaining in buffer
    /// @return Parsed TLV and bytes consumed, or nullopt on error
    static std::optional<std::pair<Tlv, std::size_t>> parse(const std::byte* ptr,
                                                            std::size_t remaining);
};

/// @brief Follow_Up TLV for IEEE 802.1AS (organization extension)
struct FollowUpTlv {
    static constexpr std::array<std::uint8_t, 3> IEEE_802_1_OUI = {0x00, 0x80, 0xC2};
    static constexpr std::uint32_t SUBTYPE_FOLLOW_UP = 1;

    std::array<std::uint8_t, 3> organization_id = {};        ///< OUI
    std::array<std::uint8_t, 3> organization_sub_type = {};  ///< Sub-type
    std::int32_t cumulative_scaled_rate_offset = 0;          ///< Cumulative scaled rate offset
    std::uint16_t gm_time_base_indicator = 0;                ///< GM time base indicator
    std::uint32_t last_gm_phase_change_ns_msb = 0;           ///< Last GM phase change (MSB)
    std::uint64_t last_gm_phase_change_ns_lsb = 0;           ///< Last GM phase change (LSB)
    std::int32_t scaled_last_gm_freq_change = 0;             ///< Scaled last GM freq change

    /// @brief Parse from organization extension TLV value
    static std::optional<FollowUpTlv> parse(const std::vector<std::byte>& value);
};

/// @brief Path Trace TLV
struct PathTraceTlv {
    std::vector<ClockIdentity> path_sequence;  ///< List of clocks in path

    /// @brief Parse from TLV value
    static std::optional<PathTraceTlv> parse(const std::vector<std::byte>& value);
};

/// @brief Sync message (event message, needs timestamp)
///
/// Sync messages are sent by the grandmaster to synchronize time-aware systems.
/// In gPTP (802.1AS), Sync is always used with two-step mode (Follow_Up).
struct SyncMessage {
    /// @brief Reserved field (10 bytes, must be zero in gPTP)
    std::array<std::byte, 10> reserved = {};

    /// @brief Origin timestamp (in event messages, filled by hardware)
    /// Note: In two-step mode, this is 0 and the actual timestamp is in Follow_Up
    GptpTimestamp origin_timestamp;

    /// @brief Message size for Sync (44 bytes total: 34 header + 10 body)
    static constexpr std::size_t MESSAGE_SIZE = 44;
    static constexpr std::size_t BODY_SIZE = 10;  ///< Body after common header

    /// @brief Parse Sync message body from bytes
    static std::optional<SyncMessage> parse(const std::byte* ptr, std::size_t len);
};

/// @brief Follow_Up message
///
/// Sent after Sync to provide the precise origin timestamp.
/// Contains TLVs with additional synchronization information.
struct FollowUpMessage {
    /// @brief Precise origin timestamp (when Sync was actually sent)
    GptpTimestamp precise_origin_timestamp;

    /// @brief Optional Follow_Up information TLV (802.1AS specific)
    std::optional<FollowUpTlv> follow_up_info;

    /// @brief Other TLVs attached to this message
    std::vector<Tlv> tlvs;

    /// @brief Minimum message size (34 header + 10 timestamp)
    static constexpr std::size_t MIN_MESSAGE_SIZE = 44;
    static constexpr std::size_t BODY_SIZE = 10;  ///< Body before TLVs

    /// @brief Parse Follow_Up message from bytes
    static std::optional<FollowUpMessage> parse(const std::byte* ptr, std::size_t len);
};

/// @brief Pdelay_Req message (peer delay request)
///
/// Sent by a time-aware system to measure the peer delay to its neighbor.
/// Part of the peer-to-peer delay measurement mechanism in gPTP.
struct PdelayReqMessage {
    /// @brief Reserved/origin timestamp (10 bytes)
    GptpTimestamp origin_timestamp;

    /// @brief Reserved (10 bytes for future use)
    std::array<std::byte, 10> reserved = {};

    /// @brief Message size for Pdelay_Req (54 bytes total)
    static constexpr std::size_t MESSAGE_SIZE = 54;
    static constexpr std::size_t BODY_SIZE = 20;  ///< Body after common header

    /// @brief Parse Pdelay_Req message from bytes
    static std::optional<PdelayReqMessage> parse(const std::byte* ptr, std::size_t len);
};

/// @brief Pdelay_Resp message (peer delay response)
///
/// Response to Pdelay_Req, contains requestReceiptTimestamp.
struct PdelayRespMessage {
    /// @brief Timestamp when Pdelay_Req was received
    GptpTimestamp request_receipt_timestamp;

    /// @brief Port identity of the requesting port
    PortIdentity requesting_port_identity;

    /// @brief Message size for Pdelay_Resp (54 bytes total)
    static constexpr std::size_t MESSAGE_SIZE = 54;
    static constexpr std::size_t BODY_SIZE = 20;  ///< Body after common header

    /// @brief Parse Pdelay_Resp message from bytes
    static std::optional<PdelayRespMessage> parse(const std::byte* ptr, std::size_t len);
};

/// @brief Pdelay_Resp_Follow_Up message
///
/// Provides precise responseOriginTimestamp for peer delay measurement.
struct PdelayRespFollowUpMessage {
    /// @brief Precise timestamp when Pdelay_Resp was sent
    GptpTimestamp response_origin_timestamp;

    /// @brief Port identity of the requesting port
    PortIdentity requesting_port_identity;

    /// @brief Message size (54 bytes total)
    static constexpr std::size_t MESSAGE_SIZE = 54;
    static constexpr std::size_t BODY_SIZE = 20;

    /// @brief Parse message from bytes
    static std::optional<PdelayRespFollowUpMessage> parse(const std::byte* ptr, std::size_t len);
};

/// @brief Announce message
///
/// Sent by grandmaster-capable clocks to announce their properties.
/// Used in BMCA (Best Master Clock Algorithm) to elect the grandmaster.
struct AnnounceMessage {
    /// @brief Origin timestamp (10 bytes, reserved in gPTP)
    GptpTimestamp origin_timestamp;

    /// @brief Current UTC offset (seconds)
    std::int16_t current_utc_offset = 0;

    /// @brief Reserved byte
    std::uint8_t reserved = 0;

    /// @brief Grandmaster priority1 (lower is better, default 248)
    std::uint8_t grandmaster_priority1 = 255;

    /// @brief Grandmaster clock quality
    ClockQuality grandmaster_clock_quality;

    /// @brief Grandmaster priority2 (tiebreaker, lower is better)
    std::uint8_t grandmaster_priority2 = 255;

    /// @brief Grandmaster identity
    ClockIdentity grandmaster_identity;

    /// @brief Steps removed (distance from grandmaster in hops)
    std::uint16_t steps_removed = 0;

    /// @brief Time source (enumeration)
    TimeSource time_source = TimeSource::InternalOscillator;

    /// @brief Path trace TLV (list of clocks in path)
    std::optional<PathTraceTlv> path_trace;

    /// @brief Other TLVs
    std::vector<Tlv> tlvs;

    /// @brief Minimum message size
    static constexpr std::size_t MIN_MESSAGE_SIZE = 64;
    static constexpr std::size_t BODY_SIZE = 30;

    /// @brief Parse Announce message from bytes
    static std::optional<AnnounceMessage> parse(const std::byte* ptr, std::size_t len);

    /// @brief Time source descriptions
    static std::string_view time_source_string(std::uint8_t source);
};

/// @brief Signaling message
///
/// Used for signaling between time-aware systems (e.g., message interval requests).
struct SignalingMessage {
    /// @brief Target port identity
    PortIdentity target_port_identity;

    /// @brief Attached TLVs
    std::vector<Tlv> tlvs;

    /// @brief Minimum message size
    static constexpr std::size_t MIN_MESSAGE_SIZE = 44;
    static constexpr std::size_t BODY_SIZE = 10;

    /// @brief Parse Signaling message from bytes
    static std::optional<SignalingMessage> parse(const std::byte* ptr, std::size_t len);
};

/// @brief Variant type for all gPTP message bodies
using MessageBody = std::variant<SyncMessage, FollowUpMessage, PdelayReqMessage, PdelayRespMessage,
                                 PdelayRespFollowUpMessage, AnnounceMessage, SignalingMessage>;

/// @brief Variant type for polymorphic TLV handling
///
/// Allows type-safe processing of different TLV types using std::visit().
/// Supports the main TLV types (FollowUpTlv, PathTraceTlv) and generic Tlv for unknown types.
using GptpTlv = std::variant<FollowUpTlv, PathTraceTlv, Tlv>;

/// @brief Get the expected body size for a message type
[[nodiscard]] constexpr std::size_t expected_body_size(MessageType type) {
    switch (type) {
        case MessageType::Sync:
            return SyncMessage::BODY_SIZE;
        case MessageType::Follow_Up:
            return FollowUpMessage::BODY_SIZE;
        case MessageType::Pdelay_Req:
            return PdelayReqMessage::BODY_SIZE;
        case MessageType::Pdelay_Resp:
            return PdelayRespMessage::BODY_SIZE;
        case MessageType::Pdelay_Resp_Follow_Up:
            return PdelayRespFollowUpMessage::BODY_SIZE;
        case MessageType::Announce:
            return AnnounceMessage::BODY_SIZE;
        case MessageType::Signaling:
            return SignalingMessage::BODY_SIZE;
        default:
            return 0;
    }
}

}  // namespace wadjet::protocols::gptp

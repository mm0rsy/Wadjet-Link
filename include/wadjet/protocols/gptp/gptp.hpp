#pragma once

/// @file gptp.hpp
/// @brief gPTP (IEEE 802.1AS) protocol decoder
/// 
/// This file provides the main gPTP header structure and decoder implementation
/// for Generalized Precision Time Protocol used in automotive time synchronization.
/// 
/// @gptp gPTP is the profile of IEEE 1588 (PTP) specified by IEEE 802.1AS for
/// use in time-sensitive networks. It provides sub-microsecond synchronization
/// across Ethernet networks.
/// 
/// ## Protocol Overview
/// 
/// gPTP uses the following message types:
/// - **Sync / Follow_Up**: Time synchronization from grandmaster
/// - **Pdelay_Req / Pdelay_Resp / Pdelay_Resp_Follow_Up**: Peer delay measurement
/// - **Announce**: Grandmaster election (BMCA)
/// 
/// ## Example Usage
/// 
/// ```cpp
/// #include <wadjet/protocols/gptp/gptp.hpp>
/// 
/// using namespace wadjet::protocols::gptp;
/// 
/// // Decode a gPTP packet
/// GptpDecoder decoder;
/// auto result = decoder.decode(packet_data);
/// 
/// if (result) {
///     const auto& header = result.value().first;
///     std::cout << "Message type: " << message_type_string(header.message_type) << "\n";
///     std::cout << "Source: " << header.source_port_identity.to_string() << "\n";
/// }
/// ```

#include "wadjet/protocols/gptp/gptp_types.hpp"
#include "wadjet/protocols/gptp/gptp_messages.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <optional>
#include <span>

namespace wadjet::protocols::gptp {

/// @brief Decoded gPTP header (common header for all message types)
/// 
/// The gPTP common header is 34 bytes and contains fields common to all
/// gPTP message types.
struct GptpHeader : public IDecodedHeader {
    // Byte 0: Transport specific (4 bits) + Message type (4 bits)
    TransportSpecific transport_specific = TransportSpecific::IEEE_802_1AS;
    MessageType message_type = MessageType::Sync;

    // Byte 1: Reserved (4 bits) + Version (4 bits)
    std::uint8_t version_ptp = GPTP_VERSION_MAJOR;
    std::uint8_t minor_version_ptp = 0;

    // Bytes 2-3: Message length (total including header)
    std::uint16_t message_length = 0;

    // Byte 4: Domain number
    std::uint8_t domain_number = 0;

    // Byte 5: Minor SdoId / Reserved
    std::uint8_t minor_sdo_id = 0;

    // Bytes 6-7: Flags
    GptpFlags flags;

    // Bytes 8-15: Correction field (scaled nanoseconds)
    ScaledNanoseconds correction_field;

    // Bytes 16-19: Message type specific (4 bytes)
    std::uint32_t message_type_specific = 0;

    // Bytes 20-29: Source port identity (10 bytes)
    PortIdentity source_port_identity;

    // Bytes 30-31: Sequence ID
    std::uint16_t sequence_id = 0;

    // Byte 32: Control field (deprecated, for compatibility)
    std::uint8_t control_field = 0;

    // Byte 33: Log message interval
    LogInterval log_message_interval;

    // Parsed message body (type depends on message_type)
    std::optional<MessageBody> body;

    // IDecodedHeader interface implementation
    [[nodiscard]] std::string_view protocol_name() const override {
        return "gPTP";
    }

    [[nodiscard]] std::size_t header_size() const override {
        return COMMON_HEADER_SIZE;
    }

    [[nodiscard]] std::size_t payload_size() const override {
        return message_length > COMMON_HEADER_SIZE ? 
               message_length - COMMON_HEADER_SIZE : 0;
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Check if this is a two-step message (Follow_Up expected)
    [[nodiscard]] bool is_two_step() const {
        return flags.two_step;
    }

    /// @brief Check if this is an event message (timestamped)
    [[nodiscard]] bool is_event() const {
        return is_event_message(message_type);
    }

    /// @brief Check if this message is from 802.1AS profile
    [[nodiscard]] bool is_gptp_profile() const {
        return transport_specific == TransportSpecific::IEEE_802_1AS;
    }

    /// @brief Get message type as a string
    [[nodiscard]] std::string_view message_type_name() const {
        return message_type_string(message_type);
    }

    // Helper accessors for typed message bodies
    
    /// @brief Get Sync message body (if applicable)
    [[nodiscard]] const SyncMessage* as_sync() const {
        if (body && std::holds_alternative<SyncMessage>(*body)) {
            return &std::get<SyncMessage>(*body);
        }
        return nullptr;
    }

    /// @brief Get Follow_Up message body (if applicable)
    [[nodiscard]] const FollowUpMessage* as_follow_up() const {
        if (body && std::holds_alternative<FollowUpMessage>(*body)) {
            return &std::get<FollowUpMessage>(*body);
        }
        return nullptr;
    }

    /// @brief Get Pdelay_Req message body (if applicable)
    [[nodiscard]] const PdelayReqMessage* as_pdelay_req() const {
        if (body && std::holds_alternative<PdelayReqMessage>(*body)) {
            return &std::get<PdelayReqMessage>(*body);
        }
        return nullptr;
    }

    /// @brief Get Pdelay_Resp message body (if applicable)
    [[nodiscard]] const PdelayRespMessage* as_pdelay_resp() const {
        if (body && std::holds_alternative<PdelayRespMessage>(*body)) {
            return &std::get<PdelayRespMessage>(*body);
        }
        return nullptr;
    }

    /// @brief Get Pdelay_Resp_Follow_Up message body (if applicable)
    [[nodiscard]] const PdelayRespFollowUpMessage* as_pdelay_resp_follow_up() const {
        if (body && std::holds_alternative<PdelayRespFollowUpMessage>(*body)) {
            return &std::get<PdelayRespFollowUpMessage>(*body);
        }
        return nullptr;
    }

    /// @brief Get Announce message body (if applicable)
    [[nodiscard]] const AnnounceMessage* as_announce() const {
        if (body && std::holds_alternative<AnnounceMessage>(*body)) {
            return &std::get<AnnounceMessage>(*body);
        }
        return nullptr;
    }

    /// @brief Get Signaling message body (if applicable)
    [[nodiscard]] const SignalingMessage* as_signaling() const {
        if (body && std::holds_alternative<SignalingMessage>(*body)) {
            return &std::get<SignalingMessage>(*body);
        }
        return nullptr;
    }
};

/// @brief gPTP decoder options
struct GptpDecoderOptions {
    bool parse_body = true;           ///< Parse message body (not just header)
    bool parse_tlvs = true;           ///< Parse TLVs in messages
    bool strict_version = false;      ///< Reject non-802.1AS messages
    bool validate_length = true;      ///< Validate message length field
};

/// @brief gPTP protocol decoder
/// 
/// Decodes gPTP (IEEE 802.1AS) messages from raw Ethernet frames.
/// 
/// @example
/// ```cpp
/// GptpDecoder decoder;
/// auto result = decoder.decode(context);
/// if (result) {
///     // Access the header
///     std::cout << result->to_string() << "\n";
/// }
/// ```
class GptpDecoder : public DecoderBase<GptpDecoder, GptpHeader> {
public:
    /// @brief Decoder name
    static constexpr std::string_view NAME = "gPTP";

    explicit GptpDecoder(GptpDecoderOptions opts = GptpDecoderOptions{})
        : options_(opts) {}

    /// @brief Get decoder name
    [[nodiscard]] std::string_view name() const override { return NAME; }

    /// @brief Check if this decoder can handle the data
    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        return ctx.has_bytes(COMMON_HEADER_SIZE);
    }

    /// @brief Decode gPTP message from context
    /// @param ctx Decode context with packet data
    /// @return Decoded header result
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

    /// @brief Get decoder options
    [[nodiscard]] const GptpDecoderOptions& options() const { return options_; }

    /// @brief Set decoder options
    void set_options(const GptpDecoderOptions& opts) { options_ = opts; }

private:
    /// @brief Parse common header (34 bytes)
    [[nodiscard]] std::optional<GptpHeader> 
    parse_header(const std::byte* data, std::size_t len) const;

    /// @brief Parse message body based on type
    [[nodiscard]] std::optional<MessageBody>
    parse_body(MessageType type, const std::byte* data, std::size_t len) const;

    GptpDecoderOptions options_;
};

/// @brief Utility: Check if packet is likely gPTP based on EtherType
[[nodiscard]] inline bool is_gptp_ethertype(std::uint16_t ethertype) {
    return ethertype == GPTP_ETHERTYPE;
}

/// @brief Utility: Check if MAC address is gPTP multicast
[[nodiscard]] inline bool is_gptp_multicast(const MacAddress& mac) {
    return std::memcmp(mac.bytes.data(), GPTP_MULTICAST_MAC.data(), 6) == 0;
}

/// @brief Calculate peer delay from Pdelay message exchange
/// 
/// @param t1 Pdelay_Req transmit timestamp (requestOriginTimestamp)
/// @param t2 Pdelay_Req receive timestamp (requestReceiptTimestamp)
/// @param t3 Pdelay_Resp transmit timestamp (responseOriginTimestamp)
/// @param t4 Pdelay_Resp receive timestamp (measured at requester)
/// @return Calculated peer delay in nanoseconds
[[nodiscard]] inline std::int64_t calculate_peer_delay(
    const GptpTimestamp& t1, const GptpTimestamp& t2,
    const GptpTimestamp& t3, const GptpTimestamp& t4) {
    // peerDelay = [(t4 - t1) - (t3 - t2)] / 2
    // = [(t4 - t1) - responseTime] / 2
    // = [roundTripDelay - turnaroundTime] / 2
    auto round_trip = t4 - t1;
    auto turnaround = t3 - t2;
    return (round_trip - turnaround) / 2;
}

/// @brief Calculate rate ratio from Follow_Up TLV
/// 
/// @param cumulative_scaled_rate_offset From Follow_Up TLV
/// @return Rate ratio as a double (1.0 = no offset)
[[nodiscard]] inline double calculate_rate_ratio(
    std::int32_t cumulative_scaled_rate_offset) {
    // rateRatio = 1 + (cumulativeScaledRateOffset / 2^41)
    return 1.0 + static_cast<double>(cumulative_scaled_rate_offset) / 
                 static_cast<double>(1ULL << 41);
}

}  // namespace wadjet::protocols::gptp

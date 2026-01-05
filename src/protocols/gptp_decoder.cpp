/// @file gptp_decoder.cpp
/// @brief gPTP (IEEE 802.1AS) protocol decoder implementation

#include "wadjet/protocols/gptp/gptp.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace wadjet::protocols::gptp {

namespace {

/// @brief Read big-endian uint16 from buffer
[[nodiscard]] inline std::uint16_t read_be16(const std::byte* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                      static_cast<std::uint16_t>(data[1]));
}

/// @brief Read big-endian uint32 from buffer
[[nodiscard]] inline std::uint32_t read_be32(const std::byte* data) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) |
        (static_cast<std::uint32_t>(data[2]) << 8) | static_cast<std::uint32_t>(data[3]));
}

/// @brief Read big-endian uint64 from buffer
[[nodiscard]] inline std::uint64_t read_be64(const std::byte* data) {
    return static_cast<std::uint64_t>(
        (static_cast<std::uint64_t>(data[0]) << 56) | (static_cast<std::uint64_t>(data[1]) << 48) |
        (static_cast<std::uint64_t>(data[2]) << 40) | (static_cast<std::uint64_t>(data[3]) << 32) |
        (static_cast<std::uint64_t>(data[4]) << 24) | (static_cast<std::uint64_t>(data[5]) << 16) |
        (static_cast<std::uint64_t>(data[6]) << 8) | static_cast<std::uint64_t>(data[7]));
}

/// @brief Read big-endian int64 from buffer (for correction field)
[[nodiscard]] inline std::int64_t read_be64_signed(const std::byte* data) {
    return static_cast<std::int64_t>(read_be64(data));
}

/// @brief Read big-endian int32 from buffer
[[nodiscard]] inline std::int32_t read_be32_signed(const std::byte* data) {
    return static_cast<std::int32_t>(read_be32(data));
}

/// @brief Read big-endian int16 from buffer
[[nodiscard]] inline std::int16_t read_be16_signed(const std::byte* data) {
    return static_cast<std::int16_t>(read_be16(data));
}

/// @brief Read ClockIdentity (8 bytes)
[[nodiscard]] inline ClockIdentity read_clock_identity(const std::byte* data) {
    return ClockIdentity::from_bytes(data);
}

/// @brief Read PortIdentity (10 bytes: 8 clock + 2 port)
[[nodiscard]] inline PortIdentity read_port_identity(const std::byte* data) {
    PortIdentity id;
    id.clock_identity = read_clock_identity(data);
    id.port_number = read_be16(data + 8);
    return id;
}

/// @brief Read GptpTimestamp (10 bytes: 6 seconds + 4 nanoseconds)
[[nodiscard]] inline GptpTimestamp read_timestamp(const std::byte* data) {
    GptpTimestamp ts;
    // 48-bit seconds (big-endian, high 2 bytes first)
    ts.seconds_msb = read_be16(data);
    ts.seconds_lsb = read_be32(data + 2);
    ts.nanoseconds = read_be32(data + 6);
    return ts;
}

}  // anonymous namespace

// GptpHeader::to_string implementation
std::string GptpHeader::to_string() const {
    std::ostringstream oss;
    oss << "gPTP " << message_type_name() << " ";
    oss << "seq=" << sequence_id << " ";
    oss << "src=" << source_port_identity.to_string() << " ";
    oss << "dom=" << static_cast<int>(domain_number);

    if (flags.two_step) {
        oss << " [two-step]";
    }

    if (correction_field.scaled_ns != 0) {
        oss << " corr=" << correction_field.to_nanoseconds() << "ns";
    }

    return oss.str();
}

// GptpDecoder implementation
GptpDecoder::Result GptpDecoder::decode_impl(const DecodeContext& ctx) const {
    if (ctx.data.size() < COMMON_HEADER_SIZE) {
        return make_error(DecodeErrorCode::BufferTooSmall, "Insufficient data for gPTP header",
                          ctx.original_offset);
    }

    auto header = parse_header(ctx.data.data(), ctx.data.size());
    if (!header) {
        return make_error(DecodeErrorCode::InvalidHeader, "Failed to parse gPTP header",
                          ctx.original_offset);
    }

    // Validate version if strict mode
    if (options_.strict_version && header->transport_specific != TransportSpecific::IEEE_802_1AS) {
        return make_error(DecodeErrorCode::InvalidVersion, "Not an IEEE 802.1AS gPTP message",
                          ctx.original_offset);
    }

    // Validate message length
    if (options_.validate_length) {
        if (header->message_length < COMMON_HEADER_SIZE ||
            header->message_length > ctx.data.size()) {
            return make_error(DecodeErrorCode::InvalidLength, "Invalid gPTP message length",
                              ctx.original_offset);
        }
    }

    // Parse message body
    if (options_.parse_body && ctx.data.size() > COMMON_HEADER_SIZE) {
        const auto* body_data = ctx.data.data() + COMMON_HEADER_SIZE;
        const auto body_len = ctx.data.size() - COMMON_HEADER_SIZE;
        header->body = parse_body(header->message_type, body_data, body_len);
    }

    // gPTP is typically the final layer (no payload encapsulation)
    DecodeContext next_ctx = ctx.sub_context(header->message_length);

    return make_success(std::move(*header), std::move(next_ctx));
}

std::optional<GptpHeader> GptpDecoder::parse_header(const std::byte* data, std::size_t len) const {
    if (len < COMMON_HEADER_SIZE) {
        return std::nullopt;
    }

    GptpHeader header;

    // Byte 0: Transport specific (high 4 bits) + Message type (low 4 bits)
    const auto byte0 = static_cast<std::uint8_t>(data[0]);
    header.transport_specific = static_cast<TransportSpecific>((byte0 >> 4) & 0x0F);
    header.message_type = static_cast<MessageType>(byte0 & 0x0F);

    // Byte 1: Reserved (high 4 bits) + Version (low 4 bits)
    const auto byte1 = static_cast<std::uint8_t>(data[1]);
    header.version_ptp = byte1 & 0x0F;
    header.minor_version_ptp = (byte1 >> 4) & 0x0F;

    // Bytes 2-3: Message length
    header.message_length = read_be16(data + 2);

    // Byte 4: Domain number
    header.domain_number = static_cast<std::uint8_t>(data[4]);

    // Byte 5: Minor SdoId
    header.minor_sdo_id = static_cast<std::uint8_t>(data[5]);

    // Bytes 6-7: Flags
    const auto flags_raw = read_be16(data + 6);
    header.flags = GptpFlags::from_raw(flags_raw);

    // Bytes 8-15: Correction field
    header.correction_field.scaled_ns = read_be64_signed(data + 8);

    // Bytes 16-19: Message type specific
    header.message_type_specific = read_be32(data + 16);

    // Bytes 20-29: Source port identity
    header.source_port_identity = read_port_identity(data + 20);

    // Bytes 30-31: Sequence ID
    header.sequence_id = read_be16(data + 30);

    // Byte 32: Control field
    header.control_field = static_cast<std::uint8_t>(data[32]);

    // Byte 33: Log message interval
    header.log_message_interval.value = static_cast<std::int8_t>(data[33]);

    return header;
}

std::optional<MessageBody> GptpDecoder::parse_body(MessageType type, const std::byte* data,
                                                   std::size_t len) const {
    switch (type) {
        case MessageType::Sync: {
            if (len < SYNC_MESSAGE_SIZE) {
                return std::nullopt;
            }
            SyncMessage sync;
            sync.origin_timestamp = read_timestamp(data);
            return sync;
        }

        case MessageType::Follow_Up: {
            if (len < FOLLOW_UP_MESSAGE_SIZE) {
                return std::nullopt;
            }
            FollowUpMessage follow_up;
            follow_up.precise_origin_timestamp = read_timestamp(data);
            // TLV parsing can be added later if needed
            return follow_up;
        }

        case MessageType::Pdelay_Req: {
            if (len < PDELAY_REQ_MESSAGE_SIZE) {
                return std::nullopt;
            }
            PdelayReqMessage pdelay_req;
            pdelay_req.origin_timestamp = read_timestamp(data);
            // Reserved bytes follow
            return pdelay_req;
        }

        case MessageType::Pdelay_Resp: {
            if (len < PDELAY_RESP_MESSAGE_SIZE) {
                return std::nullopt;
            }
            PdelayRespMessage pdelay_resp;
            pdelay_resp.request_receipt_timestamp = read_timestamp(data);
            pdelay_resp.requesting_port_identity = read_port_identity(data + 10);
            return pdelay_resp;
        }

        case MessageType::Pdelay_Resp_Follow_Up: {
            if (len < PDELAY_RESP_FOLLOW_UP_MESSAGE_SIZE) {
                return std::nullopt;
            }
            PdelayRespFollowUpMessage pdelay_resp_fu;
            pdelay_resp_fu.response_origin_timestamp = read_timestamp(data);
            pdelay_resp_fu.requesting_port_identity = read_port_identity(data + 10);
            return pdelay_resp_fu;
        }

        case MessageType::Announce: {
            if (len < ANNOUNCE_MESSAGE_SIZE) {
                return std::nullopt;
            }
            AnnounceMessage announce;
            announce.origin_timestamp = read_timestamp(data);
            announce.current_utc_offset = read_be16_signed(data + 10);
            // Byte 12 is reserved
            announce.grandmaster_priority1 = static_cast<std::uint8_t>(data[13]);

            // Clock quality (4 bytes)
            announce.grandmaster_clock_quality.clock_class = static_cast<std::uint8_t>(data[14]);
            announce.grandmaster_clock_quality.clock_accuracy = static_cast<std::uint8_t>(data[15]);
            announce.grandmaster_clock_quality.offset_scaled_log_variance = read_be16(data + 16);

            announce.grandmaster_priority2 = static_cast<std::uint8_t>(data[18]);
            announce.grandmaster_identity = read_clock_identity(data + 19);
            announce.steps_removed = read_be16(data + 27);
            announce.time_source = static_cast<TimeSource>(data[29]);
            return announce;
        }

        case MessageType::Signaling: {
            if (len < SIGNALING_MESSAGE_SIZE) {
                return std::nullopt;
            }
            SignalingMessage signaling;
            signaling.target_port_identity = read_port_identity(data);
            return signaling;
        }

        case MessageType::Management:
            // Management messages are not commonly used in automotive gPTP
            // Could be implemented if needed
            return std::nullopt;

        default:
            return std::nullopt;
    }
}

}  // namespace wadjet::protocols::gptp

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
[[nodiscard, maybe_unused]] inline std::int32_t read_be32_signed(const std::byte* data) {
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

/// @brief Parse TLV array with unknown TLV handling
/// @param ptr Pointer to TLV data
/// @param remaining Bytes remaining in buffer
/// @return Vector of parsed TLVs
static std::vector<Tlv> parse_tlv_array(const std::byte* ptr, std::size_t remaining) {
    std::vector<Tlv> tlvs;

    while (remaining >= TLV_HEADER_SIZE) {
        auto result = Tlv::parse(ptr, remaining);

        if (!result) {
            // Incomplete TLV - log warning and stop
            // Could add logging here if needed
            break;
        }

        auto [tlv, bytes_consumed] = result.value();

        // Handle unknown TLV types with graceful fallback
        // Unknown TLVs are still stored but their specific meaning is not interpreted
        tlvs.push_back(tlv);

        // Move to next TLV
        ptr += bytes_consumed;
        remaining -= bytes_consumed;
    }

    return tlvs;
}

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

            // Parse TLVs after the timestamp (10 bytes body + any additional TLVs)
            if (len > FOLLOW_UP_MESSAGE_SIZE) {
                const std::byte* tlv_data = data + FOLLOW_UP_MESSAGE_SIZE;
                std::size_t tlv_remaining = len - FOLLOW_UP_MESSAGE_SIZE;

                // Parse TLV array
                auto tlvs = parse_tlv_array(tlv_data, tlv_remaining);

                // Extract Follow_Up info TLV if present
                for (const auto& tlv : tlvs) {
                    if (tlv.type == TlvType::ORGANIZATION_EXTENSION) {
                        if (auto fu_tlv = FollowUpTlv::parse(tlv.value)) {
                            follow_up.follow_up_info = fu_tlv.value();
                        }
                    } else if (tlv.type != TlvType::Management && 
                               tlv.type != TlvType::ManagementErrorStatus) {
                        // Store other TLVs
                        follow_up.tlvs.push_back(tlv);
                    }
                }
            }

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

            // Parse TLVs after the body (30 bytes body + any additional TLVs)
            if (len > ANNOUNCE_MESSAGE_SIZE) {
                const std::byte* tlv_data = data + ANNOUNCE_MESSAGE_SIZE;
                std::size_t tlv_remaining = len - ANNOUNCE_MESSAGE_SIZE;

                // Parse TLV array
                auto tlvs = parse_tlv_array(tlv_data, tlv_remaining);

                // Extract Path Trace TLV if present
                for (const auto& tlv : tlvs) {
                    if (tlv.type == TlvType::PATH_TRACE) {
                        if (auto path_trace = PathTraceTlv::parse(tlv.value)) {
                            announce.path_trace = path_trace.value();
                        }
                    } else {
                        // Store other TLVs
                        announce.tlvs.push_back(tlv);
                    }
                }
            }

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

// Tlv::parse implementation
std::optional<std::pair<Tlv, std::size_t>> Tlv::parse(const std::byte* ptr,
                                                       std::size_t remaining) {
    if (remaining < TLV_HEADER_SIZE) {
        return std::nullopt;
    }

    Tlv parsed_tlv;

    // Parse Type (2 bytes, big-endian)
    std::uint16_t type_value = read_be16(ptr);
    parsed_tlv.type = static_cast<TlvType>(type_value);

    // Parse Length (2 bytes, big-endian)
    parsed_tlv.length = read_be16(ptr + 2);

    // Validate length doesn't exceed remaining buffer
    std::size_t total_size = TLV_HEADER_SIZE + parsed_tlv.length;
    if (total_size > remaining) {
        return std::nullopt;
    }

    // Extract value
    if (parsed_tlv.length > 0) {
        const std::byte* value_ptr = ptr + TLV_HEADER_SIZE;
        parsed_tlv.value.assign(value_ptr, value_ptr + parsed_tlv.length);
    }

    return std::make_pair(parsed_tlv, total_size);
}

// FollowUpTlv::parse implementation
std::optional<FollowUpTlv> FollowUpTlv::parse(const std::vector<std::byte>& value) {
    // Follow_Up TLV value format:
    // - OUI (3 bytes): 00:80:C2
    // - Sub-type (3 bytes): usually 01
    // - Cumulative Scaled Rate Offset (4 bytes, big-endian signed)
    // - GM Time Base Indicator (2 bytes, big-endian)
    // - Last GM Phase Change (10 bytes: 2 MSB + 8 LSB, big-endian)
    // - Scaled Last GM Frequency Change (4 bytes, big-endian signed)
    // Total: 3 + 3 + 4 + 2 + 10 + 4 = 26 bytes minimum

    constexpr std::size_t MIN_SIZE = 26;
    if (value.size() < MIN_SIZE) {
        return std::nullopt;
    }

    FollowUpTlv follow_up;

    // Parse OUI (3 bytes)
    for (std::size_t i = 0; i < 3; ++i) {
        follow_up.organization_id[i] = static_cast<std::uint8_t>(value[i]);
    }

    // Parse Sub-type (3 bytes)
    for (std::size_t i = 0; i < 3; ++i) {
        follow_up.organization_sub_type[i] = static_cast<std::uint8_t>(value[3 + i]);
    }

    // Parse Cumulative Scaled Rate Offset (4 bytes, big-endian signed)
    follow_up.cumulative_scaled_rate_offset = read_be32_signed(value.data() + 6);

    // Parse GM Time Base Indicator (2 bytes)
    follow_up.gm_time_base_indicator = read_be16(value.data() + 10);

    // Parse Last GM Phase Change (10 bytes: 2 MSB + 8 LSB)
    follow_up.last_gm_phase_change_ns_msb = read_be32(value.data() + 12);
    follow_up.last_gm_phase_change_ns_lsb = read_be64(value.data() + 16);

    // Parse Scaled Last GM Frequency Change (4 bytes, big-endian signed)
    follow_up.scaled_last_gm_freq_change = read_be32_signed(value.data() + 24);

    return follow_up;
}

// PathTraceTlv::parse implementation
std::optional<PathTraceTlv> PathTraceTlv::parse(const std::vector<std::byte>& value) {
    // Path Trace TLV contains a sequence of Clock Identities
    // Each Clock Identity is 8 bytes
    // Minimum: 1 clock identity = 8 bytes

    constexpr std::size_t CLOCK_ID_SIZE = 8;

    if (value.size() < CLOCK_ID_SIZE || value.size() % CLOCK_ID_SIZE != 0) {
        return std::nullopt;
    }

    PathTraceTlv path_trace;

    // Parse clock identities
    for (std::size_t i = 0; i < value.size(); i += CLOCK_ID_SIZE) {
        ClockIdentity id = read_clock_identity(value.data() + i);
        path_trace.path_sequence.push_back(id);
    }

    return path_trace;
}

}  // namespace wadjet::protocols::gptp

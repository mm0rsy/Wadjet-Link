/// @file rtps_decoder.cpp
/// @brief RTPS (Real-Time Publish-Subscribe) protocol decoder implementation
///
/// This file implements the RTPS wire protocol decoder for DDS middleware.
/// RTPS is the wire protocol used by DDS implementations like:
/// - Fast DDS (eProsima)
/// - RTI Connext
/// - Cyclone DDS
/// - OpenDDS

#include "wadjet/protocols/dds/discovery.hpp"
#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/dds/rtps_messages.hpp"

#include <algorithm>
#include <cstring>

namespace wadjet::protocols::dds {

namespace {

// =============================================================================
// Byte Order Utilities
// =============================================================================

/// @brief Read little-endian uint16 from buffer
[[nodiscard]] inline std::uint16_t read_le16(const std::byte* data) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8));
}

/// @brief Read big-endian uint16 from buffer
[[nodiscard]] inline std::uint16_t read_be16(const std::byte* data) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8) |
        static_cast<std::uint16_t>(data[1]));
}

/// @brief Read uint16 with endianness
[[nodiscard]] inline std::uint16_t read_u16(const std::byte* data, bool little_endian) {
    return little_endian ? read_le16(data) : read_be16(data);
}

/// @brief Read little-endian uint32 from buffer
[[nodiscard]] inline std::uint32_t read_le32(const std::byte* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

/// @brief Read big-endian uint32 from buffer
[[nodiscard]] inline std::uint32_t read_be32(const std::byte* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

/// @brief Read uint32 with endianness
[[nodiscard]] inline std::uint32_t read_u32(const std::byte* data, bool little_endian) {
    return little_endian ? read_le32(data) : read_be32(data);
}

/// @brief Read int32 with endianness
[[nodiscard]] inline std::int32_t read_i32(const std::byte* data, bool little_endian) {
    return static_cast<std::int32_t>(read_u32(data, little_endian));
}

/// @brief Read little-endian uint64 from buffer
[[nodiscard]] inline std::uint64_t read_le64(const std::byte* data) {
    return static_cast<std::uint64_t>(read_le32(data)) |
           (static_cast<std::uint64_t>(read_le32(data + 4)) << 32);
}

/// @brief Read big-endian uint64 from buffer
[[nodiscard]] inline std::uint64_t read_be64(const std::byte* data) {
    return (static_cast<std::uint64_t>(read_be32(data)) << 32) |
           static_cast<std::uint64_t>(read_be32(data + 4));
}

/// @brief Read uint64 with endianness
[[nodiscard]] inline std::uint64_t read_u64(const std::byte* data, bool little_endian) {
    return little_endian ? read_le64(data) : read_be64(data);
}

/// @brief Read GuidPrefix (12 bytes)
[[nodiscard]] inline GuidPrefix read_guid_prefix(const std::byte* data) {
    GuidPrefix prefix;
    std::memcpy(prefix.value.data(), data, GUID_PREFIX_SIZE);
    return prefix;
}

/// @brief Read EntityId (4 bytes)
[[nodiscard]] inline EntityId read_entity_id(const std::byte* data) {
    EntityId id;
    std::memcpy(id.entity_key.data(), data, 3);
    id.kind = static_cast<EntityKind>(data[3]);
    return id;
}

/// @brief Read GUID (16 bytes)
[[nodiscard]] inline GUID read_guid(const std::byte* data) {
    GUID guid;
    guid.prefix = read_guid_prefix(data);
    guid.entity_id = read_entity_id(data + GUID_PREFIX_SIZE);
    return guid;
}

/// @brief Read SequenceNumber (8 bytes)
[[nodiscard]] inline SequenceNumber read_sequence_number(const std::byte* data, bool little_endian) {
    SequenceNumber sn;
    sn.high = read_i32(data, little_endian);
    sn.low = read_u32(data + 4, little_endian);
    return sn;
}

/// @brief Read SequenceNumberSet
[[nodiscard]] inline SequenceNumberSet read_sequence_number_set(const std::byte* data,
                                                                std::size_t available_len,
                                                                bool little_endian) {
    SequenceNumberSet set;

    if (available_len < 12) {
        return set;
    }

    set.base = read_sequence_number(data, little_endian);
    set.num_bits = read_u32(data + 8, little_endian);

    // Read bitmap (4-byte words)
    const auto num_words = (set.num_bits + 31) / 32;
    const auto bitmap_size = num_words * 4;

    if (available_len >= 12 + bitmap_size) {
        set.bitmap.resize(num_words);
        for (std::uint32_t i = 0; i < num_words; ++i) {
            set.bitmap[i] = read_u32(data + 12 + i * 4, little_endian);
        }
    }

    return set;
}

/// @brief Read Count (4 bytes)
[[nodiscard]] inline Count read_count(const std::byte* data, bool little_endian) {
    Count count;
    count.value = read_i32(data, little_endian);
    return count;
}

/// @brief Read Time (8 bytes)
[[nodiscard]] inline Time read_time(const std::byte* data, bool little_endian) {
    Time t;
    t.seconds = read_i32(data, little_endian);
    t.fraction = read_u32(data + 4, little_endian);
    return t;
}

/// @brief Read VendorId (2 bytes)
[[nodiscard]] inline VendorIdValue read_vendor_id(const std::byte* data) {
    VendorIdValue v;
    v.bytes[0] = static_cast<std::uint8_t>(data[0]);
    v.bytes[1] = static_cast<std::uint8_t>(data[1]);
    return v;
}

/// @brief Read ProtocolVersion (2 bytes)
[[nodiscard]] inline ProtocolVersion read_protocol_version(const std::byte* data) {
    ProtocolVersion v;
    v.major = static_cast<std::uint8_t>(data[0]);
    v.minor = static_cast<std::uint8_t>(data[1]);
    return v;
}

/// @brief Read Locator (24 bytes)
[[nodiscard]] inline Locator read_locator(const std::byte* data, bool little_endian) {
    Locator loc;
    loc.kind = static_cast<LocatorKind>(read_i32(data, little_endian));
    loc.port = read_u32(data + 4, little_endian);
    std::memcpy(loc.address.data(), data + 8, 16);
    return loc;
}

/// @brief Convert SubmessageKind to string
[[nodiscard]] inline std::string_view submessage_kind_string(SubmessageKind kind) {
    switch (kind) {
        case SubmessageKind::PAD:
            return "PAD";
        case SubmessageKind::ACKNACK:
            return "ACKNACK";
        case SubmessageKind::HEARTBEAT:
            return "HEARTBEAT";
        case SubmessageKind::GAP:
            return "GAP";
        case SubmessageKind::INFO_TS:
            return "INFO_TS";
        case SubmessageKind::INFO_SRC:
            return "INFO_SRC";
        case SubmessageKind::INFO_REPLY:
            return "INFO_REPLY";
        case SubmessageKind::INFO_DST:
            return "INFO_DST";
        case SubmessageKind::INFO_REPLY_IP4:
            return "INFO_REPLY_IP4";
        case SubmessageKind::DATA:
            return "DATA";
        case SubmessageKind::DATA_FRAG:
            return "DATA_FRAG";
        case SubmessageKind::NACK_FRAG:
            return "NACK_FRAG";
        case SubmessageKind::HEARTBEAT_FRAG:
            return "HEARTBEAT_FRAG";
        default:
            return "UNKNOWN";
    }
}

/// @brief Convert ParameterId to string
[[nodiscard]] inline std::string_view parameter_id_string(ParameterId id) {
    switch (id) {
        case ParameterId::PID_PAD:
            return "PID_PAD";
        case ParameterId::PID_SENTINEL:
            return "PID_SENTINEL";
        case ParameterId::PID_PARTICIPANT_GUID:
            return "PID_PARTICIPANT_GUID";
        case ParameterId::PID_TOPIC_NAME:
            return "PID_TOPIC_NAME";
        case ParameterId::PID_TYPE_NAME:
            return "PID_TYPE_NAME";
        case ParameterId::PID_PROTOCOL_VERSION:
            return "PID_PROTOCOL_VERSION";
        case ParameterId::PID_VENDOR_ID:
            return "PID_VENDOR_ID";
        case ParameterId::PID_ENDPOINT_GUID:
            return "PID_ENDPOINT_GUID";
        case ParameterId::PID_BUILTIN_ENDPOINT_SET:
            return "PID_BUILTIN_ENDPOINT_SET";
        case ParameterId::PID_DURABILITY:
            return "PID_DURABILITY";
        case ParameterId::PID_RELIABILITY:
            return "PID_RELIABILITY";
        case ParameterId::PID_LIVELINESS:
            return "PID_LIVELINESS";
        case ParameterId::PID_DEADLINE:
            return "PID_DEADLINE";
        case ParameterId::PID_USER_DATA:
            return "PID_USER_DATA";
        case ParameterId::PID_ENTITY_NAME:
            return "PID_ENTITY_NAME";
        default:
            return "PID_UNKNOWN";
    }
}

}  // anonymous namespace

// =============================================================================
// RtpsDecoder Implementation
// =============================================================================

bool RtpsDecoder::looks_like_rtps(std::span<const std::byte> data) {
    if (data.size() < RTPS_HEADER_SIZE) {
        return false;
    }

    // Check magic bytes "RTPS"
    return data[0] == std::byte{'R'} && data[1] == std::byte{'T'} && data[2] == std::byte{'P'} &&
           data[3] == std::byte{'S'};
}

RtpsDecoder::ResultType RtpsDecoder::decode(std::span<const std::byte> data) const {
    ResultType result;

    // Check minimum header size
    if (data.size() < RTPS_HEADER_SIZE) {
        result.error_ =
            DecodeError(DecodeErrorCode::BufferTooSmall, 0, "Insufficient data for RTPS header");
        return result;
    }

    // Validate magic bytes
    if (!looks_like_rtps(data)) {
        result.error_ = DecodeError(DecodeErrorCode::InvalidHeader, 0, "Invalid RTPS magic bytes");
        return result;
    }

    RtpsHeader header;
    
    // Bytes 4-5: Protocol version
    header.version = read_protocol_version(data.data() + 4);

    // Validate protocol version (must be 2.x)
    if (header.version.major != 2) {
        result.error_ = DecodeError(DecodeErrorCode::InvalidVersion, 4, 
                                   "Unsupported RTPS protocol version");
        return result;
    }

    // Bytes 6-7: Vendor ID
    header.vendor_id = read_vendor_id(data.data() + 6);

    // Bytes 8-19: GUID Prefix
    header.guid_prefix = read_guid_prefix(data.data() + 8);

    // Parse submessages
    if (data.size() > RTPS_HEADER_SIZE) {
        header.submessages = parse_submessages(data.subspan(RTPS_HEADER_SIZE));
    }

    result.header_ = std::move(header);
    return result;
}

std::vector<Submessage> RtpsDecoder::parse_submessages(std::span<const std::byte> data) const {
    std::vector<Submessage> submessages;
    std::size_t offset = 0;

    while (offset + SUBMESSAGE_HEADER_SIZE <= data.size()) {
        // Parse submessage header
        const auto* hdr_data = data.data() + offset;

        Submessage submsg;
        submsg.header.kind = static_cast<SubmessageKind>(hdr_data[0]);
        submsg.header.flags = SubmessageFlags::from_byte(static_cast<std::uint8_t>(hdr_data[1]));

        // Length depends on endianness flag in submessage
        const bool little_endian = submsg.header.flags.is_little_endian();
        submsg.header.length = read_u16(hdr_data + 2, little_endian);

        // Calculate total submessage size
        std::size_t submsg_size;
        if (submsg.header.length == 0) {
            // Length of 0 means "to end of message"
            submsg_size = data.size() - offset;
        } else {
            submsg_size = SUBMESSAGE_HEADER_SIZE + submsg.header.length;
        }

        if (offset + submsg_size > data.size()) {
            break;  // Truncated submessage
        }

        // Store raw data reference
        submsg.raw_data = data.subspan(offset, submsg_size);

        // Parse submessage body
        if (submsg.header.length > 0 ||
            (submsg.header.length == 0 && submsg_size > SUBMESSAGE_HEADER_SIZE)) {
            auto body_size = (submsg.header.length > 0) ? submsg.header.length
                                                        : (submsg_size - SUBMESSAGE_HEADER_SIZE);
            auto body_data = data.subspan(offset + SUBMESSAGE_HEADER_SIZE, body_size);
            submsg.body = parse_submessage_body(submsg.header.kind, submsg.header.flags, body_data);
        }

        submessages.push_back(std::move(submsg));

        // Length of 0 means "to end of message" - break after this submessage
        if (submsg.header.length == 0) {
            break;
        }
        offset += submsg_size;
    }

    return submessages;
}

SubmessageBody RtpsDecoder::parse_submessage_body(SubmessageKind kind, SubmessageFlags flags,
                                                  std::span<const std::byte> data) const {
    const bool little_endian = flags.is_little_endian();

    switch (kind) {
        case SubmessageKind::PAD:
            return PadSubmessage{};

        case SubmessageKind::DATA: {
            if (data.size() < 20) {  // Minimum DATA size
                return std::monostate{};
            }
            DataSubmessage msg;
            msg.extra_flags = read_u16(data.data(), little_endian);
            msg.octets_to_inline_qos = read_u16(data.data() + 2, little_endian);
            msg.reader_id = read_entity_id(data.data() + 4);
            msg.writer_id = read_entity_id(data.data() + 8);
            msg.writer_sn = read_sequence_number(data.data() + 12, little_endian);
            msg.flags = DataFlags::from_byte(flags.raw_flags);

            // Calculate offset to payload (after inline QoS if present)
            std::size_t payload_offset = 20;
            const bool has_inline_qos = msg.flags.inline_qos;
            const bool has_data = msg.flags.data_present;
            const bool has_key = msg.flags.key_present;

            // Parse inline QoS if present
            if (has_inline_qos && data.size() > payload_offset) {
                // Calculate QoS length (until sentinel)
                auto qos_data = data.subspan(payload_offset);
                std::size_t qos_offset = 0;
                while (qos_offset + 4 <= qos_data.size()) {
                    auto param_id = read_u16(qos_data.data() + qos_offset, little_endian);
                    auto param_len = read_u16(qos_data.data() + qos_offset + 2, little_endian);
                    qos_offset += 4 + static_cast<std::size_t>(((param_len + 3) / 4) * 4);
                    if (param_id == static_cast<std::uint16_t>(ParameterId::PID_SENTINEL)) {
                        break;
                    }
                }
                // Store raw QoS data
                msg.inline_qos.assign(qos_data.data(), qos_data.data() + qos_offset);
                payload_offset += qos_offset;
            }

            // Store serialized payload if present
            if ((has_data || has_key) && data.size() > payload_offset) {
                msg.serialized_payload.assign(
                    data.begin() + static_cast<std::ptrdiff_t>(payload_offset), data.end());
            }
            return msg;
        }

        case SubmessageKind::HEARTBEAT: {
            if (data.size() < 28) {
                return std::monostate{};
            }
            HeartbeatSubmessage msg;
            msg.reader_id = read_entity_id(data.data());
            msg.writer_id = read_entity_id(data.data() + 4);
            msg.first_sn = read_sequence_number(data.data() + 8, little_endian);
            msg.last_sn = read_sequence_number(data.data() + 16, little_endian);
            msg.count = read_count(data.data() + 24, little_endian);
            msg.flags = HeartbeatFlags::from_byte(flags.raw_flags);
            return msg;
        }

        case SubmessageKind::ACKNACK: {
            if (data.size() < 24) {
                return std::monostate{};
            }
            AckNackSubmessage msg;
            msg.reader_id = read_entity_id(data.data());
            msg.writer_id = read_entity_id(data.data() + 4);
            msg.reader_sn_state =
                read_sequence_number_set(data.data() + 8, data.size() - 8, little_endian);

            // Count is after the sequence number set
            const auto num_words = (msg.reader_sn_state.num_bits + 31) / 32;
            const auto count_offset = 8 + 12 + num_words * 4;  // reader/writer(8) + base+numbits(12) + bitmap
            if (data.size() >= count_offset + 4) {
                msg.count = read_count(data.data() + count_offset, little_endian);
            }
            msg.flags = AckNackFlags::from_byte(flags.raw_flags);
            return msg;
        }

        case SubmessageKind::GAP: {
            if (data.size() < 24) {
                return std::monostate{};
            }
            GapSubmessage msg;
            msg.reader_id = read_entity_id(data.data());
            msg.writer_id = read_entity_id(data.data() + 4);
            msg.gap_start = read_sequence_number(data.data() + 8, little_endian);
            msg.gap_list =
                read_sequence_number_set(data.data() + 16, data.size() - 16, little_endian);
            return msg;
        }

        case SubmessageKind::INFO_TS: {
            InfoTimestampSubmessage msg;
            msg.flags = InfoTimestampFlags::from_byte(flags.raw_flags);
            if (!msg.flags.invalidate_flag && data.size() >= 8) {
                msg.timestamp = read_time(data.data(), little_endian);
            }
            return msg;
        }

        case SubmessageKind::INFO_SRC: {
            if (data.size() < 20) {
                return std::monostate{};
            }
            InfoSourceSubmessage msg;
            msg.unused = read_u32(data.data(), little_endian);
            msg.version = read_protocol_version(data.data() + 4);
            msg.vendor_id = read_vendor_id(data.data() + 6);
            msg.guid_prefix = read_guid_prefix(data.data() + 8);
            return msg;
        }

        case SubmessageKind::INFO_DST: {
            if (data.size() < 12) {
                return std::monostate{};
            }
            InfoDestinationSubmessage msg;
            msg.guid_prefix = read_guid_prefix(data.data());
            return msg;
        }

        case SubmessageKind::INFO_REPLY: {
            InfoReplySubmessage msg;
            msg.flags = InfoReplyFlags::from_byte(flags.raw_flags);

            // Parse unicast locators
            std::size_t offset = 0;
            if (data.size() >= 4) {
                const auto num_unicast = read_u32(data.data(), little_endian);
                offset += 4;
                for (std::uint32_t i = 0; i < num_unicast && offset + LOCATOR_SIZE <= data.size();
                     ++i) {
                    msg.unicast_locator_list.locators.push_back(
                        read_locator(data.data() + offset, little_endian));
                    offset += LOCATOR_SIZE;
                }
            }

            // Parse multicast locators if flag is set
            if (msg.flags.multicast_flag && offset + 4 <= data.size()) {
                const auto num_multicast = read_u32(data.data() + offset, little_endian);
                offset += 4;
                for (std::uint32_t i = 0; i < num_multicast && offset + LOCATOR_SIZE <= data.size();
                     ++i) {
                    msg.multicast_locator_list.locators.push_back(
                        read_locator(data.data() + offset, little_endian));
                    offset += LOCATOR_SIZE;
                }
            }
            return msg;
        }

        // DATA_FRAG, HEARTBEAT_FRAG, NACK_FRAG, INFO_REPLY_IP4 are parsed but not
        // included in SubmessageBody variant - return monostate for now
        case SubmessageKind::DATA_FRAG:
        case SubmessageKind::HEARTBEAT_FRAG:
        case SubmessageKind::NACK_FRAG:
        case SubmessageKind::INFO_REPLY_IP4:
            return std::monostate{};

        default:
            return std::monostate{};
    }
}

// =============================================================================
// Discovery Parser Implementation
// =============================================================================

std::optional<ParameterList> DiscoveryParser::parse_parameter_list(std::span<const std::byte> data,
                                                                   bool little_endian) {
    ParameterList list;
    std::size_t offset = 0;

    while (offset + 4 <= data.size()) {
        // Read parameter ID and length
        const auto param_id = static_cast<ParameterId>(
            little_endian ? read_le16(data.data() + offset) : read_be16(data.data() + offset));
        const auto param_len = little_endian ? read_le16(data.data() + offset + 2)
                                             : read_be16(data.data() + offset + 2);

        offset += 4;

        // Check for sentinel
        if (param_id == ParameterId::PID_SENTINEL) {
            break;
        }

        // Skip padding
        if (param_id == ParameterId::PID_PAD) {
            continue;
        }

        // Validate length
        if (offset + param_len > data.size()) {
            break;
        }

        // Create parameter
        DiscoveryParameter param;
        param.id = param_id;
        param.value.assign(data.data() + offset, data.data() + offset + param_len);
        list.parameters.push_back(std::move(param));

        // Advance (parameters are 4-byte aligned)
        offset += static_cast<std::size_t>(((param_len + 3) / 4) * 4);
    }

    return list;
}

std::optional<ParticipantBuiltinTopicData> DiscoveryParser::parse_participant_data(
    std::span<const std::byte> data, bool little_endian) {
    // Skip encapsulation header (if present)
    std::size_t start = 0;
    if (data.size() >= 4) {
        // CDR encapsulation: 00 00 or 00 01 followed by options
        const auto encap =
            static_cast<std::uint16_t>(data[0]) << 8 | static_cast<std::uint8_t>(data[1]);
        if (encap == 0x0001 || encap == 0x0000) {
            start = 4;
            little_endian = (encap == 0x0001);
        }
    }

    auto params = parse_parameter_list(data.subspan(start), little_endian);
    if (!params) {
        return std::nullopt;
    }

    ParticipantBuiltinTopicData participant;
    participant.raw_parameters = std::move(*params);

    // Extract known parameters
    for (const auto& param : participant.raw_parameters.parameters) {
        switch (param.id) {
            case ParameterId::PID_PARTICIPANT_GUID:
                if (param.value.size() >= GUID_SIZE) {
                    participant.participant_guid = read_guid(param.value.data());
                }
                break;

            case ParameterId::PID_PROTOCOL_VERSION:
                if (param.value.size() >= 2) {
                    participant.protocol_version = read_protocol_version(param.value.data());
                }
                break;

            case ParameterId::PID_VENDOR_ID:
                if (param.value.size() >= 2) {
                    participant.vendor_id = read_vendor_id(param.value.data());
                }
                break;

            case ParameterId::PID_BUILTIN_ENDPOINT_SET:
                if (param.value.size() >= 4) {
                    participant.builtin_endpoints.value = little_endian
                                                              ? read_le32(param.value.data())
                                                              : read_be32(param.value.data());
                }
                break;

            case ParameterId::PID_PARTICIPANT_LEASE_DURATION:
                if (param.value.size() >= 8) {
                    const auto secs = read_i32(param.value.data(), little_endian);
                    const auto frac = read_u32(param.value.data() + 4, little_endian);
                    participant.lease_duration = Duration{secs, frac};
                }
                break;

            case ParameterId::PID_DEFAULT_UNICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    participant.default_unicast_locators.push_back(
                        read_locator(param.value.data(), little_endian));
                }
                break;

            case ParameterId::PID_DEFAULT_MULTICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    participant.default_multicast_locators.push_back(
                        read_locator(param.value.data(), little_endian));
                }
                break;

            case ParameterId::PID_METATRAFFIC_UNICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    participant.metatraffic_unicast_locators.push_back(
                        read_locator(param.value.data(), little_endian));
                }
                break;

            case ParameterId::PID_METATRAFFIC_MULTICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    participant.metatraffic_multicast_locators.push_back(
                        read_locator(param.value.data(), little_endian));
                }
                break;

            case ParameterId::PID_ENTITY_NAME:
                participant.participant_name = param.as_string();
                break;

            case ParameterId::PID_USER_DATA:
                participant.user_data.value = param.value;
                break;

            default:
                break;
        }
    }

    return participant;
}

std::optional<PublicationBuiltinTopicData> DiscoveryParser::parse_publication_data(
    std::span<const std::byte> data, bool little_endian) {
    // Skip encapsulation header
    std::size_t start = 0;
    if (data.size() >= 4) {
        const auto encap =
            static_cast<std::uint16_t>(data[0]) << 8 | static_cast<std::uint8_t>(data[1]);
        if (encap == 0x0001 || encap == 0x0000) {
            start = 4;
            little_endian = (encap == 0x0001);
        }
    }

    auto params = parse_parameter_list(data.subspan(start), little_endian);
    if (!params) {
        return std::nullopt;
    }

    PublicationBuiltinTopicData pub;
    pub.raw_parameters = std::move(*params);

    for (const auto& param : pub.raw_parameters.parameters) {
        switch (param.id) {
            case ParameterId::PID_ENDPOINT_GUID:
                if (param.value.size() >= GUID_SIZE) {
                    pub.writer_guid = read_guid(param.value.data());
                }
                break;

            case ParameterId::PID_PARTICIPANT_GUID:
                if (param.value.size() >= GUID_SIZE) {
                    pub.participant_guid = read_guid(param.value.data());
                }
                break;

            case ParameterId::PID_TOPIC_NAME:
                pub.topic_name = param.as_string();
                break;

            case ParameterId::PID_TYPE_NAME:
                pub.type_name = param.as_string();
                break;

            case ParameterId::PID_RELIABILITY:
                if (param.value.size() >= 4) {
                    pub.reliability.kind =
                        static_cast<ReliabilityKind>(little_endian ? read_le32(param.value.data())
                                                                   : read_be32(param.value.data()));
                }
                break;

            case ParameterId::PID_DURABILITY:
                if (param.value.size() >= 4) {
                    pub.durability.kind =
                        static_cast<DurabilityKind>(little_endian ? read_le32(param.value.data())
                                                                  : read_be32(param.value.data()));
                }
                break;

            case ParameterId::PID_UNICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    pub.unicast_locators.push_back(read_locator(param.value.data(), little_endian));
                }
                break;

            case ParameterId::PID_MULTICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    pub.multicast_locators.push_back(
                        read_locator(param.value.data(), little_endian));
                }
                break;

            default:
                break;
        }
    }

    return pub;
}

std::optional<SubscriptionBuiltinTopicData> DiscoveryParser::parse_subscription_data(
    std::span<const std::byte> data, bool little_endian) {
    // Skip encapsulation header
    std::size_t start = 0;
    if (data.size() >= 4) {
        const auto encap =
            static_cast<std::uint16_t>(data[0]) << 8 | static_cast<std::uint8_t>(data[1]);
        if (encap == 0x0001 || encap == 0x0000) {
            start = 4;
            little_endian = (encap == 0x0001);
        }
    }

    auto params = parse_parameter_list(data.subspan(start), little_endian);
    if (!params) {
        return std::nullopt;
    }

    SubscriptionBuiltinTopicData sub;
    sub.raw_parameters = std::move(*params);

    for (const auto& param : sub.raw_parameters.parameters) {
        switch (param.id) {
            case ParameterId::PID_ENDPOINT_GUID:
                if (param.value.size() >= GUID_SIZE) {
                    sub.reader_guid = read_guid(param.value.data());
                }
                break;

            case ParameterId::PID_PARTICIPANT_GUID:
                if (param.value.size() >= GUID_SIZE) {
                    sub.participant_guid = read_guid(param.value.data());
                }
                break;

            case ParameterId::PID_TOPIC_NAME:
                sub.topic_name = param.as_string();
                break;

            case ParameterId::PID_TYPE_NAME:
                sub.type_name = param.as_string();
                break;

            case ParameterId::PID_RELIABILITY:
                if (param.value.size() >= 4) {
                    sub.reliability.kind =
                        static_cast<ReliabilityKind>(little_endian ? read_le32(param.value.data())
                                                                   : read_be32(param.value.data()));
                }
                break;

            case ParameterId::PID_DURABILITY:
                if (param.value.size() >= 4) {
                    sub.durability.kind =
                        static_cast<DurabilityKind>(little_endian ? read_le32(param.value.data())
                                                                  : read_be32(param.value.data()));
                }
                break;

            case ParameterId::PID_UNICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    sub.unicast_locators.push_back(read_locator(param.value.data(), little_endian));
                }
                break;

            case ParameterId::PID_MULTICAST_LOCATOR:
                if (param.value.size() >= LOCATOR_SIZE) {
                    sub.multicast_locators.push_back(
                        read_locator(param.value.data(), little_endian));
                }
                break;

            default:
                break;
        }
    }

    return sub;
}

}  // namespace wadjet::protocols::dds

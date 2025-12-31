/// @file rtps_decoder.cpp
/// @brief RTPS (Real-Time Publish-Subscribe) protocol decoder implementation
///
/// This file implements the RTPS wire protocol decoder for DDS middleware.
/// RTPS is the wire protocol used by DDS implementations like:
/// - Fast DDS (eProsima)
/// - RTI Connext
/// - Cyclone DDS
/// - OpenDDS

#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/dds/rtps_messages.hpp"
#include "wadjet/protocols/dds/discovery.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

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

/// @brief Read little-endian int32 from buffer
[[nodiscard]] inline std::int32_t read_le32_signed(const std::byte* data) {
    return static_cast<std::int32_t>(read_le32(data));
}

/// @brief Read big-endian int32 from buffer
[[nodiscard]] inline std::int32_t read_be32_signed(const std::byte* data) {
    return static_cast<std::int32_t>(read_be32(data));
}

/// @brief Read int32 with endianness
[[nodiscard]] inline std::int32_t read_i32(const std::byte* data, bool little_endian) {
    return little_endian ? read_le32_signed(data) : read_be32_signed(data);
}

/// @brief Read little-endian uint64 from buffer
[[nodiscard]] inline std::uint64_t read_le64(const std::byte* data) {
    return static_cast<std::uint64_t>(data[0]) |
           (static_cast<std::uint64_t>(data[1]) << 8) |
           (static_cast<std::uint64_t>(data[2]) << 16) |
           (static_cast<std::uint64_t>(data[3]) << 24) |
           (static_cast<std::uint64_t>(data[4]) << 32) |
           (static_cast<std::uint64_t>(data[5]) << 40) |
           (static_cast<std::uint64_t>(data[6]) << 48) |
           (static_cast<std::uint64_t>(data[7]) << 56);
}

/// @brief Read big-endian uint64 from buffer
[[nodiscard]] inline std::uint64_t read_be64(const std::byte* data) {
    return (static_cast<std::uint64_t>(data[0]) << 56) |
           (static_cast<std::uint64_t>(data[1]) << 48) |
           (static_cast<std::uint64_t>(data[2]) << 40) |
           (static_cast<std::uint64_t>(data[3]) << 32) |
           (static_cast<std::uint64_t>(data[4]) << 24) |
           (static_cast<std::uint64_t>(data[5]) << 16) |
           (static_cast<std::uint64_t>(data[6]) << 8) |
           static_cast<std::uint64_t>(data[7]);
}

/// @brief Read GuidPrefix (12 bytes)
[[nodiscard]] inline GuidPrefix read_guid_prefix(const std::byte* data) {
    GuidPrefix prefix;
    std::memcpy(prefix.data.data(), data, GUID_PREFIX_SIZE);
    return prefix;
}

/// @brief Read EntityId (4 bytes)
[[nodiscard]] inline EntityId read_entity_id(const std::byte* data) {
    EntityId id;
    std::memcpy(id.entity_key.data(), data, 3);
    id.entity_kind = static_cast<EntityKind>(data[3]);
    return id;
}

/// @brief Read full GUID (16 bytes)
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

/// @brief Read Locator (24 bytes)
[[nodiscard]] inline Locator read_locator(const std::byte* data, bool little_endian) {
    Locator loc;
    loc.kind = static_cast<LocatorKind>(read_i32(data, little_endian));
    loc.port = read_u32(data + 4, little_endian);
    std::memcpy(loc.address.data(), data + 8, 16);
    return loc;
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

/// @brief Read SequenceNumberSet (variable size)
[[nodiscard]] std::optional<SequenceNumberSet> read_sequence_number_set(
    const std::byte* data, std::size_t len, bool little_endian) {
    
    constexpr std::size_t MIN_SIZE = 12;  // base + numBits
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    SequenceNumberSet set;
    set.base = read_sequence_number(data, little_endian);
    set.num_bits = read_u32(data + 8, little_endian);

    // Calculate bitmap size
    const auto bitmap_longs = (set.num_bits + 31) / 32;
    const auto bitmap_bytes = bitmap_longs * 4;
    
    if (len < MIN_SIZE + bitmap_bytes) {
        return std::nullopt;
    }

    // Read bitmap
    set.bitmap.resize(bitmap_longs);
    for (std::uint32_t i = 0; i < bitmap_longs; ++i) {
        set.bitmap[i] = read_u32(data + MIN_SIZE + i * 4, little_endian);
    }

    return set;
}

/// @brief Read FragmentNumberSet (variable size)
[[nodiscard]] std::optional<FragmentNumberSet> read_fragment_number_set(
    const std::byte* data, std::size_t len, bool little_endian) {
    
    constexpr std::size_t MIN_SIZE = 8;  // base + numBits
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    FragmentNumberSet set;
    set.base = read_u32(data, little_endian);
    set.num_bits = read_u32(data + 4, little_endian);

    // Calculate bitmap size
    const auto bitmap_longs = (set.num_bits + 31) / 32;
    const auto bitmap_bytes = bitmap_longs * 4;
    
    if (len < MIN_SIZE + bitmap_bytes) {
        return std::nullopt;
    }

    // Read bitmap
    set.bitmap.resize(bitmap_longs);
    for (std::uint32_t i = 0; i < bitmap_longs; ++i) {
        set.bitmap[i] = read_u32(data + MIN_SIZE + i * 4, little_endian);
    }

    return set;
}

}  // anonymous namespace

// =============================================================================
// String Conversion Implementations
// =============================================================================

std::string_view submessage_kind_string(SubmessageKind kind) {
    switch (kind) {
    case SubmessageKind::PAD: return "PAD";
    case SubmessageKind::ACKNACK: return "ACKNACK";
    case SubmessageKind::HEARTBEAT: return "HEARTBEAT";
    case SubmessageKind::GAP: return "GAP";
    case SubmessageKind::INFO_TS: return "INFO_TS";
    case SubmessageKind::INFO_SRC: return "INFO_SRC";
    case SubmessageKind::INFO_REPLY_IP4: return "INFO_REPLY_IP4";
    case SubmessageKind::INFO_DST: return "INFO_DST";
    case SubmessageKind::INFO_REPLY: return "INFO_REPLY";
    case SubmessageKind::NACK_FRAG: return "NACK_FRAG";
    case SubmessageKind::HEARTBEAT_FRAG: return "HEARTBEAT_FRAG";
    case SubmessageKind::DATA: return "DATA";
    case SubmessageKind::DATA_FRAG: return "DATA_FRAG";
    default: return "UNKNOWN";
    }
}

std::string_view parameter_id_string(ParameterId id) {
    switch (id) {
    case ParameterId::PID_PAD: return "PID_PAD";
    case ParameterId::PID_SENTINEL: return "PID_SENTINEL";
    case ParameterId::PID_USER_DATA: return "PID_USER_DATA";
    case ParameterId::PID_TOPIC_NAME: return "PID_TOPIC_NAME";
    case ParameterId::PID_TYPE_NAME: return "PID_TYPE_NAME";
    case ParameterId::PID_GROUP_DATA: return "PID_GROUP_DATA";
    case ParameterId::PID_TOPIC_DATA: return "PID_TOPIC_DATA";
    case ParameterId::PID_DURABILITY: return "PID_DURABILITY";
    case ParameterId::PID_DEADLINE: return "PID_DEADLINE";
    case ParameterId::PID_LATENCY_BUDGET: return "PID_LATENCY_BUDGET";
    case ParameterId::PID_LIVELINESS: return "PID_LIVELINESS";
    case ParameterId::PID_RELIABILITY: return "PID_RELIABILITY";
    case ParameterId::PID_HISTORY: return "PID_HISTORY";
    case ParameterId::PID_RESOURCE_LIMITS: return "PID_RESOURCE_LIMITS";
    case ParameterId::PID_OWNERSHIP: return "PID_OWNERSHIP";
    case ParameterId::PID_OWNERSHIP_STRENGTH: return "PID_OWNERSHIP_STRENGTH";
    case ParameterId::PID_PRESENTATION: return "PID_PRESENTATION";
    case ParameterId::PID_PARTITION: return "PID_PARTITION";
    case ParameterId::PID_PROTOCOL_VERSION: return "PID_PROTOCOL_VERSION";
    case ParameterId::PID_VENDOR_ID: return "PID_VENDOR_ID";
    case ParameterId::PID_UNICAST_LOCATOR: return "PID_UNICAST_LOCATOR";
    case ParameterId::PID_MULTICAST_LOCATOR: return "PID_MULTICAST_LOCATOR";
    case ParameterId::PID_DEFAULT_UNICAST_LOCATOR: return "PID_DEFAULT_UNICAST_LOCATOR";
    case ParameterId::PID_DEFAULT_MULTICAST_LOCATOR: return "PID_DEFAULT_MULTICAST_LOCATOR";
    case ParameterId::PID_METATRAFFIC_UNICAST_LOCATOR: return "PID_METATRAFFIC_UNICAST_LOCATOR";
    case ParameterId::PID_METATRAFFIC_MULTICAST_LOCATOR: return "PID_METATRAFFIC_MULTICAST_LOCATOR";
    case ParameterId::PID_PARTICIPANT_BUILTIN_ENDPOINTS: return "PID_PARTICIPANT_BUILTIN_ENDPOINTS";
    case ParameterId::PID_PARTICIPANT_LEASE_DURATION: return "PID_PARTICIPANT_LEASE_DURATION";
    case ParameterId::PID_PARTICIPANT_GUID: return "PID_PARTICIPANT_GUID";
    case ParameterId::PID_BUILTIN_ENDPOINT_SET: return "PID_BUILTIN_ENDPOINT_SET";
    case ParameterId::PID_ENDPOINT_GUID: return "PID_ENDPOINT_GUID";
    case ParameterId::PID_ENTITY_NAME: return "PID_ENTITY_NAME";
    case ParameterId::PID_KEY_HASH: return "PID_KEY_HASH";
    case ParameterId::PID_STATUS_INFO: return "PID_STATUS_INFO";
    default: return "UNKNOWN";
    }
}

// =============================================================================
// RtpsHeader Implementation
// =============================================================================

std::string RtpsHeader::to_string() const {
    std::ostringstream oss;
    oss << "RTPS v" << static_cast<int>(version.major) << "." 
        << static_cast<int>(version.minor);
    oss << " vendor=" << vendor_id.to_string();
    oss << " guid_prefix=" << guid_prefix.to_string();
    oss << " [" << submessages.size() << " submessages]";
    return oss.str();
}

// =============================================================================
// RtpsDecoder Implementation
// =============================================================================

RtpsDecoder::Result 
RtpsDecoder::decode_impl(const DecodeContext& ctx) const {
    const auto& data = ctx.data;

    // Check minimum header size
    if (data.size() < RTPS_HEADER_SIZE) {
        return make_error(DecodeErrorCode::BufferTooSmall,
                         "Insufficient data for RTPS header",
                         ctx.original_offset);
    }

    auto header = parse_header(data.data(), data.size());
    if (!header) {
        return make_error(DecodeErrorCode::InvalidHeader,
                         "Failed to parse RTPS header",
                         ctx.original_offset);
    }

    // Validate protocol magic if strict mode
    if (options_.strict_validation) {
        // Verify "RTPS" magic
        if (data[0] != std::byte{'R'} || data[1] != std::byte{'T'} ||
            data[2] != std::byte{'P'} || data[3] != std::byte{'S'}) {
            return make_error(DecodeErrorCode::InvalidHeader,
                             "Invalid RTPS magic bytes",
                             ctx.original_offset);
        }
    }

    // Parse submessages
    if (options_.parse_submessages && data.size() > RTPS_HEADER_SIZE) {
        parse_submessages(*header, data.data() + RTPS_HEADER_SIZE,
                         data.size() - RTPS_HEADER_SIZE);
    }

    // RTPS is typically the final layer
    DecodeContext next_ctx = ctx.sub_context(data.size());

    return make_success(std::move(*header), std::move(next_ctx));
}

std::optional<RtpsHeader>
RtpsDecoder::parse_header(const std::byte* data, std::size_t len) const {
    if (len < RTPS_HEADER_SIZE) {
        return std::nullopt;
    }

    // Validate magic bytes
    if (data[0] != std::byte{'R'} || data[1] != std::byte{'T'} ||
        data[2] != std::byte{'P'} || data[3] != std::byte{'S'}) {
        return std::nullopt;
    }

    RtpsHeader header;
    
    // Bytes 4-5: Protocol version
    header.version = read_protocol_version(data + 4);

    // Bytes 6-7: Vendor ID
    header.vendor_id = read_vendor_id(data + 6);

    // Bytes 8-19: GUID Prefix
    header.guid_prefix = read_guid_prefix(data + 8);

    return header;
}

void RtpsDecoder::parse_submessages(RtpsHeader& header, 
                                    const std::byte* data, 
                                    std::size_t len) const {
    std::size_t offset = 0;

    while (offset + SUBMESSAGE_HEADER_SIZE <= len) {
        // Parse submessage header
        auto submsg_header = parse_submessage_header(data + offset, len - offset);
        if (!submsg_header) {
            break;
        }

        Submessage submsg;
        submsg.header = *submsg_header;

        // Calculate total submessage size
        const auto submsg_size = SUBMESSAGE_HEADER_SIZE + submsg_header->length;
        if (offset + submsg_size > len) {
            break;
        }

        // Parse submessage body if enabled
        if (options_.parse_body) {
            const auto* body_data = data + offset + SUBMESSAGE_HEADER_SIZE;
            const auto body_len = submsg_header->length;
            const bool little_endian = submsg_header->flags.endian_little;
            
            submsg.body = parse_submessage_body(submsg_header->kind, body_data, 
                                                body_len, little_endian);
        }

        header.submessages.push_back(std::move(submsg));

        // Advance to next submessage
        // Length of 0 means "to end of message" - break after this submessage
        if (submsg_header->length == 0) {
            break;
        }
        offset += submsg_size;
    }
}

std::optional<SubmessageHeader>
RtpsDecoder::parse_submessage_header(const std::byte* data, std::size_t len) const {
    if (len < SUBMESSAGE_HEADER_SIZE) {
        return std::nullopt;
    }

    SubmessageHeader header;
    
    // Byte 0: Submessage kind
    header.kind = static_cast<SubmessageKind>(data[0]);

    // Byte 1: Flags
    const auto flags_byte = static_cast<std::uint8_t>(data[1]);
    header.flags.endian_little = (flags_byte & 0x01) != 0;
    header.flags.raw = flags_byte;

    // Bytes 2-3: Length (endianness determined by flags)
    header.length = header.flags.endian_little ? 
                    read_le16(data + 2) : read_be16(data + 2);

    return header;
}

std::optional<SubmessageBody>
RtpsDecoder::parse_submessage_body(SubmessageKind kind, const std::byte* data,
                                   std::size_t len, bool little_endian) const {
    switch (kind) {
    case SubmessageKind::DATA:
        return parse_data_submessage(data, len, little_endian);

    case SubmessageKind::DATA_FRAG:
        return parse_data_frag_submessage(data, len, little_endian);

    case SubmessageKind::HEARTBEAT:
        return parse_heartbeat_submessage(data, len, little_endian);

    case SubmessageKind::HEARTBEAT_FRAG:
        return parse_heartbeat_frag_submessage(data, len, little_endian);

    case SubmessageKind::ACKNACK:
        return parse_acknack_submessage(data, len, little_endian);

    case SubmessageKind::NACK_FRAG:
        return parse_nack_frag_submessage(data, len, little_endian);

    case SubmessageKind::GAP:
        return parse_gap_submessage(data, len, little_endian);

    case SubmessageKind::INFO_TS:
        return parse_info_ts_submessage(data, len, little_endian);

    case SubmessageKind::INFO_SRC:
        return parse_info_src_submessage(data, len, little_endian);

    case SubmessageKind::INFO_DST:
        return parse_info_dst_submessage(data, len, little_endian);

    case SubmessageKind::INFO_REPLY:
        return parse_info_reply_submessage(data, len, little_endian);

    case SubmessageKind::INFO_REPLY_IP4:
        return parse_info_reply_ip4_submessage(data, len, little_endian);

    case SubmessageKind::PAD:
        return PadSubmessage{};

    default:
        return std::nullopt;
    }
}

std::optional<DataSubmessage>
RtpsDecoder::parse_data_submessage(const std::byte* data, std::size_t len,
                                   bool little_endian) const {
    constexpr std::size_t MIN_SIZE = 20;  // extraFlags(2) + octetsToInlineQos(2) + 
                                          // readerEntityId(4) + writerEntityId(4) + 
                                          // writerSeqNum(8)
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    DataSubmessage data_msg;
    
    // Extra flags (2 bytes)
    data_msg.extra_flags = read_u16(data, little_endian);
    
    // Octets to inline QoS (2 bytes)
    data_msg.octets_to_inline_qos = read_u16(data + 2, little_endian);
    
    // Reader Entity ID (4 bytes)
    data_msg.reader_id = read_entity_id(data + 4);
    
    // Writer Entity ID (4 bytes)
    data_msg.writer_id = read_entity_id(data + 8);
    
    // Writer Sequence Number (8 bytes)
    data_msg.writer_sn = read_sequence_number(data + 12, little_endian);

    // Calculate offset to payload (after inline QoS if present)
    std::size_t payload_offset = MIN_SIZE;
    
    // Copy serialized payload (remaining bytes)
    if (len > payload_offset) {
        data_msg.serialized_payload.assign(data + payload_offset, 
                                          data + len);
    }

    return data_msg;
}

std::optional<DataFragSubmessage>
RtpsDecoder::parse_data_frag_submessage(const std::byte* data, std::size_t len,
                                        bool little_endian) const {
    constexpr std::size_t MIN_SIZE = 36;  // Much larger due to fragment info
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    DataFragSubmessage frag;
    
    frag.extra_flags = read_u16(data, little_endian);
    frag.octets_to_inline_qos = read_u16(data + 2, little_endian);
    frag.reader_id = read_entity_id(data + 4);
    frag.writer_id = read_entity_id(data + 8);
    frag.writer_sn = read_sequence_number(data + 12, little_endian);
    frag.fragment_starting_num = read_u32(data + 20, little_endian);
    frag.fragments_in_submessage = read_u16(data + 24, little_endian);
    frag.fragment_size = read_u16(data + 26, little_endian);
    frag.sample_size = read_u32(data + 28, little_endian);

    // Payload
    if (len > 32) {
        frag.serialized_payload.assign(data + 32, data + len);
    }

    return frag;
}

std::optional<HeartbeatSubmessage>
RtpsDecoder::parse_heartbeat_submessage(const std::byte* data, std::size_t len,
                                        bool little_endian) const {
    constexpr std::size_t SIZE = 28;  // reader(4) + writer(4) + first(8) + last(8) + count(4)
    if (len < SIZE) {
        return std::nullopt;
    }

    HeartbeatSubmessage hb;
    hb.reader_id = read_entity_id(data);
    hb.writer_id = read_entity_id(data + 4);
    hb.first_sn = read_sequence_number(data + 8, little_endian);
    hb.last_sn = read_sequence_number(data + 16, little_endian);
    hb.count = read_count(data + 24, little_endian);

    return hb;
}

std::optional<HeartbeatFragSubmessage>
RtpsDecoder::parse_heartbeat_frag_submessage(const std::byte* data, std::size_t len,
                                             bool little_endian) const {
    constexpr std::size_t SIZE = 28;
    if (len < SIZE) {
        return std::nullopt;
    }

    HeartbeatFragSubmessage hb;
    hb.reader_id = read_entity_id(data);
    hb.writer_id = read_entity_id(data + 4);
    hb.writer_sn = read_sequence_number(data + 8, little_endian);
    hb.last_fragment_num = read_u32(data + 16, little_endian);
    hb.count = read_count(data + 20, little_endian);

    return hb;
}

std::optional<AckNackSubmessage>
RtpsDecoder::parse_acknack_submessage(const std::byte* data, std::size_t len,
                                      bool little_endian) const {
    constexpr std::size_t MIN_SIZE = 24;  // reader(4) + writer(4) + snSet(min 12) + count(4)
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    AckNackSubmessage acknack;
    acknack.reader_id = read_entity_id(data);
    acknack.writer_id = read_entity_id(data + 4);
    
    auto sn_set = read_sequence_number_set(data + 8, len - 8 - 4, little_endian);
    if (!sn_set) {
        return std::nullopt;
    }
    acknack.reader_sn_state = std::move(*sn_set);

    // Count is at the end, after the variable-length bitmap
    const auto sn_set_size = 12 + ((acknack.reader_sn_state.num_bits + 31) / 32) * 4;
    if (len < 8 + sn_set_size + 4) {
        return std::nullopt;
    }
    acknack.count = read_count(data + 8 + sn_set_size, little_endian);

    return acknack;
}

std::optional<NackFragSubmessage>
RtpsDecoder::parse_nack_frag_submessage(const std::byte* data, std::size_t len,
                                        bool little_endian) const {
    constexpr std::size_t MIN_SIZE = 28;
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    NackFragSubmessage nack;
    nack.reader_id = read_entity_id(data);
    nack.writer_id = read_entity_id(data + 4);
    nack.writer_sn = read_sequence_number(data + 8, little_endian);
    
    auto frag_set = read_fragment_number_set(data + 16, len - 16 - 4, little_endian);
    if (!frag_set) {
        return std::nullopt;
    }
    nack.fragment_number_state = std::move(*frag_set);

    // Count at the end
    const auto frag_set_size = 8 + ((nack.fragment_number_state.num_bits + 31) / 32) * 4;
    if (len < 16 + frag_set_size + 4) {
        return std::nullopt;
    }
    nack.count = read_count(data + 16 + frag_set_size, little_endian);

    return nack;
}

std::optional<GapSubmessage>
RtpsDecoder::parse_gap_submessage(const std::byte* data, std::size_t len,
                                  bool little_endian) const {
    constexpr std::size_t MIN_SIZE = 28;  // reader(4) + writer(4) + gapStart(8) + gapList(min 12)
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    GapSubmessage gap;
    gap.reader_id = read_entity_id(data);
    gap.writer_id = read_entity_id(data + 4);
    gap.gap_start = read_sequence_number(data + 8, little_endian);
    
    auto sn_set = read_sequence_number_set(data + 16, len - 16, little_endian);
    if (!sn_set) {
        return std::nullopt;
    }
    gap.gap_list = std::move(*sn_set);

    return gap;
}

std::optional<InfoTimestampSubmessage>
RtpsDecoder::parse_info_ts_submessage(const std::byte* data, std::size_t len,
                                      bool little_endian) const {
    InfoTimestampSubmessage info_ts;

    // If length is 0, timestamp is invalid (no timestamp present)
    if (len >= 8) {
        info_ts.timestamp = read_time(data, little_endian);
    }

    return info_ts;
}

std::optional<InfoSourceSubmessage>
RtpsDecoder::parse_info_src_submessage(const std::byte* data, std::size_t len,
                                       bool little_endian) const {
    constexpr std::size_t SIZE = 16;  // unused(4) + version(2) + vendor(2) + guidPrefix(12)
    // Actually it's 20 bytes with 4 unused bytes at start
    if (len < SIZE) {
        return std::nullopt;
    }

    InfoSourceSubmessage info;
    // Skip unused 4 bytes
    info.protocol_version = read_protocol_version(data + 4);
    info.vendor_id = read_vendor_id(data + 6);
    info.guid_prefix = read_guid_prefix(data + 8);

    return info;
}

std::optional<InfoDestinationSubmessage>
RtpsDecoder::parse_info_dst_submessage(const std::byte* data, std::size_t len,
                                       bool /*little_endian*/) const {
    if (len < GUID_PREFIX_SIZE) {
        return std::nullopt;
    }

    InfoDestinationSubmessage info;
    info.guid_prefix = read_guid_prefix(data);

    return info;
}

std::optional<InfoReplySubmessage>
RtpsDecoder::parse_info_reply_submessage(const std::byte* data, std::size_t len,
                                         bool little_endian) const {
    if (len < 4) {  // At least locator count
        return std::nullopt;
    }

    InfoReplySubmessage info;

    // Read unicast locator list
    const auto unicast_count = read_u32(data, little_endian);
    std::size_t offset = 4;

    for (std::uint32_t i = 0; i < unicast_count && offset + LOCATOR_SIZE <= len; ++i) {
        info.unicast_locators.push_back(read_locator(data + offset, little_endian));
        offset += LOCATOR_SIZE;
    }

    // Multicast locators may follow (depending on flags)
    if (offset + 4 <= len) {
        const auto multicast_count = read_u32(data + offset, little_endian);
        offset += 4;

        for (std::uint32_t i = 0; i < multicast_count && offset + LOCATOR_SIZE <= len; ++i) {
            info.multicast_locators.push_back(read_locator(data + offset, little_endian));
            offset += LOCATOR_SIZE;
        }
    }

    return info;
}

std::optional<InfoReplyIp4Submessage>
RtpsDecoder::parse_info_reply_ip4_submessage(const std::byte* data, std::size_t len,
                                             bool little_endian) const {
    constexpr std::size_t MIN_SIZE = 8;  // address(4) + port(4)
    if (len < MIN_SIZE) {
        return std::nullopt;
    }

    InfoReplyIp4Submessage info;
    std::memcpy(info.unicast_address.data(), data, 4);
    info.unicast_port = read_u32(data + 4, little_endian);

    // Multicast follows if present
    if (len >= 16) {
        std::memcpy(info.multicast_address.data(), data + 8, 4);
        info.multicast_port = read_u32(data + 12, little_endian);
    }

    return info;
}

// =============================================================================
// Discovery Parser Implementation
// =============================================================================

std::optional<ParameterList>
DiscoveryParser::parse_parameter_list(std::span<const std::byte> data, bool little_endian) {
    ParameterList list;
    std::size_t offset = 0;

    while (offset + 4 <= data.size()) {
        // Read parameter ID and length
        const auto param_id = static_cast<ParameterId>(
            little_endian ? read_le16(data.data() + offset) : read_be16(data.data() + offset));
        const auto param_len = little_endian ? 
            read_le16(data.data() + offset + 2) : read_be16(data.data() + offset + 2);
        
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
        offset += ((param_len + 3) / 4) * 4;
    }

    return list;
}

std::optional<ParticipantBuiltinTopicData>
DiscoveryParser::parse_participant_data(std::span<const std::byte> data, bool little_endian) {
    // Skip encapsulation header (if present)
    std::size_t start = 0;
    if (data.size() >= 4) {
        // CDR encapsulation: 00 00 or 00 01 followed by options
        const auto encap = static_cast<std::uint16_t>(data[0]) << 8 | static_cast<std::uint8_t>(data[1]);
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
                participant.builtin_endpoints.value = little_endian ? 
                    read_le32(param.value.data()) : read_be32(param.value.data());
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

std::optional<PublicationBuiltinTopicData>
DiscoveryParser::parse_publication_data(std::span<const std::byte> data, bool little_endian) {
    // Skip encapsulation header
    std::size_t start = 0;
    if (data.size() >= 4) {
        const auto encap = static_cast<std::uint16_t>(data[0]) << 8 | static_cast<std::uint8_t>(data[1]);
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
                pub.reliability.kind = static_cast<ReliabilityKind>(
                    little_endian ? read_le32(param.value.data()) : read_be32(param.value.data()));
            }
            break;

        case ParameterId::PID_DURABILITY:
            if (param.value.size() >= 4) {
                pub.durability.kind = static_cast<DurabilityKind>(
                    little_endian ? read_le32(param.value.data()) : read_be32(param.value.data()));
            }
            break;

        case ParameterId::PID_UNICAST_LOCATOR:
            if (param.value.size() >= LOCATOR_SIZE) {
                pub.unicast_locators.push_back(read_locator(param.value.data(), little_endian));
            }
            break;

        case ParameterId::PID_MULTICAST_LOCATOR:
            if (param.value.size() >= LOCATOR_SIZE) {
                pub.multicast_locators.push_back(read_locator(param.value.data(), little_endian));
            }
            break;

        default:
            break;
        }
    }

    return pub;
}

std::optional<SubscriptionBuiltinTopicData>
DiscoveryParser::parse_subscription_data(std::span<const std::byte> data, bool little_endian) {
    // Skip encapsulation header
    std::size_t start = 0;
    if (data.size() >= 4) {
        const auto encap = static_cast<std::uint16_t>(data[0]) << 8 | static_cast<std::uint8_t>(data[1]);
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
                sub.reliability.kind = static_cast<ReliabilityKind>(
                    little_endian ? read_le32(param.value.data()) : read_be32(param.value.data()));
            }
            break;

        case ParameterId::PID_DURABILITY:
            if (param.value.size() >= 4) {
                sub.durability.kind = static_cast<DurabilityKind>(
                    little_endian ? read_le32(param.value.data()) : read_be32(param.value.data()));
            }
            break;

        case ParameterId::PID_UNICAST_LOCATOR:
            if (param.value.size() >= LOCATOR_SIZE) {
                sub.unicast_locators.push_back(read_locator(param.value.data(), little_endian));
            }
            break;

        case ParameterId::PID_MULTICAST_LOCATOR:
            if (param.value.size() >= LOCATOR_SIZE) {
                sub.multicast_locators.push_back(read_locator(param.value.data(), little_endian));
            }
            break;

        default:
            break;
        }
    }

    return sub;
}

}  // namespace wadjet::protocols::dds

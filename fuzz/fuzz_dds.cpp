/// @file fuzz_dds.cpp
/// @brief Fuzz test harness for DDS/RTPS protocol decoder
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This fuzzer tests the RTPS decoder with malformed/random input to ensure
/// robustness against adversarial network traffic.

#include "wadjet/protocols/dds/rtps.hpp"
#include "wadjet/protocols/dds/rtps_types.hpp"
#include "wadjet/protocols/dds/rtps_messages.hpp"
#include "wadjet/protocols/dds/discovery.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::dds;

// =============================================================================
// RTPS Header Fuzzer
// =============================================================================

/// Fuzz the RTPS header parsing
static void fuzz_rtps_header(std::span<const std::byte> data) {
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();

    RtpsDecoder decoder;
    auto result = decoder.decode(ctx);

    // If decode succeeded, access all fields to ensure no crashes
    if (result.is_ok()) {
        const auto& header = *result;

        // Access header fields
        [[maybe_unused]] auto major = header.version.major;
        [[maybe_unused]] auto minor = header.version.minor;
        [[maybe_unused]] auto vendor = header.vendor_id.to_vendor();
        [[maybe_unused]] auto vendor_str = to_string(header.vendor_id.to_vendor());
        [[maybe_unused]] auto prefix_str = header.guid_prefix.to_string();

        // Access GUID prefix bytes
        for (size_t i = 0; i < header.guid_prefix.data.size(); ++i) {
            [[maybe_unused]] auto byte = header.guid_prefix.data[i];
        }

        // Test helper methods if available
        [[maybe_unused]] auto submsg_count = header.submessages.size();
    }
}

// =============================================================================
// Submessage Fuzzer
// =============================================================================

/// Fuzz submessage parsing
static void fuzz_submessages(std::span<const std::byte> data) {
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();

    RtpsDecoder decoder;
    auto result = decoder.decode(ctx);

    if (result.is_ok()) {
        const auto& header = *result;

        for (const auto& submsg : header.submessages) {
            // Access submessage header
            [[maybe_unused]] auto kind = submsg.header.kind;
            [[maybe_unused]] auto kind_str = to_string(submsg.header.kind);
            [[maybe_unused]] auto length = submsg.header.submessage_length;
            [[maybe_unused]] auto endian = submsg.header.flags.endian_little;

            // Access submessage body based on type
            if (std::holds_alternative<DataSubmessage>(submsg.body)) {
                const auto& data_msg = std::get<DataSubmessage>(submsg.body);
                [[maybe_unused]] auto extra_flags = data_msg.extra_flags;
                [[maybe_unused]] auto octets = data_msg.octets_to_inline_qos;
                [[maybe_unused]] auto reader = data_msg.reader_id;
                [[maybe_unused]] auto writer = data_msg.writer_id;
                [[maybe_unused]] auto sn = data_msg.writer_sn.value();
                [[maybe_unused]] auto payload_size = data_msg.serialized_payload.size();
            }
            else if (std::holds_alternative<DataFragSubmessage>(submsg.body)) {
                const auto& frag = std::get<DataFragSubmessage>(submsg.body);
                [[maybe_unused]] auto frag_start = frag.fragment_starting_num;
                [[maybe_unused]] auto frags_in_submsg = frag.fragments_in_submessage;
                [[maybe_unused]] auto frag_size = frag.fragment_size;
                [[maybe_unused]] auto sample_size = frag.sample_size;
            }
            else if (std::holds_alternative<HeartbeatSubmessage>(submsg.body)) {
                const auto& hb = std::get<HeartbeatSubmessage>(submsg.body);
                [[maybe_unused]] auto reader = hb.reader_id;
                [[maybe_unused]] auto writer = hb.writer_id;
                [[maybe_unused]] auto first = hb.first_sn.value();
                [[maybe_unused]] auto last = hb.last_sn.value();
                [[maybe_unused]] auto count = hb.count.value;
            }
            else if (std::holds_alternative<HeartbeatFragSubmessage>(submsg.body)) {
                const auto& hbf = std::get<HeartbeatFragSubmessage>(submsg.body);
                [[maybe_unused]] auto writer_sn = hbf.writer_sn.value();
                [[maybe_unused]] auto last_frag = hbf.last_fragment_num;
                [[maybe_unused]] auto count = hbf.count.value;
            }
            else if (std::holds_alternative<AckNackSubmessage>(submsg.body)) {
                const auto& ack = std::get<AckNackSubmessage>(submsg.body);
                [[maybe_unused]] auto reader = ack.reader_id;
                [[maybe_unused]] auto writer = ack.writer_id;
                [[maybe_unused]] auto base = ack.reader_sn_state.bitmap_base.value();
                [[maybe_unused]] auto num_bits = ack.reader_sn_state.num_bits;
                [[maybe_unused]] auto count = ack.count.value;
            }
            else if (std::holds_alternative<NackFragSubmessage>(submsg.body)) {
                const auto& nack = std::get<NackFragSubmessage>(submsg.body);
                [[maybe_unused]] auto writer_sn = nack.writer_sn.value();
                [[maybe_unused]] auto frag_base = nack.fragment_number_state.bitmap_base;
                [[maybe_unused]] auto count = nack.count.value;
            }
            else if (std::holds_alternative<GapSubmessage>(submsg.body)) {
                const auto& gap = std::get<GapSubmessage>(submsg.body);
                [[maybe_unused]] auto reader = gap.reader_id;
                [[maybe_unused]] auto writer = gap.writer_id;
                [[maybe_unused]] auto start = gap.gap_start.value();
                [[maybe_unused]] auto base = gap.gap_list.bitmap_base.value();
            }
            else if (std::holds_alternative<InfoTimestampSubmessage>(submsg.body)) {
                const auto& ts = std::get<InfoTimestampSubmessage>(submsg.body);
                if (ts.timestamp) {
                    [[maybe_unused]] auto secs = ts.timestamp->seconds;
                    [[maybe_unused]] auto frac = ts.timestamp->fraction;
                }
            }
            else if (std::holds_alternative<InfoSourceSubmessage>(submsg.body)) {
                const auto& src = std::get<InfoSourceSubmessage>(submsg.body);
                [[maybe_unused]] auto version = src.protocol_version.major;
                [[maybe_unused]] auto vendor = src.vendor_id.to_vendor();
                [[maybe_unused]] auto prefix = src.guid_prefix.to_string();
            }
            else if (std::holds_alternative<InfoDestinationSubmessage>(submsg.body)) {
                const auto& dst = std::get<InfoDestinationSubmessage>(submsg.body);
                [[maybe_unused]] auto prefix = dst.guid_prefix.to_string();
            }
            else if (std::holds_alternative<InfoReplySubmessage>(submsg.body)) {
                const auto& reply = std::get<InfoReplySubmessage>(submsg.body);
                for (const auto& loc : reply.unicast_locator_list) {
                    [[maybe_unused]] auto kind = loc.kind;
                    [[maybe_unused]] auto port = loc.port;
                }
            }
            else if (std::holds_alternative<InfoReplyIp4Submessage>(submsg.body)) {
                const auto& reply4 = std::get<InfoReplyIp4Submessage>(submsg.body);
                [[maybe_unused]] auto addr = reply4.unicast_locator.address;
                [[maybe_unused]] auto port = reply4.unicast_locator.port;
            }
            else if (std::holds_alternative<PadSubmessage>(submsg.body)) {
                // Pad has no data
            }
        }
    }
}

// =============================================================================
// Discovery Fuzzer
// =============================================================================

/// Fuzz discovery message parsing (SPDP/SEDP)
static void fuzz_discovery(std::span<const std::byte> data) {
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();

    RtpsDecoder decoder;
    auto result = decoder.decode(ctx);

    if (result.is_ok()) {
        const auto& header = *result;

        for (const auto& submsg : header.submessages) {
            if (std::holds_alternative<DataSubmessage>(submsg.body)) {
                const auto& data_msg = std::get<DataSubmessage>(submsg.body);
                
                // Try parsing as SPDP participant data
                DiscoveryParser parser;
                auto participant = parser.parse_spdp(data_msg);
                
                if (participant) {
                    [[maybe_unused]] auto name = participant->participant_name;
                    [[maybe_unused]] auto domain = participant->domain_id;
                    [[maybe_unused]] auto guid = participant->participant_guid;
                    [[maybe_unused]] auto lease = participant->lease_duration.seconds;
                    
                    for (const auto& loc : participant->metatraffic_unicast_locators) {
                        [[maybe_unused]] auto kind = loc.kind;
                        [[maybe_unused]] auto port = loc.port;
                        [[maybe_unused]] auto str = loc.to_string();
                    }
                    
                    for (const auto& loc : participant->default_unicast_locators) {
                        [[maybe_unused]] auto kind = loc.kind;
                    }
                }
                
                // Try parsing as SEDP endpoint data
                auto endpoint = parser.parse_sedp(data_msg);
                
                if (endpoint) {
                    [[maybe_unused]] auto topic = endpoint->topic_name;
                    [[maybe_unused]] auto type = endpoint->type_name;
                    [[maybe_unused]] auto guid = endpoint->endpoint_guid;
                    
                    // Access QoS policies
                    [[maybe_unused]] auto durability = endpoint->qos.durability.kind;
                    [[maybe_unused]] auto reliability = endpoint->qos.reliability.kind;
                    [[maybe_unused]] auto ownership = endpoint->qos.ownership.kind;
                    [[maybe_unused]] auto liveliness = endpoint->qos.liveliness.kind;
                    [[maybe_unused]] auto history = endpoint->qos.history.kind;
                }
            }
        }
    }
}

// =============================================================================
// CDR Basic Types Fuzzer
// =============================================================================

/// Fuzz CDR deserialization of basic types
static void fuzz_cdr_types(std::span<const std::byte> data) {
    // Test parsing various sizes of data as CDR-encoded values
    if (data.size() >= 1) {
        [[maybe_unused]] auto byte_val = 
            static_cast<std::uint8_t>(data[0]);
    }
    
    if (data.size() >= 2) {
        // Try as little-endian uint16
        [[maybe_unused]] auto le_u16 = 
            static_cast<std::uint16_t>(data[0]) |
            (static_cast<std::uint16_t>(data[1]) << 8);
        
        // Try as big-endian uint16
        [[maybe_unused]] auto be_u16 = 
            (static_cast<std::uint16_t>(data[0]) << 8) |
            static_cast<std::uint16_t>(data[1]);
    }
    
    if (data.size() >= 4) {
        // Try as little-endian uint32
        [[maybe_unused]] auto le_u32 = 
            static_cast<std::uint32_t>(data[0]) |
            (static_cast<std::uint32_t>(data[1]) << 8) |
            (static_cast<std::uint32_t>(data[2]) << 16) |
            (static_cast<std::uint32_t>(data[3]) << 24);
    }
    
    if (data.size() >= 8) {
        // Try as sequence number
        SequenceNumber sn;
        sn.high = static_cast<std::int32_t>(data[0]) |
                  (static_cast<std::int32_t>(data[1]) << 8) |
                  (static_cast<std::int32_t>(data[2]) << 16) |
                  (static_cast<std::int32_t>(data[3]) << 24);
        sn.low = static_cast<std::uint32_t>(data[4]) |
                 (static_cast<std::uint32_t>(data[5]) << 8) |
                 (static_cast<std::uint32_t>(data[6]) << 16) |
                 (static_cast<std::uint32_t>(data[7]) << 24);
        [[maybe_unused]] auto value = sn.value();
    }
    
    if (data.size() >= 12) {
        // Try as GUID prefix
        GuidPrefix prefix;
        for (size_t i = 0; i < 12; ++i) {
            prefix.data[i] = static_cast<std::uint8_t>(data[i]);
        }
        [[maybe_unused]] auto str = prefix.to_string();
    }
    
    if (data.size() >= 16) {
        // Try as full GUID
        GUID guid;
        for (size_t i = 0; i < 12; ++i) {
            guid.prefix.data[i] = static_cast<std::uint8_t>(data[i]);
        }
        guid.entity_id.entity_key[0] = static_cast<std::uint8_t>(data[12]);
        guid.entity_id.entity_key[1] = static_cast<std::uint8_t>(data[13]);
        guid.entity_id.entity_key[2] = static_cast<std::uint8_t>(data[14]);
        guid.entity_id.entity_kind = static_cast<EntityKind>(data[15]);
        [[maybe_unused]] auto str = guid.to_string();
    }
    
    if (data.size() >= 24) {
        // Try as Locator
        Locator loc;
        loc.kind = static_cast<LocatorKind>(
            static_cast<std::int32_t>(data[0]) |
            (static_cast<std::int32_t>(data[1]) << 8) |
            (static_cast<std::int32_t>(data[2]) << 16) |
            (static_cast<std::int32_t>(data[3]) << 24)
        );
        loc.port = static_cast<std::uint32_t>(data[4]) |
                   (static_cast<std::uint32_t>(data[5]) << 8) |
                   (static_cast<std::uint32_t>(data[6]) << 16) |
                   (static_cast<std::uint32_t>(data[7]) << 24);
        for (size_t i = 0; i < 16 && (8 + i) < data.size(); ++i) {
            loc.address[i] = static_cast<std::uint8_t>(data[8 + i]);
        }
        [[maybe_unused]] auto str = loc.to_string();
    }
}

// =============================================================================
// Main Fuzzer Entry Point
// =============================================================================

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    // Run all fuzz targets
    fuzz_rtps_header(byte_data);
    fuzz_submessages(byte_data);
    fuzz_discovery(byte_data);
    fuzz_cdr_types(byte_data);
    
    // Also test can_decode without full decode
    RtpsDecoder decoder;
    [[maybe_unused]] auto can = decoder.can_decode(byte_data);
    
    return 0;
}

// =============================================================================
// Additional Entry Points for Targeted Fuzzing
// =============================================================================

/// Targeted fuzzer for RTPS header only
extern "C" int LLVMFuzzerTestOneInput_RtpsHeader(const uint8_t* data, size_t size) {
    if (size < 20) {  // Minimum RTPS header size
        return 0;
    }
    
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    fuzz_rtps_header(byte_data);
    return 0;
}

/// Targeted fuzzer for submessages only
extern "C" int LLVMFuzzerTestOneInput_Submessage(const uint8_t* data, size_t size) {
    if (size < 24) {  // Minimum RTPS header + submessage header
        return 0;
    }
    
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    fuzz_submessages(byte_data);
    return 0;
}

/// Targeted fuzzer for discovery messages
extern "C" int LLVMFuzzerTestOneInput_Discovery(const uint8_t* data, size_t size) {
    if (size < 50) {  // Minimum for discovery data
        return 0;
    }
    
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    fuzz_discovery(byte_data);
    return 0;
}

/// Targeted fuzzer for CDR types
extern "C" int LLVMFuzzerTestOneInput_CDR(const uint8_t* data, size_t size) {
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    fuzz_cdr_types(byte_data);
    return 0;
}

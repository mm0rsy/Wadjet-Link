/// @file fuzz_gptp.cpp
/// @brief Fuzz test harness for gPTP (IEEE 802.1AS) decoder
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/gptp/gptp.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::gptp;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data =
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);

    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();

    // Test with default options
    {
        GptpDecoder decoder;
        auto result = decoder.decode(ctx);

        // If decode succeeded, access all fields to ensure no crashes
        if (result) {
            const auto& header = *result;

            // Access all header fields
            [[maybe_unused]] auto transport = header.transport_specific;
            [[maybe_unused]] auto msg_type = header.message_type;
            [[maybe_unused]] auto ver = header.version;
            [[maybe_unused]] auto msg_len = header.message_length;
            [[maybe_unused]] auto domain = header.domain_number;
            [[maybe_unused]] auto flags = header.flags;
            [[maybe_unused]] auto correction = header.correction_field;
            [[maybe_unused]] auto src_port = header.source_port_identity;
            [[maybe_unused]] auto seq_id = header.sequence_id;
            [[maybe_unused]] auto control = header.control;
            [[maybe_unused]] auto log_interval = header.log_message_interval;

            // Access body variant
            [[maybe_unused]] auto body_index = header.body.index();

            // Test helper methods
            [[maybe_unused]] auto str = header.to_string();
            [[maybe_unused]] auto pname = header.protocol_name();
            [[maybe_unused]] auto hsize = header.header_size();
            [[maybe_unused]] auto psize = header.payload_size();

            // Test header state methods
            [[maybe_unused]] auto is_two_step = header.is_two_step();
            [[maybe_unused]] auto is_event = header.is_event_message();

            // Access flags detail
            [[maybe_unused]] auto two_step_flag = header.flags.two_step;
            [[maybe_unused]] auto leap61 = header.flags.leap61;
            [[maybe_unused]] auto leap59 = header.flags.leap59;
            [[maybe_unused]] auto utc_offset_valid = header.flags.current_utc_offset_valid;
            [[maybe_unused]] auto ptp_timescale = header.flags.ptp_timescale;
            [[maybe_unused]] auto time_traceable = header.flags.time_traceable;
            [[maybe_unused]] auto freq_traceable = header.flags.frequency_traceable;

            // Access source port identity detail
            [[maybe_unused]] auto clock_id = header.source_port_identity.clock_identity;
            [[maybe_unused]] auto port_num = header.source_port_identity.port_number;

            // Test message body based on type
            if (std::holds_alternative<SyncMessage>(header.body)) {
                const auto& sync = std::get<SyncMessage>(header.body);
                [[maybe_unused]] auto origin_ts = sync.origin_timestamp;
                [[maybe_unused]] auto secs_msb = sync.origin_timestamp.seconds_msb;
                [[maybe_unused]] auto secs_lsb = sync.origin_timestamp.seconds_lsb;
                [[maybe_unused]] auto nanos = sync.origin_timestamp.nanoseconds;
            }

            if (std::holds_alternative<FollowUpMessage>(header.body)) {
                const auto& fu = std::get<FollowUpMessage>(header.body);
                [[maybe_unused]] auto precise_ts = fu.precise_origin_timestamp;
                [[maybe_unused]] auto has_tlv = fu.tlv.has_value();
                if (fu.tlv) {
                    [[maybe_unused]] auto cum_rate = fu.tlv->cumulative_scaled_rate_offset;
                    [[maybe_unused]] auto gm_time_base = fu.tlv->gm_time_base_indicator;
                    [[maybe_unused]] auto last_gm_phase = fu.tlv->last_gm_phase_change;
                    [[maybe_unused]] auto last_gm_freq = fu.tlv->last_gm_freq_change;
                }
            }

            if (std::holds_alternative<PdelayReqMessage>(header.body)) {
                const auto& req = std::get<PdelayReqMessage>(header.body);
                [[maybe_unused]] auto origin_ts = req.origin_timestamp;
            }

            if (std::holds_alternative<PdelayRespMessage>(header.body)) {
                const auto& resp = std::get<PdelayRespMessage>(header.body);
                [[maybe_unused]] auto req_receipt_ts = resp.request_receipt_timestamp;
                [[maybe_unused]] auto req_port = resp.requesting_port_identity;
            }

            if (std::holds_alternative<PdelayRespFollowUpMessage>(header.body)) {
                const auto& rfu = std::get<PdelayRespFollowUpMessage>(header.body);
                [[maybe_unused]] auto resp_origin_ts = rfu.response_origin_timestamp;
                [[maybe_unused]] auto req_port = rfu.requesting_port_identity;
            }

            if (std::holds_alternative<AnnounceMessage>(header.body)) {
                const auto& ann = std::get<AnnounceMessage>(header.body);
                [[maybe_unused]] auto origin_ts = ann.origin_timestamp;
                [[maybe_unused]] auto utc_offset = ann.current_utc_offset;
                [[maybe_unused]] auto gm_priority1 = ann.grandmaster_priority1;
                [[maybe_unused]] auto gm_quality = ann.grandmaster_clock_quality;
                [[maybe_unused]] auto gm_priority2 = ann.grandmaster_priority2;
                [[maybe_unused]] auto gm_identity = ann.grandmaster_identity;
                [[maybe_unused]] auto steps_removed = ann.steps_removed;
                [[maybe_unused]] auto time_source = ann.time_source;
            }

            if (std::holds_alternative<SignalingMessage>(header.body)) {
                const auto& sig = std::get<SignalingMessage>(header.body);
                [[maybe_unused]] auto target_port = sig.target_port_identity;
                [[maybe_unused]] auto tlv_count = sig.tlvs.size();
                for (const auto& tlv : sig.tlvs) {
                    [[maybe_unused]] auto tlv_type = tlv.type;
                    [[maybe_unused]] auto tlv_len = tlv.length;
                    [[maybe_unused]] auto tlv_data_size = tlv.value.size();
                }
            }
        }
    }

    // Test with strict version checking disabled
    {
        GptpDecoderOptions opts;
        opts.strict_version = false;
        GptpDecoder decoder(opts);
        auto result = decoder.decode(ctx);
        [[maybe_unused]] auto success = result.has_value();
    }

    // Test with strict version checking enabled
    {
        GptpDecoderOptions opts;
        opts.strict_version = true;
        GptpDecoder decoder(opts);
        auto result = decoder.decode(ctx);
        [[maybe_unused]] auto success = result.has_value();
    }

    // Test helper functions
    if (size >= 8) {
        MacAddress mac;
        std::memcpy(mac.octets.data(), data, 6);
        auto clock_id = ClockIdentity::from_mac(mac);
        [[maybe_unused]] auto clock_str = clock_id.to_string();
    }

    // Test multicast check with first 6 bytes as MAC if available
    if (size >= 6) {
        MacAddress mac;
        std::memcpy(mac.octets.data(), data, 6);
        [[maybe_unused]] auto is_multicast = is_gptp_multicast(mac);
    }

    // Test message type helpers with first byte
    if (size >= 1) {
        auto msg_type = static_cast<MessageType>(data[0] & 0x0F);
        [[maybe_unused]] auto is_event = is_event_message(msg_type);
        [[maybe_unused]] auto is_general = is_general_message(msg_type);
        [[maybe_unused]] auto type_str = message_type_string(msg_type);
    }

    // Test flag parsing
    if (size >= 2) {
        std::uint16_t raw_flags = (static_cast<std::uint16_t>(data[0]) << 8) | data[1];
        auto flags = GptpFlags::from_raw(raw_flags);
        [[maybe_unused]] auto two_step = flags.two_step;
    }

    return 0;
}

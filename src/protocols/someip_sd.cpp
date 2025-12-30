/// @file someip_sd.cpp
/// @brief SOME/IP Service Discovery decoder implementation

#include "wadjet/protocols/someip_sd.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace wadjet::protocols::someip_sd {

std::string_view entry_type_string(EntryType type) {
    switch (type) {
        case EntryType::FindService: return "FindService";
        case EntryType::OfferService: return "OfferService";
        case EntryType::StopOfferService: return "StopOfferService";
        case EntryType::SubscribeEventgroup: return "SubscribeEventgroup";
        case EntryType::StopSubscribeEventgroup: return "StopSubscribeEventgroup";
        case EntryType::SubscribeEventgroupAck: return "SubscribeEventgroupAck";
        case EntryType::SubscribeEventgroupNack: return "SubscribeEventgroupNack";
    }
    return "Unknown";
}

std::optional<IPv4EndpointOption> SdOption::as_ipv4_endpoint() const {
    if (type != OptionType::IPv4Endpoint &&
        type != OptionType::IPv4Multicast &&
        type != OptionType::IPv4SDEndpoint) {
        return std::nullopt;
    }

    if (data.size() < 9) {  // 4 (IP) + 1 (reserved) + 1 (protocol) + 2 (port) + 1 (reserved at start)
        return std::nullopt;
    }

    IPv4EndpointOption opt;
    // Data layout: IP[4] + reserved[1] + protocol[1] + port[2]
    std::memcpy(opt.address.bytes.data(), data.data(), 4);
    opt.protocol = static_cast<L4Protocol>(static_cast<std::uint8_t>(data[5]));
    opt.port = static_cast<std::uint16_t>(
        (static_cast<unsigned int>(static_cast<std::uint8_t>(data[6])) << 8) |
         static_cast<unsigned int>(static_cast<std::uint8_t>(data[7]))
    );

    return opt;
}

std::string SomeIpSdHeader::to_string() const {
    std::ostringstream oss;
    oss << "SOME/IP-SD { flags=0x" << std::hex << std::setfill('0')
        << std::setw(2) << static_cast<int>(flags)
        << std::dec << ", entries=" << entries.size()
        << ", options=" << options.size();

    if (is_reboot()) {
        oss << " [REBOOT]";
    }
    if (is_unicast()) {
        oss << " [UNICAST]";
    }
    oss << " }";
    return oss.str();
}

std::vector<const ServiceEntry*> SomeIpSdHeader::get_offers() const {
    std::vector<const ServiceEntry*> result;
    for (const auto& entry : entries) {
        if (auto* svc = std::get_if<ServiceEntry>(&entry)) {
            if (svc->type == EntryType::OfferService) {
                result.push_back(svc);
            }
        }
    }
    return result;
}

std::vector<const EventgroupEntry*> SomeIpSdHeader::get_subscriptions() const {
    std::vector<const EventgroupEntry*> result;
    for (const auto& entry : entries) {
        if (auto* eg = std::get_if<EventgroupEntry>(&entry)) {
            if (eg->type == EntryType::SubscribeEventgroup) {
                result.push_back(eg);
            }
        }
    }
    return result;
}

const ServiceEntry* SomeIpSdHeader::find_service(std::uint16_t service_id,
                                                  std::uint16_t instance_id) const {
    for (const auto& entry : entries) {
        if (auto* svc = std::get_if<ServiceEntry>(&entry)) {
            if (svc->service_id == service_id) {
                if (instance_id == 0xFFFF || svc->instance_id == instance_id) {
                    return svc;
                }
            }
        }
    }
    return nullptr;
}

std::optional<SdEntry> SomeIpSdDecoder::parse_entry(const DecodeContext& ctx,
                                                     std::size_t offset) {
    if (!ctx.has_bytes(offset + ENTRY_SIZE)) {
        return std::nullopt;
    }

    auto type = static_cast<EntryType>(static_cast<std::uint8_t>(ctx.data[offset]));

    // Check if this is an eventgroup entry (type 0x06, 0x07, 0x86, 0x87)
    bool is_eventgroup = (static_cast<std::uint8_t>(type) & 0x06) == 0x06;

    if (is_eventgroup) {
        EventgroupEntry entry;
        entry.type = type;
        entry.index1_first_option = static_cast<std::uint8_t>(ctx.data[offset + 1]);
        entry.index2_first_option = static_cast<std::uint8_t>(ctx.data[offset + 2]);

        std::uint8_t num_opts = static_cast<std::uint8_t>(ctx.data[offset + 3]);
        entry.num_options_1 = (num_opts >> 4) & 0x0F;
        entry.num_options_2 = num_opts & 0x0F;

        entry.service_id = ctx.read_be16(offset + 4);
        entry.instance_id = ctx.read_be16(offset + 6);
        entry.major_version = static_cast<std::uint8_t>(ctx.data[offset + 8]);

        // TTL is 24-bit (bytes 9-11)
        entry.ttl = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ctx.data[offset + 9])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ctx.data[offset + 10])) << 8) |
                     static_cast<std::uint32_t>(static_cast<std::uint8_t>(ctx.data[offset + 11]));

        // Counter (4 bits) and eventgroup ID (12 bits) in bytes 12-15
        std::uint32_t counter_eg = ctx.read_be32(offset + 12);
        entry.counter = static_cast<std::uint8_t>((counter_eg >> 16) & 0x0F);
        entry.eventgroup_id = static_cast<std::uint16_t>(counter_eg & 0xFFFF);

        return entry;
    } else {
        ServiceEntry entry;
        entry.type = type;
        entry.index1_first_option = static_cast<std::uint8_t>(ctx.data[offset + 1]);
        entry.index2_first_option = static_cast<std::uint8_t>(ctx.data[offset + 2]);

        std::uint8_t num_opts = static_cast<std::uint8_t>(ctx.data[offset + 3]);
        entry.num_options_1 = (num_opts >> 4) & 0x0F;
        entry.num_options_2 = num_opts & 0x0F;

        entry.service_id = ctx.read_be16(offset + 4);
        entry.instance_id = ctx.read_be16(offset + 6);
        entry.major_version = static_cast<std::uint8_t>(ctx.data[offset + 8]);

        // TTL is 24-bit (bytes 9-11)
        entry.ttl = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ctx.data[offset + 9])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ctx.data[offset + 10])) << 8) |
                     static_cast<std::uint32_t>(static_cast<std::uint8_t>(ctx.data[offset + 11]));

        entry.minor_version = ctx.read_be32(offset + 12);

        return entry;
    }
}

std::optional<SdOption> SomeIpSdDecoder::parse_option(const DecodeContext& ctx,
                                                       std::size_t offset,
                                                       std::size_t& consumed) {
    if (!ctx.has_bytes(offset + 3)) {
        return std::nullopt;
    }

    // Option format: length[2] + type[1] + data[length-1]
    std::uint16_t length = ctx.read_be16(offset);
    auto type = static_cast<OptionType>(static_cast<std::uint8_t>(ctx.data[offset + 2]));

    if (length == 0) {
        consumed = 3;
        return std::nullopt;
    }

    // Total option size = 3 (length + type) + (length - 1) for data
    // Actually: length field includes the type byte, so data is length-1 bytes
    std::size_t total_size = 2 + length;  // length field (2) + length bytes

    if (!ctx.has_bytes(offset + total_size)) {
        return std::nullopt;
    }

    SdOption opt;
    opt.type = type;

    // Data starts at offset + 3, size is length - 1 (minus type byte)
    if (length > 1) {
        auto data = ctx.read_bytes(offset + 3, length - 1);
        opt.data.assign(data.begin(), data.end());
    }

    consumed = total_size;
    return opt;
}

SomeIpSdDecoder::Result SomeIpSdDecoder::decode_impl(const DecodeContext& ctx) const {
    if (!ctx.has_bytes(SD_HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall,
                          "SOME/IP-SD header too small");
    }

    SomeIpSdHeader header;

    // Flags (byte 0)
    header.flags = static_cast<std::uint8_t>(ctx.data[0]);

    // Reserved (bytes 1-3)
    // Skip

    // Entries array length (bytes 4-7)
    header.entries_length = ctx.read_be32(4);

    // Validate we have enough data for entries
    std::size_t entries_end = 8 + header.entries_length;
    if (!ctx.has_bytes(entries_end)) {
        return make_error(DecodeErrorCode::TruncatedPayload,
                          "SOME/IP-SD entries array truncated");
    }

    // Parse entries
    if (options_.parse_entries) {
        std::size_t offset = 8;
        std::size_t entry_count = 0;

        while (offset + ENTRY_SIZE <= entries_end &&
               entry_count < options_.max_entries) {
            auto entry = parse_entry(ctx, offset);
            if (entry) {
                header.entries.push_back(std::move(*entry));
            }
            offset += ENTRY_SIZE;
            ++entry_count;
        }
    }

    // Options array length (4 bytes after entries)
    if (!ctx.has_bytes(entries_end + 4)) {
        return make_error(DecodeErrorCode::TruncatedPayload,
                          "SOME/IP-SD options length truncated");
    }
    header.options_length = ctx.read_be32(entries_end);

    // Validate we have enough data for options
    std::size_t options_end = entries_end + 4 + header.options_length;
    if (!ctx.has_bytes(options_end)) {
        return make_error(DecodeErrorCode::TruncatedPayload,
                          "SOME/IP-SD options array truncated");
    }

    // Parse options
    if (options_.parse_options) {
        std::size_t offset = entries_end + 4;
        std::size_t option_count = 0;

        while (offset < options_end && option_count < options_.max_options) {
            std::size_t consumed = 0;
            auto opt = parse_option(ctx, offset, consumed);
            if (opt) {
                header.options.push_back(std::move(*opt));
            }
            if (consumed == 0) {
                break;  // Prevent infinite loop
            }
            offset += consumed;
            ++option_count;
        }
    }

    auto next_ctx = ctx.sub_context(options_end);
    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::someip_sd

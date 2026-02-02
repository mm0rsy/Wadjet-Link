/// @file ipv4.cpp
/// @brief IPv4 header decoder implementation

#include "wadjet/protocols/ipv4.hpp"

#include <cstring>
#include <sstream>

namespace wadjet::protocols::ipv4 {

std::string IPv4Header::to_string() const {
    std::ostringstream oss;
    oss << "IPv4 { src=" << src_ip.to_string() << ", dst=" << dst_ip.to_string()
        << ", proto=" << static_cast<int>(protocol) << ", len=" << total_length
        << ", ttl=" << static_cast<int>(ttl);

    if (flags.dont_fragment) {
        oss << ", DF";
    }
    if (is_fragmented()) {
        oss << ", frag_off=" << fragment_offset;
        if (flags.more_fragments) {
            oss << " (MF)";
        }
    }
    if (!checksum_valid) {
        oss << ", CHECKSUM_INVALID";
    }
    oss << " }";
    return oss.str();
}
// Parse raw IPv4 options (TLV-style) into a structured list and detect malformed options
IPv4Header::ParseOptionsResult IPv4Header::parseIpv4Options(const std::vector<std::byte>& raw) {
    ParseOptionsResult res;
    std::size_t i = 0;
    while (i < raw.size()) {
        uint8_t kind = static_cast<uint8_t>(raw[i]);
        if (kind == 0) {  // EOL
            res.options.push_back({kind, {}});
            break;
        } else if (kind == 1) {  // NOP
            res.options.push_back({kind, {}});
            ++i;
            continue;
        } else {
            if (i + 1 >= raw.size()) {
                res.malformed = true;
                break;
            }  // malformed
            uint8_t length = static_cast<uint8_t>(raw[i + 1]);
            if (length < 2 || i + length > raw.size()) {
                res.malformed = true;
                break;
            }  // malformed
            std::vector<uint8_t> data;
            if (length > 2) {
                data.reserve(length - 2);
                for (size_t j = i + 2; j < i + length; ++j)
                    data.push_back(static_cast<uint8_t>(raw[j]));
            }
            res.options.push_back({kind, data});
            i += length;
        }
    }
    return res;
}
std::uint16_t IPv4Decoder::calculate_checksum(std::span<const std::byte> header_data) {
    std::uint32_t sum = 0;

    // Sum all 16-bit words
    for (std::size_t i = 0; i + 1 < header_data.size(); i += 2) {
        std::uint16_t word = static_cast<std::uint16_t>(
            (static_cast<unsigned int>(static_cast<std::uint8_t>(header_data[i])) << 8) |
            static_cast<unsigned int>(static_cast<std::uint8_t>(header_data[i + 1])));
        sum += word;
    }

    // Handle odd byte if present
    if (header_data.size() % 2 != 0) {
        sum += static_cast<std::uint32_t>(static_cast<std::uint8_t>(header_data.back())) << 8;
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<std::uint16_t>(~sum);
}

IPv4Decoder::Result IPv4Decoder::decode_impl(const DecodeContext& ctx) const {
    // Check minimum size
    if (!ctx.has_bytes(MIN_HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "IPv4 header too small");
    }

    IPv4Header header;

    // First byte: version and IHL
    std::uint8_t ver_ihl = static_cast<std::uint8_t>(ctx.data[0]);
    header.version = (ver_ihl >> 4) & 0x0F;
    header.ihl = ver_ihl & 0x0F;

    // Validate version
    if (header.version != 4) {
        return make_error(DecodeErrorCode::InvalidVersion,
                          "Not IPv4 (version=" + std::to_string(header.version) + ")");
    }

    // Validate IHL (minimum 5 = 20 bytes)
    if (header.ihl < 5) {
        return make_error(DecodeErrorCode::InvalidHeader,
                          "IHL too small: " + std::to_string(header.ihl));
    }

    std::size_t header_len = static_cast<std::size_t>(header.ihl) * 4;
    if (!ctx.has_bytes(header_len)) {
        return make_error(DecodeErrorCode::BufferTooSmall,
                          "IPv4 header truncated (need " + std::to_string(header_len) + " bytes)");
    }

    // Second byte: DSCP and ECN
    std::uint8_t tos = static_cast<std::uint8_t>(ctx.data[1]);
    header.dscp = (tos >> 2) & 0x3F;
    header.ecn = tos & 0x03;

    // Total length (bytes 2-3)
    header.total_length = ctx.read_be16(2);

    // Identification (bytes 4-5)
    header.identification = ctx.read_be16(4);

    // Flags and fragment offset (bytes 6-7)
    std::uint16_t flags_frag = ctx.read_be16(6);
    header.flags = IPv4Flags(static_cast<std::uint8_t>((flags_frag >> 13) & 0x07));
    header.fragment_offset = flags_frag & 0x1FFF;

    // TTL (byte 8)
    header.ttl = static_cast<std::uint8_t>(ctx.data[8]);

    // Protocol (byte 9)
    header.protocol = static_cast<std::uint8_t>(ctx.data[9]);

    // Header checksum (bytes 10-11)
    header.checksum = ctx.read_be16(10);

    // Source IP (bytes 12-15)
    auto src_bytes = ctx.read_bytes(12, 4);
    std::memcpy(header.src_ip.bytes.data(), src_bytes.data(), 4);

    // Destination IP (bytes 16-19)
    auto dst_bytes = ctx.read_bytes(16, 4);
    std::memcpy(header.dst_ip.bytes.data(), dst_bytes.data(), 4);

    // Options (if present)
    if (header_len > MIN_HEADER_SIZE) {
        auto opts = ctx.read_bytes(MIN_HEADER_SIZE, header_len - MIN_HEADER_SIZE);
        header.options.assign(opts.begin(), opts.end());
        // Parse options into structured list (NOP/EOL/TLV)
        auto parsed = IPv4Header::parseIpv4Options(header.options);
        header.parsed_options = std::move(parsed.options);
        header.options_malformed = parsed.malformed;
    }

    // Validate checksum
    if (options_.validate_checksum) {
        auto header_data = ctx.read_bytes(0, header_len);
        std::uint16_t computed = calculate_checksum(header_data);
        header.checksum_valid = (computed == 0);

        if (!header.checksum_valid && !options_.allow_bad_checksum) {
            return make_error(DecodeErrorCode::InvalidChecksum, "IPv4 header checksum mismatch");
        }
    } else {
        header.checksum_valid = true;  // Assume valid if not checking
    }

    // If fragmented, fill fragment helper struct for use by reassembler
    if (header.is_fragmented()) {
        IPv4Header::Ipv4Fragment frag;
        frag.src_ip = header.src_ip;
        frag.dst_ip = header.dst_ip;
        frag.protocol = header.protocol;
        frag.identification = header.identification;
        frag.offset =
            static_cast<std::uint16_t>(header.fragment_offset * 8);  // frag offset in bytes
        frag.mf = header.flags.more_fragments;
        // payload will be added by caller using next_ctx
    }

    // Validate total length
    if (header.total_length < header_len) {
        return make_error(DecodeErrorCode::InvalidLength, "Total length less than header length");
    }

    // Create context for next layer
    auto next_ctx = ctx.sub_context(header_len);
    next_ctx.layer_info.ip_protocol = header.protocol;

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::ipv4

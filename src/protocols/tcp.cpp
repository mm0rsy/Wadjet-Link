/// @file tcp.cpp
/// @brief TCP header decoder implementation

#include "wadjet/protocols/tcp.hpp"

#include <sstream>

namespace wadjet::protocols::tcp {

std::string TcpFlags::to_string() const {
    std::string result;
    if (syn)
        result += "SYN ";
    if (ack)
        result += "ACK ";
    if (fin)
        result += "FIN ";
    if (rst)
        result += "RST ";
    if (psh)
        result += "PSH ";
    if (urg)
        result += "URG ";
    if (ece)
        result += "ECE ";
    if (cwr)
        result += "CWR ";
    if (ns)
        result += "NS ";
    if (!result.empty()) {
        result.pop_back();  // Remove trailing space
    }
    return result;
}

std::optional<std::uint16_t> TcpOption::mss() const {
    if (kind == TcpOptionKind::MaxSegmentSize && data.size() == 2) {
        return static_cast<std::uint16_t>((static_cast<std::uint8_t>(data[0]) << 8) |
                                          static_cast<std::uint8_t>(data[1]));
    }
    return std::nullopt;
}

std::optional<std::uint8_t> TcpOption::window_scale() const {
    if (kind == TcpOptionKind::WindowScale && data.size() == 1) {
        return static_cast<std::uint8_t>(data[0]);
    }
    return std::nullopt;
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> TcpOption::timestamps() const {
    if (kind == TcpOptionKind::Timestamps && data.size() == 8) {
        std::uint32_t ts_val =
            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[0])) << 24) |
            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[1])) << 16) |
            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[2])) << 8) |
            static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[3]));
        std::uint32_t ts_ecr =
            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[4])) << 24) |
            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[5])) << 16) |
            (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[6])) << 8) |
            static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[7]));
        return std::make_pair(ts_val, ts_ecr);
    }
    return std::nullopt;
}

std::string TcpHeader::to_string() const {
    std::ostringstream oss;
    oss << "TCP { src_port=" << src_port << ", dst_port=" << dst_port << ", seq=" << seq_num
        << ", ack=" << ack_num << ", flags=[" << flags.to_string() << "]" << ", win=" << window;

    if (auto mss = get_mss(); mss) {
        oss << ", mss=" << *mss;
    }
    if (auto ws = get_window_scale(); ws) {
        oss << ", wscale=" << static_cast<int>(*ws);
    }
    if (!checksum_valid) {
        oss << ", CHECKSUM_INVALID";
    }
    oss << " }";
    return oss.str();
}

std::optional<std::uint16_t> TcpHeader::get_mss() const {
    for (const auto& opt : options) {
        if (auto val = opt.mss(); val) {
            return val;
        }
    }
    return std::nullopt;
}

std::optional<std::uint8_t> TcpHeader::get_window_scale() const {
    for (const auto& opt : options) {
        if (auto val = opt.window_scale(); val) {
            return val;
        }
    }
    return std::nullopt;
}

std::vector<TcpOption> TcpDecoder::parse_options(std::span<const std::byte> opts_data) {
    std::vector<TcpOption> options;
    std::size_t offset = 0;

    while (offset < opts_data.size()) {
        auto kind = static_cast<TcpOptionKind>(static_cast<std::uint8_t>(opts_data[offset]));

        if (kind == TcpOptionKind::EndOfOptions) {
            break;
        }

        if (kind == TcpOptionKind::NoOperation) {
            ++offset;
            continue;
        }

        // Other options have length field
        if (offset + 1 >= opts_data.size()) {
            break;  // Truncated
        }

        std::uint8_t opt_len = static_cast<std::uint8_t>(opts_data[offset + 1]);
        if (opt_len < 2 || offset + opt_len > opts_data.size()) {
            break;  // Invalid length or truncated
        }

        TcpOption opt;
        opt.kind = kind;
        if (opt_len > 2) {
            opt.data.assign(opts_data.begin() + static_cast<std::ptrdiff_t>(offset) + 2,
                            opts_data.begin() + static_cast<std::ptrdiff_t>(offset + opt_len));
        }
        options.push_back(std::move(opt));

        offset += opt_len;
    }

    return options;
}

TcpDecoder::Result TcpDecoder::decode_impl(const DecodeContext& ctx) const {
    // Check minimum size
    if (!ctx.has_bytes(MIN_HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "TCP header too small");
    }

    TcpHeader header;

    // Source port (bytes 0-1)
    header.src_port = ctx.read_be16(0);

    // Destination port (bytes 2-3)
    header.dst_port = ctx.read_be16(2);

    // Sequence number (bytes 4-7)
    header.seq_num = ctx.read_be32(4);

    // Acknowledgment number (bytes 8-11)
    header.ack_num = ctx.read_be32(8);

    // Data offset, reserved, flags (bytes 12-13)
    std::uint16_t offset_flags = ctx.read_be16(12);
    header.data_offset = static_cast<std::uint8_t>((offset_flags >> 12) & 0x0F);
    header.flags = TcpFlags(offset_flags & 0x01FF);

    // Validate data offset
    if (header.data_offset < 5) {
        return make_error(DecodeErrorCode::InvalidHeader,
                          "Data offset too small: " + std::to_string(header.data_offset));
    }

    std::size_t header_len = static_cast<std::size_t>(header.data_offset) * 4;
    if (!ctx.has_bytes(header_len)) {
        return make_error(DecodeErrorCode::BufferTooSmall,
                          "TCP header truncated (need " + std::to_string(header_len) + " bytes)");
    }

    // Window size (bytes 14-15)
    header.window = ctx.read_be16(14);

    // Checksum (bytes 16-17)
    header.checksum = ctx.read_be16(16);

    // Urgent pointer (bytes 18-19)
    header.urgent_ptr = ctx.read_be16(18);

    // Parse options if present
    if (options_.parse_options && header_len > MIN_HEADER_SIZE) {
        auto opts_data = ctx.read_bytes(MIN_HEADER_SIZE, header_len - MIN_HEADER_SIZE);
        header.options = parse_options(opts_data);
    }

    // TODO: Checksum validation requires pseudo-header
    header.checksum_valid = true;

    // Create context for next layer
    auto next_ctx = ctx.sub_context(header_len);
    next_ctx.layer_info.src_port = header.src_port;
    next_ctx.layer_info.dst_port = header.dst_port;

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::tcp

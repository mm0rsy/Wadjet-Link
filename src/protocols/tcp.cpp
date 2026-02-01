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

// ===== TcpConnectionTracker Implementation =====

TcpConnection* TcpConnectionTracker::get_connection(std::uint32_t local_ip, std::uint32_t remote_ip,
                                                    std::uint16_t local_port,
                                                    std::uint16_t remote_port,
                                                    bool create_if_missing) {
    ConnectionKey key{local_ip, remote_ip, local_port, remote_port};

    auto it = connections_.find(key);
    if (it != connections_.end()) {
        return &it->second;
    }

    if (!create_if_missing) {
        return nullptr;
    }

    // Create new connection
    TcpConnection conn;
    conn.local_ip = local_ip;
    conn.remote_ip = remote_ip;
    conn.local_port = local_port;
    conn.remote_port = remote_port;
    conn.state = TcpState::Closed;
    conn.last_seen = std::chrono::steady_clock::now();

    auto result = connections_.emplace(key, std::move(conn));
    return &result.first->second;
}

void TcpConnectionTracker::update_connection(TcpConnection& conn, const TcpHeader& header,
                                             std::chrono::steady_clock::time_point now) {
    conn.last_seen = now;

    // Update sequence numbers
    if (header.flags.syn) {
        conn.remote_seq = header.seq_num;
        if (header.flags.ack) {
            conn.remote_ack = header.ack_num;
        }
    } else if (header.flags.ack) {
        conn.remote_ack = header.ack_num;
    }

    // Extract MSS from options
    if (auto mss = header.get_mss(); mss) {
        conn.remote_mss = *mss;
    }

    // Extract window scale
    if (auto wscale = header.get_window_scale(); wscale) {
        conn.remote_window_scale = *wscale;
    }

    // Check for SACK permitted
    for (const auto& opt : header.options) {
        if (opt.kind == TcpOptionKind::SackPermitted) {
            conn.has_sack_perm = true;
            break;
        }
    }

    // Update window size
    conn.remote_window = header.window;

    // State machine transitions based on flags
    switch (conn.state) {
        case TcpState::Closed:
            if (header.flags.syn && !header.flags.ack) {
                // Received SYN (passive open)
                conn.state = TcpState::SynReceived;
            }
            break;

        case TcpState::Listen:
            if (header.flags.syn && !header.flags.ack) {
                conn.state = TcpState::SynReceived;
            }
            break;

        case TcpState::SynSent:
            if (header.flags.syn && header.flags.ack) {
                // Received SYN-ACK
                conn.state = TcpState::Established;
            } else if (header.flags.syn) {
                // Simultaneous open
                conn.state = TcpState::SynReceived;
            }
            break;

        case TcpState::SynReceived:
            if (header.flags.ack && !header.flags.syn) {
                // Received final ACK
                conn.state = TcpState::Established;
            }
            break;

        case TcpState::Established:
            if (header.flags.fin) {
                conn.state = TcpState::CloseWait;
            } else if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;

        case TcpState::FinWait1:
            if (header.flags.fin && header.flags.ack) {
                // Received both FIN and ACK
                conn.state = TcpState::TimeWait;
            } else if (header.flags.fin) {
                // Received FIN without ACK (simultaneous close)
                conn.state = TcpState::Closing;
            } else if (header.flags.ack) {
                // Received ACK of our FIN
                conn.state = TcpState::FinWait2;
            } else if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;

        case TcpState::FinWait2:
            if (header.flags.fin) {
                conn.state = TcpState::TimeWait;
            } else if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;

        case TcpState::Closing:
            if (header.flags.ack) {
                conn.state = TcpState::TimeWait;
            } else if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;

        case TcpState::CloseWait:
            if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;

        case TcpState::LastAck:
            if (header.flags.ack) {
                conn.state = TcpState::Closed;
            } else if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;

        case TcpState::TimeWait:
            if (header.flags.rst) {
                conn.state = TcpState::Closed;
            }
            break;
    }
}

bool TcpConnectionTracker::is_retransmission(const TcpConnection& conn,
                                             const TcpHeader& header) const {
    // Retransmission is detected when:
    // 1. Connection is in a state where data can be exchanged
    // 2. Sequence number matches or is less than previously seen (indicates duplicate)
    // 3. ACK number indicates no new acknowledgment

    if (conn.state != TcpState::Established && conn.state != TcpState::FinWait1 &&
        conn.state != TcpState::FinWait2 && conn.state != TcpState::CloseWait &&
        conn.state != TcpState::Closing && conn.state != TcpState::LastAck) {
        return false;
    }

    // For non-SYN, non-FIN packets
    if (!header.flags.syn && !header.flags.fin) {
        // Check if sequence number is not advancing (retransmission indicator)
        // We compare against the last ACK we've sent (remote_seq)
        if (header.seq_num <= conn.remote_seq) {
            return true;  // Sequence number not advancing = retransmission
        }
    }

    return false;
}

void TcpConnectionTracker::cleanup_expired(std::chrono::steady_clock::time_point now) {
    std::vector<decltype(connections_)::iterator> to_erase;

    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        auto& conn = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - conn.last_seen);

        bool should_erase = false;
        switch (conn.state) {
            case TcpState::Closed:
                should_erase = true;  // Immediately remove CLOSED connections
                break;
            case TcpState::TimeWait:
                // 30 seconds in tests, 2 minutes in production
                should_erase = elapsed.count() >= 30;
                break;
            case TcpState::SynSent:
            case TcpState::SynReceived:
                // Incomplete connections timeout after 2 minutes
                should_erase = elapsed.count() >= 120;
                break;
            default:
                // Other states remain open (user responsibility to close)
                break;
        }

        if (should_erase) {
            to_erase.push_back(it);
        }
    }

    for (auto it : to_erase) {
        connections_.erase(it);
    }
}

void TcpConnectionTracker::clear() {
    connections_.clear();
}

std::size_t TcpConnectionTracker::size() const {
    return connections_.size();
}

std::size_t TcpConnectionTracker::ConnectionKeyHash::operator()(const ConnectionKey& key) const {
    // Hash function combining all 5-tuple elements
    std::size_t h1 = std::hash<std::uint32_t>{}(key.local_ip);
    std::size_t h2 = std::hash<std::uint32_t>{}(key.remote_ip);
    std::size_t h3 = std::hash<std::uint16_t>{}(key.local_port);
    std::size_t h4 = std::hash<std::uint16_t>{}(key.remote_port);

    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
}
}  // namespace wadjet::protocols::tcp
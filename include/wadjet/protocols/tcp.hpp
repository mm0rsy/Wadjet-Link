#pragma once

/// @file tcp.hpp
/// @brief TCP header decoder

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace wadjet::protocols::tcp {

/// @brief Minimum TCP header size (no options)
inline constexpr std::size_t MIN_HEADER_SIZE = 20;

/// @brief Maximum TCP header size (with options)
inline constexpr std::size_t MAX_HEADER_SIZE = 60;

/// @brief TCP connection states (RFC 793)
enum class TcpState : std::uint8_t {
    Closed = 0,       ///< No connection
    Listen = 1,       ///< Waiting for connection request (passive open)
    SynSent = 2,      ///< Sent SYN, waiting for SYN-ACK (active open)
    SynReceived = 3,  ///< Received SYN, sent SYN-ACK, waiting for ACK
    Established = 4,  ///< Connection established, data transfer
    FinWait1 = 5,     ///< Sent FIN, waiting for ACK
    FinWait2 = 6,     ///< Received ACK of FIN, waiting for FIN from peer
    Closing = 7,      ///< Received FIN, waiting for final ACK (simultaneous close)
    TimeWait = 8,     ///< Both sides closed, waiting for timeout (2 minutes / 30 seconds in tests)
    CloseWait = 9,    ///< Received FIN, waiting for application to close
    LastAck = 10      ///< Sent FIN in response to peer's FIN, waiting for final ACK
};

/// @brief TCP connection for stateful analysis
struct TcpConnection {
    TcpState state = TcpState::Closed;                ///< Current connection state
    std::uint32_t local_seq = 0;                      ///< Local sequence number
    std::uint32_t remote_seq = 0;                     ///< Remote sequence number
    std::uint32_t local_ack = 0;                      ///< Local acknowledgment number
    std::uint32_t remote_ack = 0;                     ///< Remote acknowledgment number
    std::uint16_t local_port = 0;                     ///< Local port (5-tuple key)
    std::uint16_t remote_port = 0;                    ///< Remote port (5-tuple key)
    std::uint32_t local_ip = 0;                       ///< Local IP (5-tuple key, IPv4)
    std::uint32_t remote_ip = 0;                      ///< Remote IP (5-tuple key)
    std::uint16_t local_window = 65535;               ///< Advertised window size
    std::uint16_t remote_window = 65535;              ///< Peer's advertised window size
    std::uint8_t local_window_scale = 0;              ///< Window scale factor (RFC 1323)
    std::uint8_t remote_window_scale = 0;             ///< Peer's window scale factor
    std::vector<std::uint32_t> out_of_order_buffer;   ///< Out-of-order segment queue (max 16)
    std::chrono::steady_clock::time_point last_seen;  ///< Last packet timestamp
    bool has_sack_perm = false;                       ///< Peer supports SACK
    std::uint16_t remote_mss = 536;                   ///< Peer's MSS
};

/// @brief TCP flags
struct TcpFlags {
    bool fin : 1;  ///< No more data from sender
    bool syn : 1;  ///< Synchronize sequence numbers
    bool rst : 1;  ///< Reset connection
    bool psh : 1;  ///< Push function
    bool ack : 1;  ///< Acknowledgment field valid
    bool urg : 1;  ///< Urgent pointer field valid
    bool ece : 1;  ///< ECN-Echo
    bool cwr : 1;  ///< Congestion Window Reduced
    bool ns : 1;   ///< ECN-nonce concealment

    TcpFlags()
        : fin(false),
          syn(false),
          rst(false),
          psh(false),
          ack(false),
          urg(false),
          ece(false),
          cwr(false),
          ns(false) {}

    explicit TcpFlags(std::uint16_t flags_word) {
        fin = (flags_word >> 0) & 1;
        syn = (flags_word >> 1) & 1;
        rst = (flags_word >> 2) & 1;
        psh = (flags_word >> 3) & 1;
        ack = (flags_word >> 4) & 1;
        urg = (flags_word >> 5) & 1;
        ece = (flags_word >> 6) & 1;
        cwr = (flags_word >> 7) & 1;
        ns = (flags_word >> 8) & 1;
    }

    /// @brief Convert to string representation
    [[nodiscard]] std::string to_string() const;
};

/// @brief TCP option kinds
enum class TcpOptionKind : std::uint8_t {
    EndOfOptions = 0,
    NoOperation = 1,
    MaxSegmentSize = 2,
    WindowScale = 3,
    SackPermitted = 4,
    Sack = 5,
    Timestamps = 8,
};

/// @brief Parsed TCP option
struct TcpOption {
    TcpOptionKind kind;
    std::vector<std::byte> data;

    /// @brief Get MSS value (if kind == MaxSegmentSize)
    [[nodiscard]] std::optional<std::uint16_t> mss() const;

    /// @brief Get window scale (if kind == WindowScale)
    [[nodiscard]] std::optional<std::uint8_t> window_scale() const;

    /// @brief Get timestamps (if kind == Timestamps)
    [[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> timestamps() const;
};

/// @brief Decoded TCP header
struct TcpHeader : public IDecodedHeader {
    std::uint16_t src_port = 0;      ///< Source port
    std::uint16_t dst_port = 0;      ///< Destination port
    std::uint32_t seq_num = 0;       ///< Sequence number
    std::uint32_t ack_num = 0;       ///< Acknowledgment number
    std::uint8_t data_offset = 5;    ///< Data offset (header length in 32-bit words)
    TcpFlags flags;                  ///< TCP flags
    std::uint16_t window = 0;        ///< Window size
    std::uint16_t checksum = 0;      ///< TCP checksum
    std::uint16_t urgent_ptr = 0;    ///< Urgent pointer
    std::vector<TcpOption> options;  ///< TCP options
    bool checksum_valid = true;      ///< Whether checksum was validated

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "TCP"; }

    [[nodiscard]] std::size_t header_size() const override {
        return static_cast<std::size_t>(data_offset) * 4;
    }

    [[nodiscard]] std::size_t payload_size() const override {
        return 0;  // Unknown from TCP header alone
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Check if this is a SYN packet
    [[nodiscard]] bool is_syn() const { return flags.syn && !flags.ack; }

    /// @brief Check if this is a SYN-ACK packet
    [[nodiscard]] bool is_syn_ack() const { return flags.syn && flags.ack; }

    /// @brief Check if this is a FIN packet
    [[nodiscard]] bool is_fin() const { return flags.fin; }

    /// @brief Check if this is a RST packet
    [[nodiscard]] bool is_rst() const { return flags.rst; }

    /// @brief Get MSS option value (if present)
    [[nodiscard]] std::optional<std::uint16_t> get_mss() const;

    /// @brief Get window scale option value (if present)
    [[nodiscard]] std::optional<std::uint8_t> get_window_scale() const;
};

/// @brief TCP header decoder
class TcpDecoder : public DecoderBase<TcpDecoder, TcpHeader> {
public:
    /// @brief Decoder options
    struct Options {
        bool parse_options;      ///< Parse TCP options
        bool validate_checksum;  ///< Validate TCP checksum
        Options() : parse_options(true), validate_checksum(false) {}
    };

    explicit TcpDecoder(Options opts = Options()) : options_(opts) {}

    [[nodiscard]] std::string_view name() const override { return "TCP"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        return ctx.layer_info.ip_protocol == static_cast<std::uint8_t>(IpProtocol::TCP);
    }

    /// @brief Decode TCP header
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

private:
    /// @brief Parse TCP options
    [[nodiscard]] static std::vector<TcpOption> parse_options(std::span<const std::byte> opts_data);

    Options options_;
};

/// @brief TCP connection tracker for stateful analysis
/// Maintains connections with timeouts (2 min incomplete, 30 sec TIME_WAIT)
class TcpConnectionTracker {
public:
    /// @brief Get or create a connection for the given 5-tuple
    [[nodiscard]] TcpConnection* get_connection(std::uint32_t local_ip, std::uint32_t remote_ip,
                                                std::uint16_t local_port, std::uint16_t remote_port,
                                                bool create_if_missing = true);

    /// @brief Update connection state based on TCP flags
    void update_connection(
        TcpConnection& conn, const TcpHeader& header,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// @brief Handle retransmission detection (duplicate sequence)
    [[nodiscard]] bool is_retransmission(const TcpConnection& conn, const TcpHeader& header) const;

    /// @brief Cleanup expired connections
    /// Removes connections with timeout based on state:
    /// - CLOSED: 0s
    /// - TIME_WAIT: 30s (test) / 2min (production)
    /// - SYN_SENT, SYN_RECEIVED: 2min
    /// - others: user configurable (default no timeout)
    void cleanup_expired(
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// @brief Clear all connections
    void clear();

    /// @brief Get number of active connections
    [[nodiscard]] std::size_t size() const;

private:
    /// @brief 5-tuple hash key for connections
    struct ConnectionKey {
        std::uint32_t local_ip;
        std::uint32_t remote_ip;
        std::uint16_t local_port;
        std::uint16_t remote_port;

        [[nodiscard]] bool operator==(const ConnectionKey& other) const {
            return local_ip == other.local_ip && remote_ip == other.remote_ip &&
                   local_port == other.local_port && remote_port == other.remote_port;
        }
    };

    /// @brief Hash function for ConnectionKey
    struct ConnectionKeyHash {
        [[nodiscard]] std::size_t operator()(const ConnectionKey& key) const;
    };

    std::unordered_map<ConnectionKey, TcpConnection, ConnectionKeyHash> connections_;
};

/// @brief Global TCP decoder instance
inline const TcpDecoder& tcp_decoder() {
    static TcpDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::tcp

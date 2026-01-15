#pragma once

/// @file ipv4.hpp
/// @brief IPv4 header decoder

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::protocols::ipv4 {

/// @brief Minimum IPv4 header size (no options)
inline constexpr std::size_t MIN_HEADER_SIZE = 20;

/// @brief Maximum IPv4 header size (with options)
inline constexpr std::size_t MAX_HEADER_SIZE = 60;

/// @brief IPv4 flags
struct IPv4Flags {
    bool reserved : 1;        ///< Reserved (must be 0)
    bool dont_fragment : 1;   ///< Don't Fragment (DF)
    bool more_fragments : 1;  ///< More Fragments (MF)

    IPv4Flags() : reserved(false), dont_fragment(false), more_fragments(false) {}

    explicit IPv4Flags(std::uint8_t flags_byte)
        : reserved((flags_byte >> 2) & 1),
          dont_fragment((flags_byte >> 1) & 1),
          more_fragments(flags_byte & 1) {}
};

/// @brief Decoded IPv4 header
struct IPv4Header : public IDecodedHeader {
    std::uint8_t version = 4;           ///< IP version (should be 4)
    std::uint8_t ihl = 5;               ///< Internet Header Length (in 32-bit words)
    std::uint8_t dscp = 0;              ///< Differentiated Services Code Point
    std::uint8_t ecn = 0;               ///< Explicit Congestion Notification
    std::uint16_t total_length = 0;     ///< Total packet length
    std::uint16_t identification = 0;   ///< Fragment identification
    IPv4Flags flags;                    ///< Fragmentation flags
    std::uint16_t fragment_offset = 0;  ///< Fragment offset (in 8-byte units)
    std::uint8_t ttl = 0;               ///< Time To Live
    std::uint8_t protocol = 0;          ///< Protocol (TCP=6, UDP=17, etc.)
    std::uint16_t checksum = 0;         ///< Header checksum
    IPv4Address src_ip;                 ///< Source IP address
    IPv4Address dst_ip;                 ///< Destination IP address
    std::vector<std::byte> options;     ///< IP options (raw bytes, if present)
    bool checksum_valid = false;        ///< Whether checksum was validated

    /// Parsed IPv4 options (populated by parseIpv4Options)
    struct Option {
        std::uint8_t type;
        std::vector<std::uint8_t> data;
    };
    using OptionsList = std::vector<Option>;
    OptionsList parsed_options;
    bool options_malformed = false; ///< Whether option parsing encountered malformed data

    /// Known IPv4 option types (IANA)
    enum class OptionType : std::uint8_t {
        EOL = 0,
        NOP = 1,
        RECORD_ROUTE = 7,
        TIMESTAMP = 68,
        SECURITY = 130,
        LOOSE_SOURCE_ROUTE = 131,
        STREAM_ID = 136,
        STRICT_SOURCE_ROUTE = 137,
    };

    /// Result of parsing IPv4 options
    struct ParseOptionsResult {
        OptionsList options;
        bool malformed = false;
    };

    /// Parse raw options into structured list and detect malformed options
    [[nodiscard]] static ParseOptionsResult parseIpv4Options(const std::vector<std::byte>& raw);

    /// Fragment info (if packet is a fragment)
    struct Ipv4Fragment {
        IPv4Address src_ip;
        IPv4Address dst_ip;
        std::uint8_t protocol = 0;
        std::uint16_t identification = 0;
        std::uint16_t offset = 0; // in bytes
        bool mf = false; // more fragments flag
        std::vector<std::uint8_t> payload;
    };


    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "IPv4"; }

    [[nodiscard]] std::size_t header_size() const override {
        return static_cast<std::size_t>(ihl) * 4;
    }

    [[nodiscard]] std::size_t payload_size() const override {
        if (total_length >= header_size()) {
            return total_length - header_size();
        }
        return 0;
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Check if packet is fragmented
    [[nodiscard]] bool is_fragmented() const { return flags.more_fragments || fragment_offset > 0; }

    /// @brief Check if this is TCP
    [[nodiscard]] bool is_tcp() const {
        return protocol == static_cast<std::uint8_t>(IpProtocol::TCP);
    }

    /// @brief Check if this is UDP
    [[nodiscard]] bool is_udp() const {
        return protocol == static_cast<std::uint8_t>(IpProtocol::UDP);
    }

    /// @brief Check if this is ICMP
    [[nodiscard]] bool is_icmp() const {
        return protocol == static_cast<std::uint8_t>(IpProtocol::ICMP);
    }
};

/// @brief IPv4 header decoder
class IPv4Decoder : public DecoderBase<IPv4Decoder, IPv4Header> {
public:
    /// @brief Decoder options
    struct Options {
        bool validate_checksum;   ///< Verify header checksum
        bool allow_bad_checksum;  ///< Continue decoding even if checksum fails
        Options() : validate_checksum(true), allow_bad_checksum(false) {}
        Options(bool validate, bool allow_bad)
            : validate_checksum(validate), allow_bad_checksum(allow_bad) {}
    };

    explicit IPv4Decoder(Options opts = Options()) : options_(opts) {}

    [[nodiscard]] std::string_view name() const override { return "IPv4"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        // Check ethertype from Ethernet layer
        return ctx.layer_info.ethertype == static_cast<std::uint16_t>(EtherType::IPv4);
    }

    /// @brief Decode IPv4 header
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

    /// @brief Calculate IPv4 header checksum
    [[nodiscard]] static std::uint16_t calculate_checksum(std::span<const std::byte> header_data);

private:
    Options options_;
};

/// @brief Global IPv4 decoder instance
inline const IPv4Decoder& ipv4_decoder() {
    static IPv4Decoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::ipv4

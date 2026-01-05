#pragma once

/// @file rtps.hpp
/// @brief RTPS (Real-Time Publish-Subscribe) protocol header and submessage types
///
/// This file provides the main RTPS header structure and submessage types
/// for the DDS wire protocol used in automotive middleware.
///
/// ## Protocol Overview
///
/// RTPS (Real-Time Publish-Subscribe Protocol) is the wire protocol for DDS.
/// It uses a message structure with:
/// - RTPS Header (20 bytes): Magic, version, vendor, GUID prefix
/// - Submessages: Variable-length messages (DATA, HEARTBEAT, ACKNACK, etc.)
///
/// ## Example Usage
///
/// ```cpp
/// #include <wadjet/protocols/dds/rtps.hpp>
///
/// using namespace wadjet::protocols::dds;
///
/// RtpsDecoder decoder;
/// auto result = decoder.decode(packet_data);
///
/// if (result) {
///     const auto& header = result.value();
///     std::cout << "Vendor: " << header.vendor_id.to_string() << "\n";
///     for (const auto& submsg : header.submessages) {
///         std::cout << "Submessage: " << submessage_kind_string(submsg.kind) << "\n";
///     }
/// }
/// ```

#include "wadjet/protocols/dds/rtps_messages.hpp"
#include "wadjet/protocols/dds/rtps_types.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace wadjet::protocols::dds {

// =============================================================================
// Submessage Types
// =============================================================================

/// @brief Submessage kind (SubmessageId)
enum class SubmessageKind : std::uint8_t {
    PAD = 0x01,             ///< Pad submessage
    ACKNACK = 0x06,         ///< Acknowledgement/negative acknowledgement
    HEARTBEAT = 0x07,       ///< Heartbeat
    GAP = 0x08,             ///< Gap in sequence numbers
    INFO_TS = 0x09,         ///< Timestamp information
    INFO_SRC = 0x0C,        ///< Source information
    INFO_REPLY_IP4 = 0x0D,  ///< IPv4 reply information
    INFO_DST = 0x0E,        ///< Destination information
    INFO_REPLY = 0x0F,      ///< Reply information
    NACK_FRAG = 0x12,       ///< Negative acknowledgement for fragments
    HEARTBEAT_FRAG = 0x13,  ///< Heartbeat for fragments
    DATA = 0x15,            ///< Data submessage
    DATA_FRAG = 0x16,       ///< Fragmented data submessage
};

/// @brief Convert submessage kind to string
[[nodiscard]] constexpr std::string_view submessage_kind_string(SubmessageKind kind) {
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
        case SubmessageKind::INFO_REPLY_IP4:
            return "INFO_REPLY_IP4";
        case SubmessageKind::INFO_DST:
            return "INFO_DST";
        case SubmessageKind::INFO_REPLY:
            return "INFO_REPLY";
        case SubmessageKind::NACK_FRAG:
            return "NACK_FRAG";
        case SubmessageKind::HEARTBEAT_FRAG:
            return "HEARTBEAT_FRAG";
        case SubmessageKind::DATA:
            return "DATA";
        case SubmessageKind::DATA_FRAG:
            return "DATA_FRAG";
        default:
            return "UNKNOWN";
    }
}

/// @brief Check if submessage kind is valid
[[nodiscard]] constexpr bool is_valid_submessage_kind(std::uint8_t kind) {
    switch (static_cast<SubmessageKind>(kind)) {
        case SubmessageKind::PAD:
        case SubmessageKind::ACKNACK:
        case SubmessageKind::HEARTBEAT:
        case SubmessageKind::GAP:
        case SubmessageKind::INFO_TS:
        case SubmessageKind::INFO_SRC:
        case SubmessageKind::INFO_REPLY_IP4:
        case SubmessageKind::INFO_DST:
        case SubmessageKind::INFO_REPLY:
        case SubmessageKind::NACK_FRAG:
        case SubmessageKind::HEARTBEAT_FRAG:
        case SubmessageKind::DATA:
        case SubmessageKind::DATA_FRAG:
            return true;
        default:
            return false;
    }
}

// =============================================================================
// Submessage Flags
// =============================================================================

/// @brief Submessage flags (common flags in the flags byte)
struct SubmessageFlags {
    bool endianness_flag = false;  ///< E flag: false=big-endian, true=little-endian
    std::uint8_t raw_flags = 0;    ///< Raw flags byte for submessage-specific flags

    /// @brief Check if data is little-endian
    [[nodiscard]] bool is_little_endian() const { return endianness_flag; }

    /// @brief Check if data is big-endian
    [[nodiscard]] bool is_big_endian() const { return !endianness_flag; }

    /// @brief Parse flags from byte
    static SubmessageFlags from_byte(std::uint8_t byte) {
        SubmessageFlags flags;
        flags.endianness_flag = (byte & 0x01) != 0;
        flags.raw_flags = byte;
        return flags;
    }
};

// =============================================================================
// Submessage Header
// =============================================================================

/// @brief Submessage header (4 bytes)
struct SubmessageHeader {
    SubmessageKind kind = SubmessageKind::PAD;
    SubmessageFlags flags;
    std::uint16_t length = 0;  ///< Octets after this header (not including header)

    /// @brief Total size including header
    [[nodiscard]] std::size_t total_size() const { return SUBMESSAGE_HEADER_SIZE + length; }
};

// =============================================================================
// Submessage Body Variant (types defined in rtps_messages.hpp)
// =============================================================================

/// @brief Submessage body variant
using SubmessageBody = std::variant<std::monostate,  // Unknown/unparsed
                                    DataSubmessage, HeartbeatSubmessage, AckNackSubmessage,
                                    GapSubmessage, InfoTimestampSubmessage, InfoSourceSubmessage,
                                    InfoDestinationSubmessage, InfoReplySubmessage, PadSubmessage>;

// =============================================================================
// Submessage Wrapper
// =============================================================================

/// @brief Parsed submessage with header and optional body
struct Submessage {
    SubmessageHeader header;
    SubmessageBody body;
    std::span<const std::byte> raw_data;  ///< Raw submessage data (including header)

    [[nodiscard]] SubmessageKind kind() const { return header.kind; }

    /// @brief Check if body is parsed
    [[nodiscard]] bool has_body() const { return !std::holds_alternative<std::monostate>(body); }

    /// @brief Get body as specific type
    template <typename T>
    [[nodiscard]] const T* as() const {
        return std::get_if<T>(&body);
    }
};

// =============================================================================
// RTPS Header
// =============================================================================

/// @brief RTPS message header (20 bytes)
///
/// Layout:
/// - Bytes 0-3: Protocol identifier "RTPS"
/// - Bytes 4-5: Protocol version (major.minor)
/// - Bytes 6-7: Vendor ID
/// - Bytes 8-19: GUID prefix (12 bytes)
struct RtpsHeader : public IDecodedHeader {
    ProtocolVersion version;
    VendorIdValue vendor_id;
    GuidPrefix guid_prefix;

    /// @brief Parsed submessages
    std::vector<Submessage> submessages;

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "RTPS"; }

    [[nodiscard]] std::size_t header_size() const override { return RTPS_HEADER_SIZE; }

    [[nodiscard]] std::size_t payload_size() const override {
        std::size_t total = 0;
        for (const auto& sub : submessages) {
            total += sub.header.total_size();
        }
        return total;
    }

    [[nodiscard]] std::string to_string() const override { return summary(); }

    /// @brief Get summary string
    [[nodiscard]] std::string summary() const {
        return "RTPS v" + version.to_string() + " from " + vendor_id.to_string() + " (" +
               std::to_string(submessages.size()) + " submessages)";
    }

    /// @brief Get participant GUID
    [[nodiscard]] GUID participant_guid() const { return {guid_prefix, EntityId::participant()}; }

    /// @brief Check if message contains DATA submessages
    [[nodiscard]] bool has_data() const {
        for (const auto& sub : submessages) {
            if (sub.kind() == SubmessageKind::DATA || sub.kind() == SubmessageKind::DATA_FRAG) {
                return true;
            }
        }
        return false;
    }

    /// @brief Check if message contains discovery data
    [[nodiscard]] bool is_discovery() const {
        for (const auto& sub : submessages) {
            if (auto* data = sub.as<DataSubmessage>()) {
                if (data->writer_id.is_builtin()) {
                    return true;
                }
            }
        }
        return false;
    }

    /// @brief Count submessages of a given kind
    [[nodiscard]] std::size_t count_submessages(SubmessageKind kind) const {
        std::size_t count = 0;
        for (const auto& sub : submessages) {
            if (sub.kind() == kind)
                ++count;
        }
        return count;
    }
};

// =============================================================================
// RTPS Decoder
// =============================================================================

/// @brief RTPS protocol decoder
class RtpsDecoder : public DecoderBase<RtpsDecoder, RtpsHeader> {
public:
    using ResultType = DecodeResultT<RtpsHeader>;

    // IProtocolDecoder interface
    [[nodiscard]] std::string_view name() const override { return "RTPS"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        return looks_like_rtps(ctx.data);
    }

    /// @brief Decode RTPS message from raw bytes
    /// @param data Raw packet data starting at RTPS header
    /// @return Decoded header or error
    [[nodiscard]] ResultType decode(std::span<const std::byte> data) const;

    /// @brief Decode from context (for dispatcher integration)
    [[nodiscard]] ResultType decode(DecodeContext& ctx) const { return decode(ctx.data); }

    /// @brief Check if data looks like RTPS
    [[nodiscard]] static bool looks_like_rtps(std::span<const std::byte> data);

    /// @brief Protocol name for identification
    [[nodiscard]] static constexpr std::string_view protocol_name() { return "RTPS"; }

private:
    /// @brief Parse submessages from data after RTPS header
    [[nodiscard]] std::vector<Submessage> parse_submessages(std::span<const std::byte> data) const;

    /// @brief Parse a single submessage body
    [[nodiscard]] SubmessageBody parse_submessage_body(SubmessageKind kind, SubmessageFlags flags,
                                                       std::span<const std::byte> data) const;
};

}  // namespace wadjet::protocols::dds

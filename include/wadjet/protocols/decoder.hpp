#pragma once

/// @file decoder.hpp
/// @brief Protocol decoder framework - interfaces, concepts, and utilities

#include "wadjet/core/byte_order.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/core/types.hpp"
#include "wadjet/net/packet_view.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wadjet::protocols {

/// @brief Error codes for decoding failures
enum class DecodeErrorCode {
    Success = 0,
    BufferTooSmall,      ///< Not enough data for header
    InvalidHeader,       ///< Header validation failed
    InvalidChecksum,     ///< Checksum mismatch
    InvalidVersion,      ///< Unsupported protocol version
    InvalidLength,       ///< Length field inconsistent
    MalformedData,       ///< Data structure is malformed
    UnsupportedProtocol, ///< Protocol not recognized
    TruncatedPayload,    ///< Payload truncated
};

/// @brief Convert error code to string
[[nodiscard]] constexpr std::string_view decode_error_string(DecodeErrorCode code) {
    switch (code) {
        case DecodeErrorCode::Success: return "Success";
        case DecodeErrorCode::BufferTooSmall: return "Buffer too small";
        case DecodeErrorCode::InvalidHeader: return "Invalid header";
        case DecodeErrorCode::InvalidChecksum: return "Invalid checksum";
        case DecodeErrorCode::InvalidVersion: return "Invalid version";
        case DecodeErrorCode::InvalidLength: return "Invalid length";
        case DecodeErrorCode::MalformedData: return "Malformed data";
        case DecodeErrorCode::UnsupportedProtocol: return "Unsupported protocol";
        case DecodeErrorCode::TruncatedPayload: return "Truncated payload";
    }
    return "Unknown error";
}

/// @brief Decode error with context
struct DecodeError {
    DecodeErrorCode code;
    std::size_t offset;      ///< Byte offset where error occurred
    std::string message;

    DecodeError() : code(DecodeErrorCode::Success), offset(0) {}

    DecodeError(DecodeErrorCode c, std::size_t off, std::string msg = "")
        : code(c), offset(off), message(std::move(msg)) {
        if (message.empty()) {
            message = std::string(decode_error_string(c));
        }
    }

    [[nodiscard]] bool is_ok() const { return code == DecodeErrorCode::Success; }
    [[nodiscard]] explicit operator bool() const { return !is_ok(); }
};

/// @brief Context passed through decoder chain
///
/// Contains the remaining data to decode plus metadata accumulated
/// from previous decoder stages (e.g., encapsulating protocol info).
struct DecodeContext {
    std::span<const std::byte> data;     ///< Remaining data to decode
    std::size_t original_offset = 0;     ///< Offset from original packet start
    Timestamp timestamp;                  ///< Packet timestamp

    /// @brief Protocol-specific metadata from previous layers
    struct LayerInfo {
        std::uint16_t ethertype = 0;     ///< Ethertype from Ethernet layer
        std::uint8_t ip_protocol = 0;    ///< Protocol from IP layer
        std::uint16_t src_port = 0;      ///< Source port from transport layer
        std::uint16_t dst_port = 0;      ///< Destination port from transport layer
    } layer_info;

    /// @brief Create context from packet view
    static DecodeContext from_packet(const PacketView& view) {
        DecodeContext ctx;
        ctx.data = view.data();
        ctx.timestamp = view.timestamp();
        return ctx;
    }

    /// @brief Create sub-context for payload
    [[nodiscard]] DecodeContext sub_context(std::size_t header_size) const {
        DecodeContext ctx = *this;
        if (header_size <= data.size()) {
            ctx.data = data.subspan(header_size);
            ctx.original_offset = original_offset + header_size;
        } else {
            ctx.data = {};
            ctx.original_offset = original_offset + data.size();
        }
        return ctx;
    }

    /// @brief Check if enough data remains
    [[nodiscard]] bool has_bytes(std::size_t n) const {
        return data.size() >= n;
    }

    /// @brief Read 16-bit value at offset (network byte order)
    [[nodiscard]] std::uint16_t read_be16(std::size_t offset) const {
        if (offset + 2 <= data.size()) {
            return wadjet::read_be16(data.data() + offset);
        }
        return 0;
    }

    /// @brief Read 32-bit value at offset (network byte order)
    [[nodiscard]] std::uint32_t read_be32(std::size_t offset) const {
        if (offset + 4 <= data.size()) {
            return wadjet::read_be32(data.data() + offset);
        }
        return 0;
    }

    /// @brief Read byte at offset
    [[nodiscard]] std::uint8_t read_u8(std::size_t offset) const {
        if (offset < data.size()) {
            return static_cast<std::uint8_t>(data[offset]);
        }
        return 0;
    }

    /// @brief Read raw bytes at offset
    [[nodiscard]] std::span<const std::byte> read_bytes(std::size_t offset,
                                                         std::size_t len) const {
        if (offset + len <= data.size()) {
            return data.subspan(offset, len);
        }
        return {};
    }
};

// Forward declaration
class IDecodedHeader;

/// @brief Result of a decode operation
template <typename HeaderT>
struct DecodeResultT {
    std::optional<HeaderT> header_;     ///< Decoded header (if success)
    DecodeContext next_context;         ///< Context for next decoder
    DecodeError error_;                 ///< Error info

    [[nodiscard]] bool is_ok() const { return error_.is_ok(); }
    [[nodiscard]] explicit operator bool() const { return is_ok(); }

    /// @brief Get error (for expected-like interface)
    [[nodiscard]] const DecodeError& error() const { return error_; }

    /// @brief Get header value (only valid if is_ok())
    [[nodiscard]] const HeaderT& value() const { return *header_; }
    [[nodiscard]] HeaderT& value() { return *header_; }

    /// @brief Dereference operator (for expected-like interface)
    [[nodiscard]] const HeaderT& operator*() const { return *header_; }
    [[nodiscard]] HeaderT& operator*() { return *header_; }

    /// @brief Arrow operator
    [[nodiscard]] const HeaderT* operator->() const { return &(*header_); }
    [[nodiscard]] HeaderT* operator->() { return &(*header_); }
};

/// @brief Abstract interface for decoded headers
///
/// Provides common operations for all decoded protocol headers.
class IDecodedHeader {
public:
    virtual ~IDecodedHeader() = default;

    /// @brief Get protocol name
    [[nodiscard]] virtual std::string_view protocol_name() const = 0;

    /// @brief Get header size in bytes
    [[nodiscard]] virtual std::size_t header_size() const = 0;

    /// @brief Get payload size (may be 0 if no payload or unknown)
    [[nodiscard]] virtual std::size_t payload_size() const = 0;

    /// @brief Get string representation for debugging
    [[nodiscard]] virtual std::string to_string() const = 0;

    // Prevent copying
    IDecodedHeader() = default;
    IDecodedHeader(const IDecodedHeader&) = default;
    IDecodedHeader& operator=(const IDecodedHeader&) = default;
    IDecodedHeader(IDecodedHeader&&) = default;
    IDecodedHeader& operator=(IDecodedHeader&&) = default;
};

/// @brief Abstract interface for protocol decoders
///
/// Decoders are stateless objects that parse protocol headers from
/// byte buffers. Each decoder handles one protocol layer.
class IProtocolDecoder {
public:
    virtual ~IProtocolDecoder() = default;

    /// @brief Get protocol name
    [[nodiscard]] virtual std::string_view name() const = 0;

    /// @brief Check if this decoder can handle the data
    /// @param ctx Decode context with layer info and data
    /// @return true if this decoder should attempt to decode
    [[nodiscard]] virtual bool can_decode(const DecodeContext& ctx) const = 0;

    // Prevent copying
    IProtocolDecoder() = default;
    IProtocolDecoder(const IProtocolDecoder&) = delete;
    IProtocolDecoder& operator=(const IProtocolDecoder&) = delete;
    IProtocolDecoder(IProtocolDecoder&&) = default;
    IProtocolDecoder& operator=(IProtocolDecoder&&) = default;
};

/// @brief CRTP base for protocol decoders with common utilities
template <typename Derived, typename HeaderT>
class DecoderBase : public IProtocolDecoder {
public:
    using Header = HeaderT;
    using Result = DecodeResultT<HeaderT>;

    /// @brief Decode header from context
    [[nodiscard]] Result decode(const DecodeContext& ctx) const {
        return static_cast<const Derived*>(this)->decode_impl(ctx);
    }

protected:
    /// @brief Create error result
    static Result make_error(DecodeErrorCode code, std::string msg = "",
                             std::size_t offset = 0) {
        Result result;
        result.error_ = DecodeError(code, offset, std::move(msg));
        return result;
    }

    /// @brief Create success result
    static Result make_success(HeaderT header, DecodeContext next_ctx) {
        Result result;
        result.header_ = std::move(header);
        result.next_context = std::move(next_ctx);
        result.error_ = DecodeError();
        return result;
    }
};

/// @brief C++20 concept for protocol decoders
template <typename T>
concept ProtocolDecoderConcept = requires(const T& decoder, const DecodeContext& ctx) {
    { decoder.name() } -> std::convertible_to<std::string_view>;
    { decoder.can_decode(ctx) } -> std::same_as<bool>;
    { decoder.decode(ctx) };
};

/// @brief Unique pointer to decoder
using DecoderPtr = std::unique_ptr<IProtocolDecoder>;

}  // namespace wadjet::protocols

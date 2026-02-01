#pragma once

/// @file someip.hpp
/// @brief SOME/IP protocol decoder (AUTOSAR Scalable service-Oriented MiddlewarE over IP)

#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

namespace wadjet::protocols::someip {

/// @brief SOME/IP header size (16 bytes)
inline constexpr std::size_t HEADER_SIZE = 16;

/// @brief SOME/IP protocol version
inline constexpr std::uint8_t PROTOCOL_VERSION = 1;

/// @brief SOME/IP Service Discovery service ID
inline constexpr std::uint16_t SD_SERVICE_ID = 0xFFFF;

/// @brief SOME/IP Service Discovery method ID
inline constexpr std::uint16_t SD_METHOD_ID = 0x8100;

/// @brief SOME/IP message types
enum class MessageType : std::uint8_t {
    Request = 0x00,
    RequestNoReturn = 0x01,
    Notification = 0x02,
    RequestAck = 0x40,          ///< Request with acknowledgment
    RequestNoReturnAck = 0x41,  ///< Request no return with acknowledgment
    NotificationAck = 0x42,     ///< Notification with acknowledgment
    Response = 0x80,
    Error = 0x81,
    ResponseAck = 0xC0,
    ErrorAck = 0xC1,
};

/// @brief Convert message type to string
[[nodiscard]] constexpr std::string_view message_type_string(MessageType type) {
    switch (type) {
        case MessageType::Request:
            return "Request";
        case MessageType::RequestNoReturn:
            return "RequestNoReturn";
        case MessageType::Notification:
            return "Notification";
        case MessageType::RequestAck:
            return "RequestAck";
        case MessageType::RequestNoReturnAck:
            return "RequestNoReturnAck";
        case MessageType::NotificationAck:
            return "NotificationAck";
        case MessageType::Response:
            return "Response";
        case MessageType::Error:
            return "Error";
        case MessageType::ResponseAck:
            return "ResponseAck";
        case MessageType::ErrorAck:
            return "ErrorAck";
    }
    return "Unknown";
}

/// @brief SOME/IP return codes
enum class ReturnCode : std::uint8_t {
    Ok = 0x00,
    NotOk = 0x01,
    UnknownService = 0x02,
    UnknownMethod = 0x03,
    NotReady = 0x04,
    NotReachable = 0x05,
    Timeout = 0x06,
    WrongProtocolVersion = 0x07,
    WrongInterfaceVersion = 0x08,
    MalformedMessage = 0x09,
    WrongMessageType = 0x0A,
    // E2E return codes: 0x0B-0x1F
    // Reserved: 0x20-0x5E
    // Application specific: 0x40-0x5E
};

/// @brief Convert return code to string
[[nodiscard]] constexpr std::string_view return_code_string(ReturnCode code) {
    switch (code) {
        case ReturnCode::Ok:
            return "OK";
        case ReturnCode::NotOk:
            return "NOT_OK";
        case ReturnCode::UnknownService:
            return "UNKNOWN_SERVICE";
        case ReturnCode::UnknownMethod:
            return "UNKNOWN_METHOD";
        case ReturnCode::NotReady:
            return "NOT_READY";
        case ReturnCode::NotReachable:
            return "NOT_REACHABLE";
        case ReturnCode::Timeout:
            return "TIMEOUT";
        case ReturnCode::WrongProtocolVersion:
            return "WRONG_PROTOCOL_VERSION";
        case ReturnCode::WrongInterfaceVersion:
            return "WRONG_INTERFACE_VERSION";
        case ReturnCode::MalformedMessage:
            return "MALFORMED_MESSAGE";
        case ReturnCode::WrongMessageType:
            return "WRONG_MESSAGE_TYPE";
    }
    return "Unknown";
}

/// @brief Decoded SOME/IP header
struct SomeIpHeader : public IDecodedHeader {
    // Message ID (32 bits)
    std::uint16_t service_id = 0;  ///< Service ID (16 bits)
    std::uint16_t method_id = 0;   ///< Method ID / Event ID (16 bits)

    std::uint32_t length = 0;  ///< Length (including header from Request ID)

    // Request ID (32 bits)
    std::uint16_t client_id = 0;   ///< Client ID (16 bits)
    std::uint16_t session_id = 0;  ///< Session ID (16 bits)

    std::uint8_t protocol_version = 0;   ///< Protocol version
    std::uint8_t interface_version = 0;  ///< Interface version
    MessageType message_type = MessageType::Request;
    ReturnCode return_code = ReturnCode::Ok;

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "SOME/IP"; }

    [[nodiscard]] std::size_t header_size() const override { return HEADER_SIZE; }

    [[nodiscard]] std::size_t payload_size() const override {
        // Length field includes 8 bytes (from Request ID onwards)
        if (length >= 8) {
            return length - 8;
        }
        return 0;
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Get combined Message ID
    [[nodiscard]] std::uint32_t message_id() const {
        return (static_cast<std::uint32_t>(service_id) << 16) | method_id;
    }

    /// @brief Get combined Request ID
    [[nodiscard]] std::uint32_t request_id() const {
        return (static_cast<std::uint32_t>(client_id) << 16) | session_id;
    }

    /// @brief Check if this is a Service Discovery message
    [[nodiscard]] bool is_service_discovery() const {
        return service_id == SD_SERVICE_ID && method_id == SD_METHOD_ID;
    }

    /// @brief Check if this is a request
    [[nodiscard]] bool is_request() const {
        return message_type == MessageType::Request ||
               message_type == MessageType::RequestNoReturn ||
               message_type == MessageType::RequestAck ||
               message_type == MessageType::RequestNoReturnAck;
    }

    /// @brief Check if this is a response
    [[nodiscard]] bool is_response() const {
        return message_type == MessageType::Response || message_type == MessageType::ResponseAck;
    }

    /// @brief Check if this is an error
    [[nodiscard]] bool is_error() const {
        return message_type == MessageType::Error || message_type == MessageType::ErrorAck;
    }

    /// @brief Check if this is a notification
    [[nodiscard]] bool is_notification() const {
        return message_type == MessageType::Notification ||
               message_type == MessageType::NotificationAck;
    }

    /// @brief Check if method ID is an event (MSB set)
    [[nodiscard]] bool is_event() const { return (method_id & 0x8000) != 0; }
};

/// @brief SOME/IP Transport Protocol (TP) header flags
constexpr std::uint8_t TP_FLAG = 0x20;  ///< TP flag in message type byte
constexpr std::uint8_t TP_MORE_SEGMENTS = 0x01;  ///< More segments flag in TP header

/// @brief SOME/IP-TP (Transport Protocol) segment information
struct SomeipTpSegment {
    std::uint32_t offset = 0;      ///< Segment offset in bytes
    bool more_segments = false;    ///< True if more segments follow
    
    /// @brief Parse TP header from raw bytes
    [[nodiscard]] static SomeipTpSegment parse(const void* tp_data) {
        SomeipTpSegment seg;
        const auto* ptr = static_cast<const std::uint8_t*>(tp_data);
        
        // First byte: reserved (7 bits) + more_segments flag (1 bit)
        seg.more_segments = (ptr[0] & TP_MORE_SEGMENTS) != 0;
        
        // Offset in 4-byte units (3 bytes, big-endian)
        std::uint32_t offset_units = (static_cast<std::uint32_t>(ptr[1]) << 16) |
                                     (static_cast<std::uint32_t>(ptr[2]) << 8) |
                                     ptr[3];
        seg.offset = offset_units * 4;
        
        return seg;
    }
};

/// @brief SOME/IP-TP reassembly state for a message
struct SomeipTpMessage {
    std::uint32_t message_id = 0;          ///< Combined (service_id << 16) | method_id
    std::uint32_t request_id = 0;          ///< Combined (client_id << 16) | session_id
    std::uint32_t total_length = 0;        ///< Total message length (from first segment)
    std::vector<std::uint8_t> data;        ///< Reassembled message data
    std::uint64_t last_update_time = 0;    ///< Timestamp of last segment received
    
    /// @brief Check if message is complete
    [[nodiscard]] bool is_complete() const {
        return data.size() >= total_length && total_length > 0;
    }
    
    /// @brief Add segment to reassembly buffer
    void add_segment(std::uint32_t offset, const std::uint8_t* segment_data, std::size_t segment_size) {
        // Ensure buffer is large enough
        if (offset + segment_size > data.size()) {
            data.resize(offset + segment_size);
        }
        
        // Copy segment data
        std::memcpy(data.data() + offset, segment_data, segment_size);
    }
};

/// @brief SOME/IP-TP (Transport Protocol) reassembler for large messages
class SomeipTpReassembler {
public:
    /// @brief Maximum message size (16 MB)
    static constexpr std::uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;
    
    /// @brief Timeout for incomplete messages (5 seconds)
    static constexpr std::uint64_t TIMEOUT_MS = 5000;
    
    /// @brief Process a TP segment
    /// @param message_id Combined service/method ID
    /// @param request_id Combined client/session ID
    /// @param segment TP segment information (offset, more_segments)
    /// @param data Segment payload data
    /// @param data_size Segment payload size
    /// @return true if message is complete after adding segment
    [[nodiscard]] bool add_segment(std::uint32_t message_id, std::uint32_t request_id,
                                   const SomeipTpSegment& segment,
                                   const std::uint8_t* data, std::size_t data_size);
    
    /// @brief Get completed message
    /// @param message_id Combined service/method ID
    /// @param request_id Combined client/session ID
    /// @return Pointer to completed message, or nullptr if not complete
    [[nodiscard]] const SomeipTpMessage* get_message(std::uint32_t message_id, std::uint32_t request_id) const;
    
    /// @brief Remove completed message
    void remove_message(std::uint32_t message_id, std::uint32_t request_id);
    
    /// @brief Clear timed-out messages
    /// @param current_time_ms Current time in milliseconds
    void cleanup_timed_out(std::uint64_t current_time_ms);
    
private:
    // Key: (message_id, request_id)
    std::map<std::pair<std::uint32_t, std::uint32_t>, SomeipTpMessage> messages_;
};

/// @brief SOME/IP decoder
class SomeIpDecoder : public DecoderBase<SomeIpDecoder, SomeIpHeader> {
public:
    /// @brief Decoder options
    struct Options {
        bool validate_protocol_version;  ///< Check protocol version is 1
        bool allow_invalid_version;      ///< Continue even if version invalid
        bool enable_tp_reassembly;       ///< Enable SOME/IP-TP message reassembly
        Options() : validate_protocol_version(true), allow_invalid_version(false), enable_tp_reassembly(true) {}
    };

    explicit SomeIpDecoder(Options opts = Options()) : options_(opts) {}

    [[nodiscard]] std::string_view name() const override { return "SOME/IP"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        // SOME/IP typically runs over UDP, but we can't check port without more info
        // Just check if we have enough data
        return ctx.has_bytes(HEADER_SIZE);
    }

    /// @brief Decode SOME/IP header
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

private:
    Options options_;
    mutable SomeipTpReassembler tp_reassembler_;  ///< TP message reassembler (mutable for const decode)
};

/// @brief Global SOME/IP decoder instance
inline const SomeIpDecoder& someip_decoder() {
    static SomeIpDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::someip

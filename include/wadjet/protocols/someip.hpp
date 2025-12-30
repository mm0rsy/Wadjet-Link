#pragma once

/// @file someip.hpp
/// @brief SOME/IP protocol decoder (AUTOSAR Scalable service-Oriented MiddlewarE over IP)

#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <string>

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
    RequestAck = 0x40,            ///< Request with acknowledgment
    RequestNoReturnAck = 0x41,    ///< Request no return with acknowledgment
    NotificationAck = 0x42,       ///< Notification with acknowledgment
    Response = 0x80,
    Error = 0x81,
    ResponseAck = 0xC0,
    ErrorAck = 0xC1,
};

/// @brief Convert message type to string
[[nodiscard]] constexpr std::string_view message_type_string(MessageType type) {
    switch (type) {
        case MessageType::Request: return "Request";
        case MessageType::RequestNoReturn: return "RequestNoReturn";
        case MessageType::Notification: return "Notification";
        case MessageType::RequestAck: return "RequestAck";
        case MessageType::RequestNoReturnAck: return "RequestNoReturnAck";
        case MessageType::NotificationAck: return "NotificationAck";
        case MessageType::Response: return "Response";
        case MessageType::Error: return "Error";
        case MessageType::ResponseAck: return "ResponseAck";
        case MessageType::ErrorAck: return "ErrorAck";
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
        case ReturnCode::Ok: return "OK";
        case ReturnCode::NotOk: return "NOT_OK";
        case ReturnCode::UnknownService: return "UNKNOWN_SERVICE";
        case ReturnCode::UnknownMethod: return "UNKNOWN_METHOD";
        case ReturnCode::NotReady: return "NOT_READY";
        case ReturnCode::NotReachable: return "NOT_REACHABLE";
        case ReturnCode::Timeout: return "TIMEOUT";
        case ReturnCode::WrongProtocolVersion: return "WRONG_PROTOCOL_VERSION";
        case ReturnCode::WrongInterfaceVersion: return "WRONG_INTERFACE_VERSION";
        case ReturnCode::MalformedMessage: return "MALFORMED_MESSAGE";
        case ReturnCode::WrongMessageType: return "WRONG_MESSAGE_TYPE";
    }
    return "Unknown";
}

/// @brief Decoded SOME/IP header
struct SomeIpHeader : public IDecodedHeader {
    // Message ID (32 bits)
    std::uint16_t service_id = 0;    ///< Service ID (16 bits)
    std::uint16_t method_id = 0;     ///< Method ID / Event ID (16 bits)

    std::uint32_t length = 0;        ///< Length (including header from Request ID)

    // Request ID (32 bits)
    std::uint16_t client_id = 0;     ///< Client ID (16 bits)
    std::uint16_t session_id = 0;    ///< Session ID (16 bits)

    std::uint8_t protocol_version = 0; ///< Protocol version
    std::uint8_t interface_version = 0; ///< Interface version
    MessageType message_type = MessageType::Request;
    ReturnCode return_code = ReturnCode::Ok;

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override {
        return "SOME/IP";
    }

    [[nodiscard]] std::size_t header_size() const override {
        return HEADER_SIZE;
    }

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
        return message_type == MessageType::Response ||
               message_type == MessageType::ResponseAck;
    }

    /// @brief Check if this is an error
    [[nodiscard]] bool is_error() const {
        return message_type == MessageType::Error ||
               message_type == MessageType::ErrorAck;
    }

    /// @brief Check if this is a notification
    [[nodiscard]] bool is_notification() const {
        return message_type == MessageType::Notification ||
               message_type == MessageType::NotificationAck;
    }

    /// @brief Check if method ID is an event (MSB set)
    [[nodiscard]] bool is_event() const {
        return (method_id & 0x8000) != 0;
    }
};

/// @brief SOME/IP decoder
class SomeIpDecoder : public DecoderBase<SomeIpDecoder, SomeIpHeader> {
public:
    /// @brief Decoder options
    struct Options {
        bool validate_protocol_version;  ///< Check protocol version is 1
        bool allow_invalid_version;     ///< Continue even if version invalid
        Options() : validate_protocol_version(true), allow_invalid_version(false) {}
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
};

/// @brief Global SOME/IP decoder instance
inline const SomeIpDecoder& someip_decoder() {
    static SomeIpDecoder decoder;
    return decoder;
}

}  // namespace wadjet::protocols::someip

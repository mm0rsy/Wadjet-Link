/// @file someip.cpp
/// @brief SOME/IP protocol decoder implementation

#include "wadjet/protocols/someip.hpp"

#include <iomanip>
#include <sstream>

namespace wadjet::protocols::someip {

std::string SomeIpHeader::to_string() const {
    std::ostringstream oss;
    oss << "SOME/IP { service=0x" << std::hex << std::setfill('0') << std::setw(4) << service_id
        << ", method=0x" << std::setw(4) << method_id << ", client=0x" << std::setw(4) << client_id
        << ", session=0x" << std::setw(4) << session_id << std::dec
        << ", type=" << message_type_string(message_type)
        << ", ret=" << return_code_string(return_code) << ", len=" << length;

    if (is_service_discovery()) {
        oss << " [SD]";
    }
    oss << " }";
    return oss.str();
}

bool SomeipTpReassembler::add_segment(std::uint32_t message_id, std::uint32_t request_id,
                                      const SomeipTpSegment& segment,
                                      const std::uint8_t* data, std::size_t data_size) {
    // Validate offset is within bounds
    if (segment.offset > MAX_MESSAGE_SIZE) {
        return false;
    }

    // Validate segment doesn't exceed max size
    if (segment.offset + data_size > MAX_MESSAGE_SIZE) {
        return false;
    }

    // Create key for this message
    auto key = std::make_pair(message_id, request_id);

    // Find or create message
    auto it = messages_.find(key);
    if (it == messages_.end()) {
        // First segment - initialize message
        auto& msg = messages_[key];
        msg.message_id = message_id;
        msg.request_id = request_id;
        msg.total_length = segment.offset + static_cast<std::uint32_t>(data_size);
        msg.last_update_time = 0;  // Timestamp will be set by caller if needed
        msg.add_segment(segment.offset, data, data_size);
    } else {
        // Subsequent segment - add to existing message
        auto& msg = it->second;
        
        // Update total_length if this segment extends further
        std::uint32_t segment_end = segment.offset + static_cast<std::uint32_t>(data_size);
        if (segment_end > msg.total_length) {
            msg.total_length = segment_end;
        }
        
        msg.last_update_time = 0;  // Reset/update timestamp if needed
        msg.add_segment(segment.offset, data, data_size);
    }

    // Check if message is complete
    return messages_[key].is_complete();
}

const SomeipTpMessage* SomeipTpReassembler::get_message(std::uint32_t message_id, std::uint32_t request_id) const {
    auto key = std::make_pair(message_id, request_id);
    auto it = messages_.find(key);
    
    if (it != messages_.end() && it->second.is_complete()) {
        return &it->second;
    }
    
    return nullptr;
}

void SomeipTpReassembler::remove_message(std::uint32_t message_id, std::uint32_t request_id) {
    auto key = std::make_pair(message_id, request_id);
    messages_.erase(key);
}

void SomeipTpReassembler::cleanup_timed_out(std::uint64_t current_time_ms) {
    // Remove messages that have exceeded the timeout
    for (auto it = messages_.begin(); it != messages_.end();) {
        auto& msg = it->second;
        
        // Check if message has timed out
        if (current_time_ms > msg.last_update_time &&
            (current_time_ms - msg.last_update_time) > TIMEOUT_MS) {
            it = messages_.erase(it);
        } else {
            ++it;
        }
    }
}

SomeIpDecoder::Result SomeIpDecoder::decode_impl(const DecodeContext& ctx) const {
    // Check minimum size
    if (!ctx.has_bytes(HEADER_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "SOME/IP header too small");
    }

    SomeIpHeader header;

    // Service ID (bytes 0-1)
    header.service_id = ctx.read_be16(0);

    // Method ID (bytes 2-3)
    header.method_id = ctx.read_be16(2);

    // Length (bytes 4-7) - includes 8 bytes from Request ID onwards
    header.length = ctx.read_be32(4);

    // Client ID (bytes 8-9)
    header.client_id = ctx.read_be16(8);

    // Session ID (bytes 10-11)
    header.session_id = ctx.read_be16(10);

    // Protocol version (byte 12)
    header.protocol_version = static_cast<std::uint8_t>(ctx.data[12]);

    // Validate protocol version
    if (options_.validate_protocol_version && header.protocol_version != PROTOCOL_VERSION) {
        if (!options_.allow_invalid_version) {
            return make_error(DecodeErrorCode::InvalidVersion,
                              "SOME/IP protocol version " +
                                  std::to_string(header.protocol_version) + " (expected " +
                                  std::to_string(PROTOCOL_VERSION) + ")");
        }
    }

    // Interface version (byte 13)
    header.interface_version = static_cast<std::uint8_t>(ctx.data[13]);

    // Message type (byte 14)
    std::uint8_t message_type_byte = static_cast<std::uint8_t>(ctx.data[14]);
    header.message_type = static_cast<MessageType>(message_type_byte);

    // Return code (byte 15)
    header.return_code = static_cast<ReturnCode>(static_cast<std::uint8_t>(ctx.data[15]));

    // Validate length field
    if (header.length < 8) {
        return make_error(DecodeErrorCode::InvalidLength,
                          "SOME/IP length too small: " + std::to_string(header.length));
    }

    // Check for TP flag (0x20) in message type byte
    if (options_.enable_tp_reassembly && (message_type_byte & TP_FLAG) != 0) {
        // This is a TP message - check if we have the TP header (4 bytes)
        if (!ctx.has_bytes(HEADER_SIZE + 4)) {
            return make_error(DecodeErrorCode::BufferTooSmall, "SOME/IP-TP header too small");
        }

        // Parse TP header
        SomeipTpSegment tp_segment = SomeipTpSegment::parse(ctx.data.data() + HEADER_SIZE);

        // Get message IDs
        std::uint32_t message_id = (static_cast<std::uint32_t>(header.service_id) << 16) |
                                   static_cast<std::uint32_t>(header.method_id);
        std::uint32_t request_id = (static_cast<std::uint32_t>(header.client_id) << 16) |
                                   static_cast<std::uint32_t>(header.session_id);

        // Get payload (skip SOME/IP header + TP header)
        const std::uint8_t* payload = reinterpret_cast<const std::uint8_t*>(ctx.data.data()) + HEADER_SIZE + 4;
        std::size_t payload_size = header.length > 12 ? header.length - 12 : 0;

        // Add segment to reassembler
        [[maybe_unused]] bool complete = tp_reassembler_.add_segment(message_id, request_id, tp_segment, payload, payload_size);
        // Note: complete flag indicates if the message is fully reassembled
    }

    // Create context for payload
    auto next_ctx = ctx.sub_context(HEADER_SIZE);

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::someip

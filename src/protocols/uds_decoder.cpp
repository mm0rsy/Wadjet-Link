/// @file uds_decoder.cpp
/// @brief UDS (ISO 14229) protocol decoder implementation

#include "wadjet/protocols/uds/uds.hpp"

#include <cstring>
#include <sstream>

namespace wadjet::protocols::uds {

namespace {

/// @brief Read big-endian uint16 from buffer
[[nodiscard]] inline std::uint16_t read_be16(const std::byte* data) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8) |
         static_cast<std::uint16_t>(data[1]));
}

/// @brief Read big-endian uint32 from buffer
[[nodiscard]] inline std::uint32_t read_be32(const std::byte* data) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(data[0]) << 24) |
        (static_cast<std::uint32_t>(data[1]) << 16) |
        (static_cast<std::uint32_t>(data[2]) << 8) |
         static_cast<std::uint32_t>(data[3]));
}

/// @brief Read variable-length big-endian integer
[[nodiscard]] inline std::uint64_t read_be_var(const std::byte* data, std::size_t len) {
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < len; ++i) {
        result = (result << 8) | static_cast<std::uint64_t>(data[i]);
    }
    return result;
}

/// @brief Convert span to vector
[[nodiscard]] inline std::vector<std::uint8_t> to_vector(std::span<const std::byte> data) {
    std::vector<std::uint8_t> result;
    result.reserve(data.size());
    for (auto b : data) {
        result.push_back(static_cast<std::uint8_t>(b));
    }
    return result;
}

}  // anonymous namespace

// =============================================================================
// UdsDecoder Implementation
// =============================================================================

UdsDecoder::Result UdsDecoder::decode(std::span<const std::byte> data) const {
    if (data.size() < MIN_MESSAGE_SIZE) {
        return Result::err(UdsDecodeError::make(
            UdsDecodeError::Code::MessageTooShort,
            "Message too short for UDS"));
    }

    auto sid = static_cast<std::uint8_t>(data[0]);

    if (sid == NEGATIVE_RESPONSE_SID) {
        return parse_negative_response(data);
    } else if ((sid & 0x40) != 0) {
        return parse_positive_response(data);
    } else {
        return parse_request(data);
    }
}

UdsDecoder::Result UdsDecoder::parse_request(std::span<const std::byte> data) const {
    auto sid = static_cast<ServiceID>(data[0]);

    UdsDecodeResult result;
    result.header.service_id = sid;
    result.header.direction = MessageDirection::Request;
    result.header.raw_data = data;

    // Parse sub-function if present
    if (service_has_sub_function(sid) && data.size() >= 2) {
        auto sub_byte = static_cast<std::uint8_t>(data[1]);
        result.header.sub_function = extract_sub_function(sub_byte);
        result.header.suppress_positive_response = extract_suppress_positive_response(sub_byte);
        result.header.service_data = data.subspan(2);
    } else {
        result.header.service_data = data.subspan(1);
    }

    // Parse service-specific message
    switch (sid) {
        case ServiceID::DiagnosticSessionControl:
            result.message = parse_diagnostic_session_control_request(data);
            break;
        case ServiceID::ECUReset:
            result.message = parse_ecu_reset_request(data);
            break;
        case ServiceID::SecurityAccess:
            result.message = parse_security_access_request(data);
            break;
        case ServiceID::TesterPresent:
            result.message = parse_tester_present_request(data);
            break;
        case ServiceID::ReadDataByIdentifier:
            result.message = parse_read_data_by_identifier_request(data);
            break;
        case ServiceID::WriteDataByIdentifier:
            result.message = parse_write_data_by_identifier_request(data);
            break;
        case ServiceID::RoutineControl:
            result.message = parse_routine_control_request(data);
            break;
        case ServiceID::RequestDownload:
            result.message = parse_request_download_request(data);
            break;
        case ServiceID::TransferData:
            result.message = parse_transfer_data_request(data);
            break;
        case ServiceID::RequestTransferExit:
            result.message = parse_request_transfer_exit_request(data);
            break;
        default:
            result.message = std::monostate{};
            break;
    }

    return Result::ok(std::move(result));
}

UdsDecoder::Result UdsDecoder::parse_positive_response(std::span<const std::byte> data) const {
    auto response_sid = static_cast<std::uint8_t>(data[0]);
    auto request_sid = static_cast<ServiceID>(response_sid - RESPONSE_SID_OFFSET);

    UdsDecodeResult result;
    result.header.service_id = request_sid;
    result.header.direction = MessageDirection::PositiveResponse;
    result.header.raw_data = data;

    // Parse sub-function echo if present
    if (service_has_sub_function(request_sid) && data.size() >= 2) {
        result.header.sub_function = extract_sub_function(static_cast<std::uint8_t>(data[1]));
        result.header.service_data = data.subspan(2);
    } else {
        result.header.service_data = data.subspan(1);
    }

    // Parse service-specific response
    switch (request_sid) {
        case ServiceID::DiagnosticSessionControl:
            result.message = parse_diagnostic_session_control_response(data);
            break;
        case ServiceID::ECUReset:
            result.message = parse_ecu_reset_response(data);
            break;
        case ServiceID::SecurityAccess:
            result.message = parse_security_access_response(data);
            break;
        case ServiceID::TesterPresent:
            result.message = parse_tester_present_response(data);
            break;
        case ServiceID::ReadDataByIdentifier:
            result.message = parse_read_data_by_identifier_response(data);
            break;
        case ServiceID::WriteDataByIdentifier:
            result.message = parse_write_data_by_identifier_response(data);
            break;
        case ServiceID::RoutineControl:
            result.message = parse_routine_control_response(data);
            break;
        case ServiceID::RequestDownload:
            result.message = parse_request_download_response(data);
            break;
        case ServiceID::TransferData:
            result.message = parse_transfer_data_response(data);
            break;
        case ServiceID::RequestTransferExit:
            result.message = parse_request_transfer_exit_response(data);
            break;
        default:
            result.message = std::monostate{};
            break;
    }

    return Result::ok(std::move(result));
}

UdsDecoder::Result UdsDecoder::parse_negative_response(std::span<const std::byte> data) const {
    if (data.size() < 3) {
        return Result::err(UdsDecodeError::make(
            UdsDecodeError::Code::MessageTooShort,
            "Negative response requires 3 bytes"));
    }

    auto rejected_sid = static_cast<ServiceID>(data[1]);
    auto nrc = static_cast<std::uint8_t>(data[2]);

    UdsDecodeResult result;
    result.header.service_id = rejected_sid;
    result.header.direction = MessageDirection::NegativeResponse;
    result.header.rejected_service_id = rejected_sid;
    result.header.negative_response_code = nrc;
    result.header.raw_data = data;
    result.header.service_data = data.subspan(3);

    NegativeResponseMessage msg;
    msg.rejected_service_id = rejected_sid;
    msg.negative_response_code = static_cast<NRC>(nrc);
    result.message = msg;

    return Result::ok(std::move(result));
}

// =============================================================================
// Service-Specific Parsers - Requests
// =============================================================================

UdsServiceMessage UdsDecoder::parse_diagnostic_session_control_request(
    std::span<const std::byte> data) const {
    if (data.size() < DiagnosticSessionControlRequest::min_size()) {
        return std::monostate{};
    }

    DiagnosticSessionControlRequest req;
    auto sub_byte = static_cast<std::uint8_t>(data[1]);
    req.session_type = static_cast<SessionType>(extract_sub_function(sub_byte));
    req.suppress_positive_response = extract_suppress_positive_response(sub_byte);
    return req;
}

UdsServiceMessage UdsDecoder::parse_ecu_reset_request(
    std::span<const std::byte> data) const {
    if (data.size() < ECUResetRequest::min_size()) {
        return std::monostate{};
    }

    ECUResetRequest req;
    auto sub_byte = static_cast<std::uint8_t>(data[1]);
    req.reset_type = static_cast<ResetType>(extract_sub_function(sub_byte));
    req.suppress_positive_response = extract_suppress_positive_response(sub_byte);
    return req;
}

UdsServiceMessage UdsDecoder::parse_security_access_request(
    std::span<const std::byte> data) const {
    if (data.size() < SecurityAccessRequest::min_size()) {
        return std::monostate{};
    }

    SecurityAccessRequest req;
    auto sub_byte = static_cast<std::uint8_t>(data[1]);
    req.access_type = extract_sub_function(sub_byte);
    req.suppress_positive_response = extract_suppress_positive_response(sub_byte);

    // For sendKey requests, remaining bytes are the key
    if (req.is_send_key() && data.size() > 2) {
        req.security_key = to_vector(data.subspan(2));
    }

    return req;
}

UdsServiceMessage UdsDecoder::parse_tester_present_request(
    std::span<const std::byte> data) const {
    if (data.size() < TesterPresentRequest::min_size()) {
        return std::monostate{};
    }

    TesterPresentRequest req;
    auto sub_byte = static_cast<std::uint8_t>(data[1]);
    req.sub_function = extract_sub_function(sub_byte);
    req.suppress_positive_response = extract_suppress_positive_response(sub_byte);
    return req;
}

UdsServiceMessage UdsDecoder::parse_read_data_by_identifier_request(
    std::span<const std::byte> data) const {
    if (data.size() < ReadDataByIdentifierRequest::min_size()) {
        return std::monostate{};
    }

    ReadDataByIdentifierRequest req;

    // Parse DIDs (each DID is 2 bytes)
    for (std::size_t i = 1; i + 1 < data.size(); i += 2) {
        auto did_value = read_be16(data.data() + i);
        req.data_identifiers.push_back(DataIdentifier{did_value});
    }

    return req;
}

UdsServiceMessage UdsDecoder::parse_write_data_by_identifier_request(
    std::span<const std::byte> data) const {
    if (data.size() < WriteDataByIdentifierRequest::min_size()) {
        return std::monostate{};
    }

    WriteDataByIdentifierRequest req;
    req.data_identifier = DataIdentifier{read_be16(data.data() + 1)};

    if (data.size() > 3) {
        req.data_record = to_vector(data.subspan(3));
    }

    return req;
}

UdsServiceMessage UdsDecoder::parse_routine_control_request(
    std::span<const std::byte> data) const {
    if (data.size() < RoutineControlRequest::min_size()) {
        return std::monostate{};
    }

    RoutineControlRequest req;
    auto sub_byte = static_cast<std::uint8_t>(data[1]);
    req.routine_control_type = static_cast<RoutineControlType>(extract_sub_function(sub_byte));
    req.suppress_positive_response = extract_suppress_positive_response(sub_byte);
    req.routine_identifier = RoutineIdentifier{read_be16(data.data() + 2)};

    if (data.size() > 4) {
        req.routine_option_record = to_vector(data.subspan(4));
    }

    return req;
}

UdsServiceMessage UdsDecoder::parse_request_download_request(
    std::span<const std::byte> data) const {
    if (data.size() < RequestDownloadRequest::min_size()) {
        return std::monostate{};
    }

    RequestDownloadRequest req;
    req.data_format = DataFormatIdentifier::from_byte(static_cast<std::uint8_t>(data[1]));
    req.address_and_length_format = AddressAndLengthFormatIdentifier::from_byte(
        static_cast<std::uint8_t>(data[2]));

    auto addr_len = static_cast<std::size_t>(req.address_and_length_format.memory_address_length);
    auto size_len = static_cast<std::size_t>(req.address_and_length_format.memory_size_length);

    if (data.size() >= 3 + addr_len + size_len) {
        req.memory_address = read_be_var(data.data() + 3, addr_len);
        req.memory_size = read_be_var(data.data() + 3 + addr_len, size_len);
    }

    return req;
}

UdsServiceMessage UdsDecoder::parse_transfer_data_request(
    std::span<const std::byte> data) const {
    if (data.size() < TransferDataRequest::min_size()) {
        return std::monostate{};
    }

    TransferDataRequest req;
    req.block_sequence_counter = static_cast<std::uint8_t>(data[1]);

    if (data.size() > 2) {
        req.transfer_request_parameter_record = to_vector(data.subspan(2));
    }

    return req;
}

UdsServiceMessage UdsDecoder::parse_request_transfer_exit_request(
    std::span<const std::byte> data) const {
    RequestTransferExitRequest req;

    if (data.size() > 1) {
        req.transfer_request_parameter_record = to_vector(data.subspan(1));
    }

    return req;
}

// =============================================================================
// Service-Specific Parsers - Responses
// =============================================================================

UdsServiceMessage UdsDecoder::parse_diagnostic_session_control_response(
    std::span<const std::byte> data) const {
    if (data.size() < 2) {
        return std::monostate{};
    }

    DiagnosticSessionControlResponse resp;
    resp.session_type = static_cast<SessionType>(data[1]);

    if (data.size() >= 6) {
        resp.p2_server_max_ms = read_be16(data.data() + 2);
        resp.p2_star_server_max_ms = read_be16(data.data() + 4);
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_ecu_reset_response(
    std::span<const std::byte> data) const {
    if (data.size() < 2) {
        return std::monostate{};
    }

    ECUResetResponse resp;
    resp.reset_type = static_cast<ResetType>(data[1]);

    if (data.size() >= 3 && 
        (resp.reset_type == ResetType::EnableRapidPowerShutDown ||
         resp.reset_type == ResetType::DisableRapidPowerShutDown)) {
        resp.power_down_time = static_cast<std::uint8_t>(data[2]);
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_security_access_response(
    std::span<const std::byte> data) const {
    if (data.size() < 2) {
        return std::monostate{};
    }

    SecurityAccessResponse resp;
    resp.access_type = static_cast<std::uint8_t>(data[1]);

    // For requestSeed responses, remaining bytes are the seed
    if ((resp.access_type & 0x01) != 0 && data.size() > 2) {
        resp.security_seed = to_vector(data.subspan(2));
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_tester_present_response(
    std::span<const std::byte> data) const {
    if (data.size() < 2) {
        return std::monostate{};
    }

    TesterPresentResponse resp;
    resp.sub_function = static_cast<std::uint8_t>(data[1]);
    return resp;
}

UdsServiceMessage UdsDecoder::parse_read_data_by_identifier_response(
    std::span<const std::byte> data) const {
    if (data.size() < 3) {
        return std::monostate{};
    }

    ReadDataByIdentifierResponse resp;

    // Parse DID + data pairs
    // Note: Without knowing DID lengths, we can only parse if there's one DID
    // For full parsing, we'd need a DID database
    if (data.size() >= 3) {
        DataRecord record;
        record.did = DataIdentifier{read_be16(data.data() + 1)};
        if (data.size() > 3) {
            record.data = to_vector(data.subspan(3));
        }
        resp.records.push_back(std::move(record));
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_write_data_by_identifier_response(
    std::span<const std::byte> data) const {
    if (data.size() < 3) {
        return std::monostate{};
    }

    WriteDataByIdentifierResponse resp;
    resp.data_identifier = DataIdentifier{read_be16(data.data() + 1)};
    return resp;
}

UdsServiceMessage UdsDecoder::parse_routine_control_response(
    std::span<const std::byte> data) const {
    if (data.size() < 4) {
        return std::monostate{};
    }

    RoutineControlResponse resp;
    resp.routine_control_type = static_cast<RoutineControlType>(data[1]);
    resp.routine_identifier = RoutineIdentifier{read_be16(data.data() + 2)};

    if (data.size() > 4) {
        resp.routine_status_record = to_vector(data.subspan(4));
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_request_download_response(
    std::span<const std::byte> data) const {
    if (data.size() < 2) {
        return std::monostate{};
    }

    RequestDownloadResponse resp;
    resp.length_format_identifier = static_cast<std::uint8_t>(data[1]);

    auto num_bytes = static_cast<std::size_t>((resp.length_format_identifier >> 4) & 0x0F);
    if (data.size() >= 2 + num_bytes && num_bytes <= 4) {
        resp.max_number_of_block_length = static_cast<std::uint32_t>(
            read_be_var(data.data() + 2, num_bytes));
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_transfer_data_response(
    std::span<const std::byte> data) const {
    if (data.size() < 2) {
        return std::monostate{};
    }

    TransferDataResponse resp;
    resp.block_sequence_counter = static_cast<std::uint8_t>(data[1]);

    if (data.size() > 2) {
        resp.transfer_response_parameter_record = to_vector(data.subspan(2));
    }

    return resp;
}

UdsServiceMessage UdsDecoder::parse_request_transfer_exit_response(
    std::span<const std::byte> data) const {
    RequestTransferExitResponse resp;

    if (data.size() > 1) {
        resp.transfer_response_parameter_record = to_vector(data.subspan(1));
    }

    return resp;
}

}  // namespace wadjet::protocols::uds

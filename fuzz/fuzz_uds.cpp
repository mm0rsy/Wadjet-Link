/// @file fuzz_uds.cpp
/// @brief Fuzz test harness for UDS decoder
/// @details Tests UDS message decoding with random/malformed input

#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_session.hpp"

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::uds;

/// Fuzz the main UDS decoder
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    
    // Create span from fuzz input
    std::span<const std::uint8_t> input_span(data, size);
    
    // Test basic UDS decoder
    UdsDecoder decoder;
    auto result = decoder.decode(input_span);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = result->header;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto sid = header.service_id;
        [[maybe_unused]] auto sub = header.sub_function;
        [[maybe_unused]] auto spr = header.suppress_positive_response;
        [[maybe_unused]] auto nrc = header.negative_response_code;
        [[maybe_unused]] auto rejected = header.rejected_service_id;
        
        // Test classification methods
        [[maybe_unused]] auto is_req = header.is_request();
        [[maybe_unused]] auto is_pos = header.is_positive_response();
        [[maybe_unused]] auto is_neg = header.is_negative_response();
        
        // Test string conversions - should never crash
        [[maybe_unused]] auto sid_str = service_id_string(header.service_id);
        if (header.negative_response_code.has_value()) {
            [[maybe_unused]] auto nrc_str = nrc_string(*header.negative_response_code);
            [[maybe_unused]] auto nrc_desc = nrc_description(*header.negative_response_code);
        }
    }
    
    // Test static classification methods
    [[maybe_unused]] auto is_req = UdsDecoder::is_request(input_span);
    [[maybe_unused]] auto is_pos = UdsDecoder::is_positive_response(input_span);
    [[maybe_unused]] auto is_neg = UdsDecoder::is_negative_response(input_span);
    
    // Test session tracking with fuzz input
    // Use part of input to determine if request or response
    bool is_request = (size > 0) ? (data[0] < 0x40) : true;
    
    UdsSession session(0x0001);
    session.process_message(input_span, is_request);
    
    // Access session state - should never crash
    [[maybe_unused]] auto session_type = session.session_type();
    [[maybe_unused]] auto state = session.state();
    [[maybe_unused]] auto active = session.is_active();
    [[maybe_unused]] auto unlocked = session.is_security_unlocked(1);
    [[maybe_unused]] auto highest = session.highest_security_level();
    [[maybe_unused]] auto& timing = session.timing();
    
    // Test session type string
    [[maybe_unused]] auto st_str = session_type_string(session_type);
    
    return 0;
}

/// Additional fuzzer for specific UDS services
extern "C" int LLVMFuzzerTestDiagnosticSessionControl(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    
    // Build DSC request from fuzz input
    std::vector<std::uint8_t> dsc_request = {0x10};
    dsc_request.insert(dsc_request.end(), data, data + std::min(size, size_t(10)));
    
    std::span<const std::uint8_t> span(dsc_request.data(), dsc_request.size());
    
    UdsDecoder decoder;
    auto result = decoder.decode(span);
    
    if (result) {
        [[maybe_unused]] auto is_dsc = (result->header.service_id == ServiceID::DiagnosticSessionControl);
    }
    
    return 0;
}

/// Fuzzer for Security Access service
extern "C" int LLVMFuzzerTestSecurityAccess(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    
    // Build SA request from fuzz input
    std::vector<std::uint8_t> sa_request = {0x27};
    sa_request.insert(sa_request.end(), data, data + std::min(size, size_t(32)));
    
    std::span<const std::uint8_t> span(sa_request.data(), sa_request.size());
    
    UdsDecoder decoder;
    auto result = decoder.decode(span);
    
    // Process through session
    UdsSession session(0x0001);
    session.process_message(span, true);
    
    // Check security state didn't corrupt
    for (std::uint8_t level = 1; level <= 33; level += 2) {
        [[maybe_unused]] auto unlocked = session.is_security_unlocked(level);
    }
    
    return 0;
}

/// Fuzzer for Read Data By Identifier service
extern "C" int LLVMFuzzerTestReadDataByIdentifier(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    
    // Build RDBI request from fuzz input
    std::vector<std::uint8_t> rdbi_request = {0x22};
    rdbi_request.insert(rdbi_request.end(), data, data + std::min(size, size_t(64)));
    
    std::span<const std::uint8_t> span(rdbi_request.data(), rdbi_request.size());
    
    UdsDecoder decoder;
    auto result = decoder.decode(span);
    
    // Try to extract DIDs from the request
    if (result && result->header.service_id == ServiceID::ReadDataByIdentifier) {
        // DIDs would start at offset 1, each 2 bytes
        size_t did_count = (rdbi_request.size() - 1) / 2;
        for (size_t i = 0; i < did_count && (1 + i * 2 + 1) < rdbi_request.size(); ++i) {
            std::uint16_t did = (rdbi_request[1 + i * 2] << 8) | rdbi_request[1 + i * 2 + 1];
            DataIdentifier did_obj{did};
            [[maybe_unused]] auto val = did_obj.value;
        }
    }
    
    return 0;
}

/// Fuzzer for negative responses
extern "C" int LLVMFuzzerTestNegativeResponse(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    
    // Build NRC from fuzz input
    std::vector<std::uint8_t> nrc_data = {0x7F, data[0], data[1]};
    if (size > 2) {
        nrc_data.insert(nrc_data.end(), data + 2, data + std::min(size, size_t(16)));
    }
    
    std::span<const std::uint8_t> span(nrc_data.data(), nrc_data.size());
    
    UdsDecoder decoder;
    auto result = decoder.decode(span);
    
    if (result && result->header.is_negative_response()) {
        auto nrc = result->header.negative_response_code;
        if (nrc.has_value()) {
            [[maybe_unused]] auto str = nrc_string(*nrc);
            [[maybe_unused]] auto desc = nrc_description(*nrc);
        }
        
        auto rejected = result->header.rejected_service_id;
        if (rejected.has_value()) {
            [[maybe_unused]] auto str = service_id_string(*rejected);
        }
    }
    
    // Process through session
    UdsSession session(0x0001);
    session.process_message(span, false);  // NRC is always a response
    
    return 0;
}

/// Fuzzer for routine control service
extern "C" int LLVMFuzzerTestRoutineControl(const uint8_t* data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    // Build routine control request
    // 0x31 <sub-function> <routine-id-high> <routine-id-low> [data...]
    std::vector<std::uint8_t> rc_request = {0x31};
    rc_request.insert(rc_request.end(), data, data + std::min(size, size_t(128)));
    
    std::span<const std::uint8_t> span(rc_request.data(), rc_request.size());
    
    UdsDecoder decoder;
    auto result = decoder.decode(span);
    
    if (result && result->header.service_id == ServiceID::RoutineControl) {
        // Extract routine ID
        if (rc_request.size() >= 4) {
            std::uint16_t rid = (rc_request[2] << 8) | rc_request[3];
            RoutineIdentifier rid_obj{rid};
            [[maybe_unused]] auto val = rid_obj.value;
        }
    }
    
    return 0;
}

/// Fuzzer for transfer data services
extern "C" int LLVMFuzzerTestTransferData(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    
    // Test various transfer services
    std::vector<std::uint8_t> services = {0x34, 0x35, 0x36, 0x37, 0x38};
    
    for (auto sid : services) {
        std::vector<std::uint8_t> request = {sid};
        request.insert(request.end(), data, data + std::min(size, size_t(256)));
        
        std::span<const std::uint8_t> span(request.data(), request.size());
        
        UdsDecoder decoder;
        auto result = decoder.decode(span);
        
        // Just ensure no crashes
        if (result) {
            [[maybe_unused]] auto service = result->header.service_id;
        }
    }
    
    return 0;
}

/// Fuzzer for session manager with multiple ECUs
extern "C" int LLVMFuzzerTestSessionManager(const uint8_t* data, size_t size) {
    if (size < 4) {
        return 0;
    }
    
    UdsSessionManager manager;
    
    // Use fuzz data to create ECU addresses and send messages
    for (size_t i = 0; i + 3 < size; i += 4) {
        std::uint16_t ecu_addr = (data[i] << 8) | data[i + 1];
        
        auto& session = manager.get_or_create_session(ecu_addr);
        
        // Build a small UDS message from remaining data
        size_t msg_len = std::min(size_t(data[i + 2] & 0x0F) + 1, size - i - 3);
        std::vector<std::uint8_t> msg(data + i + 3, data + i + 3 + msg_len);
        
        if (!msg.empty()) {
            std::span<const std::uint8_t> span(msg.data(), msg.size());
            bool is_request = (msg[0] < 0x40);
            session.process_message(span, is_request);
        }
    }
    
    // Access manager state
    [[maybe_unused]] auto count = manager.session_count();
    
    // Try to find sessions
    for (size_t i = 0; i + 1 < size; i += 2) {
        std::uint16_t addr = (data[i] << 8) | data[i + 1];
        [[maybe_unused]] auto* session = manager.find_session(addr);
    }
    
    // Clear should not crash
    manager.clear_all();
    
    return 0;
}


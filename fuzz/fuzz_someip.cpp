/// @file fuzz_someip.cpp
/// @brief Fuzz test harness for SOME/IP decoder

#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::someip;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode - should not crash regardless of input
    SomeIpDecoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = *result;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto srv_id = header.service_id;
        [[maybe_unused]] auto meth_id = header.method_id;
        [[maybe_unused]] auto len = header.length;
        [[maybe_unused]] auto cli_id = header.client_id;
        [[maybe_unused]] auto sess_id = header.session_id;
        [[maybe_unused]] auto pver = header.protocol_version;
        [[maybe_unused]] auto iver = header.interface_version;
        [[maybe_unused]] auto mtype = header.message_type;
        [[maybe_unused]] auto rcode = header.return_code;
        
        [[maybe_unused]] auto str = header.to_string();
        [[maybe_unused]] auto pname = header.protocol_name();
        [[maybe_unused]] auto hsize = header.header_size();
        [[maybe_unused]] auto psize = header.payload_size();
        
        // Test helper functions
        [[maybe_unused]] auto is_sd = header.is_service_discovery();
        [[maybe_unused]] auto is_req = header.is_request();
        [[maybe_unused]] auto is_resp = header.is_response();
        [[maybe_unused]] auto is_err = header.is_error();
        [[maybe_unused]] auto is_notif = header.is_notification();
        
        // Test message type string conversion
        [[maybe_unused]] auto mtype_str = message_type_string(header.message_type);
        [[maybe_unused]] auto rcode_str = return_code_string(header.return_code);
    }
    
    return 0;
}

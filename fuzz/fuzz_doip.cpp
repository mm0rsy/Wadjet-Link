/// @file fuzz_doip.cpp
/// @brief Fuzz test harness for DoIP decoder

#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::doip;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode - should not crash regardless of input
    DoIPDecoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = *result;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto pver = header.protocol_version;
        [[maybe_unused]] auto ipver = header.inverse_protocol_version;
        [[maybe_unused]] auto ptype = header.payload_type;
        [[maybe_unused]] auto plen = header.payload_length;
        
        [[maybe_unused]] auto str = header.to_string();
        [[maybe_unused]] auto pname = header.protocol_name();
        [[maybe_unused]] auto hsize = header.header_size();
        [[maybe_unused]] auto psize = header.payload_size();
        
        // Test validation and helper methods
        [[maybe_unused]] auto valid = header.is_version_valid();
        [[maybe_unused]] auto is_diag = header.is_diagnostic_message();
        [[maybe_unused]] auto is_ra = header.is_routing_activation();
        
        // Test payload type string conversion
        [[maybe_unused]] auto ptype_str = payload_type_string(header.payload_type);
    }
    
    return 0;
}

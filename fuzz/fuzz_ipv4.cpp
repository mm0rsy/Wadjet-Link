/// @file fuzz_ipv4.cpp
/// @brief Fuzz test harness for IPv4 decoder

#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::ipv4;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode - should not crash regardless of input
    IPv4Decoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = *result;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto ver = header.version;
        [[maybe_unused]] auto ihl = header.ihl;
        [[maybe_unused]] auto dscp = header.dscp;
        [[maybe_unused]] auto ecn = header.ecn;
        [[maybe_unused]] auto tlen = header.total_length;
        [[maybe_unused]] auto id = header.identification;
        [[maybe_unused]] auto flags = header.flags;
        [[maybe_unused]] auto foff = header.fragment_offset;
        [[maybe_unused]] auto ttl = header.ttl;
        [[maybe_unused]] auto proto = header.protocol;
        [[maybe_unused]] auto csum = header.checksum;
        [[maybe_unused]] auto src = header.src_addr;
        [[maybe_unused]] auto dst = header.dst_addr;
        [[maybe_unused]] auto hlen = header.header_len;
        [[maybe_unused]] auto plen = header.payload_len;
        
        [[maybe_unused]] auto src_str = header.src_addr.to_string();
        [[maybe_unused]] auto dst_str = header.dst_addr.to_string();
        [[maybe_unused]] auto str = header.to_string();
        [[maybe_unused]] auto pname = header.protocol_name();
        [[maybe_unused]] auto hsize = header.header_size();
        [[maybe_unused]] auto psize = header.payload_size();
        
        [[maybe_unused]] auto df = header.dont_fragment();
        [[maybe_unused]] auto mf = header.more_fragments();
        [[maybe_unused]] auto frag = header.is_fragment();
        [[maybe_unused]] auto udp = header.is_udp();
        [[maybe_unused]] auto tcp = header.is_tcp();
        
        // Access options if present
        for (const auto& opt : header.options) {
            [[maybe_unused]] auto otype = opt.type;
            [[maybe_unused]] auto olen = opt.length;
            [[maybe_unused]] auto odata = opt.data;
        }
    }
    
    return 0;
}

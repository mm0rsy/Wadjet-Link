/// @file fuzz_tcp.cpp
/// @brief Fuzz test harness for TCP decoder

#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::tcp;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode - should not crash regardless of input
    TCPDecoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = *result;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto sport = header.src_port;
        [[maybe_unused]] auto dport = header.dst_port;
        [[maybe_unused]] auto seq = header.seq_num;
        [[maybe_unused]] auto ack = header.ack_num;
        [[maybe_unused]] auto doff = header.data_offset;
        [[maybe_unused]] auto flags = header.flags;
        [[maybe_unused]] auto win = header.window;
        [[maybe_unused]] auto csum = header.checksum;
        [[maybe_unused]] auto urg = header.urgent_ptr;
        [[maybe_unused]] auto hlen = header.header_len;
        [[maybe_unused]] auto plen = header.payload_len;
        
        [[maybe_unused]] auto str = header.to_string();
        [[maybe_unused]] auto pname = header.protocol_name();
        [[maybe_unused]] auto hsize = header.header_size();
        [[maybe_unused]] auto psize = header.payload_size();
        
        // Flag accessors
        [[maybe_unused]] auto fin = header.is_fin();
        [[maybe_unused]] auto syn = header.is_syn();
        [[maybe_unused]] auto rst = header.is_rst();
        [[maybe_unused]] auto psh = header.is_psh();
        [[maybe_unused]] auto ack_flag = header.is_ack();
        [[maybe_unused]] auto urg_flag = header.is_urg();
        [[maybe_unused]] auto ece = header.is_ece();
        [[maybe_unused]] auto cwr = header.is_cwr();
        
        // Access options if present
        for (const auto& opt : header.options) {
            [[maybe_unused]] auto otype = opt.kind;
            [[maybe_unused]] auto olen = opt.length;
            [[maybe_unused]] auto odata = opt.data;
        }
    }
    
    return 0;
}

/// @file fuzz_ethernet.cpp
/// @brief Fuzz test harness for Ethernet decoder

#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::ethernet;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode - should not crash regardless of input
    EthernetDecoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = *result;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto dst = header.dst_mac.to_string();
        [[maybe_unused]] auto src = header.src_mac.to_string();
        [[maybe_unused]] auto et = header.ethertype;
        [[maybe_unused]] auto hlen = header.header_len;
        [[maybe_unused]] auto has_v = header.has_vlan();
        [[maybe_unused]] auto has_q = header.has_qinq();
        [[maybe_unused]] auto vid = header.vlan_id();
        [[maybe_unused]] auto ipv4 = header.is_ipv4();
        [[maybe_unused]] auto ipv6 = header.is_ipv6();
        [[maybe_unused]] auto str = header.to_string();
        [[maybe_unused]] auto proto = header.protocol_name();
        [[maybe_unused]] auto hsize = header.header_size();
        
        if (header.vlan) {
            [[maybe_unused]] auto pcp = header.vlan->pcp();
            [[maybe_unused]] auto dei = header.vlan->dei();
            [[maybe_unused]] auto vid2 = header.vlan->vid();
            [[maybe_unused]] auto valid = header.vlan->is_valid();
        }
        
        if (header.vlan_inner) {
            [[maybe_unused]] auto pcp = header.vlan_inner->pcp();
            [[maybe_unused]] auto dei = header.vlan_inner->dei();
            [[maybe_unused]] auto vid2 = header.vlan_inner->vid();
        }
    }
    
    return 0;
}

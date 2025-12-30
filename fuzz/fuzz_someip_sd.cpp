/// @file fuzz_someip_sd.cpp
/// @brief Fuzz test harness for SOME/IP-SD decoder

#include "wadjet/protocols/someip_sd.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::someip_sd;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create decode context from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode - should not crash regardless of input
    SomeIpSdDecoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields
    if (result) {
        const auto& header = *result;
        
        // Access all header fields to ensure no crashes
        [[maybe_unused]] auto flags = header.flags;
        [[maybe_unused]] auto elen = header.entries_length;
        [[maybe_unused]] auto olen = header.options_length;
        
        [[maybe_unused]] auto str = header.to_string();
        [[maybe_unused]] auto pname = header.protocol_name();
        [[maybe_unused]] auto hsize = header.header_size();
        [[maybe_unused]] auto psize = header.payload_size();
        
        // Test flag accessors
        [[maybe_unused]] auto reboot = header.is_reboot();
        [[maybe_unused]] auto unicast = header.is_unicast();
        
        // Access entries
        for (const auto& entry : header.entries) {
            if (auto* svc = std::get_if<ServiceEntry>(&entry)) {
                [[maybe_unused]] auto type = svc->type;
                [[maybe_unused]] auto idx1 = svc->index1_first_option;
                [[maybe_unused]] auto idx2 = svc->index2_first_option;
                [[maybe_unused]] auto nopt1 = svc->num_options_1;
                [[maybe_unused]] auto nopt2 = svc->num_options_2;
                [[maybe_unused]] auto srv_id = svc->service_id;
                [[maybe_unused]] auto inst_id = svc->instance_id;
                [[maybe_unused]] auto major = svc->major_version;
                [[maybe_unused]] auto ttl = svc->ttl;
                [[maybe_unused]] auto minor = svc->minor_version;
            } else if (auto* eg = std::get_if<EventgroupEntry>(&entry)) {
                [[maybe_unused]] auto type = eg->type;
                [[maybe_unused]] auto srv_id = eg->service_id;
                [[maybe_unused]] auto inst_id = eg->instance_id;
                [[maybe_unused]] auto major = eg->major_version;
                [[maybe_unused]] auto ttl = eg->ttl;
                [[maybe_unused]] auto counter = eg->counter;
                [[maybe_unused]] auto eg_id = eg->eventgroup_id;
            }
        }
        
        // Access options
        for (const auto& opt : header.options) {
            [[maybe_unused]] auto otype = opt.type;
            [[maybe_unused]] auto odata = opt.data;
            [[maybe_unused]] auto ep = opt.as_ipv4_endpoint();
        }
        
        // Test helper methods
        [[maybe_unused]] auto offers = header.get_offers();
        [[maybe_unused]] auto subs = header.get_subscriptions();
        [[maybe_unused]] auto found = header.find_service(0x1234);
    }
    
    return 0;
}

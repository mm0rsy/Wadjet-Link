/// @file fuzz_dispatcher.cpp
/// @brief Fuzz test harness for Protocol Dispatcher (full stack decoding)

#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet;
using namespace wadjet::protocols;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Create data span from fuzz input
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);
    
    // Test with default options
    {
        ProtocolDispatcher dispatcher;
        auto result = dispatcher.decode(byte_data);
        
        // Access result fields
        [[maybe_unused]] auto complete = result.complete;
        [[maybe_unused]] auto payload = result.payload;
        [[maybe_unused]] auto num_layers = result.layers.size();
        
        // Check for each layer type
        [[maybe_unused]] auto has_eth = result.has_layer<ethernet::EthernetHeader>();
        [[maybe_unused]] auto has_ip = result.has_layer<ipv4::IPv4Header>();
        [[maybe_unused]] auto has_udp = result.has_layer<udp::UdpHeader>();
        [[maybe_unused]] auto has_tcp = result.has_layer<tcp::TcpHeader>();
        [[maybe_unused]] auto has_someip = result.has_layer<someip::SomeIpHeader>();
        [[maybe_unused]] auto has_someip_sd = result.has_layer<someip_sd::SomeIpSdHeader>();
        [[maybe_unused]] auto has_doip = result.has_layer<doip::DoIPHeader>();
        [[maybe_unused]] auto has_gptp = result.has_layer<gptp::GptpHeader>();

        // Try to access each layer
        if (auto* eth = result.get_layer<ethernet::EthernetHeader>()) {
            [[maybe_unused]] auto src = eth->src_mac;
            [[maybe_unused]] auto dst = eth->dst_mac;
        }
        if (auto* ip = result.get_layer<ipv4::IPv4Header>()) {
            [[maybe_unused]] auto src = ip->src_addr;
            [[maybe_unused]] auto dst = ip->dst_addr;
        }
        if (auto* udp = result.get_layer<udp::UdpHeader>()) {
            [[maybe_unused]] auto sport = udp->src_port;
            [[maybe_unused]] auto dport = udp->dst_port;
        }
        if (auto* tcp = result.get_layer<tcp::TcpHeader>()) {
            [[maybe_unused]] auto sport = tcp->src_port;
            [[maybe_unused]] auto dport = tcp->dst_port;
        }
        if (auto* someip = result.get_layer<someip::SomeIpHeader>()) {
            [[maybe_unused]] auto srv_id = someip->service_id;
        }
        if (auto* sd = result.get_layer<someip_sd::SomeIpSdHeader>()) {
            [[maybe_unused]] auto flags = sd->flags;
        }
        if (auto* doip = result.get_layer<doip::DoIPHeader>()) {
            [[maybe_unused]] auto ptype = doip->payload_type;
        }
        if (auto* gptp = result.get_layer<gptp::GptpHeader>()) {
            [[maybe_unused]] auto msg_type = gptp->message_type;
            [[maybe_unused]] auto domain = gptp->domain_number;
            [[maybe_unused]] auto seq_id = gptp->sequence_id;
        }

        // Test get_all_layers
        [[maybe_unused]] auto all_eth = result.get_all_layers<ethernet::EthernetHeader>();
    }
    
    // Test with stop_on_error = true
    {
        DispatcherOptions opts;
        opts.stop_on_error = true;
        ProtocolDispatcher dispatcher(opts);
        auto result = dispatcher.decode(byte_data);
        [[maybe_unused]] auto complete = result.complete;
    }
    
    // Test with decode_application = false
    {
        DispatcherOptions opts;
        opts.decode_application = false;
        ProtocolDispatcher dispatcher(opts);
        auto result = dispatcher.decode(byte_data);
        [[maybe_unused]] auto complete = result.complete;
    }
    
    // Test with validate_ipv4_checksum = true
    {
        DispatcherOptions opts;
        opts.validate_ipv4_checksum = true;
        ProtocolDispatcher dispatcher(opts);
        auto result = dispatcher.decode(byte_data);
        [[maybe_unused]] auto complete = result.complete;
    }
    
    // Test decode_from_ip
    {
        ProtocolDispatcher dispatcher;
        auto result = dispatcher.decode_from_ip(byte_data);
        [[maybe_unused]] auto complete = result.complete;
    }
    
    // Test convenience function
    [[maybe_unused]] auto quick_result = decode_packet(byte_data);
    
    return 0;
}

# Quickstart Guide: Protocol Completeness

**Feature**: M13 Protocol Completeness  
**Date**: 2026-01-15  
**Status**: Phase 1 - Design  
**Audience**: Developers using enhanced protocol features

---

## Overview

This quickstart guide demonstrates how to use the new protocol completeness features in Wadjet-Link. All examples follow real-world automotive use cases and comply with the Constitution principles.

**New Features**:
1. **TCP Connection Tracking** - Monitor DoIP diagnostic sessions
2. **IPv4 Fragment Reassembly** - Handle large fragmented packets
3. **SOME/IP-TP Segmentation** - Process large SOME/IP messages
4. **UDP Checksum Validation** - Detect corrupted SOME/IP-SD messages
5. **Complete Option Parsing** - Extract all TCP and IPv4 options
6. **DoIP Power Mode Tracking** - Monitor ECU diagnostic readiness
7. **UDS NRC Analysis** - Understand diagnostic failures
8. **gPTP TLV Parsing** - Analyze time synchronization performance
9. **Cross-Protocol Validation** - Verify multi-layer consistency

---

## Prerequisites

```bash
# Build Wadjet-Link with M13 features
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run tests to verify installation
ctest --output-on-failure
```

**Required Headers**:
```cpp
#include <wadjet/capture/capture_session.hpp>
#include <wadjet/protocols/ipv4.hpp>
#include <wadjet/protocols/tcp.hpp>
#include <wadjet/protocols/tcp_connection_tracker.hpp>
#include <wadjet/protocols/ipv4_fragment_reassembler.hpp>
#include <wadjet/protocols/someip.hpp>
#include <wadjet/protocols/someip_tp_reassembler.hpp>
#include <wadjet/protocols/udp.hpp>
#include <wadjet/protocols/udp_checksum_validator.hpp>
#include <wadjet/protocols/protocol_validator.hpp>
```

---

## Example 1: TCP Connection Tracking for DoIP Sessions

**Use Case**: Monitor diagnostic tester ↔ ECU TCP connections, detect retransmissions

```cpp
#include <wadjet/capture/capture_session.hpp>
#include <wadjet/protocols/ethernet.hpp>
#include <wadjet/protocols/ipv4.hpp>
#include <wadjet/protocols/tcp.hpp>
#include <wadjet/protocols/tcp_connection_tracker.hpp>
#include <wadjet/protocols/doip.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    // Create capture session
    capture::CaptureSession session("eth0");
    session.set_filter("tcp port 13400");  // DoIP port
    
    // Create decoders
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    tcp::TcpDecoder tcp_decoder;
    doip::DoipDecoder doip_decoder;
    
    // Create TCP connection tracker
    tcp::TcpConnectionTracker tracker;
    
    // Start capture
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        // Decode Ethernet → IPv4 → TCP
        auto eth = eth_decoder.decode(packet.view(), ctx);
        if (!eth || !eth->is_ipv4()) return;
        
        auto ipv4 = ipv4_decoder.decode(packet.view().subview(eth->header_size()), ctx);
        if (!ipv4 || !ipv4->is_tcp()) return;
        
        auto tcp = tcp_decoder.decode(packet.view().subview(eth->header_size() + ipv4->header_size()), ctx);
        if (!tcp) return;
        
        // Track TCP connection
        auto& conn = tracker.track_packet(*tcp, ctx);
        
        // Monitor connection state changes
        if (conn.state == tcp::TcpState::ESTABLISHED) {
            if (conn.packets_client + conn.packets_server == 3) {  // Just established
                std::cout << "DoIP connection established: "
                          << ctx.layer_info.src_ip << ":" << tcp->src_port
                          << " → "
                          << ctx.layer_info.dst_ip << ":" << tcp->dst_port
                          << "\n";
            }
        }
        
        // Detect retransmissions (diagnostic session issues)
        if (conn.retransmissions_client > 0 || conn.retransmissions_server > 0) {
            std::cout << "WARNING: Retransmissions detected (network issue?)\n"
                      << "  Client retransmits: " << conn.retransmissions_client << "\n"
                      << "  Server retransmits: " << conn.retransmissions_server << "\n";
        }
        
        // Decode DoIP payload
        if (tcp->dst_port == 13400 || tcp->src_port == 13400) {
            auto doip = doip_decoder.decode(packet.view().subview(
                eth->header_size() + ipv4->header_size() + tcp->header_size()
            ), ctx);
            
            if (doip) {
                std::cout << "DoIP message: " << doip->to_string() << "\n";
            }
        }
    });
    
    // Run for 60 seconds
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    // Cleanup expired connections
    auto removed = tracker.cleanup_expired();
    std::cout << "Cleaned up " << removed << " expired connections\n";
    
    // Print statistics
    auto stats = tracker.get_stats();
    std::cout << "Total connections: " << stats.total_connections << "\n"
              << "Active connections: " << stats.active_connections << "\n"
              << "Retransmissions detected: " << stats.retransmissions_detected << "\n";
    
    return 0;
}
```

**Expected Output**:
```
DoIP connection established: 192.168.1.100:54321 → 192.168.1.10:13400
DoIP message: DiagnosticMessage(0x8001, SourceAddr=0x0E00, TargetAddr=0x0102)
Cleaned up 1 expired connections
Total connections: 5
Active connections: 2
Retransmissions detected: 0
```

---

## Example 2: IPv4 Fragment Reassembly

**Use Case**: Reassemble large fragmented diagnostic messages (>1500 bytes MTU)

```cpp
#include <wadjet/capture/capture_session.hpp>
#include <wadjet/protocols/ipv4.hpp>
#include <wadjet/protocols/ipv4_fragment_reassembler.hpp>
#include <wadjet/protocols/udp.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    capture::CaptureSession session("eth0");
    
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    udp::UdpDecoder udp_decoder;
    
    // Create fragment reassembler (30s timeout)
    ipv4::Ipv4FragmentReassembler reassembler;
    
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        auto eth = eth_decoder.decode(packet.view(), ctx);
        if (!eth || !eth->is_ipv4()) return;
        
        auto ipv4 = ipv4_decoder.decode(packet.view().subview(eth->header_size()), ctx);
        if (!ipv4) return;
        
        // Check if packet is fragmented
        if (ipv4->is_fragmented()) {
            std::cout << "Fragment received: offset=" << ipv4->fragment_offset 
                      << ", MF=" << ipv4->flags.more_fragments
                      << ", ID=" << ipv4->identification << "\n";
            
            // Add to reassembler
            auto complete = reassembler.add_fragment(
                *ipv4,
                packet.view().subview(eth->header_size() + ipv4->header_size()),
                ctx
            );
            
            if (complete) {
                std::cout << "✓ Datagram reassembled: " << complete->size() << " bytes\n";
                
                // Decode higher-layer protocol from reassembled data
                if (ipv4->is_udp()) {
                    auto udp = udp_decoder.decode(complete->view(), ctx);
                    if (udp) {
                        std::cout << "  UDP: " << udp->src_port << " → " << udp->dst_port
                                  << ", length=" << udp->payload_size() << "\n";
                    }
                }
            }
        } else {
            // Non-fragmented packet, process normally
            // ...
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    // Cleanup
    auto expired = reassembler.cleanup_expired();
    std::cout << "Discarded " << expired << " incomplete datagrams\n";
    
    auto stats = reassembler.get_stats();
    std::cout << "Datagrams reassembled: " << stats.datagrams_reassembled << "\n";
    
    return 0;
}
```

**Expected Output**:
```
Fragment received: offset=0, MF=true, ID=12345
Fragment received: offset=185, MF=true, ID=12345
Fragment received: offset=370, MF=false, ID=12345
✓ Datagram reassembled: 512 bytes
  UDP: 30490 → 30490, length=504
Datagrams reassembled: 1
```

---

## Example 3: SOME/IP-TP Message Reassembly

**Use Case**: Reassemble large SOME/IP-TP messages (firmware updates, large diagnostics)

```cpp
#include <wadjet/capture/capture_session.hpp>
#include <wadjet/protocols/someip.hpp>
#include <wadjet/protocols/someip_tp_reassembler.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    capture::CaptureSession session("eth0");
    session.set_filter("udp");
    
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    udp::UdpDecoder udp_decoder;
    someip::SomeIpDecoder someip_decoder;
    
    // Create SOME/IP-TP reassembler (5s timeout, 16 MB max)
    someip::SomeipTpReassembler reassembler;
    
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        // Decode full stack: Ethernet → IPv4 → UDP → SOME/IP
        auto eth = eth_decoder.decode(packet.view(), ctx);
        if (!eth || !eth->is_ipv4()) return;
        
        auto ipv4 = ipv4_decoder.decode(packet.view().subview(eth->header_size()), ctx);
        if (!ipv4 || !ipv4->is_udp()) return;
        
        auto udp = udp_decoder.decode(
            packet.view().subview(eth->header_size() + ipv4->header_size()), ctx
        );
        if (!udp) return;
        
        auto someip = someip_decoder.decode(
            packet.view().subview(eth->header_size() + ipv4->header_size() + udp->header_size()), ctx
        );
        if (!someip) return;
        
        // Check if this is a TP message
        if (someip::SomeipTpHeader::is_tp_message(*someip)) {
            auto tp_hdr = someip::SomeipTpHeader::parse(
                *someip,
                packet.view().subview(eth->header_size() + ipv4->header_size() + 
                                     udp->header_size() + someip->header_size())
            );
            
            if (tp_hdr) {
                std::cout << "TP segment: Service=0x" << std::hex << someip->service_id
                          << ", Method=0x" << someip->method_id
                          << ", Offset=" << std::dec << tp_hdr->offset
                          << ", More=" << tp_hdr->more_segments << "\n";
                
                auto complete = reassembler.add_segment(
                    *someip, *tp_hdr,
                    packet.view().subview(eth->header_size() + ipv4->header_size() + 
                                         udp->header_size() + someip->header_size() + 4)
                );
                
                if (complete) {
                    std::cout << "✓ TP message reassembled: " << complete->size() << " bytes\n";
                    std::cout << "  Service: 0x" << std::hex << someip->service_id
                              << ", Method: 0x" << someip->method_id << std::dec << "\n";
                    
                    // Process complete message (e.g., firmware update chunk)
                    // ...
                }
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(120));  // Firmware updates take time
    
    auto stats = reassembler.get_stats();
    std::cout << "TP messages reassembled: " << stats.messages_reassembled << "\n";
    
    return 0;
}
```

**Expected Output**:
```
TP segment: Service=0x1234, Method=0x5678, Offset=0, More=true
TP segment: Service=0x1234, Method=0x5678, Offset=1400, More=true
TP segment: Service=0x1234, Method=0x5678, Offset=2800, More=false
✓ TP message reassembled: 3145 bytes
  Service: 0x1234, Method: 0x5678
TP messages reassembled: 1
```

---

## Example 4: UDP Checksum Validation

**Use Case**: Detect corrupted SOME/IP-SD messages in noisy automotive networks

```cpp
#include <wadjet/capture/capture_session.hpp>
#include <wadjet/protocols/udp.hpp>
#include <wadjet/protocols/udp_checksum_validator.hpp>
#include <wadjet/protocols/someip_sd.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    capture::CaptureSession session("eth0");
    session.set_filter("udp port 30490");  // SOME/IP-SD port
    
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    
    // Configure UDP decoder with warning-only checksum mode (default)
    udp::Options udp_opts;
    udp_opts.checksum_mode = udp::ChecksumMode::Warning;
    udp::UdpDecoder udp_decoder(udp_opts);
    
    someip_sd::SomeipSdDecoder sd_decoder;
    
    std::size_t total_packets = 0;
    std::size_t checksum_failures = 0;
    
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        auto eth = eth_decoder.decode(packet.view(), ctx);
        if (!eth || !eth->is_ipv4()) return;
        
        auto ipv4 = ipv4_decoder.decode(packet.view().subview(eth->header_size()), ctx);
        if (!ipv4 || !ipv4->is_udp()) return;
        
        auto udp = udp_decoder.decode(
            packet.view().subview(eth->header_size() + ipv4->header_size()), ctx
        );
        if (!udp) return;
        
        total_packets++;
        
        // Check UDP checksum validation result
        if (!udp->checksum_valid) {
            checksum_failures++;
            std::cout << "⚠ UDP checksum FAILED for SOME/IP-SD message\n"
                      << "  Source: " << ctx.layer_info.src_ip << ":" << udp->src_port << "\n"
                      << "  Destination: " << ctx.layer_info.dst_ip << ":" << udp->dst_port << "\n"
                      << "  Checksum: 0x" << std::hex << udp->checksum << std::dec << "\n";
            
            // Still attempt to decode SD message (warning-only mode)
            auto sd = sd_decoder.decode(
                packet.view().subview(eth->header_size() + ipv4->header_size() + udp->header_size()),
                ctx
            );
            
            if (sd) {
                std::cout << "  SD message decoded despite checksum error (may be corrupted)\n";
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    std::cout << "\nChecksum Statistics:\n"
              << "  Total packets: " << total_packets << "\n"
              << "  Checksum failures: " << checksum_failures << "\n"
              << "  Failure rate: " << (100.0 * checksum_failures / total_packets) << "%\n";
    
    return 0;
}
```

**Expected Output** (healthy network):
```
Checksum Statistics:
  Total packets: 1523
  Checksum failures: 0
  Failure rate: 0.00%
```

**Expected Output** (noisy network):
```
⚠ UDP checksum FAILED for SOME/IP-SD message
  Source: 192.168.1.20:30490
  Destination: 224.224.224.245:30490
  Checksum: 0x1a3f
  SD message decoded despite checksum error (may be corrupted)

Checksum Statistics:
  Total packets: 1523
  Checksum failures: 3
  Failure rate: 0.20%
```

---

## Example 5: TCP and IPv4 Options Parsing

**Use Case**: Extract TCP MSS, window scale, timestamps, IPv4 Router Alert

```cpp
#include <wadjet/protocols/tcp.hpp>
#include <wadjet/protocols/ipv4.hpp>

void analyze_tcp_options(const tcp::TcpHeader& hdr) {
    std::cout << "TCP Options:\n";
    
    for (const auto& opt : hdr.options) {
        switch (opt.kind) {
            case tcp::TcpOptionKind::MaxSegmentSize:
                if (auto mss = opt.mss()) {
                    std::cout << "  MSS: " << *mss << " bytes\n";
                }
                break;
                
            case tcp::TcpOptionKind::WindowScale:
                if (auto scale = opt.window_scale()) {
                    std::cout << "  Window Scale: " << static_cast<int>(*scale) 
                              << " (multiplier: " << (1 << *scale) << ")\n";
                }
                break;
                
            case tcp::TcpOptionKind::Timestamps:
                if (auto ts = opt.timestamps()) {
                    std::cout << "  Timestamps: TSval=" << ts->first 
                              << ", TSecr=" << ts->second << "\n";
                }
                break;
                
            case tcp::TcpOptionKind::SackPermitted:
                std::cout << "  SACK Permitted\n";
                break;
                
            default:
                std::cout << "  Unknown option: " << static_cast<int>(opt.kind) << "\n";
        }
    }
}

void analyze_ipv4_options(const ipv4::IPv4Header& hdr) {
    std::cout << "IPv4 Options:\n";
    
    for (const auto& opt : hdr.options) {
        switch (opt.type) {
            case ipv4::Ipv4OptionType::RouterAlert:
                if (auto value = opt.router_alert_value()) {
                    std::cout << "  Router Alert: " << *value << "\n";
                }
                break;
                
            case ipv4::Ipv4OptionType::RecordRoute:
                auto addrs = opt.record_route_addresses();
                std::cout << "  Record Route: " << addrs.size() << " hops\n";
                for (const auto& addr : addrs) {
                    std::cout << "    " << addr.to_string() << "\n";
                }
                break;
                
            case ipv4::Ipv4OptionType::Timestamp:
                if (auto ts = opt.timestamp_data()) {
                    std::cout << "  Timestamp: " << ts->timestamps.size() << " entries\n";
                }
                break;
                
            default:
                std::cout << "  Unknown option: " << static_cast<int>(opt.type) << "\n";
        }
    }
}
```

**Expected Output**:
```
TCP Options:
  MSS: 1460 bytes
  Window Scale: 7 (multiplier: 128)
  Timestamps: TSval=1234567890, TSecr=9876543210
  SACK Permitted

IPv4 Options:
  Router Alert: 0
```

---

## Example 6: DoIP Power Mode Tracking

**Use Case**: Monitor ECU diagnostic readiness (power mode transitions)

```cpp
#include <wadjet/protocols/doip.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    capture::CaptureSession session("eth0");
    session.set_filter("tcp port 13400");
    
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    tcp::TcpDecoder tcp_decoder;
    doip::DoipDecoder doip_decoder;
    
    doip::DiagnosticPowerMode current_power_mode = doip::DiagnosticPowerMode::NotReady;
    
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        // Decode stack (Ethernet → IPv4 → TCP → DoIP)
        // ... (similar to Example 1) ...
        
        auto doip = doip_decoder.decode(/* ... */, ctx);
        if (!doip) return;
        
        // Check for power mode messages (0x4003 request, 0x4004 response)
        if (doip->payload_type == 0x4004) {  // Power Mode Response
            auto power_mode = static_cast<doip::DiagnosticPowerMode>(
                /* extract from payload */
            );
            
            if (power_mode != current_power_mode) {
                std::cout << "ECU Power Mode Changed: ";
                
                switch (power_mode) {
                    case doip::DiagnosticPowerMode::NotReady:
                        std::cout << "Not Ready (ECU sleeping/booting)\n";
                        break;
                    case doip::DiagnosticPowerMode::Ready:
                        std::cout << "Ready (diagnostics enabled)\n";
                        break;
                    case doip::DiagnosticPowerMode::NotSupported:
                        std::cout << "Not Supported (legacy ECU)\n";
                        break;
                }
                
                current_power_mode = power_mode;
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    return 0;
}
```

**Expected Output**:
```
ECU Power Mode Changed: Ready (diagnostics enabled)
```

---

## Example 7: UDS Negative Response Code Analysis

**Use Case**: Understand why diagnostic requests fail (NRC classification)

```cpp
#include <wadjet/protocols/uds.hpp>

void analyze_uds_response(const uds::UdsHeader& hdr) {
    if (hdr.is_negative_response()) {
        auto nrc_data = hdr.get_negative_response();
        if (nrc_data) {
            auto metadata = nrc_data->get_metadata();
            
            std::cout << "UDS Negative Response:\n"
                      << "  Service: 0x" << std::hex << static_cast<int>(nrc_data->service_id) << "\n"
                      << "  NRC: 0x" << static_cast<int>(nrc_data->nrc) << std::dec << "\n"
                      << "  Description: " << metadata->description << "\n"
                      << "  Classification: " << (metadata->classification == uds::NrcClass::Temporary 
                                                   ? "Temporary (retry may succeed)" 
                                                   : "Permanent (retry will fail)") << "\n";
            
            if (nrc_data->sub_function) {
                std::cout << "  Sub-function: 0x" << std::hex 
                          << static_cast<int>(*nrc_data->sub_function) << std::dec << "\n";
            }
            
            // Suggest action based on classification
            if (nrc_data->is_temporary()) {
                std::cout << "→ RECOMMENDATION: Retry after delay\n";
            } else {
                std::cout << "→ RECOMMENDATION: Check preconditions or service availability\n";
            }
        }
    }
}
```

**Expected Output**:
```
UDS Negative Response:
  Service: 0x22
  NRC: 0x21
  Description: Busy - Repeat Request
  Classification: Temporary (retry may succeed)
→ RECOMMENDATION: Retry after delay
```

---

## Example 8: gPTP TLV Analysis (Rate Ratio)

**Use Case**: Analyze clock synchronization performance (rate ratio from Follow_Up)

```cpp
#include <wadjet/protocols/gptp.hpp>

void analyze_gptp_follow_up(const gptp::GptpHeader& hdr, PacketView tlv_data) {
    // Parse TLVs
    auto tlvs = gptp::parse_tlv_array(tlv_data);
    
    for (const auto& tlv : tlvs) {
        if (std::holds_alternative<gptp::FollowUpInformationTlv>(tlv)) {
            const auto& follow_up_tlv = std::get<gptp::FollowUpInformationTlv>(tlv);
            
            double rate_ratio = follow_up_tlv.get_rate_ratio();
            
            std::cout << "gPTP Follow_Up Information TLV:\n"
                      << "  Rate Ratio: " << std::fixed << std::setprecision(9) 
                      << rate_ratio << "\n"
                      << "  GM Time Base Indicator: " 
                      << follow_up_tlv.gm_time_base_indicator << "\n";
            
            // Analyze clock drift
            double ppm_offset = (rate_ratio - 1.0) * 1e6;
            std::cout << "  Clock Offset: " << std::fixed << std::setprecision(3)
                      << ppm_offset << " ppm\n";
            
            if (std::abs(ppm_offset) > 100.0) {
                std::cout << "  ⚠ WARNING: Excessive clock drift (>100 ppm)\n";
            }
        }
    }
}
```

**Expected Output**:
```
gPTP Follow_Up Information TLV:
  Rate Ratio: 1.000000012
  GM Time Base Indicator: 42
  Clock Offset: 0.012 ppm
```

---

## Example 9: Cross-Protocol Validation

**Use Case**: Validate multi-layer protocol consistency (catch malformed packets)

```cpp
#include <wadjet/protocols/protocol_validator.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    capture::CaptureSession session("eth0");
    
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    tcp::TcpDecoder tcp_decoder;
    
    // Create validator in lenient mode (log warnings, continue)
    ProtocolLayerValidator validator(ValidationMode::Lenient);
    
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        // Decode all layers
        auto eth = eth_decoder.decode(packet.view(), ctx);
        if (eth) {
            auto ipv4 = ipv4_decoder.decode(packet.view().subview(eth->header_size()), ctx);
            if (ipv4) {
                auto tcp = tcp_decoder.decode(
                    packet.view().subview(eth->header_size() + ipv4->header_size()), ctx
                );
            }
        }
        
        // Validate full stack
        auto result = validator.validate_stack(ctx);
        
        if (!result.valid) {
            std::cout << "✗ Validation FAILED:\n";
            for (const auto& error : result.errors) {
                std::cout << "  ERROR: " << error << "\n";
            }
        }
        
        for (const auto& warning : result.warnings) {
            std::cout << "  WARNING: " << warning << "\n";
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    return 0;
}
```

**Expected Output** (malformed packet):
```
  WARNING: IPv4 total_length (520) exceeds Ethernet payload (512)
  WARNING: TCP checksum not validated (pseudo-header required)
```

---

## Example 10: Complete DoIP Diagnostic Session Analysis

**Use Case**: End-to-end DoIP diagnostic session monitoring with all features

```cpp
#include <wadjet/capture/capture_session.hpp>
#include <wadjet/protocols/tcp_connection_tracker.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds.hpp>
#include <wadjet/protocols/protocol_validator.hpp>

int main() {
    using namespace wadjet;
    using namespace wadjet::protocols;
    
    capture::CaptureSession session("eth0");
    session.set_filter("tcp port 13400");
    
    // Create decoders and trackers
    ethernet::EthernetDecoder eth_decoder;
    ipv4::IPv4Decoder ipv4_decoder;
    tcp::TcpDecoder tcp_decoder;
    doip::DoipDecoder doip_decoder;
    uds::UdsDecoder uds_decoder;
    
    tcp::TcpConnectionTracker tracker;
    ProtocolLayerValidator validator(ValidationMode::Warning);
    
    session.start();
    
    session.set_packet_handler([&](const Packet& packet) {
        DecodeContext ctx;
        
        // Decode stack
        auto eth = eth_decoder.decode(packet.view(), ctx);
        if (!eth || !eth->is_ipv4()) return;
        
        auto ipv4 = ipv4_decoder.decode(packet.view().subview(eth->header_size()), ctx);
        if (!ipv4 || !ipv4->is_tcp()) return;
        
        auto tcp = tcp_decoder.decode(
            packet.view().subview(eth->header_size() + ipv4->header_size()), ctx
        );
        if (!tcp) return;
        
        // Track TCP connection
        auto& conn = tracker.track_packet(*tcp, ctx);
        
        // Validate protocol stack
        auto validation = validator.validate_stack(ctx);
        for (const auto& warning : validation.warnings) {
            std::cout << "⚠ " << warning << "\n";
        }
        
        // Decode DoIP
        if (tcp->dst_port == 13400 || tcp->src_port == 13400) {
            auto doip = doip_decoder.decode(
                packet.view().subview(eth->header_size() + ipv4->header_size() + tcp->header_size()),
                ctx
            );
            
            if (doip) {
                std::cout << "DoIP: " << doip->to_string() << "\n";
                
                // Check for diagnostic messages
                if (doip->payload_type == 0x8001) {  // Diagnostic message
                    auto uds = uds_decoder.decode(
                        packet.view().subview(eth->header_size() + ipv4->header_size() + 
                                             tcp->header_size() + doip->header_size()),
                        ctx
                    );
                    
                    if (uds) {
                        if (uds->is_negative_response()) {
                            auto nrc = uds->get_negative_response();
                            if (nrc) {
                                auto metadata = nrc->get_metadata();
                                std::cout << "  UDS NRC: " << metadata->description 
                                          << " (" << (nrc->is_temporary() ? "Temporary" : "Permanent") << ")\n";
                            }
                        } else {
                            std::cout << "  UDS Positive Response: Service 0x" 
                                      << std::hex << static_cast<int>(uds->service_id) << std::dec << "\n";
                        }
                    }
                }
            }
        }
        
        // Report connection statistics
        if (conn.state == tcp::TcpState::FIN_WAIT_1 || conn.state == tcp::TcpState::CLOSE_WAIT) {
            std::cout << "Connection closing:\n"
                      << "  Packets: " << conn.packets_client << " (client) + " 
                      << conn.packets_server << " (server)\n"
                      << "  Bytes: " << conn.bytes_client << " (client) + " 
                      << conn.bytes_server << " (server)\n"
                      << "  Retransmissions: " << conn.retransmissions_client << " (client) + " 
                      << conn.retransmissions_server << " (server)\n";
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(120));
    
    auto stats = tracker.get_stats();
    std::cout << "\nFinal Statistics:\n"
              << "  Total DoIP sessions: " << stats.total_connections << "\n"
              << "  Retransmissions: " << stats.retransmissions_detected << "\n";
    
    return 0;
}
```

**Expected Output**:
```
DoIP: RoutingActivationRequest(SourceAddr=0x0E00)
DoIP: RoutingActivationResponse(ClientAddr=0x0E00, Status=Success)
DoIP: DiagnosticMessage(SourceAddr=0x0E00, TargetAddr=0x0102)
  UDS Positive Response: Service 0x10
DoIP: DiagnosticMessage(SourceAddr=0x0E00, TargetAddr=0x0102)
  UDS NRC: Busy - Repeat Request (Temporary)
Connection closing:
  Packets: 42 (client) + 38 (server)
  Bytes: 15234 (client) + 12890 (server)
  Retransmissions: 0 (client) + 0 (server)

Final Statistics:
  Total DoIP sessions: 5
  Retransmissions: 0
```

---

## Configuration Examples

### TCP Connection Tracker Configuration

```cpp
tcp::TcpConnectionTracker::Config config;
config.timeout_incomplete = std::chrono::minutes(5);  // 5 minutes for long diagnostics
config.timeout_timewait = std::chrono::seconds(60);   // 60 seconds (strict RFC 793)
config.track_retransmissions = true;
config.track_out_of_order = true;

tcp::TcpConnectionTracker tracker(config);
```

### IPv4 Fragment Reassembler Configuration

```cpp
ipv4::Ipv4FragmentReassembler::Config config;
config.timeout = std::chrono::seconds(60);  // 60s for slow networks
config.max_concurrent_datagrams = 2048;     // Higher limit for busy gateway

ipv4::Ipv4FragmentReassembler reassembler(config);
```

### SOME/IP-TP Reassembler Configuration

```cpp
someip::SomeipTpReassembler::Config config;
config.timeout = std::chrono::seconds(10);    // 10s for slow firmware updates
config.max_message_size = 32 * 1024 * 1024;   // 32 MB for large firmware
config.max_concurrent_messages = 512;

someip::SomeipTpReassembler reassembler(config);
```

---

## Best Practices

### 1. Memory Management

**Always cleanup expired resources**:
```cpp
// Periodic cleanup (every 30 seconds recommended)
std::thread cleanup_thread([&]() {
    while (running) {
        tcp_tracker.cleanup_expired();
        ipv4_reassembler.cleanup_expired();
        tp_reassembler.cleanup_expired();
        
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
});
```

### 2. Error Handling

**Check decode results**:
```cpp
auto tcp = tcp_decoder.decode(data, ctx);
if (!tcp) {
    std::cerr << "TCP decode failed\n";
    return;
}

if (!tcp->checksum_valid) {
    std::cerr << "WARNING: TCP checksum invalid\n";
    // Continue or abort based on use case
}
```

### 3. Performance

**Use capture filters to reduce load**:
```cpp
// Filter at kernel level (most efficient)
session.set_filter("tcp port 13400 or udp port 30490");

// Disable features you don't need
udp::Options opts;
opts.checksum_mode = udp::ChecksumMode::Disabled;  // Skip checksum for trusted network
```

### 4. Thread Safety

**Use separate trackers per thread**:
```cpp
// Per-thread trackers (no locking needed)
thread_local tcp::TcpConnectionTracker tracker;
thread_local ipv4::Ipv4FragmentReassembler reassembler;
```

---

## Troubleshooting

### Issue: "TCP connections not tracked"

**Solution**: Ensure DecodeContext contains IP addresses:
```cpp
DecodeContext ctx;
ctx.layer_info.src_ip = ipv4->src_ip;
ctx.layer_info.dst_ip = ipv4->dst_ip;

tracker.track_packet(*tcp, ctx);  // Now has IP addresses for 5-tuple key
```

### Issue: "Fragments not reassembling"

**Solution**: Check fragment timeout and verify MF flag:
```cpp
if (ipv4->is_fragmented()) {
    std::cout << "Fragment: offset=" << ipv4->fragment_offset 
              << ", MF=" << ipv4->flags.more_fragments << "\n";
    
    // Ensure timeout is sufficient
    reassembler.cleanup_expired();  // Don't call too frequently
}
```

### Issue: "TP messages not reassembling"

**Solution**: Verify TP flag detection:
```cpp
if (someip::SomeipTpHeader::is_tp_message(*someip)) {
    std::cout << "TP message detected\n";
} else {
    std::cout << "Non-TP SOME/IP message\n";
}
```

---

## Next Steps

1. **Read API Contracts**: See [contracts/api-contracts.md](contracts/api-contracts.md) for detailed API specifications
2. **Read Data Model**: See [data-model.md](data-model.md) for entity definitions
3. **Run Examples**: Build and run examples from `examples/` directory
4. **Run Tests**: `ctest` to verify installation
5. **Read Full Documentation**: Doxygen docs at `docs/html/index.html`

---

**Quickstart Complete**: 2026-01-15  
**Support**: See [CONTRIBUTING.md](../../CONTRIBUTING.md) for getting help

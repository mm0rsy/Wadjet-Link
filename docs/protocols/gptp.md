# gPTP (IEEE 802.1AS) Protocol Support

## Overview

Wadjet-Link provides comprehensive support for decoding and analyzing gPTP (Generalized Precision Time Protocol) messages as defined in IEEE 802.1AS-2020. gPTP is the profile of IEEE 1588 (PTP) used in automotive Ethernet for precise time synchronization.

## Use Cases

gPTP is essential for:

- **Time-Sensitive Networking (TSN)**: Synchronized time windows for deterministic communication
- **Audio/Video Bridging (AVB)**: Media synchronization in infotainment systems  
- **AUTOSAR Adaptive**: Timing requirements for software components
- **Sensor Fusion**: Correlating data from multiple sensors with precise timestamps
- **Event Timestamping**: Recording when events occurred with sub-microsecond accuracy

## Protocol Structure

### EtherType

gPTP uses EtherType `0x88F7` (same as PTP over Ethernet).

### Message Types

| Type | Value | Description |
|------|-------|-------------|
| Sync | 0x0 | Time synchronization message (event) |
| Delay_Req | 0x1 | Delay measurement request (not used in gPTP) |
| Pdelay_Req | 0x2 | Peer delay measurement request (event) |
| Pdelay_Resp | 0x3 | Peer delay measurement response (event) |
| Follow_Up | 0x8 | Precise timestamp for Sync (general) |
| Delay_Resp | 0x9 | Delay measurement response (not used in gPTP) |
| Pdelay_Resp_Follow_Up | 0xA | Precise timestamp for Pdelay_Resp (general) |
| Announce | 0xB | Best Master Clock Algorithm data (general) |
| Signaling | 0xC | Control messages (general) |
| Management | 0xD | Management messages (general) |

### Header Format

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Transp| MsgType |   Version     |       Message Length          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Domain      |   Reserved    |            Flags              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                     Correction Field (64-bit)                 +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Reserved (32-bit)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                                                               +
|                   Source Port Identity (80-bit)               |
+                  (Clock Identity + Port Number)               +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Sequence ID           |   Control     | LogMsgIntrvl  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## Quick Start

### Basic Decoding

```cpp
#include <wadjet/protocols/gptp/gptp.hpp>
#include <wadjet/protocols/dispatcher.hpp>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::gptp;

// Decode from raw packet data
auto result = decode_packet(packet_data);

if (result.has_layer<GptpHeader>()) {
    const auto* gptp = result.get_layer<GptpHeader>();
    
    std::cout << "gPTP Message Type: " << message_type_string(gptp->message_type) << "\n";
    std::cout << "Domain: " << static_cast<int>(gptp->domain_number) << "\n";
    std::cout << "Sequence ID: " << gptp->sequence_id << "\n";
    std::cout << "Source Clock: " << gptp->source_port_identity.clock_identity.to_string() << "\n";
}
```

### Processing Specific Message Types

```cpp
// Check for Sync message
if (gptp->message_type == MessageType::Sync) {
    if (std::holds_alternative<SyncMessage>(gptp->body)) {
        const auto& sync = std::get<SyncMessage>(gptp->body);
        std::cout << "Sync origin timestamp: "
                  << sync.origin_timestamp.seconds_lsb << "."
                  << sync.origin_timestamp.nanoseconds << "\n";
    }
}

// Check for Follow_Up with TLV
if (gptp->message_type == MessageType::Follow_Up) {
    if (std::holds_alternative<FollowUpMessage>(gptp->body)) {
        const auto& fu = std::get<FollowUpMessage>(gptp->body);
        
        if (fu.tlv) {
            // Rate ratio calculation
            double rate_offset = 
                static_cast<double>(fu.tlv->cumulative_scaled_rate_offset) / (1LL << 41);
            double rate_ratio = 1.0 + rate_offset;
            std::cout << "Rate ratio: " << rate_ratio << "\n";
        }
    }
}

// Process Announce message
if (gptp->message_type == MessageType::Announce) {
    if (std::holds_alternative<AnnounceMessage>(gptp->body)) {
        const auto& ann = std::get<AnnounceMessage>(gptp->body);
        std::cout << "Grandmaster: " << ann.grandmaster_identity.to_string() << "\n";
        std::cout << "Priority1: " << static_cast<int>(ann.grandmaster_priority1) << "\n";
    }
}
```

## Testing with Matchers

Wadjet-Link provides gMock-compatible matchers for testing gPTP traffic:

```cpp
#include <wadjet/testing/matchers.hpp>
using namespace wadjet::testing;

// Test for gPTP Sync message
EXPECT_THAT(packet, IsGptpSync());

// Test for specific message type
EXPECT_THAT(packet, HasGptpMessageType(MessageType::Follow_Up));

// Test domain number
EXPECT_THAT(packet, HasGptpDomain(0));

// Test sequence ID
EXPECT_THAT(packet, HasGptpSequenceId(42));

// Test for two-step operation
EXPECT_THAT(packet, IsGptpTwoStep());

// Test for event message (requires timestamping)
EXPECT_THAT(packet, IsGptpEventMessage());

// Combine multiple matchers
EXPECT_THAT(packet, AllOf(
    IsGptp(),
    HasGptpDomain(0),
    IsGptpSync()
));
```

## Using the Protocol Dispatcher

The ProtocolDispatcher automatically detects gPTP based on EtherType:

```cpp
ProtocolDispatcher dispatcher;
auto result = dispatcher.decode(packet_data);

if (result.has_layer<GptpHeader>()) {
    // gPTP packet detected and decoded
}
```

## Helper Functions

### Clock Identity Operations

```cpp
// Create clock identity from MAC address
MacAddress mac = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
auto clock_id = ClockIdentity::from_mac(mac);
// Result: 00:11:22:FF:FE:33:44:55

// Convert to string
std::string str = clock_id.to_string();
```

### Multicast Check

```cpp
// Check if MAC is gPTP multicast address
MacAddress dst_mac = ...;
if (is_gptp_multicast(dst_mac)) {
    // 01:80:C2:00:00:0E (802.1AS multicast)
}
```

### Message Type Helpers

```cpp
// Check if message requires timestamping
bool needs_ts = is_event_message(gptp->message_type);

// Get string representation
std::string type_str = message_type_string(gptp->message_type);
```

### Peer Delay Calculation

```cpp
// Calculate peer delay from Pdelay exchange timestamps
auto delay = calculate_peer_delay(
    t1,  // Pdelay_Req transmit time
    t2,  // Pdelay_Req receipt time (from Pdelay_Resp)
    t3,  // Pdelay_Resp transmit time (from Pdelay_Resp_Follow_Up)
    t4   // Pdelay_Resp receipt time
);
```

## Example Application

See `examples/gptp_monitor.cpp` for a complete example that:

- Captures gPTP traffic from an interface or PCAP file
- Tracks multiple clocks and their message statistics
- Extracts grandmaster information from Announce messages
- Displays rate ratio from Follow_Up TLVs
- Provides summary of all detected clocks

Run the example:

```bash
# Live capture
./gptp_monitor eth0

# Analyze PCAP file  
./gptp_monitor gptp_traffic.pcap
```

## Automotive Use Cases

### TSN Gate Scheduling

gPTP provides the time reference for TSN gate scheduling:

```cpp
// Get precise origin timestamp from Follow_Up
const auto& fu = std::get<FollowUpMessage>(gptp->body);
uint64_t precise_ns = 
    fu.precise_origin_timestamp.seconds_lsb * 1'000'000'000ULL +
    fu.precise_origin_timestamp.nanoseconds;
```

### AVB Stream Synchronization

For AVB media streams, track clock relationships:

```cpp
// Track master clock from Announce messages
if (gptp->message_type == MessageType::Announce) {
    const auto& ann = std::get<AnnounceMessage>(gptp->body);
    current_grandmaster = ann.grandmaster_identity;
    steps_to_gm = ann.steps_removed;
}
```

### Diagnostic Data Timestamping

Correlate diagnostic events with gPTP time:

```cpp
// Get synchronized time from network
auto sync_time = extract_sync_timestamp(gptp_header);

// Associate with diagnostic event
DiagnosticEvent event;
event.gptp_timestamp = sync_time;
event.data = diagnostic_data;
```

## References

- IEEE 802.1AS-2020: Timing and Synchronization for Time-Sensitive Applications
- IEEE 1588-2019: Precision Clock Synchronization Protocol for Networked Measurement and Control Systems  
- AUTOSAR Time Synchronization over Ethernet

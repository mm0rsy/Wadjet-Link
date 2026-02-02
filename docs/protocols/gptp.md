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

## TLV (Type-Length-Value) Options

gPTP uses TLVs to carry optional information in signaling messages. Wadjet-Link provides full TLV parsing and support.

### TLV Header Format

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          TLV Type             |         TLV Length             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                      TLV Value (variable)                      +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Fields:**
- **Type** (16-bit): Identifies the TLV type and organization
- **Length** (16-bit): Size of TLV Value field in octets
- **Value** (variable): TLV-type specific data

### Supported TLV Types

| Type | Value | Organization | Description | Used In |
|------|-------|--------------|-------------|---------|
| MANAGEMENT | 0x0001 | IEEE 1588 | Deprecated management messages | Management |
| MANAGEMENT_ERROR_STATUS | 0x0002 | IEEE 1588 | Error status for management | Management |
| ORGANIZATION_EXTENSION | 0x0004 | IEEE 1588 | Vendor-specific extensions | Any |
| REQUEST_UNICAST_TRANSMISSION | 0x0005 | IEEE 1588 | Request unicast messaging | Signaling |
| GRANT_UNICAST_TRANSMISSION | 0x0006 | IEEE 1588 | Grant unicast messaging | Signaling |
| CANCEL_UNICAST_TRANSMISSION | 0x0007 | IEEE 1588 | Cancel unicast messaging | Signaling |
| ACKNOWLEDGE_CANCEL_UNICAST | 0x0008 | IEEE 1588 | Acknowledge cancel request | Signaling |
| PATH_TRACE | 0x0009 | IEEE 1588 | Clock hierarchy path | Announce |
| ALTERNATE_TIME_OFFSET_INDICATOR | 0x0010 | IEEE 1588 | Alternate time offset | Announce |
| AUTHENTICATION | 0x0020 | AUTOSAR | Automotive authentication data | Signaling |
| ORGANIZATION_EXTENSION_PROP | 0x0080 | IEEE 1588 | Non-propagating vendor extensions | Any |

### FOLLOW_UP TLV Format

The Follow_Up Precise Origin Timestamp TLV contains rate information:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Precise Origin Timestamp                     |
|                       Seconds (32-bit)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Precise Origin Timestamp                     |
|                     Nanoseconds (32-bit)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Cumulative Scaled Rate Offset (signed 64-bit)          |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      Scaled Last GM Phase Change (signed 64-bit)               |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| GM Phase Change Indicator |   Reserved (5 bits)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Fields:**
- **Precise Origin Timestamp**: Exact time Sync message was transmitted
- **Cumulative Scaled Rate Offset**: Clock rate error (signed, scaled by 2^-41)
- **GM Phase Change**: Nanosecond offset if grandmaster changed
- **Phase Change Indicator**: Flags for special conditions

**Rate Ratio Calculation:**
```
rate_error = cumulative_scaled_rate_offset / 2^41
rate_ratio = 1.0 + rate_error
```

Example: Rate offset of `2^40` = rate_error of 0.5, meaning clock is running 0.5% faster.

### ANNOUNCEMENT TLV: PATH_TRACE

PATH_TRACE contains the chain of clocks from source to receiver:

```cpp
// Example PATH_TRACE value (clock identities in hierarchy)
// [00:11:22:FF:FE:33:44:55]  -> Grandmaster
//   |
//   [AA:BB:CC:FF:FE:DD:EE:FF]  -> Default Switch
//     |
//     [11:22:33:FF:FE:44:55:66]  -> Endpoint
```

**Grandmaster Selection (BMCA):**
1. **Best Master Clock Algorithm** selects the best grandmaster
2. **Priority 1** field (if set) overrides clock quality
3. **Clock Quality** ranking: LOCKED > HO > PTP > DEFAULT
4. **Priority 2** field breaks ties
5. **ClockIdentity** used as final tiebreaker

### REQUEST_UNICAST_TRANSMISSION TLV

For requesting unicast messaging instead of multicast:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Message Type | LogInterval   |      Duration (16-bit)         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Message Types that support unicast:**
- Sync (0x0)
- Announce (0xB)
- Delay_Resp (0x9)
- Pdelay_Resp (0x3)

### TLV Parsing Algorithm

```cpp
#include <wadjet/protocols/gptp/gptp.hpp>

// Parse TLVs from signaling message
std::vector<TLV> tlvs;
size_t offset = 0;

while (offset < message_length) {
    // 1. Read TLV header (4 bytes)
    uint16_t type = read_uint16_be(data + offset);
    uint16_t length = read_uint16_be(data + offset + 2);
    offset += 4;
    
    // 2. Validate length
    if (offset + length > message_length) break;
    
    // 3. Parse based on type
    TLV tlv;
    tlv.type = type;
    tlv.length = length;
    
    // 4. Extract type-specific fields
    switch (type) {
        case 0x0005:  // REQUEST_UNICAST_TRANSMISSION
            tlv.message_type = data[offset];
            tlv.log_interval = data[offset + 1];
            tlv.duration = read_uint16_be(data + offset + 2);
            break;
            
        case 0x0006:  // GRANT_UNICAST_TRANSMISSION
            tlv.message_type = data[offset];
            tlv.log_interval = data[offset + 1];
            tlv.duration = read_uint16_be(data + offset + 2);
            tlv.renewal_invited = (data[offset + 4] >> 7) & 1;
            break;
            
        default:
            // Store raw value for unknown types
            tlv.value = std::vector<uint8_t>(data + offset, 
                                             data + offset + length);
    }
    
    tlvs.push_back(tlv);
    offset += length;
}
```

### Common TLV Combinations

**Master Clock Announcement:**
```
Announce + PATH_TRACE TLV
├─ Grandmaster ID: 00:11:22:FF:FE:33:44:55
├─ Priority1: 128
├─ Clock Quality: LOCKED
└─ Steps Removed: 1
```

**Unicast Request Sequence:**
```
Signaling (from slave to master) + REQUEST_UNICAST_TRANSMISSION
├─ Message Type: Sync (0x0)
├─ Log Interval: 0 (1 Hz)
└─ Duration: 3600 (request for 1 hour)

Signaling (from master to slave) + GRANT_UNICAST_TRANSMISSION
├─ Message Type: Sync (0x0)
├─ Log Interval: 0 (1 Hz)
├─ Duration: 3600
└─ Renewal Invited: 1 (can renew before expiry)
```

### Automotive Security Extensions

AUTOSAR extensions add security to gPTP:

```cpp
// AUTHENTICATION TLV (0x0020)
struct AutomotiveAuthTLV {
    uint8_t auth_type;      // Authentication algorithm
    uint8_t key_id;         // Key identifier
    uint8_t auth_data[12];  // HMAC or signature (variable)
};
```

**Authentication Types:**
- 0x00: No authentication
- 0x01: HMAC-MD5 (deprecated)
- 0x02: HMAC-SHA256 (recommended)
- 0x03: Digital signature

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

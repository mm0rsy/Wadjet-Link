# DDS/RTPS Protocol Support

## Overview

Wadjet-Link provides comprehensive support for decoding and analyzing DDS (Data Distribution Service) traffic using the RTPS (Real-Time Publish-Subscribe) wire protocol as specified in OMG RTPS v2.4. DDS is widely used in automotive systems for publish-subscribe communication patterns.

## Use Cases

DDS/RTPS is essential for:

- **ROS2 Middleware**: Default communication layer for Robot Operating System 2
- **AUTOSAR Adaptive**: Service-oriented communication in adaptive platform
- **ADAS Systems**: Real-time sensor data distribution
- **V2X Communication**: Vehicle-to-everything messaging
- **Distributed Systems**: Loosely-coupled component communication

## Protocol Structure

### Transport

RTPS typically runs over UDP:

| Port Type | Range | Description |
|-----------|-------|-------------|
| Discovery Multicast | 7400 | SPDP participant discovery |
| Discovery Unicast | 7410+ | SPDP participant response |
| User Multicast | 7401 | User data multicast |
| User Unicast | 7411+ | User data unicast |

**Port Formula:**
- Discovery Multicast: `PB + DG * domainId`
- Discovery Unicast: `PB + DG * domainId + d1 + PG * participantId`
- User Multicast: `PB + DG * domainId + d2`
- User Unicast: `PB + DG * domainId + d3 + PG * participantId`

Where: PB=7400, DG=250, PG=2, d0=0, d1=10, d2=1, d3=11

### RTPS Header Format

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      'R'      |      'T'      |      'P'      |      'S'      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Major Version | Minor Version |          Vendor ID            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                                                               +
|                       GUID Prefix (96-bit)                    |
+                                                               +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Submessage Header

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Submessage ID |     Flags     |      Octets to Next Header    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Submessage Types

| ID | Name | Description |
|----|------|-------------|
| 0x01 | PAD | Padding for alignment |
| 0x06 | ACKNACK | Reader acknowledges data |
| 0x07 | HEARTBEAT | Writer announces available data |
| 0x08 | GAP | Writer indicates unavailable sequence numbers |
| 0x09 | INFO_TS | Timestamp for subsequent submessages |
| 0x0C | INFO_SRC | Source GUID for subsequent submessages |
| 0x0D | INFO_REPLY_IP4 | IPv4 reply locator |
| 0x0E | INFO_DST | Destination GUID prefix |
| 0x0F | INFO_REPLY | Reply locator |
| 0x12 | NACK_FRAG | Reader requests fragment retransmission |
| 0x13 | HEARTBEAT_FRAG | Writer announces available fragments |
| 0x15 | DATA | User data payload |
| 0x16 | DATA_FRAG | Fragmented user data |

## Supported Vendors

| Vendor ID | Name | Constant |
|-----------|------|----------|
| 0x0101 | eProsima Fast DDS | `VendorId::FastDDS` |
| 0x0102 | RTI Connext DDS | `VendorId::RTI` |
| 0x0103 | PrismTech OpenSplice | `VendorId::OpenSplice` |
| 0x0105 | ADLINK CycloneDDS | `VendorId::CycloneDDS` |
| 0x0106 | OCI OpenDDS | `VendorId::OpenDDS` |

## Quick Start

### Basic Decoding

```cpp
#include <wadjet/protocols/dds/rtps.hpp>
#include <wadjet/protocols/dds/rtps_messages.hpp>
#include <wadjet/protocols/dispatcher.hpp>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::dds;

// Create decoder
RtpsDecoder decoder;
DecodeContext ctx{packet_data};

// Decode RTPS message
auto result = decoder.decode(ctx);
if (result.is_ok()) {
    const auto& header = *result;
    
    std::cout << "RTPS Version: " << (int)header.version.major 
              << "." << (int)header.version.minor << "\n";
    std::cout << "Vendor: " << to_string(header.vendor_id.to_vendor()) << "\n";
    std::cout << "GUID Prefix: " << header.guid_prefix.to_string() << "\n";
    
    // Iterate submessages
    for (const auto& submsg : header.submessages) {
        std::cout << "  Submessage: " << to_string(submsg.header.kind) << "\n";
    }
}
```

### Discovery Parsing

```cpp
#include <wadjet/protocols/dds/discovery.hpp>

// Parse SPDP participant data
DiscoveryParser parser;
auto participant = parser.parse_spdp(data_submessage);

if (participant) {
    std::cout << "Participant: " << participant->participant_name << "\n";
    std::cout << "Domain ID: " << participant->domain_id << "\n";
    
    for (const auto& locator : participant->metatraffic_unicast_locators) {
        std::cout << "  Locator: " << locator.to_string() << "\n";
    }
}

// Parse SEDP endpoint data
auto endpoint = parser.parse_sedp(data_submessage);
if (endpoint) {
    std::cout << "Topic: " << endpoint->topic_name << "\n";
    std::cout << "Type: " << endpoint->type_name << "\n";
}
```

### Using the Protocol Dispatcher

```cpp
#include <wadjet/protocols/dispatcher.hpp>

ProtocolDispatcher dispatcher;

// Decode from raw Ethernet frame
auto result = dispatcher.decode(frame_data);

if (auto* rtps = result.get_layer<RtpsHeader>()) {
    // Process DDS traffic
    for (const auto& submsg : rtps->submessages) {
        if (submsg.header.kind == SubmessageKind::DATA) {
            auto* data = std::get_if<DataSubmessage>(&submsg.body);
            // Process data
        }
    }
}
```

## Testing with Matchers

Wadjet-Link provides gMock-compatible matchers for DDS protocol testing:

### Basic Matchers

```cpp
#include <wadjet/testing/matchers.hpp>

using namespace wadjet::testing;

// Check if packet is RTPS
EXPECT_THAT(packet, IsRtps());
EXPECT_THAT(packet, IsDds());  // Alias

// Version checks
EXPECT_THAT(packet, HasRtpsVersion(2, 4));

// Vendor checks
EXPECT_THAT(packet, HasRtpsVendor(VendorId::FastDDS));
EXPECT_THAT(packet, IsFromFastDDS());
EXPECT_THAT(packet, IsFromRTI());
EXPECT_THAT(packet, IsFromCycloneDDS());
EXPECT_THAT(packet, IsFromOpenDDS());
```

### Submessage Matchers

```cpp
// Check for specific submessage types
EXPECT_THAT(packet, HasRtpsSubmessage(SubmessageKind::DATA));
EXPECT_THAT(packet, HasRtpsData());
EXPECT_THAT(packet, HasRtpsHeartbeat());
EXPECT_THAT(packet, HasRtpsAckNack());
EXPECT_THAT(packet, HasRtpsGap());
EXPECT_THAT(packet, HasRtpsInfoTs());
EXPECT_THAT(packet, HasRtpsInfoDst());

// Count submessages
EXPECT_THAT(packet, HasRtpsSubmessageCount(3));  // At least 3

// Discovery traffic
EXPECT_THAT(packet, IsRtpsDiscovery());
EXPECT_THAT(packet, IsSpdpOrSedp());  // Alias
```

### GUID Matchers

```cpp
// Check GUID prefix
GuidPrefix expected_prefix = /* ... */;
EXPECT_THAT(packet, HasRtpsGuidPrefix(expected_prefix));
```

## Example: DDS Traffic Monitor

See `examples/dds_monitor.cpp` for a complete example that:

- Captures live DDS traffic from network interface
- Tracks participant discovery (SPDP)
- Enumerates topics and endpoints (SEDP)
- Calculates data rate statistics
- Displays QoS policy information

```cpp
// Example usage
./dds_monitor --interface eth0 --domain 0

// Output:
// === DDS Traffic Monitor ===
// Discovered Participants: 3
//   - /talker (FastDDS) - 192.168.1.10
//   - /listener (CycloneDDS) - 192.168.1.11
//   - /bridge (RTI) - 192.168.1.12
//
// Active Topics: 5
//   - /chatter (std_msgs::String) - 10 Hz
//   - /image_raw (sensor_msgs::Image) - 30 Hz
//   - /scan (sensor_msgs::LaserScan) - 40 Hz
```

## API Reference

### Core Classes

#### `RtpsHeader`
Represents a decoded RTPS message header.

| Field | Type | Description |
|-------|------|-------------|
| `version` | `ProtocolVersion` | RTPS version (major.minor) |
| `vendor_id` | `VendorIdRaw` | Vendor identifier |
| `guid_prefix` | `GuidPrefix` | 96-bit GUID prefix |
| `submessages` | `std::vector<Submessage>` | Parsed submessages |

#### `RtpsDecoder`
Decoder for RTPS messages.

```cpp
class RtpsDecoder {
public:
    auto decode(DecodeContext& ctx) -> Result<RtpsHeader, DecodeError>;
    auto can_decode(std::span<const std::byte> data) const -> bool;
};
```

#### `DiscoveryParser`
Parser for DDS discovery messages.

```cpp
class DiscoveryParser {
public:
    auto parse_spdp(const DataSubmessage& data) -> std::optional<ParticipantData>;
    auto parse_sedp(const DataSubmessage& data) -> std::optional<EndpointData>;
};
```

### Types

#### `VendorId` enum
```cpp
enum class VendorId : std::uint16_t {
    Unknown = 0x0000,
    FastDDS = 0x0101,
    RTI = 0x0102,
    OpenSplice = 0x0103,
    CycloneDDS = 0x0105,
    OpenDDS = 0x0106,
};
```

#### `SubmessageKind` enum
```cpp
enum class SubmessageKind : std::uint8_t {
    PAD = 0x01,
    ACKNACK = 0x06,
    HEARTBEAT = 0x07,
    GAP = 0x08,
    INFO_TS = 0x09,
    INFO_SRC = 0x0C,
    INFO_REPLY_IP4 = 0x0D,
    INFO_DST = 0x0E,
    INFO_REPLY = 0x0F,
    NACK_FRAG = 0x12,
    HEARTBEAT_FRAG = 0x13,
    DATA = 0x15,
    DATA_FRAG = 0x16,
};
```

## Practical Examples

### Example: Monitoring Heartbeats

```cpp
#include <wadjet/protocols/dds/rtps_analyzer.hpp>

void on_heartbeat(const RTPSHeartbeat& hb) {
    if (hb.final_flag && hb.liveliness_flag) {
        std::cout << "Liveliness check from " << hb.writer_id.to_string() << "\n";
    }
}

// ... in your main loop
RtpsAnalyzer analyzer;
analyzer.register_callback<RTPSHeartbeat>(on_heartbeat);
analyzer.process_packet(payload);
```

### Example: Parsing Discovery Data

```cpp
#include <wadjet/protocols/dds/rtps_decoder.hpp>
#include <wadjet/protocols/dds/rtps_discovery.hpp>

void decode_discovery(std::span<const std::byte> data) {
    auto submessage = RtpsDecoder::decode_submessage(data);
    if (submessage.header.type == SubmessageType::DATA) {
        auto data_msg = std::get<RTPSData>(submessage.body);
        // Check if it's a ParameterList (common in discovery)
        if (data_msg.has_inline_qos) {
            auto params = RtpsDecoder::parse_parameter_list(data_msg.inline_qos);
            for (const auto& param : params) {
                if (param.id == ParameterId::PID_TOPIC_NAME) {
                    std::cout << "Discovered Topic: " << param.as_string() << "\n";
                }
            }
        }
    }
}
```

## See Also

- [OMG RTPS Specification v2.4](https://www.omg.org/spec/DDSI-RTPS/2.4)
- [DDS Specification v1.4](https://www.omg.org/spec/DDS/1.4)
- [ROS2 DDS Documentation](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Different-Middleware-Vendors.html)
- [Architecture Overview](../architecture.md)

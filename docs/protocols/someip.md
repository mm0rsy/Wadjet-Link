# SOME/IP Protocol Reference

**Status**: ✅ Complete (M13)  
**Spec**: AUTOSAR PRS_SOMEIP  
**Implementation**: `include/wadjet/protocols/someip.hpp`

## Overview

Scalable Service-Oriented Middleware over IP (SOME/IP) - AUTOSAR standard RPC protocol for automotive ECUs. Complete implementation includes basic messaging and TP (Transport Protocol) segmentation for messages up to 16 MB.

## Header Structure

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Service ID (16 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Method ID (16 bits)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Length (32 bits)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Request ID (32 bits)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Proto|Hdr Type|Message Type|Return Code|    Payload           |
|Ver  |       |             |           |                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## Fields

| Field | Bits | Description |
|-------|------|-------------|
| **Service ID** | 16 | Service identifier (0x0000-0xFFFF) |
| **Method ID** | 16 | Method/event identifier within service |
| **Length** | 32 | Length of payload (Request ID + Return Code + data) |
| **Request ID** | 32 | Session ID (high 16 bits) + sequence (low 16 bits) |
| **Protocol Ver** | 4 | SOME/IP protocol version (0x1) |
| **Header Type** | 4 | 0x0=Request, 0x1=RequestNoReturn, 0x2=Response, 0x3=Error |
| **Message Type** | 8 | Service type indicator |
| **Return Code** | 8 | Response status (0x0=OK, others=error) |
| **Payload** | Var | Service-specific data |

## Message Types

| Type | Code | Direction | Description |
|------|------|-----------|-------------|
| **Request** | 0x0 | Client→Server | Expects response |
| **Request-No-Return** | 0x1 | Client→Server | No response expected |
| **Response** | 0x2 | Server→Client | Response to Request |
| **Error** | 0x3 | Server→Client | Error response |

## Return Codes

| Code | Name | Description |
|------|------|-------------|
| 0x00 | E_OK | No error |
| 0x01 | E_NOT_OK | General error |
| 0x02 | E_UNKNOWN_SERVICE | Service not found |
| 0x03 | E_UNKNOWN_METHOD | Method not found |
| 0x04 | E_NOT_READY | Service not ready |
| 0x05 | E_NOT_REACHABLE | Service unreachable |
| 0x06 | E_TIMEOUT | Service call timeout |
| 0x07 | E_3RD_PARTY_TRANSPORT_ERROR | Transport layer error |
| 0x08-0x0F | Reserved | Future use |
| 0x10 | E_INVALID_PAYLOAD | Bad payload |
| 0x11 | E_UNKNOWN_ERROR | Unknown service error |
| 0x12 | E_INCOMPATIBLE_SPEC_VERSION | Incompatible version |
| 0x13 | E_NOTIMPLEMENTED_FUNCTION | Function not implemented |
| 0x14 | E_NOTAVAILABLE_FUNCTION | Function not available |

## Request ID Format

```
 0       15 16      31
+--------+--------+
| Session| Sequence|
| ID     | Number |
+--------+--------+
```

- **Session ID (16 bits)**: Identifies logical session/connection
- **Sequence Number (16 bits)**: Request number within session, incremented for each request

## Service IDs

Service IDs are assigned by AUTOSAR (0x0001-0xFFFE):

| Range | Purpose |
|-------|---------|
| 0x0000 | Reserved |
| 0x0001-0x00FF | Standard AUTOSAR services |
| 0x0100-0xFFFD | User-defined services |
| 0xFFFE | Service discovery (SD) |
| 0xFFFF | Reserved |

## Method IDs

Method IDs are assigned per service (0x0000-0xFFFF):

| Range | Purpose |
|-------|---------|
| 0x0000-0x00FF | Standard AUTOSAR methods |
| 0x0100-0x7FFF | User-defined methods |
| 0x8000-0xFFFF | Events |

## SOME/IP Transport Protocol (TP)

SOME/IP-TP segments large messages for reliable delivery up to 16 MB.

### TP Header

When length > max segment size, message is split:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Service ID (16 bits)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Method ID (16 bits)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Total Length (32 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Request ID (32 bits)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Ver |  Hdr Type  | Message Type | Return Code | More Segments |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Segment Offset (32 bits)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Segment Data (Variable)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### TP Fields

| Field | Bits | Description |
|-------|------|-------------|
| **More Segments Flag** | 1 | 1 if more segments follow, 0 if last |
| **Segment Offset** | 31 | Byte offset of this segment in complete message |

### TP Reassembly

1. **Identification**: (src_ip, dst_ip, service_id, method_id, request_id)
2. **Timeout**: 5 seconds to receive all segments
3. **Gap Detection**: Monitor for missing segments
4. **Completion**: When More Segments=0 and all bytes received
5. **Maximum Size**: 16 MB (16,777,216 bytes)

### Example: 1 MB Message Segmented

Assuming 4 KB segments:

```
Segment 1: Offset=0,    Size=4096, More=1
Segment 2: Offset=4096, Size=4096, More=1
...
Segment N: Offset=1044480, Size=1536, More=0 (total = 1,046,016 bytes)
```

## Typical Port Assignments

| Service | Port |
|---------|------|
| **SOME/IP-SD** | UDP/TCP 30490 |
| **SOME/IP-TP** (user) | UDP/TCP 30491+ |
| **Unicast SOME/IP** | TCP 30501-30999 |
| **Multicast** | UDP 224.224.224.245 port 30490 |

## Event Handling

Events (method IDs 0x8000-0xFFFF) are one-way notifications:

```
Client (Event Subscriber)
         |
         | SUBSCRIBE (SD)
         |
         v
     Server (Event Provider)
         |
         | EVENT NOTIFICATION (no ACK)
         |
         v
      Client
```

Characteristics:
- No response expected
- Can be multicast or unicast
- Usually fire-and-forget semantics

## Common Service IDs

| ID | Service | Purpose |
|----|---------|---------|
| 0x0001 | Diagnostics | Vehicle diagnostics (UDS over SOME/IP) |
| 0x0002 | Infotainment | Media, navigation services |
| 0x0003 | Climate | HVAC control |
| 0x0004 | Powertrain | Engine, transmission control |
| 0x0005 | Body | Window, door, seat control |
| 0x0006 | Safety | Airbag, stability control |
| 0x0007 | Telematics | Cellular, GPS services |
| 0xFFFE | Service Discovery | SD only |

## Compression & Optimization

### Header Size
- Minimum: 16 bytes (fixed header without TP)
- With TP: +8 bytes (24 bytes minimum)
- Overhead: Low for typical message sizes

### Transmission Options
- **Unicast**: Direct service-to-service (TCP/UDP)
- **Multicast**: Service discovery and notifications
- **Reliable**: TCP ensures ordering and delivery
- **Unreliable**: UDP for real-time, loss-tolerant data

## Common Issues & Troubleshooting

| Issue | Cause | Resolution |
|-------|-------|-----------|
| Service not discovered | SD multicast blocked | Check firewall, multicast routing |
| Segmentation timeout | Slow network/packet loss | Check MTU, verify all segments arrive |
| Request timeout | Service offline | Verify service availability, check SD |
| Bad checksum | Corrupted packet | Verify network integrity |
| Unknown return code | Incompatible version | Check AUTOSAR/SOME/IP spec version |

## Code Examples

### Parsing SOME/IP Header

```cpp
#include <wadjet/protocols/someip/someip.hpp>
using namespace wadjet::protocols;

auto result = someip::Decoder::decode(packet_data);
if (result) {
    const auto& header = result.value();
    
    std::cout << "Service: 0x" << std::hex << header.service_id << "\n";
    std::cout << "Method: 0x" << header.method_id << "\n";
    std::cout << "Payload: " << header.payload.size() << " bytes\n";
}
```

### TP Reassembly

```cpp
someip::SomeipTpReassembler reassembler(std::chrono::seconds(5));

for (const auto& packet : packets) {
    auto reassembled = reassembler.add_segment(packet);
    if (reassembled) {
        process_complete_message(reassembled.value());
    }
}
```

## References

- AUTOSAR Standard PRS_SOMEIP - SOME/IP Protocol Specification
- [AUTOSAR Specification](https://www.autosar.org/) - Official standard
- AUTOSAR SWS_CommunicationManager
- Technical Paper: "SOME/IP Protocol Specification" (published by AUTOSAR)

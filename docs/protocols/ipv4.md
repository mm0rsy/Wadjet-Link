# IPv4 Protocol Reference

**Status**: ✅ Complete (M13)  
**RFC**: [RFC 791](https://tools.ietf.org/html/rfc791)  
**Implementation**: `include/wadjet/protocols/ipv4.hpp`

## Overview

Complete IPv4 header parsing with support for all option types, fragmentation reassembly, ToS/DSCP field extraction, and checksum validation.

## Header Structure

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|      Fragment Offset    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |         Header Checksum       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Source Address                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination Address                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options (if IHL > 5)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## Fields

| Field | Bits | Description |
|-------|------|-------------|
| **Version** | 4 | IP protocol version (must be 4 for IPv4) |
| **IHL** | 4 | Internet Header Length in 32-bit words (5-15, minimum 5 for 20 bytes) |
| **ToS** | 8 | Type of Service (deprecated, use DSCP/ECN) |
| **DSCP** | 6 | Differentiated Services Code Point (ToS bits 7-2) |
| **ECN** | 2 | Explicit Congestion Notification (ToS bits 1-0) |
| **Total Length** | 16 | Entire datagram size in bytes (header + payload, 20-65535) |
| **Identification** | 16 | Unique identifier for fragment reassembly |
| **Flags** | 3 | Control flags (Reserved, DF, MF) |
| **Fragment Offset** | 13 | Byte offset of this fragment (in 8-byte units) |
| **TTL** | 8 | Time To Live - decremented by each hop (1-255) |
| **Protocol** | 8 | Encapsulated protocol number (6=TCP, 17=UDP, 47=GRE, etc.) |
| **Checksum** | 16 | 16-bit ones-complement checksum of header only |
| **Source IP** | 32 | Source IPv4 address |
| **Dest IP** | 32 | Destination IPv4 address |

## Flags

| Flag | Bit | Purpose |
|------|-----|---------|
| **Reserved** | 0 | Must be 0 (reserved for future use) |
| **DF** | 1 | Don't Fragment - if set, router will not fragment the packet |
| **MF** | 2 | More Fragments - if set, more fragments follow this one |

## IPv4 Options

IPv4 supports variable-length options in the optional header section. Options are parsed when IHL > 5.

### Option Types (by number)

| Number | Type | Length | Description |
|--------|------|--------|-------------|
| 0 | EOOL | 1 byte | End of Option List |
| 1 | NOP | 1 byte | No Operation (padding) |
| 2 | SEC | 11 bytes | Security (deprecated, RFC 1108) |
| 3 | LSR | Variable | Loose Source & Record Route |
| 7 | RR | Variable | Record Route |
| 8 | TS | Variable | Internet Timestamp |
| 9 | CIPSO | Variable | Commercial IP Security Option |
| 68 | SSRR | Variable | Strict Source & Record Route |

### Loose Source Route Option (Type 3)

Used to route packet through specified gateways:

```
Type=3, Length (variable), Pointer, Routes...
```

| Field | Size | Description |
|-------|------|-------------|
| **Type** | 1 byte | Option type (3) |
| **Length** | 1 byte | Total option length (5 + 4×N where N=# gateways) |
| **Pointer** | 1 byte | Offset to next gateway (starts at 4, increments by 4) |
| **Routes** | 4×N bytes | Gateway IP addresses |

### Strict Source Route Option (Type 68)

Like LSR but route MUST be exactly as specified:

```
Type=68, Length (variable), Pointer, Routes...
```

### Record Route Option (Type 7)

Records IP addresses of each gateway:

```
Type=7, Length (variable), Pointer, Recorded Addresses...
```

| Field | Size | Description |
|-------|------|-------------|
| **Type** | 1 byte | Option type (7) |
| **Length** | 1 byte | Total length (3 + 4×N) |
| **Pointer** | 1 byte | Points to next slot for recording (starts at 4) |
| **Addresses** | 4×N bytes | Recorded gateway IPs (empty until recorded) |

### Internet Timestamp Option (Type 8)

Records timestamps at each gateway:

```
Type=8, Length (variable), Pointer, Overflow/Flag, Timestamps/Addresses...
```

| Field | Size | Description |
|-------|------|-------------|
| **Type** | 1 byte | Option type (8) |
| **Length** | 1 byte | Total length in bytes |
| **Pointer** | 1 byte | Points to next slot (starts at 5) |
| **Overflow** | 4 bits | Overflow counter (# gateways that couldn't record) |
| **Flag** | 4 bits | Timestamp flag (0=timestamp only, 1=each with address, 3=gateways only) |
| **Data** | Variable | Timestamps and/or IP addresses depending on flag |

### Router Alert Option (Type 94)

Signals routers to examine packet more carefully:

```
Type=94, Length=4, Value...
```

| Field | Size | Description |
|-------|------|-------------|
| **Type** | 1 byte | Option type (94) |
| **Length** | 1 byte | Always 4 |
| **Value** | 2 bytes | Alert type (0=Router shall examine packet) |

## Type of Service (ToS) / DSCP Fields

### Legacy ToS Byte (Deprecated)

Modern implementations use DSCP/ECN instead:

```
 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+
|DSCP (6 bits) |EC|
+-+-+-+-+-+-+-+-+
```

### DSCP (Differentiated Services Code Point)

Bits 7-2 of ToS field - defines service level:

| DSCP Value | Name | Purpose |
|------------|------|---------|
| 0 | BE | Best Effort (default) |
| 8 | CS1 | Class Selector 1 |
| 10 | AF11 | Assured Forwarding 1,1 |
| 12 | AF12 | Assured Forwarding 1,2 |
| 14 | AF13 | Assured Forwarding 1,3 |
| 16 | CS2 | Class Selector 2 |
| 18 | AF21 | Assured Forwarding 2,1 |
| 20 | AF22 | Assured Forwarding 2,2 |
| 22 | AF23 | Assured Forwarding 2,3 |
| 24 | CS3 | Class Selector 3 |
| 26 | AF31 | Assured Forwarding 3,1 |
| 28 | AF32 | Assured Forwarding 3,2 |
| 30 | AF33 | Assured Forwarding 3,3 |
| 32 | CS4 | Class Selector 4 |
| 34 | AF41 | Assured Forwarding 4,1 |
| 36 | AF42 | Assured Forwarding 4,2 |
| 38 | AF43 | Assured Forwarding 4,3 |
| 40 | CS5 | Class Selector 5 |
| 46 | EF | Expedited Forwarding (VoIP) |
| 48 | CS6 | Class Selector 6 |
| 56 | CS7 | Class Selector 7 |

### ECN (Explicit Congestion Notification)

Bits 1-0 of ToS field - congestion indication:

| Value | Name | Meaning |
|-------|------|---------|
| 0 | Not-ECT | Not ECN Capable Transport |
| 1 | ECT(1) | ECN Capable Transport (1) |
| 2 | ECT(0) | ECN Capable Transport (0) |
| 3 | CE | Congestion Experienced |

## Fragmentation & Reassembly

IPv4 packets can be fragmented by routers if they exceed the path MTU.

### Fragment Reassembly Algorithm

1. **Fragment Identification**: Keyed by (src_ip, dst_ip, protocol, identification)
2. **Fragment Storage**: Store arriving fragments in order by offset
3. **Gap Detection**: Detect missing fragments by checking offsets
4. **Timeout**: Discard incomplete reassembly after 30 seconds
5. **Completion**: When MF=0 on fragment and no gaps exist, reassemble payload

### Fragment Fields

| Field | Purpose |
|-------|---------|
| **Identification** | Unique ID for all fragments of same original datagram |
| **MF Flag** | More Fragments=1 if more fragments follow, 0 if last |
| **Fragment Offset** | Byte position of this fragment ÷ 8 (8-byte units) |

### Example: 3000-byte packet fragmented to 1500 MTU

```
Fragment 1:  Offset=0, MF=1, Length=1500 (contains bytes 0-1479)
Fragment 2:  Offset=185, MF=0, Length=1500 (contains bytes 1480-2959, last fragment)
```

Note: Fragment offset 185 = 1480 bytes ÷ 8

### Reassembly Constraints

- Minimum IPv4 header size: 20 bytes
- Maximum IPv4 datagram: 65535 bytes
- Fragment offset: 13-bit field allows up to 65536×8 = 524288 bytes (exceeds 65535 max)
- Overlapping fragments: Keep first-arriving data, discard overlaps

## Checksum Validation

IPv4 header checksum covers only the header (not payload).

### Calculation

1. Set checksum field to 0
2. Sum all 16-bit words in header
3. Add carries back to sum
4. Take one's complement (flip all bits)
5. Result is checksum value

### Validation Modes

| Mode | Behavior |
|------|----------|
| **Strict** | Fail if checksum invalid |
| **Warning** | Log error but continue |
| **Disabled** | Skip validation |

## Protocol Numbers

Common protocol values in Protocol field:

| Number | Protocol | Description |
|--------|----------|-------------|
| 1 | ICMP | Internet Control Message Protocol |
| 6 | TCP | Transmission Control Protocol |
| 17 | UDP | User Datagram Protocol |
| 41 | IPv6 | IPv6 encapsulation |
| 47 | GRE | Generic Routing Encapsulation |
| 50 | ESP | Encapsulating Security Payload (IPsec) |
| 51 | AH | Authentication Header (IPsec) |
| 89 | OSPF | Open Shortest Path First |
| 103 | PIM | Protocol Independent Multicast |

## Common Issues & Troubleshooting

| Issue | Cause | Resolution |
|-------|-------|-----------|
| Fragment reassembly timeout | Slow network or lost fragment | Check MTU, verify all fragments arrive |
| Invalid checksum | Corrupted packet or NIC offloading | Verify NIC checksum offload settings |
| Unknown protocol | Proprietary or new protocol | Check Protocol field, add handler if needed |
| Option processing failure | Malformed option header | Verify IHL field, check option lengths |
| TTL exceeded | Too many hops | Verify routing path, check TTL settings |

## Code Examples

### Parsing IPv4 Header

```cpp
#include <wadjet/protocols/ipv4.hpp>
using namespace wadjet::protocols;

auto result = ipv4::Decoder::decode(packet_data);
if (result) {
    const auto& header = result.value();
    
    std::cout << "Version: " << (int)header.version << "\n";
    std::cout << "Source: " << header.src_ip.to_string() << "\n";
    std::cout << "DSCP: " << (int)header.dscp() << "\n";
    std::cout << "TTL: " << (int)header.ttl << "\n";
    
    // Check for fragmentation
    if (header.more_fragments_flag()) {
        std::cout << "Fragment offset: " << header.fragment_offset() << "\n";
    }
}
```

### Fragment Reassembly

```cpp
ipv4::Ipv4FragmentReassembler reassembler(std::chrono::seconds(30));

for (const auto& packet : packets) {
    auto reassembled = reassembler.add_fragment(packet);
    if (reassembled) {
        // Complete datagram reassembled
        process_complete_packet(reassembled.value());
    }
}
```

### Checksum Validation

```cpp
ipv4::Decoder decoder(ValidationMode::STRICT);
auto result = decoder.decode(packet_data);

if (!result) {
    // Checksum invalid in strict mode
    handle_invalid_checksum();
}
```

## References

- [RFC 791: Internet Protocol](https://tools.ietf.org/html/rfc791) - Core specification
- [RFC 1108: IP Security Option](https://tools.ietf.org/html/rfc1108) - Security option
- [RFC 2474: DiffServ ECN](https://tools.ietf.org/html/rfc2474) - DSCP/ECN
- [RFC 3168: ECN](https://tools.ietf.org/html/rfc3168) - Explicit Congestion Notification
- [RFC 3986: IPv4 Source Route Filtering](https://tools.ietf.org/html/rfc3986)

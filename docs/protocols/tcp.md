# TCP Protocol Reference

**Status**: ✅ Complete (M13)  
**RFC**: [RFC 793](https://tools.ietf.org/html/rfc793)  
**Implementation**: `include/wadjet/protocols/tcp.hpp`

## Overview

Complete TCP protocol implementation with full state machine tracking (11 states), connection lifecycle management, options parsing, and timeout handling.

## Header Structure

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Source Port          |       Destination Port        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgment Number                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Data |           |U|A|P|R|S|F|                               |
| Offset| Reserved  |R|C|S|S|Y|I|            Window Size        |
|       |           |G|K|H|T|N|N|                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Checksum            |       Urgent Pointer          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options (if Data Offset > 5)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## Fields

| Field | Bits | Description |
|-------|------|-------------|
| **Source Port** | 16 | Source port number (0-65535) |
| **Destination Port** | 16 | Destination port number (0-65535) |
| **Sequence Number** | 32 | Sequence number of first data byte |
| **Acknowledgment Number** | 32 | Sequence number of next expected byte (if ACK flag set) |
| **Data Offset** | 4 | Header length in 32-bit words (5-15, minimum 20 bytes) |
| **Reserved** | 3 | Must be 0 (reserved for future use) |
| **Flags** | 9 | Control flags (URG, ACK, PSH, RST, SYN, FIN, and others) |
| **Window Size** | 16 | Receiver's advertised window size (bytes) |
| **Checksum** | 16 | 16-bit ones-complement checksum (header + payload + pseudo-header) |
| **Urgent Pointer** | 16 | Offset from sequence number to urgent data (if URG flag set) |

## Flags

| Flag | Bit | Purpose |
|------|-----|---------|
| **FIN** | 0 | Finished - sender has no more data |
| **SYN** | 1 | Synchronize - synchronize sequence numbers (connection start) |
| **RST** | 2 | Reset - abort connection |
| **PSH** | 3 | Push - flush data immediately to application |
| **ACK** | 4 | Acknowledgment - ACK number field is valid |
| **URG** | 5 | Urgent - urgent data present (see Urgent Pointer) |
| **ECE** | 6 | ECN Echo - congestion experienced |
| **CWR** | 7 | Congestion Window Reduced - sender reduced window |
| **NS** | 8 | Nonce Sum - reserved for future use |

## TCP State Machine

TCP connections go through 11 possible states during their lifecycle:

```
                      CLOSED
                        |
                 (active OPEN)
                        |
                        v
                    SYN_SENT
                        |
          (recv SYN+ACK) |
                        v
                    ESTABLISHED
                   /    |    \
         (FIN)   /      |      \
                /       |       \
               v        |        v
         FIN_WAIT_1    |    CLOSING
              |        |       |
              |        |       |
              v        |       v
         FIN_WAIT_2    |   TIME_WAIT
              |        |       |
              | (FIN)  |       |
              v        v       v
          CLOSE_WAIT
              |
         (send FIN)
              |
              v
         LAST_ACK
              |
         (recv ACK)
              |
              v
            CLOSED
```

### State Descriptions

| State | Direction | Description |
|-------|-----------|-------------|
| **CLOSED** | Both | No connection exists |
| **LISTEN** | Passive | Waiting for incoming connection request |
| **SYN_SENT** | Active | Sent SYN, waiting for SYN+ACK |
| **SYN_RCVD** | Passive | Received SYN, sent SYN+ACK, waiting for ACK |
| **ESTABLISHED** | Both | Connection established, data transfer active |
| **FIN_WAIT_1** | Active | Sent FIN, waiting for ACK or FIN+ACK |
| **FIN_WAIT_2** | Active | Received ACK for our FIN, waiting for their FIN |
| **CLOSING** | Both | Both sides sent FIN, waiting for final ACK |
| **TIME_WAIT** | Both | Received FIN and ACK, waiting 2×MSL before closing |
| **CLOSE_WAIT** | Passive | Received FIN, sending remaining data then FIN |
| **LAST_ACK** | Passive | Sent FIN, waiting for final ACK |

### State Transitions

#### Active Open (Client)

```
CLOSED → (send SYN) → SYN_SENT
SYN_SENT → (recv SYN+ACK, send ACK) → ESTABLISHED
```

#### Passive Open (Server)

```
CLOSED → (listen) → LISTEN
LISTEN → (recv SYN, send SYN+ACK) → SYN_RCVD
SYN_RCVD → (recv ACK) → ESTABLISHED
```

#### Normal Close (Initiator)

```
ESTABLISHED → (send FIN) → FIN_WAIT_1
FIN_WAIT_1 → (recv ACK) → FIN_WAIT_2
FIN_WAIT_2 → (recv FIN) → TIME_WAIT
TIME_WAIT → (2×MSL timeout) → CLOSED
```

#### Normal Close (Receiver)

```
ESTABLISHED → (recv FIN) → CLOSE_WAIT
CLOSE_WAIT → (send FIN) → LAST_ACK
LAST_ACK → (recv ACK) → CLOSED
```

#### Simultaneous Close

```
FIN_WAIT_1 → (recv FIN) → CLOSING
CLOSING → (recv ACK) → TIME_WAIT
TIME_WAIT → (2×MSL timeout) → CLOSED
```

#### Reset Handling

```
Any state → (recv RST) → CLOSED
```

## Connection Tracking

### Connection Identification

Connections uniquely identified by 4-tuple:
- Source IP address
- Source port
- Destination IP address
- Destination port

### Timeout Values

| Event | Timeout | Purpose |
|-------|---------|---------|
| **ESTABLISHED** | 2 minutes | Idle connection timeout |
| **TIME_WAIT** | 30 seconds | Wait for delayed packets |
| **CLOSE_WAIT** | 30 seconds | Wait for application to finish sending |
| **SYN_RCVD** | 30 seconds | Half-open connection timeout |

### Sequence Number Tracking

| Field | Purpose |
|-------|---------|
| **Sequence Number (SN)** | Byte number of first data byte in segment |
| **Acknowledgment (ACK)** | Byte number of next expected segment |
| **Initial Sequence Number (ISN)** | First SN used in connection |

## TCP Options

Options appear after the header if Data Offset > 5.

### Option Format

```
+--------+--------+--------+--------+
| Type   | Length | Value  | ...    |
+--------+--------+--------+--------+
  1 byte  1 byte   Length-2 bytes
```

### Common Options

| Type | Name | Length | Description |
|------|------|--------|-------------|
| 0 | EOL | 1 | End of Option List |
| 1 | NOP | 1 | No Operation (padding) |
| 2 | MSS | 4 | Maximum Segment Size |
| 3 | WSOPT | 3 | Window Scale |
| 4 | SACK-Perm | 2 | SACK Permitted |
| 5 | SACK | Variable | Selective Acknowledgment |
| 8 | TSTAMP | 10 | Timestamps |
| 9 | MPTCP | Variable | Multipath TCP |
| 28 | UTO | 4 | User Timeout |

### Maximum Segment Size (MSS)

Largest segment size the sender can transmit:

```
Type=2, Length=4, Value (2 bytes)
```

Common values:
- 1460 bytes for Ethernet (1500 MTU - 40 byte headers)
- 536 bytes for legacy networks

### Window Scale Option

Allows window size up to 1GB (bypasses 64KB limit):

```
Type=3, Length=3, Shift count (1 byte)
```

Actual window = advertised window × 2^shift_count

### Timestamp Option

Enables round-trip time measurement:

```
Type=8, Length=10, TSval (4 bytes), TSecr (4 bytes)
```

### SACK (Selective Acknowledgment)

Receiver acknowledges non-contiguous received blocks:

```
Type=5, Length=Variable, Blocks...
```

Each block: Left Edge (4 bytes) + Right Edge (4 bytes)

## Checksum Validation

TCP checksum covers header, payload, AND pseudo-header from IP layer.

### Pseudo-Header (for IPv4)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Source Address                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination Address                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      Zero     |    Protocol   |        TCP Length            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Validation Algorithm

1. Construct pseudo-header from IP layer
2. Concatenate pseudo-header + TCP header + payload
3. Set checksum field to 0
4. Sum all 16-bit words
5. Add carries to sum
6. Take one's complement
7. Compare with transmitted checksum

## Retransmission & Recovery

### Retransmission Timeout (RTO)

Calculated using RTT samples:

```
SRTT = α × SRTT + (1-α) × RTT sample     (typically α=0.875)
RTTVAR = β × RTTVAR + (1-β) × |RTT - SRTT|  (typically β=0.75)
RTO = SRTT + 4 × RTTVAR
```

Initial RTO typically 1 second, backoff to 60+ seconds.

### Fast Retransmit

When 3 duplicate ACKs received before RTO, immediately retransmit.

### Selective Acknowledgment (SACK)

Receiver can acknowledge non-contiguous ranges, allowing sender to:
- Retransmit only truly lost segments
- Avoid retransmitting already-received data

## Common Issues & Troubleshooting

| Issue | Cause | Resolution |
|-------|-------|-----------|
| Connection stuck in CLOSE_WAIT | Application not closing | Verify application handles close properly |
| TIME_WAIT accumulation | Too many simultaneous connections | Tune kernel TIME_WAIT reuse parameters |
| Sequence number wrap | Long-lived connections | Use PAWS (Protection Against Wrapped Sequences) with timestamps |
| Invalid checksum | Corrupted segment or NIC offload | Verify NIC checksum offload settings |
| Retransmission storm | Network congestion | Implement backoff, check RTT measurement |

## Code Examples

### Parsing TCP Header

```cpp
#include <wadjet/protocols/tcp.hpp>
using namespace wadjet::protocols;

auto result = tcp::Decoder::decode(packet_data);
if (result) {
    const auto& header = result.value();
    
    std::cout << "Src Port: " << header.src_port << "\n";
    std::cout << "Dst Port: " << header.dst_port << "\n";
    std::cout << "Sequence: " << header.sequence_number << "\n";
    std::cout << "Flags: ";
    if (header.syn()) std::cout << "SYN ";
    if (header.ack()) std::cout << "ACK ";
    if (header.fin()) std::cout << "FIN ";
    std::cout << "\n";
}
```

### Connection State Tracking

```cpp
tcp::ConnectionTracker tracker;

for (const auto& packet : packets) {
    auto result = tcp::Decoder::decode(packet);
    if (result) {
        const auto& header = result.value();
        auto state = tracker.track_packet(header);
        
        std::cout << "Connection state: " 
                  << tcp::state_to_string(state) << "\n";
    }
}
```

### MSS Extraction from Options

```cpp
const auto& header = parsed_tcp_header;
for (const auto& option : header.options) {
    if (option.type == tcp::OptionType::MSS) {
        uint16_t mss = option.value<uint16_t>();
        std::cout << "MSS: " << mss << " bytes\n";
    }
}
```

## References

- [RFC 793: TCP](https://tools.ietf.org/html/rfc793) - Core specification
- [RFC 1323: TCP Extensions](https://tools.ietf.org/html/rfc1323) - Timestamps, window scaling
- [RFC 2018: SACK](https://tools.ietf.org/html/rfc2018) - Selective Acknowledgment
- [RFC 5961: TCP Robustness](https://tools.ietf.org/html/rfc5961) - Security improvements
- [RFC 6298: RTO Computation](https://tools.ietf.org/html/rfc6298) - Timeout calculation

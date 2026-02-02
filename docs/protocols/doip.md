# DoIP Protocol Reference

**Status**: ✅ Complete (M13)  
**Standard**: [ISO 13400-2](https://www.iso.org/standard/74785.html)  
**Implementation**: `include/wadjet/protocols/doip.hpp`

## Overview

Diagnostic Communication over Internet Protocol (DoIP) - ISO standard for vehicle diagnostics over Ethernet. Enables UDS (Unified Diagnostic Services) to run on top of IP networks instead of CAN.

## Header Structure

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Protocol Version (0x01, 0x02, 0x03, etc.)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Inverse Protocol Version (0xFE, 0xFD, 0xFC, etc.)         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Payload Type (32 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Payload Length (32 bits)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Payload (Variable)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## Header Fields

| Field | Bytes | Description |
|-------|-------|-------------|
| **Protocol Version** | 1 | DoIP version (0x01 for ISO 13400-2:2010) |
| **Inverse Version** | 1 | Bitwise NOT of version (0xFE for 0x01) |
| **Payload Type** | 4 | Type of DoIP message (see table below) |
| **Payload Length** | 4 | Length of payload in bytes (0-65535) |
| **Payload** | Variable | Message-specific data |

## Payload Types

| Type | Code | Direction | Purpose |
|------|------|-----------|---------|
| Generic Negative ACK | 0x0000 | Bi | Error indication |
| Vehicle Identity Request | 0x0100 | Client→Server | Query vehicle info |
| Vehicle Announcement | 0x0101 | Server→Client | Vehicle identity response |
| Routing Activation Request | 0x0005 | Client→Server | Establish diagnostic session |
| Routing Activation Response | 0x0006 | Server→Client | Session established |
| Alive Check Request | 0x0007 | Bi | Keep-alive heartbeat |
| Alive Check Response | 0x0008 | Bi | Heartbeat reply |
| Diagnostic Message | 0x8001 | Bi | UDS data (diagnostic request/response) |
| Diagnostic Message-ACK | 0x8002 | Bi | Acknowledgment of diagnostic |
| Diagnostic Message-NACK | 0x8003 | Bi | Negative response to diagnostic |

## Routing Activation Request (0x0005)

Initiates diagnostic session:

```
 0               15 16              31
+--------+--------+--------+--------+
| Source | Target | Type   | Reserved|
| Address| Address|        |        |
+--------+--------+--------+--------+
|          Reserved (4 bytes)       |
+--------+--------+--------+--------+
|            Auth Info (Optional)   |
+--------+--------+--------+--------+
```

| Field | Bits | Description |
|-------|------|-------------|
| **Source Address** | 16 | Test equipment address (tester ID) |
| **Target Address** | 16 | Target ECU address (node ID) |
| **Activation Type** | 8 | 0x00=Default, 0x01=WWH-OBD |
| **Reserved** | 8+32 | Must be 0 |
| **Auth Info** | 4 | Optional authentication data |

### Activation Types

| Type | Name | Purpose |
|------|------|---------|
| 0x00 | Default | Normal diagnostic session |
| 0x01 | WWH-OBD | World-Wide Harmonized OBD session |

## Routing Activation Response (0x0006)

Session establishment confirmation:

```
 0               15 16              31
+--------+--------+--------+--------+
| Source | Target | Response Code  |
| Address| Address|                |
+--------+--------+--------+--------+
|             Reserved (4 bytes)    |
+--------+--------+--------+--------+
|          Diag Power Mode (1 byte) |
+--------+--------+--------+--------+
```

| Field | Bits | Description |
|-------|------|-------------|
| **Response Code** | 16 | 0x0010=OK, others=error |
| **Diag Power Mode** | 8 | Vehicle power state |

### Response Codes

| Code | Name | Meaning |
|------|------|---------|
| 0x0010 | OK | Routing activated successfully |
| 0x0011 | Routing Denied | Client not authorized |
| 0x0012 | Negative ACK | Routing rejected |
| 0x0013 | Alternative Route | Use different route |
| 0x0014 | Connection Refused | ECU refusing connections |

### Diagnostic Power Mode

| Mode | Code | State | Meaning |
|------|------|-------|---------|
| Sleep | 0x00 | OFF | Vehicle sleeping (low power) |
| Ready | 0x01 | ON | Engine running or ready |
| Not Ready | 0x02 | STANDBY | Vehicle powered but not ready |

## Vehicle Identity Request (0x0100)

Discover vehicle information:

```
0 bytes - no payload (just header)
```

## Vehicle Announcement (0x0101)

Vehicle identity response:

```
+--------+--------+--------+--------+
| VIN (17 bytes for Vehicle ID Number) |
+--------+--------+--------+--------+
| Serial (10 bytes)                  |
+--------+--------+--------+--------+
| HW Version (6 bytes)               |
+--------+--------+--------+--------+
| ECU Activation Records (optional)  |
+--------+--------+--------+--------+
```

### Vehicle Identification Number (VIN)

17-byte ASCII string:

```
Position  Content       Example
1         Manufacturer  'W' (VW)
2-3       Brand/Model   'VA' (Volkswagen)
4-8       Series        'TWA' + 2 digits
9         Check digit   Various
10        Model year    'T' (2020)
11        Assembly      '7' (Mexico)
12-17     Serial        '123456'
```

## Diagnostic Message (0x8001)

UDS diagnostic request/response encapsulated:

```
+--------+--------+--------+--------+
| Source | Target | Reserved       |
| Address| Address|                |
+--------+--------+--------+--------+
|      UDS Data (Variable Length)   |
+--------+--------+--------+--------+
```

**Examples**:
- 0x10 01 — Enter diagnostic session
- 0x22 F1 90 — Read DTC
- 0x2E 01 02 — Write data

## Alive Check (0x0007/0x0008)

Heartbeat to keep session alive:

```
Request:  Just header, no payload
Response: Just header, no payload
```

Used to:
- Detect disconnected clients
- Keep NAT timeout alive
- Verify connection still active

## Negative ACK (0x0000)

Error response:

```
+--------+--------+--------+--------+
| NACK Code (32 bits)                |
+--------+--------+--------+--------+
```

| Code | Name | Meaning |
|------|------|---------|
| 0x00000001 | Invalid Header | Malformed DoIP header |
| 0x00000002 | Unknown Payload Type | Unsupported message type |
| 0x00000003 | Message Too Large | Payload exceeds limits |
| 0x00000004 | Out of Memory | Server resource exhausted |
| 0x00000005 | Busy | Retry later |
| 0x00000006 | Invalid Routing State | Routing not activated |

## TCP Connection Handling

DoIP runs over TCP (port 13400 typical):

| State | Behavior | Timeout |
|-------|----------|---------|
| **Established** | Normal operation | 30s idle |
| **No Activity** | Send Alive Check | Every 15s |
| **No Response** | Disconnect | 30s |
| **After Diagnostics** | Keep-alive sent | Always |

## Power Mode Transitions

```
Sleep → (wake signal) → Ready → (engine off) → Not Ready → Sleep
```

**Implications for Diagnostics**:
- **Sleep**: No diagnostics possible
- **Ready**: Full diagnostics available
- **Not Ready**: Limited diagnostics (power but no engine)

### Detecting Power Mode

```
Routine 1: Check voltage
- >12.5V = Ready
- 8-12.5V = Not Ready  
- <8V = Sleep
```

## Source/Target Addressing

Both use 16-bit node identifiers (tester and ECU addresses):

| Range | Ownership | Purpose |
|-------|-----------|---------|
| 0x0000-0x0FFF | Tester | Test equipment addresses |
| 0x1000-0x7FFF | OEM | Original equipment addresses |
| 0x8000-0xFFFF | Supplier | Supplier/third-party addresses |

## TCP Port Assignments

| Service | Port | Description |
|---------|------|-------------|
| **DoIP** | 13400 | Standard DoIP server port |
| **Legacy** | 6801 | Older implementations |
| **Custom** | User | OEM-specific ports |

## Security Considerations

### Authentication

Routing activation can require:
- **API Key**: 4-byte authorization token
- **Challenge-Response**: Tester authenticates to ECU
- **Certificate**: X.509 client certificate

### Encryption

Optional TLS/SSL for:
- Protecting diagnostic data in transit
- Preventing unauthorized access
- Meeting data privacy regulations

## Common Issues & Troubleshooting

| Issue | Cause | Resolution |
|-------|-------|-----------|
| Routing activation refused | Wrong tester address | Verify authorized address in ECU config |
| Power mode unavailable | Vehicle sleeping | Wake vehicle, check voltage |
| Connection timeout | Firewall blocking port 13400 | Check network ACLs, firewalls |
| Message too large | Payload > 65535 bytes | Split across multiple messages |
| No alive check response | Connection dead | Reconnect, check network |
| Authentication failed | Wrong credentials | Verify auth key/certificate |

## Code Examples

### Parsing DoIP Header

```cpp
#include <wadjet/protocols/doip.hpp>
using namespace wadjet::protocols;

auto result = doip::Decoder::decode(packet_data);
if (result) {
    const auto& header = result.value();
    
    std::cout << "Protocol Version: " << (int)header.protocol_version << "\n";
    std::cout << "Payload Type: 0x" << std::hex 
              << header.payload_type << "\n";
    std::cout << "Payload Length: " << std::dec 
              << header.payload_length << "\n";
}
```

### Extracting UDS from Diagnostic Message

```cpp
if (header.payload_type == 0x8001) {
    // Extract UDS from payload
    const auto& uds_data = header.payload;
    uint8_t uds_service = uds_data[0];
    
    std::cout << "UDS Service: 0x" << std::hex 
              << (int)uds_service << "\n";
}
```

### Power Mode Detection

```cpp
if (header.payload_type == 0x0006) {
    // Routing activation response
    uint8_t power_mode = header.payload[8];
    
    switch (power_mode) {
        case 0x00: std::cout << "Sleep\n"; break;
        case 0x01: std::cout << "Ready\n"; break;
        case 0x02: std::cout << "Not Ready\n"; break;
    }
}
```

## References

- [ISO 13400-2:2019](https://www.iso.org/standard/74785.html) - DoIP specification
- SAE J1939 - Truck/bus diagnostics
- [AUTOSAR CDD](https://www.autosar.org/) - Automotive diagnostics standard
- IEEE 802.3 - Ethernet physical layer

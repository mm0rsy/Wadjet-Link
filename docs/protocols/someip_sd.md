# SOME/IP-SD Protocol Reference

**Status**: ✅ Complete (M13)  
**Spec**: AUTOSAR PRS_SOMEIPSD  
**Implementation**: `include/wadjet/protocols/someip_sd.hpp`

## Overview

SOME/IP Service Discovery (SD) - AUTOSAR protocol for dynamic service discovery in automotive networks. Enables automatic detection of service availability, multi-instance support, and configuration.

## Packet Structure

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Service ID (0xFFFE)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Method ID (0x0001/0x8001)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Length (variable)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Request ID (32 bits)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Ver |  Hdr Type  | Message Type | Return Code |    Flags      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Reserved (32 bits)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Entries Length (32 bits)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              Entry Array (Variable Length)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Options Length (32 bits)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Options Array (Variable Length)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## SD Header Fields

| Field | Bits | Description |
|-------|------|-------------|
| **Service ID** | 16 | Always 0xFFFE for SD |
| **Method ID** | 16 | 0x0001=Request, 0x8001=Notification |
| **Protocol Ver** | 4 | Always 0x1 |
| **Header Type** | 4 | 0x0=Request, 0x2=Response |
| **Message Type** | 8 | SD message type |
| **Return Code** | 8 | 0x0=OK |
| **Flags** | 8 | Reboot=1, Unicast=1, other reserved |
| **Reserved** | 32 | Must be 0 |
| **Entries Length** | 32 | Size of entries array in bytes |
| **Options Length** | 32 | Size of options array in bytes |

## Entry Types

SD entries describe service availability, request, or configuration. Each entry is 16 bytes.

### Entry Header

```
 0               15 16              31
+-------+-------+--------+----------+
| Type  | Index | NO | RP | Counter |
+-------+-------+--------+----------+
  1 byte  1 byte  2bits  1bit  2bits
```

| Field | Bits | Description |
|-------|------|-------------|
| **Type** | 8 | Entry type (0-5) |
| **Index** | 8 | Index into options array (0xFF if none) |
| **No Multicast** | 1 | 1 = Don't multicast this entry |
| **Request-MatchingFlag** | 1 | 1 = Entry matches request |
| **Counter** | 14 | Counter value |

### Entry Type 0: Service Entry

Announces service availability:

```
Type=0, Index, Flags, Counter (2 bytes)
+--------+--------+--------+--------+
| Type=0 | Index  |  Flags | Ctr    |
+--------+--------+--------+--------+
| Reserved (4 bytes)                |
+--------+--------+--------+--------+
| Service ID (2 bytes) | Instance ID |
+--------+--------+--------+--------+
| Major Ver | Minor Version (24 bits) |
+--------+--------+--------+--------+
| TTL (3 bytes) |        Reserved   |
+--------+--------+--------+--------+
```

**Purpose**: Server announces service is available
**Example**: ECU offers diagnostics service 0x0001

### Entry Type 1: Instance Entry

Instance of a service:

```
Type=1, Index, Flags, Counter
+ Service ID (2) + Instance ID (2)
+ Major Ver (1) + Minor Version (3)
+ TTL (3) | Reserved (1)
```

**Purpose**: Specific instance of service available
**Example**: Diagnostics on ECU #1 vs ECU #2

### Entry Type 2: Configuration Entry

Service configuration metadata:

```
Type=2, Index, Flags, Counter
+ Service ID (2) + Instance ID (2)
+ Major Ver (1) + Minor Version (3)
+ TTL (3) | Reserved (1)
```

**Purpose**: Configuration offered by service
**Example**: Service supports these AUTOSAR versions

### Entry Type 3: Load Balancing Entry

Load balancing information:

```
Type=3, Index, Flags, Counter
+ Service ID (2) + Instance ID (2)
+ Major Ver (1) + Minor Version (3)
+ TTL (3) | Reserved (1)
```

**Purpose**: Distribute load across instances
**Example**: Prefer least-loaded ECU

### Entry Type 4: Protection Option Entry

Protection information:

```
Type=4, Index, Flags, Counter
+ Service ID (2) + Instance ID (2)
+ Major Ver (1) + Minor Version (3)
+ TTL (3) | Reserved (1)
```

**Purpose**: Security/redundancy information
**Example**: Service requires encryption

### Entry Type 5: Subscription Entry

Subscription acknowledgment:

```
Type=5, Index, Flags, Counter
+ Service ID (2) + Instance ID (2)
+ Major Ver (1) + Minor Version (3)
+ TTL (3) | Reserved (1)
```

**Purpose**: Acknowledge event subscription
**Example**: Client subscribed to service events

## Option Types

Options provide additional configuration data referenced by entries.

### Option Header

```
 0               15 16              31
+----------+--------+--------+-------+
| Length   | Type   | Index  | Rsvd  |
+----------+--------+--------+-------+
 2 bytes    1 byte    1 byte  1 byte
```

| Field | Bits | Description |
|-------|------|-------------|
| **Length** | 16 | Option length in bytes (including header) |
| **Type** | 8 | Option type |
| **Index** | 8 | Reserved (always 0) |
| **Reserved** | 8 | Must be 0 |

### Option Type 0x01: Configuration Option

Service configuration:

```
Type=0x01, Length, Reserved
+ Config String (variable)
```

**Purpose**: Configuration string for service
**Example**: "IPv4:192.168.1.1:30490"

### Option Type 0x02: Load Balancing Option

Load balancing preference:

```
Type=0x02, Length=6, Reserved
+ Priority (2 bytes)
+ Weight (2 bytes)
```

| Field | Bits | Description |
|-------|------|-------------|
| **Priority** | 16 | Service priority (0=high, 0xFFFF=low) |
| **Weight** | 16 | Load distribution weight |

### Option Type 0x03: Protection Option

Security/redundancy:

```
Type=0x03, Length=4, Reserved
+ Flags (1 byte)
+ Reserved (3 bytes)
```

| Flag Bit | Meaning |
|----------|---------|
| 0 | Requires encryption |
| 1 | Requires authentication |
| 2 | Redundant |
| 3-7 | Reserved |

### Option Type 0x04: Endpoint Option

Service endpoint address:

```
Type=0x04, Length=12, Reserved
+ Address Type (1 byte)
+ Reserved (1 byte)
+ Protocol (1 byte)
+ Reserved (1 byte)
+ Port (2 bytes)
+ Address (4 bytes for IPv4, 16 for IPv6)
```

| Field | Value | Meaning |
|-------|-------|---------|
| **Address Type** | 0 | IPv4 |
| **Address Type** | 1 | IPv6 |
| **Protocol** | 0x06 | TCP |
| **Protocol** | 0x11 | UDP |

## Message Types

| Type | Code | Direction | Purpose |
|------|------|-----------|---------|
| **Find Service** | 0x00 | Client→Server | Request service discovery |
| **Offer Service** | 0x01 | Server→Client | Announce service availability |
| **Subscribe** | 0x02 | Client→Server | Subscribe to events |
| **Subscribe-Ack** | 0x03 | Server→Client | Acknowledge subscription |
| **Stop-Offer** | 0x04 | Server→Client | Service no longer available |
| **Stop-Subscribe** | 0x05 | Client→Server | Cancel subscription |
| **Stop-Subscribe-Ack** | 0x06 | Server→Client | Acknowledge unsubscribe |

## Discovery Workflow

### Service Discovery (Request-Response)

```
Client                          Server
  |                              |
  |---FindService (SD)---------->|
  |  (Service 0x0001)             |
  |                              |
  |<-------OfferService (SD)------|
  |  (Service 0x0001 available)   |
  |                              |
```

### Event Subscription

```
Client                          Server
  |                              |
  |---Subscribe (SD)------------>|
  |  (Event 0x0001:0x8001)        |
  |                              |
  |<---SubscribeAck (SD)---------|
  |                              |
  |<---Event Notification--------|
  |  (Event 0x0001:0x8002)       |
  |                              |
```

## TTL (Time To Live)

Service entries include 24-bit TTL in deciseconds:

| TTL Value | Meaning |
|-----------|---------|
| 0 | Entry invalid/withdrawn |
| 1-65535 | Valid for N × 100ms |
| 0xFFFFFF | Indefinite |

## Unicast vs Multicast

### Multicast (Default)

- **Address**: 224.244.224.245:30490
- **Purpose**: Broadcast service discovery
- **Traffic**: Can be high in large networks
- **Filtering**: Set No-Multicast flag to avoid broadcast

### Unicast

- **When**: Service replies with unicast=1
- **Purpose**: Direct server-to-client response
- **Efficiency**: Lower broadcast traffic
- **Use Case**: Specific service availability check

## Common Service IDs

| ID | Service | Typical Events |
|----|---------|-----------------|
| 0x0001 | Diagnostics | Status, errors |
| 0x0002 | Infotainment | Ready, error |
| 0x0003 | Climate | Mode change, error |
| 0x0004 | Powertrain | RPM, torque |
| 0x0005 | Body | Window/door status |
| 0x0006 | Safety | Airbag, stability |
| 0x0007 | Telematics | GPS, connectivity |

## Code Examples

### Parsing SD Entries

```cpp
#include <wadjet/protocols/someip_sd/sd.hpp>
using namespace wadjet::protocols::someip_sd;

auto result = SdDecoder::decode(packet_data);
if (result) {
    const auto& sd_msg = result.value();
    
    for (const auto& entry : sd_msg.entries) {
        if (entry.type == EntryType::SERVICE) {
            std::cout << "Service 0x" << std::hex 
                      << entry.service_id << " available\n";
        }
    }
}
```

### Processing SD Options

```cpp
for (const auto& option : sd_msg.options) {
    if (option.type == OptionType::ENDPOINT) {
        std::cout << "Endpoint: " 
                  << option.get_address() << ":"
                  << option.get_port() << "\n";
    }
}
```

## References

- AUTOSAR Standard PRS_SOMEIPSD - Service Discovery Specification
- [AUTOSAR Specification](https://www.autosar.org/) - Official standard
- AUTOSAR SWS_ServiceDiscovery

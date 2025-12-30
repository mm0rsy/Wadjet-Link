# YAML/JSON Scenario Format

This document describes the scenario file format used by `wadjet-run` for declarative test automation.

## Overview

Scenarios are YAML or JSON files that define automated network tests. They support:

- Packet capture configuration
- Protocol-specific expectations
- Time-bounded assertions
- Logging and debugging

## Basic Structure

```yaml
name: "My Test Scenario"
description: "Description of what this test validates"
tags: [smoke, fast]

steps:
  - capture:
      interface: eth0
      filter: "udp port 30490"
      
  - expect:
      someip:
        service_id: 0x1234
      within_ms: 1000
```

## Step Types

### `capture` - Start Packet Capture

```yaml
- capture:
    interface: eth0           # Network interface
    filter: "udp port 30490"  # BPF filter expression (optional)
    promiscuous: true         # Promiscuous mode (default: true)
    snaplen: 65535            # Max capture length (default: 65535)
```

### `expect` - Wait for Matching Packet

```yaml
- expect:
    # Protocol expectations (at least one required)
    ethernet:
      src_mac: "AA:BB:CC:DD:EE:FF"
      dst_mac: "00:11:22:33:44:55"
      ethertype: 0x0800
      
    ipv4:
      src_ip: "192.168.1.100"
      dst_ip: "192.168.1.200"
      protocol: 17              # UDP
      
    udp:
      src_port: 30490
      dst_port: 30490
      
    tcp:
      src_port: 13400
      dst_port: 13400
      
    someip:
      service_id: 0x1234
      method_id: 0x0001
      client_id: 0x5678
      session_id: 0x0001
      message_type: request     # request, response, notification, error
      return_code: ok           # ok, not_ok, unknown_service, etc.
      
    someip_sd:
      reboot_flag: true
      unicast_flag: true
      
    doip:
      payload_type: diagnostic_message
      source_address: 0x0E80
      target_address: 0x1000
      
    # Timing
    within_ms: 1000            # Timeout in milliseconds
    
    # Count (optional)
    count: ">= 3"              # Count expression
```

### `send` - Transmit Packet

```yaml
- send:
    interface: eth0
    
    # Build packet layers
    ethernet:
      src_mac: "AA:BB:CC:DD:EE:FF"
      dst_mac: "00:11:22:33:44:55"
      
    ipv4:
      src_ip: "192.168.1.100"
      dst_ip: "192.168.1.200"
      
    udp:
      src_port: 30490
      dst_port: 30490
      
    someip:
      service_id: 0x1234
      method_id: 0x0001
      message_type: request
      
    payload: "0102030405"      # Hex string
```

### `wait` - Delay Execution

```yaml
- wait:
    duration_ms: 500           # Wait for 500 milliseconds
```

### `log` - Output Message

```yaml
- log:
    message: "Starting test phase 2"
    level: info                # debug, info, warn, error
```

## Protocol Expectations

### SOME/IP Message Types

| Value | Description |
|-------|-------------|
| `request` | Client request |
| `request_no_return` | Fire-and-forget |
| `notification` | Event notification |
| `response` | Method response |
| `error` | Error response |

### SOME/IP Return Codes

| Value | Description |
|-------|-------------|
| `ok` | E_OK (0x00) |
| `not_ok` | E_NOT_OK (0x01) |
| `unknown_service` | E_UNKNOWN_SERVICE (0x02) |
| `unknown_method` | E_UNKNOWN_METHOD (0x03) |
| `not_ready` | E_NOT_READY (0x04) |
| `not_reachable` | E_NOT_REACHABLE (0x05) |
| `timeout` | E_TIMEOUT (0x06) |

### DoIP Payload Types

| Value | Description |
|-------|-------------|
| `vehicle_identification_request` | 0x0001 |
| `vehicle_identification_response` | 0x0004 |
| `routing_activation_request` | 0x0005 |
| `routing_activation_response` | 0x0006 |
| `alive_check_request` | 0x0007 |
| `alive_check_response` | 0x0008 |
| `diagnostic_message` | 0x8001 |
| `diagnostic_message_positive_ack` | 0x8002 |
| `diagnostic_message_negative_ack` | 0x8003 |

## Count Expressions

The `count` field supports comparison operators:

```yaml
count: "== 5"      # Exactly 5 packets
count: "!= 0"      # At least one packet
count: "> 3"       # More than 3 packets
count: ">= 3"      # 3 or more packets
count: "< 10"      # Fewer than 10 packets
count: "<= 10"     # 10 or fewer packets
```

## Duration Parsing

Time values support multiple units:

```yaml
within_ms: 1000        # Milliseconds
duration_ms: 500       # Milliseconds

# Alternative formats (in duration fields):
duration: "1s"         # 1 second
duration: "500ms"      # 500 milliseconds
duration: "2m"         # 2 minutes
duration: "1h"         # 1 hour
```

## Complete Examples

### SOME/IP Service Discovery Test

```yaml
name: "SOME/IP Service Discovery"
description: "Verify service offers appear within timeout"
tags: [someip, sd, smoke]

steps:
  - capture:
      interface: eth0
      filter: "udp port 30490"
      
  - log:
      message: "Waiting for service offers..."
      
  - expect:
      someip:
        service_id: 0xFFFF
      someip_sd:
        reboot_flag: true
      within_ms: 2000
      count: ">= 1"
      
  - log:
      message: "Service Discovery offers received"
```

### DoIP Routing Activation Test

```yaml
name: "DoIP Routing Activation"
description: "Validate DoIP routing activation sequence"
tags: [doip, diagnostics]

steps:
  - capture:
      interface: eth0
      filter: "tcp port 13400"
      
  - expect:
      doip:
        payload_type: routing_activation_request
        source_address: 0x0E80
      within_ms: 1000
      
  - expect:
      doip:
        payload_type: routing_activation_response
      within_ms: 500
      
  - log:
      message: "Routing activation successful"
```

### Multi-Phase Test

```yaml
name: "ECU Bootup Sequence"
description: "Monitor ECU initialization traffic"
tags: [bootup, integration]

steps:
  - capture:
      interface: eth0
      
  # Phase 1: Wait for initial announcement
  - log:
      message: "Phase 1: Waiting for initial announcement"
      
  - expect:
      someip_sd:
        reboot_flag: true
      within_ms: 5000
      
  # Phase 2: Wait for service offers
  - log:
      message: "Phase 2: Waiting for service offers"
      
  - wait:
      duration_ms: 500
      
  - expect:
      someip:
        service_id: 0x1234
        message_type: notification
      within_ms: 2000
      count: ">= 3"
      
  # Phase 3: Verify diagnostic availability
  - log:
      message: "Phase 3: Verifying diagnostic port"
      
  - expect:
      doip:
        payload_type: vehicle_identification_response
      within_ms: 1000
```

## JSON Format

Scenarios can also be written in JSON:

```json
{
  "name": "Basic UDP Test",
  "description": "Simple UDP packet capture test",
  "tags": ["udp", "smoke"],
  "steps": [
    {
      "capture": {
        "interface": "eth0",
        "filter": "udp"
      }
    },
    {
      "expect": {
        "udp": {
          "dst_port": 12345
        },
        "within_ms": 1000
      }
    }
  ]
}
```

## CLI Usage

```bash
# Run a single scenario
wadjet-run scenario.yaml -i eth0

# Run with different output format
wadjet-run scenario.yaml -i eth0 -o junit:results.xml
wadjet-run scenario.yaml -i eth0 -o json:results.json

# Dry-run (validate without capture)
wadjet-run scenario.yaml --dry-run

# Run all scenarios in directory
wadjet-run examples/scenarios/ -i eth0

# Filter by tags
wadjet-run examples/ --tags smoke,fast

# Verbose output
wadjet-run scenario.yaml -i eth0 --verbose
```

---

*𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.*

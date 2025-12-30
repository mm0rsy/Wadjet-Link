# Example Test Scenarios

This directory contains example YAML and JSON test scenarios for `wadjet-run`.

## Available Examples

| File | Description | Tags |
|------|-------------|------|
| `basic_udp_test.yaml` | Basic UDP packet capture and validation | - |
| `someip_sd_test.yaml` | SOME/IP Service Discovery test | smoke, someip |
| `doip_routing_test.json` | DoIP routing activation test (JSON format) | smoke, doip |

## Running Examples

```bash
# Run all examples in dry-run mode (no live capture)
wadjet-run --dry-run --dir examples/scenarios/

# List all scenarios
wadjet-run --list --dir examples/scenarios/

# Run specific scenario
wadjet-run examples/scenarios/someip_sd_test.yaml

# Run with specific interface
wadjet-run -i eth0 examples/scenarios/someip_sd_test.yaml

# Run only smoke tests
wadjet-run -t smoke --dir examples/scenarios/

# Generate JUnit report
wadjet-run --dir examples/scenarios/ -f junit -o results.xml
```

## Scenario Format

### YAML Example

```yaml
name: My Test Scenario
description: What this test validates
version: "1.0"
author: Your Name
tags: [smoke, regression]
timeout_ms: 5000

steps:
  - capture:
      interface: eth0
      filter: "udp port 30490"

  - wait:
      duration_ms: 100

  - expect:
      description: "Should receive SOME/IP service offer"
      timeout_ms: 1000
      count: ">= 1"
      someip:
        service_id: 0x1234
        message_type: notification
```

### JSON Example

```json
{
  "name": "My Test Scenario",
  "description": "What this test validates",
  "tags": ["smoke"],
  "timeout_ms": 5000,
  "steps": [
    {
      "capture": {
        "interface": "eth0",
        "filter": "tcp port 13400"
      }
    },
    {
      "expect": {
        "description": "DoIP routing activation",
        "timeout_ms": 500,
        "doip": {
          "payload_type": 5
        }
      }
    }
  ]
}
```

## Step Types

| Step | Purpose |
|------|---------|
| `capture` | Start packet capture on interface with optional BPF filter |
| `wait` | Pause execution for specified duration |
| `send` | Send packets from a PCAP file |
| `expect` | Assert packet conditions within timeout |
| `log` | Output a message (for debugging) |

## Count Expressions

The `count` field in `expect` steps supports comparison operators:

| Expression | Meaning |
|------------|---------|
| `5` | Exactly 5 packets |
| `>= 1` | At least 1 packet |
| `> 0` | More than 0 packets |
| `<= 10` | At most 10 packets |
| `!= 0` | Not zero packets |

## Protocol Filters

### Ethernet

```yaml
expect:
  ethernet:
    source_mac: "00:11:22:33:44:55"
    dest_mac: "ff:ff:ff:ff:ff:ff"
    ethertype: 0x0800
    vlan_id: 100
```

### IPv4

```yaml
expect:
  ipv4:
    source_ip: "192.168.1.1"
    dest_ip: "192.168.1.255"
    protocol: 17  # UDP
```

### UDP/TCP

```yaml
expect:
  udp:
    source_port: 30490
    dest_port: 30490

expect:
  tcp:
    source_port: 13400
    dest_port: 13400
```

### SOME/IP

```yaml
expect:
  someip:
    service_id: 0x1234
    method_id: 0x0001
    message_type: request  # request, response, notification, error
```

### DoIP

```yaml
expect:
  doip:
    payload_type: 5  # Routing activation response
```

## Creating New Scenarios

1. Copy an existing example as a template
2. Modify the `name`, `description`, and `tags`
3. Define your `steps` in order
4. Test with `--dry-run` first to validate syntax
5. Run against live traffic or test fixtures

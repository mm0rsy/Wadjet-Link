# Wadjet-Link Examples

This directory contains examples demonstrating Wadjet-Link features.

## Contents

| Example | Description |
|---------|-------------|
| [someip_discovery.cpp](someip_discovery.cpp) | SOME/IP Service Discovery monitor |
| [doip_routing.cpp](doip_routing.cpp) | DoIP routing activation validator |
| [ecu_bootup.cpp](ecu_bootup.cpp) | ECU bootup sequence monitor |
| [pcap_regression.cpp](pcap_regression.cpp) | GoogleTest-based PCAP regression tests |
| [scenarios/](scenarios/) | YAML/JSON test scenarios for `wadjet-run` CLI |

## Building Examples

```bash
# Configure with examples enabled
cmake -B build -DWADJET_BUILD_EXAMPLES=ON

# Build all examples
cmake --build build

# Run individual examples
./build/examples/someip_discovery eth0
./build/examples/doip_routing capture.pcap
./build/examples/ecu_bootup eth0
```

## Example Details

### SOME/IP Service Discovery Monitor

Monitor and track SOME/IP-SD traffic including service offers, finds, and subscriptions.

```bash
# Live capture
./someip_discovery eth0

# Analyze PCAP file
./someip_discovery captures/sd_traffic.pcap
```

**Features:**
- Tracks all service offers with TTL
- Monitors find requests
- Records eventgroup subscriptions
- Detects reboot flags
- Generates summary tables

### DoIP Routing Activation Validator

Validate DoIP routing activation sequences per ISO 13400-2.

```bash
# Live capture
./doip_routing eth0

# Analyze PCAP file
./doip_routing captures/doip_session.pcap
```

**Features:**
- Validates protocol version field
- Tracks routing activation requests/responses
- Monitors diagnostic message flow
- Counts alive check messages
- Validates activation response codes

### ECU Bootup Monitor

Monitor ECU initialization traffic and generate timing timeline.

```bash
# Live capture during ECU power-on
./ecu_bootup eth0

# Analyze bootup capture
./ecu_bootup captures/ecu_startup.pcap
```

**Features:**
- Captures first service offers per service
- Tracks SD reboot flags
- Records DoIP vehicle announcements
- Generates timing timeline
- Shows service availability order

### PCAP Regression Tests

GoogleTest-based regression tests using PCAP files for CI pipelines.

```bash
# Run all tests
./pcap_regression

# Run specific tests
./pcap_regression --gtest_filter="*SomeIP*"

# Run with verbose output
./pcap_regression --gtest_output=xml:results.xml
```

**Test Categories:**
- SOME/IP message parsing
- SOME/IP-SD entry parsing
- DoIP message validation
- Protocol stack decoding
- Performance benchmarks
- Edge case handling

## YAML/JSON Scenarios

### Running Test Scenarios

```bash
# Dry-run all example scenarios (validates without live capture)
wadjet-run --dry-run --dir examples/scenarios/

# List available scenarios
wadjet-run --list --dir examples/scenarios/

# Run a specific scenario
wadjet-run examples/scenarios/someip_sd_test.yaml -i eth0

# Run with JUnit output
wadjet-run examples/scenarios/ -i eth0 -o junit:results.xml
```

### Available Scenarios

| File | Description |
|------|-------------|
| `basic_udp_test.yaml` | Simple UDP packet capture test |
| `someip_sd_test.yaml` | SOME/IP Service Discovery validation |
| `doip_routing_test.json` | DoIP routing activation test (JSON format) |

## Adding New Examples

When adding new examples:

1. Create the source file in `examples/`
2. Add CMake target in `examples/CMakeLists.txt`
3. Update this README with description
4. Include usage examples and features list
5. For scenarios, place in `scenarios/` with descriptive filename

## Test Data

PCAP test files should be placed in `testdata/` at the repository root:

```
testdata/
├── someip_sample.pcap
├── someip_sd_sample.pcap
├── doip_sample.pcap
├── doip_routing_sample.pcap
├── mixed_traffic.pcap
└── large_capture.pcap    # For performance tests
```

---

*𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.*

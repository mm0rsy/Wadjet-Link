# Wadjet-Link Architecture Documentation

This folder contains architectural documentation for the Wadjet-Link project using PlantUML diagrams.

## Diagram Index

### System-Level

| Diagram | Description |
| ------- | ----------- |
| [component_overview.puml](component_overview.puml) | High-level component architecture |
| [layer_architecture.puml](layer_architecture.puml) | Layered architecture view |

### Use Cases

| Diagram | Description |
| ------- | ----------- |
| [usecase_capture_packets.puml](usecases/usecase_capture_packets.puml) | Live packet capture workflow |
| [usecase_read_pcap.puml](usecases/usecase_read_pcap.puml) | PCAP file reading workflow |
| [usecase_write_pcap.puml](usecases/usecase_write_pcap.puml) | PCAP file writing workflow |
| [usecase_run_scenarios.puml](usecases/usecase_run_scenarios.puml) | Test scenario runner use cases |

### Module Class Diagrams

| Module | Diagram |
| ------ | ------- |
| Core | [core_classes.puml](modules/core_classes.puml) |
| Net | [net_classes.puml](modules/net_classes.puml) |
| PCAP | [pcap_classes.puml](modules/pcap_classes.puml) |
| I/O | [io_classes.puml](modules/io_classes.puml) |
| Protocols | [protocols_classes.puml](modules/protocols_classes.puml) |
| Scenario | [scenario_classes.puml](modules/scenario_classes.puml) |
| Tests | [test_classes.puml](modules/test_classes.puml) |

### Module Sequence Diagrams

| Module | Diagram |
| ------ | ------- |
| Core | [core_sequences.puml](sequences/core_sequences.puml) |
| Net | [net_sequences.puml](sequences/net_sequences.puml) |
| PCAP | [pcap_sequences.puml](sequences/pcap_sequences.puml) |
| I/O | [io_sequences.puml](sequences/io_sequences.puml) |
| Protocols | [protocols_sequences.puml](sequences/protocols_sequences.puml) |
| Scenario | [scenario_sequences.puml](sequences/scenario_sequences.puml) |
| Tests | [test_sequences.puml](sequences/test_sequences.puml) |

## Test Architecture

The project has a comprehensive test suite with **303+ tests**:

### Unit Tests (~120 tests)

Located in `tests/` subdirectories by module:

- `tests/core/` - Result, ByteOrder, Timestamp
- `tests/net/` - Packet, PacketView, MacAddress, IPv4Address
- `tests/pcap/` - PcapReader, PcapWriter
- `tests/io/` - Device, FrameFilter
- `tests/protocols/` - Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP, gPTP, UDS, DDS/RTPS decoders

### Testing Framework Tests (~140 tests)

Located in `tests/testing/`:

- `test_matchers.cpp` - gMock-style packet matchers
- `test_live_capture_fixture.cpp` - LiveCaptureTestFixture integration
- `test_generators.cpp` - Property-based testing generators
- `test_record_replay.cpp` - Record-then-assert mode
- `test_live_assert.cpp` - Live-assert mode

### Scenario Tests (41 tests)

Located in `tests/scenario/`:

- `test_scenario_parser.cpp` - YAML/JSON parser tests
- `test_scenario_runner.cpp` - Runner and report generator tests

### Integration Tests (~26 tests)

Located in `tests/integration/`:

- `test_decode_pipeline.cpp` - Full Ethernet→IPv4→UDP/TCP→SOME/IP/DoIP/DDS decode chain
- `test_pcap_decode.cpp` - PCAP read/write + decode integration
- `test_capture_session.cpp` - Live loopback capture (requires CAP_NET_RAW)

### Skipped Tests

Some tests require elevated privileges or specific hardware:

- Live capture tests - Require `CAP_NET_RAW` or root
- Hardware timestamp tests - Require NIC support

## Rendering Diagrams

### Using PlantUML CLI

\`\`\`bash
# Install PlantUML
sudo apt-get install plantuml

# Render all diagrams to PNG
plantuml -tpng architecture/**/*.puml

# Render to SVG
plantuml -tsvg architecture/**/*.puml
\`\`\`

### Using VS Code

Install the "PlantUML" extension (jebbs.plantuml) for live preview.

### Online

Paste diagram content at https://www.plantuml.com/plantuml/

## Maintenance Policy

**IMPORTANT**: When code changes or extends, the corresponding architecture diagrams MUST be updated:

1. **New classes/interfaces** - Update module class diagram
2. **New workflows/interactions** - Update sequence diagrams
3. **New components/modules** - Update component overview
4. **API changes** - Update all affected diagrams

## Color Scheme

| Color | Meaning |
| ----- | ------- |
| #E8F4FD | Core utilities |
| #E8FDF4 | Network/Packet handling |
| #FDF4E8 | File I/O (PCAP) |
| #F4E8FD | Live capture I/O |
| #FDE8E8 | Protocol decoders |
| #FDF8E8 | Scenario module |
| #E8E8FD | Testing framework |
| #F8FDF8 | CLI tools |

# 𓆓 Wadjet-Link Implementation Plan

## Vision

**Wadjet-Link** — *Restoring the complete picture of the automotive stream.*

Create an open-source Automotive Ethernet validation framework focused on:

- Live packet capture and replay
- Protocol decoding (SOME/IP, DoIP, and more)
- Automated testing and validation with GoogleTest
- Linux-first environment

## License

**Apache 2.0** — Permissive license allowing commercial use, modification, and distribution.

## Non-Goals

- Simulation / HIL
- Waveform analysis
- GUI tools (CLI and library only)

## Language & Extensibility

| Layer | Technology |
|-------|------------|
| Primary | C++20 |
| C bindings | For FFI compatibility |
| Python bindings | pybind11 |
| Future | Rust-compatible FFI |

Testing framework: **GoogleTest** with custom matchers

## Target Protocols

| Protocol | Priority |
|----------|----------|
| Raw Ethernet | Mandatory |
| SOME/IP | Mandatory |
| SOME/IP Service Discovery | Mandatory |
| DoIP | Mandatory |
| gPTP (IEEE 802.1AS) | Mandatory |
| UDS over IP | Mandatory |
| DDS | Optional (later) |
| VLAN / TSN awareness | Optional (later) |

---

## Architecture Overview

### Core Layers

1. **Capture Subsystem**
   - AF_PACKET / PF_PACKET backend (Linux)
   - Optional libpcap backend
   - Zero-copy ring buffer support
   - Hardware timestamping (if available)
   - BPF filtering
   - Capture + Replay sessions

2. **Protocol Decoding Subsystem**
   - Pluggable `ProtocolDecoder` architecture
   - Visitor pattern or layered parsing
   - Zero-copy `PacketView`
   - Decode tree: `Ethernet → VLAN? → IP → UDP/TCP → SOME/IP/DoIP`
   - Ethertype dispatch
   - Configurable heuristics

3. **Validation Subsystem**
   - GoogleTest integration
   - Stream-based expectations
   - Time-bounded assertions
   - Packet matchers (gMock-style)
   - Automatic `.pcap` storage on failure

4. **Scenario Runner Subsystem** (optional)
   - YAML/JSON test definitions
   - CLI executor
   - Report output

---

## Repository Structure

```text
/wadjet-link
  /src
    io/           # CaptureSession, ReplaySession, device enumeration
    pcap/         # PcapReader, PcapWriter
    net/          # MAC, IPv4, UDP, TCP primitives
    protocols/    # SOME/IP, SOME/IP-SD, DoIP, gPTP, UDS decoders
    testing/      # GoogleTest fixtures, matchers, stream assertions
    scenario/     # YAML/JSON test runner
    utils/        # Logger, Result<T>, time utilities
  /include/wadjet/  # Public headers mirroring src/ structure
  /bindings/
    c/              # C ABI layer
    python/         # pybind11 wrappers
  /examples/        # Usage examples and tutorials
  /tests/           # Unit and integration tests
  /docs/            # Doxygen, architecture diagrams
  /benchmarks/      # Performance tests
  /pcap_samples/    # Regression test captures
```

---

## Namespaces

```text
wadjet::io              → Device handling, capture engines
  - DeviceEnumerator
  - CaptureSession (AF_PACKET + TPACKET_V2/V3)
  - PcapCaptureSession (libpcap backend)
  - ReplaySession (timing-controlled replay)
  - FrameFilter (BPF expression)

wadjet::pcap            → Readers/writers
  - PcapReader
  - PcapWriter
  - PcapngWriter (Enhanced Packet Block support)

wadjet::net             → Protocol primitives
  - MAC
  - IPv4
  - UDP
  - TCP

wadjet::protocols::ethernet
wadjet::protocols::ipv4
wadjet::protocols::udp
wadjet::protocols::tcp
wadjet::protocols::someip
  - SOMEIPHeader
  - MessageType
  - ServiceID, EventID
  - ServiceDiscovery

wadjet::protocols::doip
  - DoIPHeader
  - VIN messages
  - Routing activation

wadjet::protocols::gptp   → Precision Time Protocol (IEEE 802.1AS)
wadjet::protocols::uds    → UDS over IP

wadjet::testing         → Test utilities
  - LiveCaptureTestFixture
  - PacketStream
  - Matchers
  - WaitConditions

wadjet::scenario        → Automation
  - ScenarioParser
  - ScenarioRunner

wadjet::utils           → Helpers
  - Logger
  - Result<T>
  - TimeUtils
```

---

## Components

### Core I/O Components

| Component | Description |
|-----------|-------------|
| `CaptureSession` | Manages live packet capture from NIC |
| `ReplaySession` | Replays packets from pcap files |
| `Packet` | Mutable packet buffer |
| `PacketView` | Immutable zero-copy view into packet data |
| `FrameFilter` | BPF expression wrapper |

### Protocol Decoder Design

```cpp
// Abstract base for all decoders
class ProtocolDecoder {
public:
    virtual ~ProtocolDecoder() = default;
    virtual DecodeResult decode(PacketView view) = 0;
    virtual std::string_view name() const = 0;
};
```

---

## Milestones

### Milestone 0 — Bootstrapping ✅

**Goal:** Repository foundation and developer experience

**Status:** Completed

**Deliverables:**

- [x] Repo structure created
- [x] CMake project with modern practices
- [x] CI/CD pipeline (GitHub Actions)
- [x] clang-format configuration
- [x] clang-tidy configuration
- [x] Doxygen setup
- [x] Coding guidelines document
- [x] README with vision/architecture
- [x] CONTRIBUTING.md
- [x] LICENSE file
- [x] Example pcap capture in repo

---

### Milestone 1 — Core Packet I/O ✅

**Goal:** Capture + replay + filter frames deterministically

**Status:** Completed

**Implementation:**

- [x] Linux AF_PACKET / PF_PACKET support
- [x] Optional libpcap backend (PcapCaptureSession)
- [x] Zero-copy ring buffer support (TPACKET_V2 and V3)
- [x] TPACKET_V3 block-based ring buffer for improved performance
- [x] Timestamping support (hardware if available, nanosecond precision)
- [x] Packet writer (.pcap, .pcapng)
- [x] Packet reader abstraction
- [x] `CaptureSession` implementation with multiple backends
- [x] `ReplaySession` implementation with timing control
- [x] `Packet` and `PacketView` classes
- [x] `FrameFilter` (BPF expression)

**Validation Tests:**

- [x] Send & receive on loopback
- [x] Verify timestamps monotonic
- [x] Stress test under load (HighPacketRateCapture, SustainedCapture)
- [x] Dropped packet counters
- [x] TPACKET_V3 basic capture test
- [x] Large packet capture test
- [x] Variable packet size capture test

---

### Milestone 2 — Protocol Decoders ✅

**Goal:** Pluggable decoding architecture

**Status:** Completed

**Implementation:**

- [x] `ProtocolDecoder` abstract base (IProtocolDecoder + DecoderBase CRTP)
- [x] Ethernet header parser (with VLAN support)
- [x] VLAN parsing (802.1Q and QinQ)
- [x] IPv4 parser (with checksum validation)
- [x] UDP parser
- [x] TCP parser (with options parsing)
- [x] SOME/IP header parsing
- [x] SOME/IP Service Discovery parsing
- [x] DoIP header parsing
- [x] Ethertype dispatch mechanism (ProtocolDispatcher)

**Tests:**

- [x] Known PCAP fixtures (39 new tests)
- [x] Malformed frame handling
- [x] Boundary conditions
- [x] Integration tests (decode pipeline, PCAP roundtrip)
- [x] Live capture tests (loopback)
- [x] Fuzz testing with libFuzzer
  - Fuzz harnesses for all protocol decoders (Ethernet, IPv4, UDP, TCP, SOME/IP, SOME/IP-SD, DoIP)
  - Full protocol stack fuzz harness (ProtocolDispatcher)
  - Seed corpus with valid and edge-case packets
  - AddressSanitizer and UndefinedBehaviorSanitizer integration

**Test Summary:**
- Unit tests: 111
- Integration tests: 26
- **Total: 137 tests**

---

### Milestone 3 — Live Testing Engine (GoogleTest Integration) ✅

**Goal:** Run GoogleTest cases on live captured traffic — **the core differentiator**

**Status:** Completed

**Features:**

- [x] gMock-style matchers (matchers.hpp)
  - Ethernet: HasEthertype, HasSourceMac, HasDestMac, HasVlan, HasVlanId
  - IPv4: HasSourceIP, HasDestIP, HasIPProtocol, IsUDP, IsTCP
  - Ports: HasSourcePort, HasDestPort
  - SOME/IP: HasSOMEIPServiceId, HasSOMEIPMethodId, HasSOMEIPMessageType, IsSOMEIPRequest/Response/Notification
  - DoIP: HasDoIPPayloadType, IsDoIPDiagnosticMessage, IsDoIPRoutingActivation*
  - Payload: PayloadContains, HasPayloadSize, DecodesSuccessfully
- [x] Unit tests for matchers (33 tests)
- [x] `LiveCaptureTestFixture` base class
  - Session setup/teardown with auto PCAP save on failure
  - BPF filter configuration
  - Configurable failure directory
- [x] Time-bounded expectations
  - `wait_for_packet(predicate, timeout)`
  - `wait_for_match(matcher, timeout)`
  - `any_packet_matches(predicate, timeout)`
- [x] Pattern matching over packet streams
  - `collect_packets(duration)`
  - `collect_until(predicate, timeout)`
  - `count_packets(predicate, duration)`
- [x] `LoopbackTestFixture` for self-contained tests
  - `send_udp(port, payload)` helper
  - `connect_tcp(port)` helper
- [x] Automatic failure trace pcap storage
  - `save_on_failure` configuration
  - `save_failure_pcap()` method
  - `save_pcap(path)` manual save
- [x] Convenience macros (macros.hpp)
  - WADJET_ASSERT/EXPECT_PACKET_MATCHES
  - WADJET_ASSERT/EXPECT_PACKET
  - Protocol-specific macros for SOME/IP and DoIP
- [x] Integration tests for LiveCaptureTestFixture (24 tests)

**Test Summary:**
- Unit tests: 111
- Testing framework tests: 158 (33 matchers + 24 live capture + 40 generators + 35 record-replay + 26 live-assert)
- Scenario tests: 41 (21 parser + 20 runner/reports)
- Integration tests: 26
- **Total: 378 tests**

**Example API:**

```cpp
TEST_F(SOMEIP_Service_Discovery, ServiceOffersAppearWithin100ms) {
    auto stream = LiveCapture("eth0").filter("udp port 30490");
    auto found = stream.wait_for_someip_service(0x1234, 100ms);
    ASSERT_TRUE(found);
}
```

**Matcher Examples:**

```cpp
EXPECT_THAT(packet, HasSOMEIPServiceId(0x1234));
EXPECT_THAT(packet, HasSOMEIPMethodId(0x0001));
EXPECT_THAT(packet, PayloadContains({0x01, 0x02}));
EXPECT_THAT(packet, HasDoIPRoutingActivation());
```

**GoogleTest Macros:**

```cpp
ASSERT_PACKET_MATCHES(packet, matcher);
EXPECT_SOMEIP_SERVICE(stream, service_id, timeout);
EXPECT_DOIP_ROUTING_ACTIVATION(stream, timeout);
```

**Advanced (now complete):**

- [x] Property-based testing generators (generators.hpp)
  - Random class with seed-based reproducibility
  - Fluent builders: EthernetBuilder, IPv4Builder, UDPBuilder, TCPBuilder, SOMEIPBuilder, DoIPBuilder
  - PacketGenerator factory for random protocol packets
- [x] Record-then-assert mode (record_replay.hpp)
  - RecordedStream for offline packet analysis
  - RecordSession for live capture recording
  - Functional filtering and sequence matching
  - PCAP save/load support
- [x] Live-assert mode (live_assert.hpp)
  - LiveAssertSession for real-time assertions
  - AssertionRule types: ASSERT_ALL, ASSERT_NEVER, ASSERT_WHEN, EXPECT_WITHIN
  - Conditional assertions with when().assert_that() pattern
  - Time-bounded execution with run_for()/run_until()
- [x] Deterministic replay (via RecordedStream)

---

### Milestone 4 — Automation & Test Scenario Language ✅

**Goal:** YAML/JSON-driven test scenarios

**Status:** Completed

**Example Scenario:**

```yaml
name: SOMEIP Service Discovery Test
description: Verify service offers appear within timeout

steps:
  - capture:
      interface: eth0
      filter: "udp port 30490"

  - expect:
      someip:
        service: 0x1234
        event: 0x44
      within_ms: 250
      count: ">= 3"

  - expect:
      doip:
        message_type: routing_activation_response
      within_ms: 500
```

**Implementation:**

- [x] Scenario data model (scenario_types.hpp)
  - Scenario, Step variants (CaptureStep, SendStep, WaitStep, ExpectStep, LogStep)
  - Protocol expectations (EthernetExpect, IPv4Expect, UDPExpect, TCPExpect, SomeIpExpect, SomeIpSdExpect, DoIpExpect)
  - CountExpression with comparison operators (==, !=, <, <=, >, >=)
  - ScenarioResult and ExpectResult for test outcomes
- [x] YAML parser (yaml_parser.cpp)
  - Full scenario parsing with yaml-cpp
  - Duration parsing with unit support (ms, s, m, h)
  - Step type parsing (capture, send, wait, expect, log)
  - Protocol expectation parsing for all supported protocols
- [x] JSON parser (json_parser.cpp)
  - Full scenario parsing with nlohmann_json
  - Same feature parity as YAML parser
  - auto-detection of scenario file format
- [x] `ScenarioRunner` class (runner.hpp/cpp)
  - Packet capture and matching engine
  - ExpectStepMatcher for all protocol expectations
  - Dry-run mode for validation
  - Verbose output with callbacks
  - Stop-on-failure option
  - Tag-based filtering
- [x] CLI runner (`wadjet-run`)
  - Full command-line interface
  - Interface selection and timeout configuration
  - Multiple output formats
  - Batch execution with directory scanning
  - Tag filtering and dry-run mode
- [x] Report output (report.hpp/cpp)
  - JUnit XML (CI integration)
  - JSON (programmatic analysis)
  - Text (human-readable)
  - TAP (Test Anything Protocol)

**Files Created:**
- `include/wadjet/scenario/scenario_types.hpp` - Data model
- `include/wadjet/scenario/parser.hpp` - Parser interface
- `include/wadjet/scenario/runner.hpp` - Runner interface
- `include/wadjet/scenario/report.hpp` - Report generator interface
- `include/wadjet/scenario/scenario.hpp` - Main include header
- `src/scenario/yaml_parser.cpp` - YAML implementation
- `src/scenario/json_parser.cpp` - JSON implementation
- `src/scenario/runner.cpp` - ScenarioRunner implementation
- `src/scenario/report.cpp` - Report generators
- `tools/wadjet-run.cpp` - CLI tool
- `tests/scenario/test_scenario_parser.cpp` - Parser tests
- `tests/scenario/test_scenario_runner.cpp` - Runner tests

**Example YAML scenarios in `examples/scenarios/`:**
- `someip_sd_test.yaml` - SOME/IP Service Discovery test
- `doip_routing_test.json` - DoIP routing activation test
- `basic_udp_test.yaml` - Basic UDP capture test

**CLI Usage:**

```bash
# Run a single scenario
wadjet-run scenario.yaml -i eth0 -o junit:results.xml

# Run all scenarios in a directory
wadjet-run examples/scenarios/ --interface eth0 --format text

# Dry-run with verbose output
wadjet-run test.yaml --dry-run --verbose

# Filter by tags
wadjet-run examples/ --tags smoke,fast
```


---

### Milestone 5 — Python Bindings ✅

**Goal:** Fast adoption, Jupyter analysis, pytest integration

**Status:** Completed

**Implementation:**

C++ Binding Layer (pybind11):
- [x] `module.cpp` — Main PYBIND11_MODULE with version info
- [x] `core_bindings.cpp` — Timestamp class, bytes_to_hex/hex_to_bytes
- [x] `packet_bindings.cpp` — Packet/PacketView with buffer protocol
- [x] `capture_bindings.cpp` — CaptureSession, options, stats, device enumeration
- [x] `pcap_bindings.cpp` — PcapReader/PcapWriter with context managers
- [x] `protocol_bindings.cpp` — All protocol headers and enums
- [x] `decoder_bindings.cpp` — DecodeResult, ProtocolDispatcher, decode_packet()

Python Wrapper Layer:
- [x] `wadjet/__init__.py` — Package exports with graceful fallback
- [x] `capture.py` — LiveCapture, ReplayCapture context managers
- [x] `protocols.py` — decode(), parse(), ProtocolStack, filters
- [x] `pcap.py` — read_pcap, write_pcap, filter_pcap, merge_pcaps
- [x] `testing.py` — assert_someip/doip, PacketMatcher, PacketTestRunner

Documentation & Packaging:
- [x] `README.md` — Comprehensive API documentation with examples
- [x] `pyproject.toml` — Modern Python packaging (pip installable)
- [x] `wadjet/_wadjet.pyi` — Type stubs for native module
- [x] `wadjet/__init__.pyi` — Type stubs for Python wrappers
- [x] `pytest.ini` — pytest configuration

Tests & Examples:
- [x] `tests/test_wadjet.py` — Comprehensive pytest test suite
- [x] `examples/analyze_someip.py` — PCAP analysis example
- [x] `examples/live_capture.py` — Real-time capture example
- [x] `examples/test_protocol.py` — pytest integration example

Build Integration:
- [x] `bindings/python/CMakeLists.txt` — pybind11 module build config
- [x] `WADJET_BUILD_PYTHON_BINDINGS` option in root CMakeLists.txt

**Features:**
- Zero-copy buffer protocol for NumPy integration
- Pythonic context managers for resource management
- Iterator protocol for packet streams
- Composable packet matchers (AND, OR, NOT)
- pytest fixtures for automotive protocol testing
- Full type stub support for IDE autocomplete

**Example:**

```python
import wadjet

with wadjet.LiveCapture("eth0", filter="udp port 30490") as cap:
    for packet in cap.stream(timeout_ms=1000):
        if packet.has_someip():
            header = packet.someip()
            print(f"Service: {header.service_id:#06x}")
```

---

### Milestone 6 — Documentation & Examples ✅

**Goal:** Comprehensive documentation and practical examples

**Status:** Completed

**Deliverables:**

Doxygen Configuration:
- [x] Enhanced Doxyfile with modern settings
- [x] UML diagram generation (SVG)
- [x] Interactive SVG for class diagrams
- [x] Custom aliases (@someip, @doip, @threadsafe)
- [x] Code syntax highlighting

Documentation Files:
- [x] `docs/quickstart.md` — Getting started tutorial
  - Installation from source
  - First packet capture
  - Protocol decoding examples
  - GoogleTest integration guide
  - PCAP file operations
- [x] `docs/architecture.md` — System architecture
  - ASCII system diagrams
  - Namespace structure
  - Data flow diagrams
  - Design decisions
  - Protocol stack table
  - Testing architecture
- [x] `docs/scenarios.md` — Scenario file format
  - YAML/JSON syntax reference
  - Step types documentation
  - Protocol expectations guide
  - CLI usage examples

C++ Example Programs:
- [x] `examples/someip_discovery.cpp` — SOME/IP Service Discovery monitor
  - Live capture and PCAP analysis
  - Service registry tracking
  - Offer/Find/Subscribe detection
  - Summary table generation
- [x] `examples/doip_routing.cpp` — DoIP routing activation validator
  - ISO 13400-2 compliance checking
  - Session state machine
  - Diagnostic message tracking
  - Validation error reporting
- [x] `examples/ecu_bootup.cpp` — ECU bootup sequence monitor
  - Timeline event tracking
  - Service availability timing
  - Reboot detection
  - Multi-protocol correlation
- [x] `examples/pcap_regression.cpp` — PCAP-based regression tests
  - GoogleTest integration
  - Protocol parsing validation
  - Performance benchmarks
  - Edge case testing
  - Synthetic packet generation

Build System:
- [x] `examples/CMakeLists.txt` — Build configuration for examples
- [x] `examples/README.md` — Comprehensive examples documentation

**Files Created:**
- `docs/Doxyfile` — Enhanced Doxygen configuration (156 lines)
- `docs/quickstart.md` — Getting started tutorial (~300 lines)
- `docs/architecture.md` — Architecture documentation (~400 lines)
- `docs/scenarios.md` — Scenario format reference (~350 lines)
- `examples/someip_discovery.cpp` — SD monitor (~300 lines)
- `examples/doip_routing.cpp` — DoIP validator (~450 lines)
- `examples/ecu_bootup.cpp` — Bootup monitor (~400 lines)
- `examples/pcap_regression.cpp` — Regression tests (~500 lines)
- `examples/CMakeLists.txt` — Updated with all examples
- `examples/README.md` — Examples documentation

---

### Milestone 7 — Rust FFI (Optional Expansion)

**Approach:**

- [ ] C ABI layer (`wadjet_c.h`)
- [ ] bindgen-compatible headers
- [ ] Rust crate wrapping C API

**Future possibility:**

- Pure Rust core
- C++ frontend optional

---

## Testing Strategy

| Test Type | Description |
|-----------|-------------|
| Unit tests | Packet parsing, protocol decoders |
| Property-based | Fuzz testing for parsers |
| Integration | Loopback interface tests |
| Regression | Known pcap fixtures in repo |
| Stress | High packet rate, dropped frame detection |
| Benchmarks | Throughput and latency measurement |

---

## MVP Definition

MVP is complete when:

- [x] Raw Ethernet capture works on Linux
- [x] SOME/IP decode works
- [x] GoogleTest can assert on live traffic
- [x] README shows usage example
- [x] At least 20 unit tests exist (378 tests)
- [x] CI pipeline passes

---

## Stretch Goals

- [ ] DDS protocol support
- [ ] TSN awareness (802.1Qbv, etc.)
- [ ] Rust FFI bindings
- [ ] UDS over DoIP parsing
- [x] Packet injection (TX capability via ReplaySession)
- [ ] Web-based report viewer

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

### Milestone 7 — Rust FFI (Optional Expansion) ✅

**Goal:** Provide Rust bindings for Wadjet-Link via C ABI layer

**Status:** Completed

**Implementation:**

C ABI Layer:
- [x] `bindings/c/include/wadjet_c.h` — Complete C header (~600 lines)
  - Opaque handle types for all C++ classes
  - Error handling with thread-local messages
  - All capture, PCAP, packet, and decode APIs
  - Protocol header structures (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP)
  - Utility functions (MAC/IP string conversion, timestamps)
- [x] `bindings/c/src/wadjet_c.cpp` — Full implementation (~800 lines)
  - Thread-local error handling
  - C++ class wrapping with opaque structs
  - Memory-safe API with explicit create/destroy functions
- [x] `bindings/c/CMakeLists.txt` — Build configuration
  - `wadjet_c` shared library
  - `wadjet_c_static` for Rust linking

Rust FFI Bindings (wadjet-sys crate):
- [x] `bindings/rust/wadjet-sys/Cargo.toml` — Sys crate configuration
- [x] `bindings/rust/wadjet-sys/build.rs` — Bindgen build script
- [x] `bindings/rust/wadjet-sys/src/lib.rs` — Generated bindings

Safe Rust Wrapper (wadjet crate):
- [x] `bindings/rust/wadjet/Cargo.toml` — Safe wrapper configuration
- [x] `bindings/rust/wadjet/src/lib.rs` — Main module with init/cleanup/version
- [x] `bindings/rust/wadjet/src/error.rs` — Error types and Result alias
- [x] `bindings/rust/wadjet/src/types.rs` — MacAddress, Ipv4Address, Timestamp, etc.
- [x] `bindings/rust/wadjet/src/capture.rs` — CaptureSession, CaptureOptions
- [x] `bindings/rust/wadjet/src/packet.rs` — Packet, PacketView
- [x] `bindings/rust/wadjet/src/pcap.rs` — PcapReader, PcapWriter
- [x] `bindings/rust/wadjet/src/decode.rs` — DecodeResult, protocol headers
- [x] `bindings/rust/wadjet/src/device.rs` — DeviceInfo, list_devices

Examples:
- [x] `examples/list_devices.rs` — Device enumeration
- [x] `examples/capture.rs` — Live capture with decoding
- [x] `examples/read_pcap.rs` — PCAP file analysis
- [x] `examples/filter_pcap.rs` — Protocol filtering
- [x] `examples/someip_analysis.rs` — SOME/IP traffic analysis

Documentation:
- [x] `bindings/rust/README.md` — Usage documentation

**Features:**
- Safe Rust API wrapping unsafe FFI
- Automatic resource cleanup via Drop trait
- Builder pattern for capture options
- Iterator support for PCAP reading
- Full protocol decoding support
- Thread-safe send markers

**Example:**

```rust
use wadjet::{CaptureSession, CaptureOptions, Protocol};

fn main() -> wadjet::Result<()> {
    wadjet::init()?;
    
    let options = CaptureOptions::new()
        .snaplen(65535)
        .filter("udp port 30490");
    
    let mut session = CaptureSession::open("eth0", &options)?;
    
    while let Some(packet) = session.next_packet()? {
        if let Some(result) = packet.decode() {
            if result.has_protocol(Protocol::SomeIp) {
                println!("SOME/IP packet: {}", result.summary());
            }
        }
    }
    
    wadjet::cleanup();
    Ok(())
}
```

**Future possibility:**

- Pure Rust core
- C++ frontend optional

---

### Milestone 8 — gPTP Protocol Decoder (IEEE 802.1AS)

**Goal:** Implement Generalized Precision Time Protocol decoder for automotive time synchronization

**Status:** Not Started

**Overview:**

gPTP (IEEE 802.1AS) is the timing and synchronization standard for automotive Ethernet, enabling precise clock synchronization across ECUs. It's essential for time-sensitive networking (TSN) and coordinated vehicle functions.

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    gPTP Protocol Stack                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   Sync      │  │  Follow_Up  │  │   Pdelay_Req/Resp       │  │
│  │   Message   │  │   Message   │  │   Messages              │  │
│  └──────┬──────┘  └──────┬──────┘  └───────────┬─────────────┘  │
│         │                │                     │                │
│         └────────────────┼─────────────────────┘                │
│                          ▼                                      │
│              ┌───────────────────────┐                          │
│              │   gPTP Header Parser  │                          │
│              │   - Message Type      │                          │
│              │   - Domain Number     │                          │
│              │   - Correction Field  │                          │
│              │   - Clock Identity    │                          │
│              └───────────┬───────────┘                          │
│                          ▼                                      │
│              ┌───────────────────────┐                          │
│              │   Time Calculator     │                          │
│              │   - Path Delay        │                          │
│              │   - Clock Offset      │                          │
│              │   - Rate Ratio        │                          │
│              └───────────────────────┘                          │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Protocol Structures:
- [ ] `include/wadjet/protocols/gptp/gptp.hpp` — Main header
- [ ] `include/wadjet/protocols/gptp/gptp_types.hpp` — Type definitions
  - gPTPHeader (34 bytes base header)
  - MessageType enum (Sync, Follow_Up, Pdelay_Req, Pdelay_Resp, Pdelay_Resp_Follow_Up, Announce)
  - ClockIdentity (8-byte EUI-64)
  - PortIdentity (ClockIdentity + port number)
  - Timestamp (seconds + nanoseconds)
  - CorrectionField (scaled nanoseconds)
- [ ] `include/wadjet/protocols/gptp/gptp_messages.hpp` — Message structures
  - SyncMessage
  - FollowUpMessage with TLVs
  - PdelayReqMessage
  - PdelayRespMessage
  - PdelayRespFollowUpMessage
  - AnnounceMessage
- [ ] `include/wadjet/protocols/gptp/gptp_tlv.hpp` — TLV parsing
  - OrganizationExtension TLV
  - FollowUpInformation TLV
  - PathTrace TLV

Decoder Implementation:
- [ ] `src/protocols/gptp/gptp_decoder.cpp` — Main decoder
  - Message type dispatch
  - Header validation
  - TLV parsing
- [ ] `src/protocols/gptp/gptp_calculator.cpp` — Time calculations
  - Path delay calculation
  - Clock offset estimation
  - Rate ratio computation
- [ ] `src/protocols/gptp/gptp_state.cpp` — Protocol state tracking
  - Grandmaster election state
  - Sync interval tracking
  - Port state machine

Integration:
- [ ] Update `ProtocolDispatcher` for EtherType 0x88F7 (PTP)
- [ ] Add gPTP to scenario expectations
- [ ] Python bindings for gPTP
- [ ] Rust bindings for gPTP
- [ ] C ABI layer updates

**Testing:**

Unit Tests (`tests/protocols/test_gptp.cpp`):
- [ ] Header parsing (all message types)
- [ ] TLV parsing and validation
- [ ] ClockIdentity/PortIdentity handling
- [ ] Timestamp conversion
- [ ] CorrectionField scaling
- [ ] Malformed message handling
- [ ] Boundary conditions

Integration Tests (`tests/integration/test_gptp_integration.cpp`):
- [ ] Full message decode from raw bytes
- [ ] PCAP roundtrip with gPTP traffic
- [ ] Protocol stack decode (Ethernet → gPTP)
- [ ] Multi-message sequence validation

Fuzz Testing (`fuzz/fuzz_gptp.cpp`):
- [ ] gPTP header fuzzer
- [ ] TLV fuzzer
- [ ] Message-specific fuzzers
- [ ] Seed corpus with valid gPTP captures

Property-Based Tests:
- [ ] gPTPBuilder for packet generation
- [ ] Random message type generation
- [ ] Timestamp boundary testing

Regression Tests:
- [ ] `pcap_samples/gptp/` — Real gPTP captures
- [ ] Known automotive gPTP traffic patterns
- [ ] Edge cases from specification

**Documentation:**

- [ ] `docs/protocols/gptp.md` — Protocol reference
  - IEEE 802.1AS overview
  - Message format diagrams
  - State machine documentation
  - Automotive profile specifics
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with gPTP in protocol stack
- [ ] Update `docs/quickstart.md` with gPTP examples

**Use Cases & Examples:**

- [ ] `examples/gptp_monitor.cpp` — gPTP traffic monitor
  - Grandmaster detection
  - Sync interval analysis
  - Path delay measurement
  - Clock drift visualization
- [ ] `examples/scenarios/gptp_sync_test.yaml` — Scenario test
- [ ] Python example: `examples/python/gptp_analysis.py`

**Matchers & Assertions:**

```cpp
// New matchers for testing
EXPECT_THAT(packet, IsGptpSync());
EXPECT_THAT(packet, IsGptpFollowUp());
EXPECT_THAT(packet, HasGptpDomain(0));
EXPECT_THAT(packet, HasGptpClockIdentity(clock_id));
EXPECT_THAT(packet, GptpMessageType(MessageType::Sync));
```

---

### Milestone 9 — UDS over IP Protocol Decoder

**Goal:** Implement Unified Diagnostic Services over IP for automotive diagnostics

**Status:** Not Started

**Overview:**

UDS (ISO 14229) is the standard diagnostic protocol for automotive ECUs. UDS over IP enables diagnostic communication over Ethernet, typically transported via DoIP or directly over TCP/UDP.

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    UDS Protocol Stack                           │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    UDS Services                          │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │ Diag    │ │ Session │ │ Read/   │ │ Routine         │ │   │
│  │  │ Session │ │ Control │ │ Write   │ │ Control         │ │   │
│  │  │ Control │ │         │ │ Memory  │ │                 │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │ Security│ │ Read/   │ │ Request │ │ ECU Reset       │ │   │
│  │  │ Access  │ │ Write   │ │ Download│ │                 │ │   │
│  │  │         │ │ DID     │ │ /Upload │ │                 │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│              ┌───────────────────────────┐                      │
│              │      UDS Message Parser   │                      │
│              │   - Service ID (SID)      │                      │
│              │   - Sub-function          │                      │
│              │   - Data Parameters       │                      │
│              │   - NRC Handling          │                      │
│              └───────────────────────────┘                      │
│                              │                                  │
│              ┌───────────────┴───────────────┐                  │
│              ▼                               ▼                  │
│  ┌─────────────────────┐        ┌─────────────────────┐         │
│  │   DoIP Transport    │        │  Direct TCP/UDP     │         │
│  │   (ISO 13400)       │        │  Transport          │         │
│  └─────────────────────┘        └─────────────────────┘         │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Protocol Structures:
- [ ] `include/wadjet/protocols/uds/uds.hpp` — Main header
- [ ] `include/wadjet/protocols/uds/uds_types.hpp` — Type definitions
  - ServiceID enum (0x10-0x3E services)
  - NegativeResponseCode enum
  - SessionType enum
  - SecurityLevel
  - DataIdentifier (DID)
  - RoutineIdentifier
- [ ] `include/wadjet/protocols/uds/uds_services.hpp` — Service structures
  - DiagnosticSessionControl (0x10)
  - ECUReset (0x11)
  - SecurityAccess (0x27)
  - CommunicationControl (0x28)
  - TesterPresent (0x3E)
  - ReadDataByIdentifier (0x22)
  - WriteDataByIdentifier (0x2E)
  - RoutineControl (0x31)
  - RequestDownload (0x34)
  - RequestUpload (0x35)
  - TransferData (0x36)
  - RequestTransferExit (0x37)
- [ ] `include/wadjet/protocols/uds/uds_nrc.hpp` — Negative Response Codes
  - All ISO 14229 NRCs with descriptions
  - NRC classification (temporary, permanent)

Decoder Implementation:
- [ ] `src/protocols/uds/uds_decoder.cpp` — Main decoder
  - Service ID dispatch
  - Request/Response differentiation
  - Sub-function parsing
  - Parameter extraction
- [ ] `src/protocols/uds/uds_services.cpp` — Service-specific parsing
  - DID database lookup
  - Routine parameter parsing
  - Transfer block handling
- [ ] `src/protocols/uds/uds_session.cpp` — Session tracking
  - Active session state
  - Security level tracking
  - Timing parameters (P2, P2*)

Integration:
- [ ] Update DoIP decoder to extract UDS payload
- [ ] Add UDS to scenario expectations
- [ ] Python bindings for UDS
- [ ] Rust bindings for UDS
- [ ] C ABI layer updates

**Testing:**

Unit Tests (`tests/protocols/test_uds.cpp`):
- [ ] Service ID parsing (all 20+ services)
- [ ] Sub-function handling
- [ ] DID encoding/decoding
- [ ] NRC parsing and messages
- [ ] Multi-frame handling
- [ ] Malformed request handling
- [ ] Response validation

Integration Tests (`tests/integration/test_uds_integration.cpp`):
- [ ] Full diagnostic session simulation
- [ ] DoIP + UDS combined decode
- [ ] Request-response correlation
- [ ] Session state transitions

Fuzz Testing (`fuzz/fuzz_uds.cpp`):
- [ ] UDS message fuzzer
- [ ] Service-specific fuzzers
- [ ] NRC response fuzzer
- [ ] Seed corpus with real diagnostic traffic

Property-Based Tests:
- [ ] UDSBuilder for message generation
- [ ] Random service/sub-function generation
- [ ] DID range testing

Regression Tests:
- [ ] `pcap_samples/uds/` — Real diagnostic captures
- [ ] Known ECU diagnostic patterns
- [ ] OEM-specific extensions

**Documentation:**

- [ ] `docs/protocols/uds.md` — Protocol reference
  - ISO 14229 overview
  - Service catalog with parameters
  - Session and security concepts
  - NRC reference table
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with UDS in protocol stack
- [ ] DID database format documentation

**Use Cases & Examples:**

- [ ] `examples/uds_monitor.cpp` — UDS traffic monitor
  - Service classification
  - Request/response matching
  - Session tracking
  - Error analysis
- [ ] `examples/uds_validator.cpp` — UDS compliance checker
  - Timing validation (P2/P2*)
  - Session rule enforcement
  - Security access validation
- [ ] `examples/scenarios/uds_flash_test.yaml` — Flash sequence test
- [ ] Python example: `examples/python/uds_analysis.py`

**Matchers & Assertions:**

```cpp
// New matchers for testing
EXPECT_THAT(packet, IsUdsRequest());
EXPECT_THAT(packet, IsUdsResponse());
EXPECT_THAT(packet, HasUdsService(ServiceID::ReadDataByIdentifier));
EXPECT_THAT(packet, HasUdsDid(0xF190)); // VIN DID
EXPECT_THAT(packet, IsUdsNegativeResponse());
EXPECT_THAT(packet, HasUdsNrc(NRC::ServiceNotSupported));
```

---

### Milestone 10 — DDS Protocol Support

**Goal:** Implement Data Distribution Service decoder for advanced automotive middleware

**Status:** Not Started

**Overview:**

DDS (Data Distribution Service) is an OMG standard for real-time publish-subscribe communication. It's increasingly used in autonomous vehicles for sensor fusion, perception, and control systems (e.g., ROS2 uses DDS).

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                      DDS Protocol Stack                         │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    DDS Concepts                          │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │ Domain  │ │ Topic   │ │ Data    │ │ QoS Policies    │ │   │
│  │  │ Part.   │ │         │ │ Reader/ │ │                 │ │   │
│  │  │         │ │         │ │ Writer  │ │                 │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│              ┌───────────────────────────┐                      │
│              │         RTPS Layer        │                      │
│              │   (Real-Time Publish-     │                      │
│              │    Subscribe Protocol)    │                      │
│              └───────────────────────────┘                      │
│                              │                                  │
│         ┌────────────────────┼────────────────────┐             │
│         ▼                    ▼                    ▼             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐      │
│  │   SPDP      │    │   SEDP      │    │   User Data     │      │
│  │ (Discovery) │    │ (Endpoints) │    │   Messages      │      │
│  └─────────────┘    └─────────────┘    └─────────────────┘      │
│                              │                                  │
│                              ▼                                  │
│              ┌───────────────────────────┐                      │
│              │      UDP/IP Transport     │                      │
│              │   Multicast + Unicast     │                      │
│              └───────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Protocol Structures:
- [ ] `include/wadjet/protocols/dds/dds.hpp` — Main header
- [ ] `include/wadjet/protocols/dds/rtps.hpp` — RTPS wire protocol
  - RTPSHeader (RTPS magic, version, vendor, GUID prefix)
  - SubmessageHeader
  - Submessage types (DATA, HEARTBEAT, ACKNACK, GAP, INFO_TS, etc.)
- [ ] `include/wadjet/protocols/dds/rtps_types.hpp` — RTPS types
  - GUID_t (16 bytes)
  - SequenceNumber_t
  - Locator_t
  - BuiltinEndpointSet
  - ProtocolVersion
  - VendorId
- [ ] `include/wadjet/protocols/dds/discovery.hpp` — Discovery protocols
  - SPDP (Simple Participant Discovery Protocol)
  - SEDP (Simple Endpoint Discovery Protocol)
  - ParticipantBuiltinTopicData
  - PublicationBuiltinTopicData
  - SubscriptionBuiltinTopicData
- [ ] `include/wadjet/protocols/dds/qos.hpp` — QoS policies
  - Reliability, Durability, History
  - Deadline, Liveliness, LatencyBudget

Decoder Implementation:
- [ ] `src/protocols/dds/rtps_decoder.cpp` — RTPS decoder
  - Header validation
  - Submessage iteration
  - Endianness handling
- [ ] `src/protocols/dds/submessage_decoder.cpp` — Submessage parsing
  - DATA submessage with serialized payload
  - HEARTBEAT/ACKNACK for reliability
  - INFO_DST, INFO_SRC, INFO_TS
- [ ] `src/protocols/dds/discovery_decoder.cpp` — Discovery parsing
  - Participant announcement parsing
  - Endpoint discovery parsing
  - QoS extraction
- [ ] `src/protocols/dds/cdr_decoder.cpp` — CDR deserialization
  - Common Data Representation parsing
  - Type support basics

Integration:
- [ ] Update `ProtocolDispatcher` for DDS ports (7400-7500 range)
- [ ] Add DDS to scenario expectations
- [ ] Python bindings for DDS
- [ ] Rust bindings for DDS
- [ ] C ABI layer updates

**Testing:**

Unit Tests (`tests/protocols/test_dds.cpp`):
- [ ] RTPS header parsing
- [ ] All submessage types
- [ ] GUID handling
- [ ] Sequence number handling
- [ ] Discovery message parsing
- [ ] QoS policy extraction
- [ ] CDR basic types

Integration Tests (`tests/integration/test_dds_integration.cpp`):
- [ ] Full RTPS message decode
- [ ] Discovery sequence validation
- [ ] Data exchange patterns
- [ ] Multi-vendor interop samples

Fuzz Testing (`fuzz/fuzz_dds.cpp`):
- [ ] RTPS header fuzzer
- [ ] Submessage fuzzer
- [ ] Discovery fuzzer
- [ ] CDR fuzzer

Regression Tests:
- [ ] `pcap_samples/dds/` — Real DDS captures
- [ ] FastDDS traffic samples
- [ ] CycloneDDS traffic samples
- [ ] ROS2 traffic samples

**Documentation:**

- [ ] `docs/protocols/dds.md` — Protocol reference
  - RTPS specification overview
  - Discovery protocol documentation
  - Submessage reference
  - Vendor ID table
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with DDS

**Use Cases & Examples:**

- [ ] `examples/dds_monitor.cpp` — DDS traffic monitor
  - Participant discovery tracking
  - Topic/endpoint enumeration
  - Data rate statistics
  - QoS analysis
- [ ] `examples/ros2_analyzer.cpp` — ROS2 traffic analyzer
  - Node discovery
  - Topic mapping
  - Message frequency analysis
- [ ] `examples/scenarios/dds_discovery_test.yaml` — Discovery test
- [ ] Python example: `examples/python/dds_analysis.py`

**Matchers & Assertions:**

```cpp
// New matchers for testing
EXPECT_THAT(packet, IsRtpsMessage());
EXPECT_THAT(packet, HasRtpsSubmessage(SubmessageKind::DATA));
EXPECT_THAT(packet, HasRtpsGuid(guid));
EXPECT_THAT(packet, IsSpdpParticipant());
EXPECT_THAT(packet, HasDdsTopic("rt/sensor_data"));
```

---

### Milestone 11 — TSN Awareness (IEEE 802.1Qbv)

**Goal:** Implement Time-Sensitive Networking awareness for deterministic Ethernet

**Status:** Not Started

**Overview:**

TSN (Time-Sensitive Networking) is a set of IEEE 802.1 standards enabling deterministic, low-latency communication over Ethernet. 802.1Qbv (Time-Aware Shaper) is critical for automotive real-time applications.

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    TSN Analysis Stack                           │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │               TSN Standards Coverage                     │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │ 802.1AS │ │ 802.1Qbv│ │ 802.1Qbu│ │ 802.1CB         │ │   │
│  │  │ (gPTP)  │ │ (TAS)   │ │ (Preempt│ │ (Redundancy)    │ │   │
│  │  │         │ │         │ │ ion)    │ │                 │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  TSN Analyzer                            │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ Schedule        │  │ Latency Measurement         │    │   │
│  │  │ Validation      │  │ - End-to-end delay          │    │   │
│  │  │ - Gate timing   │  │ - Jitter analysis           │    │   │
│  │  │ - Priority map  │  │ - Deadline violations       │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ Traffic Class   │  │ Preemption Analysis         │    │   │
│  │  │ Analysis        │  │ - Express/Preemptable       │    │   │
│  │  │ - PCP mapping   │  │ - Fragment handling         │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│              ┌───────────────────────────┐                      │
│              │    VLAN Priority Parser   │                      │
│              │    (802.1Q PCP field)     │                      │
│              └───────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Protocol Structures:
- [ ] `include/wadjet/protocols/tsn/tsn.hpp` — Main header
- [ ] `include/wadjet/protocols/tsn/vlan_priority.hpp` — Priority handling
  - PriorityCodePoint (PCP) extraction
  - Traffic class mapping
  - Drop Eligible Indicator (DEI)
- [ ] `include/wadjet/protocols/tsn/schedule.hpp` — TAS schedule
  - GateControlEntry
  - GateControlList
  - CycleTime representation
  - TimeAwareOffset
- [ ] `include/wadjet/protocols/tsn/stream.hpp` — Stream identification
  - StreamID (MAC + VLAN ID)
  - StreamHandle
  - Talker/Listener specification
- [ ] `include/wadjet/protocols/tsn/frer.hpp` — Frame Replication (802.1CB)
  - R-TAG parsing
  - Sequence number tracking
  - Redundancy elimination

Analysis Components:
- [ ] `src/protocols/tsn/tsn_analyzer.cpp` — TSN analysis
  - Per-priority statistics
  - Latency measurement
  - Jitter calculation
  - Schedule compliance checking
- [ ] `src/protocols/tsn/latency_tracker.cpp` — Latency tracking
  - End-to-end delay measurement
  - Timestamped packet correlation
  - Histogram generation
- [ ] `src/protocols/tsn/preemption_analyzer.cpp` — 802.1Qbu analysis
  - Express vs preemptable classification
  - mPacket reassembly
  - Preemption event detection
- [ ] `src/protocols/tsn/redundancy_tracker.cpp` — 802.1CB analysis
  - R-TAG sequence tracking
  - Duplicate detection
  - Replication path analysis

Integration:
- [ ] Enhance VLAN decoder with TSN awareness
- [ ] Add TSN analysis to scenario expectations
- [ ] Python bindings for TSN analysis
- [ ] Rust bindings for TSN
- [ ] C ABI layer updates

**Testing:**

Unit Tests (`tests/protocols/test_tsn.cpp`):
- [ ] PCP extraction and mapping
- [ ] Schedule parsing
- [ ] Stream identification
- [ ] R-TAG parsing
- [ ] Latency calculation
- [ ] Preemption detection

Integration Tests (`tests/integration/test_tsn_integration.cpp`):
- [ ] Full TSN traffic analysis
- [ ] Multi-priority traffic mix
- [ ] gPTP + TSN correlation
- [ ] Redundancy path validation

Regression Tests:
- [ ] `pcap_samples/tsn/` — Real TSN captures
- [ ] Multi-priority traffic patterns
- [ ] Preemption scenarios
- [ ] FRER redundancy captures

**Documentation:**

- [ ] `docs/protocols/tsn.md` — TSN reference
  - IEEE 802.1 TSN standards overview
  - Priority mapping tables
  - Schedule format documentation
  - Latency analysis methodology
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with TSN

**Use Cases & Examples:**

- [ ] `examples/tsn_analyzer.cpp` — TSN traffic analyzer
  - Priority distribution charts
  - Latency histograms
  - Jitter statistics
  - Schedule compliance report
- [ ] `examples/tsn_validator.cpp` — TSN compliance checker
  - Timing constraint validation
  - Priority mapping verification
  - Bandwidth utilization analysis
- [ ] `examples/scenarios/tsn_latency_test.yaml` — Latency test
- [ ] Python example: `examples/python/tsn_analysis.py`

**Matchers & Assertions:**

```cpp
// New matchers for testing
EXPECT_THAT(packet, HasVlanPriority(7));
EXPECT_THAT(packet, IsExpressTraffic());
EXPECT_THAT(packet, IsPreemptableTraffic());
EXPECT_THAT(packet, HasStreamId(stream_id));
EXPECT_THAT(packet, HasLatencyBelow(100us));
```

---

### Milestone 12 — UDS over DoIP Integration

**Goal:** Implement complete UDS-over-DoIP diagnostic stack with session management

**Status:** Not Started

**Overview:**

UDS over DoIP combines ISO 14229 (UDS) with ISO 13400 (DoIP) for complete Ethernet-based diagnostics. This milestone integrates the UDS decoder (Milestone 9) with the existing DoIP decoder for full diagnostic session handling.

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                 UDS over DoIP Stack                             │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │               Diagnostic Session Manager                  │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │  Session State  │  Security State │  Timing State   │ │   │
│  │  │  - Default      │  - Locked       │  - P2 timer     │ │   │
│  │  │  - Programming  │  - Level 1-N    │  - P2* timer    │ │   │
│  │  │  - Extended     │  - Seeds/Keys   │  - S3 timer     │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Request/Response Correlator                 │   │
│  │  - Source/Target address matching                        │   │
│  │  - Service ID correlation                                │   │
│  │  - Multi-frame assembly                                  │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│              ┌───────────────┴───────────────┐                  │
│              ▼                               ▼                  │
│  ┌─────────────────────┐        ┌─────────────────────┐         │
│  │   UDS Decoder       │        │   DoIP Transport    │         │
│  │   (Milestone 9)     │        │   (Existing)        │         │
│  │   - Service parsing │        │   - Routing         │         │
│  │   - NRC handling    │        │   - Vehicle ID      │         │
│  └─────────────────────┘        └─────────────────────┘         │
│                              │                                  │
│                              ▼                                  │
│              ┌───────────────────────────┐                      │
│              │       TCP/IP Stack        │                      │
│              │     Port 13400 (DoIP)     │                      │
│              └───────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Integration Components:
- [ ] `include/wadjet/protocols/diagnostic/diagnostic_session.hpp` — Session management
  - DiagnosticSession class
  - SessionState enum
  - SecurityState tracking
  - TimingParameters (P2, P2*, S3)
- [ ] `include/wadjet/protocols/diagnostic/request_correlator.hpp` — Correlation
  - RequestResponsePair
  - PendingRequest tracking
  - Multi-frame assembly
- [ ] `include/wadjet/protocols/diagnostic/ecu_identifier.hpp` — ECU identification
  - LogicalAddress
  - ECUInfo (VIN, hardware/software versions)
  - AddressTable

Implementation:
- [ ] `src/protocols/diagnostic/diagnostic_session.cpp` — Session manager
  - State machine implementation
  - Timer management
  - Event callbacks
- [ ] `src/protocols/diagnostic/uds_doip_decoder.cpp` — Combined decoder
  - DoIP → UDS extraction
  - Address translation
  - Session context injection
- [ ] `src/protocols/diagnostic/flash_sequence.cpp` — Flash support
  - Download sequence tracking
  - Block counter validation
  - Checksum verification
- [ ] `src/protocols/diagnostic/dtc_manager.cpp` — DTC handling
  - DTC database
  - Status byte parsing
  - Snapshot data extraction

Validation Engine:
- [ ] `include/wadjet/testing/diagnostic_assertions.hpp` — Test assertions
  - Session timing validation
  - Security sequence validation
  - Service compliance checks
- [ ] `src/testing/diagnostic_validator.cpp` — Validation implementation

**Testing:**

Unit Tests (`tests/protocols/test_uds_doip.cpp`):
- [ ] DoIP + UDS combined decode
- [ ] Session state machine
- [ ] Request/response correlation
- [ ] Multi-ECU addressing
- [ ] Timing validation
- [ ] Flash sequence validation

Integration Tests (`tests/integration/test_diagnostic_integration.cpp`):
- [ ] Complete diagnostic session capture
- [ ] Flash download sequence
- [ ] DTC read/clear cycle
- [ ] Security access sequence

System Tests:
- [ ] Real ECU diagnostic captures
- [ ] Multi-ECU network scenarios
- [ ] Error recovery scenarios

**Documentation:**

- [ ] `docs/protocols/uds_doip.md` — Integration reference
  - ISO 13400 + ISO 14229 interaction
  - Session management guide
  - Timing requirements
  - Common diagnostic sequences
- [ ] `docs/diagnostic_testing.md` — Test guide
  - Diagnostic test patterns
  - Compliance validation
  - Best practices
- [ ] API documentation (Doxygen)

**Use Cases & Examples:**

- [ ] `examples/diagnostic_analyzer.cpp` — Full diagnostic analyzer
  - Session visualization
  - Service statistics
  - Error analysis
  - Timing charts
- [ ] `examples/flash_validator.cpp` — Flash sequence validator
  - Download sequence checking
  - Block integrity validation
  - Timing compliance
- [ ] `examples/dtc_analyzer.cpp` — DTC analysis tool
  - DTC enumeration
  - Status interpretation
  - Snapshot data display
- [ ] `examples/scenarios/diagnostic_session_test.yaml` — Session test
- [ ] Python example: `examples/python/diagnostic_analysis.py`

**Matchers & Assertions:**

```cpp
// Combined diagnostic matchers
EXPECT_THAT(session, HasValidSessionTiming());
EXPECT_THAT(sequence, IsValidSecurityAccess());
EXPECT_THAT(flash, HasValidBlockSequence());
EXPECT_THAT(response, ArrivesWithin(P2_TIMEOUT));

// Diagnostic-specific assertions
ASSERT_DIAGNOSTIC_SESSION(stream, SessionType::Programming, timeout);
ASSERT_SECURITY_ACCESS(stream, SecurityLevel::Level1, timeout);
ASSERT_FLASH_COMPLETE(stream, expected_size, timeout);
```

---

### Milestone 13 — Web-Based Report Viewer

**Goal:** Implement interactive web-based visualization for test reports and packet analysis

**Status:** Not Started

**Overview:**

A modern web interface for visualizing test results, packet captures, and protocol analysis. Enables sharing results across teams without requiring local tool installation.

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                 Web Report Viewer Architecture                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Frontend (SPA)                       │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │    │
│  │  │  Dashboard  │ │  Packet     │ │  Protocol       │    │    │
│  │  │  View       │ │  Inspector  │ │  Analyzer       │    │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────┘    │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │    │
│  │  │  Test       │ │  Timeline   │ │  Statistics     │    │    │
│  │  │  Results    │ │  View       │ │  Charts         │    │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────┘    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  Backend API Server                     │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │    │
│  │  │  REST API   │ │  WebSocket  │ │  File Server    │    │    │
│  │  │  Endpoints  │ │  (Live)     │ │  (PCAP/Reports) │    │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────┘    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   Data Layer                            │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │    │
│  │  │  Report     │ │  PCAP       │ │  Scenario       │    │    │
│  │  │  Parser     │ │  Indexer    │ │  Results        │    │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────┘    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              Wadjet-Link Core Library                   │    │
│  │         (Protocol Decoders, Analysis Engine)            │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Backend Server:
- [ ] `tools/wadjet-server/` — Backend server
- [ ] `tools/wadjet-server/main.cpp` — Server entry point
- [ ] `tools/wadjet-server/api/` — REST API handlers
  - `/api/reports` — Report listing and retrieval
  - `/api/pcaps` — PCAP file management
  - `/api/packets/{id}` — Packet details
  - `/api/decode` — On-demand packet decoding
  - `/api/scenarios` — Scenario results
  - `/api/statistics` — Traffic statistics
- [ ] `tools/wadjet-server/websocket/` — Real-time updates
  - Live capture streaming
  - Test progress updates
- [ ] HTTP server using cpp-httplib or Crow framework
- [ ] JSON serialization for all Wadjet types

Frontend Application:
- [ ] `web/` — Frontend SPA
- [ ] `web/src/` — Source code (TypeScript/React or Vue)
- [ ] Views:
  - Dashboard — Overview of recent tests
  - Test Results — Detailed test report viewer
  - Packet List — Scrollable packet table with filtering
  - Packet Detail — Hex dump + decoded fields
  - Protocol Analysis — Protocol-specific dashboards
  - Timeline — Time-based packet visualization
  - Statistics — Charts and graphs
- [ ] Components:
  - PacketTable — Virtual scrolling for large captures
  - HexViewer — Interactive hex dump
  - ProtocolTree — Expandable decode tree
  - FilterBar — BPF-like filter input
  - TimelineChart — D3.js timeline
  - StatisticsChart — Chart.js/Recharts graphs

Report Generation:
- [ ] `src/report/html_report.cpp` — Static HTML export
  - Self-contained HTML with embedded data
  - Offline viewable reports
  - Print-friendly layout
- [ ] `src/report/json_export.cpp` — JSON data export
  - Full packet data export
  - Decode results export
  - Statistics export

**Testing:**

Backend Tests:
- [ ] API endpoint tests
- [ ] WebSocket connection tests
- [ ] Report parsing tests
- [ ] PCAP indexing tests

Frontend Tests:
- [ ] Component unit tests (Jest)
- [ ] Integration tests (Cypress)
- [ ] Performance tests (large captures)

End-to-End Tests:
- [ ] Full workflow tests
- [ ] Cross-browser testing
- [ ] Mobile responsiveness

**Documentation:**

- [ ] `docs/web_viewer.md` — User guide
  - Installation and setup
  - Feature overview
  - Navigation guide
- [ ] `docs/api_reference.md` — REST API documentation
  - Endpoint reference
  - Request/response formats
  - Authentication (if applicable)
- [ ] README in `tools/wadjet-server/`
- [ ] README in `web/`

**Use Cases & Examples:**

- [ ] `examples/start_server.sh` — Server launch script
- [ ] Docker compose for easy deployment
- [ ] CI integration examples
  - GitHub Actions artifact upload
  - Report publishing workflow

**Features:**

Dashboard:
- Recent test runs with pass/fail status
- Quick statistics summary
- Alerts for failed tests

Packet Inspector:
- Scrollable packet list with virtual scrolling
- Column customization
- Filter by protocol, address, port
- Export selection to PCAP

Protocol Analyzer:
- SOME/IP service browser
- DoIP session timeline
- gPTP synchronization graph
- DDS topic explorer

Test Results:
- JUnit-style test tree
- Failure details with packet context
- PCAP link for failed assertions
- Comparison with previous runs

Statistics:
- Protocol distribution pie chart
- Traffic rate over time
- Latency histograms
- Top talkers table

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

## Post-MVP Roadmap

| Milestone | Description | Priority |
|-----------|-------------|----------|
| 8 | gPTP (IEEE 802.1AS) decoder | High |
| 9 | UDS over IP decoder | High |
| 10 | DDS protocol support | Medium |
| 11 | TSN awareness (802.1Qbv) | Medium |
| 12 | UDS over DoIP integration | High |
| 13 | Web-based report viewer | Low |

---

## Stretch Goals (Completed)

- [x] Rust FFI bindings (Milestone 7)
- [x] Packet injection (TX capability via ReplaySession)

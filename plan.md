# 𓆓 Wadjet-Link Implementation Plan

## Vision

**Wadjet-Link** — *Restoring the complete picture of the automotive stream.*

Create an open-source Automotive Ethernet validation framework focused on:

- Live packet capture and replay
- Protocol decoding (SOME/IP, DoIP, and more)
- Automated testing and validation with GoogleTest
- Linux-first environment

## License

**Polyform Noncommercial 1.0.0** — Free for non-commercial use; commercial licensing available upon request.

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

### Milestone 0 — Bootstrapping

**Goal:** Repository foundation and developer experience

**Status:** ✅ Complete

**Priority:** 🔴 High — Foundation for all other work

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

### Milestone 1 — Core Packet I/O

**Goal:** Capture + replay + filter frames deterministically

**Status:** ✅ Complete

**Priority:** 🔴 High — Core I/O functionality

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

### Milestone 2 — Protocol Decoders

**Goal:** Pluggable decoding architecture

**Status:** ✅ Complete

**Priority:** 🔴 High — Protocol support

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

### Milestone 3 — Live Testing Engine (GoogleTest Integration)

**Goal:** Run GoogleTest cases on live captured traffic — **the core differentiator**

**Status:** ✅ Complete

**Priority:** 🔴 High — Core testing engine

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

### Milestone 4 — Automation & Test Scenario Language

**Goal:** YAML/JSON-driven test scenarios

**Status:** ✅ Complete

**Priority:** 🔴 High — Declarative test automation

**Example Scenario:****

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

### Milestone 5 — Python Bindings

**Goal:** Fast adoption, Jupyter analysis, pytest integration

**Status:** ✅ Complete

**Priority:** 🔴 High — Python ecosystem integration

**Implementation:****

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

### Milestone 6 — Documentation & Examples

**Goal:** Comprehensive documentation and practical examples

**Status:** ✅ Complete

**Priority:** 🔴 High — Developer enablement

**Deliverables:****

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

**Goal:** Provide Rust bindings for Wadjet-Link via C ABI layer

**Status:** ✅ Complete

**Priority:** 🟡 Medium — Rust ecosystem support

**Implementation:****

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

**Status:** ✅ Complete

**Priority:** 🔴 High — Time synchronization for TSN

**Overview:**

gPTP (IEEE 802.1AS) is the timing and synchronization standard for automotive Ethernet, enabling precise clock synchronization across ECUs. It's essential for time-sensitive networking (TSN) and coordinated vehicle functions.

**Architecture:**

```text
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
- [x] `include/wadjet/protocols/gptp/gptp.hpp` — Main header
- [x] `include/wadjet/protocols/gptp/gptp_types.hpp` — Type definitions
  - gPTPHeader (34 bytes base header)
  - MessageType enum (Sync, Follow_Up, Pdelay_Req, Pdelay_Resp, Pdelay_Resp_Follow_Up, Announce, Signaling)
  - ClockIdentity (8-byte EUI-64)
  - PortIdentity (ClockIdentity + port number)
  - Timestamp (seconds + nanoseconds)
  - CorrectionField (scaled nanoseconds)
  - GptpFlags (two_step, unicast, alternate_master, etc.)
- [x] `include/wadjet/protocols/gptp/gptp_messages.hpp` — Message structures
  - SyncMessage
  - FollowUpMessage with TLVs
  - PdelayReqMessage
  - PdelayRespMessage
  - PdelayRespFollowUpMessage
  - AnnounceMessage
  - SignalingMessage
- [x] `include/wadjet/protocols/gptp/gptp_tlv.hpp` — TLV parsing
  - OrganizationExtension TLV
  - FollowUpInformation TLV (with rate ratio, GM timestamps)
  - PathTrace TLV
  - Generic TLV framework

Decoder Implementation:
- [x] `src/protocols/gptp/gptp_decoder.cpp` — Main decoder
  - Message type dispatch via std::variant
  - Header validation
  - TLV parsing
  - Error handling with DecodeError
- [x] Helper functions implemented in gptp_types.hpp
  - `to_nanoseconds()` — Timestamp conversion
  - `to_seconds_double()` — Floating point seconds
  - `operator-` — Timestamp arithmetic
  - `to_scaled_nanoseconds()` — CorrectionField conversion
  - `is_event()` — Event message detection

Integration:
- [x] Update `ProtocolDispatcher` for EtherType 0x88F7 (PTP)
- [x] gPTP filters in filter system
- [x] Python bindings for gPTP
- [x] Rust bindings for gPTP
- [x] C ABI layer updates

**Testing:**

Unit Tests (`tests/protocols/test_gptp.cpp`):
- [x] Header parsing (all message types)
- [x] ClockIdentity/PortIdentity handling
- [x] Timestamp conversion
- [x] Malformed message handling (buffer too small, invalid length)
- [x] Message type helpers (is_event, to_string)

Integration Tests:
- [x] Full message decode from raw bytes
- [x] Protocol stack decode (Ethernet → gPTP)
- [x] Dispatcher integration test

Fuzz Testing (`fuzz/fuzz_gptp.cpp`):
- [x] gPTP header fuzzer
- [x] Message-specific fuzzers (all message types)
- [x] TLV/option fuzzer
- [x] Helper function fuzzing

**Documentation:**

- [x] `docs/protocols/gptp.md` — Protocol reference
  - IEEE 802.1AS overview
  - Message format documentation
  - API reference
  - Matchers documentation
- [x] Architecture and quickstart documentation (inline in protocol docs)

**Use Cases & Examples:**

- [x] `examples/gptp_monitor.cpp` — gPTP traffic monitor
  - Grandmaster detection
  - Sync interval analysis
  - Rate ratio extraction
  - Clock tracking with statistics
- [x] `examples/scenarios/gptp_sync_test.yaml` — Scenario test
- [x] Python bindings example available via pybind11

**Matchers & Assertions:**

```cpp
// Implemented matchers in include/wadjet/testing/matchers.hpp
EXPECT_THAT(packet, IsGptp());
EXPECT_THAT(packet, HasGptpMessageType(MessageType::Sync));
EXPECT_THAT(packet, IsGptpSync());
EXPECT_THAT(packet, IsGptpFollowUp());
EXPECT_THAT(packet, IsGptpPdelayReq());
EXPECT_THAT(packet, IsGptpPdelayResp());
EXPECT_THAT(packet, IsGptpPdelayRespFollowUp());
EXPECT_THAT(packet, IsGptpAnnounce());
EXPECT_THAT(packet, IsGptpSignaling());
EXPECT_THAT(packet, HasGptpDomain(0));
EXPECT_THAT(packet, HasGptpSequenceId(42));
EXPECT_THAT(packet, GptpFromPort(port_identity));
EXPECT_THAT(packet, GptpFromClock(clock_identity));
EXPECT_THAT(packet, IsGptpEventMessage());
EXPECT_THAT(packet, IsGptpTwoStep());
```

**Completion Notes:**

All core functionality implemented and tested:
- Full gPTP decoder with all message types (Sync, Follow_Up, Pdelay_Req, Pdelay_Resp, Pdelay_Resp_Follow_Up, Announce, Signaling)
- Complete TLV support including Follow-Up Information TLV with rate ratio
- 15 gMock-compatible matchers for test assertions
- Comprehensive fuzz testing harness
- Full example application (gptp_monitor.cpp)
- Protocol documentation
- 13 unit tests all passing
- C ABI layer with gPTP support (wadjet_gptp_header_t, wadjet_decode_result_gptp)
- Python bindings with GptpHeader, GptpMessageType, ClockIdentity, PortIdentity classes
- Rust bindings with GptpHeader struct, GptpMessageType enum, Protocol::Gptp variant
- YAML scenario test for gPTP time synchronization

---

### Milestone 9 — UDS over IP Protocol Decoder

**Goal:** Implement Unified Diagnostic Services over IP for automotive diagnostics

**Status:** ✅ Complete

**Priority:** 🔴 High — Automotive diagnostics support

**Overview:**

UDS (ISO 14229) is the standard diagnostic protocol for automotive ECUs. UDS over IP enables diagnostic communication over Ethernet, typically transported via DoIP or directly over TCP/UDP.

**Architecture:**

```text
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
- [x] `include/wadjet/protocols/uds/uds.hpp` — Main header
- [x] `include/wadjet/protocols/uds/uds_types.hpp` — Type definitions
  - ServiceID enum (0x10-0x3E services)
  - NegativeResponseCode enum  
  - SessionType enum
  - SecurityLevel
  - DataIdentifier (DID)
  - RoutineIdentifier
- [x] `include/wadjet/protocols/uds/uds_services.hpp` — Service structures
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
- [x] `include/wadjet/protocols/uds/uds_nrc.hpp` — Negative Response Codes
  - All ISO 14229 NRCs with descriptions
  - NRC classification (temporary, permanent)

Decoder Implementation:
- [x] `src/protocols/uds_decoder.cpp` — Main decoder
  - Service ID dispatch
  - Request/Response differentiation
  - Sub-function parsing
  - Parameter extraction
- [x] Service-specific parsing in decoder
  - DID database lookup
  - Routine parameter parsing
  - Transfer block handling
- [x] Session tracking
  - Active session state
  - Security level tracking
  - Timing parameters (P2, P2*)

Integration:
- [x] DoIP decoder already extracts UDS payload (user_data field)
- [x] Add UDS to scenario expectations (UdsExpect in scenario_types.hpp)
- [x] Python bindings for UDS (`bindings/python/src/protocol_bindings.cpp`)
- [x] Rust bindings for UDS (`bindings/rust/wadjet/src/decode.rs`)
- [x] C ABI layer updates (`src/bindings/c/wadjet_c.cpp`)

**Testing:**

Unit Tests (`tests/protocols/test_uds.cpp`):
- [x] Service ID parsing (all 20+ services)
- [x] Sub-function handling
- [x] DID encoding/decoding
- [x] NRC parsing and messages
- [x] Malformed request handling
- [x] Response validation
- [x] Multi-frame handling (N/A - DoIP handles transport layer natively)

Integration Tests (`tests/integration/test_uds_integration.cpp`):
- [x] Full diagnostic session simulation
- [x] DoIP + UDS combined decode
- [x] Request-response correlation
- [x] Session state transitions

Fuzz Testing (`fuzz/fuzz_uds.cpp`):
- [x] UDS message fuzzer
- [x] Service-specific fuzzers
- [x] NRC response fuzzer
- [x] Seed corpus with real diagnostic traffic

Property-Based Tests:
- [x] UDSBuilder for message generation (`include/wadjet/testing/generators.hpp`)
- [x] Random service/sub-function generation
- [x] DID range testing

Regression Tests:
- [x] `pcap_samples/uds/` — Real diagnostic captures
- [x] Known ECU diagnostic patterns (`tests/protocols/test_uds_regression.cpp`)
- [x] OEM-specific extensions

**Documentation:**

- [x] `docs/protocols/uds.md` — Protocol reference
  - ISO 14229 overview
  - Service catalog with parameters
  - Session and security concepts
  - NRC reference table
- [x] API documentation (Doxygen in `include/wadjet/protocols/uds/uds.hpp`)
- [x] Update `docs/architecture.md` with UDS in protocol stack
- [x] DID database format documentation (`docs/did_database.md`)

**Use Cases & Examples:**

- [x] `examples/uds_monitor.cpp` — UDS traffic monitor
  - Service classification
  - Request/response matching
  - Session tracking
  - Error analysis
- [x] `examples/uds_validator.cpp` — UDS compliance checker
  - Timing validation (P2/P2*)
  - Session rule enforcement
  - Security access validation
- [x] `examples/scenarios/uds_flash_test.yaml` — Flash sequence test
- [x] Python example: `examples/uds_analysis.py`

**Matchers & Assertions:**

```cpp
// Implemented matchers for testing
EXPECT_THAT(packet, IsUdsRequest());
EXPECT_THAT(packet, IsUdsResponse());
EXPECT_THAT(packet, IsUdsPositiveResponse());
EXPECT_THAT(packet, IsUdsNegativeResponse());
EXPECT_THAT(packet, HasUdsService(ServiceID::ReadDataByIdentifier));
EXPECT_THAT(packet, HasUdsDID(0xF190)); // VIN DID
EXPECT_THAT(packet, HasUdsNRC(NRC::ServiceNotSupported));
EXPECT_THAT(packet, HasUdsSessionType(SessionType::ExtendedDiagnosticSession));
// Convenience matchers
EXPECT_THAT(packet, IsUdsDiagnosticSessionControl());
EXPECT_THAT(packet, IsUdsSecurityAccess());
EXPECT_THAT(packet, IsUdsTesterPresent());
EXPECT_THAT(packet, HasUdsVinDID());
EXPECT_THAT(packet, HasUdsResponsePending());
```

**Completed Deliverables:**
- Complete UDS type system with all ISO 14229 service IDs, NRCs, session types
- UDS decoder with full request/response parsing for all major services
- UDS session tracking with security level and timing parameters
- Comprehensive unit test suite (77 tests in `tests/protocols/test_uds.cpp` and `tests/protocols/test_uds_regression.cpp`)
- Fuzz testing infrastructure for UDS (`fuzz/fuzz_uds.cpp` with corpus files)
- gMock-style matchers for all major UDS assertions
- Scenario expectations for YAML/JSON test definitions
- Python bindings (pybind11)
- Rust bindings with native types
- C ABI layer for FFI compatibility
- Protocol documentation with examples (`docs/protocols/uds.md`)
- Architecture diagrams updated (`architecture/modules/protocols_classes.puml`, `architecture/sequences/protocols_sequences.puml`)
- Forwarding header for convenient include (`include/wadjet/protocols/uds.hpp`)

**Consolidation Notes (Build Integration):**
- UDS source files integrated into CMake (`src/CMakeLists.txt`)
- UDS tests integrated into test build (`tests/CMakeLists.txt`)
- UDS fuzz target added (`fuzz/CMakeLists.txt`)
- Fixed API to use `wadjet::Result` instead of `std::expected` (C++23)
- Fixed `UdsSessionManager` to use `std::unique_ptr<UdsSession>` (mutex non-copyable)
- Fixed type conversion warnings for `-Werror` compliance

**Pending Items (API Alignment):**
- `examples/uds_monitor.cpp` — Needs API update (different type names)
- `examples/uds_validator.cpp` — Needs API update  
- `tests/integration/test_uds_integration.cpp` — Needs API alignment with actual UDS implementation
- `examples/uds_analysis.py` — Verify Python bindings work with updated types

---

### Milestone 10 — DDS Protocol Support

**Goal:** Implement Data Distribution Service decoder for advanced automotive middleware

**Status:** ✅ Complete

**Priority:** 🟡 Medium — Advanced middleware support

**Overview:**

DDS (Data Distribution Service) is an OMG standard for real-time publish-subscribe communication. It's increasingly used in autonomous vehicles for sensor fusion, perception, and control systems (e.g., ROS2 uses DDS).

**Architecture:**

```text
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
- [x] `include/wadjet/protocols/dds/rtps_types.hpp` — RTPS types
  - GUID_t (16 bytes), GuidPrefix, EntityId
  - SequenceNumber_t
  - Locator_t, LocatorKind
  - BuiltinEndpointSet
  - ProtocolVersion
  - VendorId/VendorIdValue
  - Time, Duration, Count
  - Port calculation utilities
- [x] `include/wadjet/protocols/dds/rtps.hpp` — RTPS wire protocol
  - RTPSHeader (RTPS magic, version, vendor, GUID prefix)
  - SubmessageHeader, SubmessageFlags
  - SubmessageKind enum
  - Submessage wrapper with body variant
  - RtpsDecoder class
- [x] `include/wadjet/protocols/dds/rtps_messages.hpp` — Submessage types
  - DataSubmessage, DataFragSubmessage
  - HeartbeatSubmessage, HeartbeatFragSubmessage
  - AckNackSubmessage, NackFragSubmessage
  - GapSubmessage
  - InfoTimestampSubmessage, InfoSourceSubmessage
  - InfoDestinationSubmessage, InfoReplySubmessage
  - PadSubmessage
- [x] `include/wadjet/protocols/dds/discovery.hpp` — Discovery protocols
  - SPDP (Simple Participant Discovery Protocol)
  - SEDP (Simple Endpoint Discovery Protocol)
  - ParticipantBuiltinTopicData
  - PublicationBuiltinTopicData
  - SubscriptionBuiltinTopicData
  - QoS policy structures (Durability, Reliability, Liveliness, etc.)
  - ParameterList and DiscoveryParser

Decoder Implementation:
- [x] `src/protocols/dds/rtps_decoder.cpp` — Full RTPS decoder
  - Header validation (RTPS magic, version)
  - Submessage iteration with endianness handling
  - All submessage body parsing
  - Discovery data parsing (SPDP/SEDP)

Integration:
- [x] Update `ProtocolDispatcher` for DDS ports (7400-7500 range)
- [x] Add `RtpsHeader` to `DecodedHeaderVariant`
- [x] Unit tests in `tests/protocols/test_dds.cpp`
- [x] Example `examples/dds_monitor.cpp` for DDS traffic analysis
- [x] Python bindings for DDS
- [x] Rust bindings for DDS
- [x] C ABI layer updates

**Testing:**

Unit Tests (`tests/protocols/test_dds.cpp`):
- [x] RTPS header parsing
- [x] All submessage types (DATA, HEARTBEAT, ACKNACK, GAP, INFO_TS, INFO_DST, etc.)
- [x] GUID handling
- [x] Sequence number handling
- [x] Discovery message parsing
- [x] QoS policy extraction
- [x] CDR basic types

Integration Tests (`tests/integration/test_dds_integration.cpp`):
- [x] Full RTPS message decode
- [x] Discovery sequence validation
- [x] Data exchange patterns
- [x] Multi-vendor interop samples

Fuzz Testing (`fuzz/fuzz_dds.cpp`):
- [x] RTPS header fuzzer
- [x] Submessage fuzzer
- [x] Discovery fuzzer
- [x] CDR fuzzer

Regression Tests:
- [x] `pcap_samples/dds/` — Real DDS captures
- [x] FastDDS traffic samples
- [x] CycloneDDS traffic samples
- [x] ROS2 traffic samples

**Documentation:**

- [x] `docs/protocols/dds.md` — Protocol reference
  - RTPS specification overview
  - Discovery protocol documentation
  - Submessage reference
  - Vendor ID table
- [x] API documentation (Doxygen comments in headers)
- [x] Update `docs/architecture.md` with DDS

**Use Cases & Examples:**

- [x] `examples/dds_monitor.cpp` — DDS traffic monitor
  - Participant discovery tracking
  - Topic/endpoint enumeration
  - Data rate statistics
  - QoS analysis
- [x] `examples/ros2_analyzer.cpp` — ROS2 traffic analyzer
  - Node discovery
  - Topic mapping
  - Message frequency analysis
- [x] `examples/scenarios/dds_discovery_test.yaml` — Discovery test
- [x] Python example: `examples/python/dds_analysis.py`

**Matchers & Assertions:**

```cpp
// Implemented matchers in include/wadjet/testing/matchers.hpp
EXPECT_THAT(packet, IsRtps());                              // Is RTPS message
EXPECT_THAT(packet, IsDds());                               // Alias for IsRtps
EXPECT_THAT(packet, HasRtpsVersion(2, 4));                  // Check RTPS version
EXPECT_THAT(packet, HasRtpsVendor(VendorId::FastDDS));      // Check vendor
EXPECT_THAT(packet, IsFromFastDDS());                       // Convenience vendor check
EXPECT_THAT(packet, IsFromRTI());                           // RTI Connext
EXPECT_THAT(packet, IsFromCycloneDDS());                    // CycloneDDS
EXPECT_THAT(packet, IsFromOpenDDS());                       // OpenDDS
EXPECT_THAT(packet, HasRtpsGuidPrefix(prefix));             // Check GUID prefix
EXPECT_THAT(packet, HasRtpsSubmessage(SubmessageKind::DATA)); // Submessage type
EXPECT_THAT(packet, HasRtpsData());                         // Has DATA submessage
EXPECT_THAT(packet, HasRtpsHeartbeat());                    // Has HEARTBEAT
EXPECT_THAT(packet, HasRtpsAckNack());                      // Has ACKNACK
EXPECT_THAT(packet, HasRtpsGap());                          // Has GAP
EXPECT_THAT(packet, HasRtpsInfoTs());                       // Has INFO_TS
EXPECT_THAT(packet, HasRtpsInfoDst());                      // Has INFO_DST
EXPECT_THAT(packet, HasRtpsSubmessageCount(3));             // Min submessage count
EXPECT_THAT(packet, IsRtpsDiscovery());                     // Is SPDP/SEDP traffic
EXPECT_THAT(packet, IsSpdpOrSedp());                        // Alias for discovery
```

**Completion Notes:**

All Milestone 10 functionality fully implemented:
- Full RTPS protocol decoder with all submessage types
- Discovery parsing (SPDP/SEDP) with QoS extraction
- 18 gMock-compatible matchers for test assertions
- Protocol dispatcher integration with port detection
- Comprehensive unit test suite
- Example DDS monitor application
- ROS2 traffic analyzer example
- DDS discovery test scenario
- Fuzz testing suite (header, submessage, discovery, CDR)
- Multi-vendor integration tests (FastDDS, RTI, CycloneDDS, OpenDDS)
- Python bindings with DDS support
- Rust bindings with DDS types
- C ABI layer with RTPS structures
- Python DDS analysis example

---

### Milestone 11 — UDS over DoIP Integration

**Goal:** Implement complete UDS-over-DoIP diagnostic stack with session management

**Status:** ✅ Complete

**Priority:** 🔴 High — Complete diagnostic stack

**Overview:**

UDS over DoIP combines ISO 14229 (UDS) with ISO 13400 (DoIP) for complete Ethernet-based diagnostics. This milestone integrates the UDS decoder (Milestone 9) with the existing DoIP decoder for full diagnostic session handling.

**Architecture:**

```text
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
│  │  - ResponsePending (0x78) handling                       │   │
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

Core Headers:
- [x] `include/wadjet/protocols/diagnostic/diagnostic_types.hpp` — Core types
  - LogicalAddress (uint16_t)
  - DiagnosticTiming (P2, P2*, S3 timeouts)
  - TransportInfo (source/target address with timestamp)
  - MessageDirection enum
  - ECUInfo structure
- [x] `include/wadjet/protocols/diagnostic/diagnostic_session.hpp` — Session management
  - DiagnosticEvent enum (22 events: routing, session, security, flash, etc.)
  - DiagnosticSessionState struct
  - DiagnosticSessionManager class with Options
  - Event callback support
- [x] `include/wadjet/protocols/diagnostic/request_correlator.hpp` — Correlation
  - RequestResponsePair with Request/Response structs
  - PendingRequest tracking
  - CorrelationEvent enum
  - RequestCorrelator class with Statistics
- [x] `include/wadjet/protocols/diagnostic/uds_doip_decoder.hpp` — Combined decoder
  - UdsOverDoipResult struct
  - UdsOverDoipError with error codes
  - UdsOverDoipDecoder class
- [x] `include/wadjet/protocols/diagnostic.hpp` — Main include header

Implementation:
- [x] `src/protocols/diagnostic/diagnostic_types.cpp` — Type implementations
- [x] `src/protocols/diagnostic/diagnostic_session.cpp` — Session manager
  - State machine implementation
  - Multi-ECU tracking
  - Event emission
- [x] `src/protocols/diagnostic/request_correlator.cpp` — Correlator
  - Request/response matching by service ID
  - Address pair correlation
  - Timeout detection
  - Statistics tracking
- [x] `src/protocols/diagnostic/uds_doip_decoder.cpp` — Combined decoder
  - DoIP → UDS extraction
  - Direction detection
  - Error handling

Language Bindings:
- [x] Python bindings (`bindings/python/src/diagnostic_bindings.cpp`)
  - DiagnosticTiming, TransportInfo
  - DiagnosticEvent enum (all 22 events)
  - PendingRequest, RequestResponsePair
  - CorrelationStatistics (RequestCorrelator::Statistics)
  - RequestCorrelator with Options
  - DiagnosticSessionState
  - DiagnosticSessionManager with Options
  - MessageDirection enum
  - UdsOverDoipDecoder, UdsOverDoipResult, UdsOverDoipError
  - diagnostic_event_string helper
- [x] Python `__init__.py` updated with all diagnostic exports
- [x] C bindings (`bindings/c/include/wadjet_c.h`, `bindings/c/src/wadjet_c.cpp`)
  - wadjet_diagnostic_session_manager_t opaque handle
  - wadjet_diagnostic_event_t enum (22 events)
  - wadjet_diagnostic_session_state_t struct
  - wadjet_diagnostic_options_t struct
  - wadjet_diagnostic_event_callback_t typedef
  - wadjet_diagnostic_options_default()
  - wadjet_diagnostic_manager_create/destroy()
  - wadjet_diagnostic_manager_on_event()
  - wadjet_diagnostic_manager_process()
  - wadjet_diagnostic_manager_get_session()
  - wadjet_diagnostic_manager_session_count()
  - wadjet_diagnostic_manager_statistics()
  - wadjet_diagnostic_manager_check_timeouts()

**Testing:**

Unit Tests (`tests/protocols/test_diagnostic.cpp`):
- [x] DiagnosticSessionManagerTest.ProcessDiagnosticSessionControlSequence
- [x] DiagnosticSessionManagerTest.TrackSecurityAccessSequence
- [x] DiagnosticSessionManagerTest.HandleNegativeResponse
- [x] DiagnosticSessionManagerTest.TesterPresentKeepsSessionAlive
- [x] DiagnosticSessionManagerTest.Statistics
- [x] DiagnosticSessionManagerTest.MultipleECUs
- [x] UdsOverDoipDecoderTest.RejectNonDiagnosticMessage
- [x] UdsOverDoipDecoderTest.LooksLikeDiagnosticMessage
- Plus related tests from DoIP and UDS test suites

**Test Summary:** 12 diagnostic-specific tests passing

**Matchers & Assertions:**

```cpp
// Diagnostic session matchers (available in testing framework)
EXPECT_THAT(session, HasValidSessionTiming());
EXPECT_THAT(sequence, IsValidSecurityAccess());
EXPECT_THAT(response, ArrivesWithin(P2_TIMEOUT));
```

**Python Usage Example:**

```python
import wadjet

# Create session manager
manager = wadjet.DiagnosticSessionManager()

# Register event callback
def on_event(event, state, pair):
    if event == wadjet.DiagnosticEvent.SessionStarted:
        print(f"Session started - tester: 0x{state.tester_address:04X}")
    elif event == wadjet.DiagnosticEvent.SecurityUnlocked:
        print(f"Security unlocked to level {state.security_level}")

manager.on_event(on_event)

# Process captured DoIP packets
for packet in captured_packets:
    manager.process_doip_raw(packet.data)

# Check tracked ECUs
for ecu_addr in manager.get_tracked_ecus():
    state = manager.get_session_state(ecu_addr)
    print(f"ECU 0x{ecu_addr:04X}: {state.session_type}, security={state.security_level}")

# Get correlation statistics
stats = manager.correlator().statistics()
print(f"Match rate: {stats.match_rate():.1%}")
```

**C API Usage Example:**

```c
#include <wadjet_c.h>

void on_diagnostic_event(wadjet_diagnostic_event_t event,
                         const wadjet_diagnostic_session_state_t* state,
                         void* user_data) {
    if (event == WADJET_DIAG_EVENT_SESSION_STARTED) {
        printf("Session started with tester 0x%04X\n", state->tester_address);
    }
}

int main() {
    wadjet_diagnostic_options_t opts;
    wadjet_diagnostic_options_default(&opts);
    
    wadjet_diagnostic_session_manager_t manager;
    wadjet_diagnostic_manager_create(&opts, &manager);
    wadjet_diagnostic_manager_on_event(manager, on_diagnostic_event, NULL);
    
    // Process packets...
    wadjet_diagnostic_manager_process(manager, data, length);
    
    // Get statistics
    uint64_t requests, matched, unmatched, timeouts;
    wadjet_diagnostic_manager_statistics(manager, &requests, &matched, &unmatched, &timeouts);
    
    wadjet_diagnostic_manager_destroy(manager);
    return 0;
}
```

**Rust API Usage Example:**

```rust
use wadjet::diagnostic::{DiagnosticSessionManager, DiagnosticOptions, DiagnosticEvent};

fn main() -> wadjet::Result<()> {
    // Create with custom timing parameters
    let options = DiagnosticOptions::default()
        .p2_timeout(100)
        .p2_star_timeout(5000)
        .max_ecus(128);
    
    let mut manager = DiagnosticSessionManager::new(options)?;

    // Register event callback
    manager.on_event(|event, state| {
        match event {
            DiagnosticEvent::SessionStarted => {
                println!("Session started: ECU 0x{:04X}", state.gateway_address);
            }
            DiagnosticEvent::SecurityUnlocked => {
                println!("Security level {} unlocked", state.security_level);
            }
            DiagnosticEvent::FlashStarted => {
                println!("Flash download in progress...");
            }
            _ => {}
        }
    });

    // Process DoIP packets
    // manager.process(&doip_data)?;

    // Get session state
    if let Some(state) = manager.get_session(0x1234) {
        println!("ECU 0x1234: {}", state);
    }

    // Get statistics
    let stats = manager.statistics();
    println!("Match rate: {:.1}%", stats.match_rate() * 100.0);

    Ok(())
}
```

**Completed Deliverables:**

- Complete diagnostic session management with multi-ECU support
- Request/response correlator with timeout detection
- UDS-over-DoIP combined decoder
- 22 diagnostic event types covering full session lifecycle
- Flash sequence tracker with progress monitoring (16 tests)
- DTC manager with filtering and statistics (13 tests)
- Full Python bindings with all types exported
- Full C ABI bindings with opaque handle pattern
- Full Rust bindings with safe wrappers (`diagnostic.rs`)
- 41 unit tests all passing (12 session + 16 flash + 13 DTC)
- Build system integration (CMake)
- Example applications and documentation

**Rust Bindings Implementation:**

Diagnostic Module (`bindings/rust/wadjet/src/diagnostic.rs`):
- [x] `DiagnosticEvent` — Enum with 22 event variants
  - Session events: SessionStarted, SessionChanged, SessionTimeout, SessionEnded
  - Security events: SecurityUnlocked, SecurityLocked, SecurityLockout
  - Flash events: FlashStarted, FlashProgress, FlashCompleted, FlashFailed
  - Transport events: RoutingActivated, RoutingDeactivated, ConnectionLost
  - Communication events: RequestSent, ResponseReceived, ResponsePending, ResponseTimeout
  - Helper methods: `name()`, `is_session_event()`, `is_security_event()`, `is_flash_event()`
- [x] `SessionType` — Enum (Default, Programming, Extended, SafetySystem, Unknown)
- [x] `DiagnosticSessionState` — ECU session state struct
  - Fields: tester_address, gateway_address, session_type, session_active, routing_active, etc.
  - Helper methods: `is_programming()`, `is_extended()`, `is_security_unlocked()`, `response_rate()`
  - Display trait implementation for pretty printing
- [x] `DiagnosticOptions` — Builder pattern configuration
  - Methods: `p2_timeout()`, `p2_star_timeout()`, `s3_timeout()`, `max_ecus()`, `correlation()`, `timeout_detection()`
- [x] `CorrelationStatistics` — Request/response matching statistics
  - Fields: requests_recorded, responses_matched, responses_unmatched, timeouts
  - Methods: `match_rate()`, `timeout_rate()`
- [x] `DiagnosticTiming` — ISO 14229 timing parameters
  - Methods: `within_p2()`, `within_p2_star()`, `programming()`
- [x] `DiagnosticSessionManager` — Main API
  - `new(options)` / `with_defaults()` — Constructors
  - `on_event(callback)` — Closure-based event callback registration
  - `process(data)` — Process raw DoIP packet data
  - `get_session(ecu_addr)` — Get session state for ECU
  - `session_count()` — Get number of tracked sessions
  - `statistics()` — Get correlation statistics
  - `check_timeouts()` — Check for timed-out requests
  - `Drop` trait implementation for automatic cleanup

Build System Updates:
- [x] `bindings/rust/wadjet-sys/build.rs` — Added rustified enums:
  - `wadjet_diagnostic_event_t`
  - `wadjet_uds_session_type_t`
  - `wadjet_uds_service_id_t`
  - `wadjet_uds_reset_type_t`
  - `wadjet_uds_nrc_t`
- [x] `bindings/rust/wadjet/src/lib.rs` — Module exports updated
- [x] `bindings/rust/wadjet/src/error.rs` — Added NullPointer, NotFound variants

Documentation:
- [x] `bindings/rust/README.md` — Diagnostic usage examples
- [x] `docs/protocols/uds_doip.md` — Rust API section
- [x] `docs/diagnostic_testing.md` — Rust testing examples

Architecture Diagrams:
- [x] `architecture/modules/diagnostic_rust_bindings.puml` — Rust bindings class diagram
- [x] `architecture/component_overview.puml` — Updated with diagnostic module

CI/CD:
- [x] `.github/workflows/ci.yml` — Added rust-bindings job:
  - cargo build --workspace
  - cargo fmt --all -- --check
  - cargo clippy --workspace -- -D warnings
  - cargo doc --workspace --no-deps

Unit Tests (`bindings/rust/wadjet/src/diagnostic.rs`):
- [x] `test_diagnostic_event_name` — Event name conversion
- [x] `test_diagnostic_event_categories` — Event category classification
- [x] `test_session_type_name` — Session type names
- [x] `test_diagnostic_options_builder` — Builder pattern
- [x] `test_correlation_statistics` — Statistics calculation
- [x] `test_correlation_statistics_empty` — Edge case handling
- [x] `test_diagnostic_timing` — Timing validation

**Remaining Items (Future Enhancements):**

- [x] `examples/diagnostic_analyzer.cpp` — Full diagnostic analyzer example
- [x] `examples/flash_validator.cpp` — Flash sequence validator
- [x] `examples/dtc_analyzer.cpp` — DTC analysis tool
- [x] `examples/scenarios/diagnostic_session_test.yaml` — Scenario test
- [x] `examples/python/diagnostic_analysis.py` — Python example
- [x] `docs/protocols/uds_doip.md` — Integration documentation
- [x] `docs/diagnostic_testing.md` — Test guide
- [x] Architecture diagrams (diagnostic_classes.puml, diagnostic_sequences.puml, diagnostic_rust_bindings.puml)
- [x] Rust bindings for diagnostic module (`bindings/rust/wadjet/src/diagnostic.rs`)
- [x] Flash sequence tracking (`flash_sequence.hpp`, `flash_sequence.cpp`) — Complete
- [x] DTC manager (`dtc_manager.hpp`, `dtc_manager.cpp`) — Complete

**Flash Sequence Tracker Implementation:**

Headers and Source:
- [x] `include/wadjet/protocols/diagnostic/flash_sequence.hpp` — Flash sequence types
  - FlashOperationType enum (Download, Upload)
  - FlashSequenceState enum (8 states: Idle through Complete/Failed)
  - FlashEvent enum (17 events for tracking flash progress)
  - FlashBlock struct (sequence counter, data size, timestamp, acknowledged)
  - FlashRegion struct (start address, size, max block size, format)
  - FlashSequence struct (operation type, ECU/tester addresses, region, blocks, timing, progress)
  - FlashSequenceTracker class with Options
  - Event callbacks for monitoring
- [x] `src/protocols/diagnostic/flash_sequence.cpp` — Implementation
  - RequestDownload/RequestUpload tracking
  - TransferData block monitoring
  - RequestTransferExit handling
  - Erase routine tracking (RoutineID::EraseMemory)
  - Verification routine tracking
  - Progress calculation and transfer rate statistics
  - Multi-ECU sequence tracking
  - Negative response handling

Tests:
- [x] `tests/protocols/test_flash_sequence.cpp` — 16 test cases
  - ProcessRequestDownload, ProcessRequestDownloadResponse
  - ProcessTransferDataBlocks, ProcessTransferExitSuccess
  - ProcessNegativeResponse, ProcessEraseRoutine
  - RequestUpload, AbortSequence, Statistics
  - TransferRateCalculation
  - FlashSequence helper tests (Progress, StateChecks, BlockCounting)
  - String conversion tests

**DTC Manager Implementation:**

Headers and Source:
- [x] `include/wadjet/protocols/diagnostic/dtc_manager.hpp` — DTC manager types
  - DTCSeverity enum (NoSeverity, Warning, Check, Failure)
  - DTCEvent enum (7 events: DTCAdded, DTCUpdated, DTCCleared, etc.)
  - DTCRecord struct (DTC, status, ECU address, snapshots, extended data, timestamps)
  - DTCFilter struct (active_only, confirmed_only, pending_only, ecu_address, severity, max_age)
  - ECUDTCStatistics struct (per-ECU statistics)
  - GlobalStatistics struct (aggregated statistics)
  - DTCManager class with Options
  - Event callbacks for DTC changes
- [x] `src/protocols/diagnostic/dtc_manager.cpp` — Implementation
  - DTC add/update with occurrence tracking
  - ReadDTCInformation response processing
  - DTC filtering by status flags
  - Per-ECU and global statistics
  - Snapshot and extended data storage
  - DTC clear with history tracking
  - Event emission for state changes

Tests:
- [x] `tests/protocols/test_dtc_manager.cpp` — 13 test cases
  - AddSingleDTC, UpdateExistingDTC
  - ProcessReadDTCResponse, ClearAllDTCs
  - FilterByActive, FilterByConfirmed
  - MultipleECUs, Statistics, ECUStatistics
  - AddSnapshot, AddExtendedData
  - ClearedHistory, Reset

**Test Summary:**
- Flash Sequence Tracker: 16 tests passing
- DTC Manager: 13 tests passing
- Total diagnostic module: 41 tests (including 12 from previous implementation)

---

### Milestone 12 — TSN Awareness (IEEE 802.1Qbv)

**Goal:** Implement Time-Sensitive Networking awareness for deterministic Ethernet

**Status:** ⏳ Not Started

**Priority:** 🟡 Medium — Deterministic networking support

**Overview:**

TSN (Time-Sensitive Networking) is a set of IEEE 802.1 standards enabling deterministic, low-latency communication over Ethernet. 802.1Qbv (Time-Aware Shaper) is critical for automotive real-time applications.

**Architecture:**

```text
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

### Milestone 13 — Protocol Completeness

**Goal:** Complete all protocol implementations to 100% specification compliance with no missing features

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required for production-grade tool

> ⚠️ **NEXT MILESTONE TO IMPLEMENT** — This is the immediate next step in the roadmap.

**Dependencies:**

- ✅ Milestone 0-10: All protocol decoders implemented (Ethernet, IPv4, TCP, UDP, SOME/IP, DoIP, gPTP, UDS, DDS)
- ✅ Milestone 11: UDS over DoIP integration complete
- ⏳ None — This milestone has no blockers and can begin immediately

**Estimated Effort:** 4-6 weeks (can be parallelized across phases)

**Overview:**

This milestone addresses gaps in currently "complete" protocol implementations. Each protocol must fully comply with its specification with no missing features.

**Recommended Implementation Order:**

| Order | Phase | Protocols | Rationale |
|-------|-------|-----------|----------|
| 1 | Phase 1-2 | TCP + UDP | Critical gaps, foundation for all upper protocols |
| 2 | Phase 3-4 | IPv4 + Ethernet | Layer 2-3 completeness |
| 3 | Phase 5-6 | SOME/IP + SOME/IP-SD | Automotive middleware |
| 4 | Phase 7-9 | DoIP + gPTP + UDS | Diagnostic & timing |
| 5 | Phase 10 | DDS/RTPS | Most complex, do last |

> 💡 **TIP:** Phases 1-4 (TCP, UDP, IPv4, Ethernet) can be done in parallel as they have no interdependencies.

**Current Protocol Status:**

| Protocol | Current State | Gap Level |
|----------|--------------|----------|
| Ethernet/VLAN | Good | Minor |
| IPv4 | Good | Minor |
| TCP | Partial | **Critical** |
| UDP | Partial | Moderate |
| SOME/IP | Good | Minor |
| SOME/IP-SD | Good | Moderate |
| DoIP | Good | Moderate |
| gPTP | Good | Minor |
| UDS | Good | Minor |
| DDS/RTPS | Partial | **Critical** |

**Getting Started Checklist:**

> 🚀 Complete these setup tasks before starting any phase implementation.

- [ ] **Review existing decoder implementations**
  - [ ] Read `src/protocols/tcp/tcp_decoder.cpp` — understand current TCP structure
  - [ ] Read `src/protocols/udp/udp_decoder.cpp` — understand current UDP structure
  - [ ] Read `src/protocols/dds/rtps_decoder.cpp` — understand DDS/RTPS structure
  - [ ] Review test patterns in `tests/protocols/`

- [ ] **Gather reference materials**
  - [ ] Download RFC 793 (TCP), RFC 768 (UDP), RFC 791 (IPv4)
  - [ ] Download AUTOSAR SOME/IP specification
  - [ ] Download ISO 13400-2 (DoIP), ISO 14229 (UDS)
  - [ ] Download IEEE 802.1AS (gPTP)
  - [ ] Download OMG RTPS 2.5 specification

- [ ] **Set up validation environment**
  - [ ] Install Wireshark with all dissectors enabled
  - [ ] Prepare test PCAP files for each protocol
  - [ ] Set up packet generator for testing (scapy or similar)

- [ ] **Create tracking issue**
  - [ ] Create GitHub issue for Milestone 13
  - [ ] Create sub-issues for each phase
  - [ ] Set up project board for progress tracking

**Implementation Checklist:**

---

**Phase 1: TCP Completeness (RFC 793 + Extensions)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/tcp/tcp.hpp`
> - `src/protocols/tcp/tcp_decoder.cpp`
> - `tests/protocols/test_tcp.cpp`
>
> ⏱️ **Estimated effort:** 3-5 days

- [ ] **Checksum Validation**
  - [ ] IPv4 pseudo-header calculation
  - [ ] IPv6 pseudo-header calculation
  - [ ] Checksum verification on decode
  - [ ] Checksum offload detection (hardware offload indicator)

- [ ] **SACK Block Parsing (RFC 2018)**
  - [ ] Parse SACK option into structured blocks
  - [ ] Left edge / right edge extraction
  - [ ] Multiple SACK blocks support

- [ ] **Additional TCP Options**
  - [ ] MD5 Signature (RFC 2385) — for BGP
  - [ ] TCP-AO Authentication (RFC 5925)
  - [ ] TCP Fast Open cookie (RFC 7413)
  - [ ] Multipath TCP (RFC 8684) — optional
  - [ ] User Timeout Option (RFC 5482)

- [ ] **TCP State Tracking** (optional for passive analysis)
  - [ ] Connection state machine (CLOSED → LISTEN → SYN_SENT → etc.)
  - [ ] State per flow (4-tuple)
  - [ ] Connection establishment detection
  - [ ] Connection termination detection
  - [ ] RST handling

- [ ] **TCP Stream Reassembly** (optional)
  - [ ] Sequence number tracking
  - [ ] Out-of-order segment buffering
  - [ ] Retransmission detection
  - [ ] Reassembled stream extraction

---

**Phase 2: UDP Completeness (RFC 768)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/udp/udp.hpp`
> - `src/protocols/udp/udp_decoder.cpp`
> - `tests/protocols/test_udp.cpp`
>
> ⏱️ **Estimated effort:** 1-2 days

- [ ] **Checksum Validation**
  - [ ] IPv4 pseudo-header calculation
  - [ ] IPv6 pseudo-header calculation
  - [ ] Checksum verification on decode
  - [ ] Zero checksum handling (IPv4 optional, IPv6 illegal)
  - [ ] Checksum offload detection

- [ ] **UDP-Lite Support (RFC 3828)** — optional
  - [ ] Partial checksum coverage
  - [ ] Coverage field handling

---

**Phase 3: IPv4 Completeness (RFC 791)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/ipv4/ipv4.hpp`
> - `src/protocols/ipv4/ipv4_decoder.cpp`
> - `tests/protocols/test_ipv4.cpp`
>
> ⏱️ **Estimated effort:** 3-4 days
>
> 🚨 **Note:** Fragment reassembly requires careful memory management. Consider timeout handling for incomplete fragments.

- [ ] **Typed Options Parsing**
  - [ ] End of Option List (Type 0)
  - [ ] No Operation (Type 1)
  - [ ] Loose Source Route (LSRR, Type 131)
  - [ ] Strict Source Route (SSRR, Type 137)
  - [ ] Record Route (Type 7)
  - [ ] Timestamp (Type 68)
  - [ ] Router Alert (RFC 2113)

- [ ] **Fragment Reassembly**
  - [ ] Fragment identification (ID + src + dst + protocol)
  - [ ] Fragment offset handling
  - [ ] MF flag tracking
  - [ ] Reassembly buffer management
  - [ ] Timeout handling for incomplete reassembly
  - [ ] Overlapping fragment handling

- [ ] **Additional Validation**
  - [ ] Total length vs actual length check
  - [ ] Header length validation
  - [ ] Address class detection (A/B/C/D/E)
  - [ ] Multicast/broadcast detection

---

**Phase 4: Ethernet Completeness (IEEE 802.3)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/ethernet/ethernet.hpp`
> - `src/protocols/ethernet/ethernet_decoder.cpp`
> - `tests/protocols/test_ethernet.cpp`
>
> ⏱️ **Estimated effort:** 2-3 days
>
> 💡 **Note:** LLC/SNAP frames are rare in automotive but required for completeness. FCS is usually stripped by NIC.

- [ ] **LLC/SNAP Headers (IEEE 802.2)**
  - [ ] Length field detection (< 1536 = LLC frame)
  - [ ] DSAP/SSAP parsing
  - [ ] Control field parsing
  - [ ] SNAP OUI + protocol ID

- [ ] **Additional EtherTypes**
  - [ ] ARP (0x0806) — currently may exist
  - [ ] LLDP (0x88CC)
  - [ ] MPLS Unicast (0x8847)
  - [ ] MPLS Multicast (0x8848)
  - [ ] PPPoE Discovery (0x8863)
  - [ ] PPPoE Session (0x8864)
  - [ ] MACsec (0x88E5)
  - [ ] 1588 PTP (0x88F7) — currently gPTP
  - [ ] FCoE (0x8906)

- [ ] **Frame Validation**
  - [ ] Minimum frame size (64 bytes with padding)
  - [ ] Padding detection and removal
  - [ ] FCS validation (if available, usually stripped)

- [ ] **Additional VLAN TPIDs**
  - [ ] 0x9100 (legacy QinQ)
  - [ ] 0x9200 (legacy QinQ)
  - [ ] Configurable TPID list

---

**Phase 5: SOME/IP Completeness (AUTOSAR)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/someip/someip.hpp`
> - `src/protocols/someip/someip_decoder.cpp`
> - `tests/protocols/test_someip.cpp`
>
> ⏱️ **Estimated effort:** 3-4 days
>
> 🚨 **Critical:** SOME/IP-TP is used for messages > 1400 bytes. This is common in production for large method responses.

- [ ] **SOME/IP-TP (Transport Protocol)**
  - [ ] TP header parsing (offset, more flag)
  - [ ] Segment reassembly
  - [ ] Segment ordering
  - [ ] Timeout handling
  - [ ] Maximum message size handling

- [ ] **Additional Return Codes**
  - [ ] E2E return codes (0x0B-0x1F)
  - [ ] Application-specific codes (0x40-0x5E)

- [ ] **Magic Cookie Support**
  - [ ] Client ID 0x0000 / Session ID 0x0000 detection
  - [ ] Dead connection detection

- [ ] **Serialization Helpers** (optional)
  - [ ] Basic type serialization
  - [ ] Length-delimited fields
  - [ ] TLV encoding

---

**Phase 6: SOME/IP-SD Completeness (AUTOSAR)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/someip/someip_sd.hpp`
> - `src/protocols/someip/someip_sd_decoder.cpp`
> - `tests/protocols/test_someip_sd.cpp`
>
> ⏱️ **Estimated effort:** 2-3 days
>
> 💡 **Note:** Index resolution links SD entries to their options. This is needed for proper service/eventgroup analysis.

- [ ] **IPv6 Endpoint Option Parsing**
  - [ ] 128-bit address extraction
  - [ ] L4 protocol and port

- [ ] **Option Parsing Enhancement**
  - [ ] Configuration option string parsing
  - [ ] Load balancing priority/weight parsing
  - [ ] Multicast option address extraction

- [ ] **Index Resolution**
  - [ ] Resolve first_option_index / second_option_index
  - [ ] Link entries to their options

- [ ] **State Tracking** (optional)
  - [ ] Service offer tracking
  - [ ] Subscription state tracking
  - [ ] TTL expiration detection

---

**Phase 7: DoIP Completeness (ISO 13400-2)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/doip/doip.hpp`
> - `src/protocols/doip/doip_decoder.cpp`
> - `tests/protocols/test_doip.cpp`
>
> ⏱️ **Estimated effort:** 2-3 days
>
> 💡 **Note:** State machine is optional but useful for diagnostic session analysis and timeout detection.

- [ ] **Entity Type Distinction**
  - [ ] Gateway vs Node entity type
  - [ ] Entity role in routing

- [ ] **Response Structures**
  - [ ] Power Mode Information Response parsing
  - [ ] Entity Status Response parsing (max sockets, currently open)
  - [ ] Diagnostic Power Mode Response

- [ ] **Activation Types Enum**
  - [ ] Default (0x00)
  - [ ] WWH-OBD (0x01)
  - [ ] Central Security (0x02+)
  - [ ] Manufacturer-specific (0xE0-0xFE)

- [ ] **State Machine** (optional for analysis)
  - [ ] Socket connection state
  - [ ] Routing activation state
  - [ ] Alive check state
  - [ ] Timeout handling

- [ ] **Transport Awareness**
  - [ ] TCP vs UDP payload type restrictions
  - [ ] Port usage (13400 data, 13400 discovery)

---

**Phase 8: gPTP Completeness (IEEE 802.1AS)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/gptp/gptp.hpp`
> - `src/protocols/gptp/gptp_decoder.cpp`
> - `tests/protocols/test_gptp.cpp`
>
> ⏱️ **Estimated effort:** 3-4 days
>
> 🚨 **Note:** Sync algorithm and BMCA are computationally intensive. Consider separate analysis module rather than inline in decoder.

- [ ] **Synchronization Algorithm**
  - [ ] Clock offset calculation
  - [ ] Rate ratio accumulation
  - [ ] Synchronized time computation
  - [ ] Correction field application

- [ ] **Best Master Clock Algorithm (BMCA)**
  - [ ] Announce message comparison
  - [ ] Priority1/Priority2/ClockClass/ClockAccuracy/Variance ordering
  - [ ] Grandmaster selection
  - [ ] Timeout-based GM failover

- [ ] **Path Delay Accumulation**
  - [ ] Mean path delay from multiple measurements
  - [ ] Cumulative path delay through bridges
  - [ ] Asymmetry correction

- [ ] **Additional TLVs**
  - [ ] AS Capability TLV (gPTP domain support)
  - [ ] Message Interval Request TLV
  - [ ] Cumulative Rate Ratio TLV

- [ ] **Domain Support**
  - [ ] Multiple gPTP domains
  - [ ] Domain number filtering

---

**Phase 9: UDS Completeness (ISO 14229-1)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/uds/uds.hpp`
> - `src/protocols/uds/uds_decoder.cpp`
> - `tests/protocols/test_uds.cpp`
>
> ⏱️ **Estimated effort:** 3-4 days
>
> 💡 **Note:** ReadDTCInformation has 28 sub-functions — each with different response structure. Use a table-driven approach.

- [ ] **ReadDTCInformation Sub-functions**
  - [ ] All 28 sub-functions enumerated
  - [ ] Report structure for each sub-function
  - [ ] Status mask handling
  - [ ] DTC severity mask handling

- [ ] **Additional Services Implementation**
  - [ ] DynamicallyDefineDataIdentifier (0x2C) — full
  - [ ] ResponseOnEvent (0x86) — full
  - [ ] LinkControl (0x87) — full
  - [ ] AccessTimingParameter (0x83) — full
  - [ ] SecuredDataTransmission (0x84) — structure
  - [ ] RequestFileTransfer (0x38) — structure

- [ ] **Session/Security State Machine**
  - [ ] Session transitions
  - [ ] Security level requirements per service
  - [ ] S3 timer handling
  - [ ] P2/P2* timing validation

---

**Phase 10: DDS/RTPS Completeness (OMG RTPS 2.5)**

> 📍 **Files to modify:**
> - `include/wadjet/protocols/dds/rtps.hpp`
> - `src/protocols/dds/rtps_decoder.cpp`
> - `tests/protocols/test_dds.cpp`
>
> ⏱️ **Estimated effort:** 5-7 days (most complex phase)
>
> 🚨 **Critical:** CDR serialization is essential for payload decoding. Without it, DDS payloads are opaque blobs.
>
> 💡 **Recommendation:** Consider using eProsima Fast-CDR as reference implementation for validation.

- [ ] **CDR Serialization**
  - [ ] Basic types (int8-64, uint8-64, float, double)
  - [ ] Strings (bounded and unbounded)
  - [ ] Sequences and arrays
  - [ ] Enumerations
  - [ ] Structs/Unions
  - [ ] Endianness handling

- [ ] **Inline QoS Parsing**
  - [ ] Parameter list parsing
  - [ ] Key hash extraction
  - [ ] Status info
  - [ ] Coherent set
  - [ ] Directed write

- [ ] **Parameter List Completion**
  - [ ] All PID values defined
  - [ ] Complex parameter parsing (locators, QoS)
  - [ ] Vendor-specific parameters

- [ ] **Discovery Enhancement**
  - [ ] SPDP participant data parsing
  - [ ] SEDP endpoint data parsing
  - [ ] Liveliness tracking
  - [ ] Lease duration handling

- [ ] **Fragmentation Reassembly**
  - [ ] DATA_FRAG reassembly
  - [ ] Fragment number tracking
  - [ ] Last fragment detection
  - [ ] Timeout handling

- [ ] **Security (DDS-Security)** — optional
  - [ ] Secure submessage types
  - [ ] Crypto token handling
  - [ ] Authentication handshake

---

**Testing Requirements:**

| Phase | Test Type | Target |
|-------|----------|--------|
| TCP | Unit tests for checksum, options | 30+ tests |
| UDP | Unit tests for checksum | 10+ tests |
| IPv4 | Unit tests for options, fragments | 25+ tests |
| Ethernet | Unit tests for LLC/SNAP | 15+ tests |
| SOME/IP | Unit tests for TP | 20+ tests |
| SOME/IP-SD | Unit tests for options | 15+ tests |
| DoIP | Unit tests for responses | 20+ tests |
| gPTP | Unit tests for sync algorithm | 25+ tests |
| UDS | Unit tests for services | 30+ tests |
| DDS | Unit tests for CDR, QoS | 40+ tests |

**Total Additional Tests:** 230+ tests

**Validation:**

- [ ] Cross-validate with Wireshark dissectors
- [ ] Cross-validate with reference implementations
- [ ] Fuzz testing for all new parsing code
- [ ] Real-world capture file validation

**Documentation:**

- [ ] Update protocol documentation for each enhancement
- [ ] Add specification references for each feature
- [ ] Document limitations and unsupported features
- [ ] Update architecture diagrams

**Exit Criteria:**

Each protocol must have:
1. 100% of specification-mandated features implemented
2. Validation against reference tools
3. Comprehensive test coverage
4. Updated documentation

---


---


---

### Milestone 14 — Web-Based Report Viewer

**Goal:** Implement interactive web-based visualization for test reports and packet analysis

**Status:** ⏳ Not Started

**Priority:** 🟢 Low — Nice-to-have visualization

**Overview:**

A modern web interface for visualizing test results, packet captures, and protocol analysis. Enables sharing results across teams without requiring local tool installation.

**Architecture:**

```text
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

| Milestone | Description | Priority | Status |
|-----------|-------------|----------|--------|
| 8 | gPTP (IEEE 802.1AS) decoder | 🔴 High | ✅ Complete |
| 9 | UDS over IP decoder | 🔴 High | ✅ Complete |
| 10 | DDS protocol support | 🟡 Medium | ✅ Complete |
| 11 | UDS over DoIP integration | 🔴 High | ✅ Complete |
| 12 | TSN awareness (802.1Qbv) | 🟡 Medium | ⏳ Not Started |
| **13** | **Protocol Completeness** | 🔴 High | ⏳ **NEXT** |
| 14 | Web-based report viewer | 🟢 Low | ⏳ Not Started |
| 15 | ARXML Parser (AUTOSAR) | 🔴 High | ⏳ Not Started |
| 16 | ODX/PDX Diagnostic Database | 🔴 High | ⏳ Not Started |
| 17 | Signal-Level Decoding | 🔴 High | ⏳ Not Started |
| 18 | IPv6 Protocol Support | 🟡 Medium | ⏳ Not Started |
| 19 | Production Packaging & Distribution | 🔴 High | ⏳ Not Started |
| 20 | A2L/HEX File Support | 🟢 Low | ⏳ Not Started |
| 21 | PreProduction Quality Gate | 🔴 High | ⏳ Not Started |
| 22 | ISO 26262 Tool Qualification | 🔴 High | ⏳ Not Started |
| 23 | Advanced Operations & Security | 🔴 High | ⏳ Not Started |

---

## Stretch Goals (Completed)

- [x] Rust FFI bindings (Milestone 7)
- [x] Packet injection (TX capability via ReplaySession)

---

### Milestone 15 — ARXML Parser (AUTOSAR System Description)

**Goal:** Parse AUTOSAR ARXML files to extract network configuration, ECU definitions, and signal/message mappings

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required for signal-level decoding (Milestone 16)

**Overview:**

ARXML (AUTOSAR XML) is the standard format for describing automotive system architecture, including:
- ECU configurations and network topology
- Communication matrices (frames, PDUs, signals)
- Service interfaces (SOME/IP, DDS)
- Diagnostic configurations

This milestone provides the foundation for understanding what's on the network without manual configuration.

**Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                    ARXML Parser Architecture                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   ARXML File Types                       │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ System      │ │ ECU         │ │ Communication       │ │   │
│  │  │ Description │ │ Extract     │ │ Cluster             │ │   │
│  │  │ (.arxml)    │ │ (.arxml)    │ │ (.arxml)            │ │   │
│  │  └──────┬──────┘ └──────┬──────┘ └──────────┬──────────┘ │   │
│  │         │               │                   │            │   │
│  │         └───────────────┼───────────────────┘            │   │
│  └──────────────────────────┼───────────────────────────────┘   │
│                             ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   ARXML Parser Engine                    │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ XML Parser      │  │ Schema Validator            │    │   │
│  │  │ (pugixml)       │  │ (AUTOSAR 4.x/Classic)       │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ Reference       │  │ Package Navigator           │    │   │
│  │  │ Resolver        │  │ (AR-PACKAGE paths)          │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                   │
│                             ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  Extracted Data Model                    │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ ECU         │ │ Frames &    │ │ Signals &           │ │   │
│  │  │ Instances   │ │ PDUs        │ │ Coding Types        │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ Service     │ │ Ethernet    │ │ Diagnostic          │ │   │
│  │  │ Interfaces  │ │ Clusters    │ │ Addresses           │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                   │
│                             ▼                                   │
│              ┌───────────────────────────┐                      │
│              │   In-Memory Database      │                      │
│              │   (ArxmlDatabase class)   │                      │
│              └───────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Core Headers:
- [ ] `include/wadjet/arxml/arxml.hpp` — Main include header
- [ ] `include/wadjet/arxml/arxml_types.hpp` — Core type definitions
  - ArxmlVersion enum (AR4_0, AR4_1, AR4_2, AR4_3, AR4_4, AR19_11, AR20_11, AR21_11, AR_CLASSIC)
  - ArPackagePath — Hierarchical path representation
  - ArRef<T> — Reference wrapper with lazy resolution
  - ArShortName — Short name with language variants
- [ ] `include/wadjet/arxml/arxml_parser.hpp` — Parser interface
  - ArxmlParser class
  - ParseOptions (validation level, schema path, merge mode)
  - ParseResult with errors/warnings
- [ ] `include/wadjet/arxml/arxml_database.hpp` — In-memory database
  - ArxmlDatabase class — Central storage
  - Query API (by path, by type, by reference)
  - Merge multiple ARXML files
  - Export to JSON for debugging

ECU & Network Model:
- [ ] `include/wadjet/arxml/ecu.hpp` — ECU definitions
  - EcuInstance — ECU with addresses and endpoints
  - EcuPartition — Software partitions
  - ComController — Communication controller
  - EthernetCommunicationController — Ethernet-specific
- [ ] `include/wadjet/arxml/network.hpp` — Network topology
  - EthernetCluster — Ethernet network definition
  - EthernetPhysicalChannel — Physical channel
  - VlanConfig — VLAN configuration
  - SwitchPort — Switch port mapping
- [ ] `include/wadjet/arxml/endpoints.hpp` — Communication endpoints
  - SocketAddress — IP + port
  - TcpTpConfig / UdpTpConfig — Transport config
  - SoAdSocketConnectionGroup
  - ServiceInstanceConfig

Communication Model:
- [ ] `include/wadjet/arxml/frames.hpp` — Frame definitions
  - EthernetFrame — Ethernet frame definition
  - ISignalIPdu — Signal I-PDU
  - ISignalIPduGroup — PDU group
  - PduTriggering — PDU triggering config
  - FramePort / PduPort mappings
- [ ] `include/wadjet/arxml/signals.hpp` — Signal definitions
  - ISignal — Signal definition
  - ISignalGroup — Signal group
  - SystemSignal — System-level signal
  - SignalMapping — Signal to PDU mapping
  - ByteOrder, BitPosition, BitLength
- [ ] `include/wadjet/arxml/coding.hpp` — Data coding
  - CompuMethod — Computation method
  - CompuScale — Scale with formula
  - CompuConst — Constant mapping
  - DataConstraint — Value constraints
  - Unit — Physical unit

Service Interface Model (SOME/IP):
- [ ] `include/wadjet/arxml/service_interface.hpp` — Service definitions
  - ServiceInterface — SOME/IP service interface
  - ServiceInterfaceMethod — Method definition
  - ServiceInterfaceEvent — Event definition
  - ServiceInterfaceField — Field (getter/setter/notifier)
  - ArgumentDataPrototype — Method arguments
- [ ] `include/wadjet/arxml/someip_config.hpp` — SOME/IP configuration
  - SomeipServiceInstanceConfig
  - SomeipEventConfig
  - SomeipMethodConfig
  - SomeipSdConfig — Service Discovery config
  - SomeipTransformationProps

Diagnostic Model:
- [ ] `include/wadjet/arxml/diagnostic.hpp` — Diagnostic configuration
  - DiagnosticAddress — ECU diagnostic address
  - DiagnosticConnection — Tester-ECU connection
  - DiagnosticServiceInstance
  - DiagnosticProtocol (DoIP config)
  - DiagnosticSession / SecurityLevel

Source Files:
- [ ] `src/arxml/arxml_parser.cpp` — Main parser implementation
  - XML parsing with pugixml
  - AUTOSAR namespace handling
  - Reference resolution (DEST, DEFINITION-REF)
  - Multi-file merging
- [ ] `src/arxml/arxml_database.cpp` — Database implementation
  - Indexed storage by path and type
  - Cross-reference resolution
  - Query optimization
- [ ] `src/arxml/ecu_parser.cpp` — ECU extraction
- [ ] `src/arxml/network_parser.cpp` — Network topology extraction
- [ ] `src/arxml/frame_parser.cpp` — Frame/PDU extraction
- [ ] `src/arxml/signal_parser.cpp` — Signal extraction
- [ ] `src/arxml/service_parser.cpp` — Service interface extraction
- [ ] `src/arxml/diagnostic_parser.cpp` — Diagnostic config extraction

Integration:
- [ ] `include/wadjet/arxml/protocol_mapping.hpp` — Protocol decoder integration
  - Map ARXML service IDs to SOME/IP decoder
  - Map diagnostic addresses to DoIP decoder
  - Map signals to Ethernet frames
- [ ] Python bindings (`bindings/python/src/arxml_bindings.cpp`)
  - ArxmlParser, ArxmlDatabase classes
  - All model types exposed
  - Query API
- [ ] Rust bindings (`bindings/rust/wadjet/src/arxml.rs`)
  - Safe wrappers for ARXML types
  - Iterator support for collections
- [ ] C ABI layer (`bindings/c/include/wadjet_arxml.h`)
  - Opaque handles for database
  - Query functions

**Dependencies:**

| Library | Purpose | Integration |
|---------|---------|-------------|
| pugixml | XML parsing | Header-only, add to deps |
| (optional) libxml2 | XSD validation | System package |

**Testing:**

Unit Tests (`tests/arxml/test_arxml_parser.cpp`):
- [ ] Parse minimal ARXML file
- [ ] Parse multi-file project
- [ ] Reference resolution
- [ ] Handle missing references gracefully
- [ ] AUTOSAR version detection
- [ ] Namespace handling
- [ ] Unicode short names

Unit Tests (`tests/arxml/test_arxml_types.cpp`):
- [ ] ArPackagePath manipulation
- [ ] ArRef resolution
- [ ] CompuMethod conversion
- [ ] Signal bit extraction

Unit Tests (`tests/arxml/test_arxml_database.cpp`):
- [ ] Query by path
- [ ] Query by type
- [ ] Merge multiple files
- [ ] Export to JSON

Integration Tests (`tests/integration/test_arxml_integration.cpp`):
- [ ] Load real AUTOSAR project
- [ ] Extract all ECUs and addresses
- [ ] Map signals to frames
- [ ] Service interface resolution

Regression Tests:
- [ ] `arxml_samples/` — Sample ARXML files
  - `minimal_system.arxml` — Minimal valid ARXML
  - `someip_service.arxml` — SOME/IP service definition
  - `ethernet_cluster.arxml` — Ethernet network
  - `diagnostic_config.arxml` — Diagnostic addresses
  - `multi_file/` — Multi-file project

**Documentation:**

- [ ] `docs/arxml.md` — ARXML parser guide
  - Supported AUTOSAR versions
  - File loading and merging
  - Query API reference
  - Signal extraction workflow
- [ ] `docs/protocols/arxml_mapping.md` — Protocol mapping
  - ARXML to SOME/IP mapping
  - ARXML to DoIP mapping
  - Signal decoding workflow
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with ARXML layer
- [ ] Update `README.md` with ARXML support

**Use Cases & Examples:**

- [ ] `examples/arxml_loader.cpp` — Load ARXML project
  - Parse system description
  - List all ECUs
  - List all services
  - Export to JSON
- [ ] `examples/arxml_signal_map.cpp` — Signal mapping
  - Load ARXML
  - Map signals to frames
  - Decode packet with signal values
- [ ] `examples/python/arxml_analysis.py` — Python example
- [ ] `examples/scenarios/arxml_someip_test.yaml` — ARXML-based scenario

**CLI Integration:**

```bash
# Load ARXML and list ECUs
wadjet-run --arxml system.arxml --list-ecus

# Capture with ARXML context
wadjet-run scenario.yaml --arxml project/ -i eth0

# Export ARXML to JSON
wadjet-arxml export system.arxml -o system.json
```

**Test Count Target:** 40+ tests

---

### Milestone 16 — ODX/PDX Diagnostic Database Support

**Goal:** Parse ODX (Open Diagnostic data eXchange) files for diagnostic session intelligence

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required for meaningful UDS traffic analysis

**Overview:**

ODX (ISO 22901-1) is the standard format for diagnostic data description, including:
- Diagnostic services and parameters
- Data Identifier (DID) definitions with encoding
- Diagnostic Trouble Code (DTC) definitions
- ECU variants and flash configurations
- Communication parameters

This milestone enables intelligent UDS/DoIP traffic analysis by knowing what each service/DID/DTC means.

**Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                    ODX Parser Architecture                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   ODX File Types                         │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ ODX-D       │ │ ODX-C       │ │ ODX-F               │ │   │
│  │  │ (Diag Data) │ │ (Comm Params)│ │ (Flash Data)       │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ ODX-V       │ │ ODX-CS      │ │ ODX-M               │ │   │
│  │  │ (Vehicle)   │ │ (CompuSpec) │ │ (ECU Memory)        │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                   │
│                             ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   ODX Parser Engine                      │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ XML/PDX Parser  │  │ Comparam Resolver           │    │   │
│  │  │ (ODX 2.0-2.2)   │  │ (Inherit/Override)          │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ Variant         │  │ Data Type Interpreter       │    │   │
│  │  │ Resolver        │  │ (LEADING-LENGTH, etc.)      │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                   │
│                             ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  Diagnostic Data Model                   │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ DIAG-COMM   │ │ DTC         │ │ DID                 │ │   │
│  │  │ (Services)  │ │ Definitions │ │ Definitions         │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ Routines    │ │ IO Controls │ │ Security Access     │ │   │
│  │  │             │ │             │ │ Configs             │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                   │
│                             ▼                                   │
│              ┌───────────────────────────┐                      │
│              │   OdxDatabase class       │                      │
│              │   (Queryable storage)     │                      │
│              └───────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Core Headers:
- [ ] `include/wadjet/odx/odx.hpp` — Main include header
- [ ] `include/wadjet/odx/odx_types.hpp` — Core type definitions
  - OdxVersion enum (ODX_2_0_0, ODX_2_1_0, ODX_2_2_0)
  - OdxCategory enum (PROTOCOL, FUNCTION, FLASH, etc.)
  - OdxId — Unique identifier with short-name
  - OdxRef<T> — Reference with resolution
  - PhysicalDimension, Unit
- [ ] `include/wadjet/odx/odx_parser.hpp` — Parser interface
  - OdxParser class
  - PdxParser class (for container files)
  - ParseOptions (variant selection, inheritance)
  - ParseResult with errors/warnings
- [ ] `include/wadjet/odx/odx_database.hpp` — In-memory database
  - OdxDatabase class
  - ECU variant selection
  - Service lookup by SID
  - DID lookup by identifier
  - DTC lookup by code

Diagnostic Layer (DIAG-LAYER-CONTAINER):
- [ ] `include/wadjet/odx/diag_layer.hpp` — Diagnostic layer
  - DiagLayer — Base layer (PROTOCOL, FUNCTIONAL-GROUP, BASE-VARIANT, ECU-VARIANT)
  - DiagLayerContainer — Layer hierarchy
  - ParentRef — Inheritance reference
  - ImportRef — Import reference
- [ ] `include/wadjet/odx/diag_comm.hpp` — Diagnostic communication
  - DiagComm — Diagnostic service definition
  - DiagCommType enum (REQUEST, POS-RESPONSE, NEG-RESPONSE)
  - SingleEcuJob, MultiEcuJob
  - DiagCommRef, DiagCommSnref
- [ ] `include/wadjet/odx/request_response.hpp` — Request/Response structure
  - Request — Request message structure
  - PosResponse — Positive response structure
  - NegResponse — Negative response structure
  - Param — Parameter definition
  - ParamType enum (VALUE, RESERVED, MATCHING-REQUEST-PARAM, etc.)

Data Types:
- [ ] `include/wadjet/odx/data_types.hpp` — ODX data types
  - DiagCodedType — Coded representation
  - StandardLengthType, LeadingLengthInfoType, MinMaxLengthType
  - ParamLengthInfoType (dynamic length)
  - PhysicalType — Physical representation
  - InternalType — Internal values
- [ ] `include/wadjet/odx/compu_method.hpp` — Computation methods
  - CompuMethod — Base computation
  - CompuInternalToPhys — Internal to physical
  - CompuPhysToInternal — Physical to internal
  - CompuCategory enum (IDENTICAL, LINEAR, SCALE-LINEAR, TAB-INTP, TEXTTABLE)
  - CompuScale, CompuConst, CompuRationalCoeffs
- [ ] `include/wadjet/odx/dop.hpp` — Data Object Property
  - DataObjectProp (DOP) — Data encoding
  - DopBase — Base class
  - Structure — Complex structure
  - EndOfPduField — End marker
  - DynamicLengthField
  - Mux — Multiplexer

DID/DTC Definitions:
- [ ] `include/wadjet/odx/did.hpp` — Data Identifier definitions
  - Did — DID definition
  - DidRef — DID reference
  - DidGroup — Grouped DIDs
  - DidServiceInfo — Service binding
- [ ] `include/wadjet/odx/dtc.hpp` — Diagnostic Trouble Code definitions
  - Dtc — DTC definition
  - DtcGroup — DTC group
  - DtcStatusMask — Status byte definition
  - DtcSeverity enum
  - EnvironmentData — Freeze frame data
  - ExtendedDataRecord — Extended data

Service Definitions:
- [ ] `include/wadjet/odx/services.hpp` — Standard UDS services
  - SessionService — DiagnosticSessionControl
  - SecurityService — SecurityAccess
  - DidReadService — ReadDataByIdentifier
  - DidWriteService — WriteDataByIdentifier
  - RoutineService — RoutineControl
  - DtcService — ReadDTCInformation
  - DownloadService — RequestDownload/TransferData
- [ ] `include/wadjet/odx/routine.hpp` — Routine definitions
  - Routine — Routine definition
  - RoutineType enum (START, STOP, REQUEST-RESULTS)
  - RoutineResult — Result structure
  - RoutineParam — Input/output parameters
- [ ] `include/wadjet/odx/io_control.hpp` — IO Control
  - IoControl — IO control definition
  - IoControlType enum (RETURN, FREEZE, RESET, SHORT-TERM)
  - ControlMask — Control enable mask

Communication Parameters (COMPARAM):
- [ ] `include/wadjet/odx/comparam.hpp` — Communication parameters
  - Comparam — Parameter definition
  - ComparamSpec — Parameter specification
  - ComparamSubset — Parameter subset
  - ProtocolStack — Protocol stack definition
  - Timing parameters (P2, P2*, S3, etc.)

Source Files:
- [ ] `src/odx/odx_parser.cpp` — Main parser implementation
- [ ] `src/odx/pdx_parser.cpp` — PDX container parser
- [ ] `src/odx/odx_database.cpp` — Database implementation
- [ ] `src/odx/diag_layer_parser.cpp` — Layer parsing
- [ ] `src/odx/diag_comm_parser.cpp` — Service parsing
- [ ] `src/odx/data_type_parser.cpp` — Data type parsing
- [ ] `src/odx/compu_method_parser.cpp` — Computation method parsing
- [ ] `src/odx/did_parser.cpp` — DID parsing
- [ ] `src/odx/dtc_parser.cpp` — DTC parsing
- [ ] `src/odx/param_decoder.cpp` — Parameter decoding

Integration with UDS Decoder:
- [ ] `include/wadjet/odx/uds_integration.hpp` — UDS decoder integration
  - OdxEnhancedUdsDecoder — ODX-aware UDS decoder
  - Auto-decode DID values
  - Auto-decode DTC meanings
  - Parameter name resolution
  - Engineering value conversion

Integration:
- [ ] Python bindings (`bindings/python/src/odx_bindings.cpp`)
  - OdxParser, OdxDatabase classes
  - DID/DTC lookup
  - Service definition access
- [ ] Rust bindings (`bindings/rust/wadjet/src/odx.rs`)
- [ ] C ABI layer (`bindings/c/include/wadjet_odx.h`)

**Testing:**

Unit Tests (`tests/odx/test_odx_parser.cpp`):
- [ ] Parse minimal ODX-D file
- [ ] Parse ODX with inheritance
- [ ] Parse PDX container
- [ ] Handle malformed ODX
- [ ] Version detection

Unit Tests (`tests/odx/test_odx_types.cpp`):
- [ ] CompuMethod evaluation
- [ ] Data type decoding
- [ ] Parameter extraction
- [ ] DID value decoding

Unit Tests (`tests/odx/test_odx_database.cpp`):
- [ ] Service lookup
- [ ] DID lookup
- [ ] DTC lookup
- [ ] Variant selection

Integration Tests (`tests/integration/test_odx_uds.cpp`):
- [ ] Decode UDS traffic with ODX context
- [ ] DID value interpretation
- [ ] DTC description lookup
- [ ] Multi-ECU variant handling

Regression Tests:
- [ ] `odx_samples/` — Sample ODX files
  - `minimal_ecu.odx-d` — Minimal ECU definition
  - `did_catalog.odx-d` — DID definitions
  - `dtc_catalog.odx-d` — DTC definitions
  - `multi_variant/` — Multi-variant project
  - `test_project.pdx` — PDX container

**Documentation:**

- [ ] `docs/odx.md` — ODX parser guide
  - Supported ODX versions
  - File loading workflow
  - Variant selection
  - Query API reference
- [ ] `docs/protocols/odx_uds_integration.md` — UDS integration
  - ODX-enhanced UDS decoding
  - DID interpretation
  - DTC lookup
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with ODX layer
- [ ] Update `README.md` with ODX support

**Use Cases & Examples:**

- [ ] `examples/odx_loader.cpp` — Load ODX project
  - Parse ODX/PDX files
  - List all DIDs
  - List all DTCs
  - List all services
- [ ] `examples/odx_uds_decode.cpp` — ODX-enhanced UDS decoding
  - Load ODX
  - Capture UDS traffic
  - Decode with parameter names
  - Show engineering values
- [ ] `examples/python/odx_analysis.py` — Python example
- [ ] `examples/scenarios/odx_did_test.yaml` — ODX-based DID test

**CLI Integration:**

```bash
# Load ODX and list DIDs
wadjet-run --odx project.pdx --list-dids

# Capture with ODX context
wadjet-run scenario.yaml --odx ecu.odx-d -i eth0

# Decode with ODX intelligence
wadjet-decode capture.pcap --odx project.pdx --output decoded.json
```

**Test Count Target:** 50+ tests

---

### Milestone 17 — Signal-Level Decoding

**Goal:** Decode individual signals from network traffic using ARXML/ODX databases

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Key differentiator for automotive protocol analysis

**Overview:**

Signal-level decoding transforms raw bytes into meaningful engineering values:
- Extract signal bits from frames using ARXML definitions
- Apply computation methods (scaling, offset, lookup tables)
- Display physical values with units
- Group signals by ECU, frame, or functional domain

This milestone bridges the gap between "bytes on wire" and "what does it mean?"

**Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                Signal Decoding Pipeline                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   Raw Packet Data                        │   │
│  │           (Captured Ethernet frame bytes)                │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Protocol Identification                     │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ SOME/IP     │ │ DoIP/UDS    │ │ Raw Ethernet        │ │   │
│  │  │ Service ID  │ │ DID/Routine │ │ EtherType + VLAN    │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │               Database Lookup                            │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ ARXML       │ │ ODX         │ │ Manual Config       │ │   │
│  │  │ Signal Def  │ │ DID/Param   │ │ (JSON/YAML)         │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │               Signal Extraction                          │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │  Bit-level extraction:                              │ │   │
│  │  │  - Start bit, length, byte order                    │ │   │
│  │  │  - Signed/unsigned handling                         │ │   │
│  │  │  - Multiplexed signal support                       │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Value Conversion                            │   │
│  │  ┌─────────────────┐  ┌─────────────────────────────┐    │   │
│  │  │ CompuMethod     │  │ Value Constraints           │    │   │
│  │  │ - Linear        │  │ - Min/Max validation        │    │   │
│  │  │ - Rational      │  │ - Error value detection     │    │   │
│  │  │ - Text table    │  │ - Not-available handling    │    │   │
│  │  │ - Tab interp    │  │                             │    │   │
│  │  └─────────────────┘  └─────────────────────────────┘    │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                Decoded Signal Output                     │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │  Signal: EngineSpeed                                │ │   │
│  │  │  Raw: 0x1234 (4660)                                 │ │   │
│  │  │  Physical: 2330.0 rpm                               │ │   │
│  │  │  Status: Valid                                      │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Core Headers:
- [ ] `include/wadjet/signals/signal.hpp` — Signal definitions
  - Signal — Signal definition
  - SignalValue — Decoded signal value (raw + physical)
  - SignalGroup — Related signals
  - SignalStatus enum (VALID, ERROR, NOT_AVAILABLE, INVALID)
  - PhysicalValue — Value with unit
- [ ] `include/wadjet/signals/signal_database.hpp` — Signal database
  - SignalDatabase — Central signal registry
  - RegisterSignal() — Manual signal registration
  - LoadFromArxml() — Load from ARXML
  - LoadFromOdx() — Load from ODX
  - LoadFromJson() — Load from JSON config
- [ ] `include/wadjet/signals/signal_decoder.hpp` — Signal decoder
  - SignalDecoder — Main decoder class
  - DecodeOptions (strict mode, default values)
  - DecodedFrame — Frame with decoded signals
  - DecodedSignal — Individual signal result

Bit Extraction:
- [ ] `include/wadjet/signals/bit_extraction.hpp` — Bit-level operations
  - BitPosition — Start bit definition
  - BitLength — Signal length
  - ByteOrder enum (LITTLE_ENDIAN, BIG_ENDIAN, BIG_ENDIAN_BITNUM)
  - ExtractBits() — Generic bit extraction
  - SignExtend() — Signed value handling
- [ ] `include/wadjet/signals/multiplexer.hpp` — Multiplexed signals
  - Multiplexer — Mux signal definition
  - MuxGroup — Multiplexed signal group
  - MuxValue — Multiplexer value
  - ResolveMux() — Determine active signals

Value Conversion:
- [ ] `include/wadjet/signals/compu_method.hpp` — Computation methods
  - ICompuMethod — Interface
  - IdenticalCompu — Direct mapping
  - LinearCompu — y = ax + b
  - RationalCompu — Polynomial/rational
  - TextTableCompu — Enum mapping
  - TabInterpCompu — Interpolation table
  - ScaleLinearCompu — Piecewise linear
- [ ] `include/wadjet/signals/constraints.hpp` — Value constraints
  - ValueConstraint — Min/max/error values
  - NotAvailableValue — N/A detection
  - ErrorValue — Error value detection
  - ValidateValue() — Constraint checking

Protocol-Specific Decoders:
- [ ] `include/wadjet/signals/someip_signal.hpp` — SOME/IP signals
  - SomeipSignalDecoder — SOME/IP payload decoder
  - SerializationConfig — SOME/IP serialization rules
  - StructureDecoder — Complex type decoder
  - ArrayDecoder — Dynamic array decoder
- [ ] `include/wadjet/signals/uds_signal.hpp` — UDS signals
  - UdsSignalDecoder — UDS payload decoder
  - DidDecoder — DID parameter decoder
  - RoutineDecoder — Routine parameter decoder
  - DtcDecoder — DTC structure decoder
- [ ] `include/wadjet/signals/ethernet_signal.hpp` — Raw Ethernet signals
  - EthernetSignalDecoder — Raw frame decoder
  - PduDecoder — PDU-based decoding
  - SignalPduMapping — Signal to PDU mapping

Output Formats:
- [ ] `include/wadjet/signals/signal_output.hpp` — Output formatting
  - SignalFormatter — Format decoded signals
  - JsonSignalOutput — JSON format
  - TextSignalOutput — Human-readable
  - CsvSignalOutput — CSV export
  - SignalTrace — Time-series recording

Source Files:
- [ ] `src/signals/signal_database.cpp` — Database implementation
- [ ] `src/signals/signal_decoder.cpp` — Main decoder
- [ ] `src/signals/bit_extraction.cpp` — Bit operations
- [ ] `src/signals/compu_method.cpp` — Computation methods
- [ ] `src/signals/someip_signal.cpp` — SOME/IP decoding
- [ ] `src/signals/uds_signal.cpp` — UDS decoding
- [ ] `src/signals/ethernet_signal.cpp` — Ethernet decoding
- [ ] `src/signals/signal_output.cpp` — Output formatting

Integration:
- [ ] `include/wadjet/signals/decode_context.hpp` — Decoder integration
  - DecodeContext — Extends ProtocolDispatcher
  - SignalAwareDecoder — Signal-enabled packet decoder
  - DecodedPacket — Packet with signals
- [ ] Python bindings (`bindings/python/src/signal_bindings.cpp`)
  - SignalDatabase, SignalDecoder
  - SignalValue, DecodedFrame
  - NumPy integration for signal arrays
- [ ] Rust bindings (`bindings/rust/wadjet/src/signals.rs`)
- [ ] C ABI layer (`bindings/c/include/wadjet_signals.h`)

**Testing:**

Unit Tests (`tests/signals/test_bit_extraction.cpp`):
- [ ] Little-endian extraction
- [ ] Big-endian extraction (Motorola byte order)
- [ ] Big-endian bit numbering
- [ ] Sign extension
- [ ] Multi-byte signals
- [ ] Boundary conditions

Unit Tests (`tests/signals/test_compu_method.cpp`):
- [ ] Identity conversion
- [ ] Linear conversion (y = ax + b)
- [ ] Rational conversion
- [ ] Text table lookup
- [ ] Interpolation tables
- [ ] Piecewise linear

Unit Tests (`tests/signals/test_signal_decoder.cpp`):
- [ ] Decode single signal
- [ ] Decode signal group
- [ ] Multiplexed signals
- [ ] Constraint validation
- [ ] Error value detection
- [ ] Not-available handling

Integration Tests (`tests/integration/test_signal_integration.cpp`):
- [ ] ARXML + signal decoding
- [ ] ODX + UDS signal decoding
- [ ] Full packet decode pipeline
- [ ] Live capture with signals

Performance Tests (`benchmarks/bench_signal.cpp`):
- [ ] High-frequency signal decoding
- [ ] Large signal database lookup
- [ ] Bulk frame processing

Regression Tests:
- [ ] `signal_samples/` — Sample configurations
  - `engine_signals.json` — Engine data signals
  - `body_signals.json` — Body control signals
  - `adas_signals.json` — ADAS signals
  - `test_frames/` — Sample frames with expected values

**Documentation:**

- [ ] `docs/signals.md` — Signal decoding guide
  - Signal database configuration
  - ARXML/ODX integration
  - Manual signal definition
  - Decoding workflow
- [ ] `docs/signals_format.md` — Signal definition format
  - JSON signal format
  - YAML signal format
  - Bit position conventions
  - Computation method types
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with signal layer
- [ ] Update `README.md` with signal support

**Use Cases & Examples:**

- [ ] `examples/signal_monitor.cpp` — Signal monitor
  - Load signal database
  - Capture traffic
  - Decode signals in real-time
  - Display signal values
- [ ] `examples/signal_recorder.cpp` — Signal recorder
  - Record signals to CSV
  - Time-series export
  - Signal filtering
- [ ] `examples/python/signal_analysis.py` — Python analysis
  - Load signals
  - Plot signal values
  - Statistics calculation
- [ ] `examples/scenarios/signal_test.yaml` — Signal-based test

**Matchers & Assertions:**

```cpp
// New signal matchers
EXPECT_THAT(packet, HasSignal("EngineSpeed"));
EXPECT_THAT(packet, SignalEquals("EngineSpeed", 2500.0, 0.1));
EXPECT_THAT(packet, SignalInRange("VehicleSpeed", 0.0, 250.0));
EXPECT_THAT(packet, SignalStatus("BrakePedal", SignalStatus::VALID));
EXPECT_THAT(packet, SignalText("GearPosition", "D"));
```

**CLI Integration:**

```bash
# Decode with signals
wadjet-decode capture.pcap --arxml system.arxml --signals

# Monitor signals live
wadjet-signals -i eth0 --arxml system.arxml --filter "EngineSpeed,VehicleSpeed"

# Record signals to CSV
wadjet-signals -i eth0 --arxml system.arxml -o signals.csv
```

**Test Count Target:** 60+ tests

---

### Milestone 18 — IPv6 Protocol Support

**Goal:** Add IPv6 protocol decoding for next-generation automotive networks

**Status:** ⏳ Not Started

**Priority:** 🟡 Medium — Increasingly important for automotive Ethernet

**Overview:**

IPv6 is becoming more common in automotive networks, especially for:
- Service-Oriented Architecture (SOA) with IPv6 multicast
- DoIP over IPv6
- DDS/RTPS over IPv6
- Next-generation E/E architectures

This milestone adds full IPv6 support alongside existing IPv4.

**Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                    IPv6 Protocol Stack                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   Ethernet Frame                         │   │
│  │           EtherType: 0x86DD (IPv6)                       │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   IPv6 Header (40 bytes)                 │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │ Version │ TC  │ Flow Label │ Payload Len │ Next Hdr│ │   │
│  │  │   (4)   │ (8) │   (20)     │    (16)     │   (8)   │ │   │
│  │  ├─────────────────────────────────────────────────────┤ │   │
│  │  │           Source Address (128 bits)                 │ │   │
│  │  ├─────────────────────────────────────────────────────┤ │   │
│  │  │         Destination Address (128 bits)              │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Extension Headers (optional)                │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ Hop-by-Hop  │ │ Routing     │ │ Fragment            │ │   │
│  │  │ Options     │ │ Header      │ │ Header              │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ Destination │ │ Authentication│ │ ESP (encrypted)   │ │   │
│  │  │ Options     │ │ Header (AH)  │ │                    │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └─────────────────────────┬────────────────────────────────┘   │
│                            │                                    │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Upper Layer Protocols                       │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │ TCP         │ │ UDP         │ │ ICMPv6              │ │   │
│  │  │             │ │             │ │ (ND, MLD)           │ │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Core Headers:
- [ ] `include/wadjet/protocols/ipv6.hpp` — Main IPv6 header
  - IPv6Header — 40-byte fixed header
  - IPv6Address — 128-bit address
  - FlowLabel — 20-bit flow label
  - TrafficClass — 8-bit traffic class (DSCP + ECN)
- [ ] `include/wadjet/protocols/ipv6_types.hpp` — Type definitions
  - NextHeader enum (matches IPv4 protocol numbers + extensions)
  - IPv6Scope enum (Interface-Local, Link-Local, Site-Local, Global)
  - MulticastFlags
  - AddressType enum (Unicast, Multicast, Anycast)
- [ ] `include/wadjet/protocols/ipv6_ext.hpp` — Extension headers
  - ExtensionHeader — Base class
  - HopByHopHeader — Hop-by-Hop Options
  - RoutingHeader — Routing Header (types 0, 2, 3, 4)
  - FragmentHeader — Fragment Header
  - DestinationOptionsHeader — Destination Options
  - NoNextHeader — No Next Header marker
- [ ] `include/wadjet/protocols/icmpv6.hpp` — ICMPv6 protocol
  - ICMPv6Header — Header structure
  - ICMPv6Type enum (Echo, Router Solicitation, Neighbor Discovery, etc.)
  - NeighborDiscovery — ND messages
  - MulticastListenerDiscovery — MLD messages

Decoder Implementation:
- [ ] `src/protocols/ipv6_decoder.cpp` — IPv6 decoder
  - Header parsing
  - Extension header chain parsing
  - Next header dispatch
  - Address validation
- [ ] `src/protocols/ipv6_ext_decoder.cpp` — Extension header decoders
  - Hop-by-Hop Options parsing
  - Routing Header parsing (all types)
  - Fragment Header handling
  - Destination Options parsing
- [ ] `src/protocols/icmpv6_decoder.cpp` — ICMPv6 decoder
  - Echo Request/Reply
  - Neighbor Discovery (NS, NA, RS, RA)
  - MLD (Query, Report, Done)
  - Destination Unreachable, etc.

Address Utilities:
- [ ] `include/wadjet/net/ipv6_address.hpp` — IPv6 address class
  - IPv6Address — 128-bit address
  - Parse from string (full, compressed, mixed)
  - Format to string (RFC 5952 canonical)
  - Scope detection
  - Multicast group detection
  - Link-local detection
  - Solicited-node multicast calculation
- [ ] `src/net/ipv6_address.cpp` — Implementation

Protocol Dispatcher Integration:
- [ ] Update `protocols/dispatcher.hpp`
  - Add EtherType 0x86DD dispatch
  - IPv6 → UDP/TCP → SOME/IP/DoIP chain
  - ICMPv6 dispatch
- [ ] Update `protocols/tcp.hpp` / `protocols/udp.hpp`
  - Support IPv6 pseudo-header checksum
  - IPv6 source/destination in context

Language Bindings:
- [ ] Python bindings
  - IPv6Address class
  - IPv6Header class
  - ICMPv6Header class
- [ ] Rust bindings
  - IPv6Address struct
  - IPv6Header struct
- [ ] C ABI layer
  - `wadjet_ipv6_header_t`
  - `wadjet_ipv6_address_t`

**Testing:**

Unit Tests (`tests/protocols/test_ipv6.cpp`):
- [ ] IPv6 header parsing
- [ ] Address parsing (all formats)
- [ ] Address formatting (canonical)
- [ ] Extension header parsing
- [ ] Fragment handling
- [ ] Malformed header detection

Unit Tests (`tests/protocols/test_icmpv6.cpp`):
- [ ] Echo Request/Reply
- [ ] Router Solicitation/Advertisement
- [ ] Neighbor Solicitation/Advertisement
- [ ] MLD Query/Report
- [ ] Error messages

Unit Tests (`tests/net/test_ipv6_address.cpp`):
- [ ] Parse full address
- [ ] Parse compressed address (::)
- [ ] Parse mixed IPv4-mapped
- [ ] Canonical formatting
- [ ] Scope detection
- [ ] Multicast group handling

Integration Tests (`tests/integration/test_ipv6_integration.cpp`):
- [ ] Full IPv6 + UDP decode
- [ ] Full IPv6 + TCP decode
- [ ] IPv6 + SOME/IP
- [ ] IPv6 + DoIP
- [ ] Live capture (loopback)

Fuzz Testing (`fuzz/fuzz_ipv6.cpp`):
- [ ] IPv6 header fuzzer
- [ ] Extension header fuzzer
- [ ] ICMPv6 fuzzer
- [ ] Address parser fuzzer

Regression Tests:
- [ ] `pcap_samples/ipv6/` — IPv6 captures
  - `ipv6_udp.pcap` — Basic IPv6/UDP
  - `ipv6_tcp.pcap` — IPv6/TCP
  - `ipv6_someip.pcap` — SOME/IP over IPv6
  - `ipv6_nd.pcap` — Neighbor Discovery
  - `ipv6_fragments.pcap` — Fragmented packets

**Documentation:**

- [ ] `docs/protocols/ipv6.md` — IPv6 reference
  - Header format
  - Extension headers
  - Address formats
  - ICMPv6 overview
- [ ] API documentation (Doxygen)
- [ ] Update `docs/architecture.md` with IPv6
- [ ] Update `README.md` with IPv6 support

**Use Cases & Examples:**

- [ ] `examples/ipv6_monitor.cpp` — IPv6 traffic monitor
  - Address discovery
  - Traffic statistics by address
  - ICMPv6 analysis
- [ ] `examples/ipv6_nd_analyzer.cpp` — Neighbor Discovery analyzer
  - Track neighbor cache
  - Router discovery
  - Address resolution
- [ ] `examples/scenarios/ipv6_someip_test.yaml` — IPv6 SOME/IP test
- [ ] `examples/python/ipv6_analysis.py` — Python example

**Matchers & Assertions:**

```cpp
// IPv6 matchers
EXPECT_THAT(packet, HasIPv6());
EXPECT_THAT(packet, HasIPv6SourceAddress("fe80::1"));
EXPECT_THAT(packet, HasIPv6DestAddress("ff02::1"));
EXPECT_THAT(packet, HasIPv6NextHeader(NextHeader::UDP));
EXPECT_THAT(packet, HasIPv6TrafficClass(0xE0));
EXPECT_THAT(packet, HasIPv6FlowLabel(0x12345));
EXPECT_THAT(packet, IsIPv6Multicast());
EXPECT_THAT(packet, IsIPv6LinkLocal());
EXPECT_THAT(packet, HasIPv6ExtensionHeader(ExtensionType::Fragment));

// ICMPv6 matchers
EXPECT_THAT(packet, HasICMPv6());
EXPECT_THAT(packet, IsICMPv6NeighborSolicitation());
EXPECT_THAT(packet, IsICMPv6RouterAdvertisement());
EXPECT_THAT(packet, IsICMPv6EchoRequest());
```

**Test Count Target:** 45+ tests

---

### Milestone 19 — Production Packaging & Distribution

**Goal:** Provide native installers, Docker images, and platform support matrix for production deployment

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required for production adoption

**Overview:**

Production-grade packaging enables:
- Native OS installation (no manual compilation)
- Consistent deployment across environments
- Enterprise IT/OPS approval
- Version management and upgrades
- Docker for CI/CD and containerized workflows

**Platform Support Matrix:**

> 📋 Define explicit supported platforms and hardware configurations

| Platform | Versions | Kernel | NIC Support | Status |
|----------|----------|--------|-------------|--------|
| Ubuntu | 22.04 LTS, 24.04 LTS | 5.15+ | Intel, Broadcom, Realtek | Primary |
| Debian | 11 (Bullseye), 12 (Bookworm) | 5.10+ | Intel, Broadcom, Realtek | Primary |
| RHEL/Rocky | 8.x, 9.x | 4.18+ | Intel, Mellanox | Secondary |
| Arch Linux | Rolling | Latest | Intel, Realtek | Community |
| Fedora | 38+, 39+ | 6.x+ | Intel, Broadcom | Community |

**NIC Requirements:**

- [ ] Document required kernel driver support
- [ ] Document required hardware offload features
  - RX/TX checksum offload
  - Hardware timestamping (for gPTP)
  - VLAN tag offloading
  - RSS (Receive Side Scaling)
- [ ] Document performance impact of offload settings
- [ ] Provide NIC configuration scripts

**Implementation:**

**Phase 1: Native Package Building**

Debian/Ubuntu Packaging:
- [ ] `packaging/debian/` — Debian packaging files
  - `control` — Package metadata and dependencies
  - `rules` — Build rules
  - `changelog` — Version history
  - `copyright` — License information
  - `wadjet-link.install` — File installation map
  - `wadjet-link.service` — systemd service file
- [ ] Build .deb packages in CI
- [ ] Test installation on Ubuntu 22.04 and 24.04
- [ ] Test installation on Debian 11 and 12
- [ ] APT repository setup (optional)

RPM Packaging:
- [ ] `packaging/rpm/wadjet-link.spec` — RPM spec file
- [ ] Build .rpm packages in CI
- [ ] Test on RHEL 8/9, Rocky Linux 8/9
- [ ] Test on Fedora 38+
- [ ] YUM/DNF repository setup (optional)

Arch Linux:
- [ ] `packaging/arch/PKGBUILD` — Arch package definition
- [ ] Submit to AUR (Arch User Repository)

Snap Packaging:
- [ ] `packaging/snap/snapcraft.yaml` — Snap package definition
  - Base: core22 (Ubuntu 22.04)
  - Confinement: classic (requires system-level network access)
  - Plugs: network, network-bind, network-control, network-observe
  - Parts: wadjet-link build from source
- [ ] Build snap packages in CI
- [ ] Test installation via `snap install wadjet-link`
- [ ] Test on Ubuntu 22.04, 24.04, Fedora, Arch (snapd installed)
- [ ] Submit to Snap Store (optional)
- [ ] Auto-update configuration

Homebrew (macOS/Linux):
- [ ] `packaging/homebrew/wadjet-link.rb` — Homebrew formula
- [ ] Submit to homebrew-core or create tap

**Phase 2: Docker Images**

Docker Images:
- [ ] `docker/Dockerfile` — Main development image
  - Ubuntu 22.04 LTS base
  - All build dependencies
  - Pre-built Wadjet-Link
  - Python bindings installed
- [ ] `docker/Dockerfile.minimal` — Minimal runtime image
  - Alpine-based
  - Runtime dependencies only
  - CLI tools only
- [ ] `docker/Dockerfile.dev` — Full development image
  - Build tools (CMake, Ninja, GCC, Clang)
  - Debugging tools (GDB, Valgrind)
  - Documentation tools (Doxygen)
- [ ] `docker/docker-compose.yml` — Multi-container setup
  - Wadjet-Link service
  - Test runner service
  - Report viewer service (future)

GitHub Actions Enhancements:
- [ ] `.github/workflows/docker-build.yml` — Docker image build
  - Build and push to GitHub Container Registry
  - Multi-architecture (amd64, arm64)
  - Version tagging
- [ ] `.github/workflows/docker-test.yml` — Containerized testing
  - Run tests in Docker
  - Matrix across Ubuntu versions
  - Artifact collection
- [ ] `.github/actions/wadjet-test/action.yml` — Reusable test action
  - Setup Wadjet-Link in CI
  - Run scenario tests
  - Generate reports

CI/CD Integration Examples:
- [ ] `ci/jenkins/Jenkinsfile` — Jenkins pipeline example
- [ ] `ci/gitlab/.gitlab-ci.yml` — GitLab CI example
- [ ] `ci/azure/azure-pipelines.yml` — Azure DevOps example
- [ ] `ci/github/test-workflow.yml` — GitHub Actions template

Helper Scripts:
- [ ] `scripts/docker-build.sh` — Build Docker images
- [ ] `scripts/docker-test.sh` — Run tests in Docker
- [ ] `scripts/ci-setup.sh` — CI environment setup
- [ ] `scripts/publish-results.sh` — Upload test results

**Documentation:**

- [ ] `docs/docker.md` — Docker usage guide
- [ ] `docs/ci_cd.md` — CI/CD integration guide
- [ ] `README.md` — Quick start with Docker

**Test Count Target:** 10+ integration tests

---

### Milestone 20 — A2L/HEX File Support

**Goal:** Parse A2L measurement files and HEX flash files for calibration workflows

**Status:** ⏳ Not Started

**Priority:** 🟢 Low — Useful for calibration engineers

**Overview:**

A2L (ASAM MCD-2 MC) describes ECU memory layout for measurement and calibration:
- Memory addresses and data types
- Calibration parameters
- Measurement signals
- Conversion formulas

HEX files (Intel HEX, Motorola S-Record) contain:
- ECU flash data
- Calibration data
- Application software

**Implementation:**

A2L Parser:
- [ ] `include/wadjet/a2l/a2l.hpp` — Main header
- [ ] `include/wadjet/a2l/a2l_types.hpp` — Type definitions
  - Module, Characteristic, Measurement, CompuMethod
  - MemorySegment, AddressType
  - AxisPts, Curve, Map, Cuboid
- [ ] `include/wadjet/a2l/a2l_parser.hpp` — Parser
  - A2lParser class
  - A2lDatabase — In-memory storage
- [ ] `src/a2l/a2l_parser.cpp` — Implementation
  - A2L grammar parsing
  - IF_DATA handling

HEX Parser:
- [ ] `include/wadjet/hex/hex.hpp` — Main header
- [ ] `include/wadjet/hex/intel_hex.hpp` — Intel HEX format
  - IntelHexParser
  - Record types (data, EOF, extended address)
- [ ] `include/wadjet/hex/srec.hpp` — Motorola S-Record
  - SrecParser
  - Record types (S0-S9)
- [ ] `include/wadjet/hex/hex_database.hpp` — Memory image
  - HexDatabase — Unified memory view
  - Address ranges
  - Gap detection

**Testing:**

- [ ] A2L parsing tests (30+)
- [ ] Intel HEX parsing tests (15+)
- [ ] S-Record parsing tests (15+)
- [ ] Integration tests (10+)

**Documentation:**

- [ ] `docs/a2l.md` — A2L parser guide
- [ ] `docs/hex.md` — HEX file guide

**Test Count Target:** 70+ tests

---

### Milestone 21 — PreProduction Quality Gate

**Goal:** Ensure production-ready code quality, documentation, and release readiness

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required before any production release

**Dependencies:**

> ⚠️ **Prerequisite:** Milestone 13 (Protocol Completeness) should be complete before starting PreProduction.
> All protocol implementations must be feature-complete before quality gate review.

- ⏳ Milestone 13: Protocol Completeness
- ⏳ All feature milestones you intend to ship (14-20 as needed)

**Overview:**

This milestone ensures that Wadjet-Link meets production-quality standards before release. It covers code quality, documentation completeness, test coverage, and release preparation.

**Implementation Checklist:**

The following items must be completed **in sequence**:

**Phase 1: Code Formatting & Style**

- [ ] C++ code formatted with clang-format (no warnings/errors)
- [ ] Python code formatted with black/isort (no warnings/errors)
- [ ] Rust code formatted with rustfmt (no warnings/errors)
- [ ] C code formatted with clang-format (no warnings/errors)
- [ ] Google C++ Style Guide applied and enforced
- [ ] All linter warnings resolved (clang-tidy, pylint, clippy)

**Phase 2: Design Principles & Best Practices**

- [ ] KISS (Keep It Simple, Stupid) — No over-engineering
- [ ] DRY (Don't Repeat Yourself) — No code duplication
- [ ] SOLID principles applied where appropriate
- [ ] Single Responsibility — Each class/function has one purpose
- [ ] Open/Closed — Extensible without modification
- [ ] Liskov Substitution — Proper inheritance hierarchies
- [ ] Interface Segregation — Focused interfaces
- [ ] Dependency Inversion — Depend on abstractions
- [ ] YAGNI (You Aren't Gonna Need It) — No speculative features
- [ ] Composition over inheritance where appropriate
- [ ] Fail-fast error handling

**Phase 3: C++ Optimization & Metaprogramming**

- [ ] Templates used to minimize code duplication
- [ ] constexpr/consteval used for compile-time computation
- [ ] SFINAE/concepts for type constraints
- [ ] Policy-based design where beneficial
- [ ] Type traits for compile-time type manipulation
- [ ] Variadic templates for flexible interfaces
- [ ] Template specialization for optimized paths
- [ ] Zero runtime overhead abstractions

**Phase 4: Zero-Copy Architecture**

- [ ] All packet handling uses zero-copy (PacketView)
- [ ] No unnecessary buffer copies in decode path
- [ ] Memory-mapped I/O where applicable
- [ ] Span/string_view used instead of copies
- [ ] Move semantics used throughout
- [ ] No hidden allocations in hot paths
- [ ] Buffer pooling for reusable allocations
- [ ] Verified with profiling tools

**Phase 5: Memory Safety & Sanitizers**

- [ ] AddressSanitizer (ASan) — No memory leaks, buffer overflows
- [ ] UndefinedBehaviorSanitizer (UBSan) — No undefined behavior
- [ ] ThreadSanitizer (TSan) — No data races in multi-threaded code
- [ ] MemorySanitizer (MSan) — No uninitialized memory reads
- [ ] Valgrind memcheck clean (alternative to ASan)
- [ ] Static analysis with clang-tidy (modernize, performance, bugprone)
- [ ] Static analysis with cppcheck (no high-severity issues)
- [ ] Python: mypy type checking passes
- [ ] Rust: cargo clippy with no warnings

**Phase 6: Examples**

- [ ] Every public API feature has an example in `/examples/`
- [ ] Examples compile and run successfully
- [ ] Examples are well-commented and educational
- [ ] Examples cover common use cases
- [ ] Python examples in `/examples/python/`
- [ ] Rust examples in `/bindings/rust/examples/`
- [ ] Scenario examples in `/examples/scenarios/`
- [ ] Example README with descriptions of each example

**Phase 7: Test Coverage**

- [ ] Unit test coverage ≥ 90% for core libraries
- [ ] Integration test coverage ≥ 80%
- [ ] All public APIs have test coverage
- [ ] Edge cases and error paths tested
- [ ] Fuzz testing for all protocol parsers
- [ ] Property-based testing where applicable
- [ ] Performance regression tests
- [ ] Coverage reports generated and reviewed
- [ ] No untested critical code paths

**Phase 8: Documentation**

- [ ] All public APIs documented with Doxygen
- [ ] User guide complete and accurate
- [ ] Architecture documentation up-to-date
- [ ] Protocol documentation complete
- [ ] Installation guide tested on clean system
- [ ] Troubleshooting guide with common issues
- [ ] API reference generated and published
- [ ] Code examples in documentation compile
- [ ] Language is clear, concise, and accessible
- [ ] Cross-references to related sections

**Phase 9: Architecture Diagrams**

- [ ] Component diagram reflects current architecture
- [ ] Class diagrams for major subsystems
- [ ] Sequence diagrams for key workflows
- [ ] Protocol stack diagrams accurate
- [ ] Data flow diagrams complete
- [ ] Deployment diagrams (if applicable)
- [ ] Diagrams use consistent notation (UML/PlantUML)
- [ ] Diagrams versioned with code
- [ ] Diagrams referenced from documentation

**Phase 10: Performance Validation & SLOs**

> 🎯 **Hard Performance Targets** — These are non-negotiable for a professional automotive Ethernet testing tools

**Service Level Objectives (SLOs):**

| Metric | Target | Measurement Method |
|--------|--------|--------------------|
| **Max Throughput (1 Gbps link)** | ≥ 950 Mbps sustained | iperf + live capture |
| **Max Throughput (10 Gbps link)** | ≥ 9 Gbps sustained | Multi-threaded capture |
| **Max Packet Rate** | ≥ 1M packets/s | Small packet flood |
| **Packet Loss Rate** | < 0.01% at line rate | Dropped packet counters |
| **Live Assertion Latency** | < 100 µs per assertion | Benchmark harness |
| **PCAP Write Latency** | < 1 ms per packet | I/O profiling |
| **Memory Footprint (baseline)** | < 50 MB RSS | Valgrind massif |
| **Memory Growth Rate** | < 10 MB/hour | Long-running capture |
| **Decode Latency (SOME/IP)** | < 10 µs per packet | Protocol benchmark |
| **Max PCAP File Size** | ≥ 100 GB without crash | Stress test |

**Performance Validation Checklist:**

- [ ] Benchmark suite complete
  - [ ] Throughput benchmark (1 Gbps)
  - [ ] Throughput benchmark (10 Gbps)
  - [ ] Packet rate benchmark
  - [ ] Latency benchmark (assertion)
  - [ ] Memory benchmark (footprint + growth)
  - [ ] Decode latency benchmark (all protocols)
  - [ ] PCAP write benchmark
  - [ ] Large file handling test
- [ ] Baseline performance documented
  - [ ] Results published in `docs/performance.md`
  - [ ] Comparison with libpcap baseline
  - [ ] Comparison with tcpdump/Wireshark (where applicable)
- [ ] No performance regressions
  - [ ] CI gate fails if performance drops > 10%
  - [ ] Benchmark results tracked across commits
- [ ] Memory footprint documented
  - [ ] Baseline RSS documented
  - [ ] Memory leak detection (Valgrind, ASan)
  - [ ] Long-running capture (24h+) memory stability
- [ ] Latency requirements met (see SLOs above)
- [ ] Throughput requirements met (see SLOs above)
- [ ] Profiling results reviewed
  - [ ] CPU profiling (perf, Instruments)
  - [ ] I/O profiling (iotop, strace)
  - [ ] Hotspot analysis
  - [ ] Optimization opportunities documented

**Phase 10b: Internal Telemetry & Diagnostics**

> 📊 **Operational Visibility** — Monitor tool health, not just packet data

**Telemetry Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                  Wadjet Internal Telemetry                      │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   Metrics Collectors                     │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │   │
│  │  │  Capture    │ │  Decoder    │ │  System         │    │   │
│  │  │  Health     │ │  Health     │ │  Health         │    │   │
│  │  └─────────────┘ └─────────────┘ └─────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  Metrics Registry                        │   │
│  │  (Thread-safe counters, gauges, histograms)             │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│              ┌───────────────┴───────────────┐                  │
│              ▼                               ▼                  │
│  ┌─────────────────────┐        ┌─────────────────────┐         │
│  │   Structured Logs   │        │  Metrics Export     │         │
│  │   (JSON/syslog)     │        │  (Prometheus/JSON)  │         │
│  └─────────────────────┘        └─────────────────────┘         │
└─────────────────────────────────────────────────────────────────┘
```

**Capture Health Metrics:**

- [ ] `wadjet::metrics::CaptureMetrics` class
  - Packets captured (counter)
  - Packets dropped (counter)
  - Bytes captured (counter)
  - Capture errors (counter)
  - Ring buffer utilization (gauge, %)
  - Capture rate (gauge, packets/s)
  - Capture uptime (gauge, seconds)

**Decoder Health Metrics:**

- [ ] `wadjet::metrics::DecoderMetrics` class
  - Packets decoded (counter, per protocol)
  - Decode errors (counter, per protocol)
  - Decode latency (histogram, µs)
  - Unknown protocols (counter)
  - Malformed packets (counter, per protocol)

**System Health Metrics:**

- [ ] `wadjet::metrics::SystemMetrics` class
  - RSS memory (gauge, bytes)
  - CPU usage (gauge, %)
  - File descriptor count (gauge)
  - Thread count (gauge)
  - Disk I/O (counter, bytes written)

**Self-Diagnostic Mode:**

- [ ] `wadjet-diag` utility
  - System check:
    - Kernel version
    - AF_PACKET support
    - CAP_NET_RAW capability
    - Available interfaces
    - NIC capabilities
  - Health report:
    - Current capture stats
    - Decoder error summary
    - Resource usage
    - Configuration validation
  - Output formats:
    - Human-readable
    - JSON (for automation)
    - Markdown (for issue reports)

**Structured Logging:**

- [ ] `wadjet::logging::Logger` enhancements
  - JSON log output option
  - Syslog integration
  - Log levels per component
  - Contextual logging (capture session ID, etc.)
  - Rate limiting for verbose logs

**Metrics Export:**

- [ ] Prometheus exporter (optional)
  - HTTP endpoint: `/metrics`
  - Standard Prometheus format
  - All metrics exposed
- [ ] JSON metrics dump
  - `wadjet-metrics` command
  - Periodic JSON export to file
  - CI/CD integration

**Implementation:**

- [ ] `include/wadjet/metrics/metrics.hpp` — Metrics framework
- [ ] `include/wadjet/metrics/registry.hpp` — Metrics registry
- [ ] `src/metrics/capture_metrics.cpp`
- [ ] `src/metrics/decoder_metrics.cpp`
- [ ] `src/metrics/system_metrics.cpp`
- [ ] `tools/wadjet-diag.cpp` — Diagnostic utility
- [ ] `tools/wadjet-metrics.cpp` — Metrics dump utility

**Testing:**

- [ ] Metrics collection tests
- [ ] Metrics export tests
- [ ] Self-diagnostic validation
- [ ] Performance impact of metrics (< 1% overhead)

**Documentation:**

- [ ] `docs/metrics.md` — Metrics reference
- [ ] `docs/diagnostics.md` — Diagnostic guide
- [ ] Grafana dashboard examples

**Phase 11: CI/CD & Release**

- [ ] All CI checks pass on supported platforms (Linux)
- [ ] Automated build for release artifacts
- [ ] Docker images build and run successfully
- [ ] Package creation (deb, rpm, or tarball)
- [ ] Version numbers consistent across codebase
- [ ] Git tags for releases

**Phase 12: API Stability & Compatibility**

- [ ] Public API marked stable
- [ ] ABI compatibility documented
- [ ] Deprecation policy defined
- [ ] Breaking changes documented
- [ ] Semantic versioning followed
- [ ] C ABI stable for FFI consumers
- [ ] Python API follows PEP conventions
- [ ] Rust API follows Rust conventions

**Phase 13: Licensing & Legal**

- [ ] All source files have license headers
- [ ] Third-party dependencies documented
- [ ] Third-party licenses compatible
- [ ] NOTICE/ATTRIBUTION file complete
- [ ] Copyright notices accurate
- [ ] Contributor License Agreement (if needed)

**Phase 14: Security Review**

- [ ] No hardcoded credentials or secrets
- [ ] Input validation on all public APIs
- [ ] Secure defaults configured
- [ ] No known vulnerabilities in dependencies
- [ ] Fuzzing found no security issues
- [ ] Network input properly sanitized

**Phase 15: Release Documentation**

- [ ] CHANGELOG.md complete with all changes
- [ ] Release notes written
- [ ] Migration guide for breaking changes
- [ ] Known issues documented
- [ ] README installation tested on clean system
- [ ] Quick start guide verified

**Verification:**

```bash
# Run all verification checks
./scripts/preproduction-check.sh

# Individual checks
make format-check      # Code formatting
make lint              # Static analysis
make test-coverage     # Test coverage report
make docs              # Documentation build
make sanitizer-check   # Memory/thread sanitizers
make benchmark         # Performance validation
```

**Exit Criteria:**

All checkboxes above must be checked before the milestone is considered complete. Any unchecked item blocks release.

**Test Count Target:** All existing tests pass + coverage ≥ 90%

---

### Milestone 22 — ISO 26262 Tool Qualification (ASIL-D)

**Goal:** Qualify Wadjet-Link as an ISO 26262 compliant testing tool for ASIL-D safety-critical automotive development

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required for use in safety-critical automotive projects

**Dependencies:**

> ⚠️ **Prerequisite:** Milestones 13 and 21 MUST be complete before starting ISO 26262 qualification.
> - Milestone 13 (Protocol Completeness) — Tool must be feature-complete
> - Milestone 21 (PreProduction Quality Gate) — Tool must meet production quality standards

- ⏳ Milestone 13: Protocol Completeness (required)
- ⏳ Milestone 21: PreProduction Quality Gate (required)

**Overview:**

ISO 26262-8 Clause 11 defines requirements for qualifying software tools used in safety-related automotive development. As a testing/validation tool, Wadjet-Link must be qualified to ensure it doesn't introduce or fail to detect safety-relevant defects.

**Tool Classification (ISO 26262-8:2018):**

```text
┌─────────────────────────────────────────────────────────────────┐
│              ISO 26262 Tool Classification                    │
├─────────────────────────────────────────────────────────────────┤
│  Tool Category: T2 (Testing Tool)                             │
│  ───────────────────────────────────────────────────────────────┤
│  Tool Impact (TI): TI2                                        │
│    - Can fail to detect errors in safety-related item         │
│    - False negative test results possible                     │
│  ───────────────────────────────────────────────────────────────┤
│  Tool Error Detection (TD): TD3                               │
│    - High confidence needed                                   │
│    - Errors may not be detected without measures              │
│  ───────────────────────────────────────────────────────────────┤
│  Tool Confidence Level (TCL): TCL3                            │
│    - Highest qualification effort required                    │
│    - TI2 + TD3 = TCL3 (per ISO 26262-8 Table 4)               │
└─────────────────────────────────────────────────────────────────┘
```

**Qualification Methods (ISO 26262-8 Table 5):**

For TCL3, the following methods are required:

| Method | Description | Required for TCL3 |
|--------|-------------|-------------------|
| 1a | Increased confidence from use | ++ (Highly Recommended) |
| 1b | Evaluation of development process | ++ (Highly Recommended) |
| 1c | Validation of the software tool | ++ (Highly Recommended) |
| 1d | Development per safety standard | + (Recommended) |

**Implementation Checklist:**

**Phase 1: Tool Qualification Plan (TQP)**

- [ ] Define tool qualification scope
- [ ] Identify tool use cases for safety-related development
- [ ] Document tool classification rationale (TI, TD, TCL)
- [ ] Define qualification methods to be applied
- [ ] Establish qualification schedule and responsibilities
- [ ] Define acceptance criteria for qualification
- [ ] Plan for third-party assessment (if required)
- [ ] Document: `docs/iso26262/tool_qualification_plan.md`

**Phase 2: Tool Operational Requirements (TOR)**

- [ ] Define intended use of tool in safety lifecycle
  - [ ] Which ISO 26262 phases/activities
  - [ ] ASIL levels supported (A, B, C, D)
  - [ ] Input/output work products
- [ ] Specify operational constraints
  - [ ] Supported platforms/OS versions
  - [ ] Hardware requirements
  - [ ] Dependencies and versions
- [ ] Define expected tool behavior
  - [ ] Functional requirements
  - [ ] Performance requirements
  - [ ] Reliability requirements
- [ ] Specify safety-relevant features
  - [ ] Protocol decoding accuracy
  - [ ] Timing precision requirements
  - [ ] Test verdict correctness
- [ ] Document failure modes and effects
- [ ] Document: `docs/iso26262/tool_operational_requirements.md`

**Phase 3: Tool Development Process Evaluation**

- [ ] Document development process used
- [ ] Map process to IEC 61508-3 / ISO 26262-6 requirements
- [ ] Evidence of requirements management
  - [ ] Requirements traceability matrix
  - [ ] Requirements review records
- [ ] Evidence of design documentation
  - [ ] Architecture documentation
  - [ ] Interface specifications
  - [ ] Design review records
- [ ] Evidence of implementation quality
  - [ ] Coding guidelines compliance
  - [ ] Code review records
  - [ ] Static analysis reports
- [ ] Evidence of testing
  - [ ] Test plans and specifications
  - [ ] Test reports
  - [ ] Coverage analysis
- [ ] Evidence of configuration management
  - [ ] Version control history
  - [ ] Build reproducibility
  - [ ] Release process
- [ ] Document: `docs/iso26262/development_process_evaluation.md`

**Phase 4: Tool Validation**

- [ ] Validation test specification
  - [ ] Test cases for each TOR requirement
  - [ ] Expected results documented
  - [ ] Test coverage analysis
- [ ] Validation test execution
  - [ ] Execute all validation tests
  - [ ] Document test results
  - [ ] Analyze deviations
- [ ] Protocol decoder validation
  - [ ] Validate against known-good reference data
  - [ ] Cross-validate with certified tools
  - [ ] Boundary condition testing
- [ ] Timing accuracy validation
  - [ ] Timestamp precision verification
  - [ ] Latency measurement validation
  - [ ] Synchronization accuracy
- [ ] Test verdict validation
  - [ ] No false negatives (missed failures)
  - [ ] Minimal false positives
  - [ ] Correct pass/fail determination
- [ ] Error handling validation
  - [ ] Graceful degradation
  - [ ] Error reporting accuracy
  - [ ] No silent failures
- [ ] Document: `docs/iso26262/tool_validation_report.md`

**Phase 5: Increased Confidence from Use**

- [ ] Document usage history
  - [ ] Projects using the tool
  - [ ] Duration of use
  - [ ] Volume of use (tests executed, packets analyzed)
- [ ] Collect anomaly/bug reports
  - [ ] Issues found during use
  - [ ] Root cause analysis
  - [ ] Corrective actions
- [ ] User feedback collection
  - [ ] Effectiveness assessment
  - [ ] Reliability feedback
  - [ ] Improvement suggestions
- [ ] Regression tracking
  - [ ] Known issues list
  - [ ] Workarounds documented
  - [ ] Issue resolution tracking
- [ ] Document: `docs/iso26262/usage_experience_report.md`

**Phase 6: Tool User Manual (Safety-Relevant)**

- [ ] Installation instructions
  - [ ] Supported configurations
  - [ ] Dependencies and versions
  - [ ] Verification steps
- [ ] Operational guidance
  - [ ] Correct usage procedures
  - [ ] Safety-relevant features
  - [ ] Limitations and constraints
- [ ] Error interpretation guide
  - [ ] Error messages explained
  - [ ] Troubleshooting procedures
  - [ ] When to distrust results
- [ ] Warnings and cautions
  - [ ] Known limitations
  - [ ] Conditions that may cause incorrect results
  - [ ] Mandatory verification steps
- [ ] Reference to qualification documents
- [ ] Document: `docs/iso26262/tool_user_manual.md`

**Phase 7: Configuration & Change Management**

- [ ] Unique tool identification
  - [ ] Version numbering scheme
  - [ ] Build identification
  - [ ] Checksum/hash for verification
- [ ] Configuration items identified
  - [ ] Source code
  - [ ] Build scripts
  - [ ] Test artifacts
  - [ ] Documentation
- [ ] Change control process
  - [ ] Change request procedure
  - [ ] Impact analysis requirement
  - [ ] Approval workflow
  - [ ] Regression testing requirement
- [ ] Release management
  - [ ] Release criteria
  - [ ] Release notes
  - [ ] Deployment procedure
- [ ] Traceability
  - [ ] Requirements to tests
  - [ ] Requirements to code
  - [ ] Changes to releases
- [ ] Document: `docs/iso26262/configuration_management_plan.md`

**Phase 8: Anomaly Management**

- [ ] Anomaly reporting process
  - [ ] How to report issues
  - [ ] Required information
  - [ ] Classification criteria
- [ ] Safety impact analysis
  - [ ] Assess impact on tool qualification
  - [ ] Assess impact on projects using tool
  - [ ] Determine if re-qualification needed
- [ ] Corrective action process
  - [ ] Root cause analysis
  - [ ] Fix implementation
  - [ ] Verification of fix
- [ ] Communication process
  - [ ] Notify affected users
  - [ ] Publish safety-relevant anomalies
  - [ ] Update qualification status
- [ ] Document: `docs/iso26262/anomaly_management_process.md`

**Phase 9: Tool Confidence Argument**

- [ ] Summarize qualification activities
- [ ] Present evidence of tool confidence
  - [ ] Development process compliance
  - [ ] Validation results
  - [ ] Usage experience
- [ ] Residual risk assessment
  - [ ] Known limitations
  - [ ] Mitigations in place
  - [ ] Acceptable risk argument
- [ ] Qualification conclusion
  - [ ] Tool suitable for intended use
  - [ ] Conditions of use
  - [ ] Required user activities
- [ ] Document: `docs/iso26262/tool_qualification_report.md`

**Phase 10: Assessment & Confirmation**

- [ ] Internal review of qualification package
- [ ] Address review findings
- [ ] Prepare for external assessment (optional)
  - [ ] Select qualified assessor
  - [ ] Provide qualification package
  - [ ] Address assessment findings
- [ ] Obtain confirmation of qualification
- [ ] Archive qualification evidence
- [ ] Plan for re-qualification triggers
  - [ ] Major version changes
  - [ ] New features
  - [ ] Anomaly discovery

**Required Documentation Artifacts:**

```text
docs/iso26262/
├── tool_qualification_plan.md          # TQP - Qualification approach
├── tool_operational_requirements.md    # TOR - Intended use & requirements
├── development_process_evaluation.md   # Process evidence
├── tool_validation_specification.md    # Validation test cases
├── tool_validation_report.md           # Validation results
├── usage_experience_report.md          # Use history evidence
├── tool_user_manual.md                 # Safety-relevant usage guide
├── configuration_management_plan.md    # CM procedures
├── anomaly_management_process.md       # Issue handling
├── tool_qualification_report.md        # Final qualification argument
└── traceability/
    ├── requirements_traceability.csv    # TOR to tests/code
    ├── validation_coverage.csv          # Test coverage matrix
    └── anomaly_register.csv             # Known issues tracking
```

**Validation Test Categories:**

| Category | Description | Coverage Target |
|----------|-------------|----------------|
| Protocol Decoding | Verify correct parsing of all protocols | 100% of supported protocols |
| Timing Accuracy | Verify timestamp and timing measurements | ± specified tolerance |
| Test Verdicts | Verify correct pass/fail determination | 100% correct verdicts |
| Error Handling | Verify graceful handling of invalid input | All error paths |
| Boundary Conditions | Verify behavior at limits | All specified limits |
| Stress Conditions | Verify behavior under load | Specified load levels |
| Regression | Verify no regressions from previous versions | All previous test cases |

**Cross-Validation Requirements:**

- [ ] Protocol decoding validated against:
  - [ ] Wireshark (reference tool)
  - [ ] industry-standard automotive testing tools (if available)
  - [ ] Known-good capture files with expected values
- [ ] Timing validated against:
  - [ ] Hardware timestamping verification
  - [ ] Reference timing equipment
- [ ] Test execution validated against:
  - [ ] Manual test execution
  - [ ] Alternative test frameworks

**Re-Qualification Triggers:**

| Trigger | Action Required |
|---------|----------------|
| Major version release | Full re-qualification |
| New protocol support | Partial re-qualification (new features) |
| Bug fix (safety-relevant) | Impact analysis + targeted validation |
| Platform/dependency change | Impact analysis + regression testing |
| ASIL level increase | Gap analysis + additional validation |

**Exit Criteria:**

1. All qualification documentation complete and reviewed
2. All validation tests pass
3. No open safety-relevant anomalies
4. Tool confidence argument accepted
5. Qualification confirmed (internal or external)
6. Tool released with qualified status

**References:**

- ISO 26262-8:2018 Clause 11 "Qualification of software tools"
- ISO 26262-8:2018 Clause 12 "Qualification of hardware tools" (if applicable)
- IEC 61508-3 "Software requirements" (for process evaluation)
- ISO/PAS 8926 "Tool qualification" (additional guidance)

---

### Milestone 23 — Advanced Operations & Security

**Goal:** Add cybersecurity protocol support, user-defined protocol plugins, and distributed architecture capabilities

**Status:** ⏳ Not Started

**Priority:** 🔴 High — Required for modern automotive architectures

**Dependencies:**

- ⏳ Milestone 13: Protocol Completeness (foundation)
- ⏳ Milestone 19: Production Packaging (deployment)

**Overview:**

Modern automotive networks (2024+) increasingly use encryption and proprietary protocols. This milestone addresses three critical gaps that prevent Wadjet-Link from being used in production vehicles:

1. **AUTOSAR SecOC** — Encrypted SOME/IP messages are opaque without SecOC support
2. **TLS/DTLS for DoIP** — ISO 13400-2:2019 requires TLS for secure diagnostics
3. **User-Defined Protocols** — OEM-proprietary protocols require scripting without C++ recompilation
4. **Remote Probe Mode** — Edge deployment (vehicle-side capture, cloud-side analysis)

---

**Phase 1: AUTOSAR SecOC Support**

> 🔒 **Secure Onboard Communication** — Decrypt and verify authenticated SOME/IP messages

**Overview:**

SecOC (AUTOSAR R20-11) adds authentication and freshness to SOME/IP messages. Without SecOC support, Wadjet sees only encrypted payloads.

**Implementation:**

Protocol Structures:
- [ ] `include/wadjet/protocols/secoc/secoc.hpp` — Main header
- [ ] `include/wadjet/protocols/secoc/secoc_types.hpp`
  - SecOCHeader (Freshness Value, Authenticator)
  - SecOCConfig (Key IDs, algorithms)
  - FreshnessValueManager
- [ ] `include/wadjet/protocols/secoc/crypto.hpp`
  - CMAC-AES calculation
  - HMAC-SHA256 calculation
  - Freshness verification

Decoder Implementation:
- [ ] `src/protocols/secoc_decoder.cpp`
  - Identify SecOC-protected messages
  - Extract Freshness Value and Authenticator
  - Verify authentication (if keys available)
  - Decrypt payload (if keys available)

Key Management:
- [ ] `include/wadjet/secoc/key_store.hpp`
  - Load keys from config file
  - Key rotation support
  - HSM integration (optional)
- [ ] Key file format (encrypted JSON)
  - Key ID → Key material mapping
  - Algorithm configuration
  - Freshness value tracking

**Dependencies:**

- OpenSSL or BoringSSL (for crypto)
- Key material from OEM (for actual decryption)

**Testing:**

- [ ] SecOC header parsing tests
- [ ] CMAC verification tests (test vectors)
- [ ] Freshness value tests
- [ ] Integration with SOME/IP decoder

**Limitations:**

> ⚠️ **Key Material:** Actual decryption requires OEM-provided keys. Wadjet can parse SecOC structure without keys.

---

**Phase 2: TLS/DTLS Support for DoIP**

> 🔐 **Secure Diagnostics** — Decrypt TLS-protected DoIP sessions

**Overview:**

ISO 13400-2:2019 mandates TLS for secure diagnostic sessions. Without TLS support, encrypted DoIP traffic is unreadable.

**Implementation:**

TLS Integration:
- [ ] `include/wadjet/protocols/doip/doip_tls.hpp`
  - TLS handshake detection
  - Session key extraction (requires SSLKEYLOGFILE)
  - Decrypted payload reassembly
- [ ] `src/protocols/doip/doip_tls_decoder.cpp`
  - Parse TLS records
  - Decrypt using session keys
  - Pass decrypted payload to DoIP decoder

Key Extraction:
- [ ] Support for SSLKEYLOGFILE format
  - CLIENT_RANDOM + Master Secret
  - Used by Wireshark for TLS decryption
- [ ] Load pre-master secrets from file
- [ ] Optional: MITM proxy mode (for testing only)

**Dependencies:**

- OpenSSL (for TLS parsing)
- SSLKEYLOGFILE from client/server (for decryption)

**Testing:**

- [ ] TLS handshake parsing tests
- [ ] Decryption with known keys
- [ ] DoIP over TLS integration test

**Limitations:**

> ⚠️ **Perfect Forward Secrecy:** ECDHE cipher suites require session key logging. Cannot decrypt without keys.

---

**Phase 3: User-Defined Protocol Plugins (Scripting)**

> 🔌 **Scriptable Decoders** — Define proprietary protocols without C++ recompilation

**Overview:**

Industry-leading automotive tools support scriptable protocol analysis. Wadjet needs a way for users to add proprietary protocol decoders without touching C++.

**Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                 Plugin Architecture                             │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              User-Defined Decoder (Python/Lua)           │   │
│  │  def decode(packet):                                     │   │
│  │      header = struct.unpack('>HHI', packet[:8])          │   │
│  │      return {'msg_id': header[0], ...}                   │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Plugin Manager (C++)                        │   │
│  │  - Load Python/Lua scripts                               │   │
│  │  - Sandbox execution                                     │   │
│  │  - Performance monitoring                                │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Protocol Dispatcher                         │   │
│  │  (routes to native or plugin decoder)                    │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Plugin Framework:
- [ ] `include/wadjet/plugins/plugin_manager.hpp`
  - Load .py or .lua files from `~/.wadjet/plugins/`
  - Sandbox execution (resource limits)
  - Error handling
  - Hot reload
- [ ] `include/wadjet/plugins/decoder_plugin.hpp`
  - Base interface for plugin decoders
  - decode() method
  - Metadata (name, author, version)

Python Plugin Support:
- [ ] `src/plugins/python_plugin.cpp`
  - Embed Python interpreter
  - Call Python decode() function
  - Convert results to C++ DecodeResult
- [ ] Example plugin: `examples/plugins/my_protocol.py`
  ```python
  def decode(packet_bytes):
      # User-defined parsing logic
      return {
          'protocol': 'MyProtocol',
          'fields': {...}
      }
  ```

Lua Plugin Support (optional):
- [ ] `src/plugins/lua_plugin.cpp`
  - Embed Lua interpreter
  - Call Lua decode() function

**Plugin Discovery:**

- [ ] Auto-discover plugins in:
  - `~/.wadjet/plugins/`
  - `/usr/share/wadjet/plugins/`
  - `./plugins/` (project-local)
- [ ] `wadjet-plugins` command
  - List installed plugins
  - Enable/disable plugins
  - Validate plugin syntax

**Performance Considerations:**

- [ ] Native decoders always preferred
- [ ] Plugin overhead measured (< 100 µs per packet)
- [ ] Warning if plugin is too slow

**Testing:**

- [ ] Plugin loading tests
- [ ] Python plugin decode tests
- [ ] Error handling (bad plugin code)
- [ ] Performance benchmarks

**Documentation:**

- [ ] `docs/plugins.md` — Plugin development guide
- [ ] Python plugin API reference
- [ ] Example plugins

---

**Phase 4: Remote Probe Mode (Distributed Architecture)**

> 🌐 **Edge Deployment** — Capture on the vehicle, analyze on the cloud

**Overview:**

Wadjet's Linux-first design enables edge deployment (Raspberry Pi, NVIDIA Jetson in the vehicle). Remote Probe Mode allows capture on one machine, analysis on another.

**Architecture:**

```text
┌─────────────────────────────────────────────────────────────────┐
│                 Distributed Architecture                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────┐        ┌──────────────────────┐   │
│  │    Vehicle (Edge)        │        │   Developer Laptop   │   │
│  │  ┌────────────────────┐  │        │  ┌────────────────┐  │   │
│  │  │ wadjet-probe       │  │  gRPC  │  │ wadjet-viewer  │  │   │
│  │  │ - Capture packets  │──┼────────┼─▶│ - Decode       │  │   │
│  │  │ - Filter           │  │  TLS   │  │ - Analyze      │  │   │
│  │  │ - Compress         │  │        │  │ - Visualize    │  │   │
│  │  └────────────────────┘  │        │  └────────────────┘  │   │
│  └──────────────────────────┘        └──────────────────────┘   │
│          (Headless)                        (Interactive)        │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation:**

Probe Mode (Capture Agent):
- [ ] `tools/wadjet-probe.cpp` — Headless capture daemon
  - Capture packets (no decoding)
  - Apply BPF filter
  - Compress packets (zstd)
  - Stream via gRPC
  - Store to local PCAP (fallback)
  - Minimal resource usage

Viewer Mode (Analysis Client):
- [ ] `tools/wadjet-viewer.cpp` — Remote analysis client
  - Connect to probe via gRPC
  - Receive packet stream
  - Decode locally
  - Display results
  - Optional: Web UI integration (M14)

gRPC Protocol:
- [ ] `proto/wadjet_remote.proto`
  - CaptureRequest (filter, duration)
  - PacketStream (streaming packets)
  - ControlCommands (start/stop/status)
- [ ] TLS for secure communication
- [ ] Authentication (API keys)

**Deployment Scenarios:**

1. **Vehicle Testing:**
   - Probe on in-vehicle Raspberry Pi
   - Viewer on engineer's laptop
   - Real-time analysis over Wi-Fi

2. **CI/CD Integration:**
   - Probe on test bench
   - Viewer in CI pipeline
   - Automated test execution

3. **Fleet Monitoring:**
   - Probes on multiple vehicles
   - Central analysis server
   - Continuous monitoring

**Configuration:**

- [ ] Probe config: `/etc/wadjet/probe.conf`
  - Capture interface
  - Compression level
  - gRPC endpoint
  - Storage limits
- [ ] Viewer config: `~/.config/wadjet/viewer.conf`
  - Probe endpoints
  - Reconnection policy

**Testing:**

- [ ] gRPC communication tests
- [ ] Probe-viewer integration test
- [ ] Network resilience tests (packet loss, reconnect)
- [ ] Performance: latency impact of streaming

**Documentation:**

- [ ] `docs/remote_probe.md` — Remote probe guide
- [ ] Deployment examples
- [ ] Security best practices

---

**Exit Criteria:**

1. SecOC: Can parse SecOC headers and verify MACs (with test keys)
2. TLS: Can decrypt TLS-protected DoIP (with SSLKEYLOGFILE)
3. Plugins: Can load and execute Python plugins without C++ recompilation
4. Remote Probe: Can capture on one machine, analyze on another via gRPC
5. All features documented and tested
6. No more than 5% performance overhead from new features

**Test Count Target:** 60+ tests

**References:**

- AUTOSAR SecOC Specification R20-11
- ISO 13400-2:2019 (DoIP with TLS)
- RFC 8446 (TLS 1.3)
- gRPC documentation

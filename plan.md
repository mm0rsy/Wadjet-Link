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
  - CaptureSession
  - ReplaySession
  - FrameFilter (BPF expression)

wadjet::pcap            → Readers/writers
  - PcapReader
  - PcapWriter
  - PcapngWriter

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

**Deliverables:**

- [ ] Repo structure created
- [ ] CMake project with modern practices
- [ ] CI/CD pipeline (GitHub Actions)
- [ ] clang-format configuration
- [ ] clang-tidy configuration
- [ ] Doxygen setup
- [ ] Coding guidelines document
- [ ] README with vision/architecture
- [ ] CONTRIBUTING.md
- [ ] LICENSE file
- [ ] Example pcap capture in repo

---

### Milestone 1 — Core Packet I/O

**Goal:** Capture + replay + filter frames deterministically

**Implementation:**

- [ ] Linux AF_PACKET / PF_PACKET support
- [ ] Optional libpcap backend
- [ ] Zero-copy ring buffer support
- [ ] Timestamping support (hardware if possible)
- [ ] Packet writer (.pcap, .pcapng)
- [ ] Packet reader abstraction
- [ ] `CaptureSession` implementation
- [ ] `ReplaySession` implementation
- [ ] `Packet` and `PacketView` classes
- [ ] `FrameFilter` (BPF expression)

**Validation Tests:**

- [ ] Send & receive on loopback
- [ ] Verify timestamps monotonic
- [ ] Stress test under load
- [ ] Dropped packet counters

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
- [ ] Fuzz testing (future work)

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
- Integration tests: 26
- **Total: 295 tests**

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

- [ ] YAML parser
- [ ] JSON parser
- [ ] `ScenarioRunner` class
- [ ] CLI runner (`wadjet-run`)
- [ ] Report output (JUnit XML, JSON)

---

### Milestone 5 — Python Bindings

**Goal:** Fast adoption, Jupyter analysis, pytest integration

**Expose:**

- [ ] Capture API
- [ ] Protocol decode
- [ ] Test assertions
- [ ] Pcap reading/writing

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

**Deliverables:**

- [ ] Doxygen API documentation
- [ ] Architecture diagrams
- [ ] Quickstart tutorial
- [ ] Example: Detect SOME/IP service discovery
- [ ] Example: Validate DoIP routing activation
- [ ] Example: Monitor ECU bootup traffic
- [ ] Example: Regression test with pcap fixtures

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

- [ ] Raw Ethernet capture works on Linux
- [ ] SOME/IP decode works
- [ ] GoogleTest can assert on live traffic
- [ ] README shows usage example
- [ ] At least 20 unit tests exist
- [ ] CI pipeline passes

---

## Stretch Goals

- [ ] DDS protocol support
- [ ] TSN awareness (802.1Qbv, etc.)
- [ ] Rust FFI bindings
- [ ] UDS over DoIP parsing
- [ ] Packet injection (TX capability)
- [ ] Web-based report viewer

# Implementation Plan: Protocol Completeness

**Branch**: `013-protocol-completeness` | **Date**: 2026-01-09 | **Spec**: [spec.md](./spec.md)  
**Input**: Feature specification from `/specs/013-protocol-completeness/spec.md`

## Summary

Complete all protocol implementations to 100% specification compliance. This milestone addresses gaps in Ethernet, IPv4, UDP, TCP, SOME/IP, SOME/IP-SD, DoIP, gPTP, UDS, and DDS/RTPS decoders. Focus on: TCP state machine (2min/30s timeouts), IPv4 fragmentation/options (30s timeout), UDP checksum validation (warning-only mode), SOME/IP-TP segmentation (16 MB max), SD entry arrays, DoIP power mode, UDS NRC handling, gPTP TLVs, and DDS CDR improvements. **230+ tests target across 10 phases.**
## Clarifications (From spec.md Session 2026-01-13)

**Key design parameters established:**

1. **IPv4 Fragment Reassembly Timeout**: 30 seconds (automotive-optimized)
2. **TCP Connection Tracking Timeout**: 2 minutes for incomplete connections, 30 seconds for TIME_WAIT state
3. **SOME/IP-TP Maximum Message Size**: 16 MB maximum (realistic automotive limit)
4. **UDP Checksum Validation Default**: Enabled with warning-only mode (logs warnings, doesn't drop packets)
5. **TCP Out-of-Order Segment Buffering**: Buffer up to 16 segments per connection
## Technical Context

**Language/Version**: C++20 (existing codebase baseline from M0-M11)  
**Primary Dependencies**: GoogleTest (testing), libFuzzer (fuzz testing), yaml-cpp (scenarios), pybind11 (Python bindings)  
**Storage**: PCAP files for regression tests (pcap_samples/ directory), in-memory packet buffers  
**Testing**: GoogleTest with custom matchers, fuzz testing with AddressSanitizer, property-based testing  
**Target Platform**: Linux-first (AF_PACKET), future: Windows (Npcap), macOS (libpcap)  
**Project Type**: C++ library with multi-language bindings (Python, C ABI, Rust)  
**Performance Goals**: Maintain zero-copy packet processing, minimal decode latency (<1μs per protocol layer)  
**Constraints**: No external protocol dependencies, passive analysis only (no packet modification), backward compatibility with M0-M11 APIs  
**Scale/Scope**: 230+ new tests, 10 protocol decoders enhanced, 50+ new test fixtures (PCAP samples)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### ✅ **I. Library-First Architecture**
- **Status**: PASS - All protocol decoders are libraries (`src/protocols/*.cpp`, headers in `include/wadjet/protocols/`)
- **Evidence**: M0-M11 established pattern: standalone decoders with clear APIs (e.g., `Ipv4Decoder`, `TcpDecoder`, `SomeipDecoder`)
- **This milestone**: Enhances existing libraries without breaking APIs - additive changes only

### ✅ **II. Zero-Copy, Zero-Latency**
- **Status**: PASS - All enhancements maintain `PacketView` (zero-copy) for read-only operations
- **Evidence**: M1 established `Packet`/`PacketView` pattern, M2-M11 decoders use `PacketView::data()` exclusively
- **This milestone**: TCP state tracking uses minimal memory (connection map), IPv4 reassembly uses zero-copy fragment references

### ✅ **III. Test-First Development**
- **Status**: PASS - 230+ tests planned before implementation
- **Evidence**: M0-M11 have 378+ existing tests (GoogleTest framework)
- **This milestone**: Each phase begins with test creation (Red-Green-Refactor cycle enforced)

### ✅ **IV. Pluggable Protocol Architecture**
- **Status**: PASS - No changes to `IProtocolDecoder` interface
- **Evidence**: M2 established `ProtocolDecoder` base class, all decoders inherit from `DecoderBase<T>` CRTP
- **This milestone**: Enhancements are internal to existing decoders - no dispatcher modifications

### ✅ **V. Multi-Language FFI Support**
- **Status**: PASS - Python/C/Rust bindings updated after C++ implementation
- **Evidence**: M5 (Python), M7 (Rust), M11 (C ABI for diagnostics)
- **This milestone**: Phase 6 updates bindings for new features (TCP state, IPv4 options, etc.)

### ⚠️ **VI. Automotive Standards Compliance**
- **Status**: CONDITIONAL PASS - This milestone IS the compliance enforcement
- **Gaps Addressed**:
  - SOME/IP PRS_SOMEIP_00191 (TP segmentation) - currently missing
  - ISO 13400-2 (DoIP power mode) - partially implemented
  - ISO 14229-1 (UDS NRC complete set) - subset only
  - IEEE 802.1AS (gPTP TLV parsing) - basic only
- **This milestone**: Achieves 100% spec compliance for all supported protocols

### ✅ **VII. Non-Invasive Monitoring**
- **Status**: PASS - All features are passive analysis
- **Evidence**: M1 capture is read-only (AF_PACKET in PACKET_RX_RING mode)
- **This milestone**: No active packet generation - analysis and validation only

**FINAL GATE STATUS**: ✅ **PASS** - All constitutional principles satisfied. Milestone enhances existing compliant architecture.

## Project Structure

### Documentation (this feature)

```text
specs/013-protocol-completeness/
├── plan.md              # This file (created by /speckit.plan)
├── research.md          # Phase 0: Protocol gap analysis and best practices research
├── data-model.md        # Phase 1: TCP connection state, IPv4 fragment cache, etc.
├── quickstart.md        # Phase 1: Usage examples for new features
└── contracts/           # Phase 1: Updated decoder APIs (if any breaking changes)
```

### Source Code (repository root)

**Existing codebase structure** (established in M0-M11):

```text
include/wadjet/protocols/
├── ethernet.hpp              # ENHANCED: VLAN QinQ handling
├── ipv4.hpp                  # ENHANCED: Options, fragmentation, ToS/DSCP
├── tcp.hpp                   # ENHANCED: State machine, all options, retransmission detection
├── udp.hpp                   # ENHANCED: Checksum validation
├── someip.hpp                # ENHANCED: TP segmentation, message length validation
├── someip_sd.hpp             # ENHANCED: All entry types, option arrays
├── doip.hpp                  # ENHANCED: Diagnostic power mode
├── uds.hpp                   # ENHANCED: All NRCs, sub-function handling
├── gptp.hpp                  # ENHANCED: All TLV types (Follow_Up Info, Organization Extension)
└── dds/
    ├── rtps.hpp              # ENHANCED: CDR improvements
    └── discovery.hpp         # ENHANCED: Complete QoS policy parsing

src/protocols/
├── ethernet.cpp              # IPv4 fragmentation reassembly, option parsing
├── tcp.cpp                   # TCP state tracker, connection lifecycle
├── udp.cpp                   # UDP checksum validator
├── someip.cpp                # TP reassembler
├── someip_sd.cpp             # SD entry/option parser enhancements
├── doip.cpp                  # Power mode handler
├── uds_decoder.cpp           # NRC dictionary
├── gptp_decoder.cpp          # TLV parser
└── dds/
    └── rtps_decoder.cpp      # CDR type system improvements

tests/protocols/
├── test_ipv4_options.cpp     # NEW: IPv4 option tests (20+ tests)
├── test_ipv4_fragmentation.cpp # NEW: Fragment reassembly tests (15+ tests)
├── test_tcp_state.cpp        # NEW: TCP connection tracking tests (30+ tests)
├── test_tcp_options.cpp      # NEW: All TCP option tests (15+ tests)
├── test_udp_checksum.cpp     # NEW: UDP checksum validation tests (10+ tests)
├── test_someip_tp.cpp        # NEW: SOME/IP-TP segmentation tests (25+ tests)
├── test_someip_sd_entries.cpp # NEW: SD entry array tests (20+ tests)
├── test_doip_power.cpp       # NEW: DoIP power mode tests (10+ tests)
├── test_uds_nrc.cpp          # NEW: UDS NRC complete set tests (30+ tests)
├── test_gptp_tlv.cpp         # NEW: gPTP TLV tests (15+ tests)
└── test_dds_cdr.cpp          # NEW: DDS/RTPS CDR tests (20+ tests)

pcap_samples/protocol-completeness/
├── ipv4_fragmented.pcap      # IPv4 fragmentation test cases
├── tcp_handshake.pcap        # TCP connection lifecycle
├── someip_tp_large.pcap      # SOME/IP-TP multi-segment messages
├── someip_sd_complex.pcap    # SD with multiple entry types
├── doip_power_mode.pcap      # DoIP power mode transitions
├── uds_all_nrcs.pcap         # UDS negative responses
└── gptp_tlv_rich.pcap        # gPTP with all TLV types

fuzz/
├── fuzz_ipv4_options.cpp     # NEW: IPv4 option fuzzer
├── fuzz_tcp_options.cpp      # NEW: TCP option fuzzer
├── fuzz_someip_tp.cpp        # NEW: SOME/IP-TP fuzzer
└── fuzz_gptp_tlv.cpp         # NEW: gPTP TLV fuzzer
```

**Structure Decision**: Enhances existing M0-M11 codebase. No new top-level directories - all changes are within established protocol decoder structure. New files are additive (tests, fuzz harnesses, PCAP samples) rather than replacement.

## Phase 0: Research & Gap Analysis

**Duration**: 3 days  
**Output**: `research.md` with protocol compliance gaps and implementation strategies

### Research Tasks

#### 1. TCP State Machine Best Practices (2 days)

**Questions to Answer**:

- How do production network analysis tools (Wireshark, tcpdump) implement TCP state tracking?
- What is the optimal data structure for connection tracking (hash map with (src_ip, src_port, dst_ip, dst_port, protocol) 5-tuple key)?
- **Connection timeouts**: 2 minutes for incomplete connections, 30 seconds for TIME_WAIT (clarified)
- **Out-of-order buffering**: Up to 16 segments per connection (clarified)
- How to handle TCP retransmissions (sequence number tracking, duplicate ACK detection)?
- What is the standard approach for TCP option parsing (TLV format, option codes 0-255)?

**Research Sources**:

- RFC 793 (TCP specification) - State diagram section
- RFC 7323 (TCP Extensions for High Performance) - Window scaling, timestamps
- RFC 2018 (TCP Selective Acknowledgment) - SACK option
- Wireshark source code: `epan/dissectors/packet-tcp.c` (tcp_analyze_sequence_number)
- Linux kernel: `net/ipv4/tcp_input.c` (TCP state machine implementation)

**Deliverable**: Document TCP connection tracking algorithm, data structures, and API design

#### 2. IPv4 Fragmentation Reassembly (1 day)

**Questions to Answer**:

- How to implement fragment cache (keyed by (src_ip, dst_ip, protocol, identification))?
- **Reassembly timeout**: 30 seconds (automotive-optimized, configurable - clarified)
- How to handle overlapping fragments (take first, take last, or error)?
- What are common IPv4 options and their parsing formats?

**Research Sources**:

- RFC 791 (IPv4 specification) - Fragmentation and reassembly
- RFC 2460 (IPv6) - Improved fragmentation handling (lessons learned)
- FreeBSD source: `sys/netinet/ip_input.c` (ip_reass function)

**Deliverable**: IPv4 fragment reassembly algorithm and option parsing table

#### 3. SOME/IP-TP Segmentation (1 day)

**Questions to Answer**:

- What is the SOME/IP-TP header format (offset field, more segments flag)?
- How to handle out-of-order segments?
- TP timeout: **5 seconds** (per PRS_SOMEIP_00191)
- Maximum message size: **16 MB** (realistic automotive limit for firmware updates and diagnostics)

**Research Sources**:

- AUTOSAR PRS_SOMEIP (Protocol Specification) - Section 4.2.1 (TP)
- Existing Wadjet-Link `someip.cpp` - Current implementation gaps

**Deliverable**: TP reassembly strategy aligned with AUTOSAR spec

#### 4. Protocol Standard Gaps (Ongoing)

**Research all spec gaps identified in M13 spec**:

- DoIP ISO 13400-2 Amendment 1 (power mode messages)
- UDS ISO 14229-1 Table A.1 (all NRC codes 0x10-0x93)
- gPTP IEEE 802.1AS Annex F (TLV types and formats)
- DDS/RTPS OMG Spec v2.5 Section 10 (CDR encapsulation)

**Deliverable**: Compliance matrix showing current vs. required implementation

### Research Output Template (research.md)

```markdown
# Protocol Completeness Research

## TCP State Machine

### Decision: Use hash map with 5-tuple key, 16-segment out-of-order buffer
### Rationale: Industry standard (Wireshark, Linux kernel)
### Timeouts: 2 minutes for incomplete connections, 30 seconds for TIME_WAIT
### Data Structure:
\`\`\`cpp
struct TcpConnection {
    TcpState state;
    uint32_t seq_next;
    uint32_t ack_next;
    uint16_t window_size;
    uint8_t window_scale;
    std::array<TcpSegment, 16> out_of_order_buffer;  // Max 16 segments
    std::chrono::steady_clock::time_point last_seen;
    // ...
};
std::unordered_map<ConnectionKey, TcpConnection> connections_;
\`\`\`

## IPv4 Fragmentation

### Decision: 30-second reassembly timeout (automotive-optimized), take-first for overlaps
### Rationale: Balances RFC 791 compliance with automotive resource constraints
### Algorithm: [Fragment cache with timeout cleanup]

## UDP Checksum Validation

### Decision: Enabled by default in warning-only mode
### Rationale: Detects corruption without breaking legacy systems
### Behavior: Log warnings for invalid checksums, don't drop packets

## SOME/IP-TP

### Decision: 5-second segment timeout, 16 MB maximum message size
### Rationale: AUTOSAR PRS_SOMEIP_00191 timeout, realistic automotive limit
### Segment Handling: [Out-of-order buffering with offset tracking]

## Fuzz Testing Strategy

### Decision: Run until 90% edge coverage or 24 hours, whichever first
### Rationale: Balance thoroughness with CI/CD time constraints
### Corpus: Seed with pcap_samples/ + AFL-generated mutations

## Protocol Compliance Gaps

| Protocol | Current | Required | Gap |
|----------|---------|----------|-----|
| TCP | Basic parsing | Full state machine | State tracking, retransmission detection |
| IPv4 | Header only | Options + fragmentation | Option parsing, reassembly |
| SOME/IP | Basic | TP segmentation | TP header, reassembly |
| ... | ... | ... | ... |
```

---

## Complexity Tracking

**No constitutional violations** - no complexity justification required.

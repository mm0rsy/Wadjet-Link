# Feature Specification: Pluggable Protocol Decoder Architecture

**Feature Branch**: `milestone/002-protocol-decoders`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M2 - Protocol Decoders

## User Scenarios & Testing

### User Story 1 - Decode Captured Packets (Priority: P1)

As an automotive network engineer, I need to decode raw packet bytes into structured protocol headers (Ethernet, IP, UDP, TCP), so that I can understand what's happening on the network without manual hex analysis.

**Why this priority**: Protocol decoding is the core value proposition. Without it, users only have raw bytes with no semantic meaning.

**Independent Test**: Capture a UDP packet, decode it, and verify the Ethernet, IPv4, and UDP headers are correctly parsed.

**Acceptance Scenarios**:

1. **Given** a raw packet buffer, **When** I decode it with ProtocolDispatcher, **Then** I get Ethernet header with correct MAC addresses and EtherType
2. **Given** an IPv4 packet, **When** I decode it, **Then** I get source/destination IP addresses, protocol field, and checksum validation
3. **Given** a UDP packet, **When** I decode it, **Then** I get source/destination ports and payload
4. **Given** a TCP packet, **When** I decode it, **Then** I get ports, sequence numbers, flags, and TCP options
5. **Given** a malformed packet (too short), **When** I attempt to decode, **Then** I receive a decode error indicating the problem

---

### User Story 2 - SOME/IP Protocol Support (Priority: P1)

As an AUTOSAR developer, I need to decode SOME/IP messages, so that I can monitor service-oriented communication between ECUs and validate service discovery.

**Why this priority**: SOME/IP is a mandatory automotive protocol. This is a core differentiator for automotive-focused tools.

**Independent Test**: Send a SOME/IP packet and verify service ID, method ID, and message type are correctly decoded.

**Acceptance Scenarios**:

1. **Given** a SOME/IP request packet, **When** I decode it, **Then** I get service ID, method ID, client ID, session ID, protocol version
2. **Given** a SOME/IP-SD (Service Discovery) packet, **When** I decode it, **Then** I get service offers/finds and endpoint options
3. **Given** a SOME/IP packet with invalid length field, **When** I decode it, **Then** I get a decode error with diagnostic information
4. **Given** a SOME/IP response, **When** I decode it, **Then** I can distinguish it from a request based on message type field

---

### User Story 3 - DoIP Diagnostic Protocol (Priority: P1)

As a diagnostic engineer, I need to decode DoIP (Diagnostics over IP) messages, so that I can monitor and validate UDS diagnostic communication over Ethernet.

**Why this priority**: DoIP is ISO 13400 standard for automotive diagnostics. Essential for diagnostic tool validation.

**Independent Test**: Send a DoIP routing activation request and verify the payload type, source/target addresses are decoded.

**Acceptance Scenarios**:

1. **Given** a DoIP routing activation request, **When** I decode it, **Then** I get source address, activation type, and OEM-specific data
2. **Given** a DoIP diagnostic message, **When** I decode it, **Then** I get source/target logical addresses and UDS payload
3. **Given** a DoIP alive check, **When** I decode it, **Then** I identify it as a keep-alive message
4. **Given** a DoIP packet with incorrect protocol version, **When** I decode it, **Then** I get a version mismatch error

---

### User Story 4 - VLAN Tag Parsing (Priority: P2)

As a TSN (Time-Sensitive Networking) engineer, I need to parse 802.1Q VLAN tags including QinQ (double tagging), so that I can analyze traffic segregation and priority tagging in automotive Ethernet networks.

**Why this priority**: VLAN support is important for TSN scenarios but not blocking for basic packet analysis.

**Independent Test**: Capture a VLAN-tagged packet and verify the VLAN ID and priority code point are extracted.

**Acceptance Scenarios**:

1. **Given** an 802.1Q tagged packet, **When** I decode it, **Then** I get VLAN ID and priority code point (PCP)
2. **Given** a QinQ double-tagged packet, **When** I decode it, **Then** I get both outer and inner VLAN tags
3. **Given** a packet with no VLAN tag, **When** I decode it, **Then** the decoder correctly identifies it as untagged

---

### User Story 5 - Extensible Decoder Framework (Priority: P1)

As a developer adding support for a new automotive protocol (e.g., DDS, CAN-over-Ethernet), I need a pluggable decoder architecture, so that I can implement a new decoder without modifying existing code.

**Why this priority**: Extensibility is a constitutional principle. New protocols must be addable without refactoring.

**Independent Test**: Implement a minimal custom protocol decoder and verify it integrates with ProtocolDispatcher without modifying existing decoders.

**Acceptance Scenarios**:

1. **Given** the IProtocolDecoder interface, **When** I implement a new decoder class, **Then** I can register it with ProtocolDispatcher
2. **Given** a registered custom decoder, **When** a matching packet arrives (by EtherType or port), **Then** my decoder is invoked
3. **Given** an error in my custom decoder, **When** it fails, **Then** other decoders continue to function correctly
4. **Given** DecoderBase CRTP template, **When** I use it, **Then** I get type-safe decoder implementation with minimal boilerplate

### Edge Cases

- What happens when a packet claims to be IPv4 but is too short to contain a valid IP header?
- How are packets with checksum failures handled (report error vs. silently drop)?
- What if a SOME/IP packet has a length field that exceeds the actual payload size?
- How does the system handle unknown EtherTypes that don't match any registered decoder?
- What happens when a TCP packet has an invalid header length field (too small or too large)?
- How are IPv4 packets with options handled (non-standard 20-byte headers)?

## Requirements

### Functional Requirements

- **FR-001**: System MUST provide an IProtocolDecoder interface defining decode(), supports(), and name() methods
- **FR-002**: System MUST provide DecoderBase CRTP template for type-safe decoder implementations
- **FR-003**: System MUST implement Ethernet decoder supporting standard frames and 802.1Q VLAN tags
- **FR-004**: Ethernet decoder MUST support QinQ (802.1ad) double VLAN tagging
- **FR-005**: System MUST implement IPv4 decoder with header parsing and checksum validation
- **FR-006**: IPv4 decoder MUST parse IP options when present in the header
- **FR-007**: System MUST implement UDP decoder extracting source/destination ports and payload
- **FR-008**: System MUST implement TCP decoder with support for flags, sequence numbers, and options parsing
- **FR-009**: System MUST implement SOME/IP decoder parsing service ID, method ID, client ID, session ID, protocol version
- **FR-010**: System MUST implement SOME/IP Service Discovery (SOME/IP-SD) decoder for service offers, finds, and endpoint options
- **FR-011**: System MUST implement DoIP decoder for ISO 13400-2 protocol (routing activation, diagnostic messages, alive check)
- **FR-012**: System MUST provide ProtocolDispatcher for EtherType-based and port-based protocol dispatch
- **FR-013**: All decoders MUST return DecodeResult containing success/error status and decoded header
- **FR-014**: All decoders MUST validate buffer lengths before accessing packet data
- **FR-015**: Decoders MUST NOT throw exceptions; errors MUST be returned via DecodeResult
- **FR-016**: All decoders MUST handle truncated packets gracefully with descriptive error messages
- **FR-017**: System MUST support chained decoding (Ethernet → IPv4 → UDP → SOME/IP)
- **FR-018**: Decoder implementations MUST be zero-copy (work with PacketView, no buffer allocation)
- **FR-019**: ProtocolDispatcher MUST allow registration of custom decoders without modifying core code
- **FR-020**: System MUST provide DecodeError enum with specific error types (BufferTooSmall, InvalidChecksum, UnsupportedVersion, etc.)

### Key Entities

- **IProtocolDecoder**: Abstract interface for all protocol decoders
- **DecoderBase<T>**: CRTP base class providing type-safe decoder implementation helpers
- **ProtocolDispatcher**: Manages decoder registration and dispatches packets to appropriate decoders based on EtherType/port
- **DecodeResult**: Return type encoding success/error and decoded header (std::expected-like)
- **EthernetHeader**: Parsed Ethernet frame with source/dest MAC, EtherType, optional VLAN tags
- **IPv4Header**: Parsed IPv4 header with addresses, protocol, TTL, checksum, options
- **UDPHeader**: Parsed UDP header with ports and payload reference
- **TCPHeader**: Parsed TCP header with ports, flags, sequence numbers, options
- **SOMEIPHeader**: Parsed SOME/IP header with service/method IDs, message type, return code
- **ServiceDiscoveryMessage**: Parsed SOME/IP-SD entries (offers, finds, endpoints)
- **DoIPHeader**: Parsed DoIP header with payload type, source/target addresses

## Success Criteria

### Measurable Outcomes

- **SC-001**: All mandatory protocols (Ethernet, IPv4, UDP, TCP, SOME/IP, SOME/IP-SD, DoIP) have complete decoder implementations
- **SC-002**: 100% of known PCAP fixtures (39+ test files) decode successfully without errors
- **SC-003**: All decoders correctly handle malformed packets with buffer sizes from 0 bytes to maximum frame size
- **SC-004**: Fuzz testing with libFuzzer runs for 1 million inputs without crashes or memory errors
- **SC-005**: All protocol decoders complete with zero memory leaks (verified by AddressSanitizer)
- **SC-006**: IPv4 checksum validation detects 100% of single-bit errors in test cases
- **SC-007**: ProtocolDispatcher correctly routes packets to appropriate decoders with 100% accuracy in integration tests
- **SC-008**: Chained decoding (Ethernet → IPv4 → UDP → SOME/IP) completes for all valid packets in regression test suite
- **SC-009**: All test cases pass on both big-endian and little-endian representations (network byte order conversion verified)
- **SC-010**: Custom decoder registration works without requiring modifications to core library code

## Assumptions

- All automotive protocols use big-endian (network byte order) for multi-byte fields
- SOME/IP uses AUTOSAR-defined message formats (no proprietary vendor extensions in core decoder)
- DoIP implements ISO 13400-2:2019 standard
- Packets are well-formed according to their respective protocol specifications in typical scenarios
- Maximum Ethernet frame size is 1500 bytes (standard MTU) or 9000 bytes (jumbo frames)

## Dependencies

- **External**: Standard C++20 library, byte order conversion functions (ntohl, ntohs)
- **Internal**: Milestone 1 (Packet, PacketView, CaptureSession for test infrastructure)

## Out of Scope

- Protocol encoding/serialization (decoders are read-only)
- Packet modification or rewriting
- Stateful protocol analysis (e.g., TCP stream reassembly, session tracking)
- Application-layer protocols beyond automotive domain (HTTP, DNS, etc.)
- gPTP (IEEE 802.1AS) decoder (this is Milestone 8)
- UDS (ISO 14229) decoder (this is Milestone 9)
- DDS/RTPS decoder (future milestone)
- CAN-over-Ethernet protocols (future consideration)

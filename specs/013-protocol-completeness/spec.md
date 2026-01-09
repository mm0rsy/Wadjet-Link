# Feature Specification: Protocol Completeness

**Feature Branch**: `milestone/013-protocol-completeness`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M13 - Protocol Completeness (NEXT MILESTONE)  
**Priority**: 🔴 **Critical** - Foundation for advanced features

## Overview

Fill gaps in all existing protocol implementations to achieve production-grade robustness. This milestone addresses incomplete parsing, missing validation, edge cases, and protocol compliance issues across Ethernet, IPv4, UDP, TCP, SOME/IP, SOME/IP-SD, DoIP, gPTP, and UDS decoders. **This is the foundation for all advanced protocol features.**

## User Scenarios & Testing

### User Story 1 - Complete IPv4 Header Parsing (Priority: P1)

As a network protocol engineer, I need full IPv4 header parsing including options and fragmentation, so that I can analyze complex IP traffic scenarios.

**Why this priority**: IPv4 is the transport layer for all automotive Ethernet protocols - gaps here break higher-layer analysis.

**Independent Test**: Parse IPv4 packets with all header options and fragmented packets.

**Acceptance Scenarios**:

1. **Given** IPv4 packets with options (Router Alert, Timestamp, Record Route, Source Route), **When** decoded, **Then** all options are parsed correctly
2. **Given** fragmented IPv4 packets, **When** processed, **Then** fragment offset, MF flag, and identification are extracted
3. **Given** fragmented datagrams, **When** reassembled, **Then** original payload is reconstructed correctly
4. **Given** IPv4 with Type of Service (ToS)/DSCP, **When** parsed, **Then** QoS markings are exposed
5. **Given** IPv4 checksum validation, **When** enabled, **Then** corrupted packets are detected

### User Story 2 - Complete TCP State Machine (Priority: P1)

As a diagnostic engineer, I need full TCP connection tracking, so that I can monitor DoIP and UDS session states.

**Why this priority**: DoIP and UDS run over TCP - without state tracking, diagnostic sessions cannot be validated.

**Independent Test**: Capture TCP connection lifecycle and verify state transitions.

**Acceptance Scenarios**:

1. **Given** TCP three-way handshake (SYN, SYN-ACK, ACK), **When** tracked, **Then** connection establishment is detected
2. **Given** TCP data transfer, **When** monitored, **Then** sequence/acknowledgment numbers are validated
3. **Given** TCP connection teardown (FIN, ACK), **When** tracked, **Then** connection closure is detected
4. **Given** TCP retransmissions, **When** detected, **Then** duplicate packets are identified
5. **Given** TCP window scaling and options, **When** parsed, **Then** all options (MSS, SACK, timestamps) are extracted

### User Story 3 - UDP Checksum Validation (Priority: P2)

As a quality assurance engineer, I need UDP checksum validation, so that I can detect corrupted SOME/IP and Service Discovery messages.

**Why this priority**: SOME/IP-SD runs over UDP - corrupted packets can cause service discovery failures.

**Independent Test**: Validate UDP checksums on captured traffic.

**Acceptance Scenarios**:

1. **Given** UDP packets with valid checksums, **When** validated, **Then** packets pass checksum test
2. **Given** UDP packets with invalid checksums, **When** validated, **Then** corruption is detected and reported
3. **Given** UDP zero checksum (allowed for IPv4), **When** encountered, **When** validation handles it correctly
4. **Given** checksum validation mode, **When** enabled/disabled, **Then** system respects configuration
5. **Given** corrupted SOME/IP messages, **When** detected, **Then** higher-layer decoding is skipped

### User Story 4 - SOME/IP Message Segmentation (Priority: P1)

As a SOME/IP developer, I need support for TP (Transport Protocol) message segmentation, so that I can handle large SOME/IP messages.

**Why this priority**: Large method calls and events require TP - without this, large payloads fail.

**Independent Test**: Decode segmented SOME/IP-TP messages.

**Acceptance Scenarios**:

1. **Given** SOME/IP-TP header with More Segments flag, **When** decoded, **Then** segmentation is detected
2. **Given** multiple TP segments, **When** reassembled, **Then** complete SOME/IP message is reconstructed
3. **Given** TP segment offset, **When** parsed, **Then** segment ordering is correct
4. **Given** incomplete TP message (missing segments), **When** detected, **Then** system reports partial message
5. **Given** TP timeout, **When** exceeded, **Then** incomplete message is discarded

### User Story 5 - SOME/IP Service Discovery Entry Arrays (Priority: P2)

As an automotive software engineer, I need complete SD entry parsing (Service, EventGroup, Options), so that I can validate service discovery behavior.

**Why this priority**: SD drives service availability - incomplete parsing misses critical entries.

**Independent Test**: Parse all SD entry types and option configurations.

**Acceptance Scenarios**:

1. **Given** SD messages with multiple Service entries, **When** parsed, **Then** all entries are extracted
2. **Given** SD EventGroup subscription entries, **When** decoded, **Then** TTL and endpoint options are parsed
3. **Given** SD configuration options (IPv4 Endpoint, IPv4 Multicast), **When** processed, **Then** all option fields are extracted
4. **Given** SD entry arrays (Service, ConsumedEventGroup, EventGroup), **When** parsed, **Then** array counts match actual entries
5. **Given** SD Reboot/Unicast flags, **When** detected, **Then** flag semantics are respected

### User Story 6 - DoIP Diagnostic Power Mode (Priority: P2)

As a diagnostic tester, I need DoIP diagnostic power mode handling, so that I can test ECU wake-up and sleep scenarios.

**Why this priority**: Power mode affects diagnostic session availability - needed for power management testing.

**Independent Test**: Monitor DoIP power mode transitions.

**Acceptance Scenarios**:

1. **Given** DoIP Diagnostic Power Mode messages, **When** received, **Then** power mode value is extracted
2. **Given** power mode transitions (Ready → NotReady → NotSupported), **When** tracked, **Then** state changes are logged
3. **Given** diagnostic requests during NotReady mode, **When** detected, **Then** expected behavior is validated
4. **Given** power mode information request, **When** sent, **Then** response is correctly parsed
5. **Given** power mode timeout, **When** configured, **Then** stale power mode state is detected

### User Story 7 - UDS Negative Response Code Handling (Priority: P1)

As a diagnostic engineer, I need comprehensive NRC (Negative Response Code) handling, so that I can understand why diagnostic requests fail.

**Why this priority**: NRCs indicate diagnostic failures - without proper handling, troubleshooting is impossible.

**Independent Test**: Trigger all UDS NRCs and verify parsing.

**Acceptance Scenarios**:

1. **Given** UDS negative response (0x7F SID NRC), **When** decoded, **Then** service ID and NRC are extracted
2. **Given** NRC codes (0x10-0x93), **When** parsed, **Then** human-readable descriptions are provided
3. **Given** sub-function NRCs, **When** encountered, **Then** sub-function byte is correctly extracted
4. **Given** NRC classification, **When** determined, **Then** temporary vs. permanent errors are identified
5. **Given** service-specific NRCs, **When** parsed, **Then** context-sensitive interpretation is provided

### User Story 8 - gPTP Follow_Up Information TLV (Priority: P2)

As a time synchronization engineer, I need complete gPTP TLV parsing including rate ratio, so that I can analyze clock performance.

**Why this priority**: Rate ratio is critical for clock drift analysis - missing TLVs provide incomplete sync picture.

**Independent Test**: Parse gPTP Follow_Up messages with all TLV types.

**Acceptance Scenarios**:

1. **Given** Follow_Up Information TLV, **When** parsed, **Then** rate ratio and GM time base indicator are extracted
2. **Given** Organization Extension TLV, **When** encountered, **Then** organization ID and sub-type are decoded
3. **Given** multiple TLVs in Announce, **When** processed, **Then** all TLVs are parsed in order
4. **Given** unknown TLV types, **When** encountered, **Then** graceful fallback occurs with warning
5. **Given** malformed TLVs (incorrect length), **When** detected, **Then** error is reported without crash

## Edge Cases

- What happens with IPv4 packets that have both options and fragmentation?
- How are out-of-order TCP segments handled during connection tracking?
- What if SOME/IP-TP segments arrive out of order or with gaps?
- How does system handle SD messages with inconsistent entry/option counts?
- What happens when DoIP diagnostic message exceeds maximum size?
- How are UDS requests with invalid sub-function values processed?
- What if gPTP Announce messages lack required TLVs?

## Requirements

### Functional Requirements

#### IPv4 Protocol Completion

- **FR-001**: System MUST parse all IPv4 header options (Router Alert, Timestamp, Record Route, Source/Strict Source Route)
- **FR-002**: System MUST handle IPv4 fragmentation (fragment offset, MF flag, identification)
- **FR-003**: System MUST support IPv4 fragment reassembly with timeout (60s default per RFC 791)
- **FR-004**: System MUST parse Type of Service (ToS) and DSCP fields
- **FR-005**: System MUST validate IPv4 header checksum with enable/disable option
- **FR-006**: System MUST detect and report IPv4 header anomalies (invalid version, header length)

#### TCP Protocol Completion

- **FR-007**: System MUST track TCP connection state (CLOSED, SYN_SENT, ESTABLISHED, FIN_WAIT, etc.)
- **FR-008**: System MUST parse all TCP options (MSS, Window Scale, SACK, Timestamps, NOP, EOL)
- **FR-009**: System MUST validate TCP sequence and acknowledgment numbers
- **FR-010**: System MUST detect TCP retransmissions
- **FR-011**: System MUST track TCP window size and scaling factor
- **FR-012**: System MUST handle TCP connection establishment (three-way handshake)
- **FR-013**: System MUST handle TCP connection teardown (four-way FIN or RST)
- **FR-014**: System MUST calculate TCP payload length correctly

#### UDP Protocol Completion

- **FR-015**: System MUST validate UDP checksum with enable/disable option
- **FR-016**: System MUST handle UDP zero checksum (IPv4 only)
- **FR-017**: System MUST detect UDP checksum errors and flag corrupted packets
- **FR-018**: System MUST calculate UDP payload length accurately

#### SOME/IP Protocol Completion

- **FR-019**: System MUST parse SOME/IP-TP (Transport Protocol) headers
- **FR-020**: System MUST reassemble segmented SOME/IP-TP messages
- **FR-021**: System MUST handle TP More Segments flag and segment offset
- **FR-022**: System MUST timeout incomplete TP messages (configurable, default 5s)
- **FR-023**: System MUST validate SOME/IP message length field
- **FR-024**: System MUST parse all SOME/IP message types (REQUEST, REQUEST_NO_RETURN, NOTIFICATION, REQUEST_ACK, RESPONSE, ERROR, TP)

#### SOME/IP-SD Protocol Completion

- **FR-025**: System MUST parse all SD entry types (FindService, OfferService, SubscribeEventgroup, StopSubscribeEventgroup)
- **FR-026**: System MUST parse all SD option types (Configuration, LoadBalancing, IPv4 Endpoint, IPv4 Multicast, IPv4 SD Endpoint)
- **FR-027**: System MUST handle SD entry arrays with variable counts
- **FR-028**: System MUST parse SD option arrays linked to entries (Index1, Index2, NumOpt1, NumOpt2)
- **FR-029**: System MUST validate SD entry and option counts
- **FR-030**: System MUST parse SD flags (Reboot, Unicast)
- **FR-031**: System MUST handle SD TTL field (0 = StopOffer/Unsubscribe, 0xFFFFFF = infinite)

#### DoIP Protocol Completion

- **FR-032**: System MUST parse Diagnostic Power Mode messages (0x4003 request, 0x4004 response)
- **FR-033**: System MUST parse DoIP entity status messages (0x4001 request, 0x4002 response)
- **FR-034**: System MUST handle DoIP negative acknowledgment (NACK) codes
- **FR-035**: System MUST validate DoIP payload length field
- **FR-036**: System MUST handle DoIP header NACK (0x0000)
- **FR-037**: System MUST parse DoIP alive check request/response

#### UDS Protocol Completion

- **FR-038**: System MUST parse all UDS negative response codes (0x10-0x93)
- **FR-039**: System MUST provide human-readable NRC descriptions
- **FR-040**: System MUST classify NRCs as temporary or permanent
- **FR-041**: System MUST extract NRC sub-function byte when present
- **FR-042**: System MUST handle service-specific NRC interpretations
- **FR-043**: System MUST parse UDS positive response suppression bit

#### gPTP Protocol Completion

- **FR-044**: System MUST parse Follow_Up Information TLV (type 0x0003)
- **FR-045**: System MUST extract rate ratio from Follow_Up TLV
- **FR-046**: System MUST extract GM time base indicator from Follow_Up TLV
- **FR-047**: System MUST parse Organization Extension TLV (type 0x0003)
- **FR-048**: System MUST handle unknown TLV types gracefully (log warning, skip TLV, continue parsing)
- **FR-049**: System MUST validate TLV length fields

#### Cross-Protocol Validation

- **FR-050**: System MUST validate protocol layering (Ethernet → IPv4 → UDP/TCP → Application)
- **FR-051**: System MUST detect length inconsistencies across protocol layers
- **FR-052**: System MUST validate checksum chain (IPv4, UDP, TCP as applicable)
- **FR-053**: System MUST support validation mode with strict/lenient error handling

### Key Entities

- **Ipv4Options**: Parsed IPv4 header options array
- **Ipv4Fragment**: Fragment identification and reassembly state
- **TcpConnection**: Connection tracking state and metadata
- **TcpOptions**: Parsed TCP header options
- **UdpChecksumValidator**: Checksum validation engine
- **SomeipTpMessage**: TP message reassembly state
- **SdEntryArray**: Parsed SD entries collection
- **SdOptionArray**: Parsed SD options collection
- **DoipPowerMode**: DoIP diagnostic power mode state tracker
- **UdsNegativeResponse**: NRC parsing and classification
- **GptpTlv**: TLV variant type for all gPTP TLVs
- **ProtocolValidator**: Cross-layer validation engine

## Success Criteria

### Measurable Outcomes

- **SC-001**: All IPv4 options types correctly parsed in test suite (8 option types)
- **SC-002**: IPv4 fragment reassembly correctly reconstructs datagrams up to 64KB
- **SC-003**: TCP connection tracking accurately identifies all standard state transitions
- **SC-004**: UDP checksum validation detects 100% of intentionally corrupted packets
- **SC-005**: SOME/IP-TP reassembly handles messages up to 1GB with segmentation
- **SC-006**: SOME/IP-SD parser extracts all entry types and options without error
- **SC-007**: DoIP power mode transitions correctly tracked across all states
- **SC-008**: All 50+ UDS NRC codes correctly identified with descriptions
- **SC-009**: gPTP TLV parsing extracts rate ratio and organization extensions
- **SC-010**: Cross-protocol validation detects length/checksum inconsistencies with 100% accuracy
- **SC-011**: 230+ new unit tests achieve ≥95% code coverage for all protocol enhancements
- **SC-012**: Fuzz testing runs 1M+ iterations per protocol without crashes
- **SC-013**: Performance impact ≤5% compared to baseline (M11) for typical automotive traffic

## Assumptions

- All protocols follow published standards (RFC 791 for IPv4, RFC 793 for TCP, etc.)
- Focus on parsing and validation, not protocol implementation (no TCP stack, no IP routing)
- Reassembly buffers sized for typical automotive use (not arbitrary large attacks)
- gPTP TLVs follow IEEE 1588 and 802.1AS specifications

## Dependencies

- **External**: None (pure protocol parsing)
- **Internal**: M2 (Protocol Decoders), M8 (gPTP), M9 (UDS)

## Out of Scope

- Full TCP stack implementation (connection management, congestion control, window management)
- IPv4 routing or forwarding
- UDP/TCP socket programming
- SOME/IP service instance management
- DoIP gateway routing tables
- UDS diagnostic server implementation
- gPTP clock synchronization algorithm (only parsing, not PTP engine)

## Implementation Notes

### Recommended Approach

**Phase 1 - IPv4 Completion** (1 week)
- Implement IPv4 options parser
- Add fragmentation handling and reassembly
- Add ToS/DSCP parsing
- Tests: 20+ covering all IPv4 features

**Phase 2 - TCP Completion** (1.5 weeks)
- Implement TCP connection tracking
- Parse all TCP options
- Add retransmission detection
- Tests: 30+ covering state machine and options

**Phase 3 - UDP Completion** (0.5 weeks)
- Add UDP checksum validation
- Handle zero checksum case
- Tests: 10+ for checksum validation

**Phase 4 - SOME/IP Completion** (1 week)
- Implement SOME/IP-TP parsing and reassembly
- Complete message type handling
- Tests: 25+ for TP and message types

**Phase 5 - SOME/IP-SD Completion** (1.5 weeks)
- Complete SD entry array parsing
- Complete SD option array parsing
- Add entry-option linking
- Tests: 30+ for all entry/option combinations

**Phase 6 - DoIP Completion** (1 week)
- Add power mode messages
- Add entity status messages
- Complete NACK handling
- Tests: 20+ for all DoIP messages

**Phase 7 - UDS Completion** (1 week)
- Comprehensive NRC parsing
- NRC classification and descriptions
- Service-specific handling
- Tests: 25+ for all NRCs

**Phase 8 - gPTP Completion** (0.5 weeks)
- Complete TLV parsing (Follow_Up Info, Org Extension)
- Add unknown TLV handling
- Tests: 10+ for all TLVs

**Phase 9 - Cross-Protocol Validation** (1 week)
- Implement multi-layer validation
- Add length/checksum chain validation
- Tests: 20+ for validation scenarios

**Phase 10 - Integration & Fuzz Testing** (1 week)
- Update all fuzz harnesses
- Run extended fuzz campaigns (1M+ iterations each)
- Performance profiling and optimization
- Integration tests: 30+ end-to-end scenarios

**Total Duration**: ~10 weeks

### Test Count Target

**230+ tests** covering:
- IPv4: 20 tests (options, fragmentation, ToS, checksum)
- TCP: 30 tests (state machine, options, retransmission)
- UDP: 10 tests (checksum validation)
- SOME/IP: 25 tests (TP, message types)
- SOME/IP-SD: 30 tests (entries, options, linking)
- DoIP: 20 tests (power mode, entity status, NACK)
- UDS: 25 tests (NRCs, classification)
- gPTP: 10 tests (TLVs)
- Cross-validation: 20 tests
- Integration: 30 tests
- Fuzz: 10 harnesses with 1M+ iterations each

### Example IPv4 Options Parsing

```cpp
struct Ipv4Options {
    std::vector<Ipv4Option> options;
    
    struct RouterAlert {
        uint16_t value;  // 0 = Router shall examine packet
    };
    
    struct Timestamp {
        uint8_t pointer;
        uint8_t overflow : 4;
        uint8_t flags : 4;
        std::vector<uint32_t> timestamps;
    };
    
    struct RecordRoute {
        uint8_t pointer;
        std::vector<uint32_t> route_data;
    };
};
```

### Example TCP Connection Tracking

```cpp
enum class TcpState {
    CLOSED, LISTEN, SYN_SENT, SYN_RECEIVED,
    ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2,
    CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT
};

struct TcpConnection {
    TcpState state;
    uint32_t seq_next;
    uint32_t ack_next;
    uint16_t window_size;
    uint8_t window_scale;
    std::chrono::steady_clock::time_point last_seen;
};
```

### Example SOME/IP-TP Reassembly

```cpp
struct SomeipTpReassembler {
    std::map<uint32_t, TpMessage> messages;  // Key: Request ID
    std::chrono::seconds timeout{5};
    
    std::optional<Packet> add_segment(const SomeipHeader& hdr, PacketView data) {
        // Extract offset and more_segments flag
        // Add to reassembly buffer
        // Return complete message if all segments received
    }
};
```

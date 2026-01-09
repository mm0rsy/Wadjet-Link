# Feature Specification: DDS/RTPS Protocol Decoder

**Feature Branch**: `milestone/010-dds-rtps-decoder`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M10 - DDS/RTPS Protocol Decoder

## User Scenarios & Testing

### User Story 1 - Decode DDS Discovery Messages (Priority: P1)

As a robotics/autonomous vehicle engineer, I need to decode DDS (Data Distribution Service) RTPS protocol messages, so that I can monitor and validate publish-subscribe communication between ROS2 nodes and automotive middleware components.

**Why this priority**: DDS is becoming the standard for distributed automotive applications, especially in autonomous driving and ROS2-based systems.

**Independent Test**: Capture a DDS discovery message (SPDP/SEDP), decode it, and verify participant GUID and endpoint information.

**Acceptance Scenarios**:

1. **Given** a DDS RTPS packet on UDP port 7400-7410, **When** I decode it, **Then** I get RTPS header with protocol version, vendor ID, and GUID prefix
2. **Given** an SPDP (Simple Participant Discovery Protocol) message, **When** decoded, **Then** I get participant GUID, locators, and QoS policies
3. **Given** an SEDP (Simple Endpoint Discovery Protocol) message, **When** decoded, **Then** I get reader/writer endpoints with topic names and QoS
4. **Given** a DATA submessage, **When** decoded, **Then** I get sequence number, writer GUID, and serialized payload
5. **Given** an ACKNACK submessage, **When** decoded, **Then** I get reader/writer GUIDs and acknowledged sequence number set

---

### User Story 2 - DDS Topic Monitoring (Priority: P1)

As a DDS application developer, I need to monitor active DDS topics and their publishers/subscribers, so that I can debug discovery issues and validate topic matching.

**Why this priority**: Topic discovery is critical for DDS applications. Mismatched QoS or topic names cause silent failures.

**Independent Test**: Capture DDS traffic from a ROS2 demo application and verify all topics, publishers, and subscribers are discovered correctly.

**Acceptance Scenarios**:

1. **Given** DDS discovery traffic, **When** I analyze it, **Then** I can list all discovered participants with their GUIDs
2. **Given** SEDP messages, **When** analyzed, **Then** I can enumerate all topics being published/subscribed
3. **Given** topic information, **When** displayed, **Then** I see topic name, type name, QoS policies (reliability, durability, history)
4. **Given** endpoint matching, **When** validated, **Then** I can detect QoS mismatches between publishers and subscribers

---

### User Story 3 - RTPS Reliability Protocol Analysis (Priority: P2)

As a DDS performance engineer, I need to analyze RTPS reliability protocol messages (HEARTBEAT, ACKNACK, GAP), so that I can troubleshoot packet loss and retransmission issues.

**Why this priority**: Important for debugging DDS performance issues, but discovery is more critical.

**Independent Test**: Generate packet loss scenario, capture HEARTBEAT/ACKNACK exchanges, and verify retransmission occurs.

**Acceptance Scenarios**:

1. **Given** a HEARTBEAT submessage, **When** decoded, **Then** I get first/last sequence numbers and reader/writer GUIDs
2. **Given** an ACKNACK submessage, **When** decoded, **Then** I get acknowledged sequence number set and NACK bitmap
3. **Given** a GAP submessage, **When** decoded, **Then** I get missing sequence number ranges
4. **Given** reliability traffic, **When** analyzed, **Then** I can calculate retransmission rates and latency

---

### User Story 4 - CDR Payload Deserialization (Priority: P3)

As a DDS data analyst, I need to deserialize CDR (Common Data Representation) payloads from DATA submessages, so that I can inspect actual message content.

**Why this priority**: Nice-to-have for deep analysis, but protocol-level decoding is more critical initially.

**Independent Test**: Decode a DATA submessage with known CDR payload (e.g., ROS2 std_msgs/String) and verify content.

**Acceptance Scenarios**:

1. **Given** a DATA submessage with CDR encapsulation, **When** I decode it, **Then** I get encapsulation kind (CDR_LE, CDR_BE, PL_CDR)
2. **Given** a CDR payload with known schema, **When** deserialized, **Then** I get structured data fields
3. **Given** a PL_CDR (Parameter List) payload, **When** decoded, **Then** I get key-value parameters
4. **Given** nested CDR structures, **When** deserialized, **Then** I can traverse the object graph

---

### User Story 5 - DDS Security Protocol Support (Priority: P3)

As a secure DDS application developer, I need to decode DDS Security submessages (SRTPS), so that I can validate encryption and authentication.

**Why this priority**: Security is important for production systems but basic DDS support comes first.

**Independent Test**: Capture DDS Security handshake and verify authentication submessages are detected.

**Acceptance Scenarios**:

1. **Given** an SRTPS secure payload, **When** detected, **Then** I identify it as encrypted data
2. **Given** a ParticipantVolatileMessageSecure builtin topic, **When** decoded, **Then** I see authentication handshake
3. **Given** DDS Security metadata, **When** analyzed, **Then** I can identify governance and permissions documents
4. **Given** encrypted payloads, **When** encountered, **Then** system gracefully reports "encrypted, cannot decode"

### Edge Cases

- What happens when RTPS packets are fragmented across multiple UDP datagrams?
- How are malformed submessages handled (invalid submessage kind, incorrect length)?
- What if RTPS protocol version is unknown (future DDS-RTPS 3.x)?
- How does the system handle interleaved submessages from different writers in one packet?
- What happens when CDR payload doesn't match declared encapsulation kind?
- How are vendor-specific submessages (non-standard extensions) handled?

## Requirements

### Functional Requirements

#### RTPS Protocol Support

- **FR-001**: System MUST decode RTPS header (protocol version, vendor ID, GUID prefix, flags)
- **FR-002**: System MUST support RTPS 2.3 (DDS-RTPS specification v2.3, OMG standard)
- **FR-003**: System MUST dispatch submessages by submessage ID (DATA, ACKNACK, HEARTBEAT, GAP, INFO_TS, etc.)
- **FR-004**: System MUST validate submessage lengths and endianness flags
- **FR-005**: System MUST support both big-endian and little-endian submessages in same RTPS packet

#### Discovery Protocol

- **FR-006**: System MUST decode SPDP (Simple Participant Discovery Protocol) DATA submessages
- **FR-007**: System MUST extract participant GUID, locators (UDP/TCP), default QoS from SPDP
- **FR-008**: System MUST decode SEDP (Simple Endpoint Discovery Protocol) for reader/writer discovery
- **FR-009**: System MUST extract topic name, type name, QoS policies from SEDP messages
- **FR-010**: System MUST parse builtin endpoint GUIDs (SEDP_BUILTIN_PUBLICATIONS, SEDP_BUILTIN_SUBSCRIPTIONS)

#### Submessage Decoding

- **FR-011**: System MUST decode DATA submessage with writer GUID, sequence number, serialized payload
- **FR-012**: System MUST decode HEARTBEAT submessage with first/last sequence numbers
- **FR-013**: System MUST decode ACKNACK submessage with reader/writer GUIDs and sequence number set
- **FR-014**: System MUST decode GAP submessage indicating missing samples
- **FR-015**: System MUST decode INFO_TS, INFO_DST, INFO_SRC submessages for metadata
- **FR-016**: System MUST handle submessage flags (Endianness, InlineQoS, Data, Key)

#### CDR Payload Support

- **FR-017**: System MUST detect CDR encapsulation kind (CDR_LE, CDR_BE, PL_CDR, CDR2)
- **FR-018**: System MUST provide CDR payload as raw bytes for external deserialization
- **FR-019**: System MUST decode PL_CDR (Parameter List CDR) for discovery data
- **FR-020**: System SHOULD support basic CDR primitive types (string, uint32, etc.) for common messages
- **FR-021**: CDR deserialization errors MUST be reported gracefully without crashing

#### Integration & Testing

- **FR-022**: DDS decoder MUST integrate with ProtocolDispatcher for UDP port-based dispatch (7400-7410 default)
- **FR-023**: System MUST provide gMock matchers: IsDds(), HasRtpsVersion(), HasGuidPrefix(), HasSubmessageKind()
- **FR-024**: System MUST provide DDS-specific matchers: IsSpdpMessage(), IsSedpMessage(), HasTopicName()
- **FR-025**: Python bindings MUST expose RTPS header, submessages, and discovery information
- **FR-026**: Rust bindings MUST support DDS decoding with safe API
- **FR-027**: C ABI layer MUST provide opaque handles for DDS structures
- **FR-028**: System MUST include fuzz testing harness for all submessage types
- **FR-029**: System MUST include regression tests with real ROS2 capture samples
- **FR-030**: Example program (dds_monitor.cpp) MUST monitor participants, topics, and endpoints

### Key Entities

- **RTSPHeader**: Protocol version, vendor ID, GUID prefix, flags
- **Submessage**: Base type with variants (DATA, HEARTBEAT, ACKNACK, GAP, INFO_TS, INFO_DST, INFO_SRC, SPDP, SEDP)
- **GUID**: Globally Unique Identifier (GUID prefix + entity ID)
- **SequenceNumber**: 64-bit sequence number for reliable delivery
- **Locator**: Network address (UDP/TCP IP:port)
- **ParticipantData**: SPDP discovery information (GUID, locators, QoS)
- **EndpointData**: SEDP discovery information (topic, type, QoS, GUID)
- **QoSPolicy**: Reliability, Durability, History, Deadline, etc.
- **CDRPayload**: Serialized data with encapsulation kind
- **ParameterList**: Key-value parameters in PL_CDR format

## Success Criteria

### Measurable Outcomes

- **SC-001**: All RTPS submessage types (12+ types) decode correctly with unit tests
- **SC-002**: SPDP and SEDP messages from ROS2 demo applications decode with 100% accuracy
- **SC-003**: DDS monitor example successfully lists all participants, topics, publishers, subscribers from live ROS2 traffic
- **SC-004**: Topic name, type name, and QoS policies extracted correctly from SEDP messages
- **SC-005**: RTPS reliability protocol (HEARTBEAT, ACKNACK, GAP) decodes and calculates retransmission statistics
- **SC-006**: Fuzz testing runs for 1 million inputs without crashes
- **SC-007**: Integration with existing Wadjet-Link infrastructure (matchers, Python/Rust bindings, scenarios) complete
- **SC-008**: All DDS decoder tests pass on both little-endian and big-endian submessages
- **SC-009**: Example program demonstrates participant lifecycle (SPDP announce, SEDP publish, DATA exchange, participant disposal)
- **SC-010**: Documentation includes DDS-RTPS protocol overview, OMG specification references, ROS2 integration guide

## Assumptions

- DDS-RTPS 2.3 is the target specification (OMG standard)
- Most automotive DDS deployments use UDP multicast (239.255.0.1:7400 default)
- ROS2 is a primary use case (Fast-DDS, CycloneDDS compatibility)
- CDR deserialization is limited to discovery messages initially (full IDL support is future work)
- DDS Security (SRTPS) detection only; decryption requires keys (out of scope initially)

## Dependencies

- **External**: None (DDS-RTPS is self-contained protocol)
- **Internal**: M0-M5 (infrastructure, decoders, matchers, bindings), M2 (UDP decoder for port-based dispatch)

## Out of Scope

- Full IDL (Interface Definition Language) compiler integration
- Complete CDR deserialization for all data types (focus on discovery messages)
- DDS Security decryption (detection only; keys not available)
- RTI DDS, OpenDDS vendor-specific extensions (focus on OMG standard + ROS2)
- RTPS-over-TCP transport (UDP only initially)
- DDS Discovery Server protocol (focus on Simple Discovery)
- Quality of Service contract monitoring (just parsing QoS, not enforcing)
- DDS application code generation

## Implementation Notes

### Recommended Approach

1. **Phase 1 - RTPS Header & Submessage Framework**
   - Implement RTPS header parser (protocol version, vendor ID, GUID prefix)
   - Create submessage dispatcher (similar to ProtocolDispatcher)
   - Implement submessage length validation and endianness handling

2. **Phase 2 - Discovery Protocol (SPDP/SEDP)**
   - Decode SPDP DATA submessages with participant information
   - Decode SEDP DATA submessages with endpoint information
   - Parse PL_CDR parameter lists for QoS policies
   - Extract topic names, type names, locators

3. **Phase 3 - Reliability Protocol**
   - Decode HEARTBEAT, ACKNACK, GAP submessages
   - Implement sequence number set parsing (bitmap)
   - Calculate retransmission statistics

4. **Phase 4 - Integration & Testing**
   - Integrate with ProtocolDispatcher (UDP port 7400-7410)
   - Create gMock matchers for DDS testing
   - Add Python/Rust bindings
   - Develop dds_monitor.cpp example
   - Write comprehensive tests (unit, integration, fuzz, regression)

### Reference Implementations

- **Fast-DDS** (eProsima): ROS2 default DDS implementation
- **CycloneDDS** (Eclipse): Alternative ROS2 DDS
- **Wireshark DDS dissector**: Reference for RTPS decoding

### Documentation References

- OMG DDS-RTPS 2.3 Specification
- ROS2 DDS integration guide
- Fast-DDS documentation
- CycloneDDS documentation

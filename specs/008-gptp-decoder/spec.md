# Feature Specification: gPTP Protocol Decoder (IEEE 802.1AS)

**Feature Branch**: `milestone/008-gptp-decoder`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M8 - gPTP Protocol Decoder

## User Scenarios & Testing

### User Story 1 - Decode gPTP Messages (Priority: P1)

As a TSN timing engineer, I need to decode gPTP messages (Sync, Follow_Up, Pdelay), so that I can analyze time synchronization between ECUs.

**Acceptance Scenarios**:

1. **Given** a gPTP Sync packet, **When** I decode it, **Then** I get message type, sequence ID, clock identity
2. **Given** a Follow_Up message, **When** decoded, **Then** I get precise origin timestamp
3. **Given** Pdelay messages, **When** decoded, **Then** I can calculate path delay

### User Story 2 - gPTP Matchers (Priority: P1)

As a gPTP test engineer, I need GoogleTest matchers for gPTP, so that I can assert on time synchronization behavior.

**Acceptance Scenarios**:

1. **Given** EXPECT_THAT(packet, IsGptpSync()), **When** applied to gPTP packet, **Then** it validates message type
2. **Given** HasGptpDomain(0) matcher, **When** used, **Then** it checks domain number field

## Requirements

### Functional Requirements

- **FR-001**: gPTP decoder for all message types (Sync, Follow_Up, Pdelay_Req, Pdelay_Resp, Announce, Signaling)
- **FR-002**: TLV parsing for Follow-Up Information with rate ratio
- **FR-003**: 15 gMock matchers (IsGptp, HasGptpMessageType, IsGptpSync, etc.)
- **FR-004**: Integration with ProtocolDispatcher for EtherType 0x88F7
- **FR-005**: Fuzz testing harness
- **FR-006**: Python bindings (GptpHeader, MessageType enums)
- **FR-007**: Rust bindings
- **FR-008**: C ABI layer
- **FR-009**: Example program (gptp_monitor.cpp)
- **FR-010**: Scenario test (gptp_sync_test.yaml)

## Success Criteria

- **SC-001**: All gPTP message types decode correctly (13 unit tests pass)
- **SC-002**: Fuzz testing runs without crashes
- **SC-003**: Example monitors grandmaster changes and clock drift

## Dependencies

- **Internal**: M0-M5 (decoders, matchers, bindings)

## Out of Scope

- gPTP timing calculations (clock offset, rate ratio math)
- Grand master clock selection algorithm

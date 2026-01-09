# Feature Specification: UDS over IP Protocol Decoder

**Feature Branch**: `milestone/009-uds-decoder`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M9 - UDS over IP Protocol Decoder

## User Scenarios & Testing

### User Story 1 - Decode UDS Services (Priority: P1)

As a diagnostic engineer, I need to decode UDS (ISO 14229) diagnostic services, so that I can validate ECU diagnostic communication.

**Acceptance Scenarios**:

1. **Given** a UDS DiagnosticSessionControl packet, **When** decoded, **Then** I get service ID and session type
2. **Given** a UDS ReadDataByIdentifier request, **When** decoded, **Then** I get the DID being requested
3. **Given** a UDS negative response, **When** decoded, **Then** I get NRC (Negative Response Code) with description

### User Story 2 - UDS Test Support (Priority: P1)

As a UDS validation engineer, I need matchers and example programs for UDS testing, so that I can validate diagnostic sequences.

**Acceptance Scenarios**:

1. **Given** UDS monitor example, **When** run on diagnostic traffic, **Then** it classifies services and detects errors
2. **Given** UDS validator example, **When** run, **Then** it validates P2/P2* timing and security access flows

## Requirements

### Functional Requirements

- **FR-001**: UDS decoder for 20+ services (0x10-0x3E service IDs)
- **FR-002**: NRC decoder with all ISO 14229 negative response codes
- **FR-003**: Service-specific structures (DiagnosticSessionControl, SecurityAccess, ReadDataByIdentifier, etc.)
- **FR-004**: DID (Data Identifier) encoding/decoding
- **FR-005**: Integration with DoIP decoder (UDS payload extraction)
- **FR-006**: Scenario expectations (UdsExpect in YAML/JSON)
- **FR-007**: Python bindings (UDS service enums, header classes)
- **FR-008**: Rust bindings
- **FR-009**: C ABI layer
- **FR-010**: Fuzz testing for all services
- **FR-011**: Unit tests (20+ services, NRC parsing)
- **FR-012**: Integration tests (DoIP + UDS combined decode)
- **FR-013**: Property-based tests (UDSBuilder for random messages)
- **FR-014**: Regression tests with real diagnostic captures
- **FR-015**: Examples (uds_monitor.cpp, uds_validator.cpp, uds_analysis.py)
- **FR-016**: Documentation (docs/protocols/uds.md with ISO 14229 overview)

## Success Criteria

- **SC-001**: All UDS services (20+) decode correctly
- **SC-002**: NRC messages include human-readable descriptions
- **SC-003**: UDS monitor classifies services with statistics
- **SC-004**: Validator checks timing and security constraints
- **SC-005**: Fuzz testing runs without crashes

## Dependencies

- **Internal**: M2 (DoIP decoder extracts UDS payload), M0-M5 (infrastructure)

## Out of Scope

- Stateful session tracking (session timeouts, active diagnostics state)
- OEM-specific UDS extensions beyond ISO 14229
- DID database population (framework exists, no default database)

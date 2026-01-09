# Feature Specification: ODX/PDX Diagnostic Database Support

**Feature Branch**: `milestone/016-odx-pdx-database`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M16 - ODX/PDX Diagnostic Database

## Overview

Implement ODX (Open Diagnostic Data Exchange) and PDX parser for automotive diagnostic database support. ODX/PDX files contain comprehensive diagnostic service definitions, including request/response structures, parameters, data types, and diagnostic trouble codes (DTCs). This enables automated UDS validation and human-readable diagnostic interpretation.

## User Scenarios & Testing

### User Story 1 - UDS Service Decode with ODX (Priority: P1)

As a diagnostic engineer, I need to decode UDS services using ODX definitions, so that I can interpret request/response parameters automatically.

**Why this priority**: ODX provides the schema for UDS messages - without it, UDS decode is limited to raw bytes.

**Independent Test**: Load ODX file and decode UDS DiagnosticSessionControl request/response.

**Acceptance Scenarios**:

1. **Given** UDS request (0x10 DiagnosticSessionControl), **When** decoded with ODX, **Then** session type parameter is extracted and named (e.g., "Programming Session")
2. **Given** UDS positive response (0x50), **When** decoded, **Then** response parameters (session parameter record, timing values P2, P2*) are extracted
3. **Given** UDS negative response (0x7F), **When** decoded with ODX, **Then** NRC description from ODX is provided
4. **Given** service with multiple parameters, **When** parsed, **Then** all parameters are decoded according to ODX data types
5. **Given** ODX-defined data constraints, **When** validated, **Then** out-of-range parameter values are flagged

### User Story 2 - DTC Information Lookup (Priority: P1)

As a service technician, I need DTC (Diagnostic Trouble Code) descriptions from ODX, so that I can understand fault codes without manual lookups.

**Why this priority**: DTC interpretation is critical for diagnostics - ODX provides human-readable descriptions.

**Independent Test**: Query DTC from ODX database and retrieve description.

**Acceptance Scenarios**:

1. **Given** DTC code (e.g., P0300), **When** queried in ODX, **Then** DTC description is returned (e.g., "Random/Multiple Cylinder Misfire Detected")
2. **Given** DTC with severity levels, **When** looked up, **Then** severity (informational, warning, critical) is provided
3. **Given** DTC with environmental data, **When** extracted, **Then** freeze frame parameters are identified
4. **Given** DTC status byte, **When** decoded with ODX, **Then** bit meanings (test failed, pending, confirmed, etc.) are explained
5. **Given** proprietary DTCs (manufacturer-specific), **When** encountered, **Then** ODX provides custom descriptions

### User Story 3 - ReadDataByIdentifier (DID) Decode (Priority: P1)

As a calibration engineer, I need to decode DIDs using ODX, so that I can extract signal values from read responses.

**Why this priority**: DID decode is fundamental for parameter reading - ODX defines DID structure.

**Independent Test**: Read DID 0xF190 (VIN) and decode using ODX.

**Acceptance Scenarios**:

1. **Given** ReadDataByIdentifier request (0x22 DID), **When** decoded with ODX, **Then** DID name is resolved (e.g., "Vehicle Identification Number")
2. **Given** positive response with DID data, **When** decoded, **Then** data elements are extracted according to ODX structure
3. **Given** DID with multiple parameters, **When** parsed, **Then** each parameter is extracted with correct byte position and length
4. **Given** DID with scaling, **When** decoded, **Then** physical values are calculated (raw value × scale + offset)
5. **Given** DID with enumeration, **When** decoded, **Then** enum value is mapped to text (e.g., 0x01 → "Active")

### User Story 4 - RoutineControl Decode (Priority: P2)

As a diagnostic validator, I need to decode RoutineControl services using ODX, so that I can validate routine parameters.

**Why this priority**: Routines have complex parameters - ODX provides parameter structure.

**Independent Test**: Decode RoutineControl start request with parameters.

**Acceptance Scenarios**:

1. **Given** RoutineControl request (0x31), **When** decoded with ODX, **Then** routine ID and control type (start/stop/result) are identified
2. **Given** routine with input parameters, **When** parsed, **Then** parameter values are extracted and named
3. **Given** routine response, **When** decoded, **Then** output parameters (status, result codes) are extracted
4. **Given** routine state machine, **When** tracked with ODX, **Then** valid state transitions are validated
5. **Given** routine with complex data types (structs, arrays), **When** decoded, **Then** nested structures are parsed correctly

### User Story 5 - Session and Security Context (Priority: P2)

As a security analyst, I need ODX session and security level definitions, so that I can validate diagnostic access control.

**Why this priority**: Security access controls diagnostic services - ODX defines allowed sessions and security levels.

**Independent Test**: Validate service execution requirements from ODX.

**Acceptance Scenarios**:

1. **Given** diagnostic service in ODX, **When** queried, **Then** required session type is identified (default, programming, extended)
2. **Given** service with security requirement, **When** checked, **Then** required security level is specified
3. **Given** service execution in wrong session, **When** detected, **Then** NRC 0x7F (Service Not Supported In Active Session) is expected
4. **Given** security access sequence, **When** validated with ODX, **Then** seed/key algorithm parameters are available
5. **Given** address/length format identifiers, **When** decoded, **Then** ODX provides memory addressing schemes

## Edge Cases

- What happens when ODX file references missing or external files (imports)?
- How are ODX version differences handled (2.0.1 vs 2.2.0)?
- What if DID definitions conflict across multiple ECUs in same ODX?
- How does system handle proprietary ODX extensions from tool vendors?
- What happens when UDS service doesn't have ODX definition (fallback behavior)?
- How are circular references in ODX data structures handled?

## Requirements

### Functional Requirements

#### Core ODX Parsing

- **FR-001**: System MUST parse ODX 2.x files (ISO 22901-1, focus on 2.0.1 and 2.2.0)
- **FR-002**: System MUST parse PDX (Packed ODX) container files
- **FR-003**: System MUST support multi-file ODX projects (imports, includes)
- **FR-004**: System MUST validate ODX against schema (XSD validation optional)
- **FR-005**: System MUST handle large ODX files (up to 50MB) efficiently
- **FR-006**: System MUST use pugixml for XML parsing

#### Diagnostic Service Definitions

- **FR-007**: System MUST parse DIAG-SERVICE elements (service ID, name, semantic)
- **FR-008**: System MUST parse REQUEST and RESPONSE structures (POS-RESPONSE, NEG-RESPONSE)
- **FR-009**: System MUST extract PARAMs (parameters) with data types and positions
- **FR-010**: System MUST parse PARAM-LENGTH-INFO (fixed, variable, key-based)
- **FR-011**: System MUST handle all UDS services (0x10-0x3E, 0x85, 0x86, 0x87, etc.)
- **FR-012**: System MUST extract session requirements (DEFAULT-SESSION, PROGRAMMING-SESSION, etc.)
- **FR-013**: System MUST extract security level requirements

#### Data Type System

- **FR-014**: System MUST parse DIAG-DATA-DICTIONARY-SPEC (data types)
- **FR-015**: System MUST handle all ODX data types (uint8, uint16, uint32, int8, int16, int32, float, double, string, bytefield)
- **FR-016**: System MUST parse PHYS-CONST (constant physical values)
- **FR-017**: System MUST parse LINEAR-COMPU-METHOD (scale + offset transformations)
- **FR-018**: System MUST parse SCALE-LINEAR-COMPU-METHOD and RAT-FUNC-COMPU-METHOD (rational functions)
- **FR-019**: System MUST parse TEXT-TABLE-COMPU-METHOD (enumeration mappings)
- **FR-020**: System MUST handle byte ordering (big-endian, little-endian)

#### DTC Definitions

- **FR-021**: System MUST parse DTC-DOPS (DTC definitions)
- **FR-022**: System MUST extract DTC codes (P, C, B, U codes)
- **FR-023**: System MUST extract DTC text descriptions
- **FR-024**: System MUST extract DTC severity levels
- **FR-025**: System MUST parse environmental data (freeze frame) definitions
- **FR-026**: System MUST parse DTC status byte bit definitions

#### DID Definitions

- **FR-027**: System MUST parse DATA-OBJECT-PROPS (DID definitions)
- **FR-028**: System MUST extract DID identifiers (0x0000-0xFFFF)
- **FR-029**: System MUST extract DID names and descriptions
- **FR-030**: System MUST parse DID data structures (parameters, byte positions)
- **FR-031**: System MUST handle dynamically defined DIDs

#### Routine Definitions

- **FR-032**: System MUST parse DIAG-SERVICE for RoutineControl (0x31)
- **FR-033**: System MUST extract routine identifiers
- **FR-034**: System MUST parse routine parameters (input, output)
- **FR-035**: System MUST extract routine state machines (start → stop → result)

#### Integration & API

- **FR-036**: System MUST provide OdxDatabase C++ API for querying
- **FR-037**: System MUST support Python bindings for ODX access
- **FR-038**: UDS decoder MUST integrate with OdxDatabase for automatic decode
- **FR-039**: CLI tool (wadjet-odx) MUST support ODX inspection and DID/DTC lookup
- **FR-040**: Scenario tests MUST support ODX-based service validation

### Key Entities

- **OdxDatabase**: Main container for parsed ODX data
- **DiagService**: UDS service definition (request/response structure)
- **DiagParam**: Service parameter with data type and position
- **CompuMethod**: Data transformation (linear, rational, text-table)
- **DataType**: ODX data type (primitive or complex)
- **DtcDefinition**: DTC code with description and severity
- **DidDefinition**: DID structure with parameters
- **RoutineDefinition**: Routine control parameters and state machine
- **SessionContext**: Required session type for service execution
- **SecurityContext**: Required security level for service

## Success Criteria

### Measurable Outcomes

- **SC-001**: Parser correctly loads ODX 2.0.1 and 2.2.0 files from real automotive projects
- **SC-002**: 100% of standard UDS services (0x10-0x3E) correctly parsed with parameters
- **SC-003**: DTC lookup returns correct descriptions for all standardized DTCs (P0xxx, C0xxx, B0xxx, U0xxx)
- **SC-004**: DID decode extracts all parameters with correct byte positions and scaling
- **SC-005**: RoutineControl parameters correctly parsed for complex data structures
- **SC-006**: UDS decoder + OdxDatabase integration provides human-readable output for all decoded services
- **SC-007**: Parser handles 50MB ODX file in ≤60 seconds
- **SC-008**: CLI tool (`wadjet-odx`) successfully looks up DIDs and DTCs
- **SC-009**: Python bindings enable ODX querying for automated test script generation
- **SC-010**: 50+ unit tests cover all ODX element types and data transformations

## Assumptions

- ODX files are valid (well-formed XML conforming to ODX schema)
- Focus on diagnostic services over IP (UDS over DoIP)
- Vendor-specific extensions are logged but may not be fully supported
- PDX container format follows standard ZIP-based structure
- Focus on automotive ODX (not generic diagnostic protocols)

## Dependencies

- **External**: pugixml (XML parser), optional: libzip (PDX support)
- **Internal**: M9 (UDS decoder)

## Out of Scope

- ODX authoring or editing tools (read-only parser)
- Full ODX 2.2.0 feature support (focus on commonly used subset)
- Diagnostic server implementation (only parsing definitions)
- Security seed/key algorithm implementation (only metadata extraction)
- Flash programming sequence execution (only definition parsing)
- ODX comparison or diff tools
- CANdela/CDX format support (only ODX/PDX)

## Implementation Notes

### Recommended Approach

**Phase 1 - Core ODX XML Parsing** (1 week)
- Set up pugixml integration
- Parse ODXLINK, CONTAINER elements
- Handle multi-file imports
- Tests: 10+ for XML loading

**Phase 2 - Data Type System** (1.5 weeks)
- Parse DIAG-DATA-DICTIONARY-SPEC
- Implement all data types (uint, int, float, string, bytefield)
- Parse COMPU-METHOD (linear, rational, text-table)
- Tests: 20+ for data types and transformations

**Phase 3 - Diagnostic Services** (2 weeks)
- Parse DIAG-SERVICE elements
- Extract REQUEST/POS-RESPONSE/NEG-RESPONSE
- Parse PARAMs with positions
- Handle session and security contexts
- Tests: 25+ for all UDS services

**Phase 4 - DTC Definitions** (1 week)
- Parse DTC-DOPS
- Extract DTC codes and descriptions
- Parse environmental data
- Tests: 12+ for DTC lookup

**Phase 5 - DID Definitions** (1 week)
- Parse DATA-OBJECT-PROPS
- Extract DID structures
- Handle dynamic DIDs
- Tests: 15+ for DID decode

**Phase 6 - Routine Definitions** (0.5 weeks)
- Parse RoutineControl services
- Extract parameters and state machines
- Tests: 8+ for routines

**Phase 7 - Integration** (1 week)
- Create OdxDatabase API
- Integrate with UDS decoder
- CLI tool (wadjet-odx)
- Python bindings
- Tests: 12+ for integration

**Total Duration**: ~8 weeks

### ODX Element Hierarchy Example

```xml
<ODX>
  <DIAG-LAYER-CONTAINER>
    <BASE-VARIANT>
      <DIAG-DATA-DICTIONARY-SPEC>
        <DATA-OBJECT-PROPS>
          <SHORT-NAME>DID_VIN</SHORT-NAME>
          <LONG-NAME>Vehicle Identification Number</LONG-NAME>
          <ID>0xF190</ID>
          <COMPU-METHOD>
            <CATEGORY>TEXTTABLE</CATEGORY>
            <COMPU-PHYS-TO-INTERNAL-SCALE>
              <COMPU-SCALES>
                <COMPU-SCALE>
                  <LOWER-LIMIT>0</LOWER-LIMIT>
                  <UPPER-LIMIT>255</UPPER-LIMIT>
                  <COMPU-CONST>
                    <V>ASCII</V>
                  </COMPU-CONST>
                </COMPU-SCALE>
              </COMPU-SCALES>
            </COMPU-PHYS-TO-INTERNAL-SCALE>
          </COMPU-METHOD>
        </DATA-OBJECT-PROPS>
      </DIAG-DATA-DICTIONARY-SPEC>
      <DIAG-COMMS>
        <DIAG-SERVICE>
          <SHORT-NAME>ReadDataByIdentifier</SHORT-NAME>
          <SERVICE-ID>0x22</SERVICE-ID>
          <REQUEST>
            <PARAMS>
              <PARAM>
                <SHORT-NAME>DataIdentifier</SHORT-NAME>
                <BYTE-POSITION>1</BYTE-POSITION>
                <DOP-REF ID-REF="DID_VIN"/>
              </PARAM>
            </PARAMS>
          </REQUEST>
          <POS-RESPONSE>
            <PARAMS>
              <PARAM>
                <SHORT-NAME>DataRecord</SHORT-NAME>
                <BYTE-POSITION>2</BYTE-POSITION>
              </PARAM>
            </PARAMS>
          </POS-RESPONSE>
        </DIAG-SERVICE>
      </DIAG-COMMS>
    </BASE-VARIANT>
  </DIAG-LAYER-CONTAINER>
</ODX>
```

### OdxDatabase API Example

```cpp
class OdxDatabase {
public:
    // Load ODX/PDX file
    void load(const std::filesystem::path& odx_file);
    
    // Query services
    std::optional<DiagService> find_service(uint8_t sid) const;
    std::vector<DiagService> get_all_services() const;
    
    // Query DTCs
    std::optional<DtcDefinition> find_dtc(uint32_t dtc_code) const;
    std::string get_dtc_description(uint32_t dtc_code) const;
    
    // Query DIDs
    std::optional<DidDefinition> find_did(uint16_t did) const;
    std::string get_did_name(uint16_t did) const;
    
    // Decode UDS message with ODX
    DecodedUdsMessage decode_uds(const UdsMessage& msg) const;
};
```

### CLI Tool Usage

```bash
# Inspect ODX file
wadjet-odx inspect ecu_diagnostics.odx

# Lookup DTC
wadjet-odx dtc P0300 --odx ecu_diagnostics.odx

# Lookup DID
wadjet-odx did 0xF190 --odx ecu_diagnostics.odx

# List all services
wadjet-odx services --odx ecu_diagnostics.odx

# Decode UDS message
wadjet-odx decode --odx ecu.odx --uds "22 F1 90"
```

### Test Count Target

**50+ tests** covering:
- Core XML parsing: 10 tests
- Data type system: 20 tests
- Diagnostic services: 25 tests
- DTC definitions: 12 tests
- DID definitions: 15 tests
- Routine definitions: 8 tests
- Integration: 12 tests

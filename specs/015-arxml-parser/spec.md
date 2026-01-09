# Feature Specification: ARXML Parser (AUTOSAR System Description)

**Feature Branch**: `milestone/015-arxml-parser`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M15 - ARXML Parser

## Overview

Implement AUTOSAR XML (ARXML) parser to extract system configuration data from AUTOSAR system descriptions. ARXML files define ECU software components, communication matrices, and network topologies. Parsing ARXML enables automated test generation and configuration-aware validation.

## User Scenarios & Testing

### User Story 1 - SOME/IP Service Configuration Import (Priority: P1)

As a SOME/IP test engineer, I need to import service definitions from ARXML, so that I can validate service IDs, method IDs, and event IDs match system description.

**Why this priority**: SOME/IP configuration is defined in ARXML - without it, manual test config is error-prone.

**Independent Test**: Load ARXML with SOME/IP service definitions and extract service metadata.

**Acceptance Scenarios**:

1. **Given** ARXML with ServiceInterface definitions, **When** parsed, **Then** service IDs, method IDs, event IDs are extracted
2. **Given** SOME/IP service deployments, **When** loaded, **Then** TCP/UDP port assignments are identified
3. **Given** serialization configurations, **When** parsed, **Then** byte order (big/little endian) is detected
4. **Given** SD configuration, **When** extracted, **Then** initial delay, repetition, TTL are available
5. **Given** service versioning, **When** parsed, **Then** major and minor versions are validated

### User Story 2 - Signal Database Import (Priority: P1)

As a signal-level analyst, I need to import signal definitions from ARXML, so that I can decode signals from raw network traffic.

**Why this priority**: Signal definitions are the foundation for signal-level analysis - critical for ARXML-driven decoding.

**Independent Test**: Load ARXML with signal definitions and map to network messages.

**Acceptance Scenarios**:

1. **Given** ARXML with System Signal definitions, **When** parsed, **Then** signal names, units, and ranges are extracted
2. **Given** signal-to-PDU mappings, **When** loaded, **Then** byte position and bit position are identified
3. **Given** signal transformations, **When** parsed, **Then** scale, offset, and ComPuMethod are extracted
4. **Given** signal groups, **When** processed, **Then** related signals are associated
5. **Given** multiplexed signals, **When** detected, **Then** selector signals and conditional mappings are identified

### User Story 3 - ECU Instance Extraction (Priority: P2)

As a system architect, I need ECU instance information from ARXML, so that I can understand ECU roles and network connectivity.

**Why this priority**: ECU metadata provides context for traffic analysis and test scenario generation.

**Independent Test**: Parse ARXML and extract ECU instance configuration.

**Acceptance Scenarios**:

1. **Given** ARXML with EcuInstance definitions, **When** parsed, **Then** ECU names and types are extracted
2. **Given** ECU communication controllers, **When** loaded, **Then** assigned Ethernet controllers are identified
3. **Given** ECU software components, **When** extracted, **Then** runnable entities and port interfaces are available
4. **Given** ECU addressing, **When** parsed, **Then** IPv4 addresses and MAC addresses are associated with ECUs
5. **Given** ECU diagnostic addresses, **When** loaded, **Then** DoIP logical addresses are mapped to ECUs

### User Story 4 - PDU and Frame Mapping (Priority: P1)

As a network engineer, I need PDU-to-frame mappings from ARXML, so that I can identify which frames contain which data elements.

**Why this priority**: PDU mapping enables frame-level to signal-level traceability.

**Independent Test**: Load ARXML and trace signal → PDU → frame → Ethernet packet path.

**Acceptance Scenarios**:

1. **Given** ARXML with PDU definitions, **When** parsed, **Then** PDU names, IDs, and lengths are extracted
2. **Given** PDU-to-frame mappings, **When** loaded, **Then** container frames are identified
3. **Given** I-PDU configurations, **When** processed, **Then** payload byte positions within frames are known
4. **Given** multiplexed PDUs, **When** detected, **Then** selector field values and PDU variants are identified
5. **Given** secured PDUs, **When** encountered, **Then** SecOC configuration is noted (parsing SecOC data is out of scope)

### User Story 5 - Communication Matrix Generation (Priority: P2)

As a validation engineer, I need to generate communication matrix from ARXML, so that I can understand ECU-to-ECU communication patterns.

**Why this priority**: Communication matrix drives test coverage - ensures all ECU interactions are validated.

**Independent Test**: Parse ARXML and generate ECU-to-ECU communication matrix.

**Acceptance Scenarios**:

1. **Given** ARXML system description, **When** analyzed, **Then** communication matrix shows sender and receiver ECUs for each signal/PDU
2. **Given** socket connections, **When** extracted, **Then** TCP/UDP connections between ECUs are mapped
3. **Given** service discovery configuration, **When** parsed, **Then** service provider and consumer relationships are identified
4. **Given** EventGroup subscriptions, **When** detected, **Then** multicast group memberships are extracted
5. **Given** communication matrix export, **When** generated, **Then** output is in CSV or Excel format

## Edge Cases

- What happens when ARXML references missing or external files (fragments)?
- How are AUTOSAR version differences handled (4.2.2 vs 4.3.1 vs 4.4.0)?
- What if signal definitions have inconsistent units or ranges?
- How does parser handle circular references in ARXML structure?
- What happens when PDU mappings are ambiguous or overlapping?
- How are vendor-specific ARXML extensions processed?

## Requirements

### Functional Requirements

#### Core ARXML Parsing

- **FR-001**: System MUST parse AUTOSAR 4.x ARXML files (4.2.2, 4.3.1, 4.4.0 and later)
- **FR-002**: System MUST support multi-file ARXML projects (AR-PACKAGE references)
- **FR-003**: System MUST validate ARXML against AUTOSAR schema (XSD validation optional)
- **FR-004**: System MUST handle large ARXML files (up to 100MB) efficiently
- **FR-005**: System MUST report parsing errors with file, line number, and element path
- **FR-006**: System MUST use pugixml library for XML parsing (fast, low-memory, header-only)

#### SOME/IP Configuration Extraction

- **FR-007**: System MUST extract ServiceInterface definitions (service ID, methods, events, fields)
- **FR-008**: System MUST extract SomeipServiceInstanceToMachineMapping (IP, port, protocol)
- **FR-009**: System MUST extract SomeipServiceDiscovery configuration (delays, TTL, multicast)
- **FR-010**: System MUST extract serialization properties (byte order, alignment)
- **FR-011**: System MUST extract SOME/IP versioning (major, minor)
- **FR-012**: System MUST map method/event IDs to human-readable names

#### Signal Database Extraction

- **FR-013**: System MUST extract SystemSignal definitions (name, unit, range, init value)
- **FR-014**: System MUST extract signal-to-ISignal mappings (PDU signal representations)
- **FR-015**: System MUST extract ISignalToIPduMapping (byte position, bit position, update indication)
- **FR-016**: System MUST extract ComPuMethod transformations (linear, scale-table, text-table)
- **FR-017**: System MUST extract signal groups (related signals)
- **FR-018**: System MUST extract multiplexed signal configurations (selector signals)
- **FR-019**: System MUST handle both big-endian and little-endian signal byte orders

#### ECU and Network Topology

- **FR-020**: System MUST extract EcuInstance definitions (name, type)
- **FR-021**: System MUST extract EthCtrlConfig (MAC address, VLAN memberships)
- **FR-022**: System MUST extract SocketAddress (IPv4, port) assignments to ECUs
- **FR-023**: System MUST extract DoIP logical addresses mapped to ECUs
- **FR-024**: System MUST extract SoftwareComposition (components, ports, runnables)

#### PDU and Frame Mapping

- **FR-025**: System MUST extract IPdu definitions (name, ID, length)
- **FR-026**: System MUST extract Frame definitions (Ethernet, CAN FD - focus on Ethernet)
- **FR-027**: System MUST extract IPduToFrameMapping (position within frame)
- **FR-028**: System MUST extract PduToFrameTrigger mappings
- **FR-029**: System MUST handle SecuredIPdu references (without parsing SecOC data)

#### Communication Matrix & Analysis

- **FR-030**: System MUST generate communication matrix (sender ECU, receiver ECU, signal/PDU)
- **FR-031**: System MUST identify all socket connections (TCP, UDP) between ECUs
- **FR-032**: System MUST extract service provider-consumer relationships
- **FR-033**: System MUST identify multicast group memberships
- **FR-034**: System MUST export communication matrix to CSV format

#### Integration & API

- **FR-035**: System MUST provide C++ API for ARXML querying (ArxmlDatabase class)
- **FR-036**: System MUST support Python bindings for ARXML parsing
- **FR-037**: CLI tool (wadjet-arxml) MUST support ARXML inspection and export
- **FR-038**: Scenario tests MUST support ARXML-based configuration loading
- **FR-039**: System MUST cache parsed ARXML to avoid re-parsing (optional optimization)

### Key Entities

- **ArxmlDatabase**: Main container for parsed ARXML data
- **ServiceInterface**: SOME/IP service definition
- **SomeipServiceInstance**: Service deployment with IP/port
- **SystemSignal**: Signal definition with metadata
- **ISignalToIPduMapping**: Signal position in PDU
- **ComPuMethod**: Signal transformation (scale/offset/tables)
- **EcuInstance**: ECU configuration and addressing
- **IPdu**: Protocol Data Unit definition
- **EthFrame**: Ethernet frame definition
- **CommunicationMatrix**: Sender-receiver relationship map

## Success Criteria

### Measurable Outcomes

- **SC-001**: Parser correctly loads AUTOSAR 4.2.2, 4.3.1, and 4.4.0 ARXML files
- **SC-002**: 100% of SOME/IP service IDs, method IDs, event IDs extracted from sample ARXML
- **SC-003**: Signal database extraction includes all required metadata (position, size, byte order, ComPuMethod)
- **SC-004**: ECU instance extraction correctly identifies all ECUs with IP/MAC addresses
- **SC-005**: PDU-to-frame mapping accurately traces signal path through network stack
- **SC-006**: Communication matrix generation correctly identifies all sender-receiver pairs
- **SC-007**: Parser handles 100MB ARXML file in ≤30 seconds
- **SC-008**: CLI tool (`wadjet-arxml`) successfully exports communication matrix from real automotive ARXML
- **SC-009**: Python bindings enable ARXML querying from Jupyter notebooks
- **SC-010**: 40+ unit tests cover all ARXML element types

## Assumptions

- ARXML files are valid (well-formed XML conforming to AUTOSAR schema)
- Focus on Ethernet communication (CAN/CAN FD parsing is secondary)
- Vendor-specific extensions are ignored or logged as warnings
- ARXML fragments (multi-file projects) use standard AR-PACKAGE reference mechanism
- Target AUTOSAR Adaptive and Classic platforms (Ethernet-centric)

## Dependencies

- **External**: pugixml library (header-only XML parser), AUTOSAR 4.x XSD schemas (optional validation)
- **Internal**: None (standalone parser module)

## Out of Scope

- ARXML authoring or modification (read-only parser)
- Full AUTOSAR meta-model implementation (only communication-relevant elements)
- CAN/CAN FD signal database (FlexRay, LIN) - focus is Ethernet
- SecOC secured payload decryption (only metadata extraction)
- AUTOSAR software component behavior simulation
- ARXML diff/merge tools
- AUTOSAR 3.x support (only AUTOSAR 4.x)

## Implementation Notes

### Recommended Approach

**Phase 1 - Core XML Parsing** (1 week)
- Set up pugixml integration
- Implement AR-PACKAGE and AUTOSAR-ROOT parsing
- Handle multi-file ARXML references
- Tests: 10+ for XML loading and navigation

**Phase 2 - SOME/IP Configuration** (1.5 weeks)
- Parse ServiceInterface elements
- Extract SomeipServiceInstanceToMachineMapping
- Extract SomeipServiceDiscovery config
- Tests: 15+ for SOME/IP extraction

**Phase 3 - Signal Database** (2 weeks)
- Parse SystemSignal definitions
- Extract ISignalToIPduMapping
- Parse ComPuMethod transformations
- Handle multiplexed signals
- Tests: 20+ for signal extraction

**Phase 4 - ECU and Topology** (1 week)
- Parse EcuInstance, EthCtrlConfig
- Extract SocketAddress assignments
- Extract DoIP logical addresses
- Tests: 10+ for ECU extraction

**Phase 5 - PDU and Frame Mapping** (1 week)
- Parse IPdu and Frame definitions
- Extract IPduToFrameMapping
- Handle SecuredIPdu references
- Tests: 12+ for PDU/frame mapping

**Phase 6 - Communication Matrix** (1 week)
- Build sender-receiver relationships
- Generate communication matrix
- Export to CSV
- Tests: 8+ for matrix generation

**Phase 7 - Integration** (1 week)
- Create ArxmlDatabase API
- CLI tool (wadjet-arxml)
- Python bindings
- Scenario test integration
- Tests: 10+ for API and CLI

**Total Duration**: ~8.5 weeks

### Technology Stack

- **XML Parser**: pugixml (fast, header-only, low memory footprint)
- **Schema Validation**: Optional (libxml2 if needed, or pugixml + custom validation)
- **Export**: CSV (simple text format), optional: Excel (libxlsxwriter)

### ARXML Element Hierarchy Example

```
<AUTOSAR>
  <AR-PACKAGES>
    <AR-PACKAGE>
      <SHORT-NAME>SystemDescription</SHORT-NAME>
      <ELEMENTS>
        <SYSTEM>
          <ECU-INSTANCES>
            <ECU-INSTANCE>
              <SHORT-NAME>ECU_Gateway</SHORT-NAME>
              <COM-CONFIG-GW-TIME-BASE>...</COM-CONFIG-GW-TIME-BASE>
              <COMM-CONTROLLERS>
                <ETHERNET-COMMUNICATION-CONTROLLER>
                  <MAC-ADDRESS>02:00:00:00:00:01</MAC-ADDRESS>
                </ETHERNET-COMMUNICATION-CONTROLLER>
              </COMM-CONTROLLERS>
            </ECU-INSTANCE>
          </ECU-INSTANCES>
          <MAPPINGS>
            <SYSTEM-MAPPING>
              <SW-MAPPINGS>
                <SWC-TO-ECU-MAPPING>
                  <COMPONENT-IREFS>...</COMPONENT-IREFS>
                  <ECU-INSTANCE-REF>...</ECU-INSTANCE-REF>
                </SWC-TO-ECU-MAPPING>
              </SW-MAPPINGS>
            </SYSTEM-MAPPING>
          </MAPPINGS>
        </SYSTEM>
      </ELEMENTS>
    </AR-PACKAGE>
    <AR-PACKAGE>
      <SHORT-NAME>ServiceInterfaces</SHORT-NAME>
      <ELEMENTS>
        <SERVICE-INTERFACE>
          <SHORT-NAME>MyService</SHORT-NAME>
          <SERVICE-ID>0x1234</SERVICE-ID>
          <METHODS>
            <CLIENT-SERVER-OPERATION>
              <SHORT-NAME>MyMethod</SHORT-NAME>
              <METHOD-ID>0x0001</METHOD-ID>
            </CLIENT-SERVER-OPERATION>
          </METHODS>
        </SERVICE-INTERFACE>
      </ELEMENTS>
    </AR-PACKAGE>
  </AR-PACKAGES>
</AUTOSAR>
```

### ArxmlDatabase API Example

```cpp
class ArxmlDatabase {
public:
    // Load ARXML file(s)
    void load(const std::filesystem::path& arxml_file);
    
    // Query SOME/IP services
    std::vector<ServiceInterface> get_service_interfaces() const;
    ServiceInterface find_service(uint16_t service_id) const;
    
    // Query signals
    std::vector<SystemSignal> get_signals() const;
    SystemSignal find_signal(std::string_view name) const;
    
    // Query ECUs
    std::vector<EcuInstance> get_ecus() const;
    EcuInstance find_ecu(std::string_view name) const;
    
    // Generate communication matrix
    CommunicationMatrix generate_comm_matrix() const;
    void export_comm_matrix_csv(const std::filesystem::path& output) const;
};
```

### CLI Tool Usage

```bash
# Inspect ARXML file
wadjet-arxml inspect system.arxml

# List all SOME/IP services
wadjet-arxml list-services system.arxml

# Export communication matrix
wadjet-arxml comm-matrix system.arxml -o matrix.csv

# Extract signal database
wadjet-arxml signals system.arxml --format json -o signals.json

# Validate ARXML (optional schema validation)
wadjet-arxml validate system.arxml --schema autosar_4-3-1.xsd
```

### Test Count Target

**40+ tests** covering:
- Core XML parsing: 10 tests
- SOME/IP extraction: 15 tests
- Signal database: 20 tests
- ECU topology: 10 tests
- PDU/frame mapping: 12 tests
- Communication matrix: 8 tests
- API and CLI: 10 tests

# Feature Specification: A2L/HEX File Support (Calibration Workflows)

**Feature Branch**: `milestone/020-a2l-hex-support`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M20 - A2L/HEX File Support

## Overview

Implement ASAP2 (A2L) and Intel HEX/Motorola S-Record file support for automotive calibration workflows. A2L files describe ECU memory layout, measurement points, and calibration parameters. HEX files contain firmware images. Together, these enable calibration data extraction from network traffic and firmware analysis.

## User Scenarios & Testing

### User Story 1 - A2L Database Loading (Priority: P1)

As a calibration engineer, I need to load A2L files, so that I can map memory addresses to calibration parameters.

**Why this priority**: A2L is the schema for ECU memory - without it, calibration data is meaningless byte arrays.

**Independent Test**: Load A2L file and query measurement/characteristic definitions.

**Acceptance Scenarios**:

1. **Given** A2L file for ECU, **When** loaded, **Then** all MEASUREMENT and CHARACTERISTIC objects are extracted
2. **Given** measurement definition, **When** queried, **Then** address, data type, conversion method (COMPU_METHOD), and unit are available
3. **Given** characteristic (calibration parameter), **When** queried, **Then** address, data type, min/max values, and default value are available
4. **Given** A2L with RECORD_LAYOUT, **When** parsed, **Then** memory layout (axis points, values, offsets) is correctly interpreted
5. **Given** COMPU_METHOD, **When** applied, **Then** raw value is converted to physical value (linear, table, formula)

### User Story 2 - Calibration Data Extraction from Memory Dumps (Priority: P1)

As a validation engineer, I need to extract calibration values from memory dumps, so that I can verify ECU configuration matches specification.

**Why this priority**: Calibration verification ensures ECU uses correct parameter sets.

**Independent Test**: Load A2L and memory dump, extract calibration parameter values.

**Acceptance Scenarios**:

1. **Given** memory dump (binary file) and A2L, **When** characteristic queried, **Then** value is extracted from correct address
2. **Given** characteristic with COMPU_METHOD, **When** extracted, **Then** physical value is calculated
3. **Given** array characteristic (e.g., lookup table), **When** extracted, **Then** all array elements are returned
4. **Given** 2D map characteristic, **When** extracted, **Then** axis values and data points are correctly structured
5. **Given** ECU address extensions (address page switching), **When** handled, **Then** correct memory page is accessed

### User Story 3 - Intel HEX File Parsing (Priority: P2)

As a firmware engineer, I need to parse Intel HEX files, so that I can extract firmware images and verify checksums.

**Why this priority**: HEX files are standard firmware format - needed for firmware analysis.

**Independent Test**: Load Intel HEX file and extract memory regions.

**Acceptance Scenarios**:

1. **Given** Intel HEX file, **When** parsed, **Then** all data records (type 00) are extracted with addresses
2. **Given** HEX extended linear address records (type 04), **When** parsed, **Then** addresses ≥64KB are correctly calculated
3. **Given** HEX start address record (type 05), **When** parsed, **Then** entry point address is extracted
4. **Given** HEX checksum validation, **When** enabled, **Then** invalid records are detected
5. **Given** multi-segment HEX file, **When** loaded, **Then** all memory segments are assembled into continuous regions

### User Story 4 - Motorola S-Record Parsing (Priority: P2)

As an embedded systems engineer, I need to parse Motorola S-Record files, so that I can load firmware for different ECU architectures.

**Why this priority**: S-Record is common alternative to HEX - many automotive ECUs use this format.

**Independent Test**: Load S-Record file and extract memory regions.

**Acceptance Scenarios**:

1. **Given** S-Record file, **When** parsed, **Then** all data records (S1/S2/S3) are extracted
2. **Given** S-Record with 16-bit addresses (S1), **When** parsed, **Then** addresses are correctly interpreted
3. **Given** S-Record with 24-bit addresses (S2), **When** parsed, **Then** addresses are correctly interpreted
4. **Given** S-Record with 32-bit addresses (S3), **When** parsed, **Then** addresses are correctly interpreted
5. **Given** S-Record start address (S7/S8/S9), **When** parsed, **Then** entry point is extracted
6. **Given** S-Record checksum validation, **When** enabled, **Then** corrupt records are detected

### User Story 5 - Calibration Data Monitoring from Network Traffic (Priority: P2)

As a test engineer, I need to monitor calibration parameter reads over network, so that I can verify calibration tool behavior.

**Why this priority**: Network-based calibration monitoring validates calibration workflows.

**Independent Test**: Capture UDS ReadMemoryByAddress traffic, decode with A2L.

**Acceptance Scenarios**:

1. **Given** UDS ReadMemoryByAddress (0x23) request, **When** decoded with A2L, **Then** requested parameter name is resolved
2. **Given** ReadMemoryByAddress response with data, **When** decoded, **Then** physical value is calculated using A2L COMPU_METHOD
3. **Given** UDS WriteMemoryByAddress (0x3D) request, **When** decoded, **Then** parameter name and new value are displayed
4. **Given** sequence of read/write operations, **When** analyzed, **Then** calibration session timeline is visualized
5. **Given** A2L-based filtering, **When** applied, **Then** only traffic for specific parameters is shown

## Edge Cases

- What happens when A2L references non-existent COMPU_METHOD?
- How are A2L files with multiple ASAP2 versions handled?
- What if HEX file has overlapping address ranges?
- How does system handle S-Record with mixed address types (S1 + S2 + S3)?
- What happens when memory dump doesn't cover address range in A2L?
- How are big-endian vs. little-endian memory dumps distinguished?

## Requirements

### Functional Requirements

#### A2L Parsing

- **FR-001**: System MUST parse ASAP2 (A2L) files version 1.60 and 1.71
- **FR-002**: System MUST extract MODULE and PROJECT metadata
- **FR-003**: System MUST parse MEASUREMENT objects (address, data type, COMPU_METHOD, ECU_ADDRESS_EXTENSION)
- **FR-004**: System MUST parse CHARACTERISTIC objects (address, data type, RECORD_LAYOUT, min/max, default)
- **FR-005**: System MUST parse COMPU_METHOD (RAT_FUNC, TAB_VERB, TAB_NOINTERP, IDENTICAL, LINEAR, FORM)
- **FR-006**: System MUST parse RECORD_LAYOUT (FNC_VALUES, AXIS_PTS_X, AXIS_PTS_Y offsets)
- **FR-007**: System MUST handle IF_DATA blocks (protocol-specific extensions)
- **FR-008**: System MUST support /include directives for multi-file A2L projects

#### Calibration Data Extraction

- **FR-009**: System MUST extract scalar measurement/characteristic values from memory dump
- **FR-010**: System MUST extract 1D array (CURVE) from memory using RECORD_LAYOUT
- **FR-011**: System MUST extract 2D map (MAP) with axis points and values
- **FR-012**: System MUST apply COMPU_METHOD to convert raw values to physical values
- **FR-013**: System MUST handle byte order (MSB_FIRST, MSB_LAST) correctly
- **FR-014**: System MUST support ECU_ADDRESS_EXTENSION for paged memory

#### Intel HEX File Parsing

- **FR-015**: System MUST parse Intel HEX format (all record types: 00, 01, 02, 03, 04, 05)
- **FR-016**: System MUST validate HEX record checksums
- **FR-017**: System MUST handle extended segment address (type 02) and extended linear address (type 04)
- **FR-018**: System MUST extract data records and assemble into memory regions
- **FR-019**: System MUST support sparse HEX files (non-contiguous address ranges)
- **FR-020**: System MUST extract HEX start address (type 03, 05)

#### Motorola S-Record Parsing

- **FR-021**: System MUST parse S-Record format (S0-S9 records)
- **FR-022**: System MUST validate S-Record checksums
- **FR-023**: System MUST handle S1 (16-bit), S2 (24-bit), S3 (32-bit) address formats
- **FR-024**: System MUST extract data records and assemble into memory regions
- **FR-025**: System MUST extract start address (S7, S8, S9)
- **FR-026**: System MUST support sparse S-Record files

#### Network Traffic Integration

- **FR-027**: System MUST decode UDS ReadMemoryByAddress (0x23) with A2L parameter lookup
- **FR-028**: System MUST decode UDS WriteMemoryByAddress (0x3D) with A2L parameter lookup
- **FR-029**: System MUST resolve memory addresses to measurement/characteristic names
- **FR-030**: System MUST display physical values using A2L COMPU_METHOD
- **FR-031**: System MUST support calibration traffic filtering by parameter name

#### API & Tools

- **FR-032**: System MUST provide A2lDatabase C++ API for querying
- **FR-033**: System MUST provide HexFileReader and SRecordReader classes
- **FR-034**: CLI tool (wadjet-a2l) MUST support A2L inspection and parameter lookup
- **FR-035**: CLI tool (wadjet-hex) MUST support HEX/S-Record parsing and conversion
- **FR-036**: Python bindings MUST expose A2L and HEX parsing APIs

### Key Entities

- **A2lDatabase**: Container for A2L parsed data
- **Measurement**: Measurement point definition
- **Characteristic**: Calibration parameter definition
- **CompuMethod**: Value conversion method (linear, table, formula)
- **RecordLayout**: Memory layout structure for arrays/maps
- **MemoryDump**: Binary memory content
- **HexFile**: Intel HEX file representation
- **SRecordFile**: Motorola S-Record file representation
- **MemoryRegion**: Contiguous memory segment
- **CalibrationExtractor**: Extracts values from memory using A2L

## Success Criteria

### Measurable Outcomes

- **SC-001**: A2L parser correctly loads ASAP2 v1.60 and v1.71 files from real automotive ECUs
- **SC-002**: All MEASUREMENT and CHARACTERISTIC objects extracted with correct addresses and data types
- **SC-003**: COMPU_METHOD conversion produces correct physical values for linear, table, and formula types
- **SC-004**: Calibration extractor correctly reads scalar, 1D curve, and 2D map parameters from memory dumps
- **SC-005**: Intel HEX parser correctly assembles memory regions from multi-segment HEX files
- **SC-006**: S-Record parser handles S1, S2, S3 address formats with 100% accuracy
- **SC-007**: UDS memory read/write traffic decoded with A2L parameter name resolution
- **SC-008**: CLI tools (`wadjet-a2l`, `wadjet-hex`) successfully inspect and export data
- **SC-009**: Python bindings enable A2L parameter extraction in ≤10 lines of code
- **SC-010**: 70+ unit tests cover all A2L elements, HEX/S-Record formats, and calibration extraction scenarios

## Assumptions

- A2L files conform to ASAM ASAP2 standard (v1.60, v1.71)
- Focus on measurement and characteristic objects (not complex axis rescaling)
- Memory dumps are binary files (not live memory access)
- Intel HEX and S-Record are primary firmware formats (no ELF parsing)
- Byte order is specified in A2L or provided by user
- ECU address extensions use standard ASAP2 syntax

## Dependencies

- **External**: None (custom A2L/HEX parsers)
- **Internal**: M9 (UDS decoder for memory read/write integration)

## Out of Scope

- XCP/CCP protocol implementation (only A2L parsing)
- Live ECU calibration via network (only offline analysis)
- A2L authoring or editing tools (read-only)
- ELF/DWARF file parsing
- Firmware disassembly or reverse engineering
- Complex COMPU_METHOD types (COMPU_TAB_REF, RAT_FUNC with multiple parameters)
- Binary firmware flashing or programming

## Implementation Notes

### Recommended Approach

**Phase 1 - A2L Parser Core** (2 weeks)
- Implement ASAP2 lexer/parser
- Parse MODULE, PROJECT, MEASUREMENT, CHARACTERISTIC
- Parse COMPU_METHOD and RECORD_LAYOUT
- Tests: 25+ for A2L parsing

**Phase 2 - Calibration Data Extraction** (1.5 weeks)
- Implement CalibrationExtractor
- Extract scalar, 1D, 2D parameters from memory
- Apply COMPU_METHOD conversions
- Tests: 20+ for extraction

**Phase 3 - Intel HEX Parser** (1 week)
- Parse all HEX record types
- Assemble memory regions
- Checksum validation
- Tests: 15+ for HEX parsing

**Phase 4 - S-Record Parser** (0.5 weeks)
- Parse S0-S9 records
- Handle S1/S2/S3 address formats
- Checksum validation
- Tests: 10+ for S-Record parsing

**Phase 5 - UDS Integration** (1 week)
- Integrate with UDS decoder
- Resolve addresses to parameter names
- Display physical values
- Tests: 12+ for network integration

**Phase 6 - CLI Tools & Bindings** (1 week)
- Create wadjet-a2l and wadjet-hex tools
- Python bindings
- Example programs
- Tests: 10+ for tools and bindings

**Total Duration**: ~7 weeks

### A2L Element Example

```asam2
/begin MODULE ECU_Name "Description"
  /begin MEASUREMENT EngineSpeed "Engine RPM"
    UWORD 0x12340 FNC_RPM 0 0 10000
    ECU_ADDRESS 0x12340
    FORMAT "%8.2"
    /begin IF_DATA XCP
      /begin DAQ_EVENT 0x1234
        /end DAQ_EVENT
    /end IF_DATA
  /end MEASUREMENT
  
  /begin CHARACTERISTIC VehicleSpeed_Limit "Max vehicle speed"
    VALUE 0x56780 DISPO_SPEED 0 0 250 150
    FORMAT "%5.1"
  /end CHARACTERISTIC
  
  /begin COMPU_METHOD FNC_RPM "RPM conversion"
    LINEAR "%8.2" "rpm"
    COEFFS_LINEAR 0 0.25
  /end COMPU_METHOD
/end MODULE
```

### A2lDatabase API Example

```cpp
class A2lDatabase {
public:
    void load(const std::filesystem::path& a2l_file);
    
    // Query definitions
    std::optional<Measurement> find_measurement(std::string_view name) const;
    std::optional<Characteristic> find_characteristic(std::string_view name) const;
    std::optional<Measurement> find_measurement_by_address(uint32_t addr) const;
    
    // Extract values from memory
    std::optional<double> extract_measurement(
        std::string_view name,
        const MemoryDump& memory
    ) const;
    
    std::vector<double> extract_curve(
        std::string_view name,
        const MemoryDump& memory
    ) const;
    
    std::pair<std::vector<double>, std::vector<std::vector<double>>>
    extract_map(std::string_view name, const MemoryDump& memory) const;
};
```

### HEX File API Example

```cpp
class HexFile {
public:
    void load(const std::filesystem::path& hex_file);
    
    // Get memory regions
    std::vector<MemoryRegion> get_regions() const;
    
    // Read data
    std::optional<uint8_t> read_byte(uint32_t address) const;
    std::vector<uint8_t> read(uint32_t address, size_t length) const;
    
    // Metadata
    std::optional<uint32_t> get_start_address() const;
    size_t total_data_size() const;
};
```

### CLI Tool Usage

```bash
# Inspect A2L file
wadjet-a2l inspect ecu.a2l

# Query measurement
wadjet-a2l query ecu.a2l --measurement EngineSpeed

# Extract calibration from memory dump
wadjet-a2l extract ecu.a2l memory.bin --characteristic VehicleSpeed_Limit

# Convert HEX to binary
wadjet-hex convert firmware.hex -o firmware.bin

# Inspect HEX file
wadjet-hex inspect firmware.hex

# Verify HEX checksums
wadjet-hex verify firmware.hex
```

### Test Count Target

**70+ tests** covering:
- A2L parsing: 25 tests
- Calibration extraction: 20 tests
- Intel HEX parsing: 15 tests
- S-Record parsing: 10 tests
- UDS integration: 12 tests
- CLI tools and bindings: 10 tests

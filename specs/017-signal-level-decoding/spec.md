# Feature Specification: Signal-Level Decoding

**Feature Branch**: `milestone/017-signal-level-decoding`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M17 - Signal-Level Decoding

## Overview

Implement signal-level decoding to extract and interpret individual signals from network traffic. This feature combines ARXML signal definitions with raw packet data to decode physical values from bit-level payloads. Signal decoding enables analysis of sensor data, actuator commands, and system state from captured Ethernet traffic.

## User Scenarios & Testing

### User Story 1 - Extract Signal from SOME/IP Event (Priority: P1)

As an ADAS engineer, I need to extract camera frame rate signal from SOME/IP events, so that I can verify camera performance.

**Why this priority**: Signal extraction is the core functionality - enables analysis of actual data values.

**Independent Test**: Load ARXML with signal definition, capture SOME/IP event, extract signal value.

**Acceptance Scenarios**:

1. **Given** SOME/IP event with signal "CameraFrameRate" at byte position 4, bit position 0, length 16 bits, **When** decoded, **Then** raw value is extracted
2. **Given** signal with ComPuMethod (linear scaling: physical = raw × 0.1), **When** converted, **Then** physical value is calculated (e.g., raw 300 → 30.0 fps)
3. **Given** signal with units (fps), **When** displayed, **Then** value includes unit (30.0 fps)
4. **Given** signal with range constraints (0-60 fps), **When** validated, **Then** out-of-range values are flagged
5. **Given** signal with initialization value, **When** first packet lacks signal, **Then** init value is used

### User Story 2 - Decode Multiple Signals from PDU (Priority: P1)

As a vehicle dynamics engineer, I need to decode all signals from a vehicle state PDU, so that I can analyze speed, steering angle, and brake status together.

**Why this priority**: Multi-signal decode enables comprehensive state analysis.

**Independent Test**: Decode PDU with 10+ signals at various bit positions.

**Acceptance Scenarios**:

1. **Given** PDU with 10 signals, **When** decoded, **Then** all signals are extracted with correct values
2. **Given** signals with different byte orders (big-endian, little-endian), **When** decoded, **Then** byte order is respected
3. **Given** signals spanning byte boundaries (e.g., 12-bit signal starting at bit 4), **When** extracted, **Then** bit extraction is correct
4. **Given** signals with different data types (uint8, uint16, float32), **When** decoded, **Then** correct data type interpretation is applied
5. **Given** signal alignment constraints, **When** validated, **Then** misaligned signals are detected

### User Story 3 - Signal Transformation (ComPuMethod) (Priority: P1)

As a calibration engineer, I need signal physical value conversion using ComPuMethod, so that I can interpret raw values meaningfully.

**Why this priority**: Physical value conversion is essential for engineering analysis.

**Independent Test**: Apply all ComPuMethod types to signals.

**Acceptance Scenarios**:

1. **Given** signal with linear ComPuMethod (y = mx + b), **When** converted, **Then** physical value = raw × scale + offset
2. **Given** signal with rational function ComPuMethod (y = (ax + b)/(cx + d)), **When** converted, **Then** correct rational function is applied
3. **Given** signal with text-table ComPuMethod (enumeration), **When** decoded, **Then** raw value is mapped to text (e.g., 0 → "Off", 1 → "On", 2 → "Error")
4. **Given** signal with scale-table ComPuMethod (piecewise linear), **When** converted, **Then** correct scale segment is applied
5. **Given** signal with identity ComPuMethod (no transformation), **When** converted, **Then** physical value equals raw value

### User Story 4 - Time-Series Signal Extraction (Priority: P2)

As a performance analyst, I need time-series signal data from captures, so that I can plot signal trends over time.

**Why this priority**: Time-series analysis reveals patterns and anomalies.

**Independent Test**: Extract signal values with timestamps from 1-minute capture.

**Acceptance Scenarios**:

1. **Given** captured traffic over time, **When** signal extracted, **Then** each signal value has associated timestamp
2. **Given** periodic signal updates, **When** analyzed, **Then** update rate is calculated (e.g., 100 Hz)
3. **Given** signal changes, **When** detected, **Then** change events are timestamped
4. **Given** signal with missing updates, **When** detected, **Then** gaps in time-series are identified
5. **Given** time-series export, **When** generated, **Then** output is in CSV format (timestamp, signal_name, value)

### User Story 5 - Multiplexed Signal Decoding (Priority: P2)

As a systems engineer, I need to decode multiplexed signals based on selector values, so that I can analyze complex PDU structures.

**Why this priority**: Multiplexing is common in automotive - enables conditional signal presence.

**Independent Test**: Decode PDU with multiplexed signals controlled by selector field.

**Acceptance Scenarios**:

1. **Given** PDU with selector signal (e.g., "MessageType"), **When** decoded, **Then** selector value is extracted
2. **Given** multiplexed signals based on selector, **When** selector = 0x01, **Then** signal group A is decoded
3. **Given** different selector value, **When** selector = 0x02, **Then** signal group B is decoded instead
4. **Given** selector value not matching any group, **When** encountered, **Then** warning is issued
5. **Given** nested multiplexing (selector within multiplex group), **When** decoded, **Then** nested structure is handled correctly

### User Story 6 - Signal Database Query API (Priority: P1)

As a test automation engineer, I need programmatic signal database API, so that I can query signal definitions and values.

**Why this priority**: API enables integration with test frameworks and analysis tools.

**Independent Test**: Query signal by name and extract from packet.

**Acceptance Scenarios**:

1. **Given** signal name "VehicleSpeed", **When** queried, **Then** signal definition (byte pos, bit pos, length, ComPuMethod) is returned
2. **Given** PDU ID and signal name, **When** queried, **Then** signal is located within PDU
3. **Given** packet data, **When** signal extracted by name, **Then** physical value is returned
4. **Given** list of signals, **When** requested, **Then** all signals in PDU are enumerated
5. **Given** signal value change listener, **When** registered, **Then** callback is invoked on signal update

## Edge Cases

- What happens when signal bit extraction spans more than 2 bytes?
- How are signals with float data types (IEEE 754) decoded from bit-level payloads?
- What if signal position in actual packet doesn't match ARXML definition (data corruption)?
- How does system handle signals with invalid ComPuMethod references?
- What happens when multiplexed selector signal is itself multiplexed?
- How are signals in secured PDUs (SecOC) handled (signal after authentication header)?

## Requirements

### Functional Requirements

#### Signal Extraction from Packets

- **FR-001**: System MUST extract signals from raw packet payload at specified byte and bit positions
- **FR-002**: System MUST handle signal lengths from 1 to 64 bits
- **FR-003**: System MUST support both big-endian and little-endian byte order
- **FR-004**: System MUST handle signals spanning multiple bytes
- **FR-005**: System MUST extract signals aligned and unaligned to byte boundaries
- **FR-006**: System MUST support all data types (uint8, uint16, uint32, uint64, int8, int16, int32, int64, float32, float64, string)

#### Signal Database Integration

- **FR-007**: System MUST load signal definitions from ARXML (M15 integration)
- **FR-008**: System MUST map signals to PDUs using ISignalToIPduMapping
- **FR-009**: System MUST map PDUs to frames using IPduToFrameMapping
- **FR-010**: System MUST resolve signal definitions by name, PDU ID, or frame ID
- **FR-011**: System MUST support signal groups (related signals)

#### Physical Value Conversion

- **FR-012**: System MUST apply LINEAR ComPuMethod (physical = raw × scale + offset)
- **FR-013**: System MUST apply RATIONAL-FUNC ComPuMethod (rational functions)
- **FR-014**: System MUST apply TEXT-TABLE ComPuMethod (enumeration mapping)
- **FR-015**: System MUST apply SCALE-LINEAR ComPuMethod (piecewise linear)
- **FR-016**: System MUST handle IDENTICAL ComPuMethod (no conversion)
- **FR-017**: System MUST validate ComPuMethod references and report errors

#### Signal Validation

- **FR-018**: System MUST validate signal values against min/max ranges
- **FR-019**: System MUST detect out-of-range values and issue warnings
- **FR-020**: System MUST apply initialization values for missing signals
- **FR-021**: System MUST detect signal encoding errors (invalid enum values)
- **FR-022**: System MUST validate signal alignment constraints

#### Multiplexed Signals

- **FR-023**: System MUST decode multiplexed signals based on selector signal value
- **FR-024**: System MUST support nested multiplexing (selector within multiplex group)
- **FR-025**: System MUST handle selector values not matching any defined group
- **FR-026**: System MUST warn when selector signal is missing or invalid

#### Time-Series Support

- **FR-027**: System MUST associate timestamps with signal values
- **FR-028**: System MUST calculate signal update rate from timestamp sequence
- **FR-029**: System MUST detect signal value changes and timestamp transitions
- **FR-030**: System MUST export time-series data to CSV format (timestamp, signal, value)
- **FR-031**: System MUST identify gaps in signal updates

#### Query API

- **FR-032**: System MUST provide C++ API for signal database queries
- **FR-033**: System MUST support signal lookup by name, PDU ID, and frame ID
- **FR-034**: System MUST enumerate all signals in PDU
- **FR-035**: System MUST extract signal value from packet data by signal name
- **FR-036**: System MUST return signal metadata (unit, range, ComPuMethod)

#### Protocol-Specific Integration

- **FR-037**: System MUST extract signals from SOME/IP events
- **FR-038**: System MUST extract signals from SOME/IP methods
- **FR-039**: System MUST extract signals from Ethernet frames (non-IP payloads)
- **FR-040**: System MUST handle signals in DoIP diagnostic messages
- **FR-041**: System MUST support signals in DDS/RTPS data messages (if M10/M11 complete)

#### Bindings & Tools

- **FR-042**: Python bindings MUST expose signal extraction and conversion APIs
- **FR-043**: CLI tool (wadjet-signals) MUST support signal extraction from PCAP
- **FR-044**: Example program (signal_analyzer.cpp) MUST demonstrate signal-level analysis
- **FR-045**: Scenario tests MUST support signal value assertions

### Key Entities

- **SignalDatabase**: Container for signal definitions from ARXML
- **SignalDefinition**: Signal metadata (name, position, length, byte order, ComPuMethod, range)
- **SignalValue**: Extracted signal value (raw and physical)
- **ComPuMethod**: Base class for value transformation (Linear, Rational, TextTable, ScaleLinear)
- **MultiplexedSignal**: Signal with selector-based conditional decoding
- **SignalExtractor**: Core engine for bit-level extraction
- **TimeSeriesSignalData**: Signal values with timestamps
- **SignalValidator**: Range and constraint validation

## Success Criteria

### Measurable Outcomes

- **SC-001**: Signal extraction correctly handles all bit lengths (1-64 bits) with big/little endian
- **SC-002**: All ComPuMethod types (Linear, Rational, TextTable, ScaleLinear) produce correct physical values
- **SC-003**: Multiplexed signal decoding correctly switches signal groups based on selector value
- **SC-004**: Signal extractor handles 1000+ signals per second with ≤10ms latency
- **SC-005**: Time-series export generates valid CSV with timestamp, signal name, and value columns
- **SC-006**: Signal database query API provides sub-millisecond lookup by signal name
- **SC-007**: Integration with ARXML parser (M15) correctly loads all signal definitions
- **SC-008**: Python bindings enable signal extraction in ≤5 lines of code
- **SC-009**: CLI tool extracts signals from 100MB PCAP in ≤30 seconds
- **SC-010**: 60+ unit tests cover all signal extraction scenarios

## Assumptions

- Signal definitions are available in ARXML (M15 dependency)
- Focus on Ethernet-based protocols (SOME/IP, DoIP, Ethernet frames)
- Signals are byte-aligned or unaligned (no assumption of byte boundaries)
- ComPuMethod definitions are valid and reference existing data
- Multiplexing uses standard AUTOSAR selector pattern

## Dependencies

- **External**: None (uses ARXML parser from M15)
- **Internal**: M15 (ARXML parser for signal definitions), M2 (protocol decoders)

## Out of Scope

- Signal value simulation or generation (read-only extraction)
- Signal transmission or encoding (only decoding)
- CAN/CAN FD signal decoding (focus on Ethernet)
- Real-time signal value monitoring (focus on offline analysis)
- Signal database authoring or editing
- Complex ComPuMethod types (beyond Linear, Rational, TextTable, ScaleLinear)

## Implementation Notes

### Recommended Approach

**Phase 1 - Signal Extraction Core** (1.5 weeks)
- Implement bit extraction from byte arrays
- Support big-endian and little-endian
- Handle multi-byte signals
- Tests: 15+ for extraction scenarios

**Phase 2 - Data Type Conversion** (1 week)
- Implement all data types (uint, int, float, string)
- Handle type-specific conversions
- Tests: 12+ for data types

**Phase 3 - ComPuMethod Implementation** (1.5 weeks)
- Implement Linear ComPuMethod
- Implement Rational, TextTable, ScaleLinear
- Tests: 18+ for all ComPuMethod types

**Phase 4 - Signal Database Integration** (1 week)
- Integrate with ARXML parser (M15)
- Build SignalDatabase from ARXML
- Implement signal-to-PDU-to-frame mapping
- Tests: 10+ for database integration

**Phase 5 - Multiplexed Signals** (1 week)
- Implement selector-based decoding
- Handle nested multiplexing
- Tests: 10+ for multiplexing

**Phase 6 - Time-Series Support** (0.5 weeks)
- Implement timestamp association
- Calculate update rates
- Export to CSV
- Tests: 8+ for time-series

**Phase 7 - Protocol Integration** (1 week)
- Integrate with SOME/IP decoder
- Integrate with DoIP decoder
- Integrate with Ethernet frame decoder
- Tests: 12+ for protocol-specific extraction

**Phase 8 - API, CLI, and Bindings** (1 week)
- Create SignalDatabase API
- CLI tool (wadjet-signals)
- Python bindings
- Example program
- Tests: 10+ for API and tools

**Total Duration**: ~8.5 weeks

### Bit Extraction Algorithm Example

```cpp
class SignalExtractor {
public:
    // Extract signal from byte array
    uint64_t extract_raw_value(
        std::span<const uint8_t> data,
        size_t byte_position,
        size_t bit_position,
        size_t bit_length,
        ByteOrder byte_order
    ) const {
        // 1. Extract bytes containing signal
        // 2. Shift bits to align signal
        // 3. Mask to extract signal bits
        // 4. Apply byte order conversion
        // 5. Return raw value
    }
    
    // Convert raw to physical value
    double apply_compu_method(
        uint64_t raw_value,
        const ComPuMethod& compu
    ) const {
        return compu.convert(raw_value);
    }
};
```

### ComPuMethod Hierarchy

```cpp
class ComPuMethod {
public:
    virtual ~ComPuMethod() = default;
    virtual double convert(uint64_t raw) const = 0;
    virtual std::optional<std::string> text(uint64_t raw) const { return std::nullopt; }
};

class LinearComPuMethod : public ComPuMethod {
    double scale_;
    double offset_;
public:
    double convert(uint64_t raw) const override {
        return raw * scale_ + offset_;
    }
};

class TextTableComPuMethod : public ComPuMethod {
    std::map<uint64_t, std::string> table_;
public:
    double convert(uint64_t raw) const override { return raw; }
    std::optional<std::string> text(uint64_t raw) const override {
        if (auto it = table_.find(raw); it != table_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};
```

### CLI Tool Usage

```bash
# Extract all signals from PCAP
wadjet-signals extract capture.pcap --arxml system.arxml -o signals.csv

# Query signal definition
wadjet-signals info --arxml system.arxml --signal VehicleSpeed

# List all signals in PDU
wadjet-signals list-pdu --arxml system.arxml --pdu-id 0x1234

# Real-time signal display (not for offline analysis, but useful for testing)
wadjet-signals live eth0 --arxml system.arxml --signals "VehicleSpeed,SteeringAngle"
```

### Python API Example

```python
import wadjet

# Load signal database
signals = wadjet.SignalDatabase("system.arxml")

# Open capture
with wadjet.read_pcap("capture.pcap") as pcap:
    for packet in pcap:
        # Extract signal by name
        speed = signals.extract_signal(packet, "VehicleSpeed")
        if speed:
            print(f"Speed: {speed.physical_value} {speed.unit}")
```

### Test Count Target

**60+ tests** covering:
- Bit extraction: 15 tests
- Data type conversion: 12 tests
- ComPuMethod: 18 tests
- Signal database: 10 tests
- Multiplexing: 10 tests
- Time-series: 8 tests
- Protocol integration: 12 tests
- API and tools: 10 tests

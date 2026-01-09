# Feature Specification: TSN Awareness (IEEE 802.1Qbv)

**Feature Branch**: `milestone/012-tsn-awareness`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M12 - TSN Awareness

## Overview

Implement Time-Sensitive Networking (TSN) awareness for deterministic Ethernet analysis. TSN is a set of IEEE 802.1 standards enabling deterministic, low-latency communication over Ethernet. 802.1Qbv (Time-Aware Shaper) is critical for automotive real-time applications.

## User Scenarios & Testing

### User Story 1 - VLAN Priority Code Point (PCP) Analysis (Priority: P1)

As an automotive network engineer, I need to analyze VLAN priority mappings, so that I can verify traffic class assignments for critical automotive data streams.

**Why this priority**: Priority mapping is fundamental for TSN - ensures critical traffic gets appropriate QoS treatment.

**Independent Test**: Capture mixed-priority traffic and verify PCP extraction and traffic class mapping.

**Acceptance Scenarios**:

1. **Given** packets with PCP values 0-7, **When** analyzed, **Then** I can map them to TSN traffic classes (Best Effort, Background, Excellent Effort, Critical Applications, Video, Voice, Internetwork Control, Network Control)
2. **Given** VLAN-tagged packets, **When** decoded, **Then** PCP field is extracted correctly
3. **Given** QinQ (double VLAN) packets, **When** decoded, **Then** both inner and outer PCP values are available
4. **Given** traffic statistics, **When** calculated, **Then** per-priority packet count, bytes, and bandwidth are reported
5. **Given** priority distribution, **When** visualized, **Then** system shows which traffic classes dominate the network

### User Story 2 - Schedule Parsing (802.1Qbv Gate Control) (Priority: P2)

As a TSN configuration engineer, I need to parse Time-Aware Shaper schedules, so that I can validate gate timing configurations.

**Why this priority**: TAS schedules define transmission windows - critical for validating deterministic behavior.

**Independent Test**: Load TAS configuration and verify gate control list parsing.

**Acceptance Scenarios**:

1. **Given** TAS gate control list configuration, **When** parsed, **Then** I get gate states (open/closed) for each traffic class per time slot
2. **Given** TAS cycle time and base time, **When** analyzed, **Then** I can calculate expected transmission windows
3. **Given** gate control entries, **When** processed, **Then** time intervals and operation types are extracted correctly
4. **Given** multiple traffic classes (0-7), **When** schedule defined, **Then** per-class transmission windows are identified
5. **Given** schedule changes, **When** detected, **Then** system reports TAS reconfiguration events

### User Story 3 - Latency Measurement with PCP Context (Priority: P1)

As a real-time systems analyst, I need latency measurements per priority level, so that I can verify TSN meets timing requirements.

**Why this priority**: Per-priority latency is key TSN metric - different classes have different latency budgets.

**Independent Test**: Measure end-to-end latency for packets across priority levels.

**Acceptance Scenarios**:

1. **Given** packets with timestamps, **When** correlated, **Then** end-to-end latency is calculated per PCP value
2. **Given** latency distribution, **When** calculated, **Then** p50, p95, p99 latency is reported per priority class
3. **Given** high-priority traffic (PCP 6-7), **When** measured, **Then** latency is consistently lower than low-priority traffic
4. **Given** latency violations, **When** detected, **Then** system reports which priority class exceeded threshold
5. **Given** time-series data, **When** analyzed, **Then** latency trends per priority are visualized

### User Story 4 - Stream Identification (Priority: P2)

As a TSN network architect, I need to identify TSN streams by (MAC, VLAN ID) tuples, so that I can track individual data flows.

**Why this priority**: Stream identification enables per-flow analysis and troubleshooting.

**Independent Test**: Identify and track multiple TSN streams in capture.

**Acceptance Scenarios**:

1. **Given** TSN traffic, **When** analyzed, **Then** streams are identified by (source MAC, VLAN ID) pairs
2. **Given** stream definition, **When** applied, **Then** packets belonging to stream are filtered correctly
3. **Given** multiple streams, **When** tracked, **Then** per-stream statistics (packet count, bandwidth, latency) are calculated
4. **Given** stream lifecycle, **When** monitored, **Then** stream start, active, and end times are detected
5. **Given** stream prioritization, **When** analyzed, **Then** actual PCP values match expected stream priority

### User Story 5 - Traffic Class Distribution Analysis (Priority: P1)

As a network validation engineer, I need traffic class distribution statistics, so that I can verify bandwidth allocation per priority.

**Why this priority**: Bandwidth allocation verification ensures critical traffic gets required resources.

**Independent Test**: Analyze capture and generate traffic class distribution report.

**Acceptance Scenarios**:

1. **Given** captured traffic, **When** analyzed, **Then** bandwidth percentage per traffic class is calculated
2. **Given** packet counts, **When** totaled, **Then** per-priority packet distribution is reported
3. **Given** bandwidth limits, **When** configured, **Then** system detects traffic classes exceeding allocation
4. **Given** time windows, **When** analyzed, **Then** traffic class distribution varies over time as expected
5. **Given** comparison mode, **When** enabled, **Then** actual vs. configured bandwidth allocation is compared

## Edge Cases

- What happens when VLAN tags are missing (untagged traffic)?
- How are malformed TAS schedules handled (overlapping gates, invalid times)?
- What if stream identification yields duplicate (MAC, VLAN) pairs?
- How does system handle priority remapping at switches?
- What happens when TAS base time is in the future or past?
- How are QinQ double VLAN tags processed for PCP extraction?

## Requirements

### Functional Requirements

#### VLAN Priority Enhancement

- **FR-001**: System MUST extract PCP (Priority Code Point) from VLAN tags (3 bits)
- **FR-002**: System MUST map PCP values (0-7) to TSN traffic class names
- **FR-003**: System MUST support QinQ (802.1ad) double VLAN tag PCP extraction
- **FR-004**: System MUST collect per-priority statistics (packet count, bytes, bandwidth)
- **FR-005**: System MUST calculate bandwidth percentage per traffic class
- **FR-006**: System MUST detect DEI (Drop Eligible Indicator) flag
- **FR-007**: System MUST handle untagged traffic as priority 0 (Best Effort)

#### TAS Schedule Parsing

- **FR-008**: System MUST parse Time-Aware Shaper gate control lists
- **FR-009**: System MUST extract gate control entries (time interval, gate state per class)
- **FR-010**: System MUST parse cycle time and base time
- **FR-011**: System MUST validate gate control list consistency
- **FR-012**: System MUST detect overlapping or invalid gate entries
- **FR-013**: System MUST support all 8 traffic classes (0-7) in schedule

#### Stream Identification

- **FR-014**: System MUST identify streams by (source MAC, VLAN ID) tuple
- **FR-015**: System MUST track multiple concurrent streams
- **FR-016**: System MUST associate packets with streams
- **FR-017**: System MUST calculate per-stream statistics
- **FR-018**: System MUST detect stream start and end
- **FR-019**: System MUST support stream filtering by ID

#### Latency Measurement

- **FR-020**: System MUST measure end-to-end latency per PCP value
- **FR-021**: System MUST calculate latency percentiles (p50, p95, p99) per priority
- **FR-022**: System MUST detect latency threshold violations per class
- **FR-023**: System MUST support latency measurement with hardware timestamps
- **FR-024**: System MUST correlate latency with PCP values

#### Analysis & Reporting

- **FR-025**: System MUST generate traffic class distribution reports
- **FR-026**: System MUST export TSN analysis results to JSON/CSV
- **FR-027**: System MUST provide time-series data for priority-based metrics
- **FR-028**: System MUST support bandwidth allocation compliance checking
- **FR-029**: System MUST detect priority inversions (low-priority delaying high-priority)

#### Integration

- **FR-030**: TSN analysis MUST integrate with existing VLAN decoder
- **FR-031**: System MUST provide gMock matchers: HasVlanPriority(), IsExpressTraffic()
- **FR-032**: Python bindings MUST expose TSN analysis functions
- **FR-033**: Example program (tsn_analyzer.cpp) MUST demonstrate TSN traffic analysis
- **FR-034**: Scenario tests MUST support TSN-based assertions

### Key Entities

- **VlanPriority**: PCP value with traffic class name mapping
- **TrafficClassStats**: Per-priority statistics (count, bytes, bandwidth, latency)
- **TasSchedule**: Gate control list with cycle time and base time
- **GateControlEntry**: Time interval with gate states per traffic class
- **StreamId**: (MAC address, VLAN ID) stream identifier
- **StreamStats**: Per-stream statistics and lifecycle tracking
- **TsnAnalyzer**: Main analysis engine for TSN traffic

## Success Criteria

### Measurable Outcomes

- **SC-001**: PCP extraction works correctly for 802.1Q and 802.1ad (QinQ) VLAN tags
- **SC-002**: Traffic class mapping correctly identifies all 8 priority levels
- **SC-003**: TAS schedule parsing extracts all gate control entries accurately
- **SC-004**: Stream identification correctly tracks multiple concurrent streams
- **SC-005**: Per-priority latency measurements accurate within ±1µs (with hardware timestamps)
- **SC-006**: Bandwidth allocation reporting shows percentage per traffic class with ≤1% error
- **SC-007**: TSN analyzer example generates comprehensive report with priority distribution
- **SC-008**: All TSN features integrate with existing matchers and testing framework
- **SC-009**: Documentation includes TSN configuration examples for automotive use cases
- **SC-010**: TSN test scenarios validate common automotive priority mappings

## Assumptions

- Target TSN standards: IEEE 802.1Q (VLAN priority), IEEE 802.1Qbv (TAS)
- Hardware capture supports VLAN tag preservation
- TAS schedules are provided via configuration or management protocol
- Timestamps available for latency measurement (hardware or software)
- Focus on priority-based analysis rather than full TAS gate enforcement simulation

## Dependencies

- **External**: IEEE 802.1Q/Qbv standards, TSN-capable switches for testing
- **Internal**: M2 (VLAN decoder), M8 (gPTP for time synchronization if needed)

## Out of Scope

- Active TAS schedule enforcement or simulation
- Traffic generation with TSN scheduling
- Full IEEE 802.1Qbu frame preemption detection
- IEEE 802.1CB frame replication and elimination (FRER)
- TSN configuration generation or network config-as-code
- Centralized Network Configuration (CNC) protocol integration

## Implementation Notes

### Recommended Approach

1. **Phase 1 - VLAN Priority Enhancement**
   - Enhance VLAN decoder to expose PCP field
   - Implement traffic class name mapping
   - Add per-priority statistics collection

2. **Phase 2 - Stream Identification**
   - Implement stream ID (MAC, VLAN) tracking
   - Build stream statistics collector
   - Add stream lifecycle detection

3. **Phase 3 - Latency Analysis**
   - Implement per-priority latency measurement
   - Calculate latency percentiles
   - Add threshold violation detection

4. **Phase 4 - TAS Schedule Support**
   - Parse TAS gate control lists
   - Validate schedule consistency
   - Support schedule-based analysis

5. **Phase 5 - Analysis Tools & Integration**
   - Create tsn_analyzer.cpp example
   - Add TSN matchers
   - Python/Rust bindings
   - Scenario test support

### Traffic Class Mapping (PCP to TSN)

| PCP | Traffic Class | Typical Use |
|-----|---------------|-------------|
| 0   | Best Effort (BE) | Default traffic |
| 1   | Background (BK) | Bulk data transfer |
| 2   | Excellent Effort (EE) | Streaming media |
| 3   | Critical Applications (CA) | Business-critical data |
| 4   | Video (VI) | Real-time video |
| 5   | Voice (VO) | Real-time voice |
| 6   | Internetwork Control (IC) | Network protocols |
| 7   | Network Control (NC) | Network management |

### Example TSN Configuration

```yaml
# TSN priority mapping for automotive
traffic_classes:
  - pcp: 7
    name: "Safety-Critical Control"
    max_latency_ms: 1
    bandwidth_percent: 15
  
  - pcp: 6
    name: "ADAS Camera Data"
    max_latency_ms: 5
    bandwidth_percent: 30
  
  - pcp: 5
    name: "Diagnostics"
    max_latency_ms: 20
    bandwidth_percent: 10
  
  - pcp: 0
    name: "Infotainment"
    max_latency_ms: 100
    bandwidth_percent: 20
```

### Test Count Target

50+ tests covering:
- VLAN PCP extraction (10 tests)
- Traffic class mapping (8 tests)
- Stream identification (12 tests)
- Latency measurement (10 tests)
- TAS schedule parsing (10 tests)

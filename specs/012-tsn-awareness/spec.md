# Feature Specification: TSN Awareness (IEEE 802.1Qbv)

**Feature Branch**: `milestone/012-tsn-awareness`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M12 - TSN Awareness

## Overview

Implement Time-Sensitive Networking (TSN) awareness for deterministic Ethernet analysis. TSN is a set of IEEE 802.1 standards enabling deterministic, low-latency communication over Ethernet. 802.1Qbv (Time-Aware Shaper) is critical for automotive real-time applications.

## Clarifications

### Session 2026-01-13

- Q: When dealing with QinQ (double VLAN tags, 802.1ad), which PCP value should be used for priority classification? → A: Extract both outer and inner PCP, but outer PCP takes precedence for traffic classification and statistics (aligns with IEEE 802.1ad switch behavior)

- Q: For per-priority latency measurement (FR-020), how should "end-to-end latency" be calculated? → A: Inter-packet timing (measure time between consecutive packets of same priority) - practical for passive monitoring, provides useful jitter/interval metrics

- Q: For TAS schedule parsing (FR-008 to FR-013), what configuration format should be the primary implementation target? → A: JSON format with custom schema (simplest to implement and test, YANG support can be added later)

- Q: For stream tracking (FR-018), what should be the default idle timeout before a stream is marked as "Ended"? → A: 10 seconds (configurable) - balances detection of ended streams while tolerating automotive network gaps

- Q: What should be the maximum number of latency samples stored per priority class? → A: 10,000 samples per priority (~640KB total for 8 classes, sufficient for percentile calculation while bounding memory)

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
- **FR-003**: System MUST support QinQ (802.1ad) double VLAN tag PCP extraction (both inner and outer PCP extracted; outer PCP used for classification)
- **FR-004**: System MUST collect per-priority statistics (packet count, bytes, bandwidth)
- **FR-005**: System MUST calculate bandwidth percentage per traffic class
- **FR-006**: System MUST detect DEI (Drop Eligible Indicator) flag
- **FR-007**: System MUST handle untagged traffic as priority 0 (Best Effort)

#### TAS Schedule Parsing

- **FR-008**: System MUST parse Time-Aware Shaper gate control lists (JSON format as primary target)
- **FR-009**: System MUST extract gate control entries (time interval, gate state per class)
- **FR-010**: System MUST parse cycle time and base time
- **FR-011**: System MUST validate gate control list consistency
- **FR-012**: System MUST detect overlapping or invalid gate entries
- **FR-013**: System MUST support all 8 traffic classes (0-7) in schedule

#### Stream Identification

- **FR-014**: System MUST identify streams by 5-tuple (src_mac, dst_mac, vlan_id, pcp, ethertype)
- **FR-015**: System MUST track multiple concurrent streams
- **FR-016**: System MUST associate packets with streams
- **FR-017**: System MUST calculate per-stream statistics
- **FR-018**: System MUST detect stream start and end (idle timeout: 10 seconds default, configurable)
- **FR-019**: System MUST support stream filtering by ID

#### Latency Measurement

- **FR-020**: System MUST measure end-to-end latency per PCP value (using inter-packet timing method)
- **FR-021**: System MUST calculate latency percentiles (p50, p95, p99) per priority (max 10,000 samples per priority class)
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
- **FR-031**: System MUST provide gMock matchers: HasVlanPriority(), IsHighPriority(), IsLowPriority(), HasDei(), BelongsToStream()
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
- Hardware capture supports VLAN tag preservation (both 802.1Q and 802.1ad QinQ)
- TAS schedules provided via JSON configuration files (YANG support deferred)
- Timestamps available for latency measurement (hardware or software)
- Focus on priority-based analysis rather than full TAS gate enforcement simulation
- Latency measured using inter-packet timing (not request-response correlation)
- Stream idle timeout of 10 seconds balances detection needs with automotive network patterns
- Maximum 10,000 latency samples per priority class bounds memory usage (~640KB total)

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

## Detailed Implementation Plan

### Phase 1: VLAN Priority Enhancement (Week 1)

#### 1.1 Data Structures

**File**: `include/wadjet/protocols/tsn/vlan_priority.hpp`

```cpp
namespace wadjet::protocols::tsn {

// Priority Code Point (3 bits from VLAN tag)
enum class PriorityCodePoint : uint8_t {
    BestEffort = 0,      // BE
    Background = 1,      // BK
    ExcellentEffort = 2, // EE
    CriticalApp = 3,     // CA
    Video = 4,           // VI
    Voice = 5,           // VO
    InternetControl = 6, // IC
    NetworkControl = 7   // NC
};

// Traffic class metadata
struct TrafficClass {
    PriorityCodePoint pcp;
    std::string_view name;
    std::string_view description;
    uint32_t default_max_latency_us;  // Default latency budget
};

// Per-priority statistics
struct TrafficClassStats {
    PriorityCodePoint pcp;
    uint64_t packet_count{0};
    uint64_t byte_count{0};
    uint64_t bandwidth_bps{0};
    double bandwidth_percent{0.0};
    
    // Latency stats (if timestamps available)
    std::optional<LatencyStats> latency;
};

struct LatencyStats {
    uint64_t min_ns{std::numeric_limits<uint64_t>::max()};
    uint64_t max_ns{0};
    uint64_t mean_ns{0};
    uint64_t p50_ns{0};
    uint64_t p95_ns{0};
    uint64_t p99_ns{0};
    uint64_t violations{0};  // Count exceeding threshold
};

// VLAN priority information extracted from packet
struct VlanPriorityInfo {
    PriorityCodePoint pcp;
    bool dei;  // Drop Eligible Indicator
    
    // For QinQ (double VLAN)
    std::optional<PriorityCodePoint> inner_pcp;
    std::optional<bool> inner_dei;
};

class VlanPriorityExtractor {
public:
    // Extract PCP from 802.1Q VLAN tag
    static std::optional<VlanPriorityInfo> extract(const PacketView& packet);
    
    // Get traffic class metadata
    static const TrafficClass& get_traffic_class(PriorityCodePoint pcp);
    
    // Map PCP to class name
    static std::string_view to_string(PriorityCodePoint pcp);
};

} // namespace wadjet::protocols::tsn
```

#### 1.2 VLAN Decoder Enhancement

**File**: `src/protocols/ethernet/ethernet_decoder.cpp`

Enhance existing VLAN decoder to expose PCP:
- Extract 3-bit PCP field from VLAN TCI (bits 13-15)
- Extract 1-bit DEI field (bit 12)
- Handle QinQ double VLAN tags
- Store in decoded packet metadata

#### 1.3 Statistics Collector

**File**: `src/protocols/tsn/traffic_class_stats.cpp`

```cpp
class TrafficClassStatsCollector {
public:
    void process_packet(const PacketView& packet, 
                       std::optional<VlanPriorityInfo> priority);
    
    void finalize(std::chrono::nanoseconds duration);
    
    std::vector<TrafficClassStats> get_stats() const;
    
    // Export to JSON/CSV
    nlohmann::json to_json() const;
    std::string to_csv() const;
    
private:
    std::array<TrafficClassStats, 8> stats_;
    std::chrono::nanoseconds capture_duration_{0};
};
```

#### 1.4 Unit Tests

**File**: `tests/protocols/test_vlan_priority.cpp`

- Test PCP extraction for values 0-7
- Test DEI flag extraction
- Test QinQ double VLAN PCP extraction
- Test untagged traffic handling (default PCP 0)
- Test malformed VLAN tags
- Test traffic class mapping
- Test statistics collection
- Test JSON/CSV export

**Test Count**: 10 tests

---

### Phase 2: Stream Identification (Week 1-2)

#### 2.1 Data Structures

**File**: `include/wadjet/protocols/tsn/stream.hpp`

```cpp
namespace wadjet::protocols::tsn {

// Stream identifier: (MAC address, VLAN ID)
struct StreamId {
    net::MAC source_mac;
    uint16_t vlan_id;
    
    bool operator==(const StreamId& other) const;
    bool operator<(const StreamId& other) const;
    
    std::string to_string() const;
};

// Stream state
enum class StreamState {
    Active,
    Idle,
    Ended
};

// Per-stream statistics
struct StreamStats {
    StreamId id;
    StreamState state{StreamState::Active};
    
    // Timing
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    
    // Counters
    uint64_t packet_count{0};
    uint64_t byte_count{0};
    
    // Priority
    std::optional<PriorityCodePoint> expected_pcp;
    std::map<PriorityCodePoint, uint64_t> pcp_distribution;
    
    // Latency
    std::optional<LatencyStats> latency;
    
    // Bandwidth
    uint64_t bandwidth_bps{0};
};

class StreamTracker {
public:
    StreamTracker(std::chrono::seconds idle_timeout = std::chrono::seconds(10));
    
    void process_packet(const PacketView& packet,
                       const VlanPriorityInfo& priority,
                       std::chrono::system_clock::time_point timestamp);
    
    std::vector<StreamStats> get_all_streams() const;
    std::optional<StreamStats> get_stream(const StreamId& id) const;
    
    // Filter streams by criteria
    std::vector<StreamStats> filter_by_priority(PriorityCodePoint pcp) const;
    std::vector<StreamStats> filter_by_state(StreamState state) const;
    
    // Export
    nlohmann::json to_json() const;
    
private:
    void check_idle_timeout();
    
    std::map<StreamId, StreamStats> streams_;
    std::chrono::seconds idle_timeout_;
};

} // namespace wadjet::protocols::tsn
```

#### 2.2 Implementation Details

- Hash map for O(1) stream lookup by (MAC, VLAN ID)
- Idle timeout detection (configurable, default 10s)
- Per-stream statistics accumulation
- Priority violation detection (unexpected PCP for stream)

#### 2.3 Unit Tests

**File**: `tests/protocols/test_stream_tracking.cpp`

- Test stream identification
- Test multiple concurrent streams
- Test stream statistics collection
- Test idle timeout
- Test priority distribution tracking
- Test stream filtering
- Test stream lifecycle (start, active, end)
- Test edge cases (duplicate stream IDs, zero VLAN ID)

**Test Count**: 12 tests

---

### Phase 3: Latency Analysis (Week 2)

#### 3.1 Data Structures

**File**: `include/wadjet/protocols/tsn/latency_tracker.hpp`

```cpp
namespace wadjet::protocols::tsn {

// Latency measurement configuration
struct LatencyConfig {
    bool enable_per_priority{true};
    bool enable_per_stream{false};
    
    // Thresholds per priority (ns)
    std::array<uint64_t, 8> threshold_ns{
        100'000'000,  // PCP 0: 100ms
        100'000'000,  // PCP 1: 100ms
        50'000'000,   // PCP 2: 50ms
        20'000'000,   // PCP 3: 20ms
        5'000'000,    // PCP 4: 5ms
        5'000'000,    // PCP 5: 5ms
        2'000'000,    // PCP 6: 2ms
        1'000'000     // PCP 7: 1ms
    };
};

class LatencyTracker {
public:
    explicit LatencyTracker(LatencyConfig config = {});
    
    // Record packet with timestamp
    void record_packet(const PacketView& packet,
                      std::chrono::system_clock::time_point timestamp,
                      std::optional<VlanPriorityInfo> priority);
    
    // Calculate latency between request-response pairs
    // (requires packet correlation logic)
    void correlate_packets();
    
    // Get latency stats per priority
    std::map<PriorityCodePoint, LatencyStats> get_per_priority_stats() const;
    
    // Get latency stats per stream
    std::map<StreamId, LatencyStats> get_per_stream_stats() const;
    
    // Detect violations
    std::vector<LatencyViolation> get_violations() const;
    
private:
    LatencyConfig config_;
    std::map<PriorityCodePoint, std::vector<uint64_t>> latency_samples_;
    std::vector<LatencyViolation> violations_;
};

struct LatencyViolation {
    std::chrono::system_clock::time_point timestamp;
    PriorityCodePoint pcp;
    uint64_t measured_ns;
    uint64_t threshold_ns;
    std::string description;
};

} // namespace wadjet::protocols::tsn
```

#### 3.2 Latency Measurement Strategy

Two approaches:
1. **End-to-end correlation**: Match request-response packets (e.g., SOME/IP request-response, DoIP diagnostic)
2. **Per-hop measurement**: Use hardware timestamps if available

For Phase 3, focus on approach #2 (simpler):
- Track inter-packet arrival time per priority
- Calculate jitter (variance in latency)
- Detect threshold violations

#### 3.3 Unit Tests

**File**: `tests/protocols/test_latency_tracking.cpp`

- Test latency calculation
- Test per-priority latency stats
- Test percentile calculation (p50, p95, p99)
- Test threshold violation detection
- Test latency with hardware timestamps
- Test latency histogram generation
- Test edge cases (single packet, identical timestamps)

**Test Count**: 10 tests

---

### Phase 4: TAS Schedule Support (Week 3)

#### 4.1 Data Structures

**File**: `include/wadjet/protocols/tsn/schedule.hpp`

```cpp
namespace wadjet::protocols::tsn {

// Gate operation: open (true) or closed (false) for each traffic class
using GateStates = std::array<bool, 8>;

// Single gate control entry
struct GateControlEntry {
    std::chrono::nanoseconds time_interval;
    GateStates gate_states;  // Per traffic class (0-7)
    
    std::string to_string() const;
};

// Complete TAS schedule
struct TasSchedule {
    // Base time (absolute time when schedule starts)
    std::chrono::system_clock::time_point base_time;
    
    // Cycle time (schedule repeats every cycle_time)
    std::chrono::nanoseconds cycle_time;
    
    // Gate control list
    std::vector<GateControlEntry> gate_control_list;
    
    // Optional cycle extension
    std::optional<std::chrono::nanoseconds> cycle_extension;
    
    // Validate schedule consistency
    Result<void> validate() const;
    
    // Get gate state at specific time
    GateStates get_gate_states_at(std::chrono::system_clock::time_point time) const;
    
    // Calculate transmission window for priority
    std::vector<TimeWindow> get_transmission_windows(PriorityCodePoint pcp) const;
};

struct TimeWindow {
    std::chrono::nanoseconds start_offset;  // From cycle start
    std::chrono::nanoseconds duration;
};

// Schedule parser (from YANG model or JSON config)
class TasScheduleParser {
public:
    static Result<TasSchedule> parse_json(const nlohmann::json& config);
    static Result<TasSchedule> parse_yang(std::string_view yang_data);
};

// Schedule analyzer
class TasScheduleAnalyzer {
public:
    explicit TasScheduleAnalyzer(TasSchedule schedule);
    
    // Check if packet transmission time complies with schedule
    bool is_compliant(const PacketView& packet,
                     std::chrono::system_clock::time_point tx_time,
                     PriorityCodePoint pcp) const;
    
    // Detect schedule violations
    struct Violation {
        std::chrono::system_clock::time_point time;
        PriorityCodePoint pcp;
        std::string reason;
    };
    
    std::vector<Violation> find_violations(
        const std::vector<TimestampedPacket>& packets) const;
    
private:
    TasSchedule schedule_;
};

} // namespace wadjet::protocols::tsn
```

#### 4.2 Schedule Validation Rules

- Total of all gate control entry intervals must equal cycle time
- No overlapping time intervals
- Each traffic class must have at least one open gate per cycle (warning, not error)
- Base time should be aligned with gPTP epoch (if gPTP active)

#### 4.3 Example TAS Schedule JSON

```json
{
    "base_time": "2026-01-12T10:00:00.000000000Z",
    "cycle_time_ns": 1000000,
    "gate_control_list": [
        {
            "time_interval_ns": 200000,
            "gate_states": [false, false, false, false, false, false, true, true]
        },
        {
            "time_interval_ns": 300000,
            "gate_states": [false, false, false, false, true, true, false, false]
        },
        {
            "time_interval_ns": 500000,
            "gate_states": [true, true, true, true, false, false, false, false]
        }
    ]
}
```

#### 4.4 Unit Tests

**File**: `tests/protocols/test_tas_schedule.cpp`

- Test schedule parsing from JSON
- Test gate state calculation at specific time
- Test transmission window extraction
- Test schedule validation (valid schedule)
- Test schedule validation (invalid: intervals don't sum to cycle time)
- Test schedule validation (overlapping intervals)
- Test compliance checking
- Test violation detection

**Test Count**: 10 tests

---

### Phase 5: Analysis Tools & Integration (Week 3-4)

#### 5.1 TSN Analyzer Main Interface

**File**: `include/wadjet/protocols/tsn/tsn_analyzer.hpp`

```cpp
namespace wadjet::protocols::tsn {

class TsnAnalyzer {
public:
    struct Config {
        bool enable_priority_analysis{true};
        bool enable_stream_tracking{true};
        bool enable_latency_tracking{true};
        LatencyConfig latency_config;
        std::optional<TasSchedule> tas_schedule;
        std::chrono::seconds stream_idle_timeout{10};
    };
    
    explicit TsnAnalyzer(Config config = {});
    
    // Process packets
    void process_packet(const PacketView& packet,
                       std::chrono::system_clock::time_point timestamp);
    
    // Finalize analysis
    void finalize();
    
    // Results
    std::vector<TrafficClassStats> get_traffic_class_stats() const;
    std::vector<StreamStats> get_stream_stats() const;
    std::map<PriorityCodePoint, LatencyStats> get_latency_stats() const;
    
    // Reports
    struct Report {
        std::vector<TrafficClassStats> traffic_classes;
        std::vector<StreamStats> streams;
        std::map<PriorityCodePoint, LatencyStats> latency;
        std::vector<TasScheduleAnalyzer::Violation> tas_violations;
        
        nlohmann::json to_json() const;
        std::string to_text() const;
        std::string to_csv() const;
    };
    
    Report generate_report() const;
    
private:
    Config config_;
    TrafficClassStatsCollector traffic_class_stats_;
    StreamTracker stream_tracker_;
    LatencyTracker latency_tracker_;
    std::optional<TasScheduleAnalyzer> tas_analyzer_;
};

} // namespace wadjet::protocols::tsn
```

#### 5.2 gMock Matchers

**File**: `include/wadjet/testing/tsn_matchers.hpp`

```cpp
namespace wadjet::testing {

// Priority matchers
MATCHER_P(HasVlanPriority, pcp, "has VLAN priority") {
    auto priority = tsn::VlanPriorityExtractor::extract(arg);
    return priority && priority->pcp == pcp;
}

MATCHER(IsHighPriority, "is high priority (PCP 6-7)") {
    auto priority = tsn::VlanPriorityExtractor::extract(arg);
    return priority && (priority->pcp >= tsn::PriorityCodePoint::InternetControl);
}

MATCHER(IsLowPriority, "is low priority (PCP 0-3)") {
    auto priority = tsn::VlanPriorityExtractor::extract(arg);
    return priority && (priority->pcp <= tsn::PriorityCodePoint::CriticalApp);
}

MATCHER_P(HasDei, dei_value, "has DEI flag") {
    auto priority = tsn::VlanPriorityExtractor::extract(arg);
    return priority && priority->dei == dei_value;
}

// Latency matchers
MATCHER_P(HasLatencyBelow, threshold_ns, "has latency below threshold") {
    // Requires packet correlation - may need additional context
    return arg.latency_ns < threshold_ns;
}

// Stream matchers
MATCHER_P(BelongsToStream, stream_id, "belongs to stream") {
    auto stream = tsn::StreamId::from_packet(arg);
    return stream == stream_id;
}

} // namespace wadjet::testing
```

#### 5.3 Example Program

**File**: `examples/tsn_analyzer.cpp`

```cpp
#include <wadjet/io/capture_session.hpp>
#include <wadjet/protocols/tsn/tsn_analyzer.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: tsn_analyzer <pcap_file>\n";
        return 1;
    }
    
    // Create TSN analyzer
    wadjet::protocols::tsn::TsnAnalyzer::Config config;
    config.enable_priority_analysis = true;
    config.enable_stream_tracking = true;
    config.enable_latency_tracking = true;
    
    wadjet::protocols::tsn::TsnAnalyzer analyzer(config);
    
    // Open PCAP
    auto reader = wadjet::pcap::PcapReader::open(argv[1]);
    if (!reader) {
        std::cerr << "Failed to open " << argv[1] << "\n";
        return 1;
    }
    
    // Process packets
    while (auto packet = reader->read_packet()) {
        analyzer.process_packet(packet->view(), packet->timestamp());
    }
    
    // Finalize and generate report
    analyzer.finalize();
    auto report = analyzer.generate_report();
    
    // Print report
    std::cout << report.to_text() << "\n";
    
    // Save JSON report
    std::ofstream json_file("tsn_report.json");
    json_file << report.to_json().dump(2);
    
    return 0;
}
```

#### 5.4 Scenario Test

**File**: `examples/scenarios/tsn_priority_test.yaml`

```yaml
name: TSN Priority Distribution Test
description: Verify traffic class distribution in TSN network

steps:
  - capture:
      interface: eth0
      filter: "vlan"
      duration_ms: 5000

  - expect:
      ethernet:
        has_vlan: true
      vlan:
        priority: 7
      count: ">= 100"
      description: "High priority traffic present"

  - expect:
      ethernet:
        has_vlan: true
      vlan:
        priority: 0
      count: ">= 100"
      description: "Best effort traffic present"

  - log:
      message: "TSN traffic class analysis complete"
```

#### 5.5 Python Bindings

**File**: `bindings/python/src/tsn_bindings.cpp`

```cpp
#include <pybind11/pybind11.h>
#include <wadjet/protocols/tsn/tsn_analyzer.hpp>

namespace py = pybind11;
using namespace wadjet::protocols::tsn;

void init_tsn_bindings(py::module_& m) {
    py::enum_<PriorityCodePoint>(m, "PriorityCodePoint")
        .value("BestEffort", PriorityCodePoint::BestEffort)
        .value("Background", PriorityCodePoint::Background)
        .value("ExcellentEffort", PriorityCodePoint::ExcellentEffort)
        .value("CriticalApp", PriorityCodePoint::CriticalApp)
        .value("Video", PriorityCodePoint::Video)
        .value("Voice", PriorityCodePoint::Voice)
        .value("InternetControl", PriorityCodePoint::InternetControl)
        .value("NetworkControl", PriorityCodePoint::NetworkControl);
    
    py::class_<VlanPriorityInfo>(m, "VlanPriorityInfo")
        .def_readonly("pcp", &VlanPriorityInfo::pcp)
        .def_readonly("dei", &VlanPriorityInfo::dei);
    
    py::class_<TrafficClassStats>(m, "TrafficClassStats")
        .def_readonly("pcp", &TrafficClassStats::pcp)
        .def_readonly("packet_count", &TrafficClassStats::packet_count)
        .def_readonly("byte_count", &TrafficClassStats::byte_count)
        .def_readonly("bandwidth_percent", &TrafficClassStats::bandwidth_percent);
    
    py::class_<TsnAnalyzer>(m, "TsnAnalyzer")
        .def(py::init<>())
        .def("process_packet", &TsnAnalyzer::process_packet)
        .def("finalize", &TsnAnalyzer::finalize)
        .def("get_traffic_class_stats", &TsnAnalyzer::get_traffic_class_stats)
        .def("generate_report", &TsnAnalyzer::generate_report);
}
```

**Python example**: `examples/python/tsn_analysis.py`

```python
#!/usr/bin/env python3
import wadjet

# Create TSN analyzer
analyzer = wadjet.TsnAnalyzer()

# Open PCAP
reader = wadjet.PcapReader.open("capture.pcap")

# Process packets
for packet in reader:
    analyzer.process_packet(packet.view(), packet.timestamp())

# Get results
analyzer.finalize()
report = analyzer.generate_report()

# Print traffic class distribution
for tc_stats in report.traffic_classes:
    print(f"PCP {tc_stats.pcp}: {tc_stats.packet_count} packets, "
          f"{tc_stats.bandwidth_percent:.2f}% bandwidth")
```

#### 5.6 Documentation

**File**: `docs/protocols/tsn.md`

Sections:
- TSN Overview
- IEEE 802.1Q VLAN Priority
- IEEE 802.1Qbv Time-Aware Shaper
- Traffic Class Mapping
- Priority-Based Analysis
- Stream Identification
- TAS Schedule Configuration
- API Reference
- Examples

---

### Phase 6: Testing & Validation (Week 4)

#### 6.1 Integration Tests

**File**: `tests/integration/test_tsn_integration.cpp`

- Full TSN analysis workflow
- Multi-priority traffic mix
- gPTP + TSN correlation
- TAS schedule compliance
- Python bindings integration
- Scenario test execution

**Test Count**: 8 tests

#### 6.2 Fuzz Testing

**File**: `fuzz/tsn_fuzzer.cpp`

- Fuzz VLAN tag parsing
- Fuzz TAS schedule parsing
- Fuzz stream identification

#### 6.3 Regression Test Captures

Create `pcap_samples/tsn/` with:
- `multi_priority.pcap` - Traffic with PCP 0-7
- `qinq_double_vlan.pcap` - QinQ packets
- `high_priority_only.pcap` - Only PCP 6-7
- `automotive_mix.pcap` - Realistic automotive TSN

---

### File Structure Summary

```
include/wadjet/protocols/tsn/
  ├── tsn.hpp                    // Main TSN header
  ├── vlan_priority.hpp          // Priority structures & extraction
  ├── schedule.hpp               // TAS schedule structures
  ├── stream.hpp                 // Stream identification
  ├── tsn_analyzer.hpp           // Main analyzer interface
  └── latency_tracker.hpp        // Latency measurement

src/protocols/tsn/
  ├── vlan_priority_extractor.cpp
  ├── traffic_class_stats.cpp
  ├── stream_tracker.cpp
  ├── latency_tracker.cpp
  ├── tas_schedule.cpp
  ├── tas_schedule_parser.cpp
  ├── tas_schedule_analyzer.cpp
  └── tsn_analyzer.cpp

include/wadjet/testing/
  └── tsn_matchers.hpp           // gMock matchers

tests/protocols/
  ├── test_vlan_priority.cpp     // 10 tests
  ├── test_stream_tracking.cpp   // 12 tests
  ├── test_latency_tracking.cpp  // 10 tests
  └── test_tas_schedule.cpp      // 10 tests

tests/integration/
  └── test_tsn_integration.cpp   // 8 tests

examples/
  ├── tsn_analyzer.cpp           // CLI analyzer tool
  └── scenarios/
      └── tsn_priority_test.yaml

examples/python/
  └── tsn_analysis.py

bindings/python/src/
  └── tsn_bindings.cpp

docs/protocols/
  └── tsn.md

pcap_samples/tsn/
  ├── multi_priority.pcap
  ├── qinq_double_vlan.pcap
  ├── high_priority_only.pcap
  └── automotive_mix.pcap
```

---

## Implementation Checklist

### Week 1: Priority & Streams

- [ ] Create TSN directory structure
- [ ] Implement `vlan_priority.hpp` data structures
- [ ] Implement `VlanPriorityExtractor`
- [ ] Enhance VLAN decoder with PCP extraction
- [ ] Implement `TrafficClassStatsCollector`
- [ ] Write 10 priority tests
- [ ] Implement `stream.hpp` data structures
- [ ] Implement `StreamTracker`
- [ ] Write 12 stream tracking tests

### Week 2: Latency & Schedule

- [ ] Implement `latency_tracker.hpp` data structures
- [ ] Implement `LatencyTracker`
- [ ] Write 10 latency tests
- [ ] Implement `schedule.hpp` data structures
- [ ] Implement `TasSchedule` validation
- [ ] Implement `TasScheduleParser` (JSON)
- [ ] Implement `TasScheduleAnalyzer`
- [ ] Write 10 TAS schedule tests

### Week 3: Integration

- [ ] Implement `TsnAnalyzer` main interface
- [ ] Implement report generation (JSON, text, CSV)
- [ ] Create `tsn_matchers.hpp`
- [ ] Write `tsn_analyzer.cpp` example
- [ ] Create TSN scenario tests
- [ ] Write 8 integration tests

### Week 4: Bindings & Documentation

- [ ] Python bindings for TSN
- [ ] Python example script
- [ ] Rust bindings (optional)
- [ ] C ABI layer
- [ ] Write `docs/protocols/tsn.md`
- [ ] Update architecture documentation
- [ ] Create regression PCAP samples
- [ ] Fuzz testing
- [ ] Final validation

---

## Performance Considerations

- **Zero-copy**: PCP extraction should not copy packet data
- **O(1) stream lookup**: Use hash map for stream tracker
- **Latency overhead**: Minimize analysis impact on capture performance
- **Memory**: Bound latency sample storage (e.g., max 10,000 samples per priority)
- **Thread-safety**: TsnAnalyzer should be thread-safe for multi-threaded capture

---

## Testing Strategy

### Unit Tests (50 tests total)

- VLAN priority: 10 tests
- Stream tracking: 12 tests
- Latency tracking: 10 tests
- TAS schedule: 10 tests
- Integration: 8 tests

### Test Coverage Goals

- Line coverage: ≥90%
- Branch coverage: ≥85%
- All public APIs tested

### Validation Approach

1. **Synthetic traffic**: Generate test packets with scapy
2. **Real captures**: Use TSN-capable switches to capture real traffic
3. **Cross-validation**: Compare with Wireshark TSN analysis (if available)
4. **Automotive scenarios**: Validate against typical automotive priority mappings

---

## Risk Mitigation

### Technical Risks

| Risk | Mitigation |
|------|------------|
| VLAN tag stripping by NIC | Document requirement for VLAN tag preservation |
| Limited TSN switch access | Use synthetic traffic for testing |
| Hardware timestamp unavailable | Fallback to software timestamps with warning |
| TAS schedule format variations | Support multiple formats (JSON, YANG) |
| Large memory for latency tracking | Implement sampling or bounded storage |

### Schedule Risks

| Risk | Mitigation |
|------|------------|
| Complexity underestimated | Phase 4 (TAS) is optional, can be deferred |
| Dependency on gPTP | gPTP already implemented (M8), no blocker |
| Testing delays | Start with synthetic traffic, add real captures later |

---

## Success Metrics

- **FR-001 to FR-034**: All functional requirements met
- **SC-001 to SC-010**: All success criteria achieved
- **50+ tests**: All passing
- **Documentation**: Complete TSN guide with examples
- **Performance**: <5% overhead on packet capture rate
- **Example programs**: Working tsn_analyzer demonstrating all features

---

## Future Enhancements (Out of Scope)

These can be addressed in future milestones:

- IEEE 802.1Qbu frame preemption detection
- IEEE 802.1CB frame replication and elimination (FRER)
- TSN configuration generation
- Active TAS enforcement/simulation
- Integration with Centralized Network Configuration (CNC)
- Full gPTP clock synchronization math
- Advanced traffic shaping analysis
- Credit-Based Shaper (CBS) analysis
- Asynchronous Traffic Shaper (ATS) support

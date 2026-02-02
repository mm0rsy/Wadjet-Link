# TSN Data Model

## Entity: PriorityCodePoint (Enum)

- **Type**: Enumeration (uint8_t, values 0-7)
- **Purpose**: Represent VLAN priority level per IEEE 802.1Q
- **Values**:
  - `BestEffort = 0` (BE)
  - `Background = 1` (BK)
  - `ExcellentEffort = 2` (EE)
  - `CriticalApplications = 3` (CA)
  - `Video = 4` (VI)
  - `Voice = 5` (VO)
  - `InternetworkControl = 6` (IC)
  - `NetworkControl = 7` (NC)
- **Validation**: Range check [0-7]
- **Serialization**: Integer 0-7 in JSON

---

## Entity: VlanPriorityInfo (Struct)

**Purpose**: Contains extracted VLAN priority information from packet

**Fields**:
- `pcp`: PriorityCodePoint (3 bits from VLAN TCI field)
- `dei`: bool (Drop Eligible Indicator, bit 12 of TCI)
- `has_vlan`: bool (true if VLAN tag present)
- `is_qinq`: bool (true if 802.1ad double-tagged)
- `inner_pcp`: optional<PriorityCodePoint> (for QinQ only)
- `inner_dei`: optional<bool> (for QinQ only)

**Relationships**: Extracted from Ethernet/VLAN layer

**Lifecycle**: Created per-packet during decode

**Invariants**:
- If `has_vlan == false`, PCP defaults to 0 (Best Effort)
- If `is_qinq == true`, both outer and inner PCP available
- Outer PCP always used for traffic classification

**Example**:
```cpp
VlanPriorityInfo {
    .pcp = PriorityCodePoint::NetworkControl,  // 7
    .dei = false,
    .has_vlan = true,
    .is_qinq = false,
    .inner_pcp = std::nullopt,
    .inner_dei = std::nullopt
}
```

---

## Entity: TrafficClassStats (Struct)

**Purpose**: Aggregated statistics per priority level

**Fields**:
- `pcp`: PriorityCodePoint
- `packet_count`: uint64_t (total packets with this PCP)
- `byte_count`: uint64_t (total bytes with this PCP)
- `bandwidth_bps`: uint64_t (calculated as byte_count / duration)
- `bandwidth_percent`: double (percentage of total bandwidth)
- `latency`: optional<LatencyStats> (if latency tracking enabled)

**Relationships**: Aggregated from multiple packets with same PCP

**Lifecycle**: Accumulated during capture, finalized at end

**Derivations**:
- `bandwidth_bps = byte_count * 8 / capture_duration_seconds`
- `bandwidth_percent = (byte_count / total_bytes) * 100.0`

**Example**:
```cpp
TrafficClassStats {
    .pcp = PriorityCodePoint::NetworkControl,
    .packet_count = 1250,
    .byte_count = 180000,
    .bandwidth_bps = 1440000,  // 1.44 Mbps
    .bandwidth_percent = 15.2,
    .latency = LatencyStats{...}
}
```

---

## Entity: StreamId (Struct)

**Purpose**: Unique identifier for TSN stream (5-tuple)

**Fields**:
- `src_mac`: MAC address (6 bytes)
- `dst_mac`: MAC address (6 bytes)
- `vlan_id`: uint16_t (12-bit VLAN identifier)
- `pcp`: PriorityCodePoint (expected priority)
- `ethertype`: uint16_t (protocol identifier)

**Relationships**: Identifies unique TSN stream

**Lifecycle**: Created when first packet seen, persists until idle timeout

**Equality**: Implements operator== for all 5 fields

**Hash**: Custom std::hash specialization for unordered_map usage

**Example**:
```cpp
StreamId {
    .src_mac = {0x00, 0x1B, 0x21, 0xAA, 0xBB, 0xCC},
    .dst_mac = {0x00, 0x1B, 0x21, 0xDD, 0xEE, 0xFF},
    .vlan_id = 100,
    .pcp = PriorityCodePoint::Video,
    .ethertype = 0x0800  // IPv4
}
```

---

## Entity: StreamState (Enum)

**Purpose**: Lifecycle state of TSN stream

**Values**:
- `UNKNOWN = 0`: Initial state
- `ACTIVE = 1`: Currently transmitting (recent packet received)
- `IDLE = 2`: No packets for idle_timeout duration
- `ENDED = 3`: Explicitly closed or analysis complete

**Transitions**:
```
UNKNOWN -> ACTIVE  (first packet)
ACTIVE -> IDLE     (idle_timeout exceeded)
IDLE -> ACTIVE     (packet received after idle)
IDLE -> ENDED      (finalization or explicit close)
ACTIVE -> ENDED    (finalization)
```

---

## Entity: StreamStats (Struct)

**Purpose**: Per-stream statistics and state

**Fields**:
- `id`: StreamId (unique stream identifier)
- `state`: StreamState (current lifecycle state)
- `first_seen`: timestamp (first packet time)
- `last_seen`: timestamp (most recent packet time)
- `packet_count`: uint64_t (total packets in stream)
- `byte_count`: uint64_t (total bytes in stream)
- `expected_pcp`: PriorityCodePoint (from StreamId)
- `pcp_distribution`: map<PriorityCodePoint, uint64_t> (actual PCP values seen)
- `latency`: optional<LatencyStats> (if latency tracking enabled)
- `bandwidth_bps`: uint64_t (calculated bandwidth)

**Relationships**: One per StreamId

**Lifecycle**:
1. Created on first packet with unique StreamId
2. Updated per packet (increment counters, update last_seen)
3. Marked IDLE if (current_time - last_seen) > idle_timeout
4. Finalized on analysis end or explicit close

**Derivations**:
- `duration = last_seen - first_seen`
- `bandwidth_bps = (byte_count * 8) / duration_seconds`

**Validation**:
- `pcp_distribution[expected_pcp]` should dominate for well-configured streams
- If other PCP values significant, indicates priority misconfiguration

**Example**:
```cpp
StreamStats {
    .id = StreamId{...},
    .state = StreamState::ACTIVE,
    .first_seen = 1673612345.123456789,
    .last_seen = 1673612347.987654321,
    .packet_count = 450,
    .byte_count = 64800,
    .expected_pcp = PriorityCodePoint::Video,
    .pcp_distribution = {{Video, 445}, {BestEffort, 5}},
    .latency = LatencyStats{...},
    .bandwidth_bps = 194400  // ~200 Kbps
}
```

---

## Entity: LatencyConfig (Struct)

**Purpose**: Configuration for latency tracking

**Fields**:
- `thresholds_per_pcp`: array<nanoseconds, 8> (threshold per priority)
- `max_samples_per_priority`: size_t (default 10,000)
- `enable_histogram`: bool (default false)
- `histogram_bins`: size_t (default 10, only if histogram enabled)

**Defaults**:
```cpp
LatencyConfig {
    .thresholds_per_pcp = {
        100'000'000,  // PCP 0: 100ms
        100'000'000,  // PCP 1: 100ms
        100'000'000,  // PCP 2: 100ms
        100'000'000,  // PCP 3: 100ms
        5'000'000,    // PCP 4: 5ms (Video)
        5'000'000,    // PCP 5: 5ms (Voice)
        2'000'000,    // PCP 6: 2ms (Control)
        1'000'000     // PCP 7: 1ms (Safety)
    },
    .max_samples_per_priority = 10'000,
    .enable_histogram = false,
    .histogram_bins = 10
}
```

---

## Entity: LatencyStats (Struct)

**Purpose**: Statistical summary of latency measurements

**Fields**:
- `sample_count`: uint64_t (total samples collected)
- `min_ns`: uint64_t (minimum latency observed)
- `max_ns`: uint64_t (maximum latency observed)
- `mean_ns`: double (average latency)
- `stddev_ns`: double (standard deviation, jitter indicator)
- `p50_ns`: uint64_t (median latency)
- `p95_ns`: uint64_t (95th percentile)
- `p99_ns`: uint64_t (99th percentile)
- `violation_count`: uint64_t (samples exceeding threshold)

**Relationships**: Calculated from latency sample array

**Lifecycle**: Computed during finalization using nth_element algorithm

**Example**:
```cpp
LatencyStats {
    .sample_count = 9850,
    .min_ns = 125'000,        // 125 µs
    .max_ns = 8'500'000,      // 8.5 ms
    .mean_ns = 450'000.0,     // 450 µs
    .stddev_ns = 120'000.0,   // 120 µs jitter
    .p50_ns = 400'000,        // 400 µs median
    .p95_ns = 750'000,        // 750 µs
    .p99_ns = 1'200'000,      // 1.2 ms
    .violation_count = 3      // 3 samples > threshold
}
```

---

## Entity: TasSchedule (Struct)

**Purpose**: IEEE 802.1Qbv Time-Aware Shaper schedule

**Fields**:
- `base_time_ns`: uint64_t (absolute start time in nanoseconds since epoch)
- `cycle_time_ns`: uint64_t (schedule repeat interval)
- `gate_control_list`: vector<GateControlEntry> (time-ordered gate states)
- `cycle_extension_ns`: optional<uint64_t> (optional cycle extension)
- `is_admin`: bool (true = admin schedule, false = operational)

**Relationships**: Defines expected transmission windows per traffic class

**Lifecycle**: Loaded from JSON config, static during analysis

**Validation**:
- Sum of all gate_control_list[i].time_interval_ns must equal cycle_time_ns
- No overlapping intervals
- At least one entry in gate_control_list

**Example**:
```cpp
TasSchedule {
    .base_time_ns = 0,
    .cycle_time_ns = 1'000'000,  // 1ms cycle
    .gate_control_list = {
        GateControlEntry{200'000, {1,1,0,0,0,0,0,0}},  // PCP 6-7 open
        GateControlEntry{300'000, {0,0,1,1,0,0,0,0}},  // PCP 4-5 open
        GateControlEntry{500'000, {0,0,0,0,1,1,1,1}}   // PCP 0-3 open
    },
    .cycle_extension_ns = std::nullopt,
    .is_admin = true
}
```

---

## Entity: GateControlEntry (Struct)

**Purpose**: Single gate control entry in TAS schedule

**Fields**:
- `time_interval_ns`: uint64_t (duration of this gate state)
- `gate_states`: array<bool, 8> (one per traffic class, index = PCP value)

**Relationships**: Part of TasSchedule, multiple entries per schedule

**Lifecycle**: Immutable once parsed from JSON

**Invariants**:
- `time_interval_ns > 0`
- Index 0 of gate_states = PCP 0, index 7 = PCP 7

**Example**:
```cpp
GateControlEntry {
    .time_interval_ns = 200'000,  // 200 µs
    .gate_states = {
        false,  // PCP 0: Best Effort (closed)
        false,  // PCP 1: Background (closed)
        false,  // PCP 2: Excellent Effort (closed)
        false,  // PCP 3: Critical Applications (closed)
        false,  // PCP 4: Video (closed)
        false,  // PCP 5: Voice (closed)
        true,   // PCP 6: Internetwork Control (OPEN)
        true    // PCP 7: Network Control (OPEN)
    }
}
```

---

## Entity: LatencyViolation (Struct)

**Purpose**: Record of latency threshold violation

**Fields**:
- `timestamp`: timestamp (when violation occurred)
- `stream_id`: optional<StreamId> (if stream tracking enabled)
- `pcp`: PriorityCodePoint (which priority violated)
- `measured_latency_ns`: uint64_t (actual latency)
- `threshold_ns`: uint64_t (configured threshold)

**Relationships**: Collected in violation log during analysis

**Lifecycle**: Created when sample exceeds threshold

**Example**:
```cpp
LatencyViolation {
    .timestamp = 1673612345.123456789,
    .stream_id = StreamId{...},
    .pcp = PriorityCodePoint::NetworkControl,
    .measured_latency_ns = 2'500'000,  // 2.5ms
    .threshold_ns = 1'000'000          // 1ms threshold
}
```

---

## Entity: TsnAnalyzer::Config (Struct)

**Purpose**: Configuration for TsnAnalyzer

**Fields**:
- `enable_priority_tracking`: bool (default true)
- `enable_stream_tracking`: bool (default true)
- `enable_latency_tracking`: bool (default true)
- `enable_tas_validation`: bool (default false)
- `latency_config`: LatencyConfig (latency tracking settings)
- `tas_schedule`: optional<TasSchedule> (if TAS validation enabled)
- `stream_idle_timeout_ms`: uint64_t (default 10,000 = 10 seconds)

**Example**:
```cpp
TsnAnalyzer::Config {
    .enable_priority_tracking = true,
    .enable_stream_tracking = true,
    .enable_latency_tracking = true,
    .enable_tas_validation = false,
    .latency_config = LatencyConfig{},  // defaults
    .tas_schedule = std::nullopt,
    .stream_idle_timeout_ms = 10'000
}
```

---

## Entity: TsnAnalyzer::Report (Struct)

**Purpose**: Comprehensive TSN analysis report

**Fields**:
- `priority_stats`: array<TrafficClassStats, 8> (stats per PCP)
- `stream_stats`: vector<StreamStats> (all tracked streams)
- `latency_violations`: vector<LatencyViolation> (threshold violations)
- `tas_violations`: vector<TasViolation> (schedule compliance violations)
- `total_packets`: uint64_t (total packets analyzed)
- `total_bytes`: uint64_t (total bytes analyzed)
- `capture_duration_s`: double (analysis duration in seconds)

**Relationships**: Aggregates all analysis results

**Lifecycle**: Generated by finalize() + generate_report()

**Methods**:
- `to_text()`: Human-readable text report
- `to_json()`: JSON export (nlohmann::json)
- `to_csv()`: CSV export for spreadsheet analysis

**Example**:
```cpp
TsnAnalyzer::Report {
    .priority_stats = [...],  // 8 entries
    .stream_stats = [...],    // N streams
    .latency_violations = [...],
    .tas_violations = [...],
    .total_packets = 125'000,
    .total_bytes = 18'000'000,
    .capture_duration_s = 10.0
}
```

---

## Relationships Diagram

```
TsnAnalyzer
    ├── Config
    │   ├── LatencyConfig
    │   └── TasSchedule
    │       └── GateControlEntry[]
    └── Report
        ├── TrafficClassStats[8]
        │   ├── PriorityCodePoint
        │   └── LatencyStats
        ├── StreamStats[]
        │   ├── StreamId
        │   ├── StreamState
        │   └── LatencyStats
        ├── LatencyViolation[]
        └── TasViolation[]

VlanPriorityInfo (per-packet)
    └── PriorityCodePoint
```

---

## Memory Bounds

| Entity | Max Count | Size per Entity | Total Memory |
|--------|-----------|----------------|--------------|
| TrafficClassStats | 8 (fixed) | ~200 bytes | ~1.6 KB |
| StreamStats | 1,000 (configurable) | ~400 bytes | ~400 KB |
| Latency Samples | 80,000 (10K × 8 priorities) | 8 bytes | ~640 KB |
| TasSchedule | 1 (single schedule) | Variable | <10 KB |
| **Total** | - | - | **~1.05 MB** |

All data structures have bounded memory growth to prevent unbounded memory consumption during long captures.

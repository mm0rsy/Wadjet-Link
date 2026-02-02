# TSN Types Contract

**Phase 1 Deliverable**: Type definitions  
**DO NOT IMPLEMENT** - Contract only

**Namespace**: `wadjet::tsn`

---

## Enum: PriorityCodePoint

IEEE 802.1Q Priority Code Point values

**Type**: `enum class PriorityCodePoint : uint8_t`

### Values

| Name | Value | Description | Traffic Class |
|------|-------|-------------|---------------|
| `BestEffort` | `0` | Default, lowest priority | BE |
| `Background` | `1` | Bulk transfer | BK |
| `ExcellentEffort` | `2` | Better than best effort | EE |
| `CriticalApplications` | `3` | Real-time data | CA |
| `Video` | `4` | Streaming video | VI |
| `Voice` | `5` | VoIP, telephony | VO |
| `InternetworkControl` | `6` | Network control | IC |
| `NetworkControl` | `7` | Highest priority (safety-critical) | NC |

### Notes

- Maps to 3-bit PCP field in VLAN TCI header
- Values represent traffic class priority (0 = lowest, 7 = highest)

---

## Struct: VlanPriorityInfo

VLAN Priority Information extracted from packet

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `pcp` | `PriorityCodePoint` | Priority Code Point (3 bits from VLAN TCI) |
| `dei` | `bool` | Drop Eligible Indicator (bit 12 of TCI) |
| `has_vlan` | `bool` | True if VLAN tag present |
| `is_qinq` | `bool` | True if IEEE 802.1ad double-tagged |
| `inner_pcp` | `optional<PriorityCodePoint>` | Inner PCP (for QinQ only) |
| `inner_dei` | `optional<bool>` | Inner DEI (for QinQ only) |

### Invariants

- If `has_vlan == false`, PCP defaults to 0 (Best Effort)
- If `is_qinq == true`, both outer and inner PCP available
- Outer PCP always used for traffic classification

### Methods

#### get_classification_priority

```cpp
PriorityCodePoint get_classification_priority() const
```

Get priority for traffic classification (returns outer PCP for QinQ)

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

---

## Struct: StreamId

TSN stream identifier (5-tuple)

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `src_mac` | `array<uint8_t, 6>` | Source MAC address |
| `dst_mac` | `array<uint8_t, 6>` | Destination MAC address |
| `vlan_id` | `uint16_t` | VLAN ID (12-bit) |
| `pcp` | `PriorityCodePoint` | Expected priority |
| `ethertype` | `uint16_t` | Protocol identifier |

### Operators

- `operator==(const StreamId& other) const`: Equality comparison
- `operator!=(const StreamId& other) const`: Inequality comparison

### Methods

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

### Hash Support

Must provide `std::hash<StreamId>` specialization for use in `unordered_map`

---

## Enum: StreamState

Stream lifecycle state

**Type**: `enum class StreamState : uint8_t`

### Values

| Name | Value | Description |
|------|-------|-------------|
| `UNKNOWN` | `0` | Initial state |
| `ACTIVE` | `1` | Currently transmitting |
| `IDLE` | `2` | No packets for idle_timeout duration |
| `ENDED` | `3` | Explicitly closed or analysis complete |

### State Transitions

```
UNKNOWN -> ACTIVE  (first packet)
ACTIVE -> IDLE     (idle_timeout exceeded)
IDLE -> ACTIVE     (packet received after idle)
IDLE -> ENDED      (finalization or explicit close)
ACTIVE -> ENDED    (finalization)
```

---

## Struct: LatencyConfig

Latency tracking configuration

### Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `thresholds_per_pcp` | `array<uint64_t, 8>` | Automotive defaults | Threshold in nanoseconds per PCP |
| `max_samples_per_priority` | `size_t` | `10000` | Maximum samples to store per priority |
| `enable_histogram` | `bool` | `false` | Enable latency histogram |
| `histogram_bins` | `size_t` | `10` | Number of histogram bins (if enabled) |

### Static Methods

#### automotive_defaults

```cpp
static LatencyConfig automotive_defaults()
```

Create configuration with automotive-specific thresholds:
- PCP 7 (Safety): 1ms
- PCP 6 (Control): 2ms
- PCP 4-5 (Video/Voice): 5ms
- PCP 0-3 (Best Effort): 100ms

### Methods

#### validate

```cpp
void validate() const
```

Validate configuration

**Throws**: `std::invalid_argument` if invalid

---

## Struct: LatencyStats

Latency statistics summary

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `sample_count` | `uint64_t` | Total samples collected |
| `min_ns` | `uint64_t` | Minimum latency observed (nanoseconds) |
| `max_ns` | `uint64_t` | Maximum latency observed (nanoseconds) |
| `mean_ns` | `double` | Average latency (nanoseconds) |
| `stddev_ns` | `double` | Standard deviation (jitter indicator, nanoseconds) |
| `p50_ns` | `uint64_t` | Median latency (nanoseconds) |
| `p95_ns` | `uint64_t` | 95th percentile (nanoseconds) |
| `p99_ns` | `uint64_t` | 99th percentile (nanoseconds) |
| `violation_count` | `uint64_t` | Samples exceeding threshold |

### Methods

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

---

## Struct: TrafficClassStats

Traffic class statistics per PCP

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `pcp` | `PriorityCodePoint` | Priority code point |
| `packet_count` | `uint64_t` | Total packets with this PCP |
| `byte_count` | `uint64_t` | Total bytes with this PCP |
| `bandwidth_bps` | `uint64_t` | Calculated bandwidth (bytes/second × 8) |
| `bandwidth_percent` | `double` | Percentage of total bandwidth |
| `latency` | `optional<LatencyStats>` | Latency statistics (if tracking enabled) |

### Derived Values

- `bandwidth_bps = byte_count * 8 / capture_duration_seconds`
- `bandwidth_percent = (byte_count / total_bytes) * 100.0`

### Methods

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

---

## Struct: StreamStats

Per-stream statistics and state

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | `StreamId` | Unique stream identifier |
| `state` | `StreamState` | Current lifecycle state |
| `first_seen` | `double` | First packet timestamp |
| `last_seen` | `double` | Most recent packet timestamp |
| `packet_count` | `uint64_t` | Total packets in stream |
| `byte_count` | `uint64_t` | Total bytes in stream |
| `expected_pcp` | `PriorityCodePoint` | Expected PCP from StreamId |
| `pcp_distribution` | `map<PriorityCodePoint, uint64_t>` | Actual PCP values seen |
| `latency` | `optional<LatencyStats>` | Latency stats (if tracking enabled) |
| `bandwidth_bps` | `uint64_t` | Calculated bandwidth |

### Lifecycle

1. Created on first packet with unique StreamId
2. Updated per packet (increment counters, update last_seen)
3. Marked IDLE if (current_time - last_seen) > idle_timeout
4. Finalized on analysis end or explicit close

### Derived Values

- `duration = last_seen - first_seen`
- `bandwidth_bps = (byte_count * 8) / duration_seconds`

### Validation

- `pcp_distribution[expected_pcp]` should dominate for well-configured streams
- If other PCP values significant, indicates priority misconfiguration

### Methods

#### has_priority_mismatch

```cpp
bool has_priority_mismatch(double threshold_percent = 5.0) const
```

Check if stream has priority mismatch

**Parameters**:
- `threshold_percent`: Mismatch threshold (0-100)

**Returns**: `true` if mismatch exceeds threshold

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

---

## Struct: GateControlEntry

TAS gate control entry (IEEE 802.1Qbv)

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `time_interval_ns` | `uint64_t` | Duration of this gate state (nanoseconds) |
| `gate_states` | `array<bool, 8>` | Gate state per traffic class (index = PCP value, true = open) |

### Invariants

- `time_interval_ns > 0`
- Index 0 of gate_states = PCP 0, index 7 = PCP 7

### Methods

#### is_open_for_pcp

```cpp
bool is_open_for_pcp(PriorityCodePoint pcp) const
```

Check if gate is open for given PCP

#### from_json

```cpp
static GateControlEntry from_json(const nlohmann::json& j)
```

Parse from JSON

#### to_json

```cpp
nlohmann::json to_json() const
```

Convert to JSON

---

## Struct: TasSchedule

TAS schedule (IEEE 802.1Qbv Time-Aware Shaper)

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `base_time_ns` | `uint64_t` | Absolute start time (nanoseconds since epoch) |
| `cycle_time_ns` | `uint64_t` | Schedule repeat interval (nanoseconds) |
| `gate_control_list` | `vector<GateControlEntry>` | Time-ordered gate states |
| `cycle_extension_ns` | `optional<uint64_t>` | Optional cycle extension |
| `is_admin` | `bool` | True = admin schedule, false = operational |

### Validation Rules

- Sum of all `gate_control_list[i].time_interval_ns` must equal `cycle_time_ns`
- No overlapping intervals
- At least one entry in gate_control_list

### Methods

#### validate

```cpp
void validate() const
```

Validate schedule consistency

**Throws**: `std::invalid_argument` if invalid

#### get_gate_state_at

```cpp
const GateControlEntry& get_gate_state_at(uint64_t timestamp_ns) const
```

Get gate state at specific time

**Parameters**:
- `timestamp_ns`: Absolute timestamp

**Returns**: Gate control entry active at this time

#### from_json

```cpp
static TasSchedule from_json(const nlohmann::json& j)
```

Parse from JSON file or string

#### to_json

```cpp
nlohmann::json to_json() const
```

Convert to JSON

---

## Struct: LatencyViolation

Latency threshold violation record

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | `double` | When violation occurred |
| `stream_id` | `optional<StreamId>` | Stream ID (if stream tracking enabled) |
| `pcp` | `PriorityCodePoint` | Which priority violated |
| `measured_latency_ns` | `uint64_t` | Actual latency |
| `threshold_ns` | `uint64_t` | Configured threshold |

### Methods

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

---

## Struct: TasViolation

TAS schedule violation record

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | `double` | When violation occurred |
| `stream_id` | `optional<StreamId>` | Stream ID (if stream tracking enabled) |
| `pcp` | `PriorityCodePoint` | Which priority violated |
| `expected_open` | `bool` | True if gate should be open, false if closed |
| `cycle_offset_ns` | `uint64_t` | Offset within TAS cycle |

### Methods

#### to_string

```cpp
string to_string() const
```

Convert to human-readable string representation

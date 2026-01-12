# TSN Awareness Research

## 1. VLAN Priority (IEEE 802.1Q)

### PCP Extraction

- **TCI field structure** (16 bits): PCP (3 bits) | DEI (1 bit) | VID (12 bits)
- **PCP extraction**: `(tci >> 13) & 0x07`
- **DEI extraction**: `(tci >> 12) & 0x01`

### Traffic Class Mapping

| PCP | Traffic Class | Acronym | Typical Automotive Use |
|-----|---------------|---------|------------------------|
| 0   | Best Effort   | BE      | Infotainment, diagnostics (non-urgent) |
| 1   | Background    | BK      | Bulk data, updates |
| 2   | Excellent Effort | EE   | Streaming media |
| 3   | Critical Applications | CA | Business logic |
| 4   | Video         | VI      | Camera streams (ADAS) |
| 5   | Voice         | VO      | Audio |
| 6   | Internetwork Control | IC | Network protocols, safety messaging |
| 7   | Network Control | NC    | Safety-critical control |

### QinQ (802.1ad) Handling

**Decision**: Extract both outer and inner PCP; outer PCP takes precedence for traffic classification

- **Extract both**: Outer PCP (S-TAG) and Inner PCP (C-TAG)
- **Classification**: Outer PCP used for traffic class assignment and statistics
- **Availability**: Both values exposed via API for analysis
- **Rationale**: Aligns with IEEE 802.1ad switch behavior where outer tag determines priority treatment

**Research Questions Answered**:
- When dealing with QinQ (double VLAN tags, 802.1ad), which PCP value should be used for priority classification?
- Answer: Extract both outer and inner PCP, but outer PCP takes precedence for traffic classification and statistics (aligns with IEEE 802.1ad switch behavior)

---

## 2. TAS Schedule (IEEE 802.1Qbv)

### Schedule Structure (JSON Format)

**Decision**: JSON format with custom schema as primary target (simplest to implement and test, YANG support can be added later)

```json
{
  "base_time_ns": 0,
  "cycle_time_ns": 1000000,
  "admin_control_list": [
    {
      "operation": "SetGateStates",
      "gate_states": "10000000",
      "time_interval_ns": 500000
    },
    {
      "operation": "SetGateStates",
      "gate_states": "01111111",
      "time_interval_ns": 500000
    }
  ]
}
```

**gate_states format**: 8-character binary string (MSB = PCP 7, LSB = PCP 0), '1' = open, '0' = closed

### Validation Rules

1. **Sum constraint**: Sum of all time_interval_ns must equal cycle_time_ns
2. **No overlaps**: Time intervals must be sequential, non-overlapping
3. **Completeness warning**: At least one open gate per traffic class per cycle (not an error, but warning)

### Example Automotive TAS Schedule

```json
{
  "base_time_ns": 0,
  "cycle_time_ns": 1000000,
  "admin_control_list": [
    {
      "operation": "SetGateStates",
      "gate_states": "11000000",
      "time_interval_ns": 200000,
      "comment": "Safety + control window (PCP 7, 6)"
    },
    {
      "operation": "SetGateStates",
      "gate_states": "00110000",
      "time_interval_ns": 300000,
      "comment": "ADAS camera window (PCP 4, 5)"
    },
    {
      "operation": "SetGateStates",
      "gate_states": "00001111",
      "time_interval_ns": 500000,
      "comment": "Best effort window (PCP 0-3)"
    }
  ]
}
```

**Research Questions Answered**:
- What configuration format should be the primary implementation target for TAS schedule parsing (FR-008 to FR-013)?
- Answer: JSON format with custom schema (simplest to implement and test, YANG support can be added later)

### Reference Materials

- IEEE 802.1Qbv-2015 standard (Section 8.6.8: Enhancements for scheduled traffic)
- IEEE 802.1Q-2018 (integrated TAS specification)
- YANG model for TSN configuration (IEEE 802.1Qcc) - deferred to future enhancement

---

## 3. Stream Identification

### Algorithm

**StreamId**: 5-tuple (src_mac, dst_mac, vlan_id, pcp, ethertype)

**Hash function**: std::hash specialization for O(1) unordered_map lookup

### Idle Timeout

**Decision**: 10 seconds default (configurable) - balances detection of ended streams while tolerating automotive network gaps

**Rationale**:
- Automotive networks may have legitimate gaps (ECU sleep states, diagnostic pauses)
- Too short (<5s): False stream end detection
- Too long (>30s): Delayed stream end reporting
- 10 seconds: Industry best practice for automotive Ethernet monitoring

**Research Questions Answered**:
- What should be the default idle timeout before a stream is marked as "Ended" for stream tracking (FR-018)?
- Answer: 10 seconds (configurable) - balances detection of ended streams while tolerating automotive network gaps

### Stream Statistics

Per-stream metrics:
- `packet_count`: Total packets in stream
- `byte_count`: Total bytes in stream
- `bandwidth_bps`: Calculated bandwidth (bytes / duration)
- `latency`: Optional latency statistics (if enabled)
- `pcp_distribution`: Map of PCP values seen in stream
- `first_seen`: First packet timestamp
- `last_seen`: Last packet timestamp
- `state`: Active, Idle, Ended

### Stream vs Priority Statistics

| Aspect | Per-Stream Stats | Per-Priority Stats |
|--------|------------------|-------------------|
| Granularity | Individual flow (MAC + VLAN) | Aggregated by PCP |
| Use case | Flow debugging, stream validation | Traffic class analysis, QoS validation |
| Memory | O(n) streams | O(8) fixed (8 priorities) |
| Performance | Hash map lookup O(1) | Array lookup O(1) |

---

## 4. Latency Measurement

### Strategy

**Decision**: Inter-packet timing method (measure time between consecutive packets of same priority) - practical for passive monitoring, provides useful jitter/interval metrics

**Algorithm**:
1. For each priority class, maintain array of timestamps
2. Calculate delta between consecutive packets: `latency = current_timestamp - previous_timestamp`
3. Store delta in per-priority sample array

**Rationale**:
- Passive monitoring cannot correlate request-response pairs
- Inter-packet timing provides useful metrics: jitter, scheduling compliance
- Simple to implement, minimal state required

**Research Questions Answered**:
- How should "end-to-end latency" be calculated for per-priority latency measurement (FR-020)?
- Answer: Inter-packet timing (measure time between consecutive packets of same priority) - practical for passive monitoring, provides useful jitter/interval metrics

### Sample Storage

**Decision**: Maximum 10,000 samples per priority class (~640KB total for 8 classes, sufficient for percentile calculation while bounding memory)

**Memory calculation**:
- 8 bytes per sample (uint64_t nanoseconds)
- 10,000 samples × 8 bytes × 8 priorities = 640,000 bytes (~640KB)

**Eviction strategy**: Circular buffer - oldest samples overwritten when limit reached

**Research Questions Answered**:
- What should be the maximum number of latency samples stored per priority class?
- Answer: 10,000 samples per priority (~640KB total for 8 classes, sufficient for percentile calculation while bounding memory)

### Percentiles

**Calculated metrics**:
- **p50 (median)**: Middle value when sorted
- **p95**: 95th percentile (5% worst-case latencies excluded)
- **p99**: 99th percentile (1% worst-case excluded)
- **max**: Maximum observed latency
- **min**: Minimum observed latency
- **mean**: Average latency
- **stddev**: Standard deviation (jitter indicator)

**Algorithm**: nth_element for O(n) percentile calculation (no full sort required)

### Default Latency Thresholds

Per-priority latency budgets based on automotive requirements:

| PCP | Traffic Class | Default Threshold | Rationale |
|-----|---------------|-------------------|-----------|
| 7   | Network Control (Safety) | 1ms | Safety-critical control loops |
| 6   | Internetwork Control | 2ms | ADAS coordination, emergency braking |
| 5   | Voice | 5ms | Real-time audio (VoIP) |
| 4   | Video | 5ms | Camera streaming (ADAS, rear-view) |
| 3   | Critical Applications | 100ms | Business logic, non-urgent safety |
| 2   | Excellent Effort | 100ms | Enhanced best effort |
| 1   | Background | 100ms | Bulk data transfer |
| 0   | Best Effort | 100ms | Infotainment, diagnostics |

**Configurable**: All thresholds can be overridden per-application

### Jitter Calculation

**Jitter** = Standard deviation of inter-packet timing

High jitter indicates:
- Network congestion
- Priority inversion issues
- TAS schedule non-compliance
- Switch buffer overflow

---

## 5. Priority Inversion Detection

**Algorithm**: Detect when low-priority traffic delays high-priority traffic

**Detection method**:
1. Track timestamp windows when high-priority packets transmitted
2. Check if low-priority packets occurred during those windows
3. If low-priority packet between two high-priority packets, flag as potential inversion

**Metrics**:
- Inversion count per priority pair (e.g., PCP 0 delaying PCP 7)
- Duration of inversion events
- Severity score based on priority delta

**Use case**: Validate TAS compliance, detect QoS misconfigurations

---

## Reference Materials

### Standards
- **IEEE 802.1Q-2018**: Virtual LANs and Priority Tagging
- **IEEE 802.1Qbv-2015**: Time-Aware Shaper (TAS)
- **IEEE 802.1Qcc**: Stream Reservation Protocol (SRP) enhancements
- **IEEE 802.1CB-2017**: Frame Replication and Elimination (FRER) - stream concepts
- **IEEE 802.1ad-2005**: Provider Bridges (QinQ)

### Automotive Context
- Typical safety-critical latency: <2ms (braking, steering)
- ADAS camera latency: <10ms (object detection pipeline)
- Infotainment latency: <100ms (acceptable user experience)

---

## Research Summary

All key design decisions documented and clarified (2026-01-13):

1. ✅ **QinQ PCP Handling**: Extract both, outer takes precedence
2. ✅ **Latency Method**: Inter-packet timing
3. ✅ **TAS Format**: JSON primary target
4. ✅ **Stream Timeout**: 10 seconds default
5. ✅ **Sample Limit**: 10,000 per priority class

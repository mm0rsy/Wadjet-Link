# TSN Analyzer API Contract

**Phase 1 Deliverable**: Interface definition  
**DO NOT IMPLEMENT** - Contract only

---

## Class: TsnAnalyzer

**Namespace**: `wadjet::tsn`

### Responsibilities

- Extract VLAN priority from packets (PCP field)
- Track priority distribution across 8 traffic classes
- Identify and track individual TSN streams (5-tuple)
- Measure inter-packet latency per priority class
- Validate TAS schedule compliance (IEEE 802.1Qbv)
- Generate comprehensive analysis reports

### Properties

- **Thread Safety**: Single-threaded (not thread-safe)
- **Memory**: Bounded growth (max 1MB for typical configs)
- **Performance**: <5% overhead, <100ns per packet for PCP extraction

---

## Nested Type: Config

Configuration structure for TsnAnalyzer

### Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enable_priority_tracking` | `bool` | `true` | Enable per-PCP statistics collection |
| `enable_stream_tracking` | `bool` | `true` | Enable stream identification and tracking |
| `enable_latency_tracking` | `bool` | `true` | Enable inter-packet latency measurement |
| `enable_tas_validation` | `bool` | `false` | Enable TAS schedule compliance checking |
| `latency_config` | `LatencyConfig` | default | Latency tracking configuration |
| `tas_schedule` | `optional<TasSchedule>` | `nullopt` | TAS schedule (required if `enable_tas_validation == true`) |
| `stream_idle_timeout_ms` | `uint64_t` | `10000` | Stream idle timeout in milliseconds (10 seconds) |

### Constraints

- If `enable_tas_validation == true`, `tas_schedule` must be provided
- `tas_schedule` must be valid (sum of time_interval_ns == cycle_time_ns)

---

## Nested Type: Report

Comprehensive analysis report structure

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `priority_stats` | `array<TrafficClassStats, 8>` | Statistics per PCP (0-7) |
| `stream_stats` | `vector<StreamStats>` | Statistics for all tracked streams |
| `latency_violations` | `vector<LatencyViolation>` | Latency threshold violations |
| `tas_violations` | `vector<TasViolation>` | TAS schedule compliance violations |
| `total_packets` | `uint64_t` | Total packets processed |
| `total_bytes` | `uint64_t` | Total bytes processed |
| `capture_duration_s` | `double` | Capture duration in seconds |

### Methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `to_text()` | `string` | Human-readable text report |
| `to_json()` | `nlohmann::json` | JSON export |
| `to_csv()` | `string` | CSV export |

---

## Constructor

```cpp
explicit TsnAnalyzer(Config config)
```

### Parameters

- `config`: Configuration options

### Preconditions

- If `config.enable_tas_validation == true`, `config.tas_schedule` must be set
- TasSchedule must be valid (sum of time_interval_ns == cycle_time_ns)

### Postconditions

- Analyzer ready to process packets
- All tracking enabled per config

### Exceptions

- Throws `std::invalid_argument` if config is invalid

---

## Method: process_packet

```cpp
void process_packet(const PacketView& packet)
```

Process single packet and update statistics

### Parameters

- `packet`: Packet to analyze (valid PacketView)

### Preconditions

- `packet` is valid PacketView
- `finalize()` has not been called yet

### Actions

1. Extract VLAN priority (if present)
2. Update priority distribution counters
3. Update stream tracking (if enabled)
4. Calculate inter-packet latency (if enabled)
5. Validate TAS compliance (if enabled)

### Postconditions

- Statistics updated for this packet
- Memory bounds maintained

### Performance

- O(1) for priority tracking
- O(log n) for stream lookup

### Exceptions

- Never throws (errors logged internally)

---

## Method: finalize

```cpp
void finalize()
```

Finalize analysis and compute statistics

### Preconditions

- At least one packet processed

### Actions

1. Compute latency percentiles (P50, P95, P99)
2. Calculate bandwidth percentages
3. Mark idle streams
4. Freeze state for report generation

### Postconditions

- No more packets can be processed
- Report can be generated

### Performance

- O(n log n) for latency percentile calculation

### Exceptions

- Never throws

---

## Method: generate_report

```cpp
Report generate_report() const
```

Generate comprehensive analysis report

### Returns

- `Report` structure with all statistics

### Preconditions

- `finalize()` has been called

### Postconditions

- Returns complete report
- Report is immutable snapshot

### Performance

- O(1) copy of statistics

---

## Method: is_finalized

```cpp
bool is_finalized() const
```

Check if analyzer has been finalized

### Returns

- `true` if finalized, `false` otherwise

---

## Method: get_config

```cpp
const Config& get_config() const
```

Get current configuration

### Returns

- Configuration used by this analyzer

---

## Method: reset

```cpp
void reset()
```

Reset analyzer to initial state

### Postconditions

- All statistics cleared
- Finalized state reset
- Ready to process new packets

---

## Example Usage

### Basic Priority Tracking

```cpp
#include <wadjet/tsn/tsn_analyzer.hpp>

wadjet::tsn::TsnAnalyzer::Config config{};
config.enable_priority_tracking = true;
config.enable_stream_tracking = false;

wadjet::tsn::TsnAnalyzer analyzer(config);

// Process packets
for (const auto& packet : packets) {
    analyzer.process_packet(packet);
}

// Finalize and report
analyzer.finalize();
auto report = analyzer.generate_report();
std::cout << report.to_text() << std::endl;
```

### Stream Tracking with Latency

```cpp
wadjet::tsn::TsnAnalyzer::Config config{};
config.enable_stream_tracking = true;
config.enable_latency_tracking = true;
config.stream_idle_timeout_ms = 10'000;

wadjet::tsn::TsnAnalyzer analyzer(config);

// Process packets...
analyzer.finalize();
auto report = analyzer.generate_report();

for (const auto& stream : report.stream_stats) {
    std::cout << "Stream: " << stream.id.to_string() 
              << " - Packets: " << stream.packet_count << std::endl;
}
```

### TAS Validation

```cpp
auto schedule = TasSchedule::from_json(tas_json);

wadjet::tsn::TsnAnalyzer::Config config{};
config.enable_tas_validation = true;
config.tas_schedule = schedule;

wadjet::tsn::TsnAnalyzer analyzer(config);

// Process packets...
analyzer.finalize();
auto report = analyzer.generate_report();

std::cout << "TAS Violations: " << report.tas_violations.size() << std::endl;
```

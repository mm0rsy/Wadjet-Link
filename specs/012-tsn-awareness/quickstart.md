# TSN Awareness Quickstart Guide

## C++ API Usage

### Basic Priority Tracking

```cpp
#include <wadjet/tsn/tsn_analyzer.hpp>
#include <wadjet/pcap/pcap_file.hpp>

using namespace wadjet;

int main() {
    // Configure analyzer
    tsn::TsnAnalyzer::Config config{};
    config.enable_priority_tracking = true;
    config.enable_stream_tracking = false;
    config.enable_latency_tracking = false;

    // Create analyzer
    tsn::TsnAnalyzer analyzer(config);

    // Process PCAP file
    pcap::PcapFile pcap("capture.pcap");
    while (auto packet = pcap.next_packet()) {
        analyzer.process_packet(packet);
    }

    // Finalize and generate report
    analyzer.finalize();
    auto report = analyzer.generate_report();

    // Print results
    std::cout << report.to_text() << std::endl;

    return 0;
}
```

**Expected Output**:
```
TSN Analysis Report
===================
Capture Duration: 10.5 seconds
Total Packets: 125,420
Total Bytes: 18,234,500

Priority Distribution:
  PCP 0 (Best Effort):        45,200 packets (36.0%)  -  6,780,000 bytes (37.2%)
  PCP 1 (Background):          1,500 packets ( 1.2%)  -    225,000 bytes ( 1.2%)
  PCP 2 (Excellent Effort):        0 packets ( 0.0%)  -          0 bytes ( 0.0%)
  PCP 3 (Critical Apps):           0 packets ( 0.0%)  -          0 bytes ( 0.0%)
  PCP 4 (Video):              32,000 packets (25.5%)  -  4,800,000 bytes (26.3%)
  PCP 5 (Voice):              18,000 packets (14.4%)  -  1,800,000 bytes ( 9.9%)
  PCP 6 (Internetwork Ctrl):   8,500 packets ( 6.8%)  -  1,275,000 bytes ( 7.0%)
  PCP 7 (Network Control):    20,220 packets (16.1%)  -  3,354,500 bytes (18.4%)
```

---

### Stream Tracking Example

```cpp
#include <wadjet/tsn/tsn_analyzer.hpp>

using namespace wadjet;

int main() {
    // Enable stream tracking
    tsn::TsnAnalyzer::Config config{};
    config.enable_priority_tracking = true;
    config.enable_stream_tracking = true;
    config.stream_idle_timeout_ms = 10'000;  // 10 seconds

    tsn::TsnAnalyzer analyzer(config);

    // Process packets...
    pcap::PcapFile pcap("automotive_ethernet.pcap");
    while (auto packet = pcap.next_packet()) {
        analyzer.process_packet(packet);
    }

    // Generate report
    analyzer.finalize();
    auto report = analyzer.generate_report();

    // Iterate streams
    for (const auto& stream : report.stream_stats) {
        if (stream.state == tsn::StreamState::ACTIVE) {
            std::cout << "Stream: " << stream.id.to_string() << "\n"
                      << "  Packets: " << stream.packet_count << "\n"
                      << "  Bandwidth: " << stream.bandwidth_bps / 1e6 << " Mbps\n"
                      << "  Expected PCP: " << static_cast<int>(stream.expected_pcp) << "\n";
        }
    }

    return 0;
}
```

**Expected Output**:
```
Stream: 00:1B:21:AA:BB:CC -> 00:1B:21:DD:EE:FF (VLAN 100, PCP 5, EtherType 0x0800)
  Packets: 8,500
  Bandwidth: 0.85 Mbps
  Expected PCP: 5 (Voice)

Stream: 00:1B:21:11:22:33 -> 00:1B:21:44:55:66 (VLAN 100, PCP 7, EtherType 0x0800)
  Packets: 12,300
  Bandwidth: 2.46 Mbps
  Expected PCP: 7 (Network Control)
```

---

### Latency Tracking Example

```cpp
#include <wadjet/tsn/tsn_analyzer.hpp>

using namespace wadjet;

int main() {
    // Configure latency tracking
    tsn::LatencyConfig latency_config{};
    latency_config.thresholds_per_pcp[7] = 1'000'000;  // 1ms for PCP 7
    latency_config.thresholds_per_pcp[6] = 2'000'000;  // 2ms for PCP 6
    latency_config.thresholds_per_pcp[5] = 5'000'000;  // 5ms for PCP 5
    latency_config.max_samples_per_priority = 10'000;

    tsn::TsnAnalyzer::Config config{};
    config.enable_latency_tracking = true;
    config.latency_config = latency_config;

    tsn::TsnAnalyzer analyzer(config);

    // Process packets...
    pcap::PcapFile pcap("latency_test.pcap");
    while (auto packet = pcap.next_packet()) {
        analyzer.process_packet(packet);
    }

    // Generate report
    analyzer.finalize();
    auto report = analyzer.generate_report();

    // Print latency stats
    for (size_t pcp = 0; pcp < 8; ++pcp) {
        const auto& stats = report.priority_stats[pcp];
        if (stats.latency.has_value()) {
            const auto& lat = stats.latency.value();
            std::cout << "PCP " << pcp << " Latency:\n"
                      << "  Min: " << lat.min_ns / 1000.0 << " µs\n"
                      << "  Mean: " << lat.mean_ns / 1000.0 << " µs\n"
                      << "  P95: " << lat.p95_ns / 1000.0 << " µs\n"
                      << "  P99: " << lat.p99_ns / 1000.0 << " µs\n"
                      << "  Max: " << lat.max_ns / 1000.0 << " µs\n"
                      << "  Violations: " << lat.violation_count << "\n";
        }
    }

    return 0;
}
```

**Expected Output**:
```
PCP 7 Latency:
  Min: 125.0 µs
  Mean: 450.0 µs
  P95: 750.0 µs
  P99: 1200.0 µs
  Max: 8500.0 µs
  Violations: 3

PCP 5 Latency:
  Min: 200.0 µs
  Mean: 850.0 µs
  P95: 2100.0 µs
  P99: 4500.0 µs
  Max: 12000.0 µs
  Violations: 0
```

---

### TAS Schedule Validation Example

```cpp
#include <wadjet/tsn/tsn_analyzer.hpp>
#include <wadjet/tsn/tas_schedule.hpp>

using namespace wadjet;

int main() {
    // Load TAS schedule from JSON
    tsn::TasSchedule schedule = tsn::TasSchedule::from_json(R"({
        "base_time_ns": 0,
        "cycle_time_ns": 1000000,
        "gate_control_list": [
            {"time_interval_ns": 200000, "gate_states": [1, 1, 0, 0, 0, 0, 0, 0]},
            {"time_interval_ns": 300000, "gate_states": [0, 0, 1, 1, 0, 0, 0, 0]},
            {"time_interval_ns": 500000, "gate_states": [0, 0, 0, 0, 1, 1, 1, 1]}
        ]
    })");

    // Configure analyzer with TAS
    tsn::TsnAnalyzer::Config config{};
    config.enable_tas_validation = true;
    config.tas_schedule = schedule;

    tsn::TsnAnalyzer analyzer(config);

    // Process packets...
    pcap::PcapFile pcap("tas_test.pcap");
    while (auto packet = pcap.next_packet()) {
        analyzer.process_packet(packet);
    }

    // Generate report
    analyzer.finalize();
    auto report = analyzer.generate_report();

    // Check TAS violations
    std::cout << "TAS Violations: " << report.tas_violations.size() << "\n";
    for (const auto& violation : report.tas_violations) {
        std::cout << "  Timestamp: " << violation.timestamp << "\n"
                  << "  PCP: " << static_cast<int>(violation.pcp) << "\n"
                  << "  Gate State: " << (violation.expected_open ? "should be OPEN" : "should be CLOSED") << "\n";
    }

    return 0;
}
```

---

## Python API Usage

### Basic Priority Tracking

```python
import wadjet

# Configure analyzer
config = wadjet.tsn.TsnAnalyzerConfig()
config.enable_priority_tracking = True
config.enable_stream_tracking = False
config.enable_latency_tracking = False

# Create analyzer
analyzer = wadjet.tsn.TsnAnalyzer(config)

# Process PCAP file
pcap = wadjet.pcap.PcapFile("capture.pcap")
for packet in pcap:
    analyzer.process_packet(packet)

# Finalize and generate report
analyzer.finalize()
report = analyzer.generate_report()

# Print results
print(report.to_text())
```

### Stream Tracking with Priority Checks

```python
import wadjet

config = wadjet.tsn.TsnAnalyzerConfig()
config.enable_stream_tracking = True
config.stream_idle_timeout_ms = 10000

analyzer = wadjet.tsn.TsnAnalyzer(config)

# Process packets...
pcap = wadjet.pcap.PcapFile("automotive.pcap")
for packet in pcap:
    analyzer.process_packet(packet)

analyzer.finalize()
report = analyzer.generate_report()

# Find streams with priority mismatches
for stream in report.stream_stats:
    total_packets = stream.packet_count
    expected_pcp_count = stream.pcp_distribution.get(stream.expected_pcp, 0)
    mismatch_percent = 100.0 * (total_packets - expected_pcp_count) / total_packets
    
    if mismatch_percent > 5.0:  # More than 5% mismatch
        print(f"⚠️  Stream {stream.id} has {mismatch_percent:.1f}% priority mismatch")
        print(f"   Expected PCP: {stream.expected_pcp}")
        print(f"   Distribution: {stream.pcp_distribution}")
```

**Expected Output**:
```
⚠️  Stream 00:1B:21:AA:BB:CC -> 00:1B:21:DD:EE:FF has 8.5% priority mismatch
   Expected PCP: 5 (Voice)
   Distribution: {5: 7780, 0: 720}
```

---

## GoogleTest Matchers Usage

### VLAN Priority Matchers

```cpp
#include <wadjet/tsn/matchers.hpp>
#include <gtest/gtest.h>

TEST(TsnTest, VlanPriorityMatching) {
    wadjet::PacketView packet = create_vlan_packet(/* ... */);

    // Match specific PCP value
    EXPECT_THAT(packet, wadjet::tsn::HasVlanPriority(7));

    // Match high priority (PCP >= 6)
    EXPECT_THAT(packet, wadjet::tsn::IsHighPriority());

    // Match low priority (PCP <= 1)
    EXPECT_THAT(packet, wadjet::tsn::IsLowPriority());

    // Check DEI flag
    EXPECT_THAT(packet, wadjet::tsn::HasDei(false));

    // Negation
    EXPECT_THAT(packet, Not(wadjet::tsn::HasVlanPriority(0)));
}
```

### Stream Matchers

```cpp
TEST(TsnTest, StreamMatching) {
    wadjet::tsn::StreamId expected_stream{
        .src_mac = {0x00, 0x1B, 0x21, 0xAA, 0xBB, 0xCC},
        .dst_mac = {0x00, 0x1B, 0x21, 0xDD, 0xEE, 0xFF},
        .vlan_id = 100,
        .pcp = wadjet::tsn::PriorityCodePoint::Voice,
        .ethertype = 0x0800
    };

    wadjet::PacketView packet = create_test_packet(/* ... */);

    EXPECT_THAT(packet, wadjet::tsn::BelongsToStream(expected_stream));
}
```

### Latency Matchers

```cpp
TEST(TsnTest, LatencyMatching) {
    wadjet::tsn::LatencyStats stats = calculate_latency(/* ... */);

    // Check mean latency
    EXPECT_THAT(stats, wadjet::tsn::HasMeanLatencyBelow(500'000));  // 500 µs

    // Check P95 latency
    EXPECT_THAT(stats, wadjet::tsn::HasP95LatencyBelow(1'000'000));  // 1 ms

    // Check violation count
    EXPECT_THAT(stats, wadjet::tsn::HasViolationCount(0));  // No violations
}
```

---

## Scenario YAML Examples

### Priority Distribution Analysis Scenario

```yaml
name: "TSN Priority Distribution Analysis"
description: "Validate VLAN priority distribution in automotive Ethernet"
version: "1.0.0"

setup:
  pcap_file: "automotive_ethernet.pcap"
  duration_s: 10.0

analyzers:
  - type: "TsnAnalyzer"
    name: "priority_analyzer"
    config:
      enable_priority_tracking: true
      enable_stream_tracking: false
      enable_latency_tracking: false

assertions:
  - name: "Network Control traffic present"
    matcher: "priority_analyzer.priority_stats[7].packet_count > 0"
    severity: "CRITICAL"

  - name: "Voice traffic bandwidth"
    matcher: "priority_analyzer.priority_stats[5].bandwidth_percent >= 10.0"
    severity: "WARNING"

  - name: "No background traffic"
    matcher: "priority_analyzer.priority_stats[1].packet_count == 0"
    severity: "INFO"

report:
  format: ["text", "json"]
  output: "priority_analysis_report"
```

### Stream Tracking Scenario

```yaml
name: "TSN Stream Tracking"
description: "Track individual TSN streams and detect priority mismatches"
version: "1.0.0"

setup:
  pcap_file: "stream_test.pcap"

analyzers:
  - type: "TsnAnalyzer"
    name: "stream_analyzer"
    config:
      enable_stream_tracking: true
      stream_idle_timeout_ms: 10000

assertions:
  - name: "Expected stream count"
    matcher: "stream_analyzer.stream_stats.size() == 5"
    severity: "CRITICAL"

  - name: "No priority mismatches"
    matcher: |
      for stream in stream_analyzer.stream_stats:
          expected = stream.pcp_distribution[stream.expected_pcp]
          assert expected / stream.packet_count >= 0.95
    severity: "CRITICAL"

report:
  format: ["json", "csv"]
  output: "stream_tracking_report"
```

### Latency Validation Scenario

```yaml
name: "TSN Latency Validation"
description: "Measure inter-packet latency per priority class"
version: "1.0.0"

setup:
  pcap_file: "latency_test.pcap"

analyzers:
  - type: "TsnAnalyzer"
    name: "latency_analyzer"
    config:
      enable_latency_tracking: true
      latency_config:
        thresholds_per_pcp: [100000000, 100000000, 100000000, 100000000, 5000000, 5000000, 2000000, 1000000]
        max_samples_per_priority: 10000

assertions:
  - name: "PCP 7 P99 latency"
    matcher: "latency_analyzer.priority_stats[7].latency.p99_ns <= 1000000"
    severity: "CRITICAL"
    error_message: "Network Control P99 latency exceeds 1ms"

  - name: "Voice P95 latency"
    matcher: "latency_analyzer.priority_stats[5].latency.p95_ns <= 5000000"
    severity: "WARNING"

  - name: "No high-priority violations"
    matcher: "latency_analyzer.priority_stats[7].latency.violation_count == 0"
    severity: "CRITICAL"

report:
  format: ["text", "json"]
  output: "latency_validation_report"
```

### TAS Schedule Compliance Scenario

```yaml
name: "TAS Schedule Compliance"
description: "Validate packet transmission against IEEE 802.1Qbv schedule"
version: "1.0.0"

setup:
  pcap_file: "tas_test.pcap"
  tas_schedule_file: "automotive_tas_schedule.json"

analyzers:
  - type: "TsnAnalyzer"
    name: "tas_analyzer"
    config:
      enable_tas_validation: true
      tas_schedule:
        base_time_ns: 0
        cycle_time_ns: 1000000
        gate_control_list:
          - time_interval_ns: 200000
            gate_states: [1, 1, 0, 0, 0, 0, 0, 0]  # PCP 6-7 open (control traffic)
          - time_interval_ns: 300000
            gate_states: [0, 0, 1, 1, 0, 0, 0, 0]  # PCP 4-5 open (video/voice)
          - time_interval_ns: 500000
            gate_states: [0, 0, 0, 0, 1, 1, 1, 1]  # PCP 0-3 open (best effort)

assertions:
  - name: "No TAS violations"
    matcher: "tas_analyzer.tas_violations.size() == 0"
    severity: "CRITICAL"
    error_message: "Packets transmitted outside scheduled gate windows"

  - name: "TAS violation rate"
    matcher: "tas_analyzer.tas_violations.size() / tas_analyzer.total_packets < 0.01"
    severity: "WARNING"
    error_message: "TAS violation rate exceeds 1%"

report:
  format: ["text", "json"]
  output: "tas_compliance_report"
```

---

## JSON Export Example

```cpp
// Generate JSON report
auto report = analyzer.generate_report();
nlohmann::json j = report.to_json();

// Save to file
std::ofstream out("tsn_report.json");
out << j.dump(2);  // Pretty-print with 2-space indent
```

**Example JSON Output**:
```json
{
  "capture_duration_s": 10.5,
  "total_packets": 125420,
  "total_bytes": 18234500,
  "priority_stats": [
    {
      "pcp": 0,
      "packet_count": 45200,
      "byte_count": 6780000,
      "bandwidth_bps": 5165714,
      "bandwidth_percent": 37.2
    },
    {
      "pcp": 7,
      "packet_count": 20220,
      "byte_count": 3354500,
      "bandwidth_bps": 2556190,
      "bandwidth_percent": 18.4,
      "latency": {
        "sample_count": 9850,
        "min_ns": 125000,
        "max_ns": 8500000,
        "mean_ns": 450000.0,
        "stddev_ns": 120000.0,
        "p50_ns": 400000,
        "p95_ns": 750000,
        "p99_ns": 1200000,
        "violation_count": 3
      }
    }
  ],
  "stream_stats": [...],
  "latency_violations": [...],
  "tas_violations": []
}
```

---

## CSV Export Example

```cpp
// Generate CSV report
auto report = analyzer.generate_report();
std::string csv = report.to_csv();

std::ofstream out("tsn_report.csv");
out << csv;
```

**Example CSV Output**:
```csv
PCP,Packet Count,Byte Count,Bandwidth (bps),Bandwidth %,Mean Latency (µs),P95 Latency (µs),P99 Latency (µs),Violations
0,45200,6780000,5165714,37.2,,,
1,1500,225000,171429,1.2,,,
2,0,0,0,0.0,,,
3,0,0,0,0.0,,,
4,32000,4800000,3657143,26.3,,,
5,18000,1800000,1371429,9.9,,,
6,8500,1275000,971429,7.0,,,
7,20220,3354500,2556190,18.4,450.0,750.0,1200.0,3
```

---

## Performance Tips

1. **Disable unused features**: Set `enable_stream_tracking = false` if not needed (reduces memory)
2. **Limit latency samples**: Use `max_samples_per_priority = 5000` for large captures
3. **Batch processing**: Process packets in batches for better cache locality
4. **Zero-copy**: Use `PacketView` directly without copying
5. **Finalize once**: Call `finalize()` only once at the end of analysis

---

## Common Pitfalls

- **Forgetting to finalize**: Always call `analyzer.finalize()` before `generate_report()`
- **TAS schedule validation**: Ensure `sum(time_interval_ns) == cycle_time_ns`
- **Stream idle timeout**: Adjust `stream_idle_timeout_ms` based on traffic pattern
- **Latency threshold units**: Use nanoseconds (e.g., `1'000'000` = 1ms)
- **QinQ handling**: Outer PCP always used for classification, inner PCP available but not primary

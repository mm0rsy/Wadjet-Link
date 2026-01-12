# TSN GoogleTest Matchers Contract

**Phase 1 Deliverable**: Matcher interface definitions  
**DO NOT IMPLEMENT** - Contract only

**Namespace**: `wadjet::tsn`

---

## Packet VLAN Priority Matchers

### HasVlanPriority

Matcher: Packet has specific VLAN priority (PCP value)

**Signature**:
```cpp
::testing::Matcher<const PacketView&> HasVlanPriority(PriorityCodePoint expected_pcp)
```

**Matches if**:
- Packet has VLAN tag
- PCP field equals expected value

**Usage**:
```cpp
EXPECT_THAT(packet, HasVlanPriority(PriorityCodePoint::NetworkControl));
EXPECT_THAT(packet, HasVlanPriority(7));
```

---

### IsHighPriority

Matcher: Packet has high priority (PCP >= 6)

**Signature**:
```cpp
::testing::Matcher<const PacketView&> IsHighPriority()
```

**Matches if**:
- Packet has VLAN tag
- PCP is 6 (Internetwork Control) or 7 (Network Control)

**Usage**:
```cpp
EXPECT_THAT(packet, IsHighPriority());
EXPECT_THAT(packet, Not(IsHighPriority()));
```

---

### IsLowPriority

Matcher: Packet has low priority (PCP <= 1)

**Signature**:
```cpp
::testing::Matcher<const PacketView&> IsLowPriority()
```

**Matches if**:
- Packet has VLAN tag
- PCP is 0 (Best Effort) or 1 (Background)

**Usage**:
```cpp
EXPECT_THAT(packet, IsLowPriority());
auto low_priority_packets = std::count_if(packets.begin(), packets.end(), 
    [](const auto& p) { return testing::Matches(IsLowPriority())(p); });
```

---

### HasDei

Matcher: Packet has Drop Eligible Indicator set

**Signature**:
```cpp
::testing::Matcher<const PacketView&> HasDei(bool expected_dei)
```

**Matches if**:
- Packet has VLAN tag
- DEI bit equals expected value

**Usage**:
```cpp
EXPECT_THAT(packet, HasDei(true));   // DEI set
EXPECT_THAT(packet, HasDei(false));  // DEI not set
```

---

## Stream Identification Matchers

### BelongsToStream

Matcher: Packet belongs to specific TSN stream

**Signature**:
```cpp
::testing::Matcher<const PacketView&> BelongsToStream(const StreamId& expected_stream)
```

**Matches if**:
- All 5-tuple fields match expected StreamId:
  - Source MAC address
  - Destination MAC address
  - VLAN ID
  - PCP
  - EtherType

**Usage**:
```cpp
StreamId expected{
    .src_mac = {0x00, 0x1B, 0x21, 0xAA, 0xBB, 0xCC},
    .dst_mac = {0x00, 0x1B, 0x21, 0xDD, 0xEE, 0xFF},
    .vlan_id = 100,
    .pcp = PriorityCodePoint::Video,
    .ethertype = 0x0800
};
EXPECT_THAT(packet, BelongsToStream(expected));
```

---

## Latency Statistics Matchers

### HasMeanLatencyBelow

Matcher: Latency stats have mean below threshold

**Signature**:
```cpp
::testing::Matcher<const LatencyStats&> HasMeanLatencyBelow(uint64_t threshold_ns)
```

**Parameters**:
- `threshold_ns`: Threshold in nanoseconds

**Usage**:
```cpp
EXPECT_THAT(stats, HasMeanLatencyBelow(500'000));    // 500 µs
EXPECT_THAT(stats, HasMeanLatencyBelow(1'000'000));  // 1 ms
```

---

### HasP95LatencyBelow

Matcher: Latency stats have P95 below threshold

**Signature**:
```cpp
::testing::Matcher<const LatencyStats&> HasP95LatencyBelow(uint64_t threshold_ns)
```

**Parameters**:
- `threshold_ns`: Threshold in nanoseconds

**Usage**:
```cpp
EXPECT_THAT(stats, HasP95LatencyBelow(1'000'000));  // 1 ms
EXPECT_THAT(stats, HasP95LatencyBelow(5'000'000));  // 5 ms
```

---

### HasP99LatencyBelow

Matcher: Latency stats have P99 below threshold

**Signature**:
```cpp
::testing::Matcher<const LatencyStats&> HasP99LatencyBelow(uint64_t threshold_ns)
```

**Parameters**:
- `threshold_ns`: Threshold in nanoseconds

**Usage**:
```cpp
EXPECT_THAT(stats, HasP99LatencyBelow(2'000'000));   // 2 ms
EXPECT_THAT(stats, HasP99LatencyBelow(10'000'000));  // 10 ms
```

---

### HasViolationCount

Matcher: Latency stats have specific violation count

**Signature**:
```cpp
::testing::Matcher<const LatencyStats&> HasViolationCount(uint64_t expected_count)
```

**Parameters**:
- `expected_count`: Expected violation count

**Usage**:
```cpp
EXPECT_THAT(stats, HasViolationCount(0));   // No violations
EXPECT_THAT(stats, HasViolationCount(5));   // Exactly 5 violations
EXPECT_THAT(stats.violation_count, Le(10)); // At most 10 (use standard matcher)
```

---

## Stream Statistics Matchers

### HasPriorityMismatch

Matcher: Stream has priority mismatch above threshold

**Signature**:
```cpp
::testing::Matcher<const StreamStats&> HasPriorityMismatch(double threshold_percent)
```

**Parameters**:
- `threshold_percent`: Mismatch threshold percentage (0-100)

**Usage**:
```cpp
EXPECT_THAT(stream, HasPriorityMismatch(5.0));   // >5% mismatch
EXPECT_THAT(stream, Not(HasPriorityMismatch(1.0))); // <1% mismatch acceptable
```

**Calculation**:
```
mismatch_percent = 100.0 * (total_packets - expected_pcp_count) / total_packets
matches if mismatch_percent > threshold_percent
```

---

### IsInState

Matcher: Stream is in specific state

**Signature**:
```cpp
::testing::Matcher<const StreamStats&> IsInState(StreamState expected_state)
```

**Parameters**:
- `expected_state`: Expected stream state

**Usage**:
```cpp
EXPECT_THAT(stream, IsInState(StreamState::ACTIVE));
EXPECT_THAT(stream, IsInState(StreamState::IDLE));
EXPECT_THAT(stream, IsInState(StreamState::ENDED));
```

---

## TAS Schedule Matchers

### IsValidTasSchedule

Matcher: TAS schedule is valid

**Signature**:
```cpp
::testing::Matcher<const TasSchedule&> IsValidTasSchedule()
```

**Validates**:
- Sum of time_interval_ns equals cycle_time_ns
- At least one gate control entry
- No overlapping intervals
- All gate control entries have valid time_interval_ns > 0

**Usage**:
```cpp
EXPECT_THAT(schedule, IsValidTasSchedule());

TasSchedule invalid_schedule{...};
EXPECT_THAT(invalid_schedule, Not(IsValidTasSchedule()));
```

---

## Example Test Cases

### Priority Distribution Test

```cpp
TEST_F(TsnTest, HighPriorityTrafficPresent) {
    auto packets = load_pcap("automotive_ethernet.pcap");
    
    // Check first packet is high priority
    EXPECT_THAT(packets[0], IsHighPriority());
    
    // Count high priority packets
    auto high_count = std::count_if(packets.begin(), packets.end(),
        [](const auto& p) { 
            return testing::Matches(IsHighPriority())(p); 
        });
    
    EXPECT_GT(high_count, 100);
}
```

### Stream Validation Test

```cpp
TEST_F(TsnTest, StreamPriorityConsistency) {
    auto analyzer = create_analyzer();
    process_pcap(analyzer, "tsn_capture.pcap");
    
    auto report = analyzer.generate_report();
    
    for (const auto& stream : report.stream_stats) {
        // No stream should have >5% priority mismatch
        EXPECT_THAT(stream, Not(HasPriorityMismatch(5.0)));
        
        // Active streams should exist
        if (stream.state == StreamState::ACTIVE) {
            EXPECT_GT(stream.packet_count, 0);
        }
    }
}
```

### Latency Budget Test

```cpp
TEST_F(TsnTest, SafetyCriticalLatencyBudget) {
    auto analyzer = create_analyzer_with_latency();
    process_pcap(analyzer, "safety_critical.pcap");
    
    auto report = analyzer.generate_report();
    const auto& safety_stats = report.priority_stats[7]; // PCP 7 = Network Control
    
    ASSERT_TRUE(safety_stats.latency.has_value());
    
    // Safety-critical traffic must meet strict latency requirements
    EXPECT_THAT(safety_stats.latency.value(), HasMeanLatencyBelow(500'000));   // 500 µs mean
    EXPECT_THAT(safety_stats.latency.value(), HasP95LatencyBelow(1'000'000));  // 1 ms P95
    EXPECT_THAT(safety_stats.latency.value(), HasP99LatencyBelow(2'000'000));  // 2 ms P99
    EXPECT_THAT(safety_stats.latency.value(), HasViolationCount(0));           // Zero violations
}
```

### TAS Compliance Test

```cpp
TEST_F(TsnTest, TasScheduleCompliance) {
    auto schedule = load_tas_schedule("automotive_schedule.json");
    
    // Validate schedule structure
    EXPECT_THAT(schedule, IsValidTasSchedule());
    
    // Analyze with TAS validation
    auto analyzer = create_analyzer_with_tas(schedule);
    process_pcap(analyzer, "tas_test.pcap");
    
    auto report = analyzer.generate_report();
    
    // Should have minimal TAS violations
    EXPECT_LE(report.tas_violations.size(), 10);
}
```

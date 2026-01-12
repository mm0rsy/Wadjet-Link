# Implementation Plan: TSN Awareness

**Branch**: `milestone/012-tsn-awareness` | **Date**: 2026-01-12 (Updated: 2026-01-13) | **Spec**: [spec.md](./spec.md)  
**Input**: Feature specification from `/specs/012-tsn-awareness/spec.md`

## Summary

Implement Time-Sensitive Networking (TSN) awareness for deterministic Ethernet analysis. Add VLAN Priority Code Point (PCP) extraction, stream identification by (MAC, VLAN ID), per-priority latency measurement, and IEEE 802.1Qbv Time-Aware Shaper (TAS) schedule support. Enable validation of priority-based QoS, bandwidth allocation compliance, and deterministic latency requirements. **50+ tests across 4 weeks, 28+ new files.**

## Clarifications (Session 2026-01-13)

**All design ambiguities resolved:**

1. **QinQ PCP Handling**: Extract both outer and inner PCP; outer PCP takes precedence for traffic classification and statistics (aligns with IEEE 802.1ad switch behavior)

2. **Latency Measurement**: Inter-packet timing method (measure time between consecutive packets of same priority) - practical for passive monitoring, provides useful jitter/interval metrics

3. **TAS Schedule Format**: JSON format with custom schema as primary target (simplest to implement and test, YANG support can be added later)

4. **Stream Idle Timeout**: 10 seconds default (configurable) - balances detection of ended streams while tolerating automotive network gaps

5. **Latency Sample Storage**: Maximum 10,000 samples per priority class (~640KB total for 8 classes, sufficient for percentile calculation while bounding memory)

## Technical Context

**Language/Version**: C++20 (existing codebase from M0-M11)  
**Primary Dependencies**: GoogleTest (testing), yaml-cpp (scenarios), pybind11 (Python bindings), nlohmann::json (export)  
**Storage**: In-memory statistics collectors, optional PCAP export for TSN violations  
**Testing**: GoogleTest with custom TSN matchers, property-based testing for priority distribution, real TSN captures  
**Target Platform**: Linux-first (requires VLAN tag preservation in capture)  
**Project Type**: C++ library extension with Python/C/Rust bindings  
**Performance Goals**: <5% overhead on packet capture rate, O(1) stream lookup, bounded latency sample storage  
**Constraints**: Passive analysis only, no TAS enforcement/simulation, hardware timestamp preferred but not required  
**Scale/Scope**: 50+ tests, 8 traffic classes (PCP 0-7), multi-stream tracking, TAS schedule parsing

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### ✅ **I. Library-First Architecture**
- **Status**: PASS - TSN analysis is a library extension (`src/protocols/tsn/*.cpp`, headers in `include/wadjet/protocols/tsn/`)
- **Evidence**: M2 established protocol decoder pattern, M8 added gPTP (time sync foundation for TSN)
- **This milestone**: New `wadjet::protocols::tsn` namespace with pluggable analyzers

### ✅ **II. Zero-Copy, Zero-Latency**
- **Status**: PASS - PCP extraction uses `PacketView` without copying, stream tracking minimal memory
- **Evidence**: M1 `PacketView` established, M2 VLAN decoder already exists
- **This milestone**: Enhance VLAN decoder to expose PCP field, zero-copy priority extraction

### ✅ **III. Test-First Development**
- **Status**: PASS - 50+ tests planned (10 priority, 12 stream, 10 latency, 10 TAS, 8 integration)
- **Evidence**: M0-M11 have 378+ GoogleTest tests
- **This milestone**: Each phase begins with test creation (TDD enforced)

### ✅ **IV. Pluggable Protocol Architecture**
- **Status**: PASS - TSN analyzer is standalone component, doesn't modify existing decoders
- **Evidence**: M2 protocol dispatcher pattern supports extensibility
- **This milestone**: TSN sits alongside existing analyzers, integrates via VLAN decoder enhancement

### ✅ **V. Multi-Language FFI Support**
- **Status**: PASS - Python bindings for TSN analysis, C ABI planned
- **Evidence**: M5 (Python), M7 (Rust) binding patterns established
- **This milestone**: Phase 4 adds Python/C/Rust bindings for TSN API

### ✅ **VI. Automotive Standards Compliance**
- **Status**: PASS - Implements IEEE 802.1Q (VLAN), IEEE 802.1Qbv (TAS)
- **Automotive Context**: TSN critical for ADAS, safety-critical control, deterministic ECU communication
- **This milestone**: Enables validation of automotive TSN deployments (priority mapping, latency budgets)

### ✅ **VII. Non-Invasive Monitoring**
- **Status**: PASS - TSN analysis is passive observation of priority, latency, streams
- **Evidence**: M1 capture is read-only, M8 gPTP monitoring already passive
- **This milestone**: No packet generation, no TAS enforcement - analysis only

**FINAL GATE STATUS**: ✅ **PASS** - All constitutional principles satisfied. TSN extends monitoring without violating non-invasive principle.

## Project Structure

### Documentation (this feature)

```text
specs/012-tsn-awareness/
├── plan.md              # This file (created by /speckit.plan)
├── research.md          # Phase 0: TSN standards research, TAS format analysis
├── data-model.md        # Phase 1: PCP structures, StreamId, TasSchedule entities
├── quickstart.md        # Phase 1: TSN analyzer usage examples
└── contracts/           # Phase 1: TSN analyzer API contracts
```

### Source Code (repository root)

**New TSN subsystem**:

```text
include/wadjet/protocols/tsn/
├── tsn.hpp                  # NEW: Main TSN include header
├── vlan_priority.hpp        # NEW: PCP extraction, traffic class mapping
├── stream.hpp               # NEW: Stream identification (MAC, VLAN ID)
├── latency_tracker.hpp      # NEW: Per-priority latency measurement
├── schedule.hpp             # NEW: TAS schedule structures (802.1Qbv)
└── tsn_analyzer.hpp         # NEW: Unified TSN analysis interface

src/protocols/tsn/
├── vlan_priority_extractor.cpp  # NEW: Extract PCP from VLAN tags
├── traffic_class_stats.cpp      # NEW: Per-priority statistics collector
├── stream_tracker.cpp           # NEW: Stream lifecycle tracking
├── latency_tracker.cpp          # NEW: Latency percentile calculation
├── tas_schedule.cpp             # NEW: TAS schedule representation
├── tas_schedule_parser.cpp      # NEW: JSON/YANG schedule parser
├── tas_schedule_analyzer.cpp    # NEW: Schedule compliance checker
└── tsn_analyzer.cpp             # NEW: Main TSN analyzer orchestrator

include/wadjet/testing/
└── tsn_matchers.hpp         # NEW: gMock matchers (HasVlanPriority, etc.)

tests/protocols/
├── test_vlan_priority.cpp       # NEW: PCP extraction tests (10 tests)
├── test_stream_tracking.cpp     # NEW: Stream identification tests (12 tests)
├── test_latency_tracking.cpp    # NEW: Latency measurement tests (10 tests)
└── test_tas_schedule.cpp        # NEW: TAS schedule parsing tests (10 tests)

tests/integration/
└── test_tsn_integration.cpp     # NEW: End-to-end TSN workflow (8 tests)

examples/
├── tsn_analyzer.cpp             # NEW: CLI TSN analysis tool
├── scenarios/
│   └── tsn_priority_test.yaml   # NEW: TSN scenario test
└── python/
    └── tsn_analysis.py          # NEW: Python TSN example

docs/protocols/
└── tsn.md                       # NEW: Complete TSN guide

pcap_samples/tsn/
├── multi_priority.pcap          # NEW: Traffic with PCP 0-7
├── qinq_double_vlan.pcap        # NEW: QinQ (802.1ad) packets
├── high_priority_only.pcap      # NEW: PCP 6-7 only
└── automotive_mix.pcap          # NEW: Realistic automotive TSN scenario

fuzz/
├── fuzz_vlan_pcp.cpp            # NEW: VLAN tag fuzzer
└── fuzz_tas_schedule.cpp        # NEW: TAS schedule parser fuzzer

bindings/python/src/
└── tsn_bindings.cpp             # NEW: Python TSN bindings

bindings/c/
└── wadjet_tsn.h                 # NEW: C ABI for TSN

bindings/rust/src/
└── tsn.rs                       # NEW: Rust TSN bindings
```

**Enhanced existing files**:

```text
src/protocols/ethernet/ethernet_decoder.cpp  # ENHANCED: Expose PCP field from VLAN
include/wadjet/protocols/ethernet/ethernet.hpp  # ENHANCED: Add PCP accessor
```

**Structure Decision**: New `tsn/` subdirectory under `protocols/` for TSN-specific code. Minimal changes to existing VLAN decoder (additive only). All 28+ files are new additions, not replacements.

## Phase 0: Research & Gap Analysis

**Duration**: 2 days  
**Output**: [research.md](./research.md) - TSN standards analysis, TAS format research, priority mapping best practices

### Research Tasks

**See [research.md](./research.md) for complete research deliverables including:**

1. **IEEE 802.1Q VLAN Priority Analysis** - PCP extraction, traffic class mapping, automotive priorities, QinQ handling
2. **IEEE 802.1Qbv Time-Aware Shaper Research** - TAS schedule format, gate control lists, validation rules
3. **Stream Identification Best Practices** - 5-tuple algorithm, idle timeout strategies, hash map implementation
4. **Latency Measurement Strategies** - Inter-packet timing method, percentile calculation, sample storage limits

**Key Decisions Documented in research.md:**
- QinQ: Extract both outer/inner PCP; outer PCP used for classification
- TAS Format: JSON schema as primary target (YANG deferred)
- Stream Idle Timeout: 10 seconds default (configurable)
- Latency Samples: 10,000 max per priority class (~640KB total)
- Measurement Strategy: Inter-packet timing (passive monitoring)

---

## Phase 1: Data Model & API Design

**Duration**: 3 days  
**Output**: [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md), updated agent context

### Documentation References

**See separate documentation files for complete specifications:**

- **[data-model.md](./data-model.md)** - Complete entity definitions with all TSN data structures:
  - PriorityCodePoint enum (8 traffic classes)
  - VlanPriorityInfo struct (PCP extraction, QinQ support)
  - StreamId 5-tuple (src_mac, dst_mac, vlan_id, pcp, ethertype)
  - TrafficClassStats, StreamStats, LatencyStats
  - TasSchedule, GateControlEntry (IEEE 802.1Qbv)
  - All relationships, lifecycles, invariants, examples

- **[contracts/](./contracts/)** - API contracts (interface specifications):
  - [tsn_analyzer.md](./contracts/tsn_analyzer.md) - Main TsnAnalyzer API contract
  - [types.md](./contracts/types.md) - All TSN type definitions
  - [matchers.md](./contracts/matchers.md) - GoogleTest matcher specifications

- **[quickstart.md](./quickstart.md)** - Complete usage examples:
  - C++ API usage (basic, stream tracking, latency, TAS validation)
  - Python API examples
  - GoogleTest matcher examples
  - Scenario YAML examples
  - JSON/CSV export examples
  - Performance tips and common pitfalls

### Agent Context Update

Run `.specify/scripts/bash/update-agent-context.sh copilot` to add:

```text
## TSN Awareness Technology Stack

- **TSN Standards**: IEEE 802.1Q (VLAN priority), IEEE 802.1Qbv (TAS)
- **Data Structures**: PriorityCodePoint enum, StreamId, TasSchedule
- **Analysis Components**: VlanPriorityExtractor, StreamTracker, LatencyTracker, TasScheduleAnalyzer
- **Export Formats**: JSON (nlohmann::json), CSV, text
- **Testing**: GoogleTest matchers (HasVlanPriority, IsHighPriority, etc.)
```

---

## Phase 2: Implementation - Week 1 (Priority & Streams)

**Duration**: 5 days  
**Output**: Working VLAN priority extraction and stream tracking

### Day 1-2: VLAN Priority Enhancement

**Tasks**:
1. Create `include/wadjet/protocols/tsn/vlan_priority.hpp`
   - Define `PriorityCodePoint` enum
   - Define `VlanPriorityInfo` struct
   - Declare `VlanPriorityExtractor` class

2. Implement `src/protocols/tsn/vlan_priority_extractor.cpp`
   - **PCP extraction algorithm**: `(tci >> 13) & 0x07` (bits 13-15)
   - **DEI extraction**: `(tci >> 12) & 0x01` (bit 12)
   - **QinQ handling**: Extract both outer (S-TAG) and inner (C-TAG) PCP values; store both in VlanPriorityInfo
   - **Classification**: Use outer PCP for traffic class determination
   - Traffic class mapping (PCP → name): Array lookup for O(1) performance

3. Enhance existing VLAN decoder
   - Modify `src/protocols/ethernet/ethernet_decoder.cpp`
   - Add `get_priority()` method to return `VlanPriorityInfo`
   - For 802.1Q: Set only outer PCP and DEI
   - For 802.1ad QinQ: Set both outer and inner PCP/DEI
   - Maintain backward compatibility (no breaking changes to existing API)

4. Implement `src/protocols/tsn/traffic_class_stats.cpp`
   - `TrafficClassStatsCollector` class
   - Per-priority counters (packets, bytes)
   - Bandwidth calculation
   - JSON/CSV export

5. Write tests `tests/protocols/test_vlan_priority.cpp`
   - Test PCP extraction for values 0-7 ✓
   - Test DEI flag ✓
   - Test QinQ inner/outer PCP ✓
   - Test untagged traffic (default PCP 0) ✓
   - Test traffic class mapping ✓
   - Test statistics collection ✓
   - Test JSON export ✓
   - Test malformed VLAN tags ✓
   - Test bandwidth percentage calculation ✓
   - Test zero packets edge case ✓
   - **Total: 10 tests**

### Day 3-5: Stream Identification

**Tasks**:

1. Create `include/wadjet/protocols/tsn/stream.hpp`
   - **StreamId structure**: 5-tuple (src_mac, dst_mac, vlan_id, pcp, ethertype)
   - **Hash function**: std::hash specialization for O(1) unordered_map lookup
   - Define `StreamState` enum: UNKNOWN, ACTIVE, IDLE, ENDED
   - **StreamStats structure**: packet_count, byte_count, first_seen, last_seen, latency_samples vector
   - Declare `StreamTracker` class with configurable idle timeout

2. Implement `src/protocols/tsn/stream_tracker.cpp`
   - Hash map (`std::unordered_map<StreamId, StreamStats>`) for O(1) stream lookup
   - **Idle timeout detection**: Mark stream IDLE if (current_time - last_seen) > 10 seconds (configurable)
   - Per-stream statistics accumulation: increment packet_count and byte_count per packet
   - Priority distribution tracking: count packets per PCP per stream
   - Stream lifecycle: UNKNOWN → ACTIVE (first packet) → IDLE (timeout) → ENDED (explicit close)
   - **Memory bounds**: Limit to 1,000 concurrent streams max (configurable)

3. Write tests `tests/protocols/test_stream_tracking.cpp`
   - Test stream identification by (MAC, VLAN) ✓
   - Test multiple concurrent streams ✓
   - Test stream statistics collection ✓
   - Test idle timeout detection ✓
   - Test priority distribution per stream ✓
   - Test stream filtering by priority ✓
   - Test stream filtering by state ✓
   - Test stream lifecycle (Active → Idle → Ended) ✓
   - Test duplicate stream ID handling ✓
   - Test VLAN ID 0 edge case ✓
   - Test stream JSON export ✓
   - Test empty stream tracker ✓
   - **Total: 12 tests**

**Exit Criteria**:
- All 22 tests passing (10 priority + 12 stream)
- Zero compiler warnings
- Code coverage >90%
- Example `examples/tsn_basic.cpp` compiles and runs

---

## Phase 3: Implementation - Week 2 (Latency & TAS)

**Duration**: 5 days  
**Output**: Latency tracking and TAS schedule support

### Day 6-8: Latency Measurement

**Tasks**:

1. Create `include/wadjet/protocols/tsn/latency_tracker.hpp`
   - **LatencyConfig structure**: thresholds per PCP (PCP 7: 1ms, PCP 6: 2ms, PCP 4-5: 5ms, PCP 0-3: 100ms)
   - **LatencyStats structure**: min, max, mean, stddev, p50, p95, p99, sample_count
   - **LatencyViolation structure**: timestamp, stream_id, pcp, measured_latency, threshold
   - **LatencyTracker class**: track per-priority latency using inter-packet timing method

2. Implement `src/protocols/tsn/latency_tracker.cpp`
   - **Inter-packet timing method**: Calculate latency as time delta between consecutive packets in same stream
   - Per-priority latency sample collection: Store samples per PCP (8 arrays)
   - **Bounded storage**: Max 10,000 samples per priority class (~640KB total: 8 priorities × 10K samples × 8 bytes)
   - **Percentile calculation**: Sort samples and extract p50 (median), p95, p99 using nth_element
   - Threshold violation detection: Compare each sample against configured threshold for its PCP
   - **Histogram generation**: 10 bins (0-10%, 10-20%, ..., 90-100% of threshold), optional feature
   - **Sample eviction**: When limit reached, use circular buffer (oldest samples overwritten)

3. Write tests `tests/protocols/test_latency_tracking.cpp`
   - Test latency sample collection ✓
   - Test per-priority separation ✓
   - Test percentile calculation (p50, p95, p99) ✓
   - Test threshold violation detection ✓
   - Test min/max tracking ✓
   - Test mean calculation ✓
   - Test single packet edge case ✓
   - Test identical timestamps ✓
   - Test bounded sample storage ✓
   - Test latency with hardware timestamps ✓
   - **Total: 10 tests**

### Day 9-10: TAS Schedule Support

**Tasks**:

1. Create `include/wadjet/protocols/tsn/schedule.hpp`
   - **GateControlEntry structure**: interval_start_ns, interval_duration_ns, gate_states_bitmap (8 bits for 8 priorities)
   - **TasSchedule structure**: cycle_time_ns, base_time_ns, vector<GateControlEntry>, admin/oper flag
   - **TimeWindow structure**: start_ns, end_ns, allowed_priorities_bitmap
   - **TasScheduleParser class**: Parse JSON schedules using nlohmann::json
   - **TasScheduleAnalyzer class**: Check packet compliance against schedule

2. Implement `src/protocols/tsn/tas_schedule.cpp`
   - **TAS schedule representation**: Store gate control list as vector of entries
   - **Gate state at time calculation**: Binary search through entries to find active interval at timestamp T
   - **Algorithm**: `current_cycle_offset = (T - base_time) % cycle_time`, then find entry where `sum(previous_durations) <= offset < sum(previous_durations + current_duration)`
   - **Transmission window extraction**: For each priority, extract all time ranges where its gate bit is open

3. Implement `src/protocols/tsn/tas_schedule_parser.cpp`
   - **JSON schema**:
     ```json
     {
       "cycle_time_ns": 1000000,
       "base_time_ns": 0,
       "admin_control_list": [
         {"operation": "SetGateStates", "gate_states": "10000000", "time_interval_ns": 500000},
         {"operation": "SetGateStates", "gate_states": "01111111", "time_interval_ns": 500000}
       ]
     }
     ```
   - **gate_states format**: 8-character binary string (MSB = PCP 7, LSB = PCP 0), '1' = open, '0' = closed
   - **Validation**: Sum of all time_interval_ns must equal cycle_time_ns
   - **Error handling**: Throw std::invalid_argument for malformed JSON or validation failures
   - YANG parsing: Deferred to future milestone (YANG models complex, JSON sufficient for automotive)

4. Implement `src/protocols/tsn/tas_schedule_analyzer.cpp`
   - **Compliance checking**: For packet at timestamp T with PCP P, check if gate P is open at time T
   - **Violation detection**: Log violation if gate closed when packet transmitted
   - **Violation structure**: timestamp, stream_id, pcp, gate_state_expected, gate_state_actual
   - **Transmission window validation**: For each priority, verify all packets fall within open windows

5. Write tests `tests/protocols/test_tas_schedule.cpp`
   - Test schedule parsing from JSON ✓
   - Test gate state calculation at time ✓
   - Test transmission window extraction ✓
   - Test schedule validation (valid schedule) ✓
   - Test schedule validation (invalid: intervals don't sum) ✓
   - Test schedule validation (overlapping intervals) ✓
   - Test compliance checking ✓
   - Test violation detection ✓
   - Test cycle time wrapping ✓
   - Test empty gate control list ✓
   - **Total: 10 tests**

**Exit Criteria**:
- All 20 tests passing (10 latency + 10 TAS)
- TAS schedule JSON schema documented
- Example automotive TAS schedule created

---

## Phase 4: Implementation - Week 3 (Integration & Tools)

**Duration**: 5 days  
**Output**: Unified TSN analyzer, matchers, examples

### Day 11-13: TSN Analyzer Integration

**Tasks**:

1. Create `include/wadjet/protocols/tsn/tsn_analyzer.hpp`
   - **TsnAnalyzer::Config structure**: enable_priority_tracking, enable_stream_tracking, enable_latency_tracking, enable_tas_validation, tas_schedule_path, stream_idle_timeout_ms, latency_thresholds_per_pcp
   - **TsnAnalyzer::Report structure**: priority_stats (8 counters), stream_stats (map<StreamId, StreamStats>), latency_stats (8 LatencyStats), tas_violations (vector), total_packets, total_bytes, capture_duration_s
   - **TsnAnalyzer class**: Orchestrates priority extractor, stream tracker, latency tracker, TAS analyzer

2. Implement `src/protocols/tsn/tsn_analyzer.cpp`
   - **Component integration**: Initialize all 4 components (priority, stream, latency, TAS)
   - **Packet processing pipeline**:
     1. Extract VLAN priority (VlanPriorityExtractor)
     2. Identify stream (StreamTracker)
     3. Measure latency (LatencyTracker) if enabled
     4. Check TAS compliance (TasScheduleAnalyzer) if schedule provided
   - **Report generation**:
     - Text format: Human-readable tables with priority distribution, top 10 streams, latency percentiles, violation count
     - JSON format: Complete structured data using nlohmann::json
     - CSV format: Rows for stream stats, latency per priority, violations
   - **Finalization logic**: Flush all component states, mark idle streams as ended

3. Create `include/wadjet/testing/tsn_matchers.hpp`
   - **GoogleTest matchers** for TSN-specific assertions:
   - `HasVlanPriority(pcp)`: Checks packet has specific PCP value (0-7)
   - `IsHighPriority()`: Checks PCP >= 6 (Network Control, Internetwork Control)
   - `IsLowPriority()`: Checks PCP <= 3 (Best Effort, Background)
   - `HasDei(value)`: Checks Drop Eligible Indicator is 0 or 1
   - `BelongsToStream(stream_id)`: Checks packet matches specific 5-tuple StreamId

4. Write integration tests `tests/integration/test_tsn_integration.cpp`
   - Test end-to-end TSN workflow ✓
   - Test multi-priority traffic mix ✓
   - Test stream tracking integration ✓
   - Test latency tracking integration ✓
   - Test TAS schedule compliance ✓
   - Test report generation (JSON, text, CSV) ✓
   - Test matcher integration ✓
   - Test empty capture edge case ✓
   - **Total: 8 tests**

### Day 14-15: Examples & CLI Tools

**Tasks**:

1. Create `examples/tsn_analyzer.cpp`
   - **CLI tool**: `tsn_analyzer --input <pcap> --output-format <text|json|csv> [--tas-schedule <json>]`
   - **Arguments**:
     - `--input`: Path to PCAP file (required)
     - `--output-format`: Output format - text (default), json, csv
     - `--tas-schedule`: Path to TAS schedule JSON file (optional)
     - `--stream-timeout`: Stream idle timeout in milliseconds (default: 10000)
     - `--output`: Output file path (default: stdout)
   - **Report output**: 
     - Text: Priority distribution table, top 10 streams by packet count, latency stats per priority, TAS violations summary
     - JSON: Complete structured report for programmatic consumption
     - CSV: Stream statistics in CSV format for spreadsheet analysis

2. Create `examples/scenarios/tsn_priority_test.yaml`
   - **Scenario structure**:
     ```yaml
     name: "TSN Priority Distribution Test"
     description: "Validate VLAN priority extraction and stream tracking"
     pcap: "tsn_priority_mix.pcap"
     expectations:
       - type: "priority_distribution"
         expected: {7: 100, 6: 200, 5: 300, 4: 150, 0: 250}
       - type: "stream_count"
         expected: 5
       - type: "high_priority_latency_p99"
         pcp: 7
         threshold_ms: 1.0
     ```
   - Integration with existing scenario runner in `tests/scenarios/`

3. Create `examples/python/tsn_analysis.py`
   - **Python API demonstration**:
     ```python
     import wadjet
     analyzer = wadjet.TsnAnalyzer()
     analyzer.set_tas_schedule("schedule.json")
     report = analyzer.analyze_pcap("capture.pcap")
     print(f"Priority 7 packets: {report.priority_stats[7]}")
     print(f"P99 latency (PCP 7): {report.latency_stats[7].p99_ms}ms")
     ```
   - Same functionality as C++ CLI tool
   - Demonstrates Python bindings usage

**Exit Criteria**:
- All 8 integration tests passing
- `tsn_analyzer` CLI tool functional
- Scenario test passes
- Examples documented in `docs/`

---

## Phase 5: Implementation - Week 4 (Bindings & Documentation)

**Duration**: 5 days  
**Output**: Multi-language bindings, complete documentation, validation

### Day 16-17: Python Bindings

**Tasks**:

1. Create `bindings/python/src/tsn_bindings.cpp`
   - **pybind11 bindings** for TSN API:
   - Bind `PriorityCodePoint` enum: py::enum_<PriorityCodePoint>("PriorityCodePoint").value("BK", BK).value("BE", BE)...
   - Bind `VlanPriorityInfo` struct: py::class_<VlanPriorityInfo>("VlanPriorityInfo").def_readonly("pcp", &VlanPriorityInfo::pcp)...
   - Bind `TrafficClassStats` struct: packet_count, byte_count properties
   - Bind `StreamId` and `StreamStats`: Expose all 5-tuple fields and statistics
   - Bind `TsnAnalyzer` class: Constructor, analyze_pcap(), set_tas_schedule(), get_report()
   - Bind `TsnAnalyzer::Report`: All statistics fields as read-only properties
   - **Error handling**: Convert C++ exceptions to Python exceptions (pybind11 automatic)

2. Update `bindings/python/wadjet/__init__.py`
   - Add TSN module exports: `from ._wadjet import PriorityCodePoint, VlanPriorityInfo, TsnAnalyzer`
   - Version check: Ensure Python binding version matches C++ library version

3. Test Python bindings
   - Create `bindings/python/tests/test_tsn.py`:
     - Test analyzer creation: `analyzer = wadjet.TsnAnalyzer()`
     - Test configuration: `analyzer.set_tas_schedule("schedule.json")`
     - Test packet processing: `report = analyzer.analyze_pcap("test.pcap")`
     - Test report generation: Verify priority_stats, stream_stats, latency_stats fields
     - Test JSON export: `json_str = report.to_json()`
     - Test enum access: `pcp = wadjet.PriorityCodePoint.NC` (Network Control)
     - **Total: 6 Python tests**

### Day 18: C ABI & Rust Bindings

**Tasks**:

1. Create `bindings/c/wadjet_tsn.h`
   - **C ABI functions** (all `extern "C"`):
     - `wadjet_tsn_analyzer_create()` → returns opaque handle
     - `wadjet_tsn_analyzer_destroy(handle)` → cleanup
     - `wadjet_tsn_analyzer_analyze_pcap(handle, pcap_path, report_out)` → fill C struct
     - `wadjet_tsn_report_free(report)` → free C struct memory
   - **C-compatible structs** (no C++ features):
     - `struct WadjetTsnReport`: Fixed-size arrays for priority_stats[8], pointers for dynamic data
     - `struct WadjetStreamStats`: All POD types
     - `struct WadjetLatencyStats`: All POD types
   - **Memory management**: C caller owns report memory, must call free function
   - **Error handling**: Return error codes (0 = success, negative = error), set errno

2. Create `bindings/rust/src/tsn.rs` (optional, deferred if time constrained)
   - **Rust FFI wrapper** over C ABI using `extern "C"` blocks
   - Safe Rust API: RAII wrappers (`TsnAnalyzer` struct with Drop trait)
   - Result types for error handling instead of C error codes
   - No unsafe in public API (encapsulate all FFI calls)

### Day 19-20: Documentation & Validation

**Tasks**:

1. Create `docs/protocols/tsn.md`
   - **TSN overview**: Time-Sensitive Networking for automotive Ethernet, deterministic latency requirements
   - **IEEE 802.1Q**: VLAN priority tagging (PCP field in TCI), QinQ double tagging
   - **IEEE 802.1Qbv**: Time-Aware Shaper (TAS), gate control lists, scheduled transmission windows
   - **Traffic class mapping table**: 
     | PCP | Name                   | Description                              | Typical Latency Threshold |
     |-----|------------------------|------------------------------------------|---------------------------|
     | 7   | Network Control (NC)   | Critical network management              | <1ms                      |
     | 6   | Internetwork Control   | Safety-critical messaging (ADAS)         | <2ms                      |
     | 5   | Voice                  | Real-time audio streaming                | <5ms                      |
     | 4   | Video                  | Camera video streaming                   | <5ms                      |
     | 3   | Controlled Load        | Important but not time-critical          | <100ms                    |
     | 2   | Excellent Effort       | Better-than-best-effort                  | <100ms                    |
     | 1   | Background (BK)        | Bulk data transfer                       | <100ms                    |
     | 0   | Best Effort (BE)       | Default traffic                          | <100ms                    |
   - **Priority-based analysis guide**: How to interpret priority distribution, detect misconfigured priorities
   - **Stream identification guide**: Using 5-tuple (MAC addresses, VLAN, PCP, EtherType), detecting idle streams
   - **TAS schedule configuration**: JSON schema with examples, gate control list structure
   - **API reference**: C++ API, Python API, C ABI documentation
   - **Examples and use cases**: Automotive scenarios (ADAS, camera streams, diagnostics), analysis workflows

2. Create regression PCAP samples
   - `pcap_samples/tsn/multi_priority.pcap`: Mix of PCP 0-7 (100 packets each, 800 total)
   - `pcap_samples/tsn/qinq_double_vlan.pcap`: QinQ packets with both S-TAG and C-TAG (outer PCP 7, inner PCP 3)
   - `pcap_samples/tsn/high_priority_only.pcap`: Only PCP 6-7 packets (200 total, simulates ADAS traffic)
   - `pcap_samples/tsn/automotive_mix.pcap`: Realistic scenario with camera (PCP 4), diagnostics (PCP 3), control (PCP 6)
   - **Generation method**: Use `examples/pcap_generator` tool or tcpreplay with crafted packets

3. Fuzz testing
   - Create `fuzz/fuzz_vlan_pcp.cpp`:
     - Fuzz VLAN TCI values (all possible 16-bit values)
     - Test PCP extraction with malformed tags
     - Use LibFuzzer: `LLVMFuzzerTestOneInput(data, size)`
   - Create `fuzz/fuzz_tas_schedule.cpp`:
     - Fuzz JSON schedule parsing (random JSON inputs)
     - Test schedule validation edge cases (zero cycle time, negative intervals, missing fields)
     - Use LibFuzzer with nlohmann::json fuzzing
   - **Run configuration**: `./fuzz_vlan_pcp -max_len=1500 -timeout=10 corpus/` with AddressSanitizer enabled
   - **Target**: 1M iterations without crashes or memory leaks

4. Final validation
   - **Cross-validation**: Compare Wadjet PCP extraction against Wireshark's `vlan.priority` filter on same PCAP
   - **Real-world testing**: Analyze captures from automotive TSN network (if available from partners/public datasets)
   - **Performance benchmarking**: Measure TSN analysis overhead using `tools/benchmark_tsn`
     - Baseline: PCAP reading without TSN analysis
     - With TSN: Full priority + stream + latency tracking
     - Target: <5% overhead increase (<100ns per packet for TSN logic)
   - **Memory profiling**: Valgrind Massif to verify bounded memory usage (~640KB for latency samples + stream tracker)

**Exit Criteria**:
- Python bindings fully functional
- C ABI complete
- Documentation comprehensive (>20 pages)
- All 56 tests passing (42 unit + 8 integration + 6 Python)
- Fuzz testing runs without crashes
- Performance goal met

---

## Edge Cases & Error Handling

### VLAN Priority Extraction Edge Cases

1. **No VLAN tag present**:
   - Detection: EtherType != 0x8100 (802.1Q) and != 0x88A8 (802.1ad)
   - Behavior: Return VlanPriorityInfo with `has_vlan = false`, PCP = 0 (Best Effort default)
   - Test: `test_no_vlan_tag()`

2. **Malformed VLAN tag** (truncated packet):
   - Detection: Packet length < 18 bytes (Ethernet header + VLAN)
   - Behavior: Return error status, do not process packet
   - Test: `test_truncated_vlan_packet()`

3. **QinQ with only outer tag** (malformed 802.1ad):
   - Detection: EtherType = 0x88A8 but no inner 0x8100 tag
   - Behavior: Extract outer PCP only, set `inner_pcp = 0`, `is_qinq = false`
   - Test: `test_qinq_missing_inner_tag()`

4. **Reserved PCP values** (theoretical, all 0-7 are valid):
   - Detection: N/A (all 3-bit values valid)
   - Behavior: Map to traffic class by standard table
   - Test: All values 0-7 tested in `test_all_pcp_values()`

5. **DEI flag set** (Drop Eligible Indicator):
   - Detection: DEI bit = 1
   - Behavior: Store DEI value, do not modify PCP extraction
   - Test: `test_dei_flag_set()`

### Stream Tracking Edge Cases

1. **Stream with zero VLAN ID**:
   - Detection: VLAN ID = 0 (priority-tagged frame)
   - Behavior: Still track as unique stream (VLAN ID part of 5-tuple)
   - Test: `test_vlan_id_zero_edge_case()`

2. **Duplicate stream IDs** (same 5-tuple):
   - Detection: StreamId already exists in hash map
   - Behavior: Update existing stream stats (increment counters, update last_seen)
   - Test: `test_duplicate_stream_id_handling()`

3. **Maximum concurrent streams** (memory protection):
   - Detection: Stream count >= 1,000 (configurable limit)
   - Behavior: Evict oldest idle stream (LRU), log warning
   - Test: `test_max_concurrent_streams_limit()`

4. **Stream timeout exactly at threshold**:
   - Detection: (current_time - last_seen) == 10.000 seconds
   - Behavior: Mark as IDLE (>= threshold triggers timeout)
   - Test: `test_stream_timeout_exact_threshold()`

5. **Single-packet stream**:
   - Detection: packet_count == 1
   - Behavior: Valid stream, latency unavailable (need 2+ packets)
   - Test: `test_single_packet_stream()`

### Latency Measurement Edge Cases

1. **Identical timestamps** (high-rate traffic):
   - Detection: current_timestamp == previous_timestamp
   - Behavior: Latency = 0, valid sample (count as perfect timing)
   - Test: `test_identical_timestamps()`

2. **Backward time jump** (clock adjustment):
   - Detection: current_timestamp < previous_timestamp
   - Behavior: Skip sample, log warning, do not update previous_timestamp
   - Test: `test_backward_time_jump()`

3. **Excessive latency** (>1 second, likely capture gap):
   - Detection: inter_packet_time > 1,000,000 microseconds
   - Behavior: Mark as outlier, do not include in percentile calculation
   - Test: `test_excessive_latency_outlier()`

4. **Sample buffer full** (10,000 samples reached):
   - Detection: samples[pcp].size() >= 10,000
   - Behavior: Overwrite oldest sample (circular buffer), maintain count
   - Test: `test_bounded_sample_storage()`

5. **Empty sample set** (no packets for priority):
   - Detection: samples[pcp].empty()
   - Behavior: Return default LatencyStats (min=0, max=0, mean=0, p50=0)
   - Test: `test_empty_latency_samples()`

### TAS Schedule Edge Cases

1. **Zero cycle time**:
   - Detection: cycle_time_ns == 0
   - Behavior: Throw `std::invalid_argument("Cycle time must be positive")`
   - Test: `test_schedule_validation_zero_cycle_time()`

2. **Intervals don't sum to cycle time**:
   - Detection: sum(time_interval_ns) != cycle_time_ns
   - Behavior: Throw `std::invalid_argument("Interval sum mismatch")`
   - Test: `test_schedule_validation_interval_sum_mismatch()`

3. **Empty gate control list**:
   - Detection: admin_control_list.empty()
   - Behavior: All gates closed by default, all packets violate schedule
   - Test: `test_empty_gate_control_list()`

4. **Cycle time wrap-around** (timestamp > base_time + cycle_time):
   - Detection: offset = (T - base_time) % cycle_time
   - Behavior: Calculate modulo, find interval in wrapped cycle
   - Test: `test_cycle_time_wrapping()`

5. **All gates open** (gate_states = "11111111"):
   - Detection: All bits set in gate_states_bitmap
   - Behavior: All packets compliant, no violations
   - Test: `test_all_gates_open_schedule()`

6. **Invalid JSON format**:
   - Detection: nlohmann::json::parse() throws exception
   - Behavior: Catch exception, throw `std::invalid_argument("Invalid JSON")`
   - Test: `test_invalid_json_schedule()`

### Integration Edge Cases

1. **Empty PCAP file** (zero packets):
   - Detection: No packets processed
   - Behavior: Return empty report (all counters = 0)
   - Test: `test_empty_capture_edge_case()`

2. **Non-VLAN traffic** (all packets untagged):
   - Detection: All packets have has_vlan = false
   - Behavior: All traffic classified as PCP 0 (Best Effort)
   - Test: `test_all_untagged_traffic()`

3. **Mixed tagged/untagged traffic**:
   - Detection: Some packets have VLAN, some don't
   - Behavior: Track both as separate traffic classes
   - Test: `test_mixed_tagged_untagged_traffic()`

4. **TAS schedule without timestamp source**:
   - Detection: No hardware timestamps available
   - Behavior: Use software timestamps, log warning about accuracy
   - Test: `test_tas_with_software_timestamps()`

5. **Concurrent finalization** (analyzer destroyed mid-analysis):
   - Detection: N/A (API design prevents)
   - Behavior: Flush all states in destructor, safe cleanup
   - Test: `test_analyzer_early_destruction()`

---

## Testing Summary

### Unit Tests (42 tests)

- `test_vlan_priority.cpp`: 10 tests
- `test_stream_tracking.cpp`: 12 tests
- `test_latency_tracking.cpp`: 10 tests
- `test_tas_schedule.cpp`: 10 tests

### Integration Tests (8 tests)

- `test_tsn_integration.cpp`: 8 tests

### Python Tests (6 tests)

- `bindings/python/tests/test_tsn.py`: 6 tests

### Total: 56 tests

### Coverage Goals

- Line coverage: ≥90%
- Branch coverage: ≥85%
- All public APIs tested

---

## Performance Validation

### Benchmarks

**Benchmark**: `benchmarks/bench_tsn_overhead.cpp`

Measure:
- PCP extraction overhead per packet (<100ns target)
- Stream tracking overhead per packet (<500ns target)
- Overall TSN analysis overhead (<5% of capture rate)

**Validation**:
- Process 1M packets with TSN analysis
- Compare capture rate with/without TSN
- Verify <5% overhead

---

## Deliverables Checklist

### Code Deliverables

- [ ] 6 header files in `include/wadjet/protocols/tsn/`
- [ ] 8 implementation files in `src/protocols/tsn/`
- [ ] 5 test files (4 unit + 1 integration)
- [ ] 3 example programs (C++ CLI, Python, scenario YAML)
- [ ] 1 matcher header `include/wadjet/testing/tsn_matchers.hpp`
- [ ] Python bindings in `bindings/python/src/tsn_bindings.cpp`
- [ ] C ABI in `bindings/c/wadjet_tsn.h`
- [ ] Rust bindings in `bindings/rust/src/tsn.rs` (optional)

### Documentation Deliverables

- [ ] `docs/protocols/tsn.md` - Complete TSN guide
- [ ] `specs/012-tsn-awareness/research.md` - Research findings
- [ ] `specs/012-tsn-awareness/data-model.md` - Data model specification
- [ ] `specs/012-tsn-awareness/quickstart.md` - Usage examples
- [ ] API documentation (Doxygen comments)

### Test Deliverables

- [ ] 50+ tests passing
- [ ] 4 PCAP regression samples in `pcap_samples/tsn/`
- [ ] 2 fuzz harnesses in `fuzz/`
- [ ] Performance benchmark in `benchmarks/`

### Integration Deliverables

- [ ] Enhanced VLAN decoder (backward compatible)
- [ ] TSN matchers integrated with GoogleTest
- [ ] Scenario test support (YAML expectations)
- [ ] Multi-language bindings (Python, C, Rust)

---

## Success Criteria

✅ **Functional**: All 34 functional requirements (FR-001 to FR-034) implemented  
✅ **Testing**: 50+ tests passing, >90% code coverage  
✅ **Performance**: <5% overhead on packet capture rate  
✅ **Documentation**: Complete TSN guide with examples  
✅ **Bindings**: Python and C ABI functional  
✅ **Examples**: Working CLI tool and scenario tests  
✅ **Standards**: IEEE 802.1Q and 802.1Qbv compliance  
✅ **Validation**: Real automotive TSN captures analyzed successfully

---

## Risks & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| VLAN tag stripping by NIC | High | Medium | Document requirement for tag preservation, test with multiple NICs |
| Limited TSN hardware access | Medium | High | Use synthetic traffic (scapy), partner with automotive suppliers |
| Hardware timestamps unavailable | Medium | Medium | Fallback to software timestamps with warning |
| TAS schedule format variations | Low | Medium | Support multiple formats (JSON, YANG), prioritize JSON |
| Performance overhead >5% | Medium | Low | Profile critical path, optimize hash maps, bounded sample storage |
| Complex TAS validation | Medium | Low | Phase 4 (TAS) optional, can defer if time-constrained |

---

## Timeline Summary

| Week | Phase | Deliverables | Tests |
|------|-------|--------------|-------|
| 1 | Priority & Streams | PCP extraction, stream tracking | 22 |
| 2 | Latency & TAS | Latency measurement, TAS schedules | 20 |
| 3 | Integration | TSN analyzer, matchers, examples | 8 |
| 4 | Bindings & Docs | Python/C/Rust, documentation, validation | 0 |

**Total Duration**: 4 weeks  
**Total Tests**: 50+  
**Total Files**: 28+

---

## Post-Implementation

### Future Enhancements (Out of Scope)

- IEEE 802.1Qbu frame preemption detection
- IEEE 802.1CB frame replication and elimination (FRER)
- Active TAS enforcement/simulation
- TSN configuration generation
- Centralized Network Configuration (CNC) integration
- Credit-Based Shaper (CBS) analysis

### Maintenance

- Monitor IEEE 802.1 working group for TSN standard updates
- Add new traffic class mappings as automotive use cases evolve
- Extend TAS schedule formats as vendors adopt new configurations

---

**Ready to start?** Begin with Phase 0 research, then create `vlan_priority.hpp` and the VLAN decoder enhancement! 🚀

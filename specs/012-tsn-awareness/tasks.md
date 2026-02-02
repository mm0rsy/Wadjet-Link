---
description: "Task list for TSN Awareness feature implementation"
---

# Tasks: TSN Awareness (IEEE 802.1Qbv)

**Feature**: Time-Sensitive Networking (TSN) awareness for deterministic Ethernet analysis  
**Input**: Design documents from `/specs/012-tsn-awareness/`  
**Prerequisites**: plan.md, spec.md  
**Branch**: `milestone/012-tsn-awareness`

**Tests**: ✅ REQUIRED - All tests must be written and FAIL before implementation

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4, US5)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and TSN-specific infrastructure

- [ ] T001 Create directory structure: include/wadjet/protocols/tsn/, src/protocols/tsn/, tests/protocols/
- [ ] T002 [P] Add nlohmann::json dependency for TAS schedule parsing in CMakeLists.txt
- [ ] T003 [P] Add yaml-cpp dependency check for scenario integration in CMakeLists.txt
- [ ] T004 Create tsn module CMakeLists.txt in src/protocols/tsn/CMakeLists.txt

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core TSN infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T005 Create base header include/wadjet/protocols/tsn/common.hpp with PCP enum and traffic class mapping
- [ ] T006 Implement traffic class name mapping function in src/protocols/tsn/common.cpp
- [ ] T007 Enhance VLAN decoder in src/protocols/ethernet/ethernet_decoder.cpp to expose get_priority() method
- [ ] T008 [P] Create testing matchers header include/wadjet/testing/tsn_matchers.hpp
- [ ] T009 [P] Create example directory structure examples/tsn/, examples/scenarios/tsn/

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - VLAN Priority Code Point (PCP) Analysis (Priority: P1) 🎯 MVP

**Goal**: Extract and analyze VLAN priority values from traffic to verify traffic class assignments

**Independent Test**: Capture mixed-priority traffic (PCP 0-7) and verify correct extraction and traffic class mapping

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [ ] T010 [P] [US1] Create test file tests/protocols/test_vlan_priority.cpp with test structure
- [ ] T011 [P] [US1] Write test: test_pcp_extraction_all_values() for PCP 0-7 extraction
- [ ] T012 [P] [US1] Write test: test_traffic_class_mapping() for mapping PCP to names
- [ ] T013 [P] [US1] Write test: test_qinq_double_vlan() for 802.1ad double VLAN tag handling
- [ ] T014 [P] [US1] Write test: test_dei_flag_extraction() for Drop Eligible Indicator
- [ ] T015 [P] [US1] Write test: test_untagged_traffic_default() for untagged packets default to PCP 0
- [ ] T016 [P] [US1] Write test: test_qinq_outer_precedence() for outer PCP taking precedence
- [ ] T017 [P] [US1] Write test: test_per_priority_statistics() for packet/byte counting per PCP
- [ ] T018 [P] [US1] Write test: test_bandwidth_calculation() for bandwidth percentage per traffic class
- [ ] T019 [P] [US1] Write test: test_vlan_priority_info_struct() for VlanPriorityInfo structure
- [ ] T020 [P] [US1] Write test: test_json_export() for JSON export of priority statistics

### Implementation for User Story 1

- [ ] T021 [P] [US1] Create header include/wadjet/protocols/tsn/vlan_priority_extractor.hpp with VlanPriorityInfo struct (extends base from T005)
- [ ] T022 [P] [US1] Create header include/wadjet/protocols/tsn/priority_stats.hpp with TrafficClassStats struct
- [ ] T023 [US1] Implement src/protocols/tsn/vlan_priority_extractor.cpp with PCP extraction algorithm (tci >> 13) & 0x07
- [ ] T024 [US1] Implement QinQ handling: extract both outer and inner PCP, outer takes precedence
- [ ] T025 [US1] Implement DEI extraction: (tci >> 12) & 0x01
- [ ] T026 [US1] Implement src/protocols/tsn/priority_stats.cpp with per-priority statistics collection
- [ ] T027 [US1] Add bandwidth percentage calculation logic
- [ ] T028 [US1] Add JSON export method for priority statistics using nlohmann::json

**Checkpoint**: At this point, User Story 1 should be fully functional - PCP extraction, traffic class mapping, and statistics working independently

---

## Phase 4: User Story 5 - Traffic Class Distribution Analysis (Priority: P1)

**Goal**: Generate traffic class distribution statistics to verify bandwidth allocation per priority

**Independent Test**: Analyze capture and verify bandwidth allocation percentages match expected distribution

### Tests for User Story 5

- [ ] T029 [P] [US5] Create test file tests/protocols/test_traffic_distribution.cpp
- [ ] T030 [P] [US5] Write test: test_bandwidth_percentage_calculation() for bandwidth % per class
- [ ] T031 [P] [US5] Write test: test_packet_distribution() for packet count distribution
- [ ] T032 [P] [US5] Write test: test_bandwidth_threshold_detection() for exceeding allocation
- [ ] T033 [P] [US5] Write test: test_time_window_distribution() for time-series distribution
- [ ] T034 [P] [US5] Write test: test_comparison_mode() for actual vs configured comparison

### Implementation for User Story 5

- [ ] T035 [P] [US5] Create header include/wadjet/protocols/tsn/traffic_distribution.hpp
- [ ] T036 [US5] Implement src/protocols/tsn/traffic_distribution.cpp with distribution calculation
- [ ] T037 [US5] Add bandwidth allocation compliance checking logic
- [ ] T038 [US5] Add time-series tracking for priority distribution over time
- [ ] T039 [US5] Implement actual vs configured bandwidth comparison
- [ ] T040 [US5] Add CSV export for distribution statistics

**Checkpoint**: User Stories 1 and 5 should both work independently - full priority analysis and distribution reporting functional

---

## Phase 5: User Story 4 - Stream Identification (Priority: P2)

**Goal**: Identify and track individual TSN streams by (MAC, VLAN ID) tuples for per-flow analysis

**Independent Test**: Capture with multiple streams and verify each stream is tracked independently with correct statistics

### Tests for User Story 4

- [ ] T041 [P] [US4] Create test file tests/protocols/test_stream_tracking.cpp
- [ ] T042 [P] [US4] Write test: test_stream_identification_by_mac_vlan() for 5-tuple stream ID
- [ ] T043 [P] [US4] Write test: test_multiple_concurrent_streams() for tracking multiple streams
- [ ] T044 [P] [US4] Write test: test_stream_statistics_collection() for per-stream packet/byte counts
- [ ] T045 [P] [US4] Write test: test_idle_timeout_detection() for 10-second idle timeout
- [ ] T046 [P] [US4] Write test: test_stream_lifecycle() for Active → Idle → Ended transitions
- [ ] T047 [P] [US4] Write test: test_stream_filtering_by_id() for filtering packets by stream
- [ ] T048 [P] [US4] Write test: test_priority_distribution_per_stream() for PCP distribution per stream
- [ ] T049 [P] [US4] Write test: test_vlan_id_zero_edge_case() for priority-tagged frames
- [ ] T050 [P] [US4] Write test: test_duplicate_stream_id_handling() for existing stream updates
- [ ] T051 [P] [US4] Write test: test_max_concurrent_streams_limit() for 1,000 stream limit
- [ ] T052 [P] [US4] Write test: test_stream_json_export() for JSON export of stream stats

### Implementation for User Story 4

- [ ] T053 [P] [US4] Create header include/wadjet/protocols/tsn/stream.hpp with StreamId and StreamStats structs
- [ ] T054 [US4] Implement std::hash specialization for StreamId 5-tuple in include/wadjet/protocols/tsn/stream.hpp
- [ ] T055 [US4] Implement src/protocols/tsn/stream_tracker.cpp with unordered_map for O(1) stream lookup
- [ ] T056 [US4] Implement idle timeout detection with configurable 10-second default
- [ ] T057 [US4] Implement stream lifecycle management (UNKNOWN → ACTIVE → IDLE → ENDED)
- [ ] T058 [US4] Add per-stream statistics accumulation (packet_count, byte_count, latency_samples)
- [ ] T059 [US4] Add priority distribution tracking per stream
- [ ] T060 [US4] Implement LRU eviction when max 1,000 concurrent streams reached

**Checkpoint**: User Stories 1, 4, and 5 independently functional - priority analysis, distribution, and stream tracking complete

---

## Phase 6: User Story 3 - Latency Measurement with PCP Context (Priority: P1)

**Goal**: Measure and track latency per priority level to verify TSN timing requirements

**Independent Test**: Generate traffic with known inter-packet timing and verify latency measurements match expected values per priority

### Tests for User Story 3

- [ ] T061 [P] [US3] Create test file tests/protocols/test_latency_tracking.cpp
- [ ] T062 [P] [US3] Write test: test_latency_sample_collection() for inter-packet timing measurement
- [ ] T063 [P] [US3] Write test: test_per_priority_separation() for separate tracking per PCP
- [ ] T064 [P] [US3] Write test: test_percentile_calculation() for p50, p95, p99 calculation
- [ ] T065 [P] [US3] Write test: test_threshold_violation_detection() for latency threshold violations
- [ ] T066 [P] [US3] Write test: test_min_max_tracking() for min/max latency per priority
- [ ] T067 [P] [US3] Write test: test_mean_calculation() for average latency
- [ ] T068 [P] [US3] Write test: test_identical_timestamps_edge_case() for zero latency handling
- [ ] T069 [P] [US3] Write test: test_backward_time_jump() for clock adjustment handling
- [ ] T070 [P] [US3] Write test: test_bounded_sample_storage() for 10,000 sample limit per priority
- [ ] T071 [P] [US3] Write test: test_hardware_timestamps() for hardware timestamp support

### Implementation for User Story 3

- [ ] T072 [P] [US3] Create header include/wadjet/protocols/tsn/latency_tracker.hpp with LatencyStats struct
- [ ] T073 [P] [US3] Create LatencyConfig struct with thresholds: PCP 7: 1ms, PCP 6: 2ms, PCP 4-5: 5ms, PCP 0-3: 100ms
- [ ] T074 [US3] Implement src/protocols/tsn/latency_tracker.cpp with inter-packet timing method
- [ ] T075 [US3] Implement per-priority latency sample collection using 8 separate arrays
- [ ] T076 [US3] Implement circular buffer for bounded 10,000 samples per priority (~640KB total)
- [ ] T077 [US3] Implement percentile calculation using nth_element algorithm
- [ ] T078 [US3] Add threshold violation detection comparing samples against configured thresholds
- [ ] T079 [US3] Add histogram generation with 10 bins (optional feature)
- [ ] T080 [US3] Implement backward time jump detection and skip invalid samples

**Checkpoint**: User Stories 1, 3, 4, and 5 independently functional - full priority, latency, stream, and distribution analysis complete

---

## Phase 7: User Story 2 - Schedule Parsing (802.1Qbv Gate Control) (Priority: P2)

**Goal**: Parse and validate Time-Aware Shaper schedules for deterministic transmission window analysis

**Independent Test**: Load TAS configuration JSON and verify gate control list parsing and schedule validation

### Tests for User Story 2

- [ ] T081 [P] [US2] Create test file tests/protocols/test_tas_schedule.cpp
- [ ] T082 [P] [US2] Write test: test_schedule_parsing_from_json() for JSON schedule parsing
- [ ] T083 [P] [US2] Write test: test_gate_state_calculation_at_time() for gate state lookup at timestamp
- [ ] T084 [P] [US2] Write test: test_transmission_window_extraction() for extracting open windows per priority
- [ ] T085 [P] [US2] Write test: test_schedule_validation_valid() for valid schedule acceptance
- [ ] T086 [P] [US2] Write test: test_schedule_validation_interval_mismatch() for intervals != cycle_time rejection
- [ ] T087 [P] [US2] Write test: test_compliance_checking() for packet vs schedule compliance
- [ ] T088 [P] [US2] Write test: test_violation_detection() for detecting closed gate transmissions
- [ ] T089 [P] [US2] Write test: test_cycle_time_wrapping() for timestamp wrap-around handling
- [ ] T090 [P] [US2] Write test: test_empty_gate_control_list() for empty schedule (all gates closed)
- [ ] T091 [P] [US2] Write test: test_invalid_json_format() for malformed JSON error handling

### Implementation for User Story 2

- [ ] T092 [P] [US2] Create header include/wadjet/protocols/tsn/schedule.hpp with TasSchedule struct
- [ ] T093 [P] [US2] Create GateControlEntry struct: interval_start_ns, interval_duration_ns, gate_states_bitmap
- [ ] T094 [US2] Implement src/protocols/tsn/tas_schedule.cpp with gate state calculation
- [ ] T095 [US2] Implement binary search for finding active gate interval at timestamp T
- [ ] T096 [US2] Implement transmission window extraction per priority from gate control list
- [ ] T097 [US2] Create src/protocols/tsn/tas_schedule_parser.cpp for JSON parsing using nlohmann::json
- [ ] T098 [US2] Implement JSON schema validation: cycle_time_ns, base_time_ns, admin_control_list
- [ ] T099 [US2] Implement gate_states parsing: 8-bit binary string to bitmap conversion
- [ ] T100 [US2] Add schedule validation: verify sum of time_interval_ns equals cycle_time_ns
- [ ] T101 [US2] Create src/protocols/tsn/tas_schedule_analyzer.cpp for compliance checking
- [ ] T102 [US2] Implement violation detection for packets transmitted when gate closed
- [ ] T103 [US2] Add error handling with std::invalid_argument for malformed schedules

**Checkpoint**: All 5 user stories independently functional - complete TSN analysis capabilities implemented

---

## Phase 8: Integration & Tools

**Purpose**: Unified TSN analyzer and tools that combine all user story components

### Integration Tests

- [ ] T104 [P] Create integration test file tests/integration/test_tsn_integration.cpp
- [ ] T105 [P] Write test: test_end_to_end_tsn_workflow() for full pipeline: priority → stream → latency → TAS
- [ ] T106 [P] Write test: test_multi_priority_traffic_mix() for mixed PCP 0-7 traffic analysis
- [ ] T107 [P] Write test: test_stream_tracking_integration() for stream + priority integration
- [ ] T108 [P] Write test: test_latency_tracking_integration() for latency + priority integration
- [ ] T109 [P] Write test: test_tas_schedule_compliance() for TAS + priority compliance checking
- [ ] T110 [P] Write test: test_report_generation() for JSON/text/CSV report generation
- [ ] T111 [P] Write test: test_empty_capture_edge_case() for empty PCAP handling

### TSN Analyzer Unified Component

- [ ] T112 [P] Create header include/wadjet/protocols/tsn/tsn_analyzer.hpp with TsnAnalyzer class
- [ ] T113 [P] Create TsnAnalyzer::Config struct with enable flags and thresholds
- [ ] T114 [P] Create TsnAnalyzer::Report struct with all statistics (priority, stream, latency, TAS)
- [ ] T115 Implement src/protocols/tsn/tsn_analyzer.cpp orchestrating all 4 components
- [ ] T116 Implement packet processing pipeline: priority → stream → latency → TAS
- [ ] T117 Implement report generation in text format (human-readable tables)
- [ ] T118 [P] Implement report generation in JSON format using nlohmann::json
- [ ] T119 [P] Implement report generation in CSV format for spreadsheet analysis
- [ ] T120 Implement finalization logic: flush states, mark idle streams
- [ ] T120a Implement priority inversion detection (FR-029): detect when low-priority traffic delays high-priority

### GoogleTest Matchers

- [ ] T121 [P] Implement HasVlanPriority(pcp) matcher in include/wadjet/testing/tsn_matchers.hpp
- [ ] T122 [P] Implement IsHighPriority() matcher for PCP >= 6
- [ ] T123 [P] Implement IsLowPriority() matcher for PCP <= 3
- [ ] T124 [P] Implement HasDei(value) matcher for Drop Eligible Indicator
- [ ] T125 [P] Implement BelongsToStream(stream_id) matcher for stream identification

### Example Programs

- [ ] T126 [P] Create examples/tsn_analyzer.cpp with CLI argument parsing
- [ ] T127 Implement CLI options: --input, --output-format, --tas-schedule, --stream-timeout, --output
- [ ] T128 Implement comprehensive text report output: priority table, top 10 streams, latency stats, violations
- [ ] T129 [P] Create examples/scenarios/tsn_priority_test.yaml with priority distribution expectations
- [ ] T130 [P] Create examples/python/tsn_analysis.py demonstrating Python bindings usage

**Checkpoint**: All integration tests passing, TSN analyzer tool functional, examples working

---

## Phase 9: Bindings & Documentation

**Purpose**: Multi-language bindings and comprehensive documentation

### Python Bindings

- [ ] T131 [P] Create bindings/python/src/tsn_bindings.cpp with pybind11 bindings
- [ ] T132 [P] Bind PriorityCodePoint enum to Python
- [ ] T133 [P] Bind VlanPriorityInfo struct to Python
- [ ] T134 [P] Bind StreamId and StreamStats to Python
- [ ] T135 [P] Bind TsnAnalyzer class to Python
- [ ] T136 [P] Bind TsnAnalyzer::Report to Python with read-only properties
- [ ] T137 Update bindings/python/wadjet/__init__.py to export TSN classes
- [ ] T138 [P] Create bindings/python/tests/test_tsn.py with 6 Python tests

### C ABI

- [ ] T139 [P] Create bindings/c/wadjet_tsn.h with extern "C" functions
- [ ] T140 [P] Define C-compatible structs: WadjetTsnReport, WadjetStreamStats, WadjetLatencyStats
- [ ] T141 [P] Implement wadjet_tsn_analyzer_create() returning opaque handle
- [ ] T142 [P] Implement wadjet_tsn_analyzer_destroy(handle) for cleanup
- [ ] T143 [P] Implement wadjet_tsn_analyzer_analyze_pcap(handle, path, report_out)
- [ ] T144 [P] Implement wadjet_tsn_report_free(report) for memory cleanup

### Documentation

- [ ] T145 [P] Create docs/protocols/tsn.md with TSN overview and IEEE standards summary
- [ ] T146 [P] Add traffic class mapping table to tsn.md with 8 priority levels and latency thresholds
- [ ] T147 [P] Add priority-based analysis guide to tsn.md
- [ ] T148 [P] Add stream identification guide to tsn.md
- [ ] T149 [P] Add TAS schedule configuration guide with JSON schema examples to tsn.md
- [ ] T150 [P] Add API reference (C++, Python, C ABI) to tsn.md
- [ ] T151 [P] Add examples and automotive use cases to tsn.md
- [ ] T152 [P] Create pcap_samples/tsn/multi_priority.pcap with PCP 0-7 mix (800 packets)
- [ ] T153 [P] Create pcap_samples/tsn/qinq_double_vlan.pcap with QinQ packets
- [ ] T154 [P] Create pcap_samples/tsn/high_priority_only.pcap with PCP 6-7 only
- [ ] T155 [P] Create pcap_samples/tsn/automotive_mix.pcap with realistic scenario

### Fuzz Testing

- [ ] T156 [P] Create fuzz/fuzz_vlan_pcp.cpp with LibFuzzer for VLAN TCI fuzzing
- [ ] T157 [P] Create fuzz/fuzz_tas_schedule.cpp with LibFuzzer for JSON schedule fuzzing
- [ ] T158 Run fuzz tests with AddressSanitizer: 1M iterations without crashes

### Final Validation

- [ ] T159 Cross-validate PCP extraction with Wireshark vlan.priority filter on same PCAP
- [ ] T160 [P] Create benchmarks/bench_tsn_overhead.cpp for performance measurement
- [ ] T161 Run performance benchmark: verify <100ns PCP extraction overhead per packet
- [ ] T162 Run performance benchmark: verify <5% overall TSN analysis overhead
- [ ] T163 Run memory profiling with Valgrind: verify ~640KB bounded latency storage
- [ ] T164 Run all 50+ tests and verify >90% code coverage

**Checkpoint**: All bindings functional, documentation complete, validation passed

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: Final improvements affecting multiple user stories

- [ ] T165 [P] Update README.md with TSN awareness feature description
- [ ] T166 [P] Update CHANGELOG.md with M12 TSN awareness milestone
- [ ] T167 Code cleanup: ensure all headers have proper Doxygen comments
- [ ] T168 Code cleanup: run clang-format on all TSN source files
- [ ] T169 [P] Add performance optimization: profile critical path for PCP extraction
- [ ] T170 [P] Security review: validate all JSON parsing uses exception handling
- [ ] T171 Run quickstart.md validation with TSN examples
- [ ] T172 Create release notes for M12 TSN Awareness milestone

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phases 3-7)**: All depend on Foundational phase completion
  - US1 (Priority Analysis) - Phase 3: Can start after Foundational - No dependencies on other stories
  - US5 (Traffic Distribution) - Phase 4: Depends on US1 (needs priority stats)
  - US4 (Stream Tracking) - Phase 5: Can start after Foundational - No dependencies on other stories
  - US3 (Latency Measurement) - Phase 6: Depends on US1 (needs priority context) and US4 (needs streams)
  - US2 (TAS Schedule Parsing) - Phase 7: Can start after Foundational - No dependencies on other stories
- **Integration (Phase 8)**: Depends on all user stories (US1-US5) being complete
- **Bindings & Documentation (Phase 9)**: Depends on Integration phase completion
- **Polish (Phase 10)**: Depends on all previous phases

### User Story Dependencies

```
Foundational (Phase 2)
    ├── US1: Priority Analysis (Phase 3) ✓ Independent
    ├── US4: Stream Tracking (Phase 5) ✓ Independent
    └── US2: TAS Schedule (Phase 7) ✓ Independent

US1: Priority Analysis
    ├── US5: Traffic Distribution (Phase 4)
    └── US3: Latency Measurement (Phase 6) (also needs US4)

US4: Stream Tracking
    └── US3: Latency Measurement (Phase 6)

All User Stories (US1-US5)
    └── Integration (Phase 8)
```

### Within Each User Story

1. Tests MUST be written and FAIL before implementation
2. Headers before implementation
3. Core functionality before edge cases
4. Unit tests passing before moving to next phase

### Parallel Opportunities

**Setup Phase**: All tasks T001-T004 can run in parallel

**Foundational Phase**: Tasks T006, T008, T009 can run in parallel

**User Story 1 Tests**: All tests T010-T020 can run in parallel (same file)

**User Story 1 Implementation**: T021-T022 (headers) can run in parallel

**After Foundational Complete**:
- US1 (Phase 3) AND US4 (Phase 5) AND US2 (Phase 7) can start in parallel
- Once US1 complete: US5 (Phase 4) can start
- Once US1 AND US4 complete: US3 (Phase 6) can start

**Integration Phase**: Tests T104-T111 can run in parallel

**Bindings Phase**: Python (T131-T138), C ABI (T139-T144), and Documentation (T145-T155) can all run in parallel

---

## Parallel Example: After Foundational Phase Completion

```bash
# Three independent user stories can start simultaneously:

# Developer A: User Story 1 (Priority Analysis)
Tasks: T010-T028 (Priority extraction and statistics)

# Developer B: User Story 4 (Stream Tracking) 
Tasks: T041-T060 (Stream identification and lifecycle)

# Developer C: User Story 2 (TAS Schedule Parsing)
Tasks: T081-T103 (Schedule parsing and validation)

# Once US1 completes:
# Developer D: User Story 5 (Traffic Distribution)
Tasks: T029-T040 (Distribution analysis building on US1)

# Once US1 AND US4 complete:
# Developer E: User Story 3 (Latency Measurement)
Tasks: T061-T080 (Latency tracking with priority and stream context)
```

---

## Implementation Strategy

### MVP First (User Story 1 + 5 + Integration)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL - blocks all stories)
3. Complete Phase 3: User Story 1 (Priority Analysis)
4. Complete Phase 4: User Story 5 (Traffic Distribution)
5. **STOP and VALIDATE**: Test US1+US5 independently - basic TSN priority analysis working
6. Create minimal integration in Phase 8 (T112-T120, T126-T128)
7. Deploy/demo if ready - **MVP delivers priority-based traffic analysis**

### Full Feature Delivery

1. Setup + Foundational → Foundation ready
2. Add US1 (Priority) + US5 (Distribution) → Test independently → Basic TSN analysis MVP
3. Add US4 (Streams) → Test independently → Per-flow analysis added
4. Add US3 (Latency) → Test independently → Timing analysis added  
5. Add US2 (TAS Schedule) → Test independently → Deterministic schedule compliance added
6. Complete Integration → All features unified in TsnAnalyzer
7. Add Bindings + Documentation → Multi-language support
8. Polish → Production ready

### Parallel Team Strategy (5 developers)

With multiple developers after Foundational phase:

1. **Team completes Setup + Foundational together** (1-2 days)
2. **Parallel user story development**:
   - Developer A: User Story 1 (Priority Analysis) - T010-T028
   - Developer B: User Story 4 (Stream Tracking) - T041-T060  
   - Developer C: User Story 2 (TAS Schedule) - T081-T103
3. **Sequential dependent stories**:
   - Developer D: User Story 5 (Traffic Distribution) - waits for US1 - T029-T040
   - Developer E: User Story 3 (Latency) - waits for US1+US4 - T061-T080
4. **Integration**: All developers contribute - T104-T130
5. **Bindings**: Split by language - T131-T164
6. **Polish**: Final cleanup together - T165-T172

---

## Testing Strategy

### Test-Driven Development (TDD) Workflow

For each user story:
1. **RED**: Write all tests first (marked with [P] can be written in parallel)
2. **Verify FAIL**: Run tests and confirm they fail (no implementation yet)
3. **GREEN**: Implement features one by one until tests pass
4. **REFACTOR**: Clean up implementation while keeping tests green
5. **CHECKPOINT**: Verify user story works independently before moving to next

### Test Coverage Goals

- **Unit Tests**: 42 tests across 4 test files (priority, stream, latency, TAS)
- **Integration Tests**: 8 tests in test_tsn_integration.cpp
- **Python Tests**: 6 tests in bindings/python/tests/test_tsn.py
- **Total**: 56+ tests
- **Coverage**: ≥90% line coverage, ≥85% branch coverage

### Independent Story Validation

After each user story phase:
1. Run story-specific tests in isolation
2. Verify functionality without other stories
3. Check integration points don't break existing stories
4. Document story completion in checkpoint

---

## Notes

- **[P] tasks**: Different files, no dependencies - safe to parallelize
- **[Story] labels**: Map tasks to specific user stories for traceability
- **Test-first**: All tests MUST fail before implementation begins
- **Independence**: Each user story should be completable and testable on its own
- **Checkpoints**: Stop and validate after each phase before proceeding
- **Edge cases**: All 36 edge cases from plan.md covered in tests
- **Memory bounds**: 640KB latency samples + 1,000 stream limit enforced
- **Performance**: <100ns PCP extraction, <5% overall overhead validated in Phase 9

---

## Success Criteria

✅ **All 172 tasks completed**  
✅ **56+ tests passing with >90% coverage**  
✅ **All 5 user stories independently functional**  
✅ **Performance benchmarks met (<100ns per packet, <5% overhead)**  
✅ **Multi-language bindings working (Python, C ABI)**  
✅ **Documentation complete with examples**  
✅ **Fuzz testing: 1M iterations without crashes**  
✅ **Cross-validation with Wireshark successful**

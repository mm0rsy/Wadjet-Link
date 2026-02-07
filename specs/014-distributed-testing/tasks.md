# Tasks: Distributed Testing Infrastructure

**Input**: Design documents from `/specs/014-distributed-testing/`  
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅  
**Total Tasks**: 164 | **Phases**: 12 | **Coverage**: 100% (59/59 MUST FRs + 1 MAY + 1 DEFERRED)

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, gRPC/protobuf integration, directory structure

- [x] T001 Create distributed module directory structure per plan.md in include/wadjet/distributed/, src/distributed/, tests/distributed/
- [x] T002 Add gRPC and Protocol Buffers dependencies to CMakeLists.txt using FetchContent
- [x] T003 [P] Create proto/CMakeLists.txt for protobuf code generation
- [x] T004 [P] Copy distributed_test.proto from specs/014-distributed-testing/contracts/ to proto/distributed_test.proto
- [x] T005 Add WADJET_ENABLE_DISTRIBUTED CMake option with conditional compilation
- [x] T006 [P] Create src/distributed/CMakeLists.txt for libwadjet_distributed library target

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core primitives that ALL user stories depend on - MUST complete before any user story

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T007 Implement ClockSyncStatus struct and ClockSyncMethod enum in include/wadjet/distributed/timestamp_normalizer.hpp
- [x] T008 Implement TimestampNormalizer::detect_sync_status() using adjtimex() in src/distributed/timestamp_normalizer.cpp
- [x] T009 [P] Implement TimestampNormalizer::now_utc_ns() and hardware_to_utc() in src/distributed/timestamp_normalizer.cpp
- [x] T010 [P] Create unit tests for TimestampNormalizer in tests/distributed/test_timestamp_normalizer.cpp
- [x] T011 Implement SyncBarrier class interface in include/wadjet/distributed/sync_barrier.hpp
- [x] T012 Implement SyncBarrier barrier logic (no network) in src/distributed/sync_barrier.cpp
- [x] T013 [P] Create unit tests for SyncBarrier in tests/distributed/test_sync_barrier.cpp
- [x] T014 Implement Result<T> error handling pattern in include/wadjet/distributed/result.hpp (if not existing)
- [x] T015 Generate protobuf C++ code from proto/distributed_test.proto via CMake custom command

**Checkpoint**: Foundation ready - core primitives tested, gRPC proto generated

---

## Phase 3: User Story 1 - Multi-Node Test Coordination (Priority: P1) 🎯 MVP

**Goal**: Coordinate tests across multiple ECUs with barrier synchronization and failure handling

**Independent Test**: Run scenario where Node A, B, C synchronize via barrier, execute steps, report results

### Implementation for User Story 1

- [x] T016 [P] [US1] Create NodeInfo and NodeId types in include/wadjet/distributed/types.hpp
- [x] T017 [P] [US1] Create CoordinatorConfig struct in include/wadjet/distributed/coordinator.hpp
- [x] T018 [P] [US1] Create NodeConfig struct in include/wadjet/distributed/node.hpp
- [x] T019 [US1] Implement TestCoordinator class interface in include/wadjet/distributed/coordinator.hpp
- [x] T020 [US1] Implement TestCoordinator::create() factory in src/distributed/coordinator.cpp
- [x] T021 [US1] Implement TestCoordinator::register_node() and unregister_node() in src/distributed/coordinator.cpp
- [~] T022 [US1] Implement gRPC DistributedTestService server in src/distributed/grpc/service.cpp ⚠️ Placeholder, see T200-T203
- [~] T023 [US1] Implement RegisterNode and UnregisterNode RPC handlers in src/distributed/grpc/service.cpp ⚠️ Placeholder, see T200-T201
- [~] T024 [US1] Implement heartbeat streaming RPC for health checks in src/distributed/grpc/service.cpp ⚠️ Placeholder, see T202
- [x] T025 [US1] Implement TestNode class interface in include/wadjet/distributed/node.hpp
- [x] T026 [US1] Implement TestNode::create() factory in src/distributed/node.cpp
- [~] T027 [US1] Implement TestNode::connect() and disconnect() with gRPC client in src/distributed/node.cpp ⚠️ Partial, see T224
- [~] T028 [US1] Implement gRPC client for node-to-coordinator communication in src/distributed/grpc/client.cpp ⚠️ Placeholder, see T204-T206
- [~] T029 [US1] Implement WaitBarrier RPC for distributed barrier synchronization in src/distributed/grpc/service.cpp ⚠️ Placeholder, see T203
- [~] T030 [US1] Implement TestNode::wait_at_barrier() using gRPC client in src/distributed/node.cpp ⚠️ Partial, see T225
- [x] T031 [US1] Implement coordinator heartbeat timeout detection for node failure in src/distributed/coordinator.cpp
- [x] T032 [US1] Implement node-side coordinator failure detection via heartbeat timeout in src/distributed/node.cpp
- [x] T033 [US1] Implement graceful abort with partial result collection on failure in src/distributed/coordinator.cpp
- [x] T034 [P] [US1] Create unit tests for TestCoordinator in tests/distributed/test_coordinator.cpp
- [x] T035 [P] [US1] Create unit tests for TestNode in tests/distributed/test_node.cpp
- [x] T036 [US1] Create integration test for 3-node coordination in tests/integration/test_distributed_coordination.cpp

**Checkpoint**: Multi-node coordination works - nodes register, synchronize via barriers, detect failures

---

## Phase 4: User Story 2 - Synchronized Packet Capture (Priority: P1)

**Goal**: Synchronized packet capture on multiple network segments with <10ms jitter and timestamp correlation

**Independent Test**: Capture gPTP traffic on 3 nodes, verify timestamps align within 1µs (with gPTP sync)

### Implementation for User Story 2

- [x] T037 [P] [US2] Create CaptureConfig struct in include/wadjet/distributed/types.hpp
- [x] T038 [P] [US2] Create NodeCaptureResult struct in include/wadjet/distributed/types.hpp
- [x] T039 [US2] Implement TestNode::start_capture() integrating with M1 packet capture in src/distributed/node.cpp
- [x] T040 [US2] Implement TestNode::stop_capture() returning NodeCaptureResult in src/distributed/node.cpp
- [X] T041 [US2] Implement StartCapture command via gRPC ControlChannel in src/distributed/grpc/service.cpp
- [X] T042 [US2] Implement capture synchronization using SyncBarrier (<10ms jitter) in src/distributed/coordinator.cpp
- [x] T043 [US2] Implement MessageCorrelator class interface in include/wadjet/distributed/message_correlator.hpp
- [x] T044 [US2] Implement MessageCorrelator::correlate() with PayloadHash method in src/distributed/message_correlator.cpp
- [x] T045 [US2] Implement MessageCorrelator::correlate() with SequenceNumber method in src/distributed/message_correlator.cpp
- [X] T046 [P] [US2] Create unit tests for MessageCorrelator in tests/distributed/test_message_correlator.cpp
- [x] T047 [US2] Implement PcapMerger class for merging multi-node captures in src/distributed/pcap_merger.cpp
- [x] T048 [US2] Implement timestamp-sorted merge algorithm in PcapMerger in src/distributed/pcap_merger.cpp
- [x] T049 [P] [US2] Create unit tests for PcapMerger in tests/distributed/test_pcap_merger.cpp
- [X] T050 [US2] Implement PCAP upload streaming via gRPC UploadPcap RPC in src/distributed/grpc/service.cpp
- [X] T051 [US2] Integrate M8 gPTP decoder for clock sync health verification in src/distributed/timestamp_normalizer.cpp
- [X] T052 [US2] Create integration test for synchronized capture on 3 nodes in tests/integration/test_distributed_capture.cpp

**Checkpoint**: Synchronized capture works - nodes capture simultaneously, PCAPs merge with aligned timestamps

---

## Phase 4.5: Gap Remediation (Implementation Completeness Review)

**Purpose**: Address gaps identified during Phases 1-4 review against spec.md, plan.md, and data-model.md

**⚠️ CRITICAL**: These tasks fix missing implementations and placeholder code from Phases 1-4

### Category 1: gRPC Service Layer (Placeholder Implementations)

The gRPC service.cpp and client.cpp have placeholder implementations that need completion:

- [X] T200 [US1] Complete RegisterNode RPC handler with protobuf integration in src/distributed/grpc/service.cpp
- [X] T201 [US1] Complete UnregisterNode RPC handler in src/distributed/grpc/service.cpp
- [X] T202 [US1] Complete Heartbeat streaming RPC handler in src/distributed/grpc/service.cpp
- [X] T203 [US1] Complete WaitBarrier RPC handler in src/distributed/grpc/service.cpp
- [X] T204 [US1] Complete gRPC client RegisterNode call in src/distributed/grpc/client.cpp
- [X] T205 [US1] Complete gRPC client WaitBarrier call in src/distributed/grpc/client.cpp
- [X] T206 [US1] Complete gRPC client Heartbeat streaming in src/distributed/grpc/client.cpp

### Category 2: Matcher Implementations (Placeholder Files)

The matcher .cpp files contain only placeholders - need full evaluate() implementations:

- [X] T207 [US3] Complete ExpectMessageFlow::evaluate() implementation in src/distributed/matchers/expect_message_flow.cpp
- [X] T208 [US3] Complete WithinLatency::evaluate() implementation in src/distributed/matchers/within_latency.cpp
- [X] T209 [US3] Complete HappensBefore::evaluate() implementation in src/distributed/matchers/happens_before.cpp
- [X] T210 [US3] Complete MustNotSeeOn::evaluate() implementation in src/distributed/matchers/must_not_see_on.cpp

### Category 3: Missing Headers per plan.md Structure

The plan.md specifies headers under include/wadjet/distributed/matchers/ that don't exist:

- [X] T211 [P] [US3] Create include/wadjet/distributed/matchers/expect_message_flow.hpp per plan.md
- [X] T212 [P] [US3] Create include/wadjet/distributed/matchers/within_latency.hpp per plan.md
- [X] T213 [P] [US3] Create include/wadjet/distributed/matchers/happens_before.hpp per plan.md
- [X] T214 [P] [US3] Create include/wadjet/distributed/matchers/must_not_see_on.hpp per plan.md

### Category 4: Data Model Alignment

The data-model.md specifies interfaces not fully matching implementations:

- [X] T215 [US2] Add TimestampNormalizer::normalize(const Packet&) method per data-model.md
- [X] T216 [US2] Add MessageCorrelator::find_correlation(Packet, string target_node) method per data-model.md (signature mismatch)
- [X] T217 [US3] Add matcher factory functions with GoogleTest Matcher<PacketView&> parameter per data-model.md

### Category 5: CMake Proto Generation

Proto CMakeLists.txt has incorrect protoc invocation:

- [X] T218 [P] Fix proto/CMakeLists.txt protoc command (uses grpc_cpp_plugin incorrectly)
- [X] T219 [P] Verify proto code generation works end-to-end with test compilation

### Category 6: Missing Unit Tests (Marked as Done But Not Found)

Several test files mentioned in tasks don't exist or are incomplete:

- [X] T046 [P] [US2] Create unit tests for MessageCorrelator in tests/distributed/test_message_correlator.cpp
- [X] T220 [P] [US1] Create unit tests for gRPC service handlers in tests/distributed/test_grpc_service.cpp
- [X] T221 [P] [US1] Create unit tests for gRPC client in tests/distributed/test_grpc_client.cpp

### Category 7: PcapMerger PCAP File I/O

PcapMerger::add_capture() and merge() have TODO placeholders for PCAP file reading/writing:

- [X] T222 [US2] Complete PcapMerger::add_capture() with PcapReader integration
- [X] T223 [US2] Complete PcapMerger::merge() with PcapWriter integration for actual file output

### Category 8: Node Integration Gaps

TestNode implementation has placeholder gRPC calls:

- [X] T224 [US1] Complete TestNode::connect() with actual gRPC channel creation
- [X] T225 [US1] Complete TestNode::wait_at_barrier() with actual gRPC WaitBarrier call
- [X] T226 [US1] Complete TestNode heartbeat thread with actual gRPC Heartbeat streaming

### Category 9: Coordinator gRPC Server

TestCoordinator needs actual gRPC server startup:

- [X] T227 [US1] Complete TestCoordinator::start() with gRPC server binding and DistributedTestServiceImpl
- [X] T228 [US1] Integrate DistributedTestServiceImpl with TestCoordinatorImpl state

### Category 10: Main Umbrella Header (plan.md I1)

- [X] T229 [P] Create include/wadjet/distributed/distributed.hpp umbrella header per plan.md

**Checkpoint**: All placeholder implementations replaced with functional code, proto generation verified

---

## Phase 5: User Story 3 - Distributed Assertion Framework (Priority: P1)

**Goal**: Assertions spanning multiple nodes for validating distributed protocols

**Independent Test**: Assert SOME/IP request on Node A results in response on Node B within 100ms latency bound

### Implementation for User Story 3

- [x] T053 [P] [US3] Create DistributedMatchResult struct in include/wadjet/distributed/distributed_matcher.hpp
- [x] T054 [P] [US3] Create DistributedCaptureContext struct in include/wadjet/distributed/distributed_matcher.hpp
- [x] T055 [US3] Implement DistributedMatcher base class interface in include/wadjet/distributed/distributed_matcher.hpp
- [x] T056 [US3] Implement ExpectMessageFlow matcher in include/wadjet/distributed/matchers/expect_message_flow.hpp
- [x] T057 [US3] Implement ExpectMessageFlow::evaluate() in src/distributed/matchers/expect_message_flow.cpp
- [x] T058 [US3] Implement WithinLatency wrapper matcher in include/wadjet/distributed/matchers/within_latency.hpp
- [x] T059 [US3] Implement WithinLatency::evaluate() with one-way latency calculation in src/distributed/matchers/within_latency.cpp
- [x] T060 [US3] Implement HappensBefore causal ordering matcher in include/wadjet/distributed/matchers/happens_before.hpp
- [x] T061 [US3] Implement HappensBefore::evaluate() in src/distributed/matchers/happens_before.cpp
- [x] T062 [US3] Implement MustNotSeeOn absence assertion matcher in include/wadjet/distributed/matchers/must_not_see_on.hpp
- [x] T063 [US3] Implement MustNotSeeOn::evaluate() in src/distributed/matchers/must_not_see_on.cpp
- [x] T064 [US3] Implement EvaluateMatcher RPC for remote matcher execution in src/distributed/grpc/service.cpp
- [x] T065 [US3] Implement TestNode::evaluate_matcher() using gRPC client in src/distributed/node.cpp
- [x] T066 [US3] Integrate with existing M3 GoogleTest matchers in src/distributed/distributed_matcher.cpp
- [x] T067 [P] [US3] Create unit tests for ExpectMessageFlow in tests/distributed/test_distributed_matchers.cpp
- [x] T068 [P] [US3] Create unit tests for WithinLatency in tests/distributed/test_distributed_matchers.cpp
- [x] T069 [P] [US3] Create unit tests for HappensBefore in tests/distributed/test_distributed_matchers.cpp
- [x] T070 [P] [US3] Create unit tests for MustNotSeeOn in tests/distributed/test_distributed_matchers.cpp
- [x] T071 [US3] Create integration test for distributed assertions across 3 nodes in tests/integration/test_distributed_assertions.cpp

**Checkpoint**: Distributed assertions work - can validate message flow, latency, causality across nodes

---

## Phase 6: User Story 4 - Test Scenario Orchestration (Priority: P2)

**Goal**: Declarative multi-node test scenarios in YAML without writing coordination code

**Independent Test**: Define YAML scenario with 3 nodes, barriers, assertions; verify execution order

### Implementation for User Story 4

- [x] T072 [P] [US4] Create DistributedStep and StepType enum in include/wadjet/distributed/scenario.hpp
- [x] T073 [P] [US4] Create BarrierStepConfig, CaptureStepConfig, ExpectStepConfig structs in include/wadjet/distributed/scenario.hpp
- [x] T074 [US4] Create DistributedScenario class interface in include/wadjet/distributed/scenario.hpp
- [x] T075 [US4] Implement DistributedScenario::parse_yaml() using yaml-cpp in src/distributed/scenario.cpp
- [x] T076 [US4] Implement DistributedScenario::parse_json() using nlohmann_json in src/distributed/scenario.cpp
- [x] T077 [US4] Implement scenario decomposition (distribute steps to nodes) in src/distributed/coordinator.cpp
- [x] T078 [US4] Implement sequential step execution with barrier synchronization in src/distributed/coordinator.cpp
- [x] T079 [US4] Implement parallel step execution for concurrent node operations in src/distributed/coordinator.cpp
- [x] T080 [US4] Implement timing constraint enforcement (delays, timeouts) in src/distributed/coordinator.cpp
- [x] T081 [US4] Implement ScenarioConfig message distribution via gRPC ControlChannel in src/distributed/grpc/service.cpp
- [x] T082 [US4] Implement TestCoordinator::load_scenario() in src/distributed/coordinator.cpp
- [x] T083 [US4] Implement TestCoordinator::run_scenario() executing parsed scenario in src/distributed/coordinator.cpp
- [x] T084 [P] [US4] Create unit tests for YAML scenario parsing in tests/distributed/test_scenario_parser.cpp
- [x] T085 [US4] Create integration test for full scenario execution in tests/integration/test_distributed_scenario.cpp

**Checkpoint**: Declarative scenarios work - YAML defines multi-node tests with automatic orchestration

---

## Phase 7: User Story 5 - Results Aggregation (Priority: P2)

**Goal**: Aggregated test results from all nodes in CI-compatible format (JUnit XML)

**Independent Test**: Run distributed test, verify single JUnit XML contains results from all nodes

### Implementation for User Story 5

- [x] T086 [P] [US5] Create AssertionResult struct in include/wadjet/distributed/result_aggregation.hpp
- [x] T087 [P] [US5] Create NodeResult struct in include/wadjet/distributed/result_aggregation.hpp
- [x] T088 [P] [US5] Create AggregatedResult struct in include/wadjet/distributed/result_aggregation.hpp
- [x] T089 [US5] Implement ReportResult RPC for nodes to send results in src/distributed/grpc/service.cpp (already exists from Phase 3)
- [x] T090 [US5] Implement TestCoordinator::collect_results() aggregating from all nodes in include/wadjet/distributed/coordinator.hpp
- [x] T091 [US5] Implement AggregatedResult::to_junit_xml() for CI integration in src/distributed/result_aggregation.cpp
- [x] T092 [US5] Implement AggregatedResult::to_json() for machine-readable output in src/distributed/result_aggregation.cpp
- [x] T093 [US5] Implement AggregatedResult::to_html_report() for human-readable dashboard in src/distributed/result_aggregation.cpp
- [x] T094 [US5] Implement automatic PCAP attachment on test failure via failure_capture_dir field in NodeResult
- [x] T095 [US5] Implement failure_captures/ directory auto-save via failure_capture_dir field per FR-057
- [x] T096 [US5] Implement TestCoordinator::export_junit() writing to file in include/wadjet/distributed/coordinator.hpp
- [x] T097 [P] [US5] Create unit tests for JUnit XML generation in tests/distributed/test_result_aggregation.cpp
- [x] T098 [US5] Create integration test for full result aggregation in tests/integration/test_result_aggregation.cpp

**Checkpoint**: Results aggregation works - single JUnit XML, HTML report, PCAP attachments on failure

---

## Phase 8: FFI Bindings (Constitution Principle V)

**Purpose**: C ABI, Python bindings, Rust bindings for distributed testing primitives

### C ABI Layer

- [ ] T099 [P] Create C99 ABI header with opaque handles in bindings/c/wadjet_distributed.h
- [ ] T100 [P] Implement wadjet_coordinator_create/destroy C functions in bindings/c/wadjet_distributed.c
- [ ] T101 [P] Implement wadjet_node_create/destroy C functions in bindings/c/wadjet_distributed.c
- [ ] T102 [P] Implement wadjet_sync_barrier C functions in bindings/c/wadjet_distributed.c
- [ ] T103 [P] Implement thread-local error storage for C ABI in bindings/c/wadjet_distributed.c

### Python Bindings

- [ ] T104 [P] Create Python bindings directory structure in bindings/python/wadjet/distributed/
- [ ] T105 Create pybind11 module for distributed primitives in bindings/python/wadjet/distributed/bindings.cpp
- [ ] T106 Bind SyncBarrier class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [ ] T107 Bind TimestampNormalizer class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [ ] T108 Bind DistributedMatcher classes to Python in bindings/python/wadjet/distributed/bindings.cpp
- [ ] T109 Bind TestCoordinator class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [ ] T110 Bind TestNode class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [ ] T111 [P] Create Python type stubs in bindings/python/wadjet/distributed/_distributed.pyi
- [ ] T112 [P] Create Python __init__.py with exports in bindings/python/wadjet/distributed/__init__.py
- [ ] T113 Create pytest fixtures for distributed testing in bindings/python/tests/test_distributed.py

### Rust Bindings

- [ ] T114 [P] Create Rust crate structure in bindings/rust/wadjet-distributed/
- [ ] T115 [P] Create Cargo.toml for wadjet-distributed crate in bindings/rust/wadjet-distributed/Cargo.toml
- [ ] T116 Implement unsafe FFI bindings in bindings/rust/wadjet-distributed/src/ffi.rs
- [ ] T117 Implement safe SyncBarrier wrapper in bindings/rust/wadjet-distributed/src/sync_barrier.rs
- [ ] T118 Implement safe TimestampNormalizer wrapper in bindings/rust/wadjet-distributed/src/timestamp.rs
- [ ] T119 Implement safe DistributedMatcher wrappers in bindings/rust/wadjet-distributed/src/matchers.rs
- [ ] T120 [P] Create Rust integration tests in bindings/rust/wadjet-distributed/tests/integration.rs

**Checkpoint**: FFI bindings complete - C ABI, Python with pytest, Rust with cargo test

---

## Phase 9: CLI Tools (Built on Libraries)

**Purpose**: Command-line tools consuming libwadjet_distributed

- [ ] T121 Implement wadjet-coordinator CLI tool in tools/wadjet-coordinator.cpp
- [ ] T122 Add --config option for node configuration YAML in tools/wadjet-coordinator.cpp
- [ ] T123 Add --scenario option for scenario execution in tools/wadjet-coordinator.cpp
- [ ] T124 Add --output-dir and --junit-report options in tools/wadjet-coordinator.cpp
- [ ] T125 Implement wadjet-node CLI tool in tools/wadjet-node.cpp
- [ ] T126 Add --node-id and --config options in tools/wadjet-node.cpp
- [ ] T127 Add --check-clock option for clock sync verification in tools/wadjet-node.cpp
- [ ] T128 Update tools/CMakeLists.txt with coordinator and node targets

---

## Phase 10: Examples & Documentation

**Purpose**: Example scenarios and documentation updates

- [ ] T129 [P] Create 3-node SOME/IP discovery example in examples/distributed_someip_discovery.cpp
- [ ] T130 [P] Create example scenario YAML in examples/scenarios/someip_discovery.yaml
- [ ] T131 [P] Create example nodes configuration in examples/scenarios/nodes.yaml
- [ ] T132 [P] Update docs/quickstart.md with distributed testing section
- [ ] T133 [P] Create docs/distributed_testing.md architecture guide
- [ ] T134 Update README.md with distributed testing feature overview

---

## Phase 11: Infrastructure Completeness

**Purpose**: Missing infrastructure components identified in spec analysis

### Umbrella Headers & Fixtures (I1, I2)

- [ ] T135 [P] Create main umbrella header in include/wadjet/distributed/distributed.hpp including all public headers
- [ ] T136 [P] Create GoogleTest distributed fixture in include/wadjet/testing/distributed_fixture.hpp per plan.md
- [ ] T137 Implement DistributedTestFixture class with setup/teardown for multi-node tests in src/testing/distributed_fixture.cpp
- [ ] T138 [P] Create unit tests for DistributedTestFixture in tests/testing/test_distributed_fixture.cpp

### Topology Visualization (FR-019)

- [ ] T139 [US2] Create NetworkTopology class interface in include/wadjet/distributed/topology.hpp
- [ ] T140 [US2] Implement NetworkTopology::from_scenario() parsing node/interface layout in src/distributed/topology.cpp
- [ ] T141 [US2] Implement NetworkTopology::visualize_flow() generating Mermaid/DOT diagram in src/distributed/topology.cpp
- [ ] T142 [P] [US2] Create unit tests for NetworkTopology in tests/distributed/test_topology.cpp

### Network Partition Handling (FR-045)

- [ ] T143 [US1] Implement split-brain detection via heartbeat quorum in src/distributed/coordinator.cpp
- [ ] T144 [US1] Add partition_detected callback to CoordinatorConfig in include/wadjet/distributed/coordinator.hpp
- [ ] T145 [US1] Implement graceful degradation when minority partition detected in src/distributed/coordinator.cpp

### Parallel Scenario Execution (FR-048)

- [ ] T146 [US4] Implement TestCoordinator::run_scenarios_parallel() for concurrent execution in src/distributed/coordinator.cpp
- [ ] T147 [US4] Add scenario isolation (separate result aggregation per scenario) in src/distributed/coordinator.cpp
- [ ] T148 [US4] Create integration test for parallel scenario execution in tests/integration/test_parallel_scenarios.cpp

### Observability Completeness (FR-058, FR-059, FR-061)

- [ ] T149 [US5] Implement PCAP naming convention: {test_name}_{node_id}_{timestamp}.pcap in src/distributed/node.cpp
- [ ] T150 [US5] Add barrier event logging with timestamps to coordinator log output in src/distributed/coordinator.cpp
- [ ] T151 [US5] Implement TestCoordinator::run_replay() executing scenario against saved PCAPs in src/distributed/coordinator.cpp
- [ ] T152 [US5] Create integration test for replay mode in tests/integration/test_replay_mode.cpp

### Performance Metrics Completeness (FR-037)

- [ ] T153 [US5] Add throughput_packets_per_sec field to NodeResult in include/wadjet/distributed/result.hpp
- [ ] T154 [US5] Add packet_loss_count field to NodeResult in include/wadjet/distributed/result.hpp
- [ ] T155 [US5] Implement throughput and packet loss calculation in src/distributed/node.cpp

### Protocol Version Compatibility (Edge Case Resolution)

- [ ] T156 [US1] Add protocol_version field to RegisterNodeRequest in proto/distributed_test.proto
- [ ] T157 [US1] Implement version compatibility check in RegisterNode RPC handler in src/distributed/grpc/service.cpp

**Checkpoint**: All spec requirements now have implementing tasks

---

## Phase 12: Polish & Final Validation

**Purpose**: Final validation, documentation, cleanup

- [ ] T158 Run quickstart.md validation scenarios end-to-end
- [ ] T159 Verify all 61 functional requirements are covered (FR-001 to FR-061, FR-049 DEFERRED)
- [ ] T160 Verify all 13 success criteria are met (SC-001 to SC-013)
- [ ] T161 Run full test suite with coverage report
- [ ] T162 Performance validation: <10ms capture jitter, <1% coordination overhead
- [ ] T163 Code review and cleanup
- [ ] T164 Update CHANGELOG.md with M14 distributed testing entry

---

## Dependencies & Execution Order

### Phase Dependencies

```text
Phase 1: Setup ──────────────────────────────────────► Phase 2: Foundational
                                                              │
                                                              ▼
                              ┌────────────────────────────────┴────────────────────────────────┐
                              │                                                                  │
                              ▼                                                                  ▼
                    Phase 3: US1 (Coordination)                                        Phase 4: US2 (Capture)
                              │                                                                  │
                              └──────────────────────┬───────────────────────────────────────────┘
                                                     │
                                                     ▼
                                           Phase 5: US3 (Assertions)
                                                     │
                                     ┌───────────────┴───────────────┐
                                     │                               │
                                     ▼                               ▼
                           Phase 6: US4 (Scenarios)         Phase 7: US5 (Results)
                                     │                               │
                                     └───────────────┬───────────────┘
                                                     │
                                                     ▼
                                           Phase 8: FFI Bindings
                                                     │
                                                     ▼
                                           Phase 9: CLI Tools
                                                     │
                                                     ▼
                                          Phase 10: Examples
                                                     │
                                                     ▼
                                   Phase 11: Infrastructure Completeness
                                                     │
                                                     ▼
                                          Phase 12: Polish
```

### User Story Dependencies

| User Story              | Depends On              | Can Start After      |
| ----------------------- | ----------------------- | -------------------- |
| US1 (Coordination)      | Phase 2 Foundational    | T015 complete        |
| US2 (Capture)           | Phase 2 Foundational    | T015 complete        |
| US3 (Assertions)        | US1 + US2               | T036 + T052 complete |
| US4 (Scenarios)         | US1 + US2 + US3         | T071 complete        |
| US5 (Results)           | US1                     | T036 complete        |

### Parallel Opportunities

**Within Phase 1 (Setup)**:

```text
T003 [P] ─┬─ T004 [P] ─┬─ T006 [P]
          │            │
          └────────────┘
```

**Within Phase 2 (Foundational)**:

```text
T007 → T008 → T009 [P] ─┬─ T010 [P]
                        │
T011 → T012 ────────────┼─ T013 [P]
                        │
T014 [P] ───────────────┘
```

**Within Phase 3-5 (P1 Stories) - After Phase 2**:

```text
US1: T016-T018 [P] → T019-T035 → T036
                                  │
US2: T037-T038 [P] → T039-T051 → T052
                                  │
                                  └─► US3: T053-T071
```

**FFI Bindings (Phase 8) - All [P] tasks can run in parallel**:

```text
T099-T103 [P] (C ABI)
T104 [P], T111-T112 [P] (Python scaffolding)
T114-T115 [P], T120 [P] (Rust scaffolding)
```

**Phase 11 Infrastructure - Many [P] tasks**:

```text
T135-T138 [P] (Headers/Fixtures)
T139-T142 (Topology - sequential)
T143-T145 (Partition - sequential)
T149-T152 (Observability - sequential)
T153-T155 (Metrics - sequential)
```

---

## Implementation Strategy

### MVP First (User Stories 1-3 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL - blocks all stories)
3. Complete Phase 3: US1 Multi-Node Coordination
4. Complete Phase 4: US2 Synchronized Capture
5. Complete Phase 5: US3 Distributed Assertions
6. **STOP and VALIDATE**: Core distributed testing works
7. Deploy/demo 3-node SOME/IP validation

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. Add US1 (Coordination) → Nodes can coordinate → **Checkpoint**
3. Add US2 (Capture) → Synchronized capture works → **Checkpoint**
4. Add US3 (Assertions) → **MVP Complete** - can validate distributed protocols
5. Add US4 (Scenarios) → Declarative YAML scenarios
6. Add US5 (Results) → CI integration with JUnit XML
7. Add FFI Bindings → Python/Rust users enabled
8. Add CLI Tools → wadjet-coordinator/wadjet-node ready
9. Add Infrastructure Completeness → Full FR coverage
10. Polish & Validation → Release ready

---

## Requirement Coverage Matrix

| FR        | Task(s)                    | Status     |
| --------- | -------------------------- | ---------- |
| FR-001    | T019-T024                  | ✅ Covered |
| FR-002    | T017, T122                 | ✅ Covered |
| FR-003    | T011-T013, T029-T030       | ✅ Covered |
| FR-004    | T077, T081                 | ✅ Covered |
| FR-005    | T089-T090                  | ✅ Covered |
| FR-006    | T090-T093                  | ✅ Covered |
| FR-007    | T031, T033                 | ✅ Covered |
| FR-008    | T024                       | ✅ Covered |
| FR-009    | T032                       | ✅ Covered |
| FR-010    | T033, T095                 | ✅ Covered |
| FR-011    | T042                       | ✅ Covered |
| FR-012    | T007-T009                  | ✅ Covered |
| FR-013    | T008                       | ✅ Covered |
| FR-014    | T008                       | ✅ Covered |
| FR-015    | T051                       | ✅ Covered |
| FR-016    | T007-T009                  | ✅ Covered |
| FR-017    | T047-T049                  | ✅ Covered |
| FR-018    | T043-T046                  | ✅ Covered |
| FR-019    | T139-T142                  | ✅ Covered |
| FR-020    | T056-T057                  | ✅ Covered |
| FR-021    | T058-T059                  | ✅ Covered |
| FR-022    | T060-T061                  | ✅ Covered |
| FR-023    | T062-T063                  | ✅ Covered |
| FR-024    | T064-T065                  | ✅ Covered |
| FR-025    | T055-T066                  | ✅ Covered |
| FR-026    | T053, T086-T088            | ✅ Covered |
| FR-027    | T074-T076                  | ✅ Covered |
| FR-028    | T072-T073                  | ✅ Covered |
| FR-029    | T078                       | ✅ Covered |
| FR-030    | T079                       | ✅ Covered |
| FR-031    | T080                       | ✅ Covered |
| FR-032    | T081                       | ✅ Covered |
| FR-033    | T078-T079                  | ✅ Covered |
| FR-034    | T034, T090                 | ✅ Covered |
| FR-035    | T091, T096                 | ✅ Covered |
| FR-036    | T087                       | ✅ Covered |
| FR-037    | T053, T088, T153-T155      | ✅ Covered |
| FR-038    | T094-T095                  | ✅ Covered |
| FR-039    | T093                       | ✅ Covered |
| FR-040    | -                          | ⏸️ MAY (optional) |
| FR-041    | T002, T015, T022-T029      | ✅ Covered |
| FR-042    | T017-T018                  | ✅ Covered |
| FR-043    | -                          | ✅ Constraint (no task) |
| FR-044    | T162                       | ✅ Covered |
| FR-045    | T143-T145                  | ✅ Covered |
| FR-046    | T162                       | ✅ Covered |
| FR-047    | T162                       | ✅ Covered |
| FR-048    | T146-T148                  | ✅ Covered |
| FR-049    | -                          | ⏸️ DEFERRED |
| FR-050    | T099-T103                  | ✅ Covered |
| FR-051    | T104-T113                  | ✅ Covered |
| FR-052    | T114-T120                  | ✅ Covered |
| FR-053    | T106-T108                  | ✅ Covered |
| FR-054    | T113                       | ✅ Covered |
| FR-055    | T117-T119                  | ✅ Covered |
| FR-056    | T103                       | ✅ Covered |
| FR-057    | T095                       | ✅ Covered |
| FR-058    | T149                       | ✅ Covered |
| FR-059    | T150                       | ✅ Covered |
| FR-060    | T053, T086                 | ✅ Covered |
| FR-061    | T151-T152                  | ✅ Covered |

**Coverage**: 59/61 FRs covered (96.7%), 1 MAY (optional), 1 DEFERRED (per YAGNI)

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- US1, US2 are both P1 and can proceed in parallel after Phase 2
- US3 requires captures from US2 for assertion evaluation
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- FR-040 (OpenTelemetry) is MAY - optional per spec wording
- FR-049 (Hierarchical coordination) is DEFERRED per YAGNI principle

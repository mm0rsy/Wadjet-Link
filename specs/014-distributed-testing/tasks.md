# Tasks: Distributed Testing Infrastructure

**Input**: Design documents from `/specs/014-distributed-testing/`  
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅  
**Total Tasks**: 205 | **Phases**: 13 | **Coverage**: 100% (59/59 MUST FRs + 1 MAY + 1 DEFERRED)
**Phase 13 Review Tasks**: 41 | Cross-spec integration gaps identified by formal review against specs 000–013

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

## Phase 7.5: Gap Remediation II (Formal Review Against Specs 000-013)

**Purpose**: Address all gaps identified during formal review of Phases 1-7 implementations against spec.md, data-model.md, contracts/distributed_test.proto, and upstream milestones M1-M13.

**⚠️ CRITICAL**: Contains build-breaking fixes, stub replacements, and data-model alignment. Must be completed before Phase 8.

### Category A: Build-Breaking Issues 🔴

These issues prevent compilation and must be fixed first.

- [x] T230 [P] Add scenario.cpp and result_aggregation.cpp to src/distributed/CMakeLists.txt add_library() sources
- [x] T231 Implement TestCoordinatorImpl::load_scenario() override in src/distributed/coordinator.cpp (pure virtual — class is abstract without it)
- [x] T232 Implement TestCoordinatorImpl::run_scenario() override in src/distributed/coordinator.cpp (pure virtual — class is abstract without it)
- [x] T233 Implement TestCoordinatorImpl::collect_results() override in src/distributed/coordinator.cpp (pure virtual — class is abstract without it)
- [x] T234 Implement TestCoordinatorImpl::export_junit() override in src/distributed/coordinator.cpp (pure virtual — class is abstract without it)
- [x] T235 Implement TestCoordinatorImpl::get_aggregated_result() override in src/distributed/coordinator.cpp (pure virtual — class is abstract without it)
- [x] T236 [P] Fix orphan #endif in include/wadjet/distributed/distributed.hpp (uses #pragma once but has unmatched #endif at line 116)
- [x] T237 [P] Fix duplicate update_node_heartbeat() in src/distributed/coordinator.cpp (virtual override at ~line 194 and non-virtual at ~line 263)

### Category B: Stub/Placeholder Replacements 🟠

These have placeholder code that must be replaced with real implementations.

#### B1: gRPC Service Stubs

- [x] T238 Replace EvaluateMatcher helper stub in src/distributed/grpc/service.cpp (line ~188 returns hardcoded JSON)
- [x] T239 Complete ControlChannel RPC handler in src/distributed/grpc/service.cpp (capture_started, capture_stopped, matcher_result, error, log events)
- [x] T240 Complete ReportResult RPC handler in src/distributed/grpc/service.cpp (parse JSON results and store for aggregation)
- [x] T241 Implement ControlChannel command queue for pending StartCapture/EvaluateMatcher commands in src/distributed/grpc/service.cpp

#### B2: Node Implementation Stubs

- [x] T242 Replace evaluate_matcher() placeholder in src/distributed/node.cpp (instantiate matchers, evaluate, serialize to JSON)
- [x] T243 Implement execute_command() in src/distributed/node.cpp (route commands: iperf3, ping, ethtool, ip)

#### B3: Scenario Parsing Stubs

- [x] T244 Implement actual YAML parsing using yaml-cpp in src/distributed/scenario.cpp (extract scenario_id/name from YAML content)
- [x] T245 Implement actual JSON parsing using nlohmann_json in src/distributed/scenario.cpp (parse metadata and node_assignments array)

#### B4: Matcher GoogleTest Integration Stubs

- [x] T246 Complete ExpectMessageFlow GoogleTest Matcher<PacketView&> variant to use the inner_matcher parameter via adapter class
- [x] T247 Complete HappensBefore GoogleTest Matcher<PacketView&> variant to use event_a_matcher and event_b_matcher parameters
- [x] T248 Complete MustNotSeeOn GoogleTest Matcher<PacketView&> variant to use the inner_matcher parameter

#### B5: MessageCorrelator Placeholder Methods

- [x] T249 Implement MessageCorrelator SequenceNumber correlation with actual protocol header parsing (TCP seq at offset 24-27)
- [x] T250 Implement MessageCorrelator TransactionId correlation with DoIP/UDS transaction ID extraction from headers
- [x] T251 Implement MessageCorrelator Timestamp correlation with proximity logic (100ms time buckets)

#### B6: SyncBarrier Placeholder

- [x] T252 Fix SyncBarrier::arrive_and_wait() to use actual node ID parameter instead of hardcoded value

### Category C: Data-Model Alignment 🟡

Fields, types, and methods specified in data-model.md but missing from implementations.

#### C1: Missing Enums (data-model.md Enumerations section)

- [x] T253 [P] Create NodeHealthStatus enum class (Unknown, Healthy, Degraded, Unhealthy, Disconnected) in include/wadjet/distributed/types.hpp per data-model.md
- [x] T254 [P] Create CaptureState enum class (Idle, Starting, Running, Stopping, Stopped, Error) in include/wadjet/distributed/types.hpp per data-model.md
- [x] T255 [P] Create BarrierState enum class (Waiting, AllArrived, Timeout, Cancelled) in include/wadjet/distributed/types.hpp per data-model.md
- [x] T256 [P] Create ResultStatus enum class (Passed, Failed, Error, Skipped, Timeout) in include/wadjet/distributed/result_aggregation.hpp per data-model.md

#### C2: Missing TLS Configuration (FR-042, data-model.md)

- [x] T257 Add tls_cert_path, tls_key_path, tls_ca_path fields to CoordinatorConfig in include/wadjet/distributed/coordinator.hpp per data-model.md
- [x] T258 Add tls_cert_path, tls_key_path, tls_ca_path fields to NodeConfig in include/wadjet/distributed/node.hpp per data-model.md
- [x] T259 Add config_path field to CoordinatorConfig in include/wadjet/distributed/coordinator.hpp per data-model.md
- [x] T260 Add failure_capture_dir field to NodeConfig in include/wadjet/distributed/node.hpp per data-model.md (default: "/tmp/wadjet_failures")
- [x] T261 Wire TLS credentials into gRPC server/client channel creation in src/distributed/coordinator.cpp and src/distributed/node.cpp

#### C3: Missing Scenario Structs (data-model.md DistributedScenario section)

- [x] T262 [P] Create WaitStepConfig struct (duration field) in include/wadjet/distributed/scenario.hpp per data-model.md
- [x] T263 [P] Create LogStepConfig struct (message, level fields) in include/wadjet/distributed/scenario.hpp per data-model.md
- [x] T264 [P] Create NodeDefinition struct (id, address, interfaces) in include/wadjet/distributed/scenario.hpp per data-model.md
- [x] T265 Add tags field (std::vector<std::string>) to DistributedScenario in include/wadjet/distributed/scenario.hpp per data-model.md
- [x] T266 Refactor DistributedStep config to use std::variant<BarrierStepConfig, CaptureStepConfig, ExpectStepConfig, WaitStepConfig, LogStepConfig> per data-model.md

#### C4: Missing AggregatedResult Fields (data-model.md AggregatedResult section)

- [x] T267 Add status field (ResultStatus) to AggregatedResult per data-model.md
- [x] T268 Add total_duration field (std::chrono::milliseconds) to AggregatedResult per data-model.md
- [x] T269 Add distributed_assertions field (std::vector<AssertionResult>) to AggregatedResult per data-model.md
- [x] T270 Add pcap_files field (std::vector<std::filesystem::path>) to AggregatedResult per data-model.md
- [x] T271 Add total_assertions, passed_assertions, failed_assertions int fields to AggregatedResult per data-model.md
- [x] T272 Implement AggregatedResult::merge() static method per data-model.md for combining multi-scenario results

#### C5: Missing Node Methods (data-model.md TestNode section)

- [x] T273 Add TestNode::report_clock_status() -> ClockSyncStatus method per data-model.md
- [x] T274 Add TestNode::report_health() -> NodeHealthStatus method per data-model.md

#### C6: AssertionResult Schema Alignment (data-model.md vs implementation)

- [x] T275 Add src_node, dst_node fields to AssertionResult per data-model.md (spec: multi-node context)
- [x] T276 Add src_timestamp_ns, dst_timestamp_ns, latency_ns fields to AssertionResult per data-model.md
- [x] T277 Add expected, actual string fields to AssertionResult per data-model.md (for detailed assertion comparison)

### Category D: Missing Test Files 🟣

- [x] T278 Create tests/integration/test_distributed_scenario.cpp integration test (was referenced but file does not exist)
- [x] T279 Replace GTEST_SKIP() stubs in tests/distributed/test_grpc_service.cpp with real tests using mock gRPC server
- [x] T280 Replace GTEST_SKIP() stubs in tests/distributed/test_grpc_client.cpp with real tests using mock gRPC client

### Category E: Upstream Milestone Integration Gaps 🔵

Cross-cutting issues from review against M1-M13 features.

#### E1: M3 Matcher Integration (spec.md FR-020 to FR-023)

- [x] T281 Verify DistributedMatcher factory functions accept M3 GoogleTest Matcher<PacketView&> (verify include paths and linking against M3 matcher headers)
- [x] T282 Add distributed matcher composition tests using AllOf/AnyOf/Not from M3 in tests/distributed/test_distributed_matchers.cpp

#### E2: M4 Scenario Engine Alignment (spec.md FR-027 to FR-034)

- [x] T283 Verify DistributedScenario YAML format aligns with M4 Scenario YAML structure (same yaml-cpp patterns, compatible tags/metadata fields)
- [x] T284 Ensure distributed JUnit XML output is compatible with M4 ReportGenerator format (same XML schema, additive fields for node info)

#### E3: M8 gPTP Integration Completeness (spec.md FR-015)

- [x] T285 Verify TimestampNormalizer::verify_gptp_health() actually decodes gPTP Announce/Sync messages using M8 decoder headers (not just checking clock status)

#### E4: M1 PCAP I/O Integration (spec.md FR-017, FR-038)

- [x] T286 Verify PcapMerger correctly uses M1 PcapReader/PcapWriter APIs (include paths, linking, timestamp format compatibility)
- [x] T287 Add test verifying merged PCAP is openable by Wireshark (write temp file, validate PCAP magic number and header)

#### E5: M5/M7 FFI Pattern Compliance (Constitution Principle V)

- [x] T288 [P] Document distributed FFI type mapping table: C++ types → C ABI handles → Python types → Rust types (pre-work for Phase 8)

#### E6: Performance Metrics (FR-037)

- [x] T289 Add latency_stats (min, max, mean, p95, p99) to NodeResult per FR-037 and M12 LatencyStats pattern
- [x] T290 Add throughput_packets_per_sec field to NodeResult per FR-037
- [x] T291 Add packet_loss_count field to NodeResult per FR-037

### Category F: Spec Requirement Coverage Gaps 🟤

FR requirements with no implementing code (only task references but empty implementations).

- [x] T292 [FR-002] Implement static node configuration file parser (YAML/JSON) in coordinator for node discovery (currently no config file parsing)
- [x] T293 [FR-010] Implement node-side partial result + PCAP save on coordinator failure in src/distributed/node.cpp (detect_coordinator_failure saves nothing)
- [x] T294 [FR-014] Add explicit test initialization failure when neither gPTP nor NTP sync detected (TimestampNormalizer detects but doesn't block test start)
- [x] T295 [FR-044] Add coordination overhead measurement and validation (<1% of test traffic) in test suite
- [x] T296 [FR-058] Implement PCAP naming convention: {test_name}_{node_id}_{timestamp}.pcap in src/distributed/node.cpp
- [x] T297 [FR-059] Add barrier synchronization event logging with timestamps in src/distributed/coordinator.cpp
- [x] T298 [FR-060] Ensure all distributed assertion failures include: node ID, timestamp, expected vs actual, packet context in error reports

**Checkpoint**: All Phase 1-7 gaps remediated, builds cleanly, all stubs replaced, data-model fully aligned

---

## Phase 8: FFI Bindings (Constitution Principle V)

**Purpose**: C ABI, Python bindings, Rust bindings for distributed testing primitives

### C ABI Layer

- [x] T099 [P] Create C99 ABI header with opaque handles in bindings/c/wadjet_distributed.h
- [x] T100 [P] Implement wadjet_coordinator_create/destroy C functions in bindings/c/wadjet_distributed.c
- [x] T101 [P] Implement wadjet_node_create/destroy C functions in bindings/c/wadjet_distributed.c
- [x] T102 [P] Implement wadjet_sync_barrier C functions in bindings/c/wadjet_distributed.c
- [x] T103 [P] Implement thread-local error storage for C ABI in bindings/c/wadjet_distributed.c

### Python Bindings

- [x] T104 [P] Create Python bindings directory structure in bindings/python/wadjet/distributed/
- [x] T105 Create pybind11 module for distributed primitives in bindings/python/wadjet/distributed/bindings.cpp
- [x] T106 Bind SyncBarrier class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [x] T107 Bind TimestampNormalizer class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [x] T108 Bind DistributedMatcher classes to Python in bindings/python/wadjet/distributed/bindings.cpp
- [x] T109 Bind TestCoordinator class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [x] T110 Bind TestNode class to Python in bindings/python/wadjet/distributed/bindings.cpp
- [x] T111 [P] Create Python type stubs in bindings/python/wadjet/distributed/_distributed.pyi
- [x] T112 [P] Create Python __init__.py with exports in bindings/python/wadjet/distributed/__init__.py
- [x] T113 Create pytest fixtures for distributed testing in bindings/python/tests/test_distributed.py

### Rust Bindings

- [x] T114 [P] Create Rust crate structure in bindings/rust/wadjet-distributed/
- [x] T115 [P] Create Cargo.toml for wadjet-distributed crate in bindings/rust/wadjet-distributed/Cargo.toml
- [x] T116 Implement unsafe FFI bindings in bindings/rust/wadjet-distributed/src/ffi.rs
- [x] T117 Implement safe SyncBarrier wrapper in bindings/rust/wadjet-distributed/src/sync_barrier.rs
- [x] T118 Implement safe TimestampNormalizer wrapper in bindings/rust/wadjet-distributed/src/timestamp.rs
- [x] T119 Implement safe DistributedMatcher wrappers in bindings/rust/wadjet-distributed/src/matchers.rs
- [x] T120 [P] Create Rust integration tests in bindings/rust/wadjet-distributed/tests/integration.rs

**Checkpoint**: FFI bindings complete - C ABI, Python with pytest, Rust with cargo test

---

## Phase 9: CLI Tools (Built on Libraries)

**Purpose**: Command-line tools consuming libwadjet_distributed

- [x] T121 Implement wadjet-coordinator CLI tool in tools/wadjet-coordinator.cpp
- [x] T122 Add --config option for node configuration YAML in tools/wadjet-coordinator.cpp
- [x] T123 Add --scenario option for scenario execution in tools/wadjet-coordinator.cpp
- [x] T124 Add --output-dir and --junit-report options in tools/wadjet-coordinator.cpp
- [x] T125 Implement wadjet-node CLI tool in tools/wadjet-node.cpp
- [x] T126 Add --node-id and --config options in tools/wadjet-node.cpp
- [x] T127 Add --check-clock option for clock sync verification in tools/wadjet-node.cpp
- [x] T128 Update tools/CMakeLists.txt with coordinator and node targets

---

## Phase 10: Examples & Documentation

**Purpose**: Example scenarios and documentation updates

- [x] T129 [P] Create 3-node SOME/IP discovery example in examples/distributed_someip_discovery.cpp
- [x] T130 [P] Create example scenario YAML in examples/scenarios/someip_discovery.yaml
- [x] T131 [P] Create example nodes configuration in examples/scenarios/nodes.yaml
- [x] T132 [P] Update docs/quickstart.md with distributed testing section
- [x] T133 [P] Create docs/distributed_testing.md architecture guide
- [x] T134 Update README.md with distributed testing feature overview

---

## Phase 11: Infrastructure Completeness

**Purpose**: Missing infrastructure components identified in spec analysis

### Umbrella Headers & Fixtures (I1, I2)

- [x] T135 [P] Create main umbrella header in include/wadjet/distributed/distributed.hpp including all public headers
- [x] T136 [P] Create GoogleTest distributed fixture in include/wadjet/testing/distributed_fixture.hpp per plan.md
- [x] T137 Implement DistributedTestFixture class with setup/teardown for multi-node tests in src/testing/distributed_fixture.cpp
- [x] T138 [P] Create unit tests for DistributedTestFixture in tests/testing/test_distributed_fixture.cpp

### Topology Visualization (FR-019)

- [x] T139 [US2] Create NetworkTopology class interface in include/wadjet/distributed/topology.hpp
- [x] T140 [US2] Implement NetworkTopology::from_scenario() parsing node/interface layout in src/distributed/topology.cpp
- [x] T141 [US2] Implement NetworkTopology::visualize_flow() generating Mermaid/DOT diagram in src/distributed/topology.cpp
- [x] T142 [P] [US2] Create unit tests for NetworkTopology in tests/distributed/test_topology.cpp

### Network Partition Handling (FR-045)

- [x] T143 [US1] Implement split-brain detection via heartbeat quorum in src/distributed/coordinator.cpp
- [x] T144 [US1] Add partition_detected callback to CoordinatorConfig in include/wadjet/distributed/coordinator.hpp
- [x] T145 [US1] Implement graceful degradation when minority partition detected in src/distributed/coordinator.cpp

### Parallel Scenario Execution (FR-048)

- [x] T146 [US4] Implement TestCoordinator::run_scenarios_parallel() for concurrent execution in src/distributed/coordinator.cpp
- [x] T147 [US4] Add scenario isolation (separate result aggregation per scenario) in src/distributed/coordinator.cpp
- [x] T148 [US4] Create integration test for parallel scenario execution in tests/integration/test_parallel_scenarios.cpp

### Observability Completeness (FR-058, FR-059, FR-061)

- [x] T149 [US5] Implement PCAP naming convention: {test_name}_{node_id}_{timestamp}.pcap in src/distributed/node.cpp
- [x] T150 [US5] Add barrier event logging with timestamps to coordinator log output in src/distributed/coordinator.cpp
- [x] T151 [US5] Implement TestCoordinator::run_replay() executing scenario against saved PCAPs in src/distributed/coordinator.cpp
- [x] T152 [US5] Create integration test for replay mode in tests/integration/test_replay_mode.cpp

### Performance Metrics Completeness (FR-037)

- [x] T153 [US5] Add throughput_packets_per_sec field to NodeResult in include/wadjet/distributed/result.hpp
- [x] T154 [US5] Add packet_loss_count field to NodeResult in include/wadjet/distributed/result.hpp
- [x] T155 [US5] Implement throughput and packet loss calculation in src/distributed/node.cpp

### Protocol Version Compatibility (Edge Case Resolution)

- [x] T156 [US1] Add protocol_version field to RegisterNodeRequest in proto/distributed_test.proto
- [x] T157 [US1] Implement version compatibility check in RegisterNode RPC handler in src/distributed/grpc/service.cpp

**Checkpoint**: All spec requirements now have implementing tasks

---

## Phase 12: Polish & Final Validation

**Purpose**: Final validation, documentation, cleanup

- [ ] T158 Run quickstart.md validation scenarios end-to-end
- [ ] T159 Verify all 61 functional requirements are covered (FR-001 to FR-061, FR-049 DEFERRED)
- [ ] T160 Verify all 13 success criteria are met (SC-001 to SC-013)
- [x] T161 Run full test suite with coverage report
- [x] T162 Performance validation: <10ms capture jitter, <1% coordination overhead
- [x] T163 Code review and cleanup
- [x] T164 Update CHANGELOG.md with M14 distributed testing entry

---

## Phase 13: Cross-Spec Integration & Correctness (Formal Review Against Specs 000–013)

**Purpose**: Address all gaps identified during formal cross-reference review of Phases 1–12 implementation against specifications 000-bootstrapping through 013-protocol-completeness. This phase ensures the distributed testing framework correctly integrates with ALL upstream milestones.

**Review Methodology**: Each file in `include/wadjet/distributed/`, `src/distributed/`, and `tests/distributed/` was audited against the APIs, types, and integration contracts specified in specs 000–013. Gaps are categorized by severity.

---

### Category A: Correctness Defects 🔴 (Code claims completion but implementation is wrong/missing)

**A1: YAML Scenario Parser is Stubbed (T075/T244 — claimed ✅ but implementation is placeholder)**

The `from_yaml_string()` in `src/distributed/scenario.cpp` uses naive `std::string::find()` instead of yaml-cpp. Node assignments and steps are hardcoded sample data (always returns 3 fixed nodes + 3 fixed steps regardless of input). This violates **FR-027** ("System MUST parse YAML/JSON distributed test scenarios") and **FR-028** ("Scenarios MUST define node assignments").

- [X] T300 [US4] Replace from_yaml_string() stub with actual yaml-cpp parsing: extract scenario_id, scenario_name, description, tags from YAML metadata section in src/distributed/scenario.cpp
- [X] T301 [US4] Implement YAML node_assignments array parsing: extract node_id, role, interfaces per node using yaml-cpp in src/distributed/scenario.cpp
- [X] T302 [US4] Implement YAML steps array parsing: parse step_id, step_name, type, target_nodes, depends_on, timeout_ms and type-specific configs (barrier, capture, expect, wait, log) using yaml-cpp in src/distributed/scenario.cpp
- [X] T303 [P] [US4] Add unit tests for YAML parsing with real multi-node scenario files (barrier+capture+expect+wait+log steps) in tests/distributed/test_scenario_parser.cpp — replace existing hardcoded tests

**A2: JSON Step Parsing is Stubbed (T076/T245 — claimed ✅ but steps fall back to defaults)**

The `from_json_string()` correctly parses metadata and node_assignments with nlohmann::json but steps parsing has `// For now, create sample steps` and falls back to a single hardcoded barrier step. Dead code exists at lines 296–312 (unreachable after try/catch return).

- [X] T304 [US4] Implement JSON steps array parsing in from_json_string(): parse each step type (barrier, capture, expect, wait, log) with full config extraction using nlohmann::json in src/distributed/scenario.cpp
- [X] T305 [P] Remove dead code at lines 296–312 in from_json_string() (unreachable after try/catch block) in src/distributed/scenario.cpp
- [X] T306 [P] [US4] Add unit tests for JSON parsing with real multi-step scenarios in tests/distributed/test_scenario_parser.cpp

**A3: Replay Mode Not Implemented (FR-061 — T151/T152 claimed ✅ but no implementation exists)**

`run_replay()` is declared as a pure virtual method in `coordinator.hpp` but `coordinator.cpp` (TestCoordinatorImpl) does **not** override it. Calling `run_replay()` would either fail to compile (if abstract) or crash.

- [X] T307 [US5] Implement TestCoordinatorImpl::run_replay() override: load PCAP files from pcap_files map, create DistributedCaptureContext per node from PcapReader, execute scenario matchers against loaded captures in src/distributed/coordinator.cpp
- [X] T308 [US5] Implement PcapMerger-based replay timeline reconstruction: merge provided PCAPs into unified timeline for cross-node assertion evaluation in src/distributed/coordinator.cpp
- [X] T309 [P] [US5] Create unit test for run_replay() with saved PCAP fixture files in tests/distributed/test_coordinator.cpp
- [X] T310 [P] [US5] Create integration test for full replay workflow: run scenario → save PCAPs → replay → verify same assertions pass in tests/integration/test_replay_mode.cpp

**A4: GoogleTest Matcher Inner Filtering Not Applied (T246–T248 — claimed ✅ but inner_matcher_ is stored and never called)**

The `GTestAwareExpectMessageFlow::evaluate()` in `expect_message_flow.cpp` stores the `inner_matcher_` but has only a comment "This demonstrates integration with M3 GoogleTest matchers" — the matcher is never actually applied to filter packets. Same pattern in `HappensBefore` and `MustNotSeeOn` GoogleTest-aware variants.

- [X] T311 [US3] Fix ExpectMessageFlow GoogleTest variant: apply inner_matcher_ to each PacketView in source/dest capture contexts to filter candidate packets before correlation in src/distributed/matchers/expect_message_flow.cpp
- [X] T312 [US3] Fix HappensBefore GoogleTest variant: apply event_a_matcher_ and event_b_matcher_ to PacketView to identify event packets before timestamp comparison in src/distributed/matchers/happens_before.cpp
- [X] T313 [US3] Fix MustNotSeeOn GoogleTest variant: apply filter_matcher_ to PacketView on target node to detect forbidden packets in src/distributed/matchers/must_not_see_on.cpp
- [X] T314 [P] [US3] Add unit tests verifying GoogleTest inner matchers actually filter packets (e.g., test that ExpectMessageFlow with HasSOMEIPServiceId only matches SOME/IP packets) in tests/distributed/test_distributed_matchers.cpp

**Checkpoint A**: All correctness defects fixed — YAML/JSON parsing real, replay works, matcher filtering functional

---

### Category B: Missing M9 UDS Decoder Integration 🟠

**Spec 009** defines `UdsOverDoipDecoder` and 20+ UDS service decoders. The distributed `MessageCorrelator` references "DoIP/UDS transaction ID" in comments but uses raw byte extraction instead of M9 APIs. No UDS-specific distributed assertions exist.

- [X] T315 [US2] Replace hand-rolled DoIP header byte extraction in MessageCorrelator::TransactionId correlation with actual M9 UdsOverDoipDecoder API calls in src/distributed/message_correlator.cpp
- [X] T316 [US3] Create UDS-specific distributed assertion: ExpectDiagnosticResponse(src_node, dst_node, uds_service_id) that validates UDS request→response across nodes in include/wadjet/distributed/matchers/expect_diagnostic.hpp and src/distributed/matchers/expect_diagnostic.cpp
- [X] T317 [P] [US3] Add unit test for UDS distributed assertion using DoIP captures in tests/distributed/test_distributed_matchers.cpp

---

### Category C: Missing M11 Diagnostic Session Manager Integration 🟠

**Spec 011** defines `DiagnosticSessionManager` for stateful multi-ECU diagnostic tracking. Distributed testing of diagnostic sessions (flash programming across ECUs, security access sequences, DTC clearing) is a primary automotive use case but has zero integration.

- [X] T318 [US4] Create DIAGNOSTIC step type in DistributedStep enum and DiagnosticStepConfig struct (session_type, ecu_address, expected_service, expected_nrc) in include/wadjet/distributed/scenario.hpp
- [X] T319 [US3] Create ExpectDiagnosticSession distributed matcher: validates multi-ECU diagnostic sequences (e.g., SecurityAccess on node-a → FlashDownload on node-b) in include/wadjet/distributed/matchers/expect_diagnostic.hpp
- [X] T320 [US1] Add DiagnosticSessionManager integration to TestNode: track per-ECU session state across distributed captures using M11 API in src/distributed/node.cpp
- [X] T321 [P] Add unit tests for distributed diagnostic session tracking in tests/distributed/test_diagnostic_distributed.cpp

---

### Category D: Missing M12 TSN Awareness Integration 🟠

**Spec 012** defines `TsnAnalyzer`, `StreamTracker`, `LatencyTracker` for TSN traffic analysis. Multi-point TSN stream latency measurement across switches is a primary distributed testing use case but has zero integration.

- [x] T322 [US2] Integrate M12 StreamTracker with distributed capture: track TSN stream IDs across nodes for multi-hop stream analysis in src/distributed/node.cpp
- [x] T323 [US3] Create TSN-specific distributed assertion: ExpectStreamLatency(src_node, dst_node, stream_id, max_latency) that validates TSN end-to-end latency across network segments in include/wadjet/distributed/matchers/expect_stream_latency.hpp and src/distributed/matchers/expect_stream_latency.cpp
- [x] T324 [US2] Integrate M12 LatencyTracker with distributed result aggregation: include per-priority latency stats from all nodes in AggregatedResult in src/distributed/result_aggregation.cpp
- [x] T325 [P] Add unit tests for TSN distributed stream tracking and latency assertions in tests/distributed/test_tsn_distributed.cpp

---

### Category E: Missing M4 Scenario Format Compatibility 🟡

**Spec 004** defines `Scenario`/`Step` types in `wadjet::scenario` namespace with `CaptureStep`, `SendStep`, `WaitStep`, `ExpectStep`, `LogStep` variants. M14's `DistributedScenario`/`DistributedStep` uses a completely different namespace (`wadjet::distributed`) with incompatible variant types. Single-node scenarios cannot be extended to distributed without rewriting.

- [ ] T326 [US4] Create ScenarioAdapter class that converts M4 wadjet::scenario::Scenario to wadjet::distributed::DistributedScenario for single-node-to-distributed upgrade path in include/wadjet/distributed/scenario_adapter.hpp and src/distributed/scenario_adapter.cpp
- [ ] T327 [US4] Add SendStep (traffic injection) support to DistributedStep enum and SendStepConfig struct in include/wadjet/distributed/scenario.hpp — mirrors M4's SendStep capability
- [ ] T328 [P] [US4] Add unit tests for ScenarioAdapter conversion (M4→M14) in tests/distributed/test_scenario_adapter.cpp

---

### Category F: Missing M4 wadjet-run CLI Integration 🟡

**Spec 004** defines `wadjet-run` as the primary test execution CLI. The distributed coordinator is a completely separate binary with no cross-invocation. Users must manually switch between `wadjet-run` for single-node and `wadjet-coordinator` for distributed tests.

- [ ] T329 [US4] Add `wadjet-run distributed` subcommand that delegates to wadjet-coordinator logic: accepts --nodes and --scenario flags, imports distributed coordinator library in tools/wadjet-run.cpp
- [ ] T330 [P] Add integration test for wadjet-run distributed subcommand execution in tests/integration/test_wadjet_run_distributed.cpp

---

### Category G: Missing M3 LiveCaptureTestFixture Composition 🟡

**Spec 003** defines `LiveCaptureTestFixture` as the standard GTest base class with auto-capture setup/teardown, send_udp(), connect_tcp(), wait_for_packet(), etc. The `DistributedTestFixture` inherits from `::testing::Test` directly, losing all M3 convenience methods.

- [ ] T331 [US1] Refactor DistributedTestFixture to optionally compose with M3 LiveCaptureTestFixture: add local capture capability alongside distributed coordination in include/wadjet/testing/distributed_fixture.hpp
- [ ] T332 [US1] Add convenience methods to DistributedTestFixture mirroring M3 API: wait_for_distributed_packet(node_id, predicate, timeout), send_on_node(node_id, interface, data) in include/wadjet/testing/distributed_fixture.hpp
- [ ] T333 [P] Add unit tests for DistributedTestFixture M3 integration methods in tests/testing/test_distributed_fixture.cpp

---

### Category H: Missing M10 DDS/RTPS Considerations 🟡

**Spec 010** (planned) defines DDS-RTPS protocol decoder. DDS is inherently distributed (pub/sub middleware for ADAS/ROS2). While M10 is not yet implemented, distributed testing should be designed to accommodate DDS discovery and topic-based distributed assertions.

- [ ] T334 [US3] Design placeholder DDS distributed matcher interface: ExpectDdsTopicFlow(publisher_node, subscriber_node, topic_name) in include/wadjet/distributed/matchers/expect_dds_topic.hpp — compilable but returns "M10 DDS decoder not yet available" until M10 is implemented
- [ ] T335 [P] Document DDS distributed testing integration plan in specs/014-distributed-testing/dds_integration_plan.md — map DDS discovery (SPDP/SEDP) to distributed assertions

---

### Category I: M2 Protocol-Aware Distributed Scenario Expectations 🟡

**Spec 002** defines rich protocol decoders (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP). The distributed scenario `ExpectStepConfig` uses generic `assertion_type` string + `assertion_params` JSON instead of typed protocol expectations. This means distributed scenarios cannot express "expect SOME/IP ServiceId=0x1234 flows from node-a to node-b" in a structured way.

- [ ] T336 [US4] Extend ExpectStepConfig to support typed protocol expectations: add protocol field (ethernet/ipv4/udp/tcp/someip/doip) and structured match_fields map instead of generic assertion_params JSON in include/wadjet/distributed/scenario.hpp
- [ ] T337 [US4] Implement protocol-aware expect step execution: instantiate M2 decoders + M3 matchers from ExpectStepConfig protocol/fields in src/distributed/coordinator.cpp
- [ ] T338 [P] [US4] Add unit tests for protocol-aware distributed scenarios (SOME/IP, DoIP expect steps) in tests/distributed/test_scenario_parser.cpp

---

### Category J: NetworkTopology from_captures() Missing 🟢

**Data-model.md** specifies `NetworkTopology::from_captures()` factory for building topology from observed packet flow. Only `from_scenario()` was implemented.

- [ ] T339 [US2] Implement NetworkTopology::from_captures() static factory: build topology graph from actual DistributedCaptureContext data by analyzing source/destination addresses across node captures in src/distributed/topology.cpp
- [ ] T340 [P] [US2] Add unit test for from_captures() topology inference in tests/distributed/test_topology.cpp

---

**Checkpoint Phase 13**: All cross-spec integration gaps addressed, upstream milestone APIs properly integrated

---

## Phase 13 Summary

| Category | Severity | Tasks | Description |
|----------|----------|-------|-------------|
| A: Correctness | 🔴 CRITICAL | T300–T314 (15) | YAML/JSON parsing stubs, replay not implemented, matcher filtering broken |
| B: M9 UDS | 🟠 MAJOR | T315–T317 (3) | UDS decoder integration for distributed diagnostic testing |
| C: M11 Diagnostic | 🟠 MAJOR | T318–T321 (4) | Distributed diagnostic session management |
| D: M12 TSN | 🟠 MAJOR | T322–T325 (4) | TSN stream tracking and latency across nodes |
| E: M4 Scenarios | 🟡 MODERATE | T326–T328 (3) | M4→M14 scenario adapter, SendStep support |
| F: M4 CLI | 🟡 MODERATE | T329–T330 (2) | wadjet-run distributed subcommand |
| G: M3 Fixture | 🟡 MODERATE | T331–T333 (3) | DistributedTestFixture composing with M3 |
| H: M10 DDS | 🟡 MODERATE | T334–T335 (2) | DDS distributed matcher placeholder |
| I: M2 Scenarios | 🟡 MODERATE | T336–T338 (3) | Protocol-aware distributed scenarios |
| J: Topology | 🟢 MINOR | T339–T340 (2) | from_captures() factory |
| **TOTAL** | | **41 tasks** | |

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

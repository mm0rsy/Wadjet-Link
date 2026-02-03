# Feature Specification: Distributed Testing Infrastructure

**Feature Branch**: `milestone/014-distributed-testing`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M14 - Distributed Testing

## User Scenarios & Testing

### User Story 1 - Multi-Node Test Coordination (Priority: P1)

As a system integration test engineer, I need to coordinate tests across multiple ECUs and test nodes, so that I can validate distributed automotive systems (e.g., ADAS with multiple cameras and control units).

**Why this priority**: Automotive systems are inherently distributed; single-node testing cannot validate inter-ECU communication.

**Independent Test**: Run test scenario where Node A sends SOME/IP request, Node B sends response, Node C validates exchange.

**Acceptance Scenarios**:

1. **Given** 3 test nodes (A, B, C), **When** test scenario starts, **Then** all nodes synchronize and start capture simultaneously
2. **Given** test coordinator, **When** coordinating nodes, **Then** test steps execute in defined order (sequential or parallel)
3. **Given** distributed assertion, **When** Node A sends message, **Then** Node B and C can assert message reception
4. **Given** test failure on any node, **When** detected, **Then** all nodes are notified and test aborts gracefully
5. **Given** test completion, **When** all nodes finish, **Then** results are aggregated into single test report

---

### User Story 2 - Synchronized Packet Capture Across Nodes (Priority: P1)

As a protocol validation engineer, I need synchronized packet capture on multiple network segments, so that I can correlate messages across ECU boundaries.

**Why this priority**: Protocol validation requires seeing both sides of communication (e.g., request on gateway, response on ECU).

**Independent Test**: Capture gPTP traffic on multiple nodes and verify clock synchronization timestamps align.

**Acceptance Scenarios**:

1. **Given** multiple capture nodes, **When** synchronized via gPTP, **Then** packet timestamps are aligned within 1µs
2. **Given** distributed capture, **When** analyzing multi-hop messages, **Then** system correlates packets by sequence numbers or IDs
3. **Given** network topology (gateway, ECUs), **When** capturing, **Then** system maps packet flow across topology
4. **Given** synchronized capture start, **When** triggered, **Then** all nodes start within 10ms of coordinator signal
5. **Given** capture files from all nodes, **When** merged, **Then** timeline view shows cross-node message flow

---

### User Story 3 - Distributed Assertion Framework (Priority: P1)

As a test automation developer, I need assertions that span multiple nodes, so that I can validate distributed protocols (e.g., "Node A sent X, Node B must receive Y within 100ms").

**Why this priority**: Core feature for distributed testing - must validate causal relationships across nodes.

**Independent Test**: Assert that SOME/IP request on Node A results in response on Node B within latency bound.

**Acceptance Scenarios**:

1. **Given** distributed matcher `ExpectMessageFlow(node_a, node_b)`, **When** message sent on A, **Then** system validates reception on B
2. **Given** latency assertion, **When** specifying `WithinLatency(100ms)`, **Then** system fails if one-way latency (send A → receive B) exceeds deadline
3. **Given** causal ordering assertion, **When** specifying `HappensBefore(event_a, event_b)`, **Then** system validates timestamp ordering
4. **Given** absence assertion, **When** specifying `MustNotSeeOn(node_c, message)`, **Then** system fails if C captures the message
5. **Given** distributed test failure, **When** assertion fails, **Then** error report includes all node states and packet captures

---

### User Story 4 - Test Scenario Orchestration (Priority: P2)

As a validation team lead, I need declarative multi-node test scenarios, so that I can define complex integration tests without writing coordination code.

**Why this priority**: Reduces test maintenance burden; scenarios are more readable than imperative code.

**Independent Test**: Define YAML scenario with multiple nodes, assertions, and timing constraints; verify execution.

**Acceptance Scenarios**:

1. **Given** YAML scenario with node assignments, **When** loaded, **Then** coordinator deploys matchers to correct nodes
2. **Given** scenario with sequential steps, **When** executed, **Then** steps run in order with barrier synchronization
3. **Given** scenario with parallel steps, **When** executed, **Then** multiple nodes execute concurrently
4. **Given** scenario with timing constraints (delays, timeouts), **When** executed, **Then** coordinator enforces timing
5. **Given** scenario failure, **When** occurred, **Then** coordinator collects logs and captures from all nodes

---

### User Story 5 - Distributed Test Results Aggregation (Priority: P2)

As a CI/CD engineer, I need aggregated test results from all nodes in a single report, so that I can integrate distributed tests into build pipeline.

**Why this priority**: CI integration requires standard test output format (JUnit XML, etc.).

**Independent Test**: Run distributed test and verify single JUnit XML report combines results from all nodes.

**Acceptance Scenarios**:

1. **Given** test execution on N nodes, **When** completed, **Then** single JUnit XML file contains all test cases
2. **Given** test failure on node B, **When** aggregated, **Then** report shows which node failed and why
3. **Given** performance metrics, **When** collected, **Then** report includes latency, throughput, packet loss per node
4. **Given** packet captures, **When** test fails, **Then** PCAP files from all nodes are attached to report
5. **Given** CI dashboard, **When** viewing results, **Then** distributed test appears as single test with node breakdown

### Edge Cases

- What happens when one node loses network connectivity during test? → Node fails heartbeat, coordinator detects via timeout, test aborts gracefully with partial results from connected nodes, disconnected node saves PCAP locally for later retrieval
- How does system handle clock drift between nodes (gPTP failure)? → If clock sync is lost mid-test and drift exceeds acceptable threshold, test should be marked as unreliable or aborted
- What if node clocks are in different timezones (timestamp normalization)? → TimestampNormalizer converts all timestamps to UTC nanoseconds; timezone is irrelevant as Unix epoch is timezone-agnostic
- How are distributed deadlocks detected (all nodes waiting for each other)? → Barrier synchronization has configurable timeout (default 10s); if any node fails to arrive within timeout, barrier fails and test aborts
- What happens when test coordinator crashes mid-test? → Nodes detect coordinator failure via heartbeat timeout, abort test gracefully, save partial results and PCAP files for diagnostic analysis
- How does system handle asymmetric network delays (A→B fast, B→A slow)? → One-way latency measurement uses synchronized clocks; asymmetric delays are correctly measured as different values for A→B vs B→A assertions
- What if nodes have different Wadjet-Link versions (protocol compatibility)? → RegisterNode RPC includes protocol_version field; coordinator rejects nodes with incompatible versions (major version mismatch)

## Requirements

### Functional Requirements

#### Test Coordination

- **FR-001**: System MUST provide test coordinator service running on controller node
- **FR-002**: Coordinator MUST discover test nodes via static configuration file (YAML/JSON format with node addresses and identifiers)
- **FR-003**: Coordinator MUST synchronize test start across all nodes (barrier synchronization)
- **FR-004**: Coordinator MUST distribute test scenarios to nodes (scenario decomposition)
- **FR-005**: Coordinator MUST collect test results from all nodes
- **FR-006**: Coordinator MUST aggregate results into single test report
- **FR-007**: Coordinator MUST handle node failures gracefully (timeout, retry, abort)
- **FR-008**: Coordinator MUST support health checks (ping/pong heartbeat)
- **FR-009**: Nodes MUST detect coordinator failure via heartbeat timeout and abort test gracefully
- **FR-010**: Nodes MUST save partial results and PCAP files when aborting due to coordinator failure

#### Synchronized Capture

- **FR-011**: System MUST synchronize packet capture start across nodes (<10ms jitter)
- **FR-012**: System MUST query OS-synchronized clock for timestamps (assumes linuxptp/ptp4l/phc2sys is running externally for gPTP sync, target: <1µs accuracy)
- **FR-013**: System MUST fall back to NTP-synchronized system clock if gPTP is unavailable (target: ~1ms accuracy)
- **FR-014**: System MUST fail test initialization if neither gPTP nor NTP synchronization is detected on the system
- **FR-015**: System MUST use M8 gPTP decoder to verify clock sync health by decoding gPTP Announce/Sync messages (passive monitoring)
- **FR-016**: System MUST normalize timestamps to common reference (UTC)
- **FR-017**: System MUST merge PCAP files from multiple nodes into unified timeline
- **FR-018**: System MUST correlate packets across nodes by sequence numbers, IDs, or timestamps
- **FR-019**: System MUST visualize message flow across network topology

#### Distributed Assertions

- **FR-020**: System MUST provide distributed matchers: `ExpectMessageFlow(node_src, node_dst, matcher)`
- **FR-021**: System MUST provide latency assertions: `WithinLatency(duration)` measuring one-way latency from send timestamp on source node to receive timestamp on destination node (requires synchronized clocks)
- **FR-022**: System MUST provide causal ordering assertions: `HappensBefore(event_a, event_b)`
- **FR-023**: System MUST provide absence assertions: `MustNotSeeOn(node, matcher)`
- **FR-024**: System MUST evaluate distributed assertions across node boundaries
- **FR-025**: System MUST fail tests if distributed assertions are violated
- **FR-026**: System MUST provide detailed failure reports (which node, when, expected vs actual)

#### Scenario Orchestration

- **FR-027**: System MUST parse YAML/JSON distributed test scenarios
- **FR-028**: Scenarios MUST define node assignments (which matchers run on which nodes)
- **FR-029**: Scenarios MUST support sequential steps (barrier between steps)
- **FR-030**: Scenarios MUST support parallel steps (concurrent execution)
- **FR-031**: Scenarios MUST support timing constraints (delays, timeouts, deadlines)
- **FR-032**: Coordinator MUST deploy scenario steps to nodes at runtime
- **FR-033**: Coordinator MUST enforce scenario execution order
- **FR-034**: Coordinator MUST collect scenario execution logs from all nodes

#### Results Aggregation

- **FR-035**: System MUST generate JUnit XML with all node results aggregated
- **FR-036**: System MUST report which node executed which test case
- **FR-037**: System MUST include performance metrics (latency, throughput) per node
- **FR-038**: System MUST attach PCAP files from all nodes on test failure (primary forensic artifact)
- **FR-039**: System MUST provide distributed test dashboard (HTML report)
- **FR-040**: System MAY export distributed traces in OpenTelemetry format for coordination flow visualization (supplementary, not required for test reproducibility)

#### Network Communication

- **FR-041**: Nodes MUST communicate via gRPC (HTTP/2-based RPC with protobuf serialization)
- **FR-042**: Communication MUST be authenticated using TLS mutual authentication
- **FR-043**: All nodes MUST be on the same routable network (direct IP connectivity required)
- **FR-044**: System MUST minimize coordination overhead (<1% of test traffic)
- **FR-045**: System MUST handle network partitions (split-brain detection)

#### Scalability

- **FR-046**: System MUST support at least 10 distributed test nodes
- **FR-047**: System MUST scale to 1000 distributed assertions per test
- **FR-048**: Coordinator MUST handle parallel test execution (multiple scenarios concurrently)
- **FR-049**: [DEFERRED] System MAY support hierarchical coordination (coordinator delegates to sub-coordinators) - moved to future milestone per YAGNI principle

#### FFI Bindings (Constitution Principle V)

- **FR-050**: `libwadjet_distributed` MUST expose C99 ABI layer with opaque handles for all public types
- **FR-051**: Python bindings MUST be provided via pybind11 with type stubs (.pyi files)
- **FR-052**: Rust bindings MUST provide safe idiomatic wrappers over unsafe FFI
- **FR-053**: Python bindings MUST support: SyncBarrier, TimestampNormalizer, DistributedMatcher, MessageCorrelator
- **FR-054**: Python bindings MUST integrate with pytest for distributed test scenarios
- **FR-055**: Rust bindings MUST support: SyncBarrier, TimestampNormalizer, DistributedMatcher, MessageCorrelator
- **FR-056**: C ABI MUST use thread-local error storage for error message retrieval

#### Observability and Forensics (Constitution Principle VI)

- **FR-057**: Failed distributed tests MUST automatically save PCAP files from ALL participating nodes to `failure_captures/` directory
- **FR-058**: PCAP files MUST be named with test name, node ID, and timestamp for correlation
- **FR-059**: Coordinator MUST log all barrier synchronization events with timestamps for reproducibility
- **FR-060**: All distributed assertion failures MUST include: node ID, timestamp, expected value, actual value, and packet context
- **FR-061**: System MUST support replay mode where distributed test can be re-run using saved PCAP files from all nodes

### Key Entities

- **TestCoordinator**: Central controller for distributed test orchestration
- **TestNode**: Worker node running packet capture and matchers
- **DistributedScenario**: YAML/JSON definition of multi-node test
- **NodeDefinition**: Node configuration (id, address, interfaces) in scenario
- **DistributedAssertion**: Assertion spanning multiple nodes (message flow, latency, causality)
- **SynchronizationBarrier**: Coordination point where all nodes wait
- **AggregatedResult**: Combined test results from all nodes
- **TimestampNormalizer**: Converts node-local timestamps to global reference
- **MessageCorrelator**: Links packets across nodes by sequence/ID
- **NetworkTopology**: Graph of nodes and network segments

## Success Criteria

### Measurable Outcomes

- **SC-001**: Test coordinator successfully orchestrates 10 nodes with synchronized capture start (<10ms jitter)
- **SC-002**: Distributed assertions evaluate correctly across nodes (e.g., "message sent on A, received on B within 100ms")
- **SC-003**: Clock synchronization via gPTP achieves <1µs timestamp alignment between nodes
- **SC-004**: PCAP merging produces unified timeline showing cross-node message flow
- **SC-005**: Distributed test scenarios (YAML) execute correctly with sequential and parallel steps
- **SC-006**: Aggregated JUnit XML report includes all node results in CI-compatible format
- **SC-007**: System handles node failure gracefully (test aborts, partial results collected)
- **SC-008**: Distributed testing overhead is <1% of test execution time
- **SC-009**: Example distributed scenario validates SOME/IP service discovery across 3 nodes
- **SC-010**: Documentation includes distributed testing architecture diagram and setup guide
- **SC-011**: Python bindings enable distributed test scenarios via pytest with equivalent functionality to C++ API
- **SC-012**: Rust bindings compile and pass all integration tests for distributed primitives
- **SC-013**: C ABI layer has complete coverage for all public distributed testing types

## Clarifications

### Session 2026-02-03

- Q: The spec mentions "gRPC or similar RPC framework" and also references ZeroMQ. Which RPC framework should be used for node communication? → A: Use gRPC exclusively (HTTP/2-based, built-in TLS, protobuf serialization, streaming support)
- Q: The spec mentions multiple node discovery mechanisms: "mDNS, static config, or explicit registration". Which approach should be used? → A: Static configuration only (nodes configured via YAML/JSON file with addresses)
- Q: What happens when neither gPTP nor NTP is available, or when gPTP fails mid-test? → A: Try gPTP, fall back to NTP, fail if neither available (clear precision guarantees per mode)
- Q: What happens when test coordinator crashes mid-test? → A: Nodes detect coordinator failure, abort gracefully, save partial results/PCAPs for diagnostics
- Q: How is latency measured in `WithinLatency()` assertions - one-way or round-trip? → A: One-way latency (time from send on node A to receive on node B, requires synchronized clocks)
- Q: How does this feature align with Constitution Principle I (Library-First Architecture)? → A: Library-first: Build `libwadjet_distributed` with sync primitives (SyncBarrier, DistributedMatcher, TimestampNormalizer), then coordinator/node CLI tools consume the library
- Q: What FFI bindings are required for distributed testing per Constitution Principle V (Multi-Language FFI)? → A: Full FFI: C ABI + Python bindings + Rust bindings for all distributed testing primitives (M14 scope)
- Q: How should observability work - OpenTelemetry vs PCAP per Constitution Principle VI (Observability and Forensics)? → A: PCAP-first: PCAP files remain primary forensic artifact for reproducibility, OpenTelemetry traces for coordination flow visualization only
- Q: Should NAT traversal be supported for nodes on different networks per Constitution Principle VII (Simplicity/YAGNI)? → A: No NAT traversal: Require all nodes on same routable network, move NAT to Out of Scope (simpler, fits automotive lab use case)
- Q: How does Wadjet-Link integrate with gPTP for clock sync per Constitution Principle II (passive monitoring)? → A: Passive + system clock: Query OS-synchronized clock (via linuxptp/phc2sys), use M8 decoder to verify gPTP health by decoding messages (no active gPTP participation)

## Assumptions

- Nodes are on the same routable network with direct IP connectivity (same subnet, VPN, or routed lab network)
- For gPTP sync: linuxptp (ptp4l/phc2sys) is installed and running on each node, synchronizing system clock to gPTP grandmaster
- For NTP sync: chronyd or ntpd is configured and running on each node
- Test coordinator has sufficient resources to coordinate all nodes
- Nodes run compatible Wadjet-Link versions (protocol versioning)
- Network latency between nodes is <100ms (LAN or low-latency WAN)

## Dependencies

- **External**: gRPC for node communication, Protocol Buffers for serialization, gPTP for time sync, pybind11 for Python bindings
- **Internal**: M1 (packet capture on each node), M3 (matchers), M4 (scenario engine), M5 (Python bindings infrastructure), M7 (Rust bindings infrastructure), M8 (gPTP for sync)

## Out of Scope

- Cloud-based distributed testing (nodes in different data centers)
- Real-time coordination (hard real-time guarantees for test synchronization)
- Active network emulation (latency injection, packet loss simulation - use external tools like Linux tc)
- Distributed fuzzing (coordinated fuzz testing across nodes)
- Multi-platform distributed testing (Linux + Windows + macOS in same test)
- Blockchain-based consensus for test results (overkill for automotive use case)
- NAT traversal (nodes behind different NATs) - use VPN for cross-network scenarios
- Hierarchical coordination with sub-coordinators (FR-049 deferred to future milestone per YAGNI)

## Implementation Notes

### Constitution Compliance

**Principle I - Library-First Architecture**: All distributed testing primitives MUST be implemented as standalone libraries before building coordinator/node tools:

- `libwadjet_distributed` - Core synchronization primitives (SyncBarrier, TimestampNormalizer, MessageCorrelator)
- `libwadjet_distributed_matchers` - Distributed assertion framework (ExpectMessageFlow, WithinLatency, HappensBefore)
- `wadjet-coordinator` / `wadjet-node` - CLI tools built on top of the libraries

This ensures test engineers can use distributed primitives directly in custom C++ test harnesses without the gRPC coordinator.

### Recommended Approach

1. **Phase 1 - Core Libraries (Library-First)**
   - Implement `SyncBarrier` class for barrier synchronization (no network dependency)
   - Implement `TimestampNormalizer` for timestamp conversion (gPTP/NTP/UTC)
   - Implement `MessageCorrelator` for cross-node packet linking
   - Unit tests for all primitives (no network required)

2. **Phase 2 - Distributed Matchers Library**
   - Implement `DistributedMatcher` base class extending M3 matchers
   - Implement `ExpectMessageFlow`, `WithinLatency`, `HappensBefore`, `MustNotSeeOn`
   - Matchers work with in-memory packet collections (testable without network)
   - Integration with existing GoogleTest matchers from M3

3. **Phase 3 - gRPC Communication Layer**
   - Implement gRPC service definitions (proto files)
   - Build TestCoordinator gRPC server consuming `libwadjet_distributed`
   - Build TestNode gRPC client consuming `libwadjet_distributed`
   - Node discovery via static configuration (YAML/JSON parser)
   - Health checks and heartbeats

4. **Phase 4 - Scenario Orchestration**
   - Extend M4 YAML scenario parser for node assignments
   - Implement scenario decomposition (distribute to nodes)
   - Enforce sequential and parallel step execution
   - Timing constraint enforcement

5. **Phase 5 - Results Aggregation & Tools**
   - Collect results from all nodes
   - Generate aggregated JUnit XML (extend M4 report generation)
   - Attach PCAP files on failure
   - Build distributed test dashboard (HTML)
   - `wadjet-coordinator` and `wadjet-node` CLI tools

### Architecture

```text
┌─────────────────────┐
│  Test Coordinator   │  (controller node)
│  - Scenario parser  │
│  - Node discovery   │
│  - Result aggregator│
└──────────┬──────────┘
           │ gRPC
    ┌──────┴──────┬──────────┬──────────┐
    │             │          │          │
┌───▼────┐   ┌───▼────┐ ┌───▼────┐ ┌───▼────┐
│ Node A │   │ Node B │ │ Node C │ │ Node N │
│Capture │   │Capture │ │Capture │ │Capture │
│Matchers│   │Matchers│ │Matchers│ │Matchers│
└────────┘   └────────┘ └────────┘ └────────┘
  (ECU-1)     (Gateway)   (ECU-2)    (ECU-N)
```

### gRPC Service Definition

```protobuf
service DistributedTest {
  rpc RegisterNode(NodeInfo) returns (NodeId);
  rpc SyncBarrier(BarrierRequest) returns (BarrierResponse);
  rpc StartCapture(CaptureConfig) returns (Status);
  rpc EvaluateMatcher(MatcherRequest) returns (MatcherResult);
  rpc ReportResult(TestResult) returns (Ack);
}
```

### Distributed Scenario Example (YAML)

```yaml
name: "SOME/IP Service Discovery (3 nodes)"
nodes:
  - id: gateway
    address: 192.168.1.10
  - id: ecu_a
    address: 192.168.1.20
  - id: ecu_b
    address: 192.168.1.30

steps:
  - name: "Start synchronized capture"
    type: barrier
    nodes: [gateway, ecu_a, ecu_b]
  
  - name: "ECU-A advertises service"
    node: ecu_a
    matcher: SendsSomeIpServiceOffer(service_id=0x1234)
  
  - name: "Gateway sees advertisement"
    node: gateway
    assertion:
      expect_message_flow:
        from: ecu_a
        to: gateway
        within_latency: 10ms
  
  - name: "ECU-B discovers service"
    node: ecu_b
    matcher: ReceivesSomeIpServiceOffer(service_id=0x1234)
    within: 200ms
```

### Clock Synchronization Strategy

**Passive Monitoring Approach** (Constitution Principle II compliance):

1. **Primary**: Query OS-synchronized clock via `clock_gettime(CLOCK_REALTIME)` after linuxptp (ptp4l/phc2sys) synchronizes system clock to gPTP grandmaster (<1µs accuracy)
2. **Fallback**: Query NTP-synchronized system clock (~1ms accuracy)
3. **Health Verification**: Use M8 gPTP decoder to passively decode gPTP Announce/Sync messages and verify clock sync health (grandmaster identity, clock quality, path delay)
4. **Failure Mode**: Test initialization fails if system clock is not synchronized (detected via `adjtimex()` or chrony/ntpd status)
5. **Precision Reporting**: System reports detected sync method (gPTP/NTP) and estimated accuracy in test metadata

**Prerequisites**: Nodes must have linuxptp or chronyd configured externally before running distributed tests.

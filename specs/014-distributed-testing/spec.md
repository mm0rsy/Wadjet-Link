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
2. **Given** latency assertion, **When** specifying `WithinLatency(100ms)`, **Then** system fails if B receives after deadline
3. **Given** causal ordering assertion, **When** specifying `HappensBefore(event_a, event_b)`, **Then** system validates timestamp ordering
4. **Given** absence assertion, **When** specifying `NodeCMustNotSee(message)`, **Then** system fails if C captures the message
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

- What happens when one node loses network connectivity during test?
- How does system handle clock drift between nodes (gPTP failure)?
- What if node clocks are in different timezones (timestamp normalization)?
- How are distributed deadlocks detected (all nodes waiting for each other)?
- What happens when test coordinator crashes mid-test?
- How does system handle asymmetric network delays (A→B fast, B→A slow)?
- What if nodes have different Wadjet-Link versions (protocol compatibility)?

## Requirements

### Functional Requirements

#### Test Coordination

- **FR-001**: System MUST provide test coordinator service running on controller node
- **FR-002**: Coordinator MUST discover test nodes via network (mDNS, static config, or explicit registration)
- **FR-003**: Coordinator MUST synchronize test start across all nodes (barrier synchronization)
- **FR-004**: Coordinator MUST distribute test scenarios to nodes (scenario decomposition)
- **FR-005**: Coordinator MUST collect test results from all nodes
- **FR-006**: Coordinator MUST aggregate results into single test report
- **FR-007**: Coordinator MUST handle node failures gracefully (timeout, retry, abort)
- **FR-008**: Coordinator MUST support health checks (ping/pong heartbeat)

#### Synchronized Capture

- **FR-009**: System MUST synchronize packet capture start across nodes (<10ms jitter)
- **FR-010**: System MUST synchronize clocks using gPTP (IEEE 802.1AS) when available
- **FR-011**: System MUST fall back to NTP synchronization if gPTP unavailable
- **FR-012**: System MUST normalize timestamps to common reference (UTC or monotonic)
- **FR-013**: System MUST merge PCAP files from multiple nodes into unified timeline
- **FR-014**: System MUST correlate packets across nodes by sequence numbers, IDs, or timestamps
- **FR-015**: System MUST visualize message flow across network topology

#### Distributed Assertions

- **FR-016**: System MUST provide distributed matchers: `ExpectMessageFlow(node_src, node_dst, matcher)`
- **FR-017**: System MUST provide latency assertions: `WithinLatency(duration)`
- **FR-018**: System MUST provide causal ordering assertions: `HappensBefore(event_a, event_b)`
- **FR-019**: System MUST provide absence assertions: `MustNotSeeOn(node, matcher)`
- **FR-020**: System MUST evaluate distributed assertions across node boundaries
- **FR-021**: System MUST fail tests if distributed assertions are violated
- **FR-022**: System MUST provide detailed failure reports (which node, when, expected vs actual)

#### Scenario Orchestration

- **FR-023**: System MUST parse YAML/JSON distributed test scenarios
- **FR-024**: Scenarios MUST define node assignments (which matchers run on which nodes)
- **FR-025**: Scenarios MUST support sequential steps (barrier between steps)
- **FR-026**: Scenarios MUST support parallel steps (concurrent execution)
- **FR-027**: Scenarios MUST support timing constraints (delays, timeouts, deadlines)
- **FR-028**: Coordinator MUST deploy scenario steps to nodes at runtime
- **FR-029**: Coordinator MUST enforce scenario execution order
- **FR-030**: Coordinator MUST collect scenario execution logs from all nodes

#### Results Aggregation

- **FR-031**: System MUST generate JUnit XML with all node results aggregated
- **FR-032**: System MUST report which node executed which test case
- **FR-033**: System MUST include performance metrics (latency, throughput) per node
- **FR-034**: System MUST attach PCAP files from all nodes on test failure
- **FR-035**: System MUST provide distributed test dashboard (HTML report)
- **FR-036**: System MUST export distributed traces (OpenTelemetry format for visualization)

#### Network Communication

- **FR-037**: Nodes MUST communicate via gRPC or similar RPC framework
- **FR-038**: Communication MUST be authenticated (TLS mutual auth or shared secret)
- **FR-039**: Communication MUST support NAT traversal (if nodes are on different networks)
- **FR-040**: System MUST minimize coordination overhead (<1% of test traffic)
- **FR-041**: System MUST handle network partitions (split-brain detection)

#### Scalability

- **FR-042**: System MUST support at least 10 distributed test nodes
- **FR-043**: System MUST scale to 1000 distributed assertions per test
- **FR-044**: Coordinator MUST handle parallel test execution (multiple scenarios)
- **FR-045**: System MUST support hierarchical coordination (coordinator delegates to sub-coordinators)

### Key Entities

- **TestCoordinator**: Central controller for distributed test orchestration
- **TestNode**: Worker node running packet capture and matchers
- **DistributedScenario**: YAML/JSON definition of multi-node test
- **NodeAssignment**: Mapping of matchers to specific nodes
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

## Assumptions

- Nodes are network-reachable (same subnet or routed network)
- gPTP is available for high-precision time synchronization (fallback to NTP)
- Test coordinator has sufficient resources to coordinate all nodes
- Nodes run compatible Wadjet-Link versions (protocol versioning)
- Network latency between nodes is <100ms (LAN or low-latency WAN)

## Dependencies

- **External**: gRPC or ZeroMQ for node communication, gPTP for time sync, mDNS for discovery
- **Internal**: M1 (packet capture on each node), M3 (matchers), M4 (scenario engine), M8 (gPTP for sync)

## Out of Scope

- Cloud-based distributed testing (nodes in different data centers)
- Real-time coordination (hard real-time guarantees for test synchronization)
- Active network emulation (latency injection, packet loss simulation - use external tools like Linux tc)
- Distributed fuzzing (coordinated fuzz testing across nodes)
- Multi-platform distributed testing (Linux + Windows + macOS in same test)
- Blockchain-based consensus for test results (overkill for automotive use case)

## Implementation Notes

### Recommended Approach

1. **Phase 1 - Coordinator Foundation**
   - Implement TestCoordinator service (gRPC server)
   - Implement TestNode agent (gRPC client)
   - Node discovery (mDNS or static config)
   - Health checks and heartbeats

2. **Phase 2 - Synchronized Capture**
   - Implement barrier synchronization (all nodes wait at barrier)
   - Integrate gPTP clock sync
   - Timestamp normalization (node-local → global)
   - PCAP merging from multiple nodes

3. **Phase 3 - Distributed Assertions**
   - Implement distributed matchers (ExpectMessageFlow, WithinLatency)
   - Build message correlator (link packets across nodes)
   - Implement causal ordering validation
   - Failure reporting with multi-node context

4. **Phase 4 - Scenario Orchestration**
   - Extend YAML scenario parser for node assignments
   - Implement scenario decomposition (distribute to nodes)
   - Enforce sequential and parallel step execution
   - Timing constraint enforcement

5. **Phase 5 - Results Aggregation**
   - Collect results from all nodes
   - Generate aggregated JUnit XML
   - Attach PCAP files on failure
   - Build distributed test dashboard (HTML)

### Architecture

```
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

1. **Preferred**: gPTP (M8) for <1µs sync
2. **Fallback**: NTP for ~1ms sync
3. **Last Resort**: Manual offset calibration (ping timestamps)

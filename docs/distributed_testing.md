# Distributed Testing Architecture Guide

T133: Comprehensive guide to Wadjet-Link distributed testing framework and architecture

## Overview

Wadjet-Link's distributed testing framework enables **multi-node packet capture and validation** for automotive Ethernet protocols across multiple network segments with:

- **Synchronized Capture**: All nodes capture simultaneously with <10ms jitter
- **Timestamp Alignment**: gPTP or NTP-synchronized timestamps (±1µs with gPTP)
- **Distributed Assertions**: Validate message flow and latency across nodes
- **Automatic Result Aggregation**: Single JUnit XML report from multi-node tests
- **Declarative Scenarios**: YAML-based test definitions without code

## System Architecture

### High-Level Components

```
┌─────────────────────────────────────────────────────────────┐
│                    Test Coordinator                         │
│  - Orchestrates test execution                              │
│  - Monitors node health                                     │
│  - Aggregates results                                       │
│  - Generates reports (JUnit, HTML, JSON)                    │
└──────────┬────────────────────────────────────┬─────────────┘
           │ gRPC                               │ gRPC
           │ Control                            │ Control
    ┌──────▼──────┐  ┌──────────┐  ┌──────────▼─────┐
    │ Test Node 1 │  │Test Node│  │  Test Node 3   │
    │ (Provider)  │  │ 2 (Cust)│  │  (Monitor)     │
    │             │  │         │  │                │
    │ eth0: SD    │  │eth1: SD │  │eth0,1: Monitor │
    │ Capture ←───┴──┴─→ +SD   │  │  +  Capture ←──┘
    │ Filter      │  │         │  │
    │ gPTP Sync   │  │gPTP Sync│  │gPTP Sync
    └─────────────┘  └─────────┘  └────────────────┘
         │                │                │
         └────────────────┴────────────────┘
                   Network
                (Ethernet)
```

### Component Responsibilities

**Test Coordinator**
- Listens for node registration on gRPC port
- Distributes test scenarios to nodes
- Synchronizes capture via barriers
- Collects per-node results
- Merges PCAPs with timestamp alignment
- Generates aggregated reports

**Test Node**
- Connects to coordinator
- Verifies clock synchronization
- Executes capture steps
- Uploads PCAP to coordinator
- Reports results

## Key Concepts

### Synchronization Barrier

A **barrier** is a rendezvous point where N nodes wait until all arrive:

```
Timeline:
  Node1:  ├─────► [Barrier 1] ─────►
  Node2:     ├──► [Barrier 1] ─────►
  Node3:       ├─► [Barrier 1] ─────►
               └─ Jitter: <10ms
```

**Types:**
- **Capture Start**: Synchronize capture initiation
- **Capture Stop**: Coordinate capture completion
- **Test Steps**: Separate test phases

### Clock Synchronization

Distributed testing requires **clock alignment** for:
1. Correlating messages across nodes
2. Calculating one-way latency
3. Validating timing constraints
4. Reconstructing event ordering

**Methods:**
- **gPTP (IEEE 802.1AS)**: <1µs accuracy (preferred)
- **NTP**: <1ms accuracy (fallback)
- **None**: No sync (limited validation)

### Message Correlation

When a packet appears on multiple nodes, it's the **same packet** if:
1. **Payload match**: Same packet content
2. **Timestamp proximity**: Within sync tolerance
3. **Sequence correlation**: Message sequence numbers align

## Test Scenario Structure

### YAML Format

```yaml
# Metadata
scenario_id: test_id
name: "Test Name"
version: "1.0"

# Nodes participating
nodes:
  - node_id: provider
    address: "192.168.1.10"
  - node_id: consumer
    address: "192.168.1.20"

# Test steps (executed sequentially)
steps:
  # Step 1: Synchronize
  - type: barrier
    config:
      expected_participants: 2

  # Step 2: Verify clocks
  - type: command
    config:
      command: "check_clock"

  # Step 3: Capture
  - type: capture
    config:
      duration_ms: 10000
      filter: "udp port 30490"

  # Step 4: Validate
  - type: expect
    config:
      matchers:
        - type: "ExpectMessageFlow"
        - type: "WithinLatency"

  # Step 5: Stop
  - type: barrier
    config:
      barrier_id: "capture_stop"

# Results configuration
results:
  junit_report: "results.xml"
  pcap_merge: true
```

## Distributed Matchers

Matchers validate behavior **across multiple nodes**:

### ExpectMessageFlow

Validates that a message travels from source to destination:

```cpp
ExpectMessageFlow()
    .From("provider")
    .To("consumer")
    .Via("monitor")           // Optional intermediate
    .WithPayload(...)         // Optional filter
    .Timeout(100ms)
```

### WithinLatency

Validates request-response latency:

```cpp
WithinLatency(std::chrono::milliseconds(100))
    .Request("find_service")
    .Response("offer_service")
```

### HappensBefore

Validates causal ordering:

```cpp
HappensBefore()
    .Event("service_advertise")
    .Before("find_service_request")
    .OnNodes({node1, node2, node3})
```

### MustNotSeeOn

Validates absence (negative assertion):

```cpp
MustNotSeeOn("monitor")
    .Packets("unicast_traffic")
    .Duration(10s)
```

## Execution Flow

### 1. Initialization

```
User: wadjet-coordinator --config nodes.yaml --scenario test.yaml
                ↓
Coordinator: Create config, bind gRPC
Coordinator: Load scenario from YAML
Coordinator: Listen for node connections
```

### 2. Node Registration

```
User: wadjet-node --node-id node1 --check-clock
                ↓
Node: Connect to coordinator
Node: Verify clock sync (gPTP/NTP)
Node: Register with coordinator
Coordinator: Track node in active list
```

### 3. Scenario Execution

```
Coordinator: Distribute scenario to all nodes
                ↓
Step 1: Barrier sync (wait for all nodes)
Step 2: Start synchronized capture (all at T0)
Step 3: Let test run
Step 4: Stop capture (all at T0+duration)
Step 5: Collect results
```

### 4. Result Aggregation

```
Node1: Upload PCAP → Coordinator
Node2: Upload PCAP → Coordinator
Node3: Upload PCAP → Coordinator
                ↓
Coordinator: Merge PCAPs by timestamp
Coordinator: Evaluate assertions
Coordinator: Generate reports:
             - results.xml (JUnit)
             - report.html (HTML)
             - merged.pcap (Merged PCAP)
             - results.json (Machine-readable)
```

## Performance Characteristics

### Latency

- **Barrier synchronization**: <50ms
- **Capture start jitter**: <10ms
- **Capture stop jitter**: <10ms
- **gRPC RPC latency**: <5ms (local network)

### Throughput

- **Packet capture**: 100k+ packets/sec per node
- **PCAP upload**: Limited by network (GigE ~125 MB/s)
- **Timestamp alignment**: Sub-microsecond with gPTP

### Coordination Overhead

- **Target**: <1% of test traffic
- **Typical**: 0.1-0.5% (heartbeats + sync)

## Error Handling

### Node Failure During Test

**Detection:**
- Coordinator monitors heartbeats (default: 5s timeout)
- Detects disconnection after heartbeat miss

**Response:**
- If `enable_partial_results: true`: Continue with remaining nodes
- If `enable_partial_results: false`: Abort test
- Save partial PCAP to failure directory

**Example:**
```yaml
coordinator:
  heartbeat_timeout_ms: 5000
  enable_partial_results: true  # Continue with 2 of 3 nodes
```

### Clock Synchronization Failure

**Detection:**
```bash
wadjet-node --node-id node1 --check-clock
# Outputs: Clock status: NOT SYNCHRONIZED
```

**Options:**
1. Warn but proceed (limited assertions)
2. Fail test startup
3. Use fallback (NTP if gPTP unavailable)

## Integration with CI/CD

### Jenkins

```groovy
stage('Distributed Tests') {
    steps {
        sh '''
            wadjet-coordinator \\
                --config nodes.yaml \\
                --scenario test.yaml \\
                --junit-report results.xml
        '''
    }
    post {
        always {
            junit 'results.xml'
        }
    }
}
```

### GitHub Actions

```yaml
- name: Run Distributed Tests
  run: |
    wadjet-coordinator \\
      --config nodes.yaml \\
      --scenario test.yaml \\
      --junit-report results.xml

- name: Publish Results
  uses: EnricoMi/publish-unit-test-result-action@v2
  with:
    files: results.xml
```

## Troubleshooting

### Nodes Fail to Connect

**Symptom:** "Failed to connect to coordinator"

**Causes:**
- Incorrect coordinator address
- Firewall blocking gRPC port
- Network unreachable

**Solution:**
```bash
# Verify connectivity
nc -zv 192.168.1.100 50051

# Check firewall
sudo ufw allow 50051/tcp

# Update address in nodes.yaml
coordinator:
  address: "192.168.1.100"
```

### High Capture Jitter

**Symptom:** "Capture jitter: 45ms (exceeds 10ms target)"

**Causes:**
- CPU overload
- High network latency
- gPTP not synchronized

**Solution:**
```bash
# Verify gPTP
phc_ctl /dev/ptp0 get
# Expected: offset within ±100ns

# Reduce system load
renice -n 19 -p $$

# Disable other services
systemctl stop irqbalance
```

### Timestamp Misalignment

**Symptom:** "Merged PCAP has inconsistent timestamps"

**Causes:**
- Nodes using different sync methods
- gPTP grandmaster disappeared
- NTP drift exceeds tolerance

**Solution:**
```yaml
# Verify all nodes use same sync method
nodes:
  - node_id: node1
    clock:
      sync_method: "gptp"  # Consistent across all
```

### PCAP Merge Fails

**Symptom:** "Failed to merge PCAPs: timestamp ordering invalid"

**Causes:**
- Nodes captured on different network segments
- Packet loss causing gaps
- Incorrect timestamp correlation

**Solution:**
```yaml
# Check capture filters are correct
nodes:
  - node_id: monitor
    capture:
      filter: ""  # Monitor captures ALL traffic
```

## Advanced Topics

### Custom Matchers

```cpp
// Implement custom distributed matcher
class MyCustomMatcher : public DistributedMatcher {
    Result<DistributedMatchResult> match(
        const DistributedCaptureContext& context) override {
        // Custom validation logic
    }
};
```

### Parallel Scenario Execution

```yaml
# Execute multiple scenarios in parallel
scenarios:
  - someip_discovery.yaml
  - someip_communication.yaml
  - someip_method_calls.yaml
```

### Network Partition Handling

```cpp
// Detect and handle split-brain scenarios
coordinator->on_partition(
    [](const std::vector<NodeId>& majority,
       const std::vector<NodeId>& minority) {
        // Majority continues, minority stops
        // Prevents divergent results
    });
```

## References

- [Quick Start Guide](quickstart.md#distributed-testing)
- [Example: SOME/IP Discovery](../examples/distributed_someip_discovery.cpp)
- [Scenario Format](scenarios.md)
- [API Reference](generated/html/index.html)

---

*𓆓 Wadjet-Link — Multi-node distributed testing for automotive Ethernet protocols*

# Quickstart: Distributed Testing

**Feature**: 014-distributed-testing  
**Date**: 2026-02-03

## Overview

This guide demonstrates setting up and running distributed tests across multiple nodes using Wadjet-Link's distributed testing infrastructure.

## Prerequisites

- **Clock Synchronization**: All nodes must be synchronized via NTP or gPTP (Precision Time Protocol). Verify with:
  ```bash
  # Check NTP sync
  timedatectl status | grep "NTP synchronized"
  
  # Or check gPTP sync (linuxptp)
  pmc -u -b 0 'GET PORT_DATA_SET' | grep portState
  ```

- **Network**: Nodes must be reachable on the configured gRPC port (default: 50051)

- **TLS Certificates**: Generate certificates for mutual TLS authentication (see [Certificate Setup](#tls-certificate-setup))

## Installation

```bash
# Build with distributed testing support
cmake -B build -DWADJET_ENABLE_DISTRIBUTED=ON
cmake --build build

# Install coordinator and node binaries
sudo cmake --install build
```

## Quick Example: 3-Node SOME/IP Discovery Test

### 1. Node Configuration (nodes.yaml)

Create a configuration file listing all participating nodes:

```yaml
# nodes.yaml
coordinator:
  address: "192.168.1.100:50051"
  tls:
    cert: "/etc/wadjet/certs/coordinator.crt"
    key: "/etc/wadjet/certs/coordinator.key"
    ca: "/etc/wadjet/certs/ca.crt"

nodes:
  - id: "ecu-a"
    address: "192.168.1.101:50051"
    interfaces: ["eth0"]
    role: "sender"
    
  - id: "ecu-b"
    address: "192.168.1.102:50051"
    interfaces: ["eth0"]
    role: "receiver"
    
  - id: "observer"
    address: "192.168.1.103:50051"
    interfaces: ["eth0", "eth1"]
    role: "observer"
```

### 2. Scenario Definition (someip_discovery.yaml)

```yaml
# someip_discovery.yaml
name: "SOME/IP Service Discovery Validation"
description: |
  Validates that SOME/IP service discovery messages propagate correctly
  across a 3-node network within timing requirements.

tags: ["someip", "discovery", "distributed"]

# Reference node configuration
nodes_config: "nodes.yaml"

steps:
  # Synchronize all nodes before starting
  - name: "Sync all nodes"
    type: barrier
    barrier_id: "start"
    nodes: ["ecu-a", "ecu-b", "observer"]
    timeout_ms: 5000
    
  # Start capture on all nodes simultaneously
  - name: "Start capture on ECU-A"
    type: capture
    node: "ecu-a"
    interface: "eth0"
    filter: "udp port 30490"
    
  - name: "Start capture on ECU-B"
    type: capture
    node: "ecu-b"
    interface: "eth0"
    filter: "udp port 30490"
    
  - name: "Start capture on Observer"
    type: capture
    node: "observer"
    interface: "eth0"
    filter: "udp port 30490"
    
  # Wait for barrier to ensure captures are ready
  - name: "Captures ready"
    type: barrier
    barrier_id: "captures_started"
    nodes: ["ecu-a", "ecu-b", "observer"]
    
  # ECU-A sends SOME/IP OfferService
  - name: "ECU-A sends OfferService"
    type: log
    node: "ecu-a"
    message: "Sending SOME/IP OfferService for ServiceID=0x1234"
    
  # Wait for message propagation
  - name: "Wait for propagation"
    type: wait
    duration_ms: 100
    
  # Distributed assertions
  - name: "Verify OfferService received at ECU-B"
    type: expect
    src_node: "ecu-a"
    dst_node: "ecu-b"
    matcher:
      type: "expect_message_flow"
      protocol: "someip"
      message_type: "OfferService"
      service_id: 0x1234
    max_latency_us: 5000  # 5ms max one-way latency
    
  - name: "Verify OfferService seen by Observer"
    type: expect
    src_node: "ecu-a"
    dst_node: "observer"
    matcher:
      type: "expect_message_flow"
      protocol: "someip"
      message_type: "OfferService"
      service_id: 0x1234
    max_latency_us: 5000
    
  # Stop captures
  - name: "Stop all captures"
    type: barrier
    barrier_id: "stop_capture"
    nodes: ["ecu-a", "ecu-b", "observer"]
```

### 3. Running the Test

#### Start Nodes First

On each test node (ECU-A, ECU-B, Observer):

```bash
# On ecu-a (192.168.1.101)
wadjet-node --config nodes.yaml --node-id ecu-a

# On ecu-b (192.168.1.102)
wadjet-node --config nodes.yaml --node-id ecu-b

# On observer (192.168.1.103)
wadjet-node --config nodes.yaml --node-id observer
```

#### Start Coordinator and Run Scenario

On the coordinator machine:

```bash
# Run the distributed test
wadjet-coordinator run \
  --config nodes.yaml \
  --scenario someip_discovery.yaml \
  --output-dir ./results \
  --junit-report results/junit.xml

# Or via the wadjet-run tool
wadjet-run distributed \
  --nodes nodes.yaml \
  --scenario someip_discovery.yaml \
  --verbose
```

### 4. Expected Output

```
[COORDINATOR] Connecting to nodes...
[COORDINATOR] ✓ ecu-a connected (clock: gPTP synchronized)
[COORDINATOR] ✓ ecu-b connected (clock: gPTP synchronized)
[COORDINATOR] ✓ observer connected (clock: gPTP synchronized)

[COORDINATOR] Running scenario: SOME/IP Service Discovery Validation

[STEP 1/9] Sync all nodes (barrier: start)
  [ecu-a] arrived at barrier
  [ecu-b] arrived at barrier
  [observer] arrived at barrier
  ✓ All nodes synchronized at 1675500000123456789 ns

[STEP 2-4/9] Starting captures...
  [ecu-a] capture started on eth0
  [ecu-b] capture started on eth0
  [observer] capture started on eth0

[STEP 5/9] Captures ready (barrier)
  ✓ All captures confirmed

[STEP 6/9] ECU-A sends OfferService
  [ecu-a] INFO: Sending SOME/IP OfferService for ServiceID=0x1234

[STEP 7/9] Wait for propagation (100ms)

[STEP 8/9] Verify OfferService received at ECU-B
  [ecu-a → ecu-b] SOME/IP OfferService
    Source timestamp:  1675500000123500000 ns
    Dest timestamp:    1675500000123502500 ns
    One-way latency:   2.5 μs
  ✓ PASSED (latency < 5000 μs)

[STEP 9/9] Verify OfferService seen by Observer
  [ecu-a → observer] SOME/IP OfferService
    Source timestamp:  1675500000123500000 ns
    Dest timestamp:    1675500000123501800 ns
    One-way latency:   1.8 μs
  ✓ PASSED (latency < 5000 μs)

═══════════════════════════════════════════════════════════
SCENARIO RESULT: PASSED
═══════════════════════════════════════════════════════════
Duration: 1.23s
Assertions: 2 passed, 0 failed
PCAP files collected: 3
  - results/ecu-a_capture.pcap
  - results/ecu-b_capture.pcap
  - results/observer_capture.pcap
Merged PCAP: results/merged_timeline.pcap
JUnit report: results/junit.xml
```

## C++ API Usage

### Coordinator Setup

```cpp
#include <wadjet/distributed/coordinator.hpp>
#include <wadjet/distributed/scenario.hpp>

int main() {
    using namespace wadjet::distributed;
    
    // Configure coordinator
    CoordinatorConfig config{
        .config_path = "nodes.yaml",
        .tls_cert_path = "/etc/wadjet/certs/coordinator.crt",
        .tls_key_path = "/etc/wadjet/certs/coordinator.key",
        .tls_ca_path = "/etc/wadjet/certs/ca.crt",
        .listen_port = 50051
    };
    
    // Create coordinator
    auto coordinator = TestCoordinator::create(config);
    if (!coordinator) {
        std::cerr << "Failed to create coordinator: " << coordinator.error() << "\n";
        return 1;
    }
    
    // Load and run scenario
    auto scenario_id = coordinator->load_scenario("someip_discovery.yaml");
    auto result = coordinator->run_scenario(*scenario_id);
    
    // Export results
    coordinator->export_junit(*result, "results/junit.xml");
    
    return result->status == ResultStatus::Passed ? 0 : 1;
}
```

### Node Setup

```cpp
#include <wadjet/distributed/node.hpp>

int main() {
    using namespace wadjet::distributed;
    
    NodeConfig config{
        .node_id = "ecu-a",
        .coordinator_address = "192.168.1.100:50051",
        .tls_cert_path = "/etc/wadjet/certs/ecu-a.crt",
        .tls_key_path = "/etc/wadjet/certs/ecu-a.key",
        .tls_ca_path = "/etc/wadjet/certs/ca.crt",
        .capture_interfaces = {"eth0"}
    };
    
    auto node = TestNode::create(config);
    if (!node) {
        std::cerr << "Failed to create node: " << node.error() << "\n";
        return 1;
    }
    
    // Connect and run until shutdown
    node->connect();
    
    // Block until coordinator signals shutdown
    // (node handles all commands via gRPC streaming)
    node->wait_for_shutdown();
    
    return 0;
}
```

### Custom Distributed Matcher

```cpp
#include <wadjet/distributed/matcher.hpp>
#include <wadjet/matchers/someip_matchers.hpp>

using namespace wadjet::distributed;
using namespace wadjet::matchers;

// Create a distributed test that verifies message flow
void test_someip_discovery() {
    auto scenario = DistributedScenario::parse("someip_discovery.yaml").value();
    
    // Add programmatic assertion
    auto matcher = ExpectMessageFlow(
        "ecu-a",   // source node
        "ecu-b",   // destination node
        AllOf(
            IsSomeIp(),
            HasMessageType(SomeIpMessageType::Notification),
            HasServiceId(0x1234)
        )
    );
    
    // Wrap with latency constraint
    auto timed_matcher = WithinLatency(
        std::move(matcher),
        std::chrono::microseconds{5000}
    );
    
    // Evaluate after capture
    auto result = timed_matcher->evaluate(scenario.capture_contexts());
    
    ASSERT_TRUE(result.matched);
    EXPECT_LT(result.latency_ns, 5'000'000);  // 5ms
}
```

## Python Bindings

```python
import wadjet.distributed as wd

# Configure and create coordinator
config = wd.CoordinatorConfig(
    config_path="nodes.yaml",
    tls_cert_path="/etc/wadjet/certs/coordinator.crt",
    tls_key_path="/etc/wadjet/certs/coordinator.key",
    tls_ca_path="/etc/wadjet/certs/ca.crt"
)

with wd.TestCoordinator(config) as coordinator:
    # Wait for nodes to connect
    coordinator.wait_for_nodes(["ecu-a", "ecu-b", "observer"], timeout_sec=30)
    
    # Load and run scenario
    result = coordinator.run_scenario("someip_discovery.yaml")
    
    # Check results
    assert result.status == wd.ResultStatus.PASSED
    
    # Access individual assertions
    for assertion in result.distributed_assertions:
        print(f"{assertion.src_node} → {assertion.dst_node}: "
              f"latency={assertion.latency_ns / 1000:.2f} μs")
    
    # Export results
    coordinator.export_junit(result, "results/junit.xml")
```

## TLS Certificate Setup

Generate certificates for secure node communication:

```bash
#!/bin/bash
# generate-certs.sh

CERT_DIR="/etc/wadjet/certs"
mkdir -p $CERT_DIR

# Generate CA
openssl genrsa -out $CERT_DIR/ca.key 4096
openssl req -x509 -new -nodes -key $CERT_DIR/ca.key \
  -sha256 -days 365 -out $CERT_DIR/ca.crt \
  -subj "/CN=Wadjet Test CA"

# Generate coordinator certificate
openssl genrsa -out $CERT_DIR/coordinator.key 2048
openssl req -new -key $CERT_DIR/coordinator.key \
  -out $CERT_DIR/coordinator.csr \
  -subj "/CN=coordinator"
openssl x509 -req -in $CERT_DIR/coordinator.csr \
  -CA $CERT_DIR/ca.crt -CAkey $CERT_DIR/ca.key \
  -CAcreateserial -out $CERT_DIR/coordinator.crt \
  -days 365 -sha256

# Generate node certificates (repeat for each node)
for NODE in ecu-a ecu-b observer; do
  openssl genrsa -out $CERT_DIR/$NODE.key 2048
  openssl req -new -key $CERT_DIR/$NODE.key \
    -out $CERT_DIR/$NODE.csr \
    -subj "/CN=$NODE"
  openssl x509 -req -in $CERT_DIR/$NODE.csr \
    -CA $CERT_DIR/ca.crt -CAkey $CERT_DIR/ca.key \
    -CAcreateserial -out $CERT_DIR/$NODE.crt \
    -days 365 -sha256
done

# Distribute to nodes
# scp $CERT_DIR/{ca.crt,$NODE.crt,$NODE.key} user@$NODE_IP:/etc/wadjet/certs/
```

## Troubleshooting

### Clock Synchronization Issues

```bash
# Check if clock is synchronized
cat /sys/devices/system/clocksource/clocksource0/current_clocksource
# Should show "tsc" or similar high-resolution source

# Verify NTP/gPTP sync status via wadjet
wadjet-node --check-clock
# Output: Clock synchronized via gPTP, max error: 50ns
```

### Node Connection Failures

```bash
# Verify connectivity
grpcurl -cacert ca.crt -cert node.crt -key node.key \
  coordinator:50051 list

# Check firewall
sudo iptables -L -n | grep 50051
```

### Barrier Timeout

If barriers timeout frequently:

1. Increase `timeout_ms` in scenario
2. Check network latency between nodes
3. Verify all nodes are running before starting coordinator

## Next Steps

- [Distributed Matchers Reference](./matchers.md)
- [Scenario YAML Schema](./scenario-schema.md)
- [Clock Synchronization Guide](./clock-sync.md)
- [CI/CD Integration](./ci-integration.md)

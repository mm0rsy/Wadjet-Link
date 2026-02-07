# 𓆓 Wadjet-Link Quick Start Guide

Welcome to Wadjet-Link! This guide will help you get started with automotive Ethernet protocol analysis.

## Table of Contents

- [Installation](#installation)
- [Your First Capture](#your-first-capture)
- [Decoding Protocols](#decoding-protocols)
- [Writing Tests with GoogleTest](#writing-tests-with-googletest)
- [Distributed Testing](#distributed-testing)
- [Using PCAP Files](#using-pcap-files)
- [Next Steps](#next-steps)

---

## Installation

### Prerequisites

- Linux (Ubuntu 20.04+, Debian 11+, or similar)
- CMake 3.20+
- GCC 10+ or Clang 12+ (C++20 support)
- libpcap-dev
- GoogleTest (fetched automatically)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-org/wadjet-link.git
cd wadjet-link

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Run tests to verify
ctest --output-on-failure

# Install (optional)
sudo make install
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `WADJET_BUILD_TESTS` | ON | Build unit tests |
| `WADJET_BUILD_EXAMPLES` | ON | Build examples |
| `WADJET_BUILD_PYTHON_BINDINGS` | OFF | Build Python bindings |
| `WADJET_BUILD_DOCS` | OFF | Build Doxygen documentation |
| `WADJET_ENABLE_SANITIZERS` | ON | Enable ASan/UBSan in Debug |

---

## Your First Capture

### Live Packet Capture

```cpp
#include <wadjet/io/capture_session.hpp>
#include <iostream>

int main() {
    using namespace wadjet::io;
    
    // Configure capture options
    CaptureSessionOptions options;
    options.promiscuous = true;
    options.snaplen = 65535;
    options.timeout_ms = 1000;
    
    // Create capture session on eth0
    auto session = CaptureSession::create("eth0", options);
    
    // Set BPF filter (optional)
    session->set_filter("udp port 30490");  // SOME/IP default port
    
    // Start capturing
    session->start();
    
    // Capture 10 packets
    for (int i = 0; i < 10; ++i) {
        auto packet = session->next_packet();
        if (packet) {
            std::cout << "Captured packet: " << packet->size() << " bytes\n";
        }
    }
    
    // Stop capturing
    session->stop();
    
    return 0;
}
```

### Reading from PCAP Files

```cpp
#include <wadjet/pcap/pcap_reader.hpp>
#include <iostream>

int main() {
    using namespace wadjet::pcap;
    
    // Open PCAP file
    PcapReader reader("capture.pcap");
    
    // Read all packets
    int count = 0;
    while (auto packet = reader.read()) {
        std::cout << "Packet " << ++count 
                  << ": " << packet->size() << " bytes\n";
    }
    
    std::cout << "Total: " << count << " packets\n";
    return 0;
}
```

---

## Decoding Protocols

### Using the Protocol Dispatcher

```cpp
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <iostream>

int main() {
    using namespace wadjet;
    
    // Create dispatcher (decodes full protocol stack)
    protocols::ProtocolDispatcher dispatcher;
    
    // Read from PCAP
    pcap::PcapReader reader("someip_traffic.pcap");
    
    while (auto packet = reader.read()) {
        // Decode the packet
        auto result = dispatcher.decode(packet->view());
        
        // Check for SOME/IP
        if (result.has_someip()) {
            const auto& someip = result.someip();
            std::cout << "SOME/IP: Service=0x" 
                      << std::hex << someip.service_id()
                      << " Method=0x" << someip.method_id()
                      << std::dec << "\n";
        }
        
        // Check for DoIP
        if (result.has_doip()) {
            const auto& doip = result.doip();
            std::cout << "DoIP: Type=0x" 
                      << std::hex << static_cast<int>(doip.payload_type())
                      << std::dec << "\n";
        }
        
        // Check for gPTP (IEEE 802.1AS)
        if (result.has_gptp()) {
            const auto& gptp = result.gptp();
            std::cout << "gPTP: Type=" 
                      << wadjet::protocols::gptp::to_string(gptp.message_type)
                      << " Domain=" << static_cast<int>(gptp.domain_number)
                      << "\n";
        }
    }
    
    return 0;
}
```

### Direct Header Parsing

```cpp
#include <wadjet/protocols/someip/someip_header.hpp>
#include <wadjet/protocols/ethernet/ethernet_header.hpp>

// Parse Ethernet header
auto eth_result = wadjet::protocols::ethernet::EthernetHeader::parse(data);
if (eth_result) {
    std::cout << "Src MAC: " << eth_result->src_mac().to_string() << "\n";
    std::cout << "Dst MAC: " << eth_result->dst_mac().to_string() << "\n";
}

// Parse SOME/IP header (after extracting from UDP payload)
auto someip_result = wadjet::protocols::someip::SomeIpHeader::parse(payload);
if (someip_result) {
    std::cout << "Service ID: 0x" << std::hex << someip_result->service_id() << "\n";
    std::cout << "Method ID: 0x" << someip_result->method_id() << std::dec << "\n";
}
```

---

## Writing Tests with GoogleTest

### Basic Packet Matching

```cpp
#include <wadjet/testing/matchers.hpp>
#include <gtest/gtest.h>

TEST(SomeIPTest, ServiceIdMatches) {
    // Load test packet
    auto packets = load_pcap("test_fixtures/someip_request.pcap");
    ASSERT_FALSE(packets.empty());
    
    auto& packet = packets[0];
    
    // Use matchers
    EXPECT_THAT(packet, wadjet::testing::HasSOMEIPServiceId(0x1234));
    EXPECT_THAT(packet, wadjet::testing::HasSOMEIPMethodId(0x0001));
    EXPECT_THAT(packet, wadjet::testing::IsSOMEIPRequest());
}

TEST(DoIPTest, RoutingActivation) {
    auto packets = load_pcap("test_fixtures/doip_routing.pcap");
    
    EXPECT_THAT(packets[0], wadjet::testing::IsDoIPRoutingActivationRequest());
    EXPECT_THAT(packets[1], wadjet::testing::IsDoIPRoutingActivationResponse());
}

TEST(GptpTest, SyncMessage) {
    auto packets = load_pcap("test_fixtures/gptp_sync.pcap");
    ASSERT_FALSE(packets.empty());
    
    auto& packet = packets[0];
    
    // Use gPTP matchers
    EXPECT_THAT(packet, wadjet::testing::IsGptp());
    EXPECT_THAT(packet, wadjet::testing::IsGptpSync());
    EXPECT_THAT(packet, wadjet::testing::HasGptpDomain(0));
    EXPECT_THAT(packet, wadjet::testing::IsGptpEventMessage());
    EXPECT_THAT(packet, wadjet::testing::IsGptpTwoStep());
}
```

### Live Capture Test Fixture

```cpp
#include <wadjet/testing/live_capture_fixture.hpp>
#include <gtest/gtest.h>

class SomeIPServiceTest : public wadjet::testing::LiveCaptureTestFixture {
protected:
    void SetUp() override {
        // Configure capture
        set_interface("eth0");
        set_filter("udp port 30490");
        set_save_on_failure(true);
        
        LiveCaptureTestFixture::SetUp();
    }
};

TEST_F(SomeIPServiceTest, ServiceOffersWithin100ms) {
    // Wait for SOME/IP Service Discovery offer
    auto packet = wait_for_match(
        wadjet::testing::HasSOMEIPServiceId(0x1234),
        std::chrono::milliseconds(100)
    );
    
    ASSERT_TRUE(packet.has_value()) << "No service offer received";
    
    // Verify it's a notification
    EXPECT_THAT(*packet, wadjet::testing::IsSOMEIPNotification());
}

TEST_F(SomeIPServiceTest, CollectMultipleResponses) {
    // Collect packets for 500ms
    auto packets = collect_packets(std::chrono::milliseconds(500));
    
    // Count SOME/IP responses
    int response_count = count_matching(
        packets, 
        wadjet::testing::IsSOMEIPResponse()
    );
    
    EXPECT_GE(response_count, 5) << "Expected at least 5 responses";
}
```

### Loopback Testing

```cpp
#include <wadjet/testing/loopback_fixture.hpp>
#include <gtest/gtest.h>

class LoopbackTest : public wadjet::testing::LoopbackTestFixture {
protected:
    void SetUp() override {
        set_interface("lo");
        LoopbackTestFixture::SetUp();
    }
};

TEST_F(LoopbackTest, SendAndReceiveUDP) {
    // Send UDP packet
    send_udp(12345, {0x01, 0x02, 0x03, 0x04});
    
    // Wait for it
    auto packet = wait_for_match(
        wadjet::testing::HasDestPort(12345),
        std::chrono::milliseconds(100)
    );
    
    ASSERT_TRUE(packet.has_value());
    EXPECT_THAT(*packet, wadjet::testing::PayloadContains({0x01, 0x02}));
}
```

---

## Distributed Testing

T132: Wadjet-Link supports multi-node distributed testing for validating protocols across multiple network segments with synchronized capture and timestamp alignment.

### Key Concepts

**Distributed Test Architecture:**
- **Coordinator**: Central test orchestrator managing multiple nodes
- **Nodes**: Individual test participants capturing and validating packets
- **Barriers**: Synchronization points for coordinating test steps
- **Clock Sync**: gPTP or NTP for timestamp alignment
- **Results Aggregation**: Merged PCAPs and aggregated assertions

### Quick Start: 3-Node Test

**1. Prepare Configuration Files**

Create `nodes.yaml` for your nodes:
```yaml
coordinator:
  bind_address: "0.0.0.0"
  grpc_port: 50051

nodes:
  - node_id: node1
    interfaces:
      - name: eth0
    capture:
      enabled: true
      filter: "udp port 30490"
    clock:
      sync_method: "gptp"
  
  - node_id: node2
    interfaces:
      - name: eth1
    capture:
      enabled: true
      filter: "udp port 30490"
    clock:
      sync_method: "gptp"
```

**2. Create Test Scenario**

Create `scenario.yaml` defining test steps:
```yaml
scenario_id: example_test
name: "Multi-Node Protocol Test"
nodes:
  - node_id: node1
  - node_id: node2

steps:
  - step_id: 1
    type: barrier
    description: "Synchronize nodes"
    config:
      expected_participants: 2
      timeout_ms: 5000

  - step_id: 2
    type: capture
    description: "Capture for 5 seconds"
    config:
      duration_ms: 5000

  - step_id: 3
    type: expect
    description: "Validate message flow"
    config:
      matchers:
        - type: "ExpectMessageFlow"
          description: "Message reaches both nodes"
```

**3. Start the Coordinator**

```bash
# Terminal 1: Start coordinator with scenario
wadjet-coordinator \
  --config nodes.yaml \
  --scenario scenario.yaml \
  --output-dir ./results \
  --junit-report results.xml
```

**4. Start Test Nodes**

```bash
# Terminal 2: Start node 1
wadjet-node --node-id node1 --check-clock

# Terminal 3: Start node 2
wadjet-node --node-id node2 --check-clock
```

**5. Review Results**

```bash
# Check results when done
cat ./results/results.xml          # JUnit format
open ./results/report.html         # Human-readable report
file ./results/merged_*.pcap       # Aligned PCAP
```

### Advanced Features

**Clock Synchronization Verification:**
```bash
# Node validates gPTP/NTP before starting tests
wadjet-node --node-id node1 --check-clock
```

**Per-Node Capture Control:**
```cpp
// C++ API for custom scenarios
auto config = CaptureConfig{
    .duration = std::chrono::seconds(10),
    .filter = "udp port 30490",
    .synchronization_barrier_id = "capture_start"
};
node->start_capture(config);
```

**Distributed Assertions:**
```cpp
// Validate message flow across nodes
EXPECT_THAT(packets,
    ExpectMessageFlow()
        .From(node1).To(node2)
        .WithinLatency(std::chrono::milliseconds(100))
        .HappensBefore(event_a, event_b));
```

### Common Workflows

**SOME/IP Service Discovery Test:**
- See `examples/distributed_someip_discovery.cpp`
- Configuration: `examples/scenarios/someip_discovery.yaml`
- Nodes: `examples/scenarios/nodes.yaml`

**Custom Protocol Validation:**
1. Define nodes in `nodes.yaml`
2. Create scenario in `scenario.yaml`
3. Run coordinator with `wadjet-coordinator`
4. Start nodes with `wadjet-node`
5. Review merged PCAP and reports

### Troubleshooting Distributed Tests

| Issue | Solution |
| --- | --- |
| Nodes fail to connect | Check coordinator address, firewall, gRPC port |
| High capture jitter | Ensure gPTP is synchronized, reduce system load |
| Timestamp misalignment | Verify NTP/gPTP on all nodes, check `--check-clock` |
| Coordinator timeout | Increase `heartbeat_timeout_ms` in nodes.yaml |
| PCAP merge issues | Verify all nodes captured on same interfaces |

For detailed information, see [Distributed Testing Guide](distributed_testing.md).

---

## Using PCAP Files

### Writing Packets

```cpp
#include <wadjet/pcap/pcap_writer.hpp>

// Create writer
wadjet::pcap::PcapWriter writer("output.pcap");

// Write packets
for (const auto& packet : captured_packets) {
    writer.write(packet);
}
```

### Filtering and Processing

```cpp
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/pcap/pcap_writer.hpp>
#include <wadjet/protocols/dispatcher.hpp>

// Filter SOME/IP packets to new file
wadjet::pcap::PcapReader reader("all_traffic.pcap");
wadjet::pcap::PcapWriter writer("someip_only.pcap");
wadjet::protocols::ProtocolDispatcher dispatcher;

while (auto packet = reader.read()) {
    auto result = dispatcher.decode(packet->view());
    if (result.has_someip()) {
        writer.write(*packet);
    }
}
```

---

## Troubleshooting Common Decode Errors

### 1. `DecodeError::ChecksumMismatch`

- **Cause**: UDP/IPv4 checksum validation failed.
- **Solution**:
  - Ensure the packet source supports checksums.
  - Set `UdpDecoder::Options::validate_checksum = false` for warning-only mode.
  - Check if hardware offloading is interfering with software validation.

### 2. `DecodeError::IncompleteMessage` (SOME/IP-TP)

- **Cause**: Reassembly timeout or missing segments.
- **Solution**:
  - Increase `SomeIpTpConfig::timeout` (default 5s) if operating on high-latency networks.
  - Verify that the sender is correctly setting the "More Segments" flag.
  - Ensure the MTU is consistent across the network.

### 3. `DecodeError::InvalidState` (TCP)

- **Cause**: Out-of-order segment or stale connection.
- **Solution**:
  - The tracker buffers up to 16 out-of-order segments. Increase `TcpConnectionTracker::MAX_OUT_OF_ORDER` if needed.
  - Reset the tracker if the physical link was disconnected.

### 4. `DecodeError::InvalidLength` (SOME/IP-SD)

- **Cause**: Entry array count doesn't match remaining payload.
- **Solution**:
  - Verify if the SD message follows AUTOSAR 4.4+ standards.
  - Check for extra padding at the end of the UDP payload (Wadjet-Link expects tight packing).

---

## Next Steps

Now that you have the basics:

1. **Explore the Examples**: See `examples/` for complete working examples
2. **Read the API Docs**: Run `make docs` and open `docs/generated/html/index.html`
3. **Try YAML Scenarios**: Use `wadjet-run` for declarative testing
4. **Python Bindings**: See `bindings/python/README.md` for Python usage

### Useful Links

- [Architecture Overview](architecture.md)
- [API Reference](generated/html/index.html) (after building docs)
- [Coding Guidelines](CODING_GUIDELINES.md)
- [YAML Scenario Format](scenarios.md)

---

*𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.*

# 𓆓 Wadjet-Link Quick Start Guide

Welcome to Wadjet-Link! This guide will help you get started with automotive Ethernet protocol analysis.

## Table of Contents

- [Installation](#installation)
- [Your First Capture](#your-first-capture)
- [Decoding Protocols](#decoding-protocols)
- [Writing Tests with GoogleTest](#writing-tests-with-googletest)
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

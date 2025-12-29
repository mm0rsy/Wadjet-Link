# 𓆓 Wadjet-Link

### *Restoring the complete picture of the automotive stream.*

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Protocol: Automotive Ethernet](https://img.shields.io/badge/Protocol-100/1000Base--T1-orange)](https://standards.ieee.org/standard/802_3bw-2015.html)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)

<p align="center">
  <img src="identity-photo.jpg" alt="Wadjet-Link Logo" width="300"/>
</p>

---

## Overview

**Wadjet-Link** is an open-source Automotive Ethernet validation framework for live packet capture, protocol decoding, and automated testing with GoogleTest.

Like the ancient Egyptian "All-Seeing Eye," Wadjet-Link observes and reconstructs the complete truth of automotive network traffic — without interfering with the stream.

## Key Features

- **Zero-Latency Capture** — High-fidelity sniffing of 100/1000Base-T1 traffic
- **Protocol Decoding** — SOME/IP, DoIP, gPTP, UDS over IP
- **GoogleTest Integration** — Assert on live traffic with custom matchers
- **Passive Monitoring** — Read-only mode, no impact on functional safety (ASIL)
- **Forensic Logging** — Automatic pcap storage on test failures

## Quick Example

```cpp
TEST_F(SOMEIPServiceDiscovery, ServiceOffersAppearWithin100ms) {
    auto stream = LiveCapture("eth0").filter("udp port 30490");
    auto found = stream.wait_for_someip_service(0x1234, 100ms);
    ASSERT_TRUE(found);
}
```

## Getting Started

### Prerequisites

- Linux (Ubuntu 22.04+ recommended)
- CMake 3.20+
- C++20 compiler (GCC 11+ or Clang 14+)
- libpcap

### Build

```bash
git clone https://github.com/your-repo/wadjet-link.git
cd wadjet-link
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Documentation

See [plan.md](plan.md) for the full implementation roadmap.

## License

Apache 2.0 — See [LICENSE](LICENSE) for details.

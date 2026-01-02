# 𓆓 Wadjet-Link

### *Restoring the complete picture of the automotive stream.*

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Protocol: Automotive Ethernet](https://img.shields.io/badge/Protocol-100/1000Base--T1-orange)](https://standards.ieee.org/standard/802_3bw-2015.html)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Build](https://img.shields.io/badge/build-CMake-green)](https://cmake.org/)

<p align="center">
  <img src="identity-photo.jpg" alt="Wadjet-Link Logo" width="300"/>
</p>

---

## Overview

**Wadjet-Link** is an open-source Automotive Ethernet validation framework for live packet capture, protocol decoding, and automated testing with GoogleTest.

Like the ancient Egyptian "All-Seeing Eye," Wadjet-Link observes and reconstructs the complete truth of automotive network traffic — without interfering with the stream.

## Key Features

### 🔍 Packet Capture & Analysis
- **Zero-Latency Capture** — High-fidelity sniffing of 100/1000Base-T1 traffic using AF_PACKET
- **Zero-Copy Parsing** — Efficient packet inspection without data duplication
- **BPF Filtering** — Hardware-accelerated packet filtering
- **PCAP Support** — Read and write standard pcap/pcapng files
- **Hardware Timestamping** — Nanosecond precision when available

### 📡 Protocol Decoding
- **Ethernet** — 802.1Q VLAN and QinQ support
- **IPv4** — Header parsing with checksum validation
- **UDP/TCP** — Full header parsing with TCP options
- **SOME/IP** — Service-oriented middleware protocol
- **SOME/IP-SD** — Service Discovery messages
- **DoIP** — Diagnostics over IP (ISO 13400)
- **UDS** — Unified Diagnostic Services (ISO 14229)
- **gPTP** — Generalized Precision Time Protocol (IEEE 802.1AS)
- **DDS/RTPS** — Data Distribution Service Real-Time Publish-Subscribe

### 🧪 Testing Framework
- **GoogleTest Integration** — Assert on live traffic with custom matchers
- **gMock-Style Matchers** — `HasSOMEIPServiceId()`, `HasDoIPPayloadType()`, etc.
- **Property-Based Testing** — Random packet generators with reproducible seeds
- **Record-Then-Assert** — Capture traffic, then analyze offline
- **Live-Assert Mode** — Real-time assertion checking during capture
- **Forensic Logging** — Automatic pcap storage on test failures

### 🤖 Test Automation
- **YAML/JSON Scenarios** — Define test cases in human-readable format
- **CLI Runner** — `wadjet-run` command-line tool
- **Multiple Report Formats** — JUnit XML, JSON, TAP, Text
- **Tag-Based Filtering** — Run subsets of tests (smoke, regression, etc.)
- **CI/CD Ready** — Jenkins, GitLab CI, GitHub Actions integration

### 🛡️ Safety & Compliance
- **Passive Monitoring** — Read-only mode, no impact on functional safety (ASIL)
- **Deterministic Replay** — Reproduce issues from saved captures
- **Comprehensive Logging** — Full audit trail for compliance

### 🌐 Language Bindings
- **C** — C99 compatible API for FFI integration ([bindings/c/](bindings/c/))
- **Python** — pybind11-based bindings with pytest support ([bindings/python/](bindings/python/))
- **Rust** — Safe idiomatic wrapper via bindgen FFI ([bindings/rust/](bindings/rust/))

## Use Cases

| Use Case | Description |
|----------|-------------|
| **ECU Integration Testing** | Validate SOME/IP service discovery and communication between ECUs |
| **DoIP Diagnostics Validation** | Test vehicle diagnostics protocols and routing activation |
| **Protocol Conformance** | Verify implementation against AUTOSAR specifications |
| **Regression Testing** | Automated test suites with PCAP fixtures for CI/CD |
| **Network Traffic Analysis** | Capture and decode automotive Ethernet traffic |
| **Issue Reproduction** | Replay saved captures to reproduce timing-sensitive bugs |
| **Performance Monitoring** | Measure service discovery times and response latencies |

## Quick Examples

### C++ GoogleTest Integration

```cpp
#include <wadjet/wadjet.hpp>
using namespace wadjet;

TEST_F(SOMEIPServiceDiscovery, ServiceOffersAppearWithin100ms) {
    auto session = io::CaptureSession::create("eth0");
    ASSERT_TRUE(session);
    
    session->set_filter("udp port 30490");
    session->start();
    
    auto found = wait_for_someip_service(*session, 0x1234, 100ms);
    ASSERT_TRUE(found);
}
```

### YAML Test Scenario

```yaml
# someip_service_test.yaml
name: SOME/IP Service Discovery Test
description: Verify service offers appear within timeout
tags: [smoke, someip]

steps:
  - capture:
      interface: eth0
      filter: "udp port 30490"

  - expect:
      description: "Service 0x1234 should be offered"
      timeout_ms: 250
      count: ">= 1"
      someip:
        service_id: 0x1234
        message_type: notification
```

### CLI Scenario Runner

```bash
# Run a single test scenario
wadjet-run test.yaml

# Run all scenarios in a directory with JUnit output
wadjet-run --dir scenarios/ -f junit -o results.xml

# Run only smoke tests
wadjet-run --dir scenarios/ -t smoke

# Dry-run to validate scenarios without execution
wadjet-run --dry-run --dir scenarios/

# List available scenarios
wadjet-run --list --dir scenarios/
```

---

## System Requirements

### Operating System

| OS           | Version       | Status        |
| ------------ | ------------- | ------------- |
| Ubuntu       | 22.04+        | ✅ Supported   |
| Ubuntu       | 24.04 LTS     | ✅ Recommended |
| Debian       | 12+           | ✅ Supported   |
| Fedora       | 38+           | ✅ Supported   |
| Arch Linux   | Rolling       | ✅ Supported   |
| Windows/macOS| Any           | ❌ Not supported (Linux-only) |

### Build Dependencies

| Dependency      | Minimum Version | Purpose                           |
| --------------- | --------------- | --------------------------------- |
| CMake           | 3.20+           | Build system                      |
| GCC             | 11+             | C++20 compiler                    |
| Clang           | 14+             | Alternative C++20 compiler        |
| libpcap-dev     | 1.10+           | BPF filter compilation            |
| pkg-config      | Any             | Library discovery                 |
| Git             | Any             | Fetching GoogleTest               |

### Runtime Dependencies

| Dependency        | Purpose                                |
| ----------------- | -------------------------------------- |
| Linux Kernel 4.4+ | AF_PACKET socket support               |
| CAP_NET_RAW       | Packet capture capability (or root)    |
| libpcap           | Runtime BPF filter support             |

---

## Installation

### Ubuntu/Debian

```bash
# Install build dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libpcap-dev \
    git

# Optional: Install clang for alternative compiler
sudo apt-get install -y clang

# Optional: Install documentation tools
sudo apt-get install -y doxygen graphviz plantuml
```

### Fedora

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkgconfig \
    libpcap-devel \
    git
```

### Arch Linux

```bash
sudo pacman -S \
    base-devel \
    cmake \
    pkgconf \
    libpcap \
    git
```

---

## Building

### Basic Build

```bash
git clone https://github.com/your-repo/wadjet-link.git
cd wadjet-link

# Configure (Debug build)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Release Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Build with Clang

```bash
CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Build Options

| Option                  | Default | Description                        |
| ----------------------- | ------- | ---------------------------------- |
| `CMAKE_BUILD_TYPE`      | Debug   | Debug, Release, RelWithDebInfo     |
| `WADJET_ENABLE_TESTS`   | ON      | Build unit tests                   |
| `WADJET_ENABLE_ASAN`    | ON*     | Address sanitizer (* Debug only)   |
| `WADJET_ENABLE_UBSAN`   | ON*     | Undefined behavior sanitizer       |

---

## Running

### Capture Permissions

Live packet capture requires elevated privileges:

```bash
# Option 1: Run as root (not recommended for development)
sudo ./build/tests/wadjet_tests

# Option 2: Grant CAP_NET_RAW capability (recommended)
sudo setcap cap_net_raw+ep ./build/tests/wadjet_tests
./build/tests/wadjet_tests

# Option 3: Grant capability to user for development
sudo setcap cap_net_raw+ep /usr/bin/your_app
```

### Unit Tests

```bash
# Run all unit tests
ctest --test-dir build

# Run with verbose output
ctest --test-dir build --output-on-failure

# Run specific test
./build/tests/wadjet_tests --gtest_filter="PacketViewTest.*"
```

### Integration Tests

Integration tests include live capture tests that require elevated privileges:

```bash
# Run integration tests (requires sudo for live capture)
sudo ./build/tests/wadjet_integration_tests

# Run unit tests only (no sudo required)
./build/tests/wadjet_tests
```

---

## Test Coverage

| Test Suite | Tests | Description |
|------------|-------|-------------|
| Unit Tests | 111 | Core, Net, PCAP, I/O, Protocol decoders |
| Testing Framework | 158 | gMock matchers, Live capture fixtures, Generators, Record-Replay, Live-Assert |
| Scenario Tests | 41 | YAML/JSON parsers, Runner, Report generators |
| Integration Tests | 26 | Decode pipeline, PCAP roundtrip, Live capture |
| **Total** | **378** | |

### Advanced Testing Features

**Property-Based Testing Generators** (`generators.hpp`)

- Random packet generation with seed-based reproducibility
- Fluent builders: `EthernetBuilder`, `IPv4Builder`, `UDPBuilder`, `TCPBuilder`, `SOMEIPBuilder`, `DoIPBuilder`
- Factory class: `PacketGenerator` for random protocol packet creation

**Record-Then-Assert Mode** (`record_replay.hpp`)

- `RecordedStream` for offline packet analysis with functional filtering
- `RecordSession` for live capture recording
- Stream operations: `filter()`, `slice()`, `has_sequence()`, `all_match()`, `any_match()`
- PCAP save/load support

**Live-Assert Mode** (`live_assert.hpp`)

- `LiveAssertSession` for real-time assertion checking
- Rule types: `ASSERT_ALL`, `ASSERT_NEVER`, `ASSERT_WHEN`, `EXPECT_WITHIN`
- Conditional assertions: `when(condition).assert_that(assertion)`
- Time-bounded execution: `run_for()`, `run_until()`
- Automatic failure PCAP storage

---

## Project Structure

```text
wadjet-link/
├── include/wadjet/          # Public headers
│   ├── core/                # Core types and utilities
│   ├── net/                 # Packet classes
│   ├── pcap/                # PCAP file I/O
│   ├── io/                  # Live capture
│   ├── protocols/           # Protocol decoders (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP, UDS, gPTP, DDS)
│   ├── testing/             # GoogleTest fixtures, matchers, generators
│   └── scenario/            # YAML/JSON scenario types and runner
├── src/                     # Implementation
├── tools/                   # CLI tools (wadjet-run)
├── tests/                   # Test suites
│   ├── core/                # Core unit tests
│   ├── net/                 # Packet unit tests
│   ├── pcap/                # PCAP unit tests
│   ├── io/                  # I/O unit tests
│   ├── protocols/           # Protocol decoder unit tests
│   ├── testing/             # Testing framework tests
│   ├── scenario/            # Scenario parser and runner tests
│   └── integration/         # Integration & E2E tests
├── bindings/                # Language bindings
│   ├── c/                   # C API (libwadjet_c)
│   ├── python/              # Python bindings (pybind11)
│   └── rust/                # Rust crates (wadjet-sys, wadjet)
├── examples/
│   └── scenarios/           # Example YAML/JSON test scenarios
├── architecture/            # PlantUML diagrams
├── docs/                    # Documentation
├── pcap_samples/            # Test fixtures
└── .github/workflows/       # CI/CD
```

## Documentation

| Document                         | Description                       |
| -------------------------------- | --------------------------------- |
| [plan.md](plan.md)               | Implementation roadmap            |
| [architecture/](architecture/)   | PlantUML architecture diagrams    |
| [docs/CODING_GUIDELINES.md](docs/CODING_GUIDELINES.md) | Code style guide |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines         |
| [bindings/c/README.md](bindings/c/README.md) | C bindings documentation |
| [bindings/python/README.md](bindings/python/README.md) | Python bindings documentation |
| [bindings/rust/README.md](bindings/rust/README.md) | Rust bindings documentation |

---

## Architecture

See [architecture/README.md](architecture/README.md) for detailed PlantUML diagrams including:

- Component overview
- Layer architecture
- Use case sequences
- Module class diagrams
- Detailed sequence diagrams

---

## License

Apache 2.0 — See [LICENSE](LICENSE) for details.

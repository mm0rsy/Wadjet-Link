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

- **Zero-Latency Capture** — High-fidelity sniffing of 100/1000Base-T1 traffic using AF_PACKET
- **Protocol Decoding** — Ethernet, IPv4, UDP, TCP, SOME/IP, SOME/IP-SD, DoIP
- **GoogleTest Integration** — Assert on live traffic with custom matchers
- **Passive Monitoring** — Read-only mode, no impact on functional safety (ASIL)
- **Forensic Logging** — Automatic pcap storage on test failures
- **Zero-Copy Parsing** — Efficient packet inspection without data duplication

## Quick Example

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
# Run all tests
ctest --test-dir build

# Run with verbose output
ctest --test-dir build --output-on-failure

# Run specific test
./build/tests/wadjet_tests --gtest_filter="PacketViewTest.*"
```

---

## Project Structure

```
wadjet-link/
├── include/wadjet/          # Public headers
│   ├── core/                # Core types and utilities
│   ├── net/                 # Packet classes
│   ├── pcap/                # PCAP file I/O
│   ├── io/                  # Live capture
│   └── protocols/           # Protocol decoders (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP)
├── src/                     # Implementation
├── tests/                   # Unit tests
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

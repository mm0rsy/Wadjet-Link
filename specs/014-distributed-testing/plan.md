# Implementation Plan: Distributed Testing Infrastructure

**Branch**: `014-distributed-testing` | **Date**: 2026-02-03 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/014-distributed-testing/spec.md`

## Summary

Implement a distributed testing infrastructure enabling multi-node test coordination, synchronized packet capture across ECUs, and distributed assertions for validating automotive network protocols. Built as library-first architecture (`libwadjet_distributed`) with gRPC-based coordinator/node communication, integrating with existing M1 capture, M3 matchers, M4 scenario engine, and M8 gPTP decoder.

## Technical Context

**Language/Version**: C++20 (ISO/IEC 14882:2020), GCC 13+ / Clang 17+  
**Primary Dependencies**: gRPC 1.50+, Protocol Buffers 3.x, yaml-cpp (existing), nlohmann_json (existing), GoogleTest (existing), pybind11 (existing)  
**Storage**: PCAP files for capture storage, YAML/JSON for configuration and scenarios  
**Testing**: GoogleTest for unit/integration tests, pytest for Python binding tests, cargo test for Rust bindings  
**Target Platform**: Linux (AF_PACKET, linuxptp integration)  
**Project Type**: Single project with library-first architecture  
**Performance Goals**: <10ms capture start jitter, <1% coordination overhead, support 10 nodes with 1000 assertions  
**Constraints**: Passive monitoring only (no active gPTP participation), same routable network required (no NAT)  
**Scale/Scope**: 10 distributed nodes, 1000 assertions per test, 3-node example scenario

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Evidence |
|-----------|--------|----------|
| I. Library-First Architecture | ✅ PASS | `libwadjet_distributed` and `libwadjet_distributed_matchers` implemented before CLI tools |
| II. Zero-Copy, Zero-Latency | ✅ PASS | Uses existing M1 PacketView for zero-copy access; passive clock monitoring via M8 |
| III. Test-First Development | ✅ PASS | Unit tests for all primitives (SyncBarrier, TimestampNormalizer, etc.) before implementation |
| IV. Pluggable Protocol Architecture | ✅ PASS | DistributedMatcher extends existing M3 matcher interface |
| V. Multi-Language FFI Support | ✅ PASS | C ABI + Python (pybind11) + Rust bindings for all distributed primitives |
| VI. Observability and Forensics | ✅ PASS | PCAP-first approach; auto-save from all nodes on failure; replay mode support |
| VII. Simplicity and YAGNI | ✅ PASS | NAT traversal moved to Out of Scope; static config instead of mDNS; no active gPTP |

**Complexity Justification Required**:

| Addition | Why Needed | Simpler Alternative Rejected Because |
|----------|------------|-------------------------------------|
| gRPC dependency | Node communication requires streaming, TLS mutual auth, protobuf serialization | Raw TCP would require reimplementing connection handling, auth, serialization |
| Protocol Buffers | Efficient binary serialization for distributed messages | JSON would add latency and parsing overhead for high-frequency coordination |

## Project Structure

### Documentation (this feature)

```text
specs/014-distributed-testing/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (gRPC proto files)
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
include/wadjet/
├── distributed/                    # NEW: Distributed testing headers
│   ├── distributed.hpp             # Main include header
│   ├── sync_barrier.hpp            # Barrier synchronization primitive
│   ├── timestamp_normalizer.hpp    # Timestamp conversion utilities
│   ├── message_correlator.hpp      # Cross-node packet correlation
│   ├── distributed_matcher.hpp     # Base class for distributed matchers
│   ├── matchers/                   # Distributed matcher implementations
│   │   ├── expect_message_flow.hpp
│   │   ├── within_latency.hpp
│   │   ├── happens_before.hpp
│   │   └── must_not_see_on.hpp
│   ├── coordinator.hpp             # TestCoordinator interface
│   ├── node.hpp                    # TestNode interface
│   └── grpc/                       # gRPC service interfaces
│       ├── service.hpp
│       └── client.hpp
├── testing/                        # EXISTING: Extended
│   └── distributed_fixture.hpp     # NEW: GoogleTest fixture for distributed tests

src/
├── distributed/                    # NEW: Distributed testing implementation
│   ├── CMakeLists.txt
│   ├── sync_barrier.cpp
│   ├── timestamp_normalizer.cpp
│   ├── message_correlator.cpp
│   ├── distributed_matcher.cpp
│   ├── matchers/
│   │   ├── expect_message_flow.cpp
│   │   ├── within_latency.cpp
│   │   ├── happens_before.cpp
│   │   └── must_not_see_on.cpp
│   ├── coordinator.cpp
│   ├── node.cpp
│   └── grpc/
│       ├── service.cpp
│       └── client.cpp

tests/
├── distributed/                    # NEW: Distributed testing tests
│   ├── CMakeLists.txt
│   ├── test_sync_barrier.cpp
│   ├── test_timestamp_normalizer.cpp
│   ├── test_message_correlator.cpp
│   ├── test_distributed_matchers.cpp
│   ├── test_coordinator.cpp
│   └── test_node.cpp
├── integration/
│   └── test_distributed_scenario.cpp  # NEW: End-to-end distributed test

tools/
├── wadjet-coordinator.cpp          # NEW: Coordinator CLI tool
└── wadjet-node.cpp                 # NEW: Node agent CLI tool

bindings/
├── c/
│   └── wadjet_distributed.h        # NEW: C ABI for distributed primitives
├── python/
│   └── wadjet/
│       └── distributed/            # NEW: Python bindings
│           ├── __init__.py
│           ├── _distributed.pyi    # Type stubs
│           └── bindings.cpp        # pybind11 bindings
└── rust/
    └── wadjet-distributed/         # NEW: Rust crate
        ├── Cargo.toml
        ├── src/
        │   └── lib.rs
        └── tests/

proto/                              # NEW: gRPC protocol definitions
├── distributed_test.proto
└── CMakeLists.txt

examples/
└── distributed_someip_discovery.cpp  # NEW: 3-node SOME/IP SD example
```

**Structure Decision**: Library-first single project structure extending existing Wadjet-Link layout. New `distributed/` module follows existing pattern (io/, net/, protocols/, scenario/, testing/). gRPC proto files in dedicated `proto/` directory.

**Library Clarification (D1)**: Single `libwadjet_distributed` library target containing both core primitives (SyncBarrier, TimestampNormalizer, MessageCorrelator) and distributed matchers (ExpectMessageFlow, WithinLatency, HappensBefore, MustNotSeeOn). The spec mentions `libwadjet_distributed_matchers` conceptually but implementation uses single library for simplicity per YAGNI.

**Phase Mapping (I3)**: Spec "Recommended Approach" has 5 conceptual phases; tasks.md expands to 12 implementation phases for granular tracking:
- Spec Phase 1 (Core Libraries) → Tasks Phase 2 (Foundational)
- Spec Phase 2 (Distributed Matchers) → Tasks Phase 5 (US3 Assertions)
- Spec Phase 3 (gRPC Layer) → Tasks Phases 3-4 (US1, US2)
- Spec Phase 4 (Scenario Orchestration) → Tasks Phase 6 (US4)
- Spec Phase 5 (Results & Tools) → Tasks Phases 7, 9, 10 (US5, CLI, Examples)

---

## Phase 0: Research

### Research Tasks

Based on Technical Context unknowns and dependencies:

1. **gRPC C++ Integration** - Best practices for gRPC in CMake projects, async vs sync API, TLS mutual auth setup
2. **Protocol Buffers Schema Design** - Message design for distributed testing coordination, streaming patterns
3. **Clock Synchronization Detection** - How to detect if system clock is gPTP/NTP synchronized via `adjtimex()` or chrony/ntpd status
4. **Barrier Synchronization Patterns** - Distributed barrier algorithms, handling node failures during barrier wait
5. **PCAP Merge Algorithms** - Merging multiple PCAP files by timestamp, handling clock drift
6. **Existing M3 Matcher Extension** - How to extend `PacketMatcher` for distributed assertions

### Research Output

See [research.md](research.md) for detailed findings.

---

## Phase 1: Design

### Data Model

See [data-model.md](data-model.md) for entity definitions.

### API Contracts

See [contracts/](contracts/) directory for:
- `distributed_test.proto` - gRPC service definition
- C++ header contracts for library interfaces

### Quickstart Guide

See [quickstart.md](quickstart.md) for usage examples.

---

## Phase 2: Task Breakdown

*Generated by `/speckit.tasks` command after Phase 1 completion.*

See [tasks.md](tasks.md) for implementation tasks.

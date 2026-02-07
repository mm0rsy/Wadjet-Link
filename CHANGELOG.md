# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added - M14 Distributed Testing (NEW!)

#### Multi-Node Test Coordination (Phase 8-10)
- **TestCoordinator** — Central orchestrator managing 100+ nodes
  - gRPC-based node registration and lifecycle
  - Heartbeat-based health monitoring (<5s timeout)
  - Graceful abort with partial result collection
  - Node failure detection and recovery
  
- **TestNode** — Individual test participants
  - Automatic coordinator connection and heartbeat
  - Per-node packet capture with BPF filtering
  - Synchronized capture start/stop (<10ms jitter)
  - Clock sync status verification (gPTP/NTP)

- **SyncBarrier** — Distributed synchronization primitive
  - <10ms jitter barrier synchronization
  - Timeout-based failure detection
  - Per-participant tracking and validation
  - gRPC streaming for multi-node coordination

#### Synchronized Packet Capture (Phase 4)
- **Synchronized multi-node capture** with <10ms jitter
  - Hardware timestamping support (nanosecond precision)
  - ±1µs timestamp alignment with gPTP (IEEE 802.1AS)
  - Per-node capture with independent BPF filters
  - Automatic PCAP naming: {test_name}_{node_id}_{timestamp}.pcap

#### Message Correlation & PCAP Merging (Phase 4)
- **MessageCorrelator** — Identify same packet on multiple captures
  - PayloadHash correlation (packet content matching)
  - SequenceNumber correlation (protocol sequence tracking)
  - Handles packet loss and reordering
  
- **PcapMerger** — Merge multi-node captures with timestamp alignment
  - Synchronized timestamp merging
  - Packet interleaving by global timestamp
  - Metadata preservation from original captures
  - Output in standard pcap/pcapng format

#### Distributed Assertions (Phase 5)
- **ExpectMessageFlow** — Validate message sequences across nodes
  - Source → Intermediate → Destination pattern
  - Packet payload matching
  - Configurable timeout bounds
  
- **WithinLatency** — Request-response timing validation
  - Measure latency between request and response
  - Support for timeout detection
  - Per-node and aggregate metrics
  
- **HappensBefore** — Causal ordering validation
  - Validate that event A happened before event B
  - Timestamp-based ordering checks
  - Cross-node causality verification
  
- **MustNotSeeOn** — Negative assertions for absence validation
  - Verify packets absent on specific nodes
  - Support for traffic filtering constraints
  - Useful for security and isolation testing

#### Declarative Scenario Execution (Phase 6)
- **YAML/JSON Scenario Format** — Human-readable test definitions
  - Node topology with roles (provider, consumer, observer)
  - Sequential and parallel step execution
  - Barrier synchronization points
  - Timing constraints and timeouts
  
- **Step Types**: barrier, capture, command, expect, log, wait
- **Scenario Decomposition** — Automatic distribution to nodes
- **Step Coordination** — Sequential execution with barriers
- **Parallel Steps** — Concurrent node operations
- **Timing Constraints** — Enforce latency bounds

#### Result Aggregation & Observability (Phase 7-11)
- **Per-Node Results** — Capture statistics and assertions
  - Packet counts and byte totals
  - Captured PCAP file path
  - Capture duration and timestamps
  - Performance metrics (throughput, packet loss)
  
- **Result Aggregation** — Combine results from all nodes
  - Single aggregated result per scenario
  - Failed node handling with partial results
  - Performance metrics aggregation
  
- **JUnit XML Report** — CI/CD compatible output
  - Standard JUnit XML schema
  - Test suites = nodes
  - Test cases = scenarios
  - Properties = performance metrics
  
- **HTML Reports** — Human-readable result visualization
- **Test Failure PCAP Archival** — Auto-save captures on failure
- **Comprehensive Logging** — JSON-formatted logs with timestamps
- **CI/CD Integration** — Jenkins, GitLab CI, GitHub Actions examples

#### FFI Language Bindings (Phase 8)
- **C ABI Layer** — C99 compatible C API
  - libwadjet_c dynamic library
  - Opaque pointer types for C++ objects
  - Result type handling for C
  
- **Python Bindings** — pybind11-based Python API
  - Native Python objects for all types
  - pytest integration support
  - NumPy array support for packet data
  
- **Rust Bindings** — Safe idiomatic Rust wrappers
  - bindgen FFI bindings
  - Safe Result<T, E> error handling
  - Zero-copy integration

#### CLI Tools (Phase 9)
- **wadjet-coordinator** — Distributed test orchestrator
  - Command-line scenario execution
  - Node registration and management
  - JUnit XML and HTML report generation
  
- **wadjet-node** — Test node participant
  - Automatic coordinator discovery
  - Clock sync verification (--check-clock)
  - Multi-interface support

#### Examples & Documentation (Phase 10)
- **C++ Example** — 3-node SOME/IP Service Discovery test
  - Demonstrates full distributed testing workflow
  - Shows node registration and barrier synchronization
  - Includes assertion examples
  
- **Example Scenarios** — Ready-to-run test definitions
  - someip_discovery.yaml: 7-step SOME/IP SD test
  - nodes.yaml: Provider, consumer, monitor configuration
  
- **Documentation**
  - docs/distributed_testing.md: Complete architecture guide
  - docs/quickstart.md: 5-step quick start tutorial
  - README.md: Feature overview and examples

#### Infrastructure Completeness (Phase 11)
- **Umbrella Headers** — Single include point for all distributed APIs
  - include/wadjet/distributed/distributed.hpp
  - All public headers in one place
  
- **GoogleTest Fixture** — DistributedTestFixture
  - Automatic coordinator setup/teardown
  - AddNode(), LoadScenario(), RunScenario() helpers
  - Node health monitoring and status checks
  
- **Network Topology Visualization**
  - Mermaid diagram generation (graphical)
  - Graphviz DOT format (tool-compatible)
  - ASCII art (console-friendly)
  - Path finding and connectivity analysis
  
- **Network Partition Handling**
  - Split-brain detection via heartbeat quorum
  - Graceful degradation with minority node handling
  - on_partition_detected() callback
  
- **Parallel Scenario Execution**
  - run_scenarios_parallel() for concurrent tests
  - Per-scenario result isolation
  - Separate result aggregation for each scenario
  
- **Observability Enhancements**
  - PCAP naming convention support
  - Barrier event logging with timestamps
  - Replay mode for saved PCAP playback
  
- **Performance Metrics**
  - throughput_packets_per_sec calculation
  - packet_loss_count detection
  - Helper methods for metric computation
  
- **Protocol Version Compatibility**
  - Version field in RegisterNodeRequest
  - Major/minor/patch version checking
  - Forward/backward compatibility support

### Distributed Testing Specification

- **60+ Functional Requirements** — Comprehensive feature coverage
  - Node coordination and synchronization
  - Packet capture and correlation
  - Distributed assertions
  - Scenario execution and result aggregation
  
- **13 Success Criteria** — Validation and performance targets
  - <10ms capture jitter
  - ±1µs timestamp alignment (gPTP)
  - Message correlation across nodes
  - PCAP merging with timestamp ordering
  - Distributed assertion validation
  - Result aggregation and reporting
  
- **12 Phases** — Complete implementation plan
  - Phase 1-2: Setup and foundational primitives
  - Phase 3-5: Core user stories (coordination, capture, assertions)
  - Phase 6-7: Scenarios and result aggregation
  - Phase 8-10: FFI bindings, CLI tools, examples
  - Phase 11: Infrastructure completeness
  - Phase 12: Polish and validation
  
- **187 Implementation Tasks** — From infrastructure to examples
  - 164 base tasks across 11 phases
  - 23 infrastructure completeness tasks
  - Unit and integration tests throughout

### Added - M13 Protocol Completeness

#### Core Protocol Implementations
- **TCP State Machine** — Complete RFC 793 compliance with all 11 connection states
  - Connection establishment (SYN, SYN-ACK, ACK handshake)
  - Connection termination (FIN, RST handling)
  - Timeout management (2min idle, 30s close-wait)
  - Sequence number validation and wrapping
  - Window scaling and acknowledgement tracking

- **IPv4 Fragmentation** — Full reassembly engine with 30s timeout
  - Support for all 8 IPv4 option types
  - ToS/DSCP field parsing and validation
  - TTL tracking and hop-limit enforcement
  - Fragment offset and DF/MF flag handling

- **UDP Checksum Validation** — Configurable validation modes
  - Strict: Require all checksums
  - Warning: Log but allow missing checksums
  - Disabled: Skip checksum validation

- **SOME/IP-TP Segmentation** — Large message support up to 16 MB
  - 5-second reassembly timeout
  - Segment ordering and gap detection
  - Payload length validation

- **SOME/IP-SD Entry Arrays** — Complete service discovery parsing
  - Entry type identification (Service, Instance, Configuration, LoadBalancing, ProtectionOption)
  - Option array parsing (Configuration, LoadBalancing, ProtectionOption, Endpoint)
  - Dynamic vector serialization with proper TLV handling

- **DoIP Power Mode Tracking** — Enhanced diagnostic session management
  - Power mode state transitions
  - Routing activation with authorization
  - Diagnostic session tracking

- **UDS Negative Response Code (NRC) Classification**
  - Complete NRC reference table with all standardized codes
  - Temporary vs permanent error classification
  - Message format validation and protocol state tracking

- **gPTP TLV Parsing** — Time-Sensitive Networking message support
  - All TLV types for IEEE 802.1AS time synchronization
  - Grand master clock tracking
  - Sync interval and announce interval handling

#### Validation Framework
- **ProtocolValidator** — Cross-protocol validation with three modes
  - `validateLayering()` — Check valid protocol progressions
  - `validateLengths()` — Verify header/payload consistency
  - `validateChecksums()` — Validate IPv4, UDP, TCP checksums
  - Strict and Lenient validation modes
  - Comprehensive error reporting with byte offsets

#### Language Bindings
- **C Bindings (C99 compatible)**
  - Complete wadjet_c.h API covering all functionality
  - Opaque handle types for safety
  - Memory management helpers for RAII patterns
  - Thread-safe error handling with thread-local storage

- **Python Bindings (pybind11)**
  - PyPI package with full protocol API
  - Numpy array support for efficient data handling
  - pytest integration for testing
  - Type hints and docstrings for IDE support

- **Rust Bindings (FFI-safe)**
  - wadjet-sys crate for raw FFI
  - wadjet crate for safe idiomatic Rust API
  - Ownership semantics matching Rust patterns
  - bindgen-compatible C header

#### Example Programs
- `examples/protocol_validation.cpp` — Cross-layer validation demonstrations
  - Simple 3-layer stack (Ethernet → IPv4 → TCP)
  - Diagnostic 5-layer stack (Ethernet → IPv4 → TCP → DoIP → UDS)
  - Service discovery (Ethernet → IPv4 → UDP → SOME/IP-SD)
  - Checksum and length validation
  - VLAN-tagged protocol stacks
  - Strict vs Lenient validation comparison
  - Real-world automotive firmware download scenario

#### Testing Coverage
- **970+ Unit and Integration Tests** (up from 543 baseline)
  - 450+ core protocol decoder tests
  - 250+ testing framework tests
  - 70+ integration tests with cross-protocol scenarios
  - 150+ language binding tests
  - 5 fuzzing harnesses for robustness

- **Test Infrastructure**
  - Property-based testing with random packet generators
  - Record-then-assert mode for offline analysis
  - Live-assert mode for real-time validation
  - Comprehensive packet fixture library

#### Documentation
- Protocol documentation with field-by-field reference
- Language binding guides with code examples
- Architecture diagrams (PlantUML)
- Quickstart guides for common scenarios
- Doxygen API documentation for all public interfaces
- Release notes and migration guides

### Changed
- Updated README.md with M13 milestone completion status
- Increased test badge from 543 to 970+ tests
- Enhanced example program collection with protocol_validation.cpp
- Expanded architecture documentation with validation framework

### Fixed
- TCP connection state machine completeness and timeout handling
- IPv4 fragmentation reassembly edge cases
- UDP pseudo-header checksum calculation with IPv6 support
- SOME/IP-SD entry serialization with variable-length TLVs

### Performance Improvements
- Zero-copy validation using std::span<const std::byte>
- Efficient checksum computation with vector operations
- Optimized protocol layer traversal for deep stacks (10+ layers)
- Reduced memory allocations in validation result reporting

### Technical Debt Resolution
- Completed all protocol specifications to 100% coverage
- Implemented all test scenarios from specification
- Added comprehensive error handling for all code paths
- Verified compilation with -Werror and full sanitizer coverage

## [0.1.0] - 2024-06-15 (Initial Release)

### Added
- Initial Wadjet-Link framework
- Core packet types (Ethernet, IPv4, UDP, TCP)
- Basic protocol decoders
- GoogleTest integration
- PCAP file I/O (libpcap integration)
- Live capture support (AF_PACKET)
- Basic testing framework

### Infrastructure
- CMake build system
- GitHub Actions CI/CD
- Code formatting (clang-format)
- Static analysis (clang-tidy)
- Sanitizer support (ASAN, UBSAN)

---

## Legend

- **Added** — New features
- **Changed** — Changes in existing functionality
- **Fixed** — Bug fixes
- **Removed** — Removed features
- **Deprecated** — Features that will be removed in future versions
- **Security** — Important security fixes
- **Performance** — Performance improvements
- **Infrastructure** — Build system, CI/CD, tooling changes

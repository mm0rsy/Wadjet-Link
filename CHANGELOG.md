# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

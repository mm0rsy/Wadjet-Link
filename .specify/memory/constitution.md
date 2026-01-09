<!--
Sync Impact Report:
- Version change: INITIAL → 1.0.0
- Added sections: Core Principles (7 principles), Technical Standards, Development Workflow, Governance
- Modified principles: N/A (initial version)
- Removed sections: N/A (initial version)
- Templates requiring updates:
  ✅ .specify/templates/plan-template.md (Constitution Check section aligned)
  ✅ .specify/templates/spec-template.md (no changes required - general format compatible)
  ✅ .specify/templates/tasks-template.md (no changes required - general format compatible)
- Follow-up TODOs: None
-->

# Wadjet-Link Constitution

## Core Principles

### I. Library-First Architecture (NON-NEGOTIABLE)

**Every protocol decoder, capture engine, and testing utility MUST be implemented as a standalone library before integration.**

- Libraries MUST be self-contained with clear, singular purpose
- Libraries MUST be independently testable without external dependencies on other Wadjet components
- Libraries MUST have comprehensive documentation (header comments, Doxygen, examples)
- Libraries MUST provide both C++ API and C ABI layer for foreign function interface (FFI)
- No "organizational-only" libraries — each library must deliver concrete, user-facing functionality

**Rationale**: Automotive validation requires modular, reusable components that can be integrated into various test harnesses, debuggers, and production monitoring tools. Library-first design ensures portability and prevents vendor lock-in.

### II. Zero-Copy, Zero-Latency (PERFORMANCE MANDATE)

**All packet processing MUST use zero-copy techniques where possible. Capture MUST not introduce latency into the monitored network.**

- Use `PacketView` (immutable, zero-copy) for read-only operations
- Use `Packet` (mutable) only when modification is required
- Capture sessions MUST be passive observers — read-only mode, no packet injection during monitoring
- Ring buffers (TPACKET_V2/V3) MUST be preferred over copy-based packet capture
- Hardware timestamping MUST be used when available (nanosecond precision)

**Rationale**: Automotive Ethernet operates at high packet rates (100/1000Base-T1). Copying packets degrades performance and alters timing characteristics critical for gPTP and TSN analysis. Passive monitoring ensures functional safety (ASIL) compliance — no interference with production traffic.

### III. Test-First Development (NON-NEGOTIABLE)

**All features MUST follow TDD: Tests written → User approved → Tests fail → Implementation.**

- Unit tests MUST be written before implementation code
- GoogleTest framework MUST be used for all C++ tests
- Every protocol decoder MUST have:
  - Unit tests (parsing valid/malformed frames)
  - Integration tests (full protocol stack decode)
  - Fuzz tests (libFuzzer harness with AddressSanitizer)
- Every new API MUST have contract tests demonstrating usage
- Regression tests MUST use real PCAP samples stored in `pcap_samples/`

**Red-Green-Refactor cycle strictly enforced**:

1. Write test (RED — fails)
2. Get user approval for test cases
3. Implement minimum code to pass (GREEN)
4. Refactor while keeping tests green

**Rationale**: Automotive systems are safety-critical. Bugs in diagnostic or network monitoring tools can mask vehicle defects or create false positives, leading to recalls or safety incidents. TDD ensures correctness from day one.

### IV. Pluggable Protocol Architecture (EXTENSIBILITY)

**All protocol decoders MUST implement the `IProtocolDecoder` interface. New protocols MUST be added without modifying existing decoders.**

- `ProtocolDecoder` abstract base provides uniform decode API
- `DecoderBase<T>` CRTP pattern for type-safe decoder implementation
- `ProtocolDispatcher` handles EtherType/port-based dispatch
- Decoders MUST return `DecodeResult` with success/error and decoded header
- Decoders MUST NOT throw exceptions — use `std::expected` or error codes
- Decoders MUST validate all length fields and bounds before accessing memory

**Supported protocols** (must extend without breaking existing decoders):

- Ethernet (802.1Q VLAN, QinQ) ✅
- IPv4/UDP/TCP ✅
- SOME/IP + Service Discovery ✅
- DoIP (ISO 13400) ✅
- UDS (ISO 14229) ✅
- gPTP (IEEE 802.1AS) ✅
- DDS/RTPS ✅
- Future: CAN-over-Ethernet, AVB, AVTP

**Rationale**: Automotive protocols evolve rapidly (AUTOSAR, ISO standards). Pluggable architecture allows third-party decoders (proprietary OEM protocols) without forking the codebase.

### V. Multi-Language FFI Support (ADOPTION STRATEGY)

**All libraries MUST expose C99 ABI for foreign function interface. Python and Rust bindings MUST be maintained.**

- C ABI layer (`bindings/c/`) MUST use opaque handles (no C++ types in headers)
- Python bindings (`bindings/python/`) MUST use pybind11 with type stubs (`.pyi`)
- Rust bindings (`bindings/rust/`) MUST provide safe idiomatic wrappers over unsafe FFI
- All bindings MUST support:
  - Packet capture and replay
  - Protocol decoding
  - PCAP read/write
  - GoogleTest matchers (where applicable, e.g., pytest for Python)

**Rationale**: Automotive test engineers use diverse toolchains (Python for scripting, Rust for embedded safety, C for legacy integration). Multi-language support accelerates adoption and enables Wadjet-Link to integrate into existing test frameworks (pytest, Cargo test, CI/CD pipelines).

### VI. Observability and Forensics (DIAGNOSTIC MANDATE)

**All test failures MUST be reproducible. All runtime errors MUST be traceable.**

- Failed tests MUST automatically save PCAP to `failure_captures/` with timestamp
- All capture sessions MUST support PCAP export (`save_pcap()` method)
- Structured logging MUST be used (log levels: ERROR, WARN, INFO, DEBUG, TRACE)
- Errors MUST include context: file/line, timestamp, packet count, filter expression
- Reproducibility MUST be ensured:
  - Record mode → Replay mode (deterministic analysis)
  - Property-based tests MUST log random seed for reproduction

**Rationale**: Network bugs are often transient and timing-dependent. Without forensic captures, issues cannot be reproduced or analyzed post-mortem. PCAP + logs provide complete audit trail for compliance (ISO 26262, ASPICE).

### VII. Simplicity and YAGNI (COMPLEXITY GATE)

**Start simple. Complexity MUST be justified with documented rationale.**

- Prefer standard library (`std::vector`, `std::string_view`) over custom containers
- No premature optimization — measure before optimizing
- No frameworks unless absolutely necessary (exception: GoogleTest for testing, CMake for builds)
- New dependencies MUST be justified in pull request description
- YAGNI principle: "You Aren't Gonna Need It" — implement features when needed, not speculatively
- Avoid over-engineering: No complex class hierarchies unless warranted by protocol requirements

**Complexity justification required for**:

- New third-party dependencies
- Template metaprogramming beyond CRTP
- Custom memory allocators
- Multi-threading (automotive networks are typically single-threaded in test harness)

**Rationale**: Automotive software has long maintenance cycles (10+ years). Simple, readable code reduces onboarding time and maintenance burden. Complexity is technical debt.

## Technical Standards

### Language and Toolchain

- **Primary Language**: C++20 (ISO/IEC 14882:2020)
- **Compilers**: GCC 13+ or Clang 17+ (both must be supported)
- **Build System**: CMake 3.18+ with modern targets
- **Standard Library**: Use STL; avoid Boost unless absolutely necessary
- **Platform**: Linux-first (AF_PACKET, PF_PACKET); portable abstractions for future platforms

### Code Quality

- **Formatting**: `clang-format` with project `.clang-format` config (MANDATORY before commit)
- **Static Analysis**: `clang-tidy` MUST pass with zero warnings in CI
- **Sanitizers**: AddressSanitizer, UndefinedBehaviorSanitizer MUST be enabled in CI builds
- **Coverage**: Aim for meaningful coverage, not just metrics (no arbitrary percentage targets)
- **Documentation**: Doxygen comments MUST exist for all public APIs

### Naming Conventions

- **Namespaces**: `snake_case` (e.g., `wadjet::protocols::someip`)
- **Classes/Structs**: `PascalCase` (e.g., `CaptureSession`, `SOMEIPHeader`)
- **Functions/Methods**: `snake_case` (e.g., `start_capture()`, `decode_packet()`)
- **Variables**: `snake_case` (e.g., `packet_count`, `service_id`)
- **Private Members**: `snake_case_` with trailing underscore (e.g., `buffer_`, `session_`)
- **Constants**: `UPPER_CASE` (e.g., `MAX_PACKET_SIZE`, `DEFAULT_TIMEOUT`)
- **Macros**: `UPPER_CASE` with `WADJET_` prefix (e.g., `WADJET_ASSERT_PACKET_MATCHES`)

### Error Handling

- **No Exceptions in Hot Paths**: Packet processing MUST NOT use exceptions
- **Use `std::expected` or `DecodeResult`**: Return types encode success/error
- **Validate All Inputs**: Length fields, buffer bounds, pointer dereferences
- **Thread-Local Error Messages**: C ABI layer uses thread-local storage for error strings

### Testing Requirements

- **Minimum Test Coverage**: All public APIs, all protocol decoders, all matchers
- **Fuzz Testing**: libFuzzer harness for all protocol parsers
- **Regression Tests**: PCAP samples for known bugs and edge cases
- **CI Pipeline**: All tests MUST pass on both GCC and Clang before merge

## Development Workflow

### Branch Strategy

- **Main Branch**: `master` (always releasable, protected)
- **Feature Branches**: `feature/<milestone>-<description>` (e.g., `feature/m8-gptp-decoder`)
- **Bugfix Branches**: `fix/<issue>-<description>` (e.g., `fix/123-packet-leak`)

### Commit Messages

Use conventional commits:

```text
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**:

- `feat`: New feature (e.g., `feat(protocols): add gPTP decoder`)
- `fix`: Bug fix (e.g., `fix(capture): handle zero-length packets`)
- `docs`: Documentation (e.g., `docs: update quickstart guide`)
- `test`: Tests (e.g., `test: add fuzz harness for UDS decoder`)
- `refactor`: Code refactoring (no behavior change)
- `perf`: Performance improvement
- `ci`: CI/CD changes

### Pull Request Requirements

1. All tests MUST pass (unit, integration, fuzz)
2. Code MUST be formatted (`clang-format -i`)
3. Static analysis MUST pass (`clang-tidy`)
4. Documentation MUST be updated (if public API changes)
5. PCAP samples MUST be added (if new protocol or regression test)
6. PR description MUST reference related issues/milestones
7. At least one approving review required (maintainer)

### Review Checklist

- [ ] Constitution compliance verified (principles I-VII)
- [ ] Tests written before implementation (TDD)
- [ ] Zero-copy techniques used where possible
- [ ] No unnecessary complexity introduced
- [ ] Naming conventions followed
- [ ] Error handling present (no unchecked errors)
- [ ] Fuzz tests added (if protocol decoder)
- [ ] Documentation complete (Doxygen, examples)
- [ ] FFI bindings updated (C, Python, Rust)

## Governance

### Authority

This constitution is the **supreme governing document** for Wadjet-Link development. In case of conflict:

1. Constitution principles override all other policies
2. `CONTRIBUTING.md` provides operational guidelines (must align with constitution)
3. Code review feedback must cite constitution violations explicitly

### Amendment Process

**Minor Amendments** (PATCH version bump):

- Clarifications, typo fixes, formatting improvements
- No new principles or constraints added
- Approved by single maintainer

**Feature Amendments** (MINOR version bump):

- New principle added (extends, does not replace)
- New section added (e.g., security requirements)
- Approved by two maintainers

**Breaking Amendments** (MAJOR version bump):

- Principle removed or fundamentally redefined
- Existing code becomes non-compliant
- Requires migration plan with timeline
- Approved by all maintainers + public discussion period (1 week minimum)

### Compliance Verification

- **All PRs**: Reviewer MUST check constitution compliance checklist
- **Quarterly Audits**: Maintainer MUST review codebase for drift from principles
- **Milestone Retrospectives**: Team MUST assess if constitution principles are still valid

### Complexity Budget

Complexity MUST be justified. If introducing complexity, PR description MUST include:

1. **Problem Statement**: What problem does this complexity solve?
2. **Alternatives Considered**: Why simpler approaches don't work
3. **Maintenance Plan**: How will this be maintained long-term?
4. **Exit Strategy**: How to simplify or remove this later if needed?

Examples of complexity requiring justification:

- New third-party dependency beyond (GoogleTest, pybind11, yaml-cpp, nlohmann_json)
- Custom memory allocators
- Template metaprogramming beyond CRTP
- Multi-threading in packet processing

**Version**: 1.0.0 | **Ratified**: 2026-01-09 | **Last Amended**: 2026-01-09

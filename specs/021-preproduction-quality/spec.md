# Feature Specification: PreProduction Quality Gate

**Feature Branch**: `milestone/021-preproduction-quality`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M21 - PreProduction Quality Gate  
**Priority**: 🔴 **Critical** - Required before production release

## Overview

Establish comprehensive quality gate for production readiness. This milestone includes code quality enforcement, test coverage targets, performance benchmarking, security hardening, documentation completeness, and CI/CD maturity. **No production release is allowed without passing all quality gate criteria.**

## User Scenarios & Testing

### User Story 1 - Code Quality Metrics (Priority: P1)

As a project maintainer, I need automated code quality metrics, so that code adheres to project standards.

**Why this priority**: Code quality directly impacts maintainability and reliability.

**Independent Test**: Run static analysis tools and verify all issues are resolved.

**Acceptance Scenarios**:

1. **Given** clang-tidy configuration, **When** run on codebase, **Then** zero warnings are reported
2. **Given** clang-format configuration, **When** run, **Then** all files are already formatted (no changes)
3. **Given** cppcheck static analysis, **When** executed, **Then** zero errors or warnings are found
4. **Given** IWYU (Include What You Use), **When** run, **Then** all includes are necessary and sufficient
5. **Given** code complexity analysis, **When** performed, **Then** cyclomatic complexity ≤15 for all functions

### User Story 2 - Test Coverage Requirements (Priority: P1)

As a quality engineer, I need high test coverage, so that code changes don't introduce regressions.

**Why this priority**: Test coverage indicates thoroughness of validation.

**Independent Test**: Generate coverage report and verify ≥90% line coverage.

**Acceptance Scenarios**:

1. **Given** unit tests with coverage instrumentation, **When** run, **Then** line coverage ≥90%
2. **Given** branch coverage metrics, **When** calculated, **Then** branch coverage ≥85%
3. **Given** uncovered code, **When** identified, **Then** justification is documented (dead code, unreachable error paths)
4. **Given** coverage report, **When** generated, **Then** per-file and per-function coverage is displayed
5. **Given** coverage trends, **When** monitored, **Then** coverage never decreases below threshold

### User Story 3 - Performance Benchmarks (Priority: P1)

As a performance engineer, I need established performance benchmarks, so that regressions are detected early.

**Why this priority**: Performance is critical for real-time packet analysis.

**Independent Test**: Run benchmark suite and verify all targets are met.

**Acceptance Scenarios**:

1. **Given** packet capture benchmark, **When** run, **Then** capture rate ≥1M packets/sec on standard hardware
2. **Given** protocol decode benchmark, **When** run, **Then** SOME/IP decode ≥500K msg/sec
3. **Given** memory usage benchmark, **When** run, **Then** 100K packet capture uses ≤500MB RAM
4. **Given** latency benchmark, **When** run, **Then** packet → decode latency ≤100µs (p99)
5. **Given** benchmark regression detection, **When** run, **Then** performance degradation >5% triggers CI failure

### User Story 4 - Security Hardening (Priority: P1)

As a security engineer, I need security hardening measures, so that vulnerabilities are minimized.

**Why this priority**: Security is critical for automotive deployments.

**Independent Test**: Run security scanners and fuzzing campaigns.

**Acceptance Scenarios**:

1. **Given** AddressSanitizer (ASan) build, **When** test suite run, **Then** zero memory leaks or use-after-free detected
2. **Given** UndefinedBehaviorSanitizer (UBSan) build, **When** tests run, **Then** zero undefined behavior detected
3. **Given** ThreadSanitizer (TSan) build, **When** concurrent tests run, **Then** zero data races detected
4. **Given** fuzz testing campaign (1M+ iterations per protocol), **When** run, **Then** zero crashes or hangs
5. **Given** dependency vulnerability scan, **When** run, **Then** zero high or critical CVEs in dependencies

### User Story 5 - Documentation Completeness (Priority: P2)

As a new user, I need complete documentation, so that I can use Wadjet-Link effectively.

**Why this priority**: Documentation is essential for adoption.

**Independent Test**: Review documentation checklist and verify all items are complete.

**Acceptance Scenarios**:

1. **Given** Doxygen API documentation, **When** generated, **Then** 100% of public APIs are documented
2. **Given** quickstart guide, **When** followed by new user, **Then** first example works within 10 minutes
3. **Given** architecture documentation, **When** reviewed, **Then** all major subsystems are described with diagrams
4. **Given** troubleshooting guide, **When** consulted, **Then** common issues have documented solutions
5. **Given** migration guide, **When** followed, **Then** upgrade from previous version is seamless

## Edge Cases

- What happens when coverage tool reports false positives (unreachable code)?
- How are performance benchmarks adjusted for different hardware configurations?
- What if sanitizers produce false positives (third-party library issues)?
- How does system handle documentation for experimental features (marked unstable)?
- What happens when dependency vulnerability has no fix available?

## Requirements

### Functional Requirements

#### Code Quality Enforcement

- **FR-001**: System MUST enforce clang-tidy checks in CI (all warnings = errors)
- **FR-002**: System MUST enforce clang-format style checks in CI
- **FR-003**: System MUST run cppcheck static analysis with zero errors
- **FR-004**: System MUST run IWYU (Include What You Use) verification
- **FR-005**: System MUST enforce cyclomatic complexity ≤15 per function
- **FR-006**: System MUST enforce naming conventions (camelCase, snake_case as per guidelines)
- **FR-007**: System MUST detect and flag TODO/FIXME comments in production builds

#### Test Coverage Requirements

- **FR-008**: System MUST achieve ≥90% line coverage
- **FR-009**: System MUST achieve ≥85% branch coverage
- **FR-010**: System MUST generate HTML coverage reports (lcov, gcovr)
- **FR-011**: Coverage report MUST include per-file and per-function metrics
- **FR-012**: CI MUST fail if coverage drops below threshold
- **FR-013**: Uncovered code MUST be justified in coverage exceptions file

#### Performance Benchmarks

- **FR-014**: System MUST provide benchmark suite using Google Benchmark
- **FR-015**: Packet capture benchmark MUST achieve ≥1M packets/sec
- **FR-016**: SOME/IP decode benchmark MUST achieve ≥500K messages/sec
- **FR-017**: Memory usage for 100K packets MUST be ≤500MB
- **FR-018**: p99 decode latency MUST be ≤100µs
- **FR-019**: CI MUST run benchmarks and detect >5% performance regression
- **FR-020**: Benchmark results MUST be stored for trend analysis

#### Security Hardening

- **FR-021**: System MUST build with AddressSanitizer (ASan) in CI
- **FR-022**: System MUST build with UndefinedBehaviorSanitizer (UBSan) in CI
- **FR-023**: System MUST build with ThreadSanitizer (TSan) for concurrent tests
- **FR-024**: Test suite MUST pass under all sanitizers with zero errors
- **FR-025**: Fuzz testing MUST run 1M+ iterations per protocol in CI
- **FR-026**: System MUST scan dependencies for CVEs (using tools like snyk or dependabot)
- **FR-027**: System MUST use compiler hardening flags (-fstack-protector-strong, -D_FORTIFY_SOURCE=2, -fPIE)

#### Documentation Completeness

- **FR-028**: 100% of public APIs MUST have Doxygen documentation
- **FR-029**: Quickstart guide MUST be complete and tested
- **FR-030**: Architecture documentation MUST include diagrams for all major subsystems
- **FR-031**: Troubleshooting guide MUST document top 10 common issues
- **FR-032**: Migration guide MUST exist for version upgrades
- **FR-033**: CHANGELOG MUST follow semantic versioning and conventional commits

#### CI/CD Maturity

- **FR-034**: CI pipeline MUST run on every commit (Linux, Windows, macOS)
- **FR-035**: CI MUST enforce all quality gates (code style, tests, coverage, sanitizers)
- **FR-036**: CI MUST build release artifacts automatically on tag push
- **FR-037**: CI MUST publish Docker images to Docker Hub
- **FR-038**: CI MUST run nightly extended test suite (long-running tests, fuzz campaigns)
- **FR-039**: CI status badge MUST be displayed in README.md

#### Design Principles Validation

- **FR-040**: All packet processing MUST be zero-copy (no unnecessary buffer allocations)
- **FR-041**: All protocol decoders MUST use `std::span` or `PacketView` (no raw pointers)
- **FR-042**: Error handling MUST use `std::expected` or exceptions (no error codes)
- **FR-043**: Memory management MUST use RAII (no manual new/delete)
- **FR-044**: Thread safety MUST be documented for all public APIs

### Key Entities

- **QualityGateChecker**: Validates all quality criteria
- **CoverageReporter**: Generates and analyzes test coverage
- **BenchmarkRunner**: Executes performance benchmarks
- **SanitizerValidator**: Verifies sanitizer builds pass
- **SecurityScanner**: Checks for vulnerabilities
- **DocumentationValidator**: Verifies docs completeness
- **CiPipeline**: Orchestrates all quality checks

## Success Criteria

### Measurable Outcomes

- **SC-001**: clang-tidy, clang-format, cppcheck pass with zero warnings/errors
- **SC-002**: Line coverage ≥90%, branch coverage ≥85%
- **SC-003**: All performance benchmarks meet targets (capture ≥1M pkt/s, decode ≥500K msg/s)
- **SC-004**: ASan, UBSan, TSan builds pass test suite with zero errors
- **SC-005**: Fuzz testing runs 1M+ iterations per protocol with zero crashes
- **SC-006**: Dependency scan shows zero high/critical CVEs
- **SC-007**: 100% of public APIs have Doxygen documentation
- **SC-008**: CI pipeline runs all checks on every commit across Linux, Windows, macOS
- **SC-009**: Release artifacts (DEB, RPM, Docker) build automatically on version tags
- **SC-010**: All design principles (zero-copy, RAII, error handling) validated in code review checklist

## Assumptions

- CI/CD infrastructure (GitHub Actions) is available and reliable
- Standard hardware for benchmarks: 8-core CPU, 16GB RAM, SSD
- Code coverage tools (gcov, lcov) are accurate and reliable
- Sanitizers (ASan, UBSan, TSan) are available in build environment
- Documentation is maintained in sync with code changes

## Dependencies

- **External**: clang-tidy, clang-format, cppcheck, IWYU, gcov/lcov, Google Benchmark, sanitizers
- **Internal**: All previous milestones (M0-M20)

## Out of Scope

- Manual code reviews (automated checks only)
- User acceptance testing (focus on technical quality)
- Performance tuning beyond benchmark targets
- Security penetration testing (only automated scanning)
- Internationalization and localization (English-only docs)

## Implementation Notes

### Recommended Approach

**Phase 1 - Code Quality Setup** (1 week)
- Configure clang-tidy, clang-format, cppcheck
- Add IWYU checks
- Integrate into CI
- Fix all existing violations
- Tests: Validate all tools run successfully

**Phase 2 - Test Coverage** (1 week)
- Add coverage instrumentation to CMake
- Generate baseline coverage report
- Identify uncovered code
- Add tests to reach ≥90% coverage
- Tests: Verify coverage meets threshold

**Phase 3 - Performance Benchmarks** (1 week)
- Create benchmark suite with Google Benchmark
- Implement capture, decode, memory benchmarks
- Establish baseline metrics
- Add regression detection to CI
- Tests: All benchmarks meet targets

**Phase 4 - Sanitizer Builds** (1 week)
- Add ASan, UBSan, TSan build configurations
- Fix all sanitizer violations
- Add sanitizer runs to CI
- Tests: All tests pass under sanitizers

**Phase 5 - Fuzz Testing** (1 week)
- Enhance fuzz harnesses
- Run extended fuzz campaigns (1M+ iterations)
- Fix all crashes/hangs
- Add fuzz testing to nightly CI
- Tests: Zero crashes in 1M iterations

**Phase 6 - Security Scanning** (0.5 weeks)
- Add dependency vulnerability scanning
- Update dependencies with CVEs
- Add compiler hardening flags
- Tests: Zero high/critical CVEs

**Phase 7 - Documentation Validation** (1 week)
- Audit API documentation completeness
- Update quickstart, architecture, troubleshooting guides
- Create migration guide
- Tests: All docs complete and accurate

**Phase 8 - CI/CD Maturity** (1 week)
- Consolidate all checks into CI pipeline
- Add cross-platform builds
- Add nightly extended tests
- Add release automation
- Tests: CI runs all checks successfully

**Total Duration**: ~7.5 weeks

### clang-tidy Configuration

```yaml
# .clang-tidy
Checks: >
  -*,
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  google-*,
  misc-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-identifier-length

CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.VariableCase
    value: lower_case
```

### Coverage CI Job

```yaml
# .github/workflows/coverage.yml
name: Coverage

on: [push, pull_request]

jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt-get install -y lcov
      - name: Build with coverage
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON
          cmake --build build
      - name: Run tests
        run: cd build && ctest
      - name: Generate coverage report
        run: |
          lcov --capture --directory build --output-file coverage.info
          lcov --remove coverage.info '/usr/*' '*_test.cpp' --output-file coverage.info
          lcov --list coverage.info
      - name: Check coverage threshold
        run: |
          COVERAGE=$(lcov --summary coverage.info | grep lines | awk '{print $2}' | sed 's/%//')
          if (( $(echo "$COVERAGE < 90" | bc -l) )); then
            echo "Coverage $COVERAGE% below threshold 90%"
            exit 1
          fi
```

### Benchmark Example

```cpp
#include <benchmark/benchmark.h>
#include <wadjet/protocols/someip/someip_decoder.hpp>

static void BM_SomeipDecode(benchmark::State& state) {
    // Setup: Create SOME/IP message
    std::vector<uint8_t> msg = create_someip_message();
    
    for (auto _ : state) {
        SomeipDecoder decoder;
        auto result = decoder.decode(msg);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetLabel("SOME/IP decode");
}

BENCHMARK(BM_SomeipDecode)->Iterations(1000000);

BENCHMARK_MAIN();
```

### Quality Gate Checklist

```markdown
# Pre-Release Quality Gate Checklist

## Code Quality
- [ ] clang-tidy: 0 warnings
- [ ] clang-format: All files formatted
- [ ] cppcheck: 0 errors/warnings
- [ ] IWYU: All includes verified
- [ ] Cyclomatic complexity ≤15

## Test Coverage
- [ ] Line coverage ≥90%
- [ ] Branch coverage ≥85%
- [ ] Uncovered code justified

## Performance
- [ ] Capture rate ≥1M pkt/s
- [ ] Decode rate ≥500K msg/s
- [ ] Memory usage ≤500MB (100K packets)
- [ ] p99 latency ≤100µs

## Security
- [ ] ASan build: 0 errors
- [ ] UBSan build: 0 errors
- [ ] TSan build: 0 errors
- [ ] Fuzz testing: 1M+ iterations, 0 crashes
- [ ] CVE scan: 0 high/critical

## Documentation
- [ ] API docs: 100% coverage
- [ ] Quickstart guide complete
- [ ] Architecture docs complete
- [ ] Troubleshooting guide complete
- [ ] Migration guide complete

## CI/CD
- [ ] CI passes on Linux, Windows, macOS
- [ ] Release artifacts build
- [ ] Docker image publishes
- [ ] Nightly tests pass

## Design Principles
- [ ] Zero-copy verified
- [ ] Error handling consistent
- [ ] RAII enforced
- [ ] Thread safety documented
```

### Test Count Target

No specific test count - focus is on coverage metrics:
- Line coverage ≥90%
- Branch coverage ≥85%
- All sanitizer builds pass
- All benchmarks meet targets

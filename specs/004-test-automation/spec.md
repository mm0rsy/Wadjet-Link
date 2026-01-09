# Feature Specification: Test Automation with YAML/JSON Scenarios

**Feature Branch**: `milestone/004-test-automation`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M4 - Automation & Test Scenario Language

## User Scenarios & Testing

### User Story 1 - Declarative Test Scenarios (Priority: P1)

As a test automation engineer without deep C++ knowledge, I need to define test scenarios in YAML/JSON files, so that I can create and maintain automotive network tests without writing code.

**Why this priority**: Declarative scenarios democratize test creation, enabling non-programmers to write tests.

**Independent Test**: Create a YAML scenario file that captures packets and asserts on SOME/IP service discovery, then run it and verify it passes.

**Acceptance Scenarios**:

1. **Given** a YAML file defining a scenario, **When** I parse it, **Then** I get a structured Scenario object with steps
2. **Given** a scenario with capture, wait, and expect steps, **When** I run it with wadjet-run, **Then** each step executes in sequence
3. **Given** an expect step checking for a SOME/IP service offer, **When** the expected packet arrives, **Then** the step passes
4. **Given** an expect step with a timeout, **When** no matching packet arrives, **Then** the scenario fails with descriptive error
5. **Given** a JSON scenario file (alternative to YAML), **When** I run it, **Then** it behaves identically to YAML

---

### User Story 2 - CLI Runner for CI/CD (Priority: P1)

As a DevOps engineer, I need a command-line tool (wadjet-run) that executes scenario files and outputs results in CI-friendly formats (JUnit XML, JSON), so that I can integrate tests into Jenkins/GitLab CI pipelines.

**Why this priority**: CI/CD integration is essential for automated regression testing in automotive projects.

**Independent Test**: Run wadjet-run with a scenario file, verify it executes successfully, and outputs JUnit XML that can be parsed by CI tools.

**Acceptance Scenarios**:

1. **Given** a scenario file path, **When** I run `wadjet-run scenario.yaml -i eth0`, **Then** the scenario executes and prints results to stdout
2. **Given** a --format option, **When** I run `wadjet-run --format junit scenario.yaml`, **Then** output is valid JUnit XML
3. **Given** multiple scenario files in a directory, **When** I run `wadjet-run scenarios/`, **Then** all scenarios execute sequentially
4. **Given** a failed scenario, **When** wadjet-run completes, **Then** the exit code is non-zero to signal CI failure
5. **Given** verbose mode (--verbose), **When** running scenarios, **Then** detailed packet information is logged

---

### User Story 3 - Tag-Based Test Selection (Priority: P2)

As a test engineer managing hundreds of scenarios, I need to tag scenarios (e.g., "smoke", "regression", "gptp") and run subsets using tag filters, so that I can execute only relevant tests for different contexts.

**Why this priority**: Test organization is important for large test suites but not blocking for basic scenario execution.

**Independent Test**: Tag scenarios with "smoke" and "full", then run with --tags smoke and verify only smoke tests execute.

**Acceptance Scenarios**:

1. **Given** scenarios tagged with "smoke" and "regression", **When** I run `wadjet-run --tags smoke`, **Then** only smoke-tagged scenarios execute
2. **Given** scenarios with multiple tags, **When** I use `--tags tag1,tag2`, **Then** scenarios matching any of the tags run
3. **Given** a --exclude-tags option, **When** used, **Then** matching scenarios are skipped
4. **Given** tag filtering, **When** results are reported, **Then** skipped scenarios are clearly indicated

---

### User Story 4 - Multiple Report Formats (Priority: P2)

As a test results analyst, I need scenario results in multiple formats (JUnit XML, JSON, TAP, human-readable text), so that I can integrate with different tools and present results to stakeholders.

**Why this priority**: Flexibility in reporting improves tool adoption, but basic text output is sufficient initially.

**Independent Test**: Run scenarios with different --format options (text, json, junit, tap) and verify each produces valid output.

**Acceptance Scenarios**:

1. **Given** --format text, **When** scenarios run, **Then** output is human-readable with pass/fail indicators and timing
2. **Given** --format json, **When** scenarios run, **Then** output is valid JSON with structured results
3. **Given** --format junit, **When** scenarios run, **Then** output is JUnit XML compatible with Jenkins
4. **Given** --format tap, **When** scenarios run, **Then** output follows Test Anything Protocol for Perl-style test harnesses

---

### User Story 5 - Dry-Run Mode (Priority: P3)

As a scenario author debugging test logic, I need a dry-run mode that validates scenario syntax without executing steps, so that I can catch errors quickly before running on real hardware.

**Why this priority**: Nice-to-have for productivity, but not essential for core functionality.

**Independent Test**: Run wadjet-run with --dry-run flag and verify it parses scenarios and reports syntax errors without capturing packets.

**Acceptance Scenarios**:

1. **Given** --dry-run flag, **When** wadjet-run executes, **Then** no packets are captured or network interfaces accessed
2. **Given** a scenario with syntax errors, **When** dry-run executes, **Then** errors are reported with line numbers
3. **Given** valid scenarios in dry-run, **When** they complete, **Then** all scenarios report as validated
4. **Given** dry-run mode, **When** combined with --verbose, **Then** full scenario execution plan is printed

### Edge Cases

- What happens when a YAML file has syntax errors or invalid keys?
- How are scenarios with missing required fields (e.g., no "name") handled?
- What if a scenario references an interface that doesn't exist at runtime?
- How does the system handle extremely large scenario files (10,000+ lines)?
- What happens when a scenario expects a protocol that Wadjet-Link doesn't support?
- How are network errors (interface down, permission denied) reported in scenario results?

## Requirements

### Functional Requirements

- **FR-001**: System MUST parse YAML scenario files using yaml-cpp library
- **FR-002**: System MUST parse JSON scenario files using nlohmann_json library
- **FR-003**: System MUST auto-detect file format (.yaml/.yml vs .json) based on extension
- **FR-004**: Scenario MUST have a data model with: name, description, tags, steps
- **FR-005**: System MUST support CaptureStep with interface, filter, and duration
- **FR-006**: System MUST support ExpectStep with protocol expectations (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP)
- **FR-007**: System MUST support WaitStep with duration
- **FR-008**: System MUST support SendStep for packet injection (future extension point)
- **FR-009**: System MUST support LogStep for custom messages
- **FR-010**: System MUST provide ScenarioRunner class to execute scenarios
- **FR-011**: ScenarioRunner MUST support timeout for expect steps
- **FR-012**: ScenarioRunner MUST match packets against expectations using same matchers as GoogleTest
- **FR-013**: System MUST provide wadjet-run CLI tool for scenario execution
- **FR-014**: wadjet-run MUST accept interface (-i/--interface) and output format (-o/--format) options
- **FR-015**: wadjet-run MUST support tag filtering (--tags) and exclusion (--exclude-tags)
- **FR-016**: wadjet-run MUST support dry-run mode (--dry-run) for validation without execution
- **FR-017**: wadjet-run MUST support verbose mode (--verbose) for detailed logging
- **FR-018**: System MUST generate JUnit XML reports compatible with Jenkins/GitLab CI
- **FR-019**: System MUST generate JSON reports with structured test results
- **FR-020**: System MUST generate TAP (Test Anything Protocol) output
- **FR-021**: System MUST generate human-readable text reports
- **FR-022**: All reports MUST include scenario name, pass/fail status, execution time, and error messages
- **FR-023**: wadjet-run MUST exit with code 0 on success, non-zero on any failure
- **FR-024**: Scenario parsing errors MUST include file name, line number, and descriptive error message
- **FR-025**: System MUST execute all steps in a scenario sequentially in defined order

### Key Entities

- **Scenario**: Top-level structure with name, description, tags, and steps
- **Step**: Base type with variants (CaptureStep, ExpectStep, WaitStep, SendStep, LogStep)
- **ExpectStep**: Defines protocol expectations (e.g., SOME/IP service ID, DoIP routing activation)
- **ScenarioRunner**: Executes scenarios by managing capture sessions and matching packets
- **ScenarioResult**: Outcome of scenario execution with pass/fail, timing, errors
- **ExpectResult**: Outcome of individual expect step with matched packet or timeout
- **ReportGenerator**: Produces output in various formats (JUnit, JSON, TAP, Text)
- **YAMLParser**: Parses YAML scenario files into Scenario objects
- **JSONParser**: Parses JSON scenario files into Scenario objects

## Success Criteria

### Measurable Outcomes

- **SC-001**: YAML parser successfully parses all example scenarios (3+ examples) without errors
- **SC-002**: JSON parser successfully parses all example scenarios with identical behavior to YAML
- **SC-003**: ScenarioRunner executes scenarios with 100% correctness (verified by 20+ runner tests)
- **SC-004**: wadjet-run CLI accepts all documented options and produces expected output formats
- **SC-005**: JUnit XML output validates against Jenkins JUnit XSD schema
- **SC-006**: JSON output parses correctly in standard JSON tools (jq, Python json module)
- **SC-007**: TAP output validates against TAP specification
- **SC-008**: All report formats include identical pass/fail results for the same scenario execution
- **SC-009**: Tag filtering correctly selects/excludes scenarios (verified in integration tests)
- **SC-010**: Dry-run mode catches syntax errors without network access (verified in tests)
- **SC-011**: Scenario test suite includes 41 tests (21 parser + 20 runner/reports) with 100% pass rate

## Assumptions

- Scenario files are stored as UTF-8 text files
- YAML indentation follows standard conventions (2 or 4 spaces)
- Scenarios are executed serially, not in parallel
- Network interface specified in scenarios exists and is accessible at runtime

## Dependencies

- **External**: yaml-cpp (YAML parsing), nlohmann_json (JSON parsing)
- **Internal**: Milestone 1 (CaptureSession), Milestone 2 (Protocol decoders), Milestone 3 (Matchers for expect steps)

## Out of Scope

- GUI-based scenario editor
- Scenario templates or wizards
- Parameterized scenarios (variable substitution)
- Scenario chaining or dependencies between scenarios
- Parallel scenario execution
- Real-time scenario editing during execution
- Scenario versioning or migration tools

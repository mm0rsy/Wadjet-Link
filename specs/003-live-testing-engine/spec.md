# Feature Specification: Live Testing Engine with GoogleTest Integration

**Feature Branch**: `milestone/003-live-testing-engine`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M3 - Live Testing Engine (GoogleTest Integration)

## User Scenarios & Testing

### User Story 1 - Assert on Live Traffic (Priority: P1)

As a test engineer writing integration tests, I need to make assertions on live packet streams using familiar GoogleTest syntax, so that I can write expressive tests that validate real-time ECU behavior.

**Why this priority**: This is the core differentiator of Wadjet-Link. Testing live traffic with GoogleTest is the unique value proposition.

**Independent Test**: Write a GoogleTest test case that captures packets on loopback, sends a UDP packet, and asserts it was received.

**Acceptance Scenarios**:

1. **Given** a LiveCaptureTestFixture, **When** I send a UDP packet on loopback, **Then** I can assert the packet matches expected criteria using EXPECT_THAT
2. **Given** a packet matcher like HasUdpPort(30490), **When** I apply it to a captured packet, **Then** it correctly matches or rejects based on the packet's UDP port
3. **Given** a test that fails to find expected packets, **When** the test completes, **Then** the captured traffic is automatically saved to failure_captures/ directory
4. **Given** multiple test cases in a test suite, **When** they run sequentially, **Then** each test has an isolated capture session

---

### User Story 2 - Protocol-Specific Matchers (Priority: P1)

As a SOME/IP test developer, I need domain-specific matchers like HasSOMEIPServiceId() and IsDoIPRoutingActivation(), so that I can write readable, self-documenting test assertions.

**Why this priority**: Matchers make tests expressive and reduce boilerplate. Essential for adoption by test engineers.

**Independent Test**: Write test cases using all protocol-specific matchers and verify they correctly match/reject packets.

**Acceptance Scenarios**:

1. **Given** a SOME/IP packet, **When** I use EXPECT_THAT(packet, HasSOMEIPServiceId(0x1234)), **Then** the matcher validates the service ID
2. **Given** a DoIP routing activation packet, **When** I use IsDoIPRoutingActivation() matcher, **Then** it correctly identifies the message type
3. **Given** an Ethernet frame, **When** I use HasEthertype(0x0800) matcher, **Then** it validates the EtherType field
4. **Given** matchers can be combined with AND/OR/NOT, **When** I compose complex conditions, **Then** boolean logic is correctly evaluated
5. **Given** a payload matcher like PayloadContains({0x01, 0x02}), **When** applied to a packet, **Then** it searches the payload for the byte sequence

---

### User Story 3 - Time-Bounded Expectations (Priority: P1)

As a validation engineer testing service discovery, I need time-bounded expectations like wait_for_packet(predicate, timeout), so that I can assert packets arrive within specified timeframes.

**Why this priority**: Real-time systems have timing requirements. Tests must validate temporal behavior.

**Independent Test**: Start capture, send a packet after 100ms delay, and verify wait_for_packet() succeeds within 200ms timeout.

**Acceptance Scenarios**:

1. **Given** a capture session, **When** I call wait_for_packet(predicate, 1s), **Then** the call blocks until a matching packet arrives or timeout occurs
2. **Given** a predicate that never matches, **When** wait_for_packet times out, **Then** it returns false and the test can handle the failure
3. **Given** multiple threads sending packets, **When** wait_for_packet is called, **Then** it correctly synchronizes and finds matching packets
4. **Given** any_packet_matches(predicate, duration), **When** called, **Then** it returns true if at least one packet matches within the duration

---

### User Story 4 - Pattern Matching Over Streams (Priority: P2)

As an ECU bootup sequence tester, I need to collect packets over a time window and assert on patterns (e.g., "3 SOME/IP service offers within 5 seconds"), so that I can validate complex multi-packet behaviors.

**Why this priority**: Many automotive scenarios involve sequences of packets. Important but secondary to single-packet assertions.

**Independent Test**: Send 3 UDP packets over 2 seconds, use collect_packets(2s), and verify count equals 3.

**Acceptance Scenarios**:

1. **Given** a capture session, **When** I call collect_packets(5s), **Then** I receive a vector of all packets captured in that window
2. **Given** a collected packet stream, **When** I apply std::count_if with a matcher, **Then** I can count packets matching specific criteria
3. **Given** count_packets(predicate, duration), **When** called, **Then** it returns the count of matching packets within the duration
4. **Given** a sequence pattern (e.g., packet A followed by packet B), **When** I analyze collected packets, **Then** I can validate the ordering

---

### User Story 5 - Deterministic Replay Testing (Priority: P2)

As a regression test maintainer, I need record-then-assert mode where I capture traffic once and replay it for assertions, so that tests are deterministic and don't depend on live network conditions.

**Why this priority**: Deterministic tests are easier to debug and maintain. Important for CI/CD pipelines.

**Independent Test**: Record a packet stream to a file, then replay it in a test and verify assertions pass identically on each run.

**Acceptance Scenarios**:

1. **Given** a RecordedStream from a PCAP file, **When** I iterate through packets, **Then** I get identical packets on every replay
2. **Given** a recorded stream, **When** I apply matchers, **Then** assertions behave identically to live capture
3. **Given** property-based testing with random generators, **When** I log the random seed, **Then** I can reproduce failed test cases exactly
4. **Given** a LiveAssertSession, **When** I run_for(duration), **Then** assertions execute in real-time and results are deterministic

### Edge Cases

- What happens when wait_for_packet() is called but no packets ever arrive (timeout scenario)?
- How are test fixtures cleaned up if a test crashes mid-execution?
- What if the failure_captures/ directory doesn't exist or lacks write permissions?
- How does the framework handle extremely high packet rates that might overflow buffers during collection?
- What happens when multiple tests run in parallel (different test processes) using the same loopback interface?

## Requirements

### Functional Requirements

- **FR-001**: System MUST provide LiveCaptureTestFixture base class for GoogleTest test cases
- **FR-002**: LiveCaptureTestFixture MUST automatically initialize and teardown capture sessions
- **FR-003**: System MUST provide gMock-style matchers for all supported protocols (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP)
- **FR-004**: System MUST provide Ethernet matchers: HasEthertype, HasSourceMac, HasDestMac, HasVlan, HasVlanId
- **FR-005**: System MUST provide IPv4 matchers: HasIPv4Source, HasIPv4Dest, HasIPv4Protocol, HasTTL
- **FR-006**: System MUST provide UDP matchers: HasUdpSourcePort, HasUdpDestPort, IsUdpPort
- **FR-007**: System MUST provide TCP matchers: HasTcpSourcePort, HasTcpDestPort, HasTcpFlags, HasTcpSeq
- **FR-008**: System MUST provide SOME/IP matchers: HasSOMEIPServiceId, HasSOMEIPMethodId, HasSOMEIPMessageType
- **FR-009**: System MUST provide DoIP matchers: HasDoIPPayloadType, HasDoIPRoutingActivation, HasDoIPDiagnosticMessage
- **FR-010**: System MUST provide payload matchers: PayloadContains, HasPayloadSize, DecodesSuccessfully
- **FR-011**: All matchers MUST be composable using gMock AllOf, AnyOf, Not
- **FR-012**: System MUST provide wait_for_packet(predicate, timeout) for time-bounded packet expectations
- **FR-013**: System MUST provide any_packet_matches(predicate, timeout) to check if at least one packet matches
- **FR-014**: System MUST provide collect_packets(duration) to gather packets over a time window
- **FR-015**: System MUST provide count_packets(predicate, duration) to count matching packets
- **FR-016**: LiveCaptureTestFixture MUST auto-save PCAP on test failure to configurable failure directory
- **FR-017**: System MUST provide LoopbackTestFixture with send_udp(port, payload) helper
- **FR-018**: System MUST provide LoopbackTestFixture with connect_tcp(port) helper
- **FR-019**: System MUST provide convenience macros: WADJET_ASSERT_PACKET_MATCHES, WADJET_EXPECT_PACKET_MATCHES
- **FR-020**: System MUST provide protocol-specific macros: EXPECT_SOMEIP_SERVICE, EXPECT_DOIP_ROUTING_ACTIVATION
- **FR-021**: System MUST provide RecordedStream class for deterministic replay from PCAP files
- **FR-022**: System MUST provide LiveAssertSession for real-time assertion checking with run_for()/run_until()
- **FR-023**: System MUST provide PropertyBasedTesting generators for random packet generation with reproducible seeds
- **FR-024**: All assertion failures MUST include packet content in human-readable format in test output
- **FR-025**: System MUST be thread-safe for concurrent packet capture and assertion

### Key Entities

- **LiveCaptureTestFixture**: GoogleTest fixture base class with capture session management
- **LoopbackTestFixture**: Extends LiveCaptureTestFixture with loopback packet injection helpers
- **RecordedStream**: Provides deterministic replay of packets from PCAP files
- **LiveAssertSession**: Real-time assertion checking with time-bounded execution
- **PacketMatcher**: Base class for all protocol-specific matchers (integrates with gMock)
- **WaitCondition**: Predicate-based wait mechanism for time-bounded expectations
- **PacketGenerator**: Property-based testing utility for generating random protocol packets
- **Random**: Seed-based random number generator for reproducible tests

## Success Criteria

### Measurable Outcomes

- **SC-001**: All protocol matchers (39 matchers total) have comprehensive unit tests with 100% pass rate
- **SC-002**: LiveCaptureTestFixture successfully captures and asserts on loopback traffic in 24 integration tests
- **SC-003**: wait_for_packet() correctly handles both match and timeout scenarios with <10ms timing accuracy
- **SC-004**: Failed tests automatically save PCAP files to failure_captures/ with correct timestamps and test names
- **SC-005**: All matchers compose correctly with AllOf, AnyOf, Not without logical errors
- **SC-006**: Property-based testing generates valid packets for all supported protocols with logged seeds
- **SC-007**: RecordedStream provides 100% deterministic replay (identical assertions on every run)
- **SC-008**: LiveAssertSession runs real-time assertions with timeout accuracy within 50ms
- **SC-009**: Test suite completes 158+ tests (33 matcher + 24 live capture + 40 generator + 35 record-replay + 26 live-assert) with 100% pass rate
- **SC-010**: All test fixtures properly cleanup resources (no leaked file descriptors or capture sessions)

## Assumptions

- GoogleTest framework is available and linked into the test binary
- Tests run with sufficient permissions to capture on loopback interface (or use PcapCaptureSession as fallback)
- Test execution environment has predictable timing (within 100ms accuracy for timeouts)
- Loopback interface exists and supports packet capture

## Dependencies

- **External**: GoogleTest, gMock (for matcher framework)
- **Internal**: Milestone 1 (CaptureSession, ReplaySession, PCAP I/O), Milestone 2 (Protocol decoders for matchers)

## Out of Scope

- pytest integration for Python (this is Milestone 5)
- GUI test runner or visualizations
- Distributed testing across multiple machines
- Performance benchmarking framework
- Code coverage analysis tooling
- Test result database or historical tracking

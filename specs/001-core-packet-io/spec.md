# Feature Specification: Core Packet Capture and I/O

**Feature Branch**: `milestone/001-core-packet-io`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M1 - Core Packet I/O

## User Scenarios & Testing

### User Story 1 - Live Packet Capture (Priority: P1)

As an automotive test engineer, I need to capture live Ethernet packets from a network interface in real-time, so that I can monitor ECU communication during vehicle testing without disrupting the network.

**Why this priority**: This is the foundational capability of the entire framework. Without packet capture, no other features are possible.

**Independent Test**: Start capture on loopback interface, send UDP packets, verify packets are captured with correct timestamps and payloads.

**Acceptance Scenarios**:

1. **Given** a network interface name (e.g., "eth0"), **When** I create a CaptureSession and start capture, **Then** packets flowing through that interface are captured
2. **Given** an active capture session, **When** packets arrive, **Then** each packet has a nanosecond-precision timestamp
3. **Given** a capture with BPF filter "udp port 30490", **When** packets arrive, **Then** only UDP packets on port 30490 are captured
4. **Given** high packet rates (10,000+ packets/sec), **When** capture is active, **Then** dropped packet count remains at or near zero
5. **Given** a capture session, **When** I stop the session, **Then** statistics are available (packets captured, dropped, interface errors)

---

### User Story 2 - Zero-Copy Packet Access (Priority: P1)

As a performance-critical application developer, I need to access packet data without copying memory buffers, so that I can analyze high-speed automotive Ethernet traffic without performance degradation.

**Why this priority**: Automotive Ethernet runs at high packet rates. Memory copies would create bottlenecks and affect timing analysis.

**Independent Test**: Capture packets and verify PacketView provides read-only access to packet data without allocating new buffers.

**Acceptance Scenarios**:

1. **Given** a captured packet, **When** I create a PacketView, **Then** no memory copy occurs
2. **Given** a PacketView, **When** I access packet data, **Then** I get a read-only std::span pointing to the original buffer
3. **Given** a Packet object (mutable), **When** I need to modify packet data, **Then** I can write to the buffer
4. **Given** multiple PacketView instances, **When** they reference the same underlying packet, **Then** memory usage is constant (views share data)

---

### User Story 3 - PCAP File Replay (Priority: P1)

As a validation engineer, I need to replay previously captured PCAP files, so that I can deterministically reproduce network scenarios for regression testing.

**Why this priority**: Deterministic replay is essential for regression testing and debugging intermittent issues.

**Independent Test**: Save captured packets to a PCAP file, then replay that file and verify packet contents match the original capture.

**Acceptance Scenarios**:

1. **Given** a PCAP file path, **When** I create a ReplaySession, **Then** packets are read sequentially from the file
2. **Given** a replay session, **When** I iterate through packets, **Then** each packet retains its original timestamp from the capture
3. **Given** a replay session with timing control, **When** I enable real-time replay, **Then** packets are replayed with original inter-packet delays
4. **Given** a large PCAP file (1GB+), **When** I replay it, **Then** memory usage remains bounded (streaming, not loading entire file)

---

### User Story 4 - PCAP File Writing (Priority: P2)

As a test automation engineer, I need to save captured packets to PCAP files, so that I can archive network traces for later analysis or share them with colleagues.

**Why this priority**: PCAP export enables offline analysis, forensics, and integration with other tools like Wireshark.

**Independent Test**: Capture packets and save to PCAP, then verify the file can be opened in Wireshark and contains correct packet data.

**Acceptance Scenarios**:

1. **Given** a capture session, **When** I call save_pcap(path), **Then** a valid PCAP file is created at the specified path
2. **Given** a PCAP writer, **When** I write packets, **Then** timestamps are preserved in nanosecond precision (if using pcapng format)
3. **Given** a PCAP file written by Wadjet-Link, **When** I open it in Wireshark, **Then** all packets are readable and display correct protocol information
4. **Given** a failed test case, **When** the test framework is configured for auto-save, **Then** packets are automatically saved to failure_captures/ directory

---

### User Story 5 - Hardware Timestamping (Priority: P3)

As a gPTP timing analyst, I need hardware-level timestamps with nanosecond precision, so that I can accurately measure time synchronization protocols without software-induced jitter.

**Why this priority**: Critical for gPTP/TSN analysis, but software timestamps are acceptable fallback for most use cases.

**Independent Test**: Capture packets on a NIC with hardware timestamping support and verify timestamps have nanosecond precision.

**Acceptance Scenarios**:

1. **Given** a NIC supporting hardware timestamping, **When** I enable it in CaptureOptions, **Then** packets receive hardware-generated timestamps
2. **Given** hardware timestamping is unavailable, **When** I start capture, **Then** software timestamps are used as fallback
3. **Given** timestamped packets, **When** I inspect timestamps, **Then** they are monotonically increasing within a capture session

### Edge Cases

- What happens when the network interface doesn't exist or is down?
- How does the system handle running out of disk space when writing large PCAP files?
- What if the kernel drops packets due to buffer overflow during extremely high packet rates?
- How are truncated PCAP files (corrupted mid-write) handled during replay?
- What happens when trying to capture on an interface without sufficient permissions (non-root user)?
- How does the system behave when multiple capture sessions try to use the same interface simultaneously?

## Requirements

### Functional Requirements

- **FR-001**: System MUST support packet capture using Linux AF_PACKET sockets with PF_PACKET family
- **FR-002**: System MUST provide an optional libpcap backend as an alternative to AF_PACKET
- **FR-003**: System MUST support zero-copy ring buffer capture using TPACKET_V2 and TPACKET_V3 mechanisms
- **FR-004**: System MUST apply BPF (Berkeley Packet Filter) expressions to filter captured packets
- **FR-005**: System MUST provide nanosecond-precision timestamps for captured packets
- **FR-006**: System MUST support hardware timestamps when available on the network interface
- **FR-007**: System MUST fall back to software timestamps when hardware timestamping is unavailable
- **FR-008**: System MUST provide PacketView class for immutable, zero-copy access to packet data
- **FR-009**: System MUST provide Packet class for mutable packet buffer operations
- **FR-010**: System MUST read PCAP files (.pcap format) for replay
- **FR-011**: System MUST read PCAPNG files (.pcapng format) with Enhanced Packet Block support
- **FR-012**: System MUST write PCAP files in both .pcap and .pcapng formats
- **FR-013**: System MUST preserve nanosecond timestamps when writing pcapng files
- **FR-014**: System MUST provide ReplaySession for deterministic packet replay from files
- **FR-015**: System MUST support real-time replay with original inter-packet timing delays
- **FR-016**: System MUST report capture statistics (packets captured, dropped, interface errors)
- **FR-017**: System MUST enumerate available network interfaces on the host
- **FR-018**: System MUST validate network interface names before starting capture
- **FR-019**: CaptureSession MUST be thread-safe for concurrent reads from multiple consumers
- **FR-020**: System MUST handle graceful shutdown of capture sessions without packet loss

### Key Entities

- **CaptureSession**: Manages live packet capture from a network interface using AF_PACKET or libpcap
- **ReplaySession**: Replays packets from PCAP/PCAPNG files with timing control
- **Packet**: Mutable packet buffer with timestamp and metadata
- **PacketView**: Immutable, zero-copy view into packet data
- **FrameFilter**: BPF expression wrapper for packet filtering
- **PcapReader**: Reads packets from PCAP/PCAPNG files
- **PcapWriter**: Writes packets to PCAP/PCAPNG files
- **DeviceEnumerator**: Lists available network interfaces
- **CaptureOptions**: Configuration for capture sessions (buffer size, timeout, promiscuous mode)
- **CaptureStatistics**: Counters for captured, dropped, and error packets

## Success Criteria

### Measurable Outcomes

- **SC-001**: System can capture packets at sustained rates of 10,000 packets/second with <1% packet drop rate on commodity hardware
- **SC-002**: Packet access using PacketView introduces zero memory copies (verified via memory profiling)
- **SC-003**: PCAP files written by Wadjet-Link are compatible with Wireshark 3.0+ and tcpdump
- **SC-004**: Replay of a 1GB PCAP file completes with memory usage <100MB (streaming, not buffering)
- **SC-005**: Timestamp precision is within 1 microsecond when using hardware timestamping
- **SC-006**: Software timestamps have resolution of at least 1 microsecond on modern Linux kernels
- **SC-007**: All capture and replay operations complete without memory leaks (verified by Valgrind/AddressSanitizer)
- **SC-008**: Capture can run continuously for 24+ hours without performance degradation or resource exhaustion
- **SC-009**: 100% of test cases pass on both TPACKET_V2 and TPACKET_V3 ring buffer implementations
- **SC-010**: 100% of test cases pass using both AF_PACKET and libpcap backends

## Assumptions

- Target platform is Linux with kernel 3.x or later (for TPACKET_V3 support)
- User has sufficient permissions (CAP_NET_RAW capability or root) to capture packets
- Network interfaces support promiscuous mode for passive monitoring
- For hardware timestamping, NIC firmware and driver support is available

## Dependencies

- **External**: Linux kernel packet capture APIs (AF_PACKET), libpcap (optional backend), libc standard library
- **Internal**: None (this is a foundational milestone)

## Out of Scope

- Windows or macOS packet capture support
- Packet injection or transmission (capture is read-only)
- Protocol decoding (this is pure I/O; decoding is Milestone 2)
- GUI for viewing captures (CLI and library only)
- Remote capture over network (capture is local only)
- Support for non-Ethernet link layers (e.g., Wi-Fi 802.11 management frames)

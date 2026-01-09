# Tasks: Protocol Completeness

**Branch**: `013-protocol-completeness`  
**Feature**: M13 - Protocol Completeness (NEXT MILESTONE - Foundation for all advanced features)  
**Input**: [spec.md](./spec.md), [plan.md](./plan.md)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: User story label (US1, US2, US3...) - only for user story phases
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and test infrastructure setup

- [ ] T001 Review existing protocol decoder implementations in src/protocols/ to understand current baseline
- [ ] T002 [P] Create test fixture directory structure pcap_samples/protocol-completeness/ for regression tests
- [ ] T003 [P] Update CMakeLists.txt to include new test files
- [ ] T004 [P] Setup fuzz/ directory structure for protocol-specific fuzzers (actual fuzz harnesses in T116-T120)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before user story implementation

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T005 Create include/wadjet/protocols/validation.hpp for cross-protocol validation framework
- [ ] T006 Create src/protocols/validation.cpp with ProtocolValidator base class
- [ ] T007 [P] Update include/wadjet/protocols/ethernet.hpp to add VLAN QinQ support (if not present)
- [ ] T008 [P] Create tests/protocols/test_protocol_completeness_base.cpp with shared test utilities

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - Complete IPv4 Header Parsing (Priority: P1) 🎯

**Goal**: Full IPv4 header parsing including options, fragmentation, ToS/DSCP, and checksum validation

**Independent Test**: Parse IPv4 packets with all header options and fragmented packets, verify reassembly

### Tests for User Story 1 (Write FIRST, ensure they FAIL before implementation)

- [ ] T009 [P] [US1] Create tests/protocols/test_ipv4_options.cpp with 12 tests for all IPv4 option types (Router Alert, Timestamp, Record Route, Source Route, NOP, EOL, etc.)
- [ ] T010 [P] [US1] Create tests/protocols/test_ipv4_fragmentation.cpp with 16 tests for fragmentation scenarios (simple fragments, out-of-order, overlapping, timeout, reassembly, fragmented packet with Router Alert option)
- [ ] T011 [P] [US1] Add IPv4 ToS/DSCP parsing tests to tests/protocols/test_ipv4.cpp (5 tests for QoS field extraction)
- [ ] T012 [P] [US1] Add IPv4 checksum validation tests to tests/protocols/test_ipv4.cpp (8 tests for valid/invalid/disabled checksum)
- [ ] T013 [P] [US1] Create pcap_samples/protocol-completeness/ipv4_fragmented.pcap with real fragmentation samples
- [ ] T014 [P] [US1] Create pcap_samples/protocol-completeness/ipv4_options.pcap with all option types

### Implementation for User Story 1

- [ ] T015 [P] [US1] Add Ipv4Options struct to include/wadjet/protocols/ipv4.hpp with all option types
- [ ] T016 [P] [US1] Add Ipv4Fragment struct to include/wadjet/protocols/ipv4.hpp for reassembly state
- [ ] T017 [US1] Implement parseIpv4Options() in src/protocols/ipv4.cpp with TLV parsing logic
- [ ] T018 [US1] Implement Ipv4FragmentReassembler class in src/protocols/ipv4.cpp with 60s timeout
- [ ] T019 [US1] Add ToS/DSCP field extraction to Ipv4Decoder::decode() in src/protocols/ipv4.cpp
- [ ] T020 [US1] Add IPv4 checksum validation to Ipv4Decoder with enable/disable flag in src/protocols/ipv4.cpp
- [ ] T021 [US1] Update Ipv4Header struct in include/wadjet/protocols/ipv4.hpp to include options, ToS fields
- [ ] T022 [US1] Update Ipv4Decoder to detect and report IPv4 header anomalies (invalid version, bad header length)

**Checkpoint**: IPv4 parsing complete - all options, fragmentation, ToS/DSCP working

---

## Phase 4: User Story 2 - Complete TCP State Machine (Priority: P1) 🎯

**Goal**: Full TCP connection tracking with state machine, options parsing, and retransmission detection

**Independent Test**: Capture TCP connection lifecycle and verify all state transitions

### Tests for User Story 2 (Write FIRST)

- [ ] T023 [P] [US2] Create tests/protocols/test_tcp_state.cpp with 20 tests for all TCP states (CLOSED, SYN_SENT, ESTABLISHED, FIN_WAIT, TIME_WAIT, etc.)
- [ ] T024 [P] [US2] Create tests/protocols/test_tcp_options.cpp with 15 tests for all TCP options (MSS, Window Scale, SACK, Timestamps, NOP, EOL)
- [ ] T025 [P] [US2] Add TCP retransmission detection tests to tests/protocols/test_tcp.cpp (10 tests for duplicate packets, sequence number validation)
- [ ] T026 [P] [US2] Create pcap_samples/protocol-completeness/tcp_handshake.pcap with complete 3-way handshake
- [ ] T027 [P] [US2] Create pcap_samples/protocol-completeness/tcp_teardown.pcap with FIN/ACK teardown

### Implementation for User Story 2

- [ ] T028 [P] [US2] Add TcpState enum to include/wadjet/protocols/tcp.hpp with all 11 states
- [ ] T029 [P] [US2] Add TcpConnection struct to include/wadjet/protocols/tcp.hpp with state, sequence tracking
- [ ] T030 [P] [US2] Add TcpOptions struct to include/wadjet/protocols/tcp.hpp for all option types
- [ ] T031 [US2] Create TcpConnectionTracker class in src/protocols/tcp.cpp with hash map (5-tuple key)
- [ ] T032 [US2] Implement TCP state machine logic in TcpConnectionTracker (state transitions per RFC 793)
- [ ] T033 [US2] Implement parseTcpOptions() in src/protocols/tcp.cpp for all option types
- [ ] T034 [US2] Add retransmission detection logic in TcpConnectionTracker using sequence number tracking
- [ ] T035 [US2] Update TcpDecoder to use TcpConnectionTracker for stateful analysis in src/protocols/tcp.cpp
- [ ] T036 [US2] Add TCP window size and scaling factor tracking to TcpConnection

**Checkpoint**: TCP connection tracking complete - full state machine operational

---

## Phase 5: User Story 3 - UDP Checksum Validation (Priority: P2)

**Goal**: Complete UDP checksum validation with enable/disable mode

**Independent Test**: Validate UDP checksums on captured traffic with intentional corruption

### Tests for User Story 3 (Write FIRST)

- [ ] T037 [P] [US3] Create tests/protocols/test_udp_checksum.cpp with 10 tests (valid checksum, invalid checksum, zero checksum for IPv4, checksum disabled mode)
- [ ] T038 [P] [US3] Create pcap_samples/protocol-completeness/udp_checksum_valid.pcap
- [ ] T039 [P] [US3] Create pcap_samples/protocol-completeness/udp_checksum_invalid.pcap (synthetic corrupted packets)

### Implementation for User Story 3

- [ ] T040 [P] [US3] Add UdpChecksumValidator class to include/wadjet/protocols/udp.hpp
- [ ] T041 [US3] Implement UDP checksum calculation in src/protocols/udp.cpp using IPv4 pseudo-header
- [ ] T042 [US3] Add checksum validation flag to UdpDecoder::decode() in src/protocols/udp.cpp
- [ ] T043 [US3] Handle UDP zero checksum case (allowed for IPv4, forbidden for IPv6) in src/protocols/udp.cpp
- [ ] T044 [US3] Add checksum validation result to UdpHeader struct in include/wadjet/protocols/udp.hpp

**Checkpoint**: UDP checksum validation working with all edge cases

---

## Phase 6: User Story 4 - SOME/IP Message Segmentation (Priority: P1) 🎯

**Goal**: SOME/IP-TP (Transport Protocol) support for large message segmentation

**Independent Test**: Decode segmented SOME/IP-TP messages and verify reassembly

### Tests for User Story 4 (Write FIRST)

- [ ] T045 [P] [US4] Create tests/protocols/test_someip_tp.cpp with 25 tests (TP header parsing, multi-segment reassembly, more_segments flag, offset validation, timeout handling)
- [ ] T046 [P] [US4] Create pcap_samples/protocol-completeness/someip_tp_large.pcap with real TP segmented messages
- [ ] T047 [P] [US4] Add SOME/IP message length validation tests to tests/protocols/test_someip.cpp (5 tests)

### Implementation for User Story 4

- [ ] T048 [P] [US4] Add SomeipTpMessage struct to include/wadjet/protocols/someip.hpp for reassembly state
- [ ] T049 [P] [US4] Add TP message type to SomeipMessageType enum in include/wadjet/protocols/someip.hpp
- [ ] T050 [US4] Create SomeipTpReassembler class in src/protocols/someip.cpp with segment buffering (64KB max buffer per AUTOSAR limit)
- [ ] T051 [US4] Implement TP header parsing (offset, more_segments flag) in src/protocols/someip.cpp
- [ ] T052 [US4] Add 5-second timeout for incomplete TP messages in SomeipTpReassembler
- [ ] T053 [US4] Update SomeipDecoder to detect TP messages and delegate to reassembler in src/protocols/someip.cpp
- [ ] T054 [US4] Add SOME/IP message length field validation in SomeipDecoder

**Checkpoint**: SOME/IP-TP segmentation complete - large messages reassemble correctly

---

## Phase 7: User Story 5 - SOME/IP Service Discovery Entry Arrays (Priority: P2)

**Goal**: Complete SD entry/option parsing for all types with proper linking

**Independent Test**: Parse all SD entry types and option configurations

### Tests for User Story 5 (Write FIRST)

- [ ] T055 [P] [US5] Create tests/protocols/test_someip_sd_entries.cpp with 30 tests (all entry types: FindService, OfferService, SubscribeEventgroup, StopSubscribe; entry arrays; TTL handling)
- [ ] T056 [P] [US5] Add SD option array tests to tests/protocols/test_someip_sd.cpp (15 tests for IPv4 Endpoint, Multicast, SD Endpoint, Configuration, LoadBalancing options)
- [ ] T057 [P] [US5] Create pcap_samples/protocol-completeness/someip_sd_complex.pcap with multi-entry/option messages

### Implementation for User Story 5

- [ ] T058 [P] [US5] Add all SD entry types to include/wadjet/protocols/someip_sd.hpp (FindService, OfferService, Subscribe, StopSubscribe)
- [ ] T059 [P] [US5] Add all SD option types to include/wadjet/protocols/someip_sd.hpp (Configuration, LoadBalancing, IPv4Endpoint, IPv4Multicast, IPv4SdEndpoint)
- [ ] T060 [P] [US5] Add SdEntryArray struct to include/wadjet/protocols/someip_sd.hpp
- [ ] T061 [P] [US5] Add SdOptionArray struct to include/wadjet/protocols/someip_sd.hpp
- [ ] T062 [US5] Implement parseSdEntries() in src/protocols/someip_sd.cpp with entry count validation
- [ ] T063 [US5] Implement parseSdOptions() in src/protocols/someip_sd.cpp with option linking logic (Index1, Index2, NumOpt1, NumOpt2)
- [ ] T064 [US5] Add SD Reboot and Unicast flag parsing in src/protocols/someip_sd.cpp
- [ ] T065 [US5] Implement SD TTL field handling (0 = StopOffer/Unsubscribe, 0xFFFFFF = infinite) in src/protocols/someip_sd.cpp
- [ ] T066 [US5] Update SomeipSdDecoder to validate entry/option counts match in src/protocols/someip_sd.cpp

**Checkpoint**: SOME/IP-SD entry/option parsing complete - all types supported

---

## Phase 8: User Story 6 - DoIP Diagnostic Power Mode (Priority: P2)

**Goal**: DoIP diagnostic power mode handling for ECU wake-up and sleep scenarios

**Independent Test**: Monitor DoIP diagnostic power mode transitions

### Tests for User Story 6 (Write FIRST)

- [ ] T067 [P] [US6] Create tests/protocols/test_doip_power.cpp with 20 tests (power mode messages 0x4003/0x4004, entity status 0x4001/0x4002, NACK codes, alive check, power mode transitions)
- [ ] T068 [P] [US6] Create pcap_samples/protocol-completeness/doip_power_mode.pcap with power mode state transitions

### Implementation for User Story 6

- [ ] T069 [P] [US6] Add DoipPowerMode enum to include/wadjet/protocols/doip.hpp (Ready, NotReady, NotSupported)
- [ ] T070 [P] [US6] Add power mode message types (0x4003, 0x4004) to DoipPayloadType enum in include/wadjet/protocols/doip.hpp
- [ ] T071 [P] [US6] Add entity status message types (0x4001, 0x4002) to include/wadjet/protocols/doip.hpp
- [ ] T072 [P] [US6] Add DoIP NACK codes enum to include/wadjet/protocols/doip.hpp
- [ ] T073 [US6] Implement parseDiagnosticPowerMode() in src/protocols/doip.cpp
- [ ] T074 [US6] Implement parseEntityStatus() in src/protocols/doip.cpp
- [ ] T075 [US6] Implement parseDoipNack() in src/protocols/doip.cpp
- [ ] T076 [US6] Implement parseAliveCheck() (request/response) in src/protocols/doip.cpp
- [ ] T077 [US6] Update DoipDecoder to handle all new message types in src/protocols/doip.cpp
- [ ] T078 [US6] Add DoIP payload length validation in src/protocols/doip.cpp

**Checkpoint**: DoIP power mode and entity status messages fully supported

---

## Phase 9: User Story 7 - UDS Negative Response Code Handling (Priority: P1) 🎯

**Goal**: Comprehensive NRC handling for all UDS diagnostic errors

**Independent Test**: Trigger all UDS NRCs and verify parsing with descriptions

### Tests for User Story 7 (Write FIRST)

- [ ] T079 [P] [US7] Create tests/protocols/test_uds_nrc.cpp with 30 tests (all NRC codes 0x10-0x93, sub-function extraction, temporary vs permanent classification, service-specific NRCs)
- [ ] T080 [P] [US7] Create pcap_samples/protocol-completeness/uds_all_nrcs.pcap with samples of all 50+ NRC codes

### Implementation for User Story 7

- [ ] T081 [P] [US7] Add complete UdsNrc enum to include/wadjet/protocols/uds.hpp with all codes (0x10-0x93)
- [ ] T082 [P] [US7] Add UdsNegativeResponse struct to include/wadjet/protocols/uds.hpp with service_id, nrc, sub_function fields
- [ ] T083 [US7] Create NRC description table in src/protocols/uds_decoder.cpp mapping codes to human-readable strings
- [ ] T084 [US7] Implement classifyNrc() in src/protocols/uds_decoder.cpp (temporary vs permanent error)
- [ ] T085 [US7] Implement parseNegativeResponse() in src/protocols/uds_decoder.cpp (0x7F SID NRC format)
- [ ] T086 [US7] Add service-specific NRC interpretation logic in src/protocols/uds_decoder.cpp
- [ ] T087 [US7] Add sub-function byte extraction for NRCs in src/protocols/uds_decoder.cpp
- [ ] T088 [US7] Add positive response suppression bit handling in src/protocols/uds_decoder.cpp
- [ ] T089 [US7] Update UdsDecoder to use new NRC parsing in src/protocols/uds_decoder.cpp

**Checkpoint**: UDS NRC handling complete - all 50+ codes supported with descriptions

---

## Phase 10: User Story 8 - gPTP Follow_Up Information TLV (Priority: P2)

**Goal**: Complete gPTP TLV parsing including rate ratio and organization extensions

**Independent Test**: Parse gPTP Follow_Up messages with all TLV types

### Tests for User Story 8 (Write FIRST)

- [ ] T090 [P] [US8] Create tests/protocols/test_gptp_tlv.cpp with 15 tests (Follow_Up Info TLV 0x0003, rate ratio extraction, GM time base indicator, Organization Extension TLV, unknown TLV handling, malformed TLV detection)
- [ ] T091 [P] [US8] Create pcap_samples/protocol-completeness/gptp_tlv_rich.pcap with all TLV types

### Implementation for User Story 8

- [ ] T092 [P] [US8] Add GptpTlvType enum to include/wadjet/protocols/gptp.hpp (FollowUpInfo = 0x0003, OrgExtension = 0x0003, etc.)
- [ ] T093 [P] [US8] Add FollowUpInformationTlv struct to include/wadjet/protocols/gptp.hpp with rate_ratio, gm_time_base_indicator fields
- [ ] T094 [P] [US8] Add OrganizationExtensionTlv struct to include/wadjet/protocols/gptp.hpp with organization_id, sub_type fields
- [ ] T095 [P] [US8] Add GptpTlv variant type to include/wadjet/protocols/gptp.hpp
- [ ] T096 [US8] Implement parseGptpTlv() in src/protocols/gptp_decoder.cpp with TLV length validation
- [ ] T097 [US8] Implement parseFollowUpInfoTlv() in src/protocols/gptp_decoder.cpp
- [ ] T098 [US8] Implement parseOrganizationExtensionTlv() in src/protocols/gptp_decoder.cpp
- [ ] T099 [US8] Add unknown TLV handling with graceful fallback in src/protocols/gptp_decoder.cpp
- [ ] T100 [US8] Update GptpDecoder to parse TLV arrays in Announce and Follow_Up messages in src/protocols/gptp_decoder.cpp

**Checkpoint**: gPTP TLV parsing complete - all types supported with validation

---

## Phase 11: Cross-Protocol Validation & Integration

**Purpose**: Multi-layer validation and comprehensive testing

### Cross-Protocol Validation Implementation

- [ ] T101 [P] Implement ProtocolValidator::validateLayering() in src/protocols/validation.cpp (Ethernet → IPv4 → UDP/TCP → Application)
- [ ] T102 [P] Implement ProtocolValidator::validateLengths() in src/protocols/validation.cpp (detect inconsistencies across layers)
- [ ] T103 [P] Implement ProtocolValidator::validateChecksums() in src/protocols/validation.cpp (IPv4, UDP, TCP chain)
- [ ] T104 Add strict/lenient error handling modes to ProtocolValidator in include/wadjet/protocols/validation.hpp
- [ ] T105 Create tests/protocols/test_protocol_validation.cpp with 20 tests for cross-layer validation scenarios

### Language Bindings Updates

- [ ] T106 [P] Update bindings/python/src/protocol_bindings.cpp with new IPv4 options, TCP state, UDP checksum, TP, NRC, TLV types
- [ ] T107 [P] Update bindings/c/include/wadjet_c.h with C ABI for new protocol features
- [ ] T108 [P] Update bindings/c/src/wadjet_c.cpp with implementation for new C API functions
- [ ] T109 [P] Update bindings/rust/wadjet/src/protocols.rs with Rust wrappers for new features

### Integration Testing

- [ ] T110 [P] Create tests/integration/test_protocol_completeness_integration.cpp with 30 end-to-end scenarios
- [ ] T111 [P] Test IPv4 fragmentation with SOME/IP payload (multi-layer scenario)
- [ ] T112 [P] Test TCP connection tracking with DoIP diagnostic sessions (stateful analysis)
- [ ] T113 [P] Test SOME/IP-SD over UDP with checksum validation (full stack)
- [ ] T114 [P] Test UDS over DoIP over TCP with connection state tracking
- [ ] T115 Test complete protocol stack decode: Ethernet → VLAN → IPv4 → TCP → DoIP → UDS

### Fuzz Testing

- [ ] T116 [P] Create fuzz/fuzz_ipv4_options.cpp for IPv4 option fuzzing (1M+ iterations)
- [ ] T117 [P] Create fuzz/fuzz_tcp_options.cpp for TCP option fuzzing (1M+ iterations)
- [ ] T118 [P] Create fuzz/fuzz_someip_tp.cpp for SOME/IP-TP fuzzing (1M+ iterations)
- [ ] T119 [P] Create fuzz/fuzz_someip_sd_entries.cpp for SD entry/option fuzzing (1M+ iterations)
- [ ] T120 [P] Create fuzz/fuzz_gptp_tlv.cpp for gPTP TLV fuzzing (1M+ iterations)
- [ ] T121 Run all fuzz harnesses with AddressSanitizer for 24+ hours
- [ ] T122 Fix any crashes or memory issues found by fuzzing

### Performance & Regression Testing

- [ ] T123 Create benchmark tests comparing M13 performance to M11 baseline using pcap_samples/ from M0-M11 (target ≤5% overhead)
- [ ] T124 Profile packet decode path with perf/Instruments to identify hotspots
- [ ] T125 Optimize any decode paths that show >5% performance degradation
- [ ] T126 Run full regression test suite (all pcap_samples/) to ensure no breakage

---

## Phase 12: Polish & Documentation

**Purpose**: Finalize implementation with documentation and examples

- [ ] T127 [P] [US1] Update docs/protocols/ipv4.md with options, fragmentation, ToS/DSCP documentation
- [ ] T128 [P] [US2] Update docs/protocols/tcp.md with state machine, connection tracking documentation
- [ ] T129 [P] [US4] Update docs/protocols/someip.md with TP segmentation documentation
- [ ] T130 [P] [US5] Update docs/protocols/someip_sd.md with complete entry/option documentation
- [ ] T131 [P] [US6] Update docs/protocols/doip.md with diagnostic power mode documentation
- [ ] T132 [P] [US7] Update docs/protocols/uds.md with complete NRC reference table
- [ ] T133 [P] [US8] Update docs/protocols/gptp.md with TLV documentation
- [ ] T134 [P] Create examples/protocol_validation.cpp demonstrating cross-layer validation
- [ ] T135 [P] Update README.md with M13 completion status
- [ ] T136 [P] Add Doxygen comments to all new public APIs
- [ ] T137 Update CHANGELOG.md with M13 Protocol Completeness milestone
- [ ] T138 Create release notes documenting all new features and test results

---

## Summary Statistics

**Total Tasks**: 138  
**Parallel Opportunities**: 95 tasks marked [P]  
**Test Tasks**: 40+ (T009-T091 test creation)  
**Implementation Tasks**: 75+ (T015-T100 core implementation)  
**Integration Tasks**: 23 (T101-T126)

**Test Breakdown by Protocol**:
- IPv4: 35 tests (T009-T014 fixtures + implementation)
- TCP: 45 tests (T023-T027 fixtures + implementation)
- UDP: 10 tests (T037-T039 fixtures + implementation)
- SOME/IP: 30 tests (T045-T047 fixtures + implementation)
- SOME/IP-SD: 45 tests (T055-T057 fixtures + implementation)
- DoIP: 20 tests (T067-T068 fixtures + implementation)
- UDS: 30 tests (T079-T080 fixtures + implementation)
- gPTP: 15 tests (T090-T091 fixtures + implementation)
- Cross-validation: 20 tests (T105)
- Integration: 30 tests (T110-T115)
- **Total**: 280+ tests (exceeds 230+ target)

**Estimated Timeline** (based on plan.md):
- Phase 1-2 (Setup): 3 days
- Phase 3 (US1 - IPv4): 1 week
- Phase 4 (US2 - TCP): 1.5 weeks
- Phase 5 (US3 - UDP): 0.5 weeks
- Phase 6 (US4 - SOME/IP TP): 1 week
- Phase 7 (US5 - SD): 1.5 weeks
- Phase 8 (US6 - DoIP): 1 week
- Phase 9 (US7 - UDS NRC): 1 week
- Phase 10 (US8 - gPTP): 0.5 weeks
- Phase 11 (Integration): 1 week
- Phase 12 (Polish): 1 week
- **Total**: ~10 weeks

**Dependencies & Parallelization**:
- Phases 3-10 (User Stories) can run partially in parallel (different protocols)
- Within each phase, test creation (marked [P]) can happen in parallel
- All [P] tasks within a user story can execute concurrently
- Phase 11 requires completion of Phases 3-10
- Phase 12 can start after Phase 11

**MVP Scope** (Minimum Viable for Production):
- Phase 3 (US1 - IPv4) - P1 priority, critical for all upper layers
- Phase 4 (US2 - TCP) - P1 priority, required for DoIP/UDS
- Phase 6 (US4 - SOME/IP TP) - P1 priority, needed for large messages
- Phase 9 (US7 - UDS NRC) - P1 priority, diagnostic troubleshooting essential
- Phase 11 (Integration & Fuzz) - Quality gate

**Recommended Implementation Order**:
1. Complete all P1 user stories first (US1, US2, US4, US7) - 5 weeks
2. Then P2 stories (US3, US5, US6, US8) - 4 weeks
3. Finally integration and polish - 2 weeks

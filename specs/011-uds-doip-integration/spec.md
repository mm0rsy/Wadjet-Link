# Feature Specification: UDS over DoIP Integration

**Feature Branch**: `milestone/011-uds-doip-integration`  
**Created**: 2026-01-09  
**Status**: ✅ Complete (Implemented)  
**Milestone**: M11 - UDS over DoIP Integration

## Overview

Implement complete UDS-over-DoIP diagnostic stack with session management, combining ISO 14229 (UDS) with ISO 13400 (DoIP) for full Ethernet-based diagnostics. This milestone integrates the UDS decoder (Milestone 9) with the existing DoIP decoder for complete diagnostic session handling with multi-ECU support.

## User Scenarios & Testing

### User Story 1 - Diagnostic Session Tracking (Priority: P1)

As a diagnostic engineer, I need to track UDS session states across multiple ECUs, so that I can verify session lifecycle compliance.

**Why this priority**: Session management is fundamental to UDS diagnostics - incorrect state transitions indicate protocol violations.

**Independent Test**: Capture diagnostic traffic and verify session state transitions.

**Acceptance Scenarios**:

1. **Given** DiagnosticSessionControl request, **When** processed, **Then** session state transitions from Default to Programming/Extended
2. **Given** active diagnostic session, **When** S3 timeout expires, **Then** session reverts to Default
3. **Given** TesterPresent messages, **When** received periodically, **Then** session remains active
4. **Given** multiple ECUs, **When** tracked, **Then** each ECU has independent session state
5. **Given** session timeout, **When** detected, **Then** SessionTimeout event is emitted

### User Story 2 - Security Access Monitoring (Priority: P1)

As a security analyst, I need to monitor SecurityAccess sequences, so that I can detect unauthorized access attempts.

**Why this priority**: Security access controls ECU reprogramming - tracking prevents unauthorized modifications.

**Independent Test**: Capture SecurityAccess seed/key exchange and verify sequence.

**Acceptance Scenarios**:

1. **Given** SecurityAccess seed request, **When** received, **Then** seed value is recorded
2. **Given** SecurityAccess key response, **When** received, **Then** security level is unlocked
3. **Given** invalid key, **When** provided, **Then** SecurityAccessDenied event is emitted
4. **Given** security level active, **When** session timeout occurs, **Then** security level is locked
5. **Given** seed/key sequence, **When** analyzed, **Then** timing violations are detected

### User Story 3 - Request/Response Correlation (Priority: P1)

As a test engineer, I need request/response correlation, so that I can measure diagnostic service response times.

**Why this priority**: Response time validation is critical for P2/P2* timing compliance.

**Independent Test**: Send diagnostic request and measure response correlation.

**Acceptance Scenarios**:

1. **Given** UDS request, **When** sent, **Then** request is recorded with timestamp
2. **Given** matching response, **When** received, **Then** request/response pair is created with latency
3. **Given** ResponsePending (0x78) NRC, **When** received, **Then** P2* timeout is applied
4. **Given** response timeout, **When** P2/P2* exceeded, **Then** timeout is reported
5. **Given** correlation statistics, **When** queried, **Then** match rate and timeout rate are available

### User Story 4 - Flash Programming Sequence Tracking (Priority: P2)

As a flash tool developer, I need flash sequence tracking, so that I can validate download/upload progress.

**Why this priority**: Flash programming requires multi-step sequences - tracking ensures completeness.

**Independent Test**: Capture RequestDownload sequence and verify progress tracking.

**Acceptance Scenarios**:

1. **Given** RequestDownload service, **When** received, **Then** flash sequence is initiated
2. **Given** TransferData services, **When** received, **Then** block counter and progress are tracked
3. **Given** TransferExit service, **When** received, **Then** sequence is completed
4. **Given** incomplete sequence, **When** timeout occurs, **Then** FlashSequenceAborted event is emitted
5. **Given** flash progress, **When** queried, **Then** bytes transferred and percentage are available

### User Story 5 - DTC Management (Priority: P2)

As a diagnostic technician, I need DTC tracking, so that I can monitor fault history across sessions.

**Why this priority**: DTC history provides fault diagnostics - tracking enables trend analysis.

**Independent Test**: Capture ReadDTCInformation and verify DTC storage.

**Acceptance Scenarios**:

1. **Given** ReadDTCInformation response, **When** parsed, **Then** DTCs are extracted and stored
2. **Given** DTC status change, **When** detected, **Then** DTCStatusChanged event is emitted
3. **Given** ClearDiagnosticInformation, **When** received, **Then** DTCs are marked cleared with timestamp
4. **Given** DTC filters (severity, status), **When** applied, **Then** filtered DTC list is returned
5. **Given** DTC statistics, **When** queried, **Then** active/confirmed/pending/cleared counts are available

## Edge Cases

- What happens when multiple testers access same ECU simultaneously (address conflict)?
- How are interleaved request/response sequences handled (out-of-order packets)?
- What if TesterPresent is sent while flash programming is active?
- How does system handle negative responses for unknown service IDs?
- What happens when DoIP routing activation fails during active session?

## Requirements

### Functional Requirements

#### Session Management

- **FR-001**: System MUST track diagnostic session state per ECU (Default, Programming, Extended, SafetySystem)
- **FR-002**: System MUST detect DiagnosticSessionControl (0x10) service and update session state
- **FR-003**: System MUST implement S3 timeout detection (default 5000ms)
- **FR-004**: System MUST reset S3 timer on TesterPresent (0x3E) reception
- **FR-005**: System MUST support configurable P2 timeout (default 50ms) and P2* timeout (default 5000ms)
- **FR-006**: System MUST emit SessionStarted, SessionChanged, SessionTimeout, SessionEnded events
- **FR-007**: System MUST track routing activation state per DoIP connection

#### Security Access

- **FR-008**: System MUST detect SecurityAccess (0x27) seed requests and key responses
- **FR-009**: System MUST track security level per ECU (locked, level 1-N)
- **FR-010**: System MUST correlate seed/key pairs for validation (if keys provided)
- **FR-011**: System MUST emit SecurityAccessGranted and SecurityAccessDenied events
- **FR-012**: System MUST lock security level on session timeout
- **FR-013**: System MUST support multiple security levels per ECU

#### Request/Response Correlation

- **FR-014**: System MUST match UDS requests to responses by source/target address and service ID
- **FR-015**: System MUST calculate response latency (timestamp difference)
- **FR-016**: System MUST handle ResponsePending (0x78) by extending timeout to P2*
- **FR-017**: System MUST detect timeouts when P2/P2* exceeded
- **FR-018**: System MUST track correlation statistics (match rate, timeout rate, unmatched count)
- **FR-019**: System MUST support configurable correlation window (default 10 seconds)

#### Flash Programming Tracking

- **FR-020**: System MUST detect RequestDownload (0x34) and RequestUpload (0x35) to initiate flash sequence
- **FR-021**: System MUST track TransferData (0x36) blocks with sequence counter validation
- **FR-022**: System MUST detect RequestTransferExit (0x37) to complete flash sequence
- **FR-023**: System MUST calculate flash progress (bytes transferred, percentage)
- **FR-024**: System MUST detect sequence errors (missing blocks, counter mismatches)
- **FR-025**: System MUST emit FlashSequenceStarted, FlashBlockReceived, FlashSequenceCompleted, FlashSequenceAborted events

#### DTC Management

- **FR-026**: System MUST parse ReadDTCInformation (0x19) responses and extract DTCs
- **FR-027**: System MUST store DTC code, status byte, severity, and occurrence count
- **FR-028**: System MUST detect ClearDiagnosticInformation (0x14) and mark DTCs as cleared
- **FR-029**: System MUST track DTC history (first seen, last seen, cleared timestamp)
- **FR-030**: System MUST support DTC filtering by severity, status, and mask
- **FR-031**: System MUST emit DTCDetected, DTCStatusChanged, DTCCleared events
- **FR-032**: System MUST provide DTC statistics (active, confirmed, pending, cleared counts)

#### Multi-ECU Support

- **FR-033**: System MUST track up to 256 ECUs simultaneously (configurable)
- **FR-034**: System MUST use logical address (uint16_t) as ECU identifier
- **FR-035**: System MUST maintain independent state per ECU (session, security, flash, DTCs)
- **FR-036**: System MUST provide API to query tracked ECU list
- **FR-037**: System MUST support gateway address tracking for routing analysis

### Key Entities

- **DiagnosticSessionManager**: Main session and state tracker
- **DiagnosticSessionState**: Per-ECU state (session type, security level, timing)
- **RequestCorrelator**: Request/response matching with statistics
- **RequestResponsePair**: Correlated request/response with latency
- **FlashSequenceTracker**: Flash programming progress tracker
- **FlashSequence**: Download/upload sequence state
- **DTCManager**: Diagnostic Trouble Code storage and filtering
- **DTCRecord**: Individual DTC with status and history
- **UdsOverDoipDecoder**: Combined UDS+DoIP decoder
- **DiagnosticTiming**: ISO 14229 timing parameters (P2, P2*, S3)

## Success Criteria

### Measurable Outcomes

- **SC-001**: Session state transitions correctly tracked for all 4 session types across multiple ECUs
- **SC-002**: Security access sequences correctly identified with seed/key correlation
- **SC-003**: Request/response correlation achieves ≥95% match rate on clean captures
- **SC-004**: P2/P2* timeouts correctly detected with ≤10ms accuracy
- **SC-005**: Flash programming sequences tracked with accurate progress percentage (±0.1%)
- **SC-006**: DTC parsing successfully extracts all DTCs from ReadDTCInformation responses
- **SC-007**: All 22 diagnostic events correctly emitted with accurate state transitions
- **SC-008**: Multi-ECU tracking supports 256 ECUs with independent state management
- **SC-009**: Correlation statistics provide accurate match rate and timeout rate
- **SC-010**: All language bindings (Python, C, Rust) expose complete diagnostic API

## Assumptions

- DoIP routing activation precedes UDS diagnostic services
- Tester addresses are unique per network
- DoIP transport layer provides reliable delivery (TCP-based)
- Timing parameters follow ISO 14229-2 defaults unless configured
- DTCs follow ISO 14229-1 format (3-byte DTC code + status byte)

## Dependencies

- **External**: None (uses existing C++ standard library)
- **Internal**: M2 (Protocol Decoders for Ethernet/IPv4/TCP/UDP), M9 (UDS decoder), DoIP decoder from M2

## Out of Scope

- Active diagnostic request generation (passive analysis only)
- Seed/key algorithm implementation (correlation only)
- ODX database integration (covered in M16)
- Flash file parsing (A2L/HEX covered in M20)
- CAN-based UDS (focus on DoIP/Ethernet only)

## Implementation Notes

### Recommended Approach

**Phase 1 - Core Data Structures** (1 week)
- DiagnosticSessionState struct
- DiagnosticEvent enum (22 events)
- DiagnosticTiming configuration
- ECU tracking data structures

**Phase 2 - Session Manager** (2 weeks)
- DiagnosticSessionManager class
- Session state machine (Default, Programming, Extended, SafetySystem)
- S3 timeout detection
- TesterPresent handling
- Event emission
- Tests: 12 session management tests

**Phase 3 - Request/Response Correlation** (1 week)
- RequestCorrelator class
- Request recording with timestamps
- Response matching by address and service ID
- ResponsePending (0x78) handling
- Timeout detection
- Statistics tracking

**Phase 4 - Flash Sequence Tracker** (1 week)
- FlashSequenceTracker class
- RequestDownload/Upload detection
- TransferData block tracking
- Progress calculation
- Sequence validation
- Tests: 16 flash sequence tests

**Phase 5 - DTC Manager** (1 week)
- DTCManager class
- ReadDTCInformation parsing
- DTC storage and history
- ClearDiagnosticInformation handling
- Filtering and statistics
- Tests: 13 DTC manager tests

**Phase 6 - Language Bindings** (1 week)
- Python bindings (diagnostic_bindings.cpp)
- C ABI layer (wadjet_c.h, wadjet_c.cpp)
- Rust bindings (diagnostic.rs)
- Type stubs and documentation

**Total Duration**: ~7 weeks

### Architecture Diagram

```text
┌─────────────────────────────────────────────────────────────────┐
│                 UDS over DoIP Stack                             │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │               Diagnostic Session Manager                  │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │  Session State  │  Security State │  Timing State   │ │   │
│  │  │  - Default      │  - Locked       │  - P2 timer     │ │   │
│  │  │  - Programming  │  - Level 1-N    │  - P2* timer    │ │   │
│  │  │  - Extended     │  - Seeds/Keys   │  - S3 timer     │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Request/Response Correlator                 │   │
│  │  - Source/Target address matching                        │   │
│  │  - Service ID correlation                                │   │
│  │  - ResponsePending (0x78) handling                       │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│              ┌───────────────┴───────────────┐                  │
│              ▼                               ▼                  │
│  ┌─────────────────────┐        ┌─────────────────────┐         │
│  │   UDS Decoder       │        │   DoIP Transport    │         │
│  │   (Milestone 9)     │        │   (Existing)        │         │
│  │   - Service parsing │        │   - Routing         │         │
│  │   - NRC handling    │        │   - Vehicle ID      │         │
│  └─────────────────────┘        └─────────────────────┘         │
│                              │                                  │
│                              ▼                                  │
│              ┌───────────────────────────┐                      │
│              │       TCP/IP Stack        │                      │
│              │     Port 13400 (DoIP)     │                      │
│              └───────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

### API Examples

**C++ API:**

```cpp
#include <wadjet/protocols/diagnostic.hpp>

using namespace wadjet::protocols::diagnostic;

// Create session manager with custom timing
DiagnosticOptions opts;
opts.p2_timeout_ms = 50;
opts.p2_star_timeout_ms = 5000;
opts.s3_timeout_ms = 5000;
opts.enable_correlation = true;
opts.enable_timeout_detection = true;

DiagnosticSessionManager manager(opts);

// Register event callback
manager.on_event([](DiagnosticEvent event, 
                    const DiagnosticSessionState& state,
                    const std::optional<RequestResponsePair>& pair) {
    switch (event) {
        case DiagnosticEvent::SessionStarted:
            std::cout << "Session started on ECU 0x" << std::hex << state.tester_address << "\n";
            break;
        case DiagnosticEvent::SecurityAccessGranted:
            std::cout << "Security level " << state.security_level << " granted\n";
            break;
        case DiagnosticEvent::FlashBlockReceived:
            if (auto seq = manager.get_flash_sequence(state.tester_address)) {
                std::cout << "Flash progress: " << seq->progress_percentage() << "%\n";
            }
            break;
        default:
            break;
    }
});

// Process captured DoIP packets
for (const auto& packet : captured_packets) {
    manager.process_doip_raw(packet.data(), packet.size());
}

// Check correlation statistics
auto stats = manager.correlator().statistics();
std::cout << "Match rate: " << stats.match_rate() * 100 << "%\n";
std::cout << "Timeout rate: " << stats.timeout_rate() * 100 << "%\n";

// Get DTCs for specific ECU
auto dtcs = manager.dtc_manager().get_dtcs(0x1234);
for (const auto& dtc : dtcs) {
    std::cout << "DTC 0x" << std::hex << dtc.code << " - " 
              << dtc_severity_string(dtc.severity) << "\n";
}
```

**Python API:**

```python
import wadjet

# Create session manager
manager = wadjet.DiagnosticSessionManager()

# Register event callback
def on_event(event, state, pair):
    if event == wadjet.DiagnosticEvent.SessionStarted:
        print(f"Session started on ECU 0x{state.tester_address:04X}")
    elif event == wadjet.DiagnosticEvent.FlashBlockReceived:
        seq = manager.get_flash_sequence(state.tester_address)
        if seq:
            print(f"Flash progress: {seq.progress_percentage:.1f}%")

manager.on_event(on_event)

# Process captured DoIP packets
for packet in captured_packets:
    manager.process_doip_raw(packet.data)

# Get correlation statistics
stats = manager.correlator().statistics()
print(f"Match rate: {stats.match_rate():.1%}")

# Get DTCs with filtering
active_dtcs = manager.dtc_manager().get_dtcs_filtered(
    ecu_address=0x1234,
    severity=wadjet.DTCSeverity.Failure,
    status_mask=0x08  # Confirmed bit
)
for dtc in active_dtcs:
    print(f"DTC 0x{dtc.code:06X}: {dtc.severity.name}")
```

**C API:**

```c
#include <wadjet_c.h>

void on_diagnostic_event(wadjet_diagnostic_event_t event,
                         const wadjet_diagnostic_session_state_t* state,
                         const wadjet_request_response_pair_t* pair,
                         void* user_data) {
    if (event == WADJET_DIAGNOSTIC_EVENT_SESSION_STARTED) {
        printf("Session started on ECU 0x%04X\n", state->tester_address);
    }
}

int main() {
    wadjet_diagnostic_options_t opts;
    wadjet_diagnostic_options_defaults(&opts);
    opts.p2_timeout_ms = 50;
    opts.s3_timeout_ms = 5000;
    
    wadjet_diagnostic_session_manager_t* manager = 
        wadjet_diagnostic_manager_create(&opts);
    
    wadjet_diagnostic_manager_set_callback(manager, on_diagnostic_event, NULL);
    
    // Process packets
    wadjet_diagnostic_manager_process_doip_raw(manager, packet_data, packet_size);
    
    // Get statistics
    wadjet_correlation_statistics_t stats;
    wadjet_diagnostic_manager_correlation_stats(manager, &stats);
    printf("Match rate: %.1f%%\n", stats.match_rate * 100);
    
    wadjet_diagnostic_manager_destroy(manager);
    return 0;
}
```

**Rust API:**

```rust
use wadjet::diagnostic::{DiagnosticSessionManager, DiagnosticOptions, DiagnosticEvent};

fn main() -> wadjet::Result<()> {
    // Create with custom timing parameters
    let opts = DiagnosticOptions::new()
        .p2_timeout(50)
        .p2_star_timeout(5000)
        .s3_timeout(5000)
        .correlation(true)
        .timeout_detection(true);
    
    let manager = DiagnosticSessionManager::new(opts)?;
    
    // Process packets
    for packet in captured_packets {
        manager.process_doip_raw(&packet.data)?;
    }
    
    // Check tracked ECUs
    for ecu_addr in manager.tracked_ecus()? {
        if let Some(state) = manager.session_state(ecu_addr)? {
            println!("ECU 0x{:04X}: {:?}, security level {}", 
                     ecu_addr, state.session_type, state.security_level);
        }
    }
    
    // Get correlation statistics
    let stats = manager.correlation_statistics()?;
    println!("Match rate: {:.1}%", stats.match_rate() * 100.0);
    
    Ok(())
}
```

### Test Count Target

**41+ tests** covering:
- Session management: 12 tests
- Flash sequence tracking: 16 tests
- DTC manager: 13 tests

### Files Implemented

**Headers:**
- `include/wadjet/protocols/diagnostic/diagnostic_types.hpp`
- `include/wadjet/protocols/diagnostic/diagnostic_session.hpp`
- `include/wadjet/protocols/diagnostic/request_correlator.hpp`
- `include/wadjet/protocols/diagnostic/flash_sequence.hpp`
- `include/wadjet/protocols/diagnostic/dtc_manager.hpp`
- `include/wadjet/protocols/diagnostic/uds_doip_decoder.hpp`
- `include/wadjet/protocols/diagnostic.hpp`

**Implementation:**
- `src/protocols/diagnostic/diagnostic_types.cpp`
- `src/protocols/diagnostic/diagnostic_session.cpp`
- `src/protocols/diagnostic/request_correlator.cpp`
- `src/protocols/diagnostic/flash_sequence.cpp`
- `src/protocols/diagnostic/dtc_manager.cpp`
- `src/protocols/diagnostic/uds_doip_decoder.cpp`

**Tests:**
- `tests/protocols/test_diagnostic.cpp`
- `tests/protocols/test_flash_sequence.cpp`
- `tests/protocols/test_dtc_manager.cpp`

**Language Bindings:**
- Python: `bindings/python/src/diagnostic_bindings.cpp`
- C: `bindings/c/include/wadjet_c.h`, `bindings/c/src/wadjet_c.cpp`
- Rust: `bindings/rust/wadjet/src/diagnostic.rs`

**Documentation:**
- `docs/protocols/uds_doip.md`
- `docs/diagnostic_testing.md`

**Examples:**
- `examples/diagnostic_analyzer.cpp`
- `examples/flash_validator.cpp`
- `examples/dtc_analyzer.cpp`
- `examples/python/diagnostic_analysis.py`

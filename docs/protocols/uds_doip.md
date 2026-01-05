# UDS over DoIP Integration

𓆓 Wadjet-Link — *Restoring the complete picture of the automotive stream.*

## Overview

UDS over DoIP combines ISO 14229 (Unified Diagnostic Services) with ISO 13400 (Diagnostics over IP) for complete Ethernet-based automotive diagnostics. This module provides:

- **Session Management**: Track diagnostic session state across multiple ECUs
- **Request Correlation**: Match UDS requests with their responses
- **Timing Validation**: Verify P2, P2*, and S3 timing constraints
- **Event Notifications**: Callbacks for diagnostic events (session changes, security, errors)

## Architecture

```
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

## Core Classes

### DiagnosticSessionManager

The main entry point for diagnostic session management.

```cpp
#include <wadjet/protocols/diagnostic.hpp>

using namespace wadjet::protocols::diagnostic;

// Create with default options
DiagnosticSessionManager manager;

// Or with custom options
DiagnosticSessionManager::Options opts;
opts.enable_correlation = true;
opts.enable_timeout_detection = true;
opts.max_ecus = 256;
opts.default_timing = DiagnosticTiming::defaults();

DiagnosticSessionManager manager(opts);
```

### Event Handling

Register callbacks for diagnostic events:

```cpp
manager.on_event([](DiagnosticEvent event, 
                    const DiagnosticSessionState& state,
                    const RequestResponsePair* pair) {
    switch (event) {
        case DiagnosticEvent::SessionStarted:
            std::cout << "Session started with ECU 0x" 
                      << std::hex << state.tester_address << "\n";
            break;
        case DiagnosticEvent::SecurityUnlocked:
            std::cout << "Security unlocked to level " 
                      << (int)state.security_level << "\n";
            break;
        case DiagnosticEvent::NegativeResponse:
            if (pair && pair->get_nrc()) {
                std::cout << "NRC: " << nrc_string(*pair->get_nrc()) << "\n";
            }
            break;
        // ... handle other events
    }
});
```

### Processing DoIP Packets

```cpp
// Process raw DoIP data (header + payload)
std::span<const std::byte> doip_data = /* captured packet */;
bool processed = manager.process_doip_raw(doip_data);

// Or with a decoded DoIP header
DoIPHeader header = /* decoded */;
std::span<const std::byte> payload = /* payload data */;
manager.process_doip_packet(header, payload);
```

### Querying Session State

```cpp
// Get all tracked ECUs
auto ecus = manager.get_tracked_ecus();
for (auto ecu_addr : ecus) {
    auto* state = manager.get_session_state(ecu_addr);
    if (state) {
        std::cout << "ECU 0x" << std::hex << ecu_addr
                  << " Session: " << session_type_string(state->session_type)
                  << " Security: " << (int)state->security_level
                  << "\n";
    }
}

// Check if ECU is in programming mode
if (state->is_programming()) {
    // Handle programming session
}

// Check security level
if (state->is_security_unlocked(1)) {
    // Security level 1 or higher is unlocked
}
```

## Request/Response Correlation

The `RequestCorrelator` matches UDS requests with their responses:

```cpp
auto& correlator = manager.correlator();

// Get correlation statistics
auto stats = correlator.statistics();
std::cout << "Requests: " << stats.requests_recorded << "\n"
          << "Matched: " << stats.responses_matched << "\n"
          << "Unmatched: " << stats.responses_unmatched << "\n"
          << "Timeouts: " << stats.pending_timeouts << "\n"
          << "Match Rate: " << (stats.match_rate() * 100) << "%\n";

// Check for timeouts
size_t timed_out = correlator.check_timeouts();
if (timed_out > 0) {
    std::cout << timed_out << " requests timed out\n";
}

// Get completed request/response pairs
auto pairs = manager.get_completed_pairs(ecu_address, 100);
for (auto& pair : pairs) {
    if (pair->is_complete()) {
        auto response_time = pair->response_time();
        if (response_time) {
            std::cout << "Response time: " << response_time->count() << "ms\n";
        }
    }
}
```

## UDS over DoIP Decoder

For direct decoding without session management:

```cpp
UdsOverDoipDecoder decoder;

auto result = decoder.decode(doip_packet_data);
if (result.is_ok()) {
    auto& decoded = result.value();
    
    std::cout << "Source: 0x" << std::hex << decoded.source_address << "\n"
              << "Target: 0x" << decoded.target_address << "\n"
              << "Service: " << service_id_string(decoded.service_id()) << "\n"
              << "Direction: " << (decoded.direction == MessageDirection::Request 
                                   ? "Request" : "Response") << "\n";
    
    if (decoded.is_negative_response()) {
        std::cout << "NRC: " << (int)decoded.uds_header.negative_response_code << "\n";
    }
} else {
    auto& error = result.error();
    std::cout << "Decode error: " << error.message << "\n";
}
```

## Diagnostic Events

The following events are emitted during diagnostic session processing:

| Event | Description |
|-------|-------------|
| `RoutingActivated` | DoIP routing activation successful |
| `RoutingDeactivated` | Routing deactivated or lost |
| `ConnectionLost` | TCP connection lost |
| `SessionStarted` | Diagnostic session started |
| `SessionChanged` | Session type changed |
| `SessionTimeout` | S3 timeout occurred |
| `SessionEnded` | Session explicitly ended |
| `SecurityUnlocked` | Security level unlocked |
| `SecurityLocked` | Security level locked (failed) |
| `SecurityLockout` | Lockout due to failed attempts |
| `RequestSent` | Request sent to ECU |
| `ResponseReceived` | Response received from ECU |
| `ResponsePending` | ECU sent ResponsePending (0x78) |
| `ResponseTimeout` | No response within timeout |
| `NegativeResponse` | Negative response received |
| `DTCsRead` | DTCs were read |
| `DTCsCleared` | DTCs were cleared |
| `DataIdentifierRead` | Data identifier read |
| `FlashStarted` | Flash download started |
| `FlashProgress` | Flash download progress |
| `FlashCompleted` | Flash download completed |
| `FlashFailed` | Flash download failed |

## Timing Parameters

Default timing parameters (ISO 14229):

| Parameter | Default | Description |
|-----------|---------|-------------|
| P2 Server Max | 50ms | Initial response timeout |
| P2* Server Max | 5000ms | ResponsePending timeout |
| S3 Server | 5000ms | Session keep-alive timeout |

```cpp
// Custom timing
DiagnosticTiming timing;
timing.p2_server_max = std::chrono::milliseconds(100);
timing.p2_star_server_max = std::chrono::milliseconds(10000);
timing.s3_server = std::chrono::milliseconds(3000);

// Set for specific ECU
manager.set_timing(ecu_address, timing);

// Validate timing
if (pair->within_p2(timing)) {
    std::cout << "Response within P2 timeout\n";
} else if (pair->within_p2_star(timing)) {
    std::cout << "Response within P2* timeout (after pending)\n";
} else {
    std::cout << "Response exceeded timeout!\n";
}
```

## Python Bindings

```python
import wadjet

# Create session manager
manager = wadjet.DiagnosticSessionManager()

# Register event callback
def on_event(event, state, pair):
    if event == wadjet.DiagnosticEvent.SessionStarted:
        print(f"Session started - tester: 0x{state.tester_address:04X}")
    elif event == wadjet.DiagnosticEvent.SecurityUnlocked:
        print(f"Security unlocked to level {state.security_level}")
    elif event == wadjet.DiagnosticEvent.NegativeResponse:
        print(f"Negative response received")

manager.on_event(on_event)

# Process captured packets
for packet in captured_packets:
    manager.process_doip_raw(packet.data)

# Check tracked ECUs
for ecu_addr in manager.get_tracked_ecus():
    state = manager.get_session_state(ecu_addr)
    if state:
        print(f"ECU 0x{ecu_addr:04X}: {state.session_type}")

# Get correlation statistics
stats = manager.correlator().statistics()
print(f"Match rate: {stats.match_rate():.1%}")
```

## C API

```c
#include <wadjet_c.h>

void on_diagnostic_event(wadjet_diagnostic_event_t event,
                         const wadjet_diagnostic_session_state_t* state,
                         void* user_data) {
    if (event == WADJET_DIAG_EVENT_SESSION_STARTED) {
        printf("Session started with tester 0x%04X\n", state->tester_address);
    }
}

int main() {
    wadjet_diagnostic_options_t opts;
    wadjet_diagnostic_options_default(&opts);
    
    wadjet_diagnostic_session_manager_t manager;
    if (wadjet_diagnostic_manager_create(&opts, &manager) != WADJET_OK) {
        return 1;
    }
    
    wadjet_diagnostic_manager_on_event(manager, on_diagnostic_event, NULL);
    
    // Process packets...
    wadjet_diagnostic_manager_process(manager, data, length);
    
    // Get statistics
    uint64_t requests, matched, unmatched, timeouts;
    wadjet_diagnostic_manager_statistics(manager, 
        &requests, &matched, &unmatched, &timeouts);
    
    wadjet_diagnostic_manager_destroy(manager);
    return 0;
}
```

## Rust API

Safe Rust bindings provide idiomatic access to the diagnostic module:

```rust
use wadjet::diagnostic::{
    DiagnosticSessionManager, DiagnosticOptions, DiagnosticEvent, SessionType
};

fn main() -> wadjet::Result<()> {
    // Create with builder pattern for configuration
    let options = DiagnosticOptions::default()
        .p2_timeout(100)           // P2 Server Max in ms
        .p2_star_timeout(5000)     // P2* Server Max in ms
        .s3_timeout(5000)          // S3 Server timeout in ms
        .max_ecus(256)
        .correlation(true)
        .timeout_detection(true);
    
    let mut manager = DiagnosticSessionManager::new(options)?;

    // Register event callback with pattern matching
    manager.on_event(|event, state| {
        match event {
            DiagnosticEvent::SessionStarted => {
                println!("Session started - ECU: 0x{:04X}, Tester: 0x{:04X}",
                    state.gateway_address, state.tester_address);
            }
            DiagnosticEvent::SessionChanged => {
                println!("Session changed to: {}", state.session_type);
            }
            DiagnosticEvent::SecurityUnlocked => {
                println!("Security unlocked to level {}", state.security_level);
            }
            DiagnosticEvent::FlashStarted => {
                println!("Flash download started for ECU 0x{:04X}", 
                    state.gateway_address);
            }
            DiagnosticEvent::FlashCompleted => {
                println!("Flash download completed successfully!");
            }
            DiagnosticEvent::ResponseTimeout => {
                println!("Response timeout! Requests: {}, Timeouts: {}", 
                    state.requests_sent, state.timeouts);
            }
            _ => {}
        }
    });

    // Process DoIP packets
    // manager.process(&doip_packet_data)?;

    // Query session state for specific ECU
    if let Some(state) = manager.get_session(0x1234) {
        println!("ECU 0x1234 State:");
        println!("  Session: {} (active={})", 
            state.session_type, state.session_active);
        println!("  Security Level: {}", state.security_level);
        println!("  Routing Active: {}", state.routing_active);
        println!("  Requests Sent: {}", state.requests_sent);
        println!("  Responses: {} (negative: {})", 
            state.responses_received, state.negative_responses);
        
        // Helper methods
        if state.is_programming() {
            println!("  In programming mode!");
        }
        if state.is_security_unlocked(1) {
            println!("  Security level 1+ unlocked");
        }
        println!("  Response rate: {:.1}%", state.response_rate() * 100.0);
    }

    // Get number of tracked ECU sessions
    println!("Active sessions: {}", manager.session_count());

    // Get correlation statistics
    let stats = manager.statistics();
    println!("Correlation Statistics:");
    println!("  Requests recorded: {}", stats.requests_recorded);
    println!("  Responses matched: {}", stats.responses_matched);
    println!("  Unmatched responses: {}", stats.responses_unmatched);
    println!("  Timeouts: {}", stats.timeouts);
    println!("  Match rate: {:.1}%", stats.match_rate() * 100.0);
    println!("  Timeout rate: {:.1}%", stats.timeout_rate() * 100.0);

    // Periodic timeout check (call in main loop)
    let timed_out = manager.check_timeouts();
    if timed_out > 0 {
        println!("{} requests timed out", timed_out);
    }

    Ok(())
}
```

### Rust Types

The Rust bindings provide safe wrappers for all diagnostic types:

| Rust Type | Description |
|-----------|-------------|
| `DiagnosticSessionManager` | Main manager with automatic cleanup (Drop) |
| `DiagnosticOptions` | Builder pattern configuration |
| `DiagnosticEvent` | Enum with 22 event variants |
| `DiagnosticSessionState` | ECU session state snapshot |
| `SessionType` | UDS session type enum (Default, Programming, Extended, SafetySystem) |
| `CorrelationStatistics` | Request/response matching statistics |
| `DiagnosticTiming` | ISO 14229 timing parameters |

### Event Categories

```rust
// Check event categories
if event.is_session_event() {
    // SessionStarted, SessionChanged, SessionTimeout, SessionEnded
}
if event.is_security_event() {
    // SecurityUnlocked, SecurityLocked, SecurityLockout
}
if event.is_flash_event() {
    // FlashStarted, FlashProgress, FlashCompleted, FlashFailed
}
```

## See Also

- [UDS Protocol Reference](uds.md) — ISO 14229 service details
- [DoIP Protocol Reference](../protocols.md#doip) — ISO 13400 transport
- [Testing Guide](../diagnostic_testing.md) — Diagnostic test patterns

# Diagnostic Testing Guide

𓆓 Wadjet-Link — *Restoring the complete picture of the automotive stream.*

## Overview

This guide covers best practices for testing automotive diagnostic systems using Wadjet-Link's diagnostic module. It covers:

- Session management testing
- Timing validation
- Security access verification
- Error handling patterns

## Test Fixtures

### DiagnosticTestFixture

Use the `LiveCaptureTestFixture` with diagnostic helpers:

```cpp
#include <wadjet/testing/fixture.hpp>
#include <wadjet/protocols/diagnostic.hpp>

class DiagnosticTest : public wadjet::testing::LiveCaptureTestFixture {
protected:
    void SetUp() override {
        LiveCaptureTestFixture::SetUp();
        
        manager_ = std::make_unique<DiagnosticSessionManager>();
        
        // Register event tracking
        manager_->on_event([this](DiagnosticEvent event,
                                   const DiagnosticSessionState& state,
                                   const RequestResponsePair* pair) {
            events_.push_back({event, state.tester_address});
            if (pair) {
                pairs_.push_back(*pair);
            }
        });
    }
    
    void ProcessCapture() {
        for (const auto& packet : captured_packets()) {
            manager_->process_doip_raw(packet.data());
        }
    }
    
    std::unique_ptr<DiagnosticSessionManager> manager_;
    std::vector<std::pair<DiagnosticEvent, uint16_t>> events_;
    std::vector<RequestResponsePair> pairs_;
};
```

## Session Testing

### Verify Session Transitions

```cpp
TEST_F(DiagnosticTest, SessionTransitionToExtended) {
    // Capture diagnostic traffic
    start_capture("tcp port 13400");
    
    // Wait for session control
    wait_for_condition([this]() {
        ProcessCapture();
        for (auto& [event, addr] : events_) {
            if (event == DiagnosticEvent::SessionChanged) {
                auto* state = manager_->get_session_state(addr);
                if (state && state->session_type == SessionType::ExtendedDiagnosticSession) {
                    return true;
                }
            }
        }
        return false;
    }, std::chrono::seconds(10));
    
    // Verify final state
    auto ecus = manager_->get_tracked_ecus();
    ASSERT_FALSE(ecus.empty());
    
    for (auto ecu : ecus) {
        auto* state = manager_->get_session_state(ecu);
        ASSERT_NE(state, nullptr);
        EXPECT_TRUE(state->is_extended());
    }
}
```

### Verify Session Keep-Alive

```cpp
TEST_F(DiagnosticTest, TesterPresentKeepsSessionAlive) {
    // Start with an established session
    ProcessCapture();
    
    auto* state = manager_->get_session_state(0x1234);
    ASSERT_NE(state, nullptr);
    ASSERT_TRUE(state->session_active);
    
    // Wait and verify TesterPresent resets activity
    auto initial_activity = state->last_activity;
    
    // Capture more traffic (should include TesterPresent)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ProcessCapture();
    
    // Verify activity was updated
    EXPECT_GT(state->last_activity, initial_activity);
    EXPECT_FALSE(state->is_potentially_timed_out());
}
```

## Timing Validation

### P2 Timeout Validation

```cpp
TEST_F(DiagnosticTest, ResponseWithinP2Timeout) {
    ProcessCapture();
    
    auto& correlator = manager_->correlator();
    auto pairs = correlator.get_completed(100);
    
    DiagnosticTiming timing = DiagnosticTiming::defaults();
    
    for (auto& pair : pairs) {
        if (pair->is_complete()) {
            // First response should be within P2
            if (pair->response && pair->response->pending_count == 0) {
                EXPECT_TRUE(pair->within_p2(timing))
                    << "Response exceeded P2 timeout: " 
                    << pair->response_time()->count() << "ms";
            }
            // After ResponsePending, should be within P2*
            else {
                EXPECT_TRUE(pair->within_p2_star(timing))
                    << "Response exceeded P2* timeout: "
                    << pair->response_time()->count() << "ms";
            }
        }
    }
}
```

### Custom Timing Requirements

```cpp
TEST_F(DiagnosticTest, CustomTimingRequirements) {
    // OEM-specific timing
    DiagnosticTiming oem_timing;
    oem_timing.p2_server_max = std::chrono::milliseconds(100);
    oem_timing.p2_star_server_max = std::chrono::milliseconds(10000);
    
    ProcessCapture();
    
    auto pairs = manager_->correlator().get_completed(100);
    
    int violations = 0;
    for (auto& pair : pairs) {
        if (pair->is_complete()) {
            auto rt = pair->response_time();
            if (rt && *rt > oem_timing.p2_server_max) {
                if (!pair->response || pair->response->pending_count == 0) {
                    ++violations;
                    std::cout << "P2 violation: " << rt->count() << "ms\n";
                }
            }
        }
    }
    
    EXPECT_EQ(violations, 0) << "Found " << violations << " timing violations";
}
```

## Security Access Testing

### Verify Security Unlock Sequence

```cpp
TEST_F(DiagnosticTest, SecurityAccessSequence) {
    // Track security events
    std::vector<uint8_t> security_levels;
    
    manager_->on_event([&security_levels](DiagnosticEvent event,
                                           const DiagnosticSessionState& state,
                                           const RequestResponsePair*) {
        if (event == DiagnosticEvent::SecurityUnlocked) {
            security_levels.push_back(state.security_level);
        }
    });
    
    ProcessCapture();
    
    // Verify security was unlocked
    ASSERT_FALSE(security_levels.empty());
    
    // Verify expected security level
    EXPECT_EQ(security_levels.back(), 1) << "Expected security level 1";
    
    // Verify final state
    auto ecus = manager_->get_tracked_ecus();
    for (auto ecu : ecus) {
        auto* state = manager_->get_session_state(ecu);
        if (state) {
            EXPECT_TRUE(state->is_security_unlocked(1));
        }
    }
}
```

## Error Handling

### Negative Response Analysis

```cpp
TEST_F(DiagnosticTest, NegativeResponseHandling) {
    ProcessCapture();
    
    auto pairs = manager_->correlator().get_completed(100);
    
    std::map<uint8_t, int> nrc_counts;
    
    for (auto& pair : pairs) {
        if (pair->is_negative()) {
            auto nrc = pair->get_nrc();
            if (nrc) {
                ++nrc_counts[*nrc];
            }
        }
    }
    
    // Report NRC distribution
    for (auto& [nrc, count] : nrc_counts) {
        std::cout << "NRC 0x" << std::hex << (int)nrc 
                  << " (" << nrc_string(static_cast<NRC>(nrc)) << "): "
                  << std::dec << count << "\n";
    }
    
    // Fail on critical NRCs
    EXPECT_EQ(nrc_counts[0x33], 0) << "SecurityAccessDenied detected";
    EXPECT_EQ(nrc_counts[0x72], 0) << "GeneralProgrammingFailure detected";
}
```

### Correlation Statistics

```cpp
TEST_F(DiagnosticTest, CorrelationQuality) {
    ProcessCapture();
    
    auto stats = manager_->correlator().statistics();
    
    std::cout << "Correlation Statistics:\n"
              << "  Requests: " << stats.requests_recorded << "\n"
              << "  Matched: " << stats.responses_matched << "\n"
              << "  Unmatched: " << stats.responses_unmatched << "\n"
              << "  Timeouts: " << stats.pending_timeouts << "\n"
              << "  Match Rate: " << (stats.match_rate() * 100) << "%\n";
    
    // Verify acceptable match rate
    EXPECT_GE(stats.match_rate(), 0.95) 
        << "Match rate below 95%: " << (stats.match_rate() * 100) << "%";
    
    // Verify no excessive timeouts
    EXPECT_LE(stats.pending_timeouts, stats.requests_recorded * 0.05)
        << "Too many timeouts";
}
```

## YAML Scenario Testing

### Diagnostic Session Scenario

```yaml
name: Diagnostic Session Test
description: Verify diagnostic session establishment

config:
  interface: eth0
  timeout: 30s
  
steps:
  - capture:
      filter: "tcp port 13400"
      duration: 10s
      
  - expect:
      protocol: doip
      payload_type: routing_activation_response
      result_code: 0x10  # Routing activated
      timeout: 5s
      
  - expect:
      protocol: uds
      service: DiagnosticSessionControl
      session_type: ExtendedDiagnosticSession
      response: positive
      timeout: 5s
      
  - log:
      message: "Extended session established"
      
  - expect:
      protocol: uds
      service: SecurityAccess
      subfunction: 0x01  # Request seed
      timeout: 3s
      
  - expect:
      protocol: uds
      service: SecurityAccess
      subfunction: 0x02  # Send key
      response: positive
      timeout: 3s
      
  - log:
      message: "Security level 1 unlocked"
```

## Best Practices

### 1. Use Event-Driven Testing

Instead of polling, use event callbacks:

```cpp
std::promise<bool> session_started;

manager_->on_event([&](DiagnosticEvent event, const auto& state, auto*) {
    if (event == DiagnosticEvent::SessionStarted) {
        session_started.set_value(true);
    }
});

auto future = session_started.get_future();
EXPECT_EQ(future.wait_for(std::chrono::seconds(10)), 
          std::future_status::ready);
```

### 2. Isolate ECU Testing

Test each ECU independently:

```cpp
for (auto ecu : target_ecus) {
    auto* state = manager_->get_session_state(ecu);
    if (!state) {
        ADD_FAILURE() << "ECU 0x" << std::hex << ecu << " not tracked";
        continue;
    }
    
    SCOPED_TRACE("ECU 0x" + std::to_string(ecu));
    EXPECT_TRUE(state->session_active);
    EXPECT_GE(state->responses_received, state->requests_sent * 0.9);
}
```

### 3. Save PCAP on Failure

```cpp
void TearDown() override {
    if (HasFailure()) {
        save_pcap("diagnostic_failure_" + test_name() + ".pcap");
    }
    LiveCaptureTestFixture::TearDown();
}
```

### 4. Validate Complete Sequences

```cpp
TEST_F(DiagnosticTest, FlashSequenceComplete) {
    std::vector<DiagnosticEvent> expected = {
        DiagnosticEvent::SessionChanged,      // Enter programming
        DiagnosticEvent::SecurityUnlocked,    // Unlock security
        DiagnosticEvent::FlashStarted,        // Start download
        DiagnosticEvent::FlashProgress,       // At least one progress
        DiagnosticEvent::FlashCompleted       // Complete
    };
    
    ProcessCapture();
    
    size_t expected_idx = 0;
    for (auto& [event, addr] : events_) {
        if (expected_idx < expected.size() && event == expected[expected_idx]) {
            ++expected_idx;
        }
    }
    
    EXPECT_EQ(expected_idx, expected.size()) 
        << "Incomplete flash sequence at step " << expected_idx;
}
```

## Rust Testing

The Rust bindings provide safe, idiomatic testing capabilities:

### Basic Session Testing

```rust
use wadjet::diagnostic::{
    DiagnosticSessionManager, DiagnosticOptions, DiagnosticEvent, SessionType
};

#[test]
fn test_session_management() -> wadjet::Result<()> {
    let options = DiagnosticOptions::default()
        .p2_timeout(100)
        .timeout_detection(true);
    
    let mut manager = DiagnosticSessionManager::new(options)?;
    
    // Track received events
    let events = std::sync::Arc::new(std::sync::Mutex::new(Vec::new()));
    let events_clone = events.clone();
    
    manager.on_event(move |event, state| {
        events_clone.lock().unwrap().push((event, state.gateway_address));
    });
    
    // Process test data
    // manager.process(&test_packet_data)?;
    
    // Verify events
    let events = events.lock().unwrap();
    assert!(events.iter().any(|(e, _)| *e == DiagnosticEvent::SessionStarted));
    
    Ok(())
}
```

### Timing Validation

```rust
use wadjet::diagnostic::{DiagnosticTiming, DiagnosticSessionManager, DiagnosticOptions};

#[test]
fn test_timing_validation() -> wadjet::Result<()> {
    let timing = DiagnosticTiming::default();
    
    // Validate P2 timeout
    assert!(timing.within_p2(45), "Should be within P2");
    assert!(!timing.within_p2(55), "Should exceed P2");
    
    // Validate P2* timeout
    assert!(timing.within_p2_star(4500), "Should be within P2*");
    assert!(!timing.within_p2_star(5500), "Should exceed P2*");
    
    Ok(())
}
```

### Statistics Validation

```rust
use wadjet::diagnostic::{DiagnosticSessionManager, DiagnosticOptions};

#[test]
fn test_correlation_statistics() -> wadjet::Result<()> {
    let mut manager = DiagnosticSessionManager::new(DiagnosticOptions::default())?;
    
    // Process test data...
    
    let stats = manager.statistics();
    
    // Validate match rate
    assert!(stats.match_rate() >= 0.95, 
        "Match rate should be >= 95%, got {:.1}%", stats.match_rate() * 100.0);
    
    // Validate timeout rate
    assert!(stats.timeout_rate() <= 0.05,
        "Timeout rate should be <= 5%, got {:.1}%", stats.timeout_rate() * 100.0);
    
    println!("{}", stats); // Uses Display impl for nice output
    
    Ok(())
}
```

### Session State Validation

```rust
use wadjet::diagnostic::{DiagnosticSessionManager, DiagnosticOptions, SessionType};

#[test]
fn test_session_state() -> wadjet::Result<()> {
    let manager = DiagnosticSessionManager::new(DiagnosticOptions::default())?;
    
    // After processing packets...
    if let Some(state) = manager.get_session(0x1234) {
        // Verify session type
        assert!(state.is_programming() || state.is_extended(),
            "Expected programming or extended session");
        
        // Verify security
        assert!(state.is_security_unlocked(1),
            "Security level 1 should be unlocked");
        
        // Verify response rate
        assert!(state.response_rate() >= 0.9,
            "Response rate should be >= 90%");
        
        // Verify negative response rate
        assert!(state.negative_response_rate() <= 0.1,
            "Negative response rate should be <= 10%");
        
        println!("ECU State: {}", state); // Uses Display impl
    }
    
    Ok(())
}
```

### Event Category Testing

```rust
use wadjet::diagnostic::DiagnosticEvent;

#[test]
fn test_event_categories() {
    // Session events
    assert!(DiagnosticEvent::SessionStarted.is_session_event());
    assert!(DiagnosticEvent::SessionChanged.is_session_event());
    assert!(!DiagnosticEvent::SecurityUnlocked.is_session_event());
    
    // Security events
    assert!(DiagnosticEvent::SecurityUnlocked.is_security_event());
    assert!(DiagnosticEvent::SecurityLocked.is_security_event());
    assert!(!DiagnosticEvent::FlashStarted.is_security_event());
    
    // Flash events
    assert!(DiagnosticEvent::FlashStarted.is_flash_event());
    assert!(DiagnosticEvent::FlashCompleted.is_flash_event());
    assert!(!DiagnosticEvent::RequestSent.is_flash_event());
    
    // Display names
    assert_eq!(DiagnosticEvent::SessionStarted.name(), "SessionStarted");
    assert_eq!(DiagnosticEvent::FlashCompleted.name(), "FlashCompleted");
}
```

## See Also

- [UDS over DoIP Integration](protocols/uds_doip.md) — Protocol reference
- [UDS Protocol Reference](protocols/uds.md) — Service details
- [Testing Framework](testing.md) — General testing guide

//! Diagnostic session management for UDS over DoIP.
//!
//! This module provides safe Rust wrappers for Wadjet's diagnostic session
//! management capabilities, enabling UDS (ISO 14229) over DoIP (ISO 13400)
//! protocol analysis.
//!
//! # Overview
//!
//! The diagnostic module integrates:
//! - DoIP transport (ISO 13400)
//! - UDS services (ISO 14229)
//! - Session state tracking (Default/Programming/Extended)
//! - Security access level management
//! - Request/response correlation with timing validation
//!
//! # Example
//!
//! ```rust,ignore
//! use wadjet::diagnostic::{DiagnosticSessionManager, DiagnosticOptions, DiagnosticEvent};
//!
//! let options = DiagnosticOptions::default();
//! let mut manager = DiagnosticSessionManager::new(options)?;
//!
//! // Register event callback
//! manager.on_event(|event, state| {
//!     match event {
//!         DiagnosticEvent::SessionStarted => {
//!             println!("Session started - tester: 0x{:04X}", state.tester_address);
//!         }
//!         DiagnosticEvent::SecurityUnlocked => {
//!             println!("Security unlocked to level {}", state.security_level);
//!         }
//!         _ => {}
//!     }
//! });
//!
//! // Process captured DoIP packets
//! manager.process(&doip_packet)?;
//!
//! // Check session state
//! if let Some(state) = manager.get_session(0x1234) {
//!     println!("ECU 0x1234 session active: {}", state.session_active);
//! }
//! ```

use crate::error::{check_error, Error, Result};
use std::ffi::c_void;
use std::ptr;
use std::sync::Arc;

/// Diagnostic event types emitted during session management.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u32)]
pub enum DiagnosticEvent {
    /// DoIP routing activation successful
    RoutingActivated = 0,
    /// Routing deactivated or lost
    RoutingDeactivated = 1,
    /// TCP connection lost
    ConnectionLost = 2,
    /// Diagnostic session started
    SessionStarted = 3,
    /// Session type changed (e.g., Default → Extended)
    SessionChanged = 4,
    /// S3 timeout occurred - session may be lost
    SessionTimeout = 5,
    /// Session explicitly ended
    SessionEnded = 6,
    /// Security level unlocked
    SecurityUnlocked = 7,
    /// Security level locked (failed attempt)
    SecurityLocked = 8,
    /// Security lockout due to failed attempts
    SecurityLockout = 9,
    /// Request sent to ECU
    RequestSent = 10,
    /// Response received from ECU
    ResponseReceived = 11,
    /// ECU sent ResponsePending (NRC 0x78)
    ResponsePending = 12,
    /// No response within timeout
    ResponseTimeout = 13,
    /// Negative response received
    NegativeResponse = 14,
    /// DTCs were read
    DtcsRead = 15,
    /// DTCs were cleared
    DtcsCleared = 16,
    /// Data identifier was read
    DataIdentifierRead = 17,
    /// Flash download started
    FlashStarted = 18,
    /// Flash download progress update
    FlashProgress = 19,
    /// Flash download completed successfully
    FlashCompleted = 20,
    /// Flash download failed
    FlashFailed = 21,
}

impl DiagnosticEvent {
    /// Convert from C enum value
    fn from_c(value: wadjet_sys::wadjet_diagnostic_event_t) -> Option<Self> {
        match value {
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_ROUTING_ACTIVATED => {
                Some(Self::RoutingActivated)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_ROUTING_DEACTIVATED => {
                Some(Self::RoutingDeactivated)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_CONNECTION_LOST => {
                Some(Self::ConnectionLost)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SESSION_STARTED => {
                Some(Self::SessionStarted)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SESSION_CHANGED => {
                Some(Self::SessionChanged)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SESSION_TIMEOUT => {
                Some(Self::SessionTimeout)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SESSION_ENDED => {
                Some(Self::SessionEnded)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SECURITY_UNLOCKED => {
                Some(Self::SecurityUnlocked)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SECURITY_LOCKED => {
                Some(Self::SecurityLocked)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_SECURITY_LOCKOUT => {
                Some(Self::SecurityLockout)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_REQUEST_SENT => {
                Some(Self::RequestSent)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_RESPONSE_RECEIVED => {
                Some(Self::ResponseReceived)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_RESPONSE_PENDING => {
                Some(Self::ResponsePending)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_RESPONSE_TIMEOUT => {
                Some(Self::ResponseTimeout)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_NEGATIVE_RESPONSE => {
                Some(Self::NegativeResponse)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_DTCS_READ => {
                Some(Self::DtcsRead)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_DTCS_CLEARED => {
                Some(Self::DtcsCleared)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_DATA_IDENTIFIER_READ => {
                Some(Self::DataIdentifierRead)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_FLASH_STARTED => {
                Some(Self::FlashStarted)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_FLASH_PROGRESS => {
                Some(Self::FlashProgress)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_FLASH_COMPLETED => {
                Some(Self::FlashCompleted)
            }
            wadjet_sys::wadjet_diagnostic_event_t::WADJET_DIAG_EVENT_FLASH_FAILED => {
                Some(Self::FlashFailed)
            }
            _ => None,
        }
    }

    /// Get a human-readable name for this event
    pub fn name(&self) -> &'static str {
        match self {
            Self::RoutingActivated => "RoutingActivated",
            Self::RoutingDeactivated => "RoutingDeactivated",
            Self::ConnectionLost => "ConnectionLost",
            Self::SessionStarted => "SessionStarted",
            Self::SessionChanged => "SessionChanged",
            Self::SessionTimeout => "SessionTimeout",
            Self::SessionEnded => "SessionEnded",
            Self::SecurityUnlocked => "SecurityUnlocked",
            Self::SecurityLocked => "SecurityLocked",
            Self::SecurityLockout => "SecurityLockout",
            Self::RequestSent => "RequestSent",
            Self::ResponseReceived => "ResponseReceived",
            Self::ResponsePending => "ResponsePending",
            Self::ResponseTimeout => "ResponseTimeout",
            Self::NegativeResponse => "NegativeResponse",
            Self::DtcsRead => "DTCsRead",
            Self::DtcsCleared => "DTCsCleared",
            Self::DataIdentifierRead => "DataIdentifierRead",
            Self::FlashStarted => "FlashStarted",
            Self::FlashProgress => "FlashProgress",
            Self::FlashCompleted => "FlashCompleted",
            Self::FlashFailed => "FlashFailed",
        }
    }

    /// Check if this is a session-related event
    pub fn is_session_event(&self) -> bool {
        matches!(
            self,
            Self::SessionStarted
                | Self::SessionChanged
                | Self::SessionTimeout
                | Self::SessionEnded
        )
    }

    /// Check if this is a security-related event
    pub fn is_security_event(&self) -> bool {
        matches!(
            self,
            Self::SecurityUnlocked | Self::SecurityLocked | Self::SecurityLockout
        )
    }

    /// Check if this is a flash-related event
    pub fn is_flash_event(&self) -> bool {
        matches!(
            self,
            Self::FlashStarted | Self::FlashProgress | Self::FlashCompleted | Self::FlashFailed
        )
    }
}

impl std::fmt::Display for DiagnosticEvent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name())
    }
}

/// UDS Session type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum SessionType {
    /// Default diagnostic session (0x01)
    Default = 0x01,
    /// Programming session (0x02)
    Programming = 0x02,
    /// Extended diagnostic session (0x03)
    Extended = 0x03,
    /// Safety system diagnostic session (0x04)
    SafetySystem = 0x04,
    /// Unknown session type
    Unknown = 0xFF,
}

impl SessionType {
    /// Convert from C enum value
    fn from_c(value: wadjet_sys::wadjet_uds_session_type_t) -> Self {
        match value {
            wadjet_sys::wadjet_uds_session_type_t::WADJET_UDS_SESSION_DEFAULT => Self::Default,
            wadjet_sys::wadjet_uds_session_type_t::WADJET_UDS_SESSION_PROGRAMMING => {
                Self::Programming
            }
            wadjet_sys::wadjet_uds_session_type_t::WADJET_UDS_SESSION_EXTENDED => Self::Extended,
            wadjet_sys::wadjet_uds_session_type_t::WADJET_UDS_SESSION_SAFETY_SYSTEM => {
                Self::SafetySystem
            }
            _ => Self::Unknown,
        }
    }

    /// Get session type name
    pub fn name(&self) -> &'static str {
        match self {
            Self::Default => "DefaultSession",
            Self::Programming => "ProgrammingSession",
            Self::Extended => "ExtendedDiagnosticSession",
            Self::SafetySystem => "SafetySystemSession",
            Self::Unknown => "Unknown",
        }
    }
}

impl std::fmt::Display for SessionType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name())
    }
}

/// Diagnostic session state for an ECU.
#[derive(Debug, Clone)]
pub struct DiagnosticSessionState {
    /// Tester logical address (source)
    pub tester_address: u16,
    /// Gateway/ECU logical address (target)
    pub gateway_address: u16,
    /// Current UDS session type
    pub session_type: SessionType,
    /// Is the session currently active
    pub session_active: bool,
    /// Is DoIP routing activated
    pub routing_active: bool,
    /// Current security level (0 = locked, 1+ = unlocked)
    pub security_level: u8,
    /// P2 Server Max timeout in milliseconds
    pub p2_server_max_ms: u32,
    /// P2* Server Max timeout in milliseconds (after ResponsePending)
    pub p2_star_server_max_ms: u32,
    /// Total requests sent to this ECU
    pub requests_sent: u64,
    /// Total responses received from this ECU
    pub responses_received: u64,
    /// Total negative responses received
    pub negative_responses: u64,
    /// Total timeout count
    pub timeouts: u64,
}

impl DiagnosticSessionState {
    /// Convert from C struct
    fn from_c(c: &wadjet_sys::wadjet_diagnostic_session_state_t) -> Self {
        Self {
            tester_address: c.tester_address,
            gateway_address: c.gateway_address,
            session_type: SessionType::from_c(c.session_type),
            session_active: c.session_active,
            routing_active: c.routing_active,
            security_level: c.security_level,
            p2_server_max_ms: c.p2_server_max_ms,
            p2_star_server_max_ms: c.p2_star_server_max_ms,
            requests_sent: c.requests_sent,
            responses_received: c.responses_received,
            negative_responses: c.negative_responses,
            timeouts: c.timeouts,
        }
    }

    /// Check if security is unlocked at the specified level
    pub fn is_security_unlocked(&self, level: u8) -> bool {
        self.security_level >= level
    }

    /// Check if session is in programming mode
    pub fn is_programming(&self) -> bool {
        self.session_type == SessionType::Programming
    }

    /// Check if session is in extended diagnostic mode
    pub fn is_extended(&self) -> bool {
        self.session_type == SessionType::Extended
    }

    /// Get response rate (responses / requests)
    pub fn response_rate(&self) -> f64 {
        if self.requests_sent == 0 {
            0.0
        } else {
            self.responses_received as f64 / self.requests_sent as f64
        }
    }

    /// Get negative response rate
    pub fn negative_response_rate(&self) -> f64 {
        if self.responses_received == 0 {
            0.0
        } else {
            self.negative_responses as f64 / self.responses_received as f64
        }
    }
}

impl std::fmt::Display for DiagnosticSessionState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "ECU 0x{:04X} [{}] active={} security={}",
            self.gateway_address,
            self.session_type,
            self.session_active,
            self.security_level
        )
    }
}

/// Configuration options for diagnostic session manager.
#[derive(Debug, Clone)]
pub struct DiagnosticOptions {
    /// Enable request/response correlation
    pub enable_correlation: bool,
    /// Enable automatic timeout detection
    pub enable_timeout_detection: bool,
    /// Maximum number of ECUs to track
    pub max_ecus: usize,
    /// Default P2 Server Max in milliseconds (initial response timeout)
    pub p2_server_max_ms: u32,
    /// Default P2* Server Max in milliseconds (after ResponsePending)
    pub p2_star_server_max_ms: u32,
    /// Default S3 Server timeout in milliseconds (session keep-alive)
    pub s3_server_ms: u32,
}

impl Default for DiagnosticOptions {
    fn default() -> Self {
        Self {
            enable_correlation: true,
            enable_timeout_detection: true,
            max_ecus: 256,
            p2_server_max_ms: 50,       // ISO 14229 default
            p2_star_server_max_ms: 5000, // ISO 14229 default
            s3_server_ms: 5000,         // ISO 14229 default
        }
    }
}

impl DiagnosticOptions {
    /// Create new options with defaults
    pub fn new() -> Self {
        Self::default()
    }

    /// Set P2 Server Max timeout
    pub fn p2_timeout(mut self, timeout_ms: u32) -> Self {
        self.p2_server_max_ms = timeout_ms;
        self
    }

    /// Set P2* Server Max timeout
    pub fn p2_star_timeout(mut self, timeout_ms: u32) -> Self {
        self.p2_star_server_max_ms = timeout_ms;
        self
    }

    /// Set S3 Server timeout
    pub fn s3_timeout(mut self, timeout_ms: u32) -> Self {
        self.s3_server_ms = timeout_ms;
        self
    }

    /// Set maximum ECUs to track
    pub fn max_ecus(mut self, max: usize) -> Self {
        self.max_ecus = max;
        self
    }

    /// Enable or disable correlation
    pub fn correlation(mut self, enable: bool) -> Self {
        self.enable_correlation = enable;
        self
    }

    /// Enable or disable timeout detection
    pub fn timeout_detection(mut self, enable: bool) -> Self {
        self.enable_timeout_detection = enable;
        self
    }

    /// Convert to C struct
    fn to_c(&self) -> wadjet_sys::wadjet_diagnostic_options_t {
        wadjet_sys::wadjet_diagnostic_options_t {
            enable_correlation: self.enable_correlation,
            enable_timeout_detection: self.enable_timeout_detection,
            max_ecus: self.max_ecus,
            p2_server_max_ms: self.p2_server_max_ms,
            p2_star_server_max_ms: self.p2_star_server_max_ms,
            s3_server_ms: self.s3_server_ms,
        }
    }
}

/// Correlation statistics for request/response matching.
#[derive(Debug, Clone, Default)]
pub struct CorrelationStatistics {
    /// Total requests recorded
    pub requests_recorded: u64,
    /// Responses successfully matched to requests
    pub responses_matched: u64,
    /// Responses that couldn't be matched
    pub responses_unmatched: u64,
    /// Requests that timed out without response
    pub timeouts: u64,
}

impl CorrelationStatistics {
    /// Calculate match rate (0.0 - 1.0)
    pub fn match_rate(&self) -> f64 {
        if self.requests_recorded == 0 {
            0.0
        } else {
            self.responses_matched as f64 / self.requests_recorded as f64
        }
    }

    /// Calculate timeout rate (0.0 - 1.0)
    pub fn timeout_rate(&self) -> f64 {
        if self.requests_recorded == 0 {
            0.0
        } else {
            self.timeouts as f64 / self.requests_recorded as f64
        }
    }
}

impl std::fmt::Display for CorrelationStatistics {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Requests: {} | Matched: {} ({:.1}%) | Unmatched: {} | Timeouts: {}",
            self.requests_recorded,
            self.responses_matched,
            self.match_rate() * 100.0,
            self.responses_unmatched,
            self.timeouts
        )
    }
}

/// Type alias for event callback
type EventCallback = Box<dyn Fn(DiagnosticEvent, &DiagnosticSessionState) + Send + Sync>;

/// Context for callback trampoline
struct CallbackContext {
    callback: EventCallback,
}

/// Diagnostic session manager for UDS over DoIP analysis.
///
/// Provides complete diagnostic session management including:
/// - Session state tracking (Default/Programming/Extended)
/// - Security access level management
/// - Request/response correlation with timing validation
/// - Multi-ECU support
///
/// # Example
///
/// ```rust,ignore
/// use wadjet::diagnostic::{DiagnosticSessionManager, DiagnosticOptions, DiagnosticEvent};
///
/// let mut manager = DiagnosticSessionManager::new(DiagnosticOptions::default())?;
///
/// manager.on_event(|event, state| {
///     println!("{}: {}", event, state);
/// });
///
/// // Process DoIP packets from capture
/// for packet in packets {
///     manager.process(&packet)?;
/// }
///
/// // Get statistics
/// let stats = manager.statistics();
/// println!("Match rate: {:.1}%", stats.match_rate() * 100.0);
/// ```
pub struct DiagnosticSessionManager {
    handle: wadjet_sys::wadjet_diagnostic_session_manager_t,
    // Keep callback context alive
    _callback_ctx: Option<Arc<CallbackContext>>,
}

// Safety: The underlying C handle is thread-safe for the operations we expose
unsafe impl Send for DiagnosticSessionManager {}

impl DiagnosticSessionManager {
    /// Create a new diagnostic session manager.
    ///
    /// # Arguments
    ///
    /// * `options` - Configuration options
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// let options = DiagnosticOptions::default()
    ///     .p2_timeout(100)
    ///     .p2_star_timeout(10000);
    /// let manager = DiagnosticSessionManager::new(options)?;
    /// ```
    pub fn new(options: DiagnosticOptions) -> Result<Self> {
        let c_options = options.to_c();
        let mut handle: wadjet_sys::wadjet_diagnostic_session_manager_t = ptr::null_mut();

        let err = unsafe {
            wadjet_sys::wadjet_diagnostic_manager_create(&c_options, &mut handle)
        };

        check_error(err)?;

        if handle.is_null() {
            return Err(Error::NullPointer);
        }

        Ok(Self {
            handle,
            _callback_ctx: None,
        })
    }

    /// Create with default options.
    pub fn with_defaults() -> Result<Self> {
        Self::new(DiagnosticOptions::default())
    }

    /// Register an event callback.
    ///
    /// The callback will be invoked for each diagnostic event with the
    /// event type and current session state.
    ///
    /// # Note
    ///
    /// Only one callback can be registered at a time. Calling this method
    /// again will replace the previous callback.
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// manager.on_event(|event, state| {
    ///     match event {
    ///         DiagnosticEvent::SessionStarted => {
    ///             println!("New session with ECU 0x{:04X}", state.gateway_address);
    ///         }
    ///         DiagnosticEvent::SecurityUnlocked => {
    ///             println!("Security unlocked: level {}", state.security_level);
    ///         }
    ///         _ => {}
    ///     }
    /// });
    /// ```
    pub fn on_event<F>(&mut self, callback: F)
    where
        F: Fn(DiagnosticEvent, &DiagnosticSessionState) + Send + Sync + 'static,
    {
        let ctx = Arc::new(CallbackContext {
            callback: Box::new(callback),
        });

        let ctx_ptr = Arc::into_raw(ctx.clone()) as *mut c_void;

        unsafe {
            wadjet_sys::wadjet_diagnostic_manager_on_event(
                self.handle,
                Some(event_trampoline),
                ctx_ptr,
            );
        }

        // Keep reference to prevent drop
        self._callback_ctx = Some(ctx);
    }

    /// Process a raw DoIP packet.
    ///
    /// # Arguments
    ///
    /// * `data` - Raw DoIP packet data (including DoIP header)
    ///
    /// # Returns
    ///
    /// `Ok(())` if the packet was processed successfully.
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// // Process captured packet
    /// manager.process(&packet_data)?;
    /// ```
    pub fn process(&mut self, data: &[u8]) -> Result<()> {
        let err = unsafe {
            wadjet_sys::wadjet_diagnostic_manager_process(
                self.handle,
                data.as_ptr(),
                data.len(),
            )
        };
        check_error(err)
    }

    /// Get session state for an ECU.
    ///
    /// # Arguments
    ///
    /// * `ecu_address` - ECU logical address
    ///
    /// # Returns
    ///
    /// `Some(state)` if the ECU is being tracked, `None` otherwise.
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// if let Some(state) = manager.get_session(0x1234) {
    ///     println!("ECU session: {}", state);
    ///     if state.is_programming() {
    ///         println!("  In programming mode!");
    ///     }
    /// }
    /// ```
    pub fn get_session(&self, ecu_address: u16) -> Option<DiagnosticSessionState> {
        let mut state = unsafe { std::mem::zeroed::<wadjet_sys::wadjet_diagnostic_session_state_t>() };

        let err = unsafe {
            wadjet_sys::wadjet_diagnostic_manager_get_session(
                self.handle,
                ecu_address,
                &mut state,
            )
        };

        if err == wadjet_sys::wadjet_error_t::WADJET_OK {
            Some(DiagnosticSessionState::from_c(&state))
        } else {
            None
        }
    }

    /// Get the number of active ECU sessions.
    pub fn session_count(&self) -> usize {
        unsafe { wadjet_sys::wadjet_diagnostic_manager_session_count(self.handle) }
    }

    /// Get correlation statistics.
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// let stats = manager.statistics();
    /// println!("Match rate: {:.1}%", stats.match_rate() * 100.0);
    /// println!("Timeout rate: {:.1}%", stats.timeout_rate() * 100.0);
    /// ```
    pub fn statistics(&self) -> CorrelationStatistics {
        let mut requests_recorded: u64 = 0;
        let mut responses_matched: u64 = 0;
        let mut responses_unmatched: u64 = 0;
        let mut timeouts: u64 = 0;

        unsafe {
            wadjet_sys::wadjet_diagnostic_manager_statistics(
                self.handle,
                &mut requests_recorded,
                &mut responses_matched,
                &mut responses_unmatched,
                &mut timeouts,
            );
        }

        CorrelationStatistics {
            requests_recorded,
            responses_matched,
            responses_unmatched,
            timeouts,
        }
    }

    /// Check for timed-out requests.
    ///
    /// This should be called periodically to detect requests that haven't
    /// received responses within the configured timeout.
    ///
    /// # Returns
    ///
    /// Number of requests that timed out.
    pub fn check_timeouts(&mut self) -> usize {
        unsafe { wadjet_sys::wadjet_diagnostic_manager_check_timeouts(self.handle) }
    }
}

impl Drop for DiagnosticSessionManager {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_diagnostic_manager_destroy(self.handle);
            }
        }
    }
}

/// Trampoline function for C callback to Rust closure
unsafe extern "C" fn event_trampoline(
    event: wadjet_sys::wadjet_diagnostic_event_t,
    state: *const wadjet_sys::wadjet_diagnostic_session_state_t,
    user_data: *mut c_void,
) {
    if user_data.is_null() || state.is_null() {
        return;
    }

    // Reconstruct Arc without taking ownership (we just borrow)
    let ctx = &*(user_data as *const CallbackContext);

    if let Some(rust_event) = DiagnosticEvent::from_c(event) {
        let rust_state = DiagnosticSessionState::from_c(&*state);
        (ctx.callback)(rust_event, &rust_state);
    }
}

/// Standard diagnostic timing parameters (ISO 14229).
#[derive(Debug, Clone, Copy)]
pub struct DiagnosticTiming {
    /// P2 Server Max - Time for initial response (default 50ms)
    pub p2_server_max_ms: u32,
    /// P2* Server Max - Time after ResponsePending (default 5000ms)
    pub p2_star_server_max_ms: u32,
    /// S3 Server - Session keep-alive timeout (default 5000ms)
    pub s3_server_ms: u32,
    /// P3 Client - Time between consecutive requests (default 50ms)
    pub p3_client_ms: u32,
}

impl Default for DiagnosticTiming {
    fn default() -> Self {
        Self {
            p2_server_max_ms: 50,
            p2_star_server_max_ms: 5000,
            s3_server_ms: 5000,
            p3_client_ms: 50,
        }
    }
}

impl DiagnosticTiming {
    /// Standard timing for programming sessions
    pub fn programming() -> Self {
        Self {
            p2_server_max_ms: 50,
            p2_star_server_max_ms: 5000,
            s3_server_ms: 5000,
            p3_client_ms: 50,
        }
    }

    /// Check if elapsed time is within P2 timeout
    pub fn within_p2(&self, elapsed_ms: u32) -> bool {
        elapsed_ms <= self.p2_server_max_ms
    }

    /// Check if elapsed time is within P2* timeout
    pub fn within_p2_star(&self, elapsed_ms: u32) -> bool {
        elapsed_ms <= self.p2_star_server_max_ms
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_diagnostic_event_name() {
        assert_eq!(DiagnosticEvent::SessionStarted.name(), "SessionStarted");
        assert_eq!(DiagnosticEvent::SecurityUnlocked.name(), "SecurityUnlocked");
        assert_eq!(DiagnosticEvent::FlashCompleted.name(), "FlashCompleted");
    }

    #[test]
    fn test_diagnostic_event_categories() {
        assert!(DiagnosticEvent::SessionStarted.is_session_event());
        assert!(DiagnosticEvent::SecurityUnlocked.is_security_event());
        assert!(DiagnosticEvent::FlashStarted.is_flash_event());

        assert!(!DiagnosticEvent::RequestSent.is_session_event());
        assert!(!DiagnosticEvent::RequestSent.is_security_event());
        assert!(!DiagnosticEvent::RequestSent.is_flash_event());
    }

    #[test]
    fn test_session_type_name() {
        assert_eq!(SessionType::Default.name(), "DefaultSession");
        assert_eq!(SessionType::Programming.name(), "ProgrammingSession");
        assert_eq!(SessionType::Extended.name(), "ExtendedDiagnosticSession");
    }

    #[test]
    fn test_diagnostic_options_builder() {
        let opts = DiagnosticOptions::new()
            .p2_timeout(100)
            .p2_star_timeout(10000)
            .s3_timeout(6000)
            .max_ecus(128)
            .correlation(true)
            .timeout_detection(false);

        assert_eq!(opts.p2_server_max_ms, 100);
        assert_eq!(opts.p2_star_server_max_ms, 10000);
        assert_eq!(opts.s3_server_ms, 6000);
        assert_eq!(opts.max_ecus, 128);
        assert!(opts.enable_correlation);
        assert!(!opts.enable_timeout_detection);
    }

    #[test]
    fn test_correlation_statistics() {
        let stats = CorrelationStatistics {
            requests_recorded: 100,
            responses_matched: 95,
            responses_unmatched: 3,
            timeouts: 2,
        };

        assert!((stats.match_rate() - 0.95).abs() < 0.001);
        assert!((stats.timeout_rate() - 0.02).abs() < 0.001);
    }

    #[test]
    fn test_correlation_statistics_empty() {
        let stats = CorrelationStatistics::default();
        assert_eq!(stats.match_rate(), 0.0);
        assert_eq!(stats.timeout_rate(), 0.0);
    }

    #[test]
    fn test_diagnostic_timing() {
        let timing = DiagnosticTiming::default();
        assert!(timing.within_p2(50));
        assert!(!timing.within_p2(51));
        assert!(timing.within_p2_star(5000));
        assert!(!timing.within_p2_star(5001));
    }
}

/// Unsafe FFI bindings to Wadjet-Link C ABI
///
/// This module provides low-level C FFI bindings for the distributed testing
/// primitives. These are unsafe and should be wrapped by safe abstractions
/// before use in application code.
///
/// **Task**: T116 - Implement unsafe FFI bindings

use libc::{c_char, c_uint, c_int, c_bool, uint32_t, uint16_t, int64_t, int32_t, size_t, void};
use std::ffi::{CStr, CString};
use std::ptr::NonNull;

// ============================================================================
// Error Codes (matching C ABI wadjet_dist_error_t)
// ============================================================================

#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WadjetDistError {
    Ok = 0,
    InvalidConfig = 1,
    CoordinatorUnavailable = 2,
    NodeUnavailable = 3,
    ClockSyncFailed = 4,
    BarrierTimeout = 5,
    NoClockSync = 6,
    TestFailed = 7,
    InvalidState = 8,
    Timeout = 9,
    Unknown = 99,
}

impl WadjetDistError {
    /// Convert error code to string
    pub fn as_str(&self) -> &'static str {
        match self {
            WadjetDistError::Ok => "Success",
            WadjetDistError::InvalidConfig => "Invalid configuration",
            WadjetDistError::CoordinatorUnavailable => "Coordinator unavailable",
            WadjetDistError::NodeUnavailable => "Node unavailable",
            WadjetDistError::ClockSyncFailed => "Clock synchronization failed",
            WadjetDistError::BarrierTimeout => "Barrier synchronization timeout",
            WadjetDistError::NoClockSync => "No clock synchronization detected",
            WadjetDistError::TestFailed => "Test execution failed",
            WadjetDistError::InvalidState => "Invalid test state",
            WadjetDistError::Timeout => "Operation timeout",
            WadjetDistError::Unknown => "Unknown error",
        }
    }
}

impl From<i32> for WadjetDistError {
    fn from(code: i32) -> Self {
        match code {
            0 => WadjetDistError::Ok,
            1 => WadjetDistError::InvalidConfig,
            2 => WadjetDistError::CoordinatorUnavailable,
            3 => WadjetDistError::NodeUnavailable,
            4 => WadjetDistError::ClockSyncFailed,
            5 => WadjetDistError::BarrierTimeout,
            6 => WadjetDistError::NoClockSync,
            7 => WadjetDistError::TestFailed,
            8 => WadjetDistError::InvalidState,
            9 => WadjetDistError::Timeout,
            _ => WadjetDistError::Unknown,
        }
    }
}

// ============================================================================
// Clock Synchronization Enums
// ============================================================================

#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WadjetClockSyncMethod {
    None = 0,
    Ntp = 1,
    Gptp = 2,
    Unknown = 3,
}

// ============================================================================
// Type Definitions (matching C ABI)
// ============================================================================

pub type WadjetNodeId = *const c_char;
pub type WadjetTimestampNs = int64_t;

/// Opaque handle to coordinator (opaque C struct)
#[repr(C)]
pub struct WadjetCoordinator {
    _data: [u8; 0],
    _marker: std::marker::PhantomData<(*mut u8, std::marker::Sync)>,
}

/// Opaque handle to node
#[repr(C)]
pub struct WadjetTestNode {
    _data: [u8; 0],
    _marker: std::marker::PhantomData<(*mut u8, std::marker::Sync)>,
}

/// Opaque handle to barrier
#[repr(C)]
pub struct WadjetSyncBarrier {
    _data: [u8; 0],
    _marker: std::marker::PhantomData<(*mut u8, std::marker::Sync)>,
}

/// Clock synchronization status struct (C-compatible layout)
#[repr(C)]
#[derive(Debug, Clone)]
pub struct WadjetClockSyncStatus {
    pub method: i32,                  // WadjetClockSyncMethod
    pub is_synchronized: c_bool,
    pub estimated_offset_ns: int64_t,
    pub max_error_ns: int64_t,
    pub grandmaster_id: *const c_char,
}

/// Node information for configuration
#[repr(C)]
pub struct WadjetNodeInfo {
    pub node_id: WadjetNodeId,
    pub hostname: *const c_char,
    pub grpc_port: uint16_t,
    pub capture_interfaces: *const *const c_char,
    pub interface_count: size_t,
    pub version: *const c_char,
}

/// Coordinator configuration struct
#[repr(C)]
pub struct WadjetCoordinatorConfig {
    pub coordinator_host: *const c_char,
    pub coordinator_port: uint16_t,
    pub heartbeat_timeout_ms: uint32_t,
    pub barrier_sync_timeout_ms: uint32_t,
    pub enable_gptp_sync: c_bool,
    pub enable_ntp_sync: c_bool,
}

/// Single node test result
#[repr(C)]
pub struct WadjetNodeResult {
    pub node_id: WadjetNodeId,
    pub healthy: c_bool,
    pub passed_assertions: int32_t,
    pub failed_assertions: int32_t,
    pub total_duration_ns: int64_t,
    pub pcap_file_path: *const c_char,
    pub error_message: *const c_char,
}

/// Aggregated test results
#[repr(C)]
pub struct WadjetAggregatedResult {
    pub test_name: *const c_char,
    pub node_results: *const WadjetNodeResult,
    pub node_count: int32_t,
    pub test_start_time_ns: int64_t,
    pub test_end_time_ns: int64_t,
    pub overall_status: int32_t,
    pub total_duration_ns: int64_t,
    pub total_assertions: int32_t,
    pub passed_assertions: int32_t,
    pub failed_assertions: int32_t,
}

// ============================================================================
// FFI Function Declarations
// ============================================================================

extern "C" {
    // Error handling
    pub fn wadjet_dist_error_message(error: i32) -> *const c_char;
    pub fn wadjet_dist_last_error() -> *const c_char;
    pub fn wadjet_dist_clear_error();
    pub fn wadjet_dist_set_error(error_code: i32, message: *const c_char);
    pub fn wadjet_dist_get_error_code() -> i32;
    pub fn wadjet_dist_get_error_message() -> *const c_char;
    pub fn wadjet_dist_init_error_context() -> i32;
    pub fn wadjet_dist_has_error() -> c_bool;

    // Coordinator lifecycle (T100)
    pub fn wadjet_coordinator_create(
        config: *const WadjetCoordinatorConfig,
        error_code: *mut i32,
    ) -> *mut WadjetCoordinator;

    pub fn wadjet_coordinator_start(
        coordinator: *mut WadjetCoordinator,
        nodes: *const WadjetNodeInfo,
        node_count: size_t,
    ) -> i32;

    pub fn wadjet_coordinator_run_test(
        coordinator: *mut WadjetCoordinator,
        test_name: *const c_char,
        test_duration_ms: uint32_t,
    ) -> i32;

    pub fn wadjet_coordinator_get_results(
        coordinator: *mut WadjetCoordinator,
    ) -> *const WadjetAggregatedResult;

    pub fn wadjet_coordinator_stop(coordinator: *mut WadjetCoordinator) -> i32;

    pub fn wadjet_coordinator_destroy(coordinator: *mut WadjetCoordinator);

    // Node lifecycle (T101)
    pub fn wadjet_node_create(
        node_id: WadjetNodeId,
        coordinator_host: *const c_char,
        coordinator_port: uint16_t,
        error_code: *mut i32,
    ) -> *mut WadjetTestNode;

    pub fn wadjet_node_start(
        node: *mut WadjetTestNode,
        capture_interface: *const c_char,
    ) -> i32;

    pub fn wadjet_node_wait_barrier(
        node: *mut WadjetTestNode,
        timeout_ms: uint32_t,
    ) -> i32;

    pub fn wadjet_node_get_clock_sync_status(
        node: *mut WadjetTestNode,
    ) -> WadjetClockSyncStatus;

    pub fn wadjet_node_stop(node: *mut WadjetTestNode) -> i32;

    pub fn wadjet_node_destroy(node: *mut WadjetTestNode);

    // Sync barrier (T102)
    pub fn wadjet_sync_barrier_create(
        barrier_id: *const c_char,
        expected_participants: uint32_t,
        timeout_ms: uint32_t,
        error_code: *mut i32,
    ) -> *mut WadjetSyncBarrier;

    pub fn wadjet_sync_barrier_wait(
        barrier: *mut WadjetSyncBarrier,
        timeout_ms: uint32_t,
    ) -> i32;

    pub fn wadjet_sync_barrier_is_satisfied(
        barrier: *mut WadjetSyncBarrier,
    ) -> c_bool;

    pub fn wadjet_sync_barrier_participant_count(
        barrier: *mut WadjetSyncBarrier,
    ) -> uint32_t;

    pub fn wadjet_sync_barrier_destroy(barrier: *mut WadjetSyncBarrier);

    // Result export functions
    pub fn wadjet_aggregated_result_to_junit_xml(
        results: *const WadjetAggregatedResult,
        output_path: *const c_char,
    ) -> i32;

    pub fn wadjet_aggregated_result_to_json(
        results: *const WadjetAggregatedResult,
        output_path: *const c_char,
    ) -> i32;

    pub fn wadjet_aggregated_result_summary(
        results: *const WadjetAggregatedResult,
    ) -> *const c_char;
}

// ============================================================================
// Helper Functions for String Conversion
// ============================================================================

/// Convert Rust string to C string (unsafe - caller must use immediately)
pub fn string_to_c(s: &str) -> CString {
    CString::new(s).unwrap_or_else(|_| CString::new("").unwrap())
}

/// Convert C string to Rust string (safe - handles null pointers)
pub fn c_string_to_rust(c_str: *const c_char) -> Option<String> {
    if c_str.is_null() {
        return None;
    }
    unsafe {
        CStr::from_ptr(c_str)
            .to_str()
            .ok()
            .map(|s| s.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_code_conversion() {
        assert_eq!(WadjetDistError::Ok as i32, 0);
        assert_eq!(WadjetDistError::InvalidConfig as i32, 1);
        assert_eq!(WadjetDistError::Unknown as i32, 99);
    }

    #[test]
    fn test_clock_sync_method() {
        assert_eq!(WadjetClockSyncMethod::None as i32, 0);
        assert_eq!(WadjetClockSyncMethod::Ntp as i32, 1);
        assert_eq!(WadjetClockSyncMethod::Gptp as i32, 2);
    }
}

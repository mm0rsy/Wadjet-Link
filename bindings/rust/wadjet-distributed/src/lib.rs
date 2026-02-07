/// # Wadjet-Distributed: Rust Bindings for Wadjet-Link Distributed Testing
///
/// This crate provides safe Rust bindings for the Wadjet-Link distributed
/// packet capture and testing framework.
///
/// ## Features
///
/// - **Safe FFI Bindings**: Zero-cost abstractions over C ABI
/// - **Thread-Safe**: All types implement Send and Sync where appropriate
/// - **RAII Pattern**: Automatic resource cleanup via Drop trait
/// - **Error Handling**: Rust Result types instead of error codes
///
/// ## Quick Start
///
/// ```no_run
/// use wadjet_distributed::sync_barrier::SyncBarrier;
/// use wadjet_distributed::timestamp::TimestampNormalizer;
///
/// // Create a barrier for 3-node synchronization
/// let barrier = SyncBarrier::new("test_start", 3)?;
///
/// // Check clock synchronization
/// let normalizer = TimestampNormalizer::new();
/// if normalizer.is_synchronized() {
///     println!("Clock is synchronized");
/// }
///
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
///
/// ## Architecture
///
/// The crate is organized into modules:
///
/// - **ffi**: Low-level C FFI bindings (unsafe)
/// - **sync_barrier**: Safe SyncBarrier wrapper with thread support
/// - **timestamp**: Clock synchronization detection and normalization
/// - **matchers**: Distributed assertion matchers
///
/// ## Memory Safety
///
/// All unsafe code is encapsulated in the `ffi` module. Higher-level modules
/// provide safe abstractions that implement RAII for resource management.
///
/// ## Thread Safety
///
/// All public types are Send + Sync where the underlying C implementation is
/// thread-safe. Multi-threaded tests can safely use shared barriers and
/// normalizers.

#![warn(missing_docs)]
#![warn(unsafe_code)]

pub mod ffi;
pub mod matchers;
pub mod sync_barrier;
pub mod timestamp;

pub use matchers::{
    DistributedMatcher, FailureDetails, LatencyMatcher, MatchResult, MatcherBuilder,
    PacketContentMatcher, PacketCountMatcher, ProtocolDecoderMatcher,
    TimestampOrderingMatcher,
};
pub use sync_barrier::{BarrierError, BarrierResult, SyncBarrier};
pub use timestamp::{ClockSyncMethod, ClockSyncStatus, TimestampNormalizer};

/// Re-export commonly used FFI types
pub mod types {
    pub use crate::ffi::{
        WadjetAggregatedResult, WadjetClockSyncMethod, WadjetClockSyncStatus,
        WadjetCoordinator, WadjetCoordinatorConfig, WadjetDistError, WadjetNode,
        WadjetNodeConfig, WadjetNodeId, WadjetNodeInfo, WadjetNodeResult,
        WadjetSyncBarrier, WadjetTestNode, WadjetTimestampNs,
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_module_imports() {
        // Verify all public types are accessible
        let _error: ffi::WadjetDistError;
        let _method: ClockSyncMethod;
    }
}

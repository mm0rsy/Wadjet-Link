/// Safe Rust wrapper for SyncBarrier
///
/// Provides a safe, RAII-style interface to the C FFI SyncBarrier.
///
/// **Task**: T117 - Implement safe SyncBarrier wrapper

use crate::ffi;
use std::ffi::CString;
use std::ptr::NonNull;
use std::time::Duration;

/// Error type for barrier operations
#[derive(Debug, Clone)]
pub struct BarrierError {
    code: ffi::WadjetDistError,
    message: String,
}

impl BarrierError {
    fn new(code: ffi::WadjetDistError, message: impl Into<String>) -> Self {
        BarrierError {
            code,
            message: message.into(),
        }
    }

    pub fn code(&self) -> ffi::WadjetDistError {
        self.code
    }

    pub fn message(&self) -> &str {
        &self.message
    }
}

impl std::fmt::Display for BarrierError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.code.as_str(), self.message)
    }
}

impl std::error::Error for BarrierError {}

pub type BarrierResult<T> = Result<T, BarrierError>;

/// Safe wrapper for synchronization barrier
///
/// **RAII Pattern**: Automatically destroys the C barrier on drop.
///
/// **Thread Safety**: The underlying C barrier is thread-safe.
/// Multiple threads can safely wait on the same barrier.
///
/// Example:
/// ```no_run
/// let barrier = SyncBarrier::new("test_sync", 3)?;
/// barrier.wait()?;  // Blocks until 3 threads arrive
/// assert!(barrier.is_satisfied());
/// ```
pub struct SyncBarrier {
    inner: NonNull<ffi::WadjetSyncBarrier>,
    id: String,
}

impl SyncBarrier {
    /// Create a new synchronization barrier
    ///
    /// # Arguments
    /// * `barrier_id` - Unique identifier for this barrier
    /// * `expected_participants` - Number of threads expected to wait
    ///
    /// # Errors
    /// Returns error if barrier creation fails
    pub fn new(barrier_id: &str, expected_participants: u32) -> BarrierResult<Self> {
        if expected_participants == 0 {
            return Err(BarrierError::new(
                ffi::WadjetDistError::InvalidConfig,
                "expected_participants must be > 0",
            ));
        }

        let c_id = CString::new(barrier_id)
            .map_err(|_| {
                BarrierError::new(
                    ffi::WadjetDistError::InvalidConfig,
                    "barrier_id contains null byte",
                )
            })?;

        let mut error_code: i32 = 0;

        unsafe {
            let ptr = ffi::wadjet_sync_barrier_create(
                c_id.as_ptr(),
                expected_participants,
                10000,  // Default 10 second timeout
                &mut error_code,
            );

            if ptr.is_null() {
                let err = ffi::WadjetDistError::from(error_code);
                let msg = ffi::c_string_to_rust(ffi::wadjet_dist_last_error())
                    .unwrap_or_else(|| err.as_str().to_string());
                return Err(BarrierError::new(err, msg));
            }

            Ok(SyncBarrier {
                inner: NonNull::new_unchecked(ptr),
                id: barrier_id.to_string(),
            })
        }
    }

    /// Wait at the barrier
    ///
    /// Blocks until all expected participants arrive or timeout occurs.
    ///
    /// # Arguments
    /// * `timeout` - How long to wait before timing out
    ///
    /// # Returns
    /// - `Ok(())` when barrier is satisfied
    /// - `Err` if timeout or other error
    ///
    /// # Panics
    /// Never panics; errors are returned as Result
    pub fn wait(&self, timeout: Option<Duration>) -> BarrierResult<()> {
        let timeout_ms = timeout
            .map(|d| d.as_millis() as u32)
            .unwrap_or(0);

        unsafe {
            let err_code = ffi::wadjet_sync_barrier_wait(self.inner.as_ptr(), timeout_ms);

            if err_code == 0 {
                Ok(())
            } else {
                let err = ffi::WadjetDistError::from(err_code);
                let msg = ffi::c_string_to_rust(ffi::wadjet_dist_last_error())
                    .unwrap_or_else(|| err.as_str().to_string());
                Err(BarrierError::new(err, msg))
            }
        }
    }

    /// Wait indefinitely at the barrier
    ///
    /// Equivalent to `wait(None)`
    pub fn wait_infinite(&self) -> BarrierResult<()> {
        self.wait(None)
    }

    /// Check if barrier has been satisfied
    ///
    /// Returns true if all expected participants have arrived.
    /// Safe to call from any thread.
    pub fn is_satisfied(&self) -> bool {
        unsafe { ffi::wadjet_sync_barrier_is_satisfied(self.inner.as_ptr()) != 0 }
    }

    /// Get current number of participants waiting at barrier
    pub fn participant_count(&self) -> u32 {
        unsafe { ffi::wadjet_sync_barrier_participant_count(self.inner.as_ptr()) }
    }

    /// Get the barrier identifier
    pub fn id(&self) -> &str {
        &self.id
    }
}

/// Automatic cleanup on drop
impl Drop for SyncBarrier {
    fn drop(&mut self) {
        unsafe {
            ffi::wadjet_sync_barrier_destroy(self.inner.as_ptr());
        }
    }
}

/// Sync and Send are safe because the underlying C barrier is thread-safe
unsafe impl Send for SyncBarrier {}
unsafe impl Sync for SyncBarrier {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_barrier_creation() {
        let result = SyncBarrier::new("test_barrier", 1);
        assert!(result.is_ok());
    }

    #[test]
    fn test_barrier_invalid_participants() {
        let result = SyncBarrier::new("test_barrier", 0);
        assert!(result.is_err());
        assert_eq!(result.unwrap_err().code(), ffi::WadjetDistError::InvalidConfig);
    }

    #[test]
    fn test_barrier_is_satisfied() {
        let barrier = SyncBarrier::new("test_satisfied", 1).unwrap();
        // Single participant barrier should be satisfied immediately
        assert!(barrier.is_satisfied() || barrier.wait(Some(Duration::from_secs(1))).is_ok());
    }

    #[test]
    fn test_barrier_participant_count() {
        let barrier = SyncBarrier::new("test_count", 3).unwrap();
        let count = barrier.participant_count();
        // Count should be 0 initially or >= 1 depending on thread state
        assert!(count <= 3);
    }
}

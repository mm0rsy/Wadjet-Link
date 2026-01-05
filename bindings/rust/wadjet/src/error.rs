//! Error types for Wadjet.

use std::ffi::CStr;
use thiserror::Error;

/// Result type alias using Wadjet's Error type.
pub type Result<T> = std::result::Result<T, Error>;

/// Errors that can occur when using Wadjet.
#[derive(Error, Debug, Clone)]
pub enum Error {
    /// Invalid parameter passed to function
    #[error("Invalid parameter: {0}")]
    InvalidParameter(String),

    /// Operation failed (generic error)
    #[error("Operation failed: {0}")]
    OperationFailed(String),

    /// Device not found
    #[error("Device not found: {0}")]
    DeviceNotFound(String),

    /// Permission denied
    #[error("Permission denied: {0}")]
    PermissionDenied(String),

    /// Resource busy
    #[error("Resource busy: {0}")]
    Busy(String),

    /// Timeout occurred
    #[error("Operation timed out")]
    Timeout,

    /// End of file/stream
    #[error("End of file")]
    EndOfFile,

    /// Buffer too small
    #[error("Buffer too small")]
    BufferTooSmall,

    /// Unsupported operation
    #[error("Unsupported operation: {0}")]
    Unsupported(String),

    /// Not initialized
    #[error("Library not initialized")]
    NotInitialized,

    /// I/O error
    #[error("I/O error: {0}")]
    Io(String),

    /// File not found
    #[error("File not found: {0}")]
    FileNotFound(String),

    /// Invalid format
    #[error("Invalid format: {0}")]
    InvalidFormat(String),

    /// Null pointer returned
    #[error("Null pointer returned")]
    NullPointer,

    /// Resource not found
    #[error("Resource not found")]
    NotFound,

    /// Unknown error with code
    #[error("Unknown error (code {0}): {1}")]
    Unknown(i32, String),
}

/// Convert a wadjet_error_t to a Result
pub(crate) fn check_error(err: wadjet_sys::wadjet_error_t) -> Result<()> {
    use wadjet_sys::wadjet_error_t::*;
    
    match err {
        WADJET_OK => Ok(()),
        WADJET_ERR_INVALID_ARGUMENT => {
            Err(Error::InvalidParameter(get_last_error_message()))
        }
        WADJET_ERR_NOT_FOUND => {
            Err(Error::DeviceNotFound(get_last_error_message()))
        }
        WADJET_ERR_PERMISSION => {
            Err(Error::PermissionDenied(get_last_error_message()))
        }
        WADJET_ERR_IO => {
            Err(Error::Io(get_last_error_message()))
        }
        WADJET_ERR_TIMEOUT => Err(Error::Timeout),
        WADJET_ERR_DECODE => {
            Err(Error::InvalidFormat(get_last_error_message()))
        }
        WADJET_ERR_INVALID_STATE => {
            Err(Error::OperationFailed(get_last_error_message()))
        }
        WADJET_ERR_OUT_OF_MEMORY => {
            Err(Error::OperationFailed("Out of memory".into()))
        }
        WADJET_ERR_NOT_SUPPORTED => {
            Err(Error::Unsupported(get_last_error_message()))
        }
        WADJET_ERR_UNKNOWN => {
            Err(Error::Unknown(99, get_last_error_message()))
        }
    }
}

/// Get the last error message from the C library
pub(crate) fn get_last_error_message() -> String {
    unsafe {
        let ptr = wadjet_sys::wadjet_last_error();
        if ptr.is_null() {
            return String::from("No error message available");
        }
        CStr::from_ptr(ptr)
            .to_string_lossy()
            .into_owned()
    }
}

/// Get error message for a specific error code
pub fn error_message(err: wadjet_sys::wadjet_error_t) -> String {
    unsafe {
        let ptr = wadjet_sys::wadjet_error_message(err);
        if ptr.is_null() {
            return String::from("Unknown error");
        }
        CStr::from_ptr(ptr)
            .to_string_lossy()
            .into_owned()
    }
}

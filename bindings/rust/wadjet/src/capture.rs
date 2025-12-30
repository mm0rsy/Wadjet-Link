//! Capture session for live packet capture.

use crate::error::{check_error, Result};
use crate::packet::Packet;
use std::ffi::{CStr, CString};
use std::ptr;

/// Capture options for configuring a capture session.
#[derive(Debug, Clone)]
pub struct CaptureOptions {
    /// Maximum bytes to capture per packet (default: 65535)
    pub snaplen: u32,
    /// Enable promiscuous mode (default: true)
    pub promiscuous: bool,
    /// Timeout in milliseconds for reads (default: 1000)
    pub timeout_ms: u32,
    /// Enable immediate mode for low latency (default: true)
    pub immediate_mode: bool,
    /// Buffer size in bytes (default: 2MB)
    pub buffer_size: u32,
    /// BPF filter expression (optional)
    pub filter: Option<String>,
}

impl Default for CaptureOptions {
    fn default() -> Self {
        Self {
            snaplen: 65535,
            promiscuous: true,
            timeout_ms: 1000,
            immediate_mode: true,
            buffer_size: 2 * 1024 * 1024,
            filter: None,
        }
    }
}

impl CaptureOptions {
    /// Create new capture options with defaults
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the snap length
    pub fn snaplen(mut self, snaplen: u32) -> Self {
        self.snaplen = snaplen;
        self
    }

    /// Enable or disable promiscuous mode
    pub fn promiscuous(mut self, promiscuous: bool) -> Self {
        self.promiscuous = promiscuous;
        self
    }

    /// Set the read timeout in milliseconds
    pub fn timeout_ms(mut self, timeout_ms: u32) -> Self {
        self.timeout_ms = timeout_ms;
        self
    }

    /// Enable or disable immediate mode
    pub fn immediate_mode(mut self, immediate: bool) -> Self {
        self.immediate_mode = immediate;
        self
    }

    /// Set the buffer size
    pub fn buffer_size(mut self, size: u32) -> Self {
        self.buffer_size = size;
        self
    }

    /// Set the BPF filter expression
    pub fn filter<S: Into<String>>(mut self, filter: S) -> Self {
        self.filter = Some(filter.into());
        self
    }

    /// Convert to C type
    fn to_c(&self) -> wadjet_sys::wadjet_capture_options_t {
        wadjet_sys::wadjet_capture_options_t {
            snaplen: self.snaplen,
            promiscuous: self.promiscuous,
            timeout_ms: self.timeout_ms,
            immediate_mode: self.immediate_mode,
            buffer_size: self.buffer_size,
            filter: ptr::null(),
        }
    }
}

/// Capture statistics
#[derive(Debug, Clone, Default)]
pub struct CaptureStatistics {
    /// Packets received by the capture session
    pub packets_received: u64,
    /// Packets dropped by the capture session
    pub packets_dropped: u64,
    /// Packets dropped by the interface
    pub packets_dropped_interface: u64,
}

/// A live packet capture session.
///
/// # Example
///
/// ```rust,ignore
/// use wadjet::{CaptureSession, CaptureOptions};
///
/// let options = CaptureOptions::default()
///     .snaplen(1500)
///     .timeout_ms(500);
///
/// let mut session = CaptureSession::open("eth0", &options)?;
///
/// while let Some(packet) = session.next_packet()? {
///     println!("Captured {} bytes", packet.len());
/// }
/// ```
pub struct CaptureSession {
    handle: *mut wadjet_sys::wadjet_capture_session_t,
}

impl CaptureSession {
    /// Open a capture session on the specified device.
    ///
    /// # Arguments
    ///
    /// * `device` - The name of the network device to capture on
    /// * `options` - Capture options
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// let session = CaptureSession::open("eth0", &CaptureOptions::default())?;
    /// ```
    pub fn open(device: &str, options: &CaptureOptions) -> Result<Self> {
        let device_c = CString::new(device).map_err(|_| {
            crate::Error::InvalidParameter("Device name contains null byte".into())
        })?;

        let mut c_options = options.to_c();
        
        // Handle filter string
        let filter_c = options.filter.as_ref().map(|f| {
            CString::new(f.as_str()).expect("Filter contains null byte")
        });
        if let Some(ref f) = filter_c {
            c_options.filter = f.as_ptr();
        }

        let mut handle: *mut wadjet_sys::wadjet_capture_session_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_capture_open(device_c.as_ptr(), &c_options, &mut handle)
        };
        
        check_error(err)?;
        
        if handle.is_null() {
            return Err(crate::Error::OperationFailed(
                "Failed to create capture session".into()
            ));
        }

        Ok(Self { handle })
    }

    /// Get the next packet from the capture session.
    ///
    /// Returns `Ok(Some(packet))` if a packet was captured,
    /// `Ok(None)` if the timeout expired with no packet,
    /// or an error if something went wrong.
    pub fn next_packet(&mut self) -> Result<Option<Packet>> {
        let mut packet_handle: *mut wadjet_sys::wadjet_packet_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_capture_next_packet(self.handle, &mut packet_handle)
        };

        // Timeout is not an error, just means no packet available
        if err == wadjet_sys::wadjet_error_t::WADJET_ERROR_TIMEOUT {
            return Ok(None);
        }

        check_error(err)?;

        if packet_handle.is_null() {
            return Ok(None);
        }

        Ok(Some(Packet::from_handle(packet_handle)))
    }

    /// Set the BPF filter for this capture session.
    ///
    /// # Arguments
    ///
    /// * `filter` - A BPF filter expression (e.g., "tcp port 80")
    pub fn set_filter(&mut self, filter: &str) -> Result<()> {
        let filter_c = CString::new(filter).map_err(|_| {
            crate::Error::InvalidParameter("Filter contains null byte".into())
        })?;

        let err = unsafe {
            wadjet_sys::wadjet_capture_set_filter(self.handle, filter_c.as_ptr())
        };

        check_error(err)
    }

    /// Get capture statistics.
    pub fn statistics(&self) -> Result<CaptureStatistics> {
        let mut received: u64 = 0;
        let mut dropped: u64 = 0;
        let mut dropped_if: u64 = 0;

        let err = unsafe {
            wadjet_sys::wadjet_capture_stats(
                self.handle,
                &mut received,
                &mut dropped,
                &mut dropped_if,
            )
        };

        check_error(err)?;

        Ok(CaptureStatistics {
            packets_received: received,
            packets_dropped: dropped,
            packets_dropped_interface: dropped_if,
        })
    }

    /// Get the device name this session is capturing on.
    pub fn device_name(&self) -> Option<String> {
        unsafe {
            let ptr = wadjet_sys::wadjet_capture_device_name(self.handle);
            if ptr.is_null() {
                return None;
            }
            Some(CStr::from_ptr(ptr).to_string_lossy().into_owned())
        }
    }

    /// Get the link type (data link layer type).
    pub fn link_type(&self) -> i32 {
        unsafe { wadjet_sys::wadjet_capture_link_type(self.handle) }
    }

    /// Inject a packet on the network.
    ///
    /// # Arguments
    ///
    /// * `data` - The raw packet data to inject
    ///
    /// # Returns
    ///
    /// The number of bytes sent, or an error.
    pub fn inject(&mut self, data: &[u8]) -> Result<usize> {
        let mut bytes_sent: usize = 0;

        let err = unsafe {
            wadjet_sys::wadjet_capture_inject(
                self.handle,
                data.as_ptr(),
                data.len(),
                &mut bytes_sent,
            )
        };

        check_error(err)?;
        Ok(bytes_sent)
    }

    /// Break out of the capture loop.
    ///
    /// This can be called from a signal handler or another thread
    /// to stop an ongoing capture.
    pub fn break_loop(&mut self) {
        unsafe {
            wadjet_sys::wadjet_capture_break_loop(self.handle);
        }
    }
}

impl Drop for CaptureSession {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_capture_close(self.handle);
            }
        }
    }
}

// CaptureSession is not thread-safe, but can be sent between threads
unsafe impl Send for CaptureSession {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_capture_options_default() {
        let opts = CaptureOptions::default();
        assert_eq!(opts.snaplen, 65535);
        assert!(opts.promiscuous);
        assert_eq!(opts.timeout_ms, 1000);
        assert!(opts.immediate_mode);
    }

    #[test]
    fn test_capture_options_builder() {
        let opts = CaptureOptions::new()
            .snaplen(1500)
            .promiscuous(false)
            .timeout_ms(500)
            .filter("tcp port 80");

        assert_eq!(opts.snaplen, 1500);
        assert!(!opts.promiscuous);
        assert_eq!(opts.timeout_ms, 500);
        assert_eq!(opts.filter, Some("tcp port 80".to_string()));
    }
}

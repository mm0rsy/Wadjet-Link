//! # Wadjet
//!
//! Safe Rust bindings for Wadjet-Link, an automotive Ethernet capture and analysis library.
//!
//! Wadjet-Link provides high-performance packet capture and protocol decoding for automotive
//! network protocols including SOME/IP, DoIP, and standard Ethernet/IP protocols.
//!
//! ## Features
//!
//! - Live packet capture from network interfaces
//! - PCAP file reading and writing
//! - Protocol decoding (Ethernet, IPv4, UDP, TCP, SOME/IP, DoIP)
//! - BPF filter support
//! - Cross-platform support (Linux, Windows)
//!
//! ## Quick Start
//!
//! ```rust,ignore
//! use wadjet::{CaptureSession, CaptureOptions, Result};
//!
//! fn main() -> Result<()> {
//!     // Initialize the library
//!     wadjet::init()?;
//!
//!     // List available devices
//!     for device in wadjet::list_devices()? {
//!         println!("Device: {} - {}", device.name, device.description);
//!     }
//!
//!     // Create capture session with default options
//!     let options = CaptureOptions::default();
//!     let mut session = CaptureSession::open("eth0", &options)?;
//!
//!     // Capture packets
//!     while let Some(packet) = session.next_packet()? {
//!         println!("Captured {} bytes", packet.len());
//!         
//!         // Decode the packet
//!         if let Some(decode_result) = packet.decode() {
//!             for layer in decode_result.layers() {
//!                 println!("  Layer: {:?}", layer.protocol());
//!             }
//!         }
//!     }
//!
//!     // Cleanup
//!     wadjet::cleanup();
//!     Ok(())
//! }
//! ```
//!
//! ## PCAP File Operations
//!
//! ```rust,ignore
//! use wadjet::{PcapReader, PcapWriter, Result};
//!
//! fn convert_pcap(input: &str, output: &str) -> Result<()> {
//!     let mut reader = PcapReader::open(input)?;
//!     let mut writer = PcapWriter::create(output)?;
//!
//!     while let Some(packet) = reader.next_packet()? {
//!         // Process packet...
//!         writer.write_packet(&packet)?;
//!     }
//!
//!     Ok(())
//! }
//! ```

mod error;
mod capture;
mod packet;
mod pcap;
mod decode;
mod device;
mod types;
mod validation;
pub mod diagnostic;

pub use error::{Error, Result};
pub use capture::{CaptureSession, CaptureOptions, CaptureStatistics};
pub use packet::{Packet, PacketView};
pub use pcap::{PcapReader, PcapWriter};
pub use decode::{DecodeResult, Protocol};
pub use device::DeviceInfo;
pub use types::*;
pub use validation::{ProtocolValidator, ProtocolLayer, ValidationResult, ValidationMode};
pub use diagnostic::{
    DiagnosticEvent, DiagnosticOptions, DiagnosticSessionManager,
    DiagnosticSessionState, DiagnosticTiming, CorrelationStatistics, SessionType,
};

use std::sync::atomic::{AtomicBool, Ordering};

static INITIALIZED: AtomicBool = AtomicBool::new(false);

/// Initialize the Wadjet library.
///
/// This must be called before using any other Wadjet functions.
/// It is safe to call multiple times; subsequent calls are no-ops.
///
/// # Example
///
/// ```rust,ignore
/// wadjet::init().expect("Failed to initialize Wadjet");
/// ```
pub fn init() -> Result<()> {
    if INITIALIZED.swap(true, Ordering::SeqCst) {
        return Ok(()); // Already initialized
    }

    let err = unsafe { wadjet_sys::wadjet_init() };
    error::check_error(err)?;
    Ok(())
}

/// Cleanup the Wadjet library.
///
/// This should be called when you're done using the library.
/// After calling this, you must call `init()` again before using other functions.
pub fn cleanup() {
    if INITIALIZED.swap(false, Ordering::SeqCst) {
        unsafe { wadjet_sys::wadjet_cleanup() };
    }
}

/// Get the Wadjet library version string.
///
/// # Example
///
/// ```rust,ignore
/// let version = wadjet::version();
/// println!("Wadjet version: {}", version);
/// ```
pub fn version() -> String {
    unsafe {
        let ptr = wadjet_sys::wadjet_version();
        if ptr.is_null() {
            return String::from("unknown");
        }
        std::ffi::CStr::from_ptr(ptr)
            .to_string_lossy()
            .into_owned()
    }
}

/// List all available network capture devices.
///
/// # Example
///
/// ```rust,ignore
/// for device in wadjet::list_devices()? {
///     println!("Name: {}", device.name);
///     println!("  Description: {}", device.description);
///     println!("  Loopback: {}", device.is_loopback);
///     println!("  Up: {}", device.is_up);
/// }
/// ```
pub fn list_devices() -> Result<Vec<DeviceInfo>> {
    device::list_devices()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version() {
        let v = version();
        assert!(!v.is_empty());
        println!("Version: {}", v);
    }

    #[test]
    fn test_init_cleanup() {
        init().unwrap();
        cleanup();
    }

    #[test]
    fn test_multiple_init() {
        init().unwrap();
        init().unwrap(); // Should be no-op
        cleanup();
    }
}

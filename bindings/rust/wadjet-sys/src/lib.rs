//! # wadjet-sys
//!
//! Low-level FFI bindings for the Wadjet-Link C library.
//!
//! This crate provides raw, unsafe bindings to `libwadjet_c`. For a safe,
//! idiomatic Rust API, use the `wadjet` crate instead.
//!
//! ## Usage
//!
//! ```rust,ignore
//! use wadjet_sys::*;
//!
//! unsafe {
//!     // Initialize the library
//!     let err = wadjet_init();
//!     assert_eq!(err, wadjet_error_t::WADJET_OK);
//!
//!     // Get version
//!     let version = wadjet_version();
//!     println!("Version: {:?}", std::ffi::CStr::from_ptr(version));
//!
//!     // Cleanup
//!     wadjet_cleanup();
//! }
//! ```
//!
//! ## Safety
//!
//! All functions in this crate are unsafe and require careful handling of:
//! - Null pointers
//! - Memory ownership (handles must be destroyed)
//! - Thread safety (most operations are not thread-safe)
//!
//! ## Building
//!
//! This crate requires:
//! - `libwadjet_c.so` to be built and accessible
//! - Clang for bindgen (unless using pre-generated bindings)
//!
//! Set `WADJET_C_LIB_DIR` to the directory containing the library,
//! or install it system-wide.

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![allow(clippy::all)]

// Include bindgen-generated bindings
#[cfg(not(feature = "use-pregenerated"))]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

// Use pre-generated bindings if feature is enabled
#[cfg(feature = "use-pregenerated")]
include!("bindings_pregenerated.rs");

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CStr;

    #[test]
    fn test_version() {
        unsafe {
            let version = wadjet_version();
            assert!(!version.is_null());
            let version_str = CStr::from_ptr(version).to_str().unwrap();
            assert!(!version_str.is_empty());
            println!("Wadjet version: {}", version_str);
        }
    }

    #[test]
    fn test_error_message() {
        unsafe {
            let msg = wadjet_error_message(wadjet_error_t::WADJET_OK);
            assert!(!msg.is_null());
            let msg_str = CStr::from_ptr(msg).to_str().unwrap();
            assert_eq!(msg_str, "Success");
        }
    }

    #[test]
    fn test_init_cleanup() {
        unsafe {
            let err = wadjet_init();
            assert_eq!(err, wadjet_error_t::WADJET_OK);
            wadjet_cleanup();
        }
    }

    #[test]
    fn test_capture_options_default() {
        unsafe {
            let mut opts = wadjet_capture_options_t::default();
            wadjet_capture_options_default(&mut opts);
            assert_eq!(opts.snaplen, 65535);
            assert!(opts.promiscuous);
            assert!(opts.immediate_mode);
        }
    }

    #[test]
    fn test_mac_address_conversion() {
        unsafe {
            let mac = wadjet_mac_address_t {
                bytes: [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF],
            };
            
            let mut buffer = [0u8; 18];
            let len = wadjet_mac_to_string(
                &mac,
                buffer.as_mut_ptr() as *mut i8,
                buffer.len(),
            );
            
            assert!(len > 0);
            let mac_str = std::str::from_utf8(&buffer[..len]).unwrap();
            assert_eq!(mac_str.to_lowercase(), "aa:bb:cc:dd:ee:ff");
        }
    }

    #[test]
    fn test_ipv4_address_conversion() {
        unsafe {
            let ip = wadjet_ipv4_address_t {
                bytes: [192, 168, 1, 100],
            };
            
            let mut buffer = [0u8; 16];
            let len = wadjet_ipv4_to_string(
                &ip,
                buffer.as_mut_ptr() as *mut i8,
                buffer.len(),
            );
            
            assert!(len > 0);
            let ip_str = std::str::from_utf8(&buffer[..len]).unwrap();
            assert_eq!(ip_str, "192.168.1.100");
        }
    }

    #[test]
    fn test_timestamp() {
        unsafe {
            let mut ts = wadjet_timestamp_t::default();
            wadjet_timestamp_now(&mut ts);
            assert!(ts.seconds > 0);
        }
    }
}

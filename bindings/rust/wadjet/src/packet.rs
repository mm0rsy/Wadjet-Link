//! Packet types and operations.

use crate::decode::DecodeResult;
use crate::error::{check_error, Result};
use crate::types::Timestamp;
use std::ptr;
use std::slice;

/// A captured network packet.
///
/// This type owns the packet data and will free it when dropped.
/// For borrowed access to packet data, use `PacketView`.
pub struct Packet {
    handle: wadjet_sys::wadjet_packet_t,
}

impl Packet {
    /// Create a packet from an owned handle
    pub(crate) fn from_handle(handle: wadjet_sys::wadjet_packet_t) -> Self {
        Self { handle }
    }

    /// Get the raw packet data.
    pub fn data(&self) -> &[u8] {
        let mut data: *const u8 = ptr::null();
        let mut len: usize = 0;
        
        let err = unsafe {
            wadjet_sys::wadjet_packet_data(self.handle, &mut data, &mut len)
        };
        
        if err != wadjet_sys::wadjet_error_t::WADJET_OK || data.is_null() || len == 0 {
            return &[];
        }
        
        unsafe { slice::from_raw_parts(data, len) }
    }

    /// Get the length of the packet data in bytes.
    pub fn len(&self) -> usize {
        self.data().len()
    }

    /// Check if the packet is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get the capture timestamp.
    pub fn timestamp(&self) -> Timestamp {
        let mut ts: wadjet_sys::wadjet_timestamp_t = unsafe { std::mem::zeroed() };
        
        let _ = unsafe {
            wadjet_sys::wadjet_packet_timestamp(self.handle, &mut ts)
        };
        
        Timestamp::from_c(&ts)
    }

    /// Clone the packet data into a new owned buffer.
    pub fn clone_data(&self) -> Vec<u8> {
        self.data().to_vec()
    }

    /// Get the underlying handle (for FFI use).
    pub(crate) fn handle(&self) -> wadjet_sys::wadjet_packet_t {
        self.handle
    }

    /// Decode the packet.
    ///
    /// This analyzes the packet and returns information about each protocol layer.
    pub fn decode(&self) -> Option<DecodeResult> {
        DecodeResult::decode_packet(self)
    }

    /// Create a view of this packet.
    pub fn view(&self) -> PacketView<'_> {
        PacketView {
            data: self.data(),
            timestamp: self.timestamp(),
        }
    }
}

impl Drop for Packet {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_packet_destroy(self.handle);
            }
        }
    }
}

impl std::fmt::Debug for Packet {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Packet")
            .field("len", &self.len())
            .field("timestamp", &self.timestamp())
            .finish()
    }
}

// Packet owns its handle and can be sent between threads
unsafe impl Send for Packet {}

/// A borrowed view of packet data.
///
/// This is a lightweight reference to packet data that doesn't own the memory.
/// It's useful for efficient passing of packet information without copying.
#[derive(Debug, Clone)]
pub struct PacketView<'a> {
    /// The raw packet data
    pub data: &'a [u8],
    /// Capture timestamp
    pub timestamp: Timestamp,
}

impl<'a> PacketView<'a> {
    /// Create a new packet view.
    pub fn new(data: &'a [u8], timestamp: Timestamp) -> Self {
        Self { data, timestamp }
    }

    /// Get the length of the packet data.
    pub fn len(&self) -> usize {
        self.data.len()
    }

    /// Check if the packet is empty.
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }
}

/// Create a packet from raw data.
///
/// This is useful for creating packets from arbitrary data for testing
/// or injection purposes.
pub fn packet_from_data(data: &[u8]) -> Result<Packet> {
    let mut handle: wadjet_sys::wadjet_packet_t = ptr::null_mut();
    
    let err = unsafe {
        wadjet_sys::wadjet_packet_create(data.as_ptr(), data.len(), &mut handle)
    };
    
    check_error(err)?;
    
    Ok(Packet::from_handle(handle))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_packet_view() {
        let data = [0x00, 0x01, 0x02, 0x03];
        let ts = Timestamp::new(1234567890, 123456);
        let view = PacketView::new(&data, ts);

        assert_eq!(view.len(), 4);
        assert!(!view.is_empty());
        assert_eq!(view.data, &data);
    }
}

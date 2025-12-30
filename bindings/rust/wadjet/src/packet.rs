//! Packet types and operations.

use crate::decode::DecodeResult;
use crate::types::Timestamp;
use std::slice;

/// A captured network packet.
///
/// This type owns the packet data and will free it when dropped.
/// For borrowed access to packet data, use `PacketView`.
pub struct Packet {
    handle: *mut wadjet_sys::wadjet_packet_t,
}

impl Packet {
    /// Create a packet from an owned handle
    pub(crate) fn from_handle(handle: *mut wadjet_sys::wadjet_packet_t) -> Self {
        Self { handle }
    }

    /// Get the raw packet data.
    pub fn data(&self) -> &[u8] {
        unsafe {
            let data = wadjet_sys::wadjet_packet_data(self.handle);
            let len = wadjet_sys::wadjet_packet_length(self.handle);
            if data.is_null() || len == 0 {
                return &[];
            }
            slice::from_raw_parts(data, len)
        }
    }

    /// Get the length of the packet data in bytes.
    pub fn len(&self) -> usize {
        unsafe { wadjet_sys::wadjet_packet_length(self.handle) }
    }

    /// Check if the packet is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get the capture length (how many bytes were actually captured).
    pub fn capture_len(&self) -> usize {
        unsafe { wadjet_sys::wadjet_packet_capture_length(self.handle) }
    }

    /// Get the original length on the wire.
    pub fn original_len(&self) -> usize {
        unsafe { wadjet_sys::wadjet_packet_original_length(self.handle) }
    }

    /// Get the capture timestamp.
    pub fn timestamp(&self) -> Timestamp {
        unsafe {
            let ts = wadjet_sys::wadjet_packet_timestamp(self.handle);
            Timestamp::from_c(&ts)
        }
    }

    /// Clone the packet data into a new owned buffer.
    pub fn clone_data(&self) -> Vec<u8> {
        let mut buffer = vec![0u8; self.len()];
        
        unsafe {
            wadjet_sys::wadjet_packet_copy_data(
                self.handle,
                buffer.as_mut_ptr(),
                buffer.len(),
            );
        }
        
        buffer
    }

    /// Get the underlying handle (for FFI use).
    pub(crate) fn handle(&self) -> *mut wadjet_sys::wadjet_packet_t {
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
            capture_len: self.capture_len(),
            original_len: self.original_len(),
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
            .field("capture_len", &self.capture_len())
            .field("original_len", &self.original_len())
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
    /// Captured length
    pub capture_len: usize,
    /// Original length on the wire
    pub original_len: usize,
}

impl<'a> PacketView<'a> {
    /// Create a new packet view.
    pub fn new(
        data: &'a [u8],
        timestamp: Timestamp,
        capture_len: usize,
        original_len: usize,
    ) -> Self {
        Self {
            data,
            timestamp,
            capture_len,
            original_len,
        }
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
pub fn packet_from_data(data: &[u8], timestamp: Timestamp) -> Packet {
    let ts = timestamp.to_c();
    let handle = unsafe {
        wadjet_sys::wadjet_packet_create(data.as_ptr(), data.len(), &ts)
    };
    Packet::from_handle(handle)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_packet_view() {
        let data = [0x00, 0x01, 0x02, 0x03];
        let ts = Timestamp::new(1234567890, 123456);
        let view = PacketView::new(&data, ts, 4, 4);

        assert_eq!(view.len(), 4);
        assert!(!view.is_empty());
        assert_eq!(view.data, &data);
    }
}

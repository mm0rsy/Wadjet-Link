//! PCAP file reading and writing.

use crate::error::{check_error, Result};
use crate::packet::Packet;
use crate::types::Timestamp;
use std::ffi::CString;
use std::path::Path;
use std::ptr;

/// PCAP file reader.
///
/// # Example
///
/// ```rust,ignore
/// use wadjet::PcapReader;
///
/// let mut reader = PcapReader::open("capture.pcap")?;
///
/// while let Some(packet) = reader.next_packet()? {
///     println!("Read packet: {} bytes", packet.len());
/// }
/// ```
pub struct PcapReader {
    handle: wadjet_sys::wadjet_pcap_reader_t,
}

impl PcapReader {
    /// Open a PCAP file for reading.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to the PCAP file
    pub fn open<P: AsRef<Path>>(path: P) -> Result<Self> {
        let path_str = path.as_ref().to_string_lossy();
        let path_c = CString::new(path_str.as_ref()).map_err(|_| {
            crate::Error::InvalidParameter("Path contains null byte".into())
        })?;

        let mut handle: wadjet_sys::wadjet_pcap_reader_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_pcap_reader_open(path_c.as_ptr(), &mut handle)
        };

        check_error(err)?;

        if handle.is_null() {
            return Err(crate::Error::OperationFailed(
                "Failed to open PCAP file".into()
            ));
        }

        Ok(Self { handle })
    }

    /// Read the next packet from the file.
    ///
    /// Returns `Ok(Some(packet))` if a packet was read,
    /// `Ok(None)` if end of file was reached,
    /// or an error if something went wrong.
    pub fn next_packet(&mut self) -> Result<Option<Packet>> {
        let mut packet_handle: wadjet_sys::wadjet_packet_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_pcap_reader_next(self.handle, &mut packet_handle)
        };

        // End of file is not an error - WADJET_ERR_NOT_FOUND indicates EOF
        if err == wadjet_sys::wadjet_error_t::WADJET_ERR_NOT_FOUND {
            return Ok(None);
        }

        check_error(err)?;

        if packet_handle.is_null() {
            return Ok(None);
        }

        Ok(Some(Packet::from_handle(packet_handle)))
    }

    /// Check if more packets are available.
    pub fn has_more(&self) -> bool {
        unsafe { wadjet_sys::wadjet_pcap_reader_has_more(self.handle) }
    }

    /// Reset reader to beginning of file.
    pub fn reset(&mut self) -> Result<()> {
        let err = unsafe { wadjet_sys::wadjet_pcap_reader_reset(self.handle) };
        check_error(err)
    }
}

impl Drop for PcapReader {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_pcap_reader_destroy(self.handle);
            }
        }
    }
}

impl Iterator for PcapReader {
    type Item = Result<Packet>;

    fn next(&mut self) -> Option<Self::Item> {
        match self.next_packet() {
            Ok(Some(packet)) => Some(Ok(packet)),
            Ok(None) => None,
            Err(e) => Some(Err(e)),
        }
    }
}

// PcapReader can be sent between threads
unsafe impl Send for PcapReader {}

/// PCAP file writer.
///
/// # Example
///
/// ```rust,ignore
/// use wadjet::{PcapWriter, Timestamp};
///
/// let mut writer = PcapWriter::create("output.pcap")?;
///
/// // Write raw packet data
/// let data = [0x00, 0x01, 0x02, 0x03];
/// let timestamp = Timestamp::now();
/// writer.write_raw(&data, timestamp)?;
/// ```
pub struct PcapWriter {
    handle: wadjet_sys::wadjet_pcap_writer_t,
}

impl PcapWriter {
    /// Create a new PCAP file for writing.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to the PCAP file to create
    pub fn create<P: AsRef<Path>>(path: P) -> Result<Self> {
        let path_str = path.as_ref().to_string_lossy();
        let path_c = CString::new(path_str.as_ref()).map_err(|_| {
            crate::Error::InvalidParameter("Path contains null byte".into())
        })?;

        let mut handle: wadjet_sys::wadjet_pcap_writer_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_pcap_writer_create(path_c.as_ptr(), &mut handle)
        };

        check_error(err)?;

        if handle.is_null() {
            return Err(crate::Error::OperationFailed(
                "Failed to create PCAP file".into()
            ));
        }

        Ok(Self { handle })
    }

    /// Write a packet to the file.
    pub fn write_packet(&mut self, packet: &Packet) -> Result<()> {
        let err = unsafe {
            wadjet_sys::wadjet_pcap_writer_write(self.handle, packet.handle())
        };
        check_error(err)
    }

    /// Write raw packet data to the file.
    ///
    /// # Arguments
    ///
    /// * `data` - The raw packet data
    /// * `timestamp` - The timestamp for the packet
    pub fn write_raw(&mut self, data: &[u8], timestamp: Timestamp) -> Result<()> {
        let ts = timestamp.to_c();
        
        let err = unsafe {
            wadjet_sys::wadjet_pcap_writer_write_raw(
                self.handle,
                data.as_ptr(),
                data.len(),
                &ts,
            )
        };
        
        check_error(err)
    }

    /// Flush any buffered data to disk.
    pub fn flush(&mut self) -> Result<()> {
        let err = unsafe { wadjet_sys::wadjet_pcap_writer_flush(self.handle) };
        check_error(err)
    }
}

impl Drop for PcapWriter {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_pcap_writer_destroy(self.handle);
            }
        }
    }
}

// PcapWriter can be sent between threads
unsafe impl Send for PcapWriter {}

#[cfg(test)]
mod tests {
    // Tests would require actual files or mocking
}

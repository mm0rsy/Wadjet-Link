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
/// println!("Link type: {}", reader.link_type());
///
/// while let Some(packet) = reader.next_packet()? {
///     println!("Read packet: {} bytes", packet.len());
/// }
/// ```
pub struct PcapReader {
    handle: *mut wadjet_sys::wadjet_pcap_reader_t,
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

        let mut handle: *mut wadjet_sys::wadjet_pcap_reader_t = ptr::null_mut();
        
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
        let mut packet_handle: *mut wadjet_sys::wadjet_packet_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_pcap_reader_next_packet(self.handle, &mut packet_handle)
        };

        // End of file is not an error
        if err == wadjet_sys::wadjet_error_t::WADJET_ERROR_END_OF_FILE {
            return Ok(None);
        }

        check_error(err)?;

        if packet_handle.is_null() {
            return Ok(None);
        }

        Ok(Some(Packet::from_handle(packet_handle)))
    }

    /// Get the data link type of the PCAP file.
    pub fn link_type(&self) -> i32 {
        unsafe { wadjet_sys::wadjet_pcap_reader_link_type(self.handle) }
    }

    /// Get the snap length of the PCAP file.
    pub fn snaplen(&self) -> u32 {
        unsafe { wadjet_sys::wadjet_pcap_reader_snaplen(self.handle) }
    }
}

impl Drop for PcapReader {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_pcap_reader_close(self.handle);
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
///
/// // Or write a Packet object
/// // writer.write_packet(&packet)?;
/// ```
pub struct PcapWriter {
    handle: *mut wadjet_sys::wadjet_pcap_writer_t,
}

impl PcapWriter {
    /// Create a new PCAP file for writing.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to the PCAP file to create
    pub fn create<P: AsRef<Path>>(path: P) -> Result<Self> {
        Self::create_with_link_type(path, 1) // DLT_EN10MB (Ethernet)
    }

    /// Create a new PCAP file with a specific link type.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to the PCAP file to create
    /// * `link_type` - The data link type (e.g., 1 for Ethernet)
    pub fn create_with_link_type<P: AsRef<Path>>(path: P, link_type: i32) -> Result<Self> {
        Self::create_with_options(path, link_type, 65535)
    }

    /// Create a new PCAP file with full options.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to the PCAP file to create
    /// * `link_type` - The data link type
    /// * `snaplen` - Maximum bytes per packet
    pub fn create_with_options<P: AsRef<Path>>(
        path: P,
        link_type: i32,
        snaplen: u32,
    ) -> Result<Self> {
        let path_str = path.as_ref().to_string_lossy();
        let path_c = CString::new(path_str.as_ref()).map_err(|_| {
            crate::Error::InvalidParameter("Path contains null byte".into())
        })?;

        let mut handle: *mut wadjet_sys::wadjet_pcap_writer_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_pcap_writer_create(
                path_c.as_ptr(),
                link_type,
                snaplen,
                &mut handle,
            )
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
            wadjet_sys::wadjet_pcap_writer_write_packet(self.handle, packet.handle())
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
                wadjet_sys::wadjet_pcap_writer_close(self.handle);
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

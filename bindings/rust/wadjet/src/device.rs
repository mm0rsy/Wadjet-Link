//! Network device enumeration.

use crate::error::{check_error, Result};
use std::ffi::CStr;
use std::ptr;

/// Information about a network device.
#[derive(Debug, Clone)]
pub struct DeviceInfo {
    /// Device name (e.g., "eth0", "en0")
    pub name: String,
    /// Human-readable description
    pub description: String,
    /// Whether this is a loopback device
    pub is_loopback: bool,
    /// Whether the device is up
    pub is_up: bool,
    /// Whether the device is running
    pub is_running: bool,
    /// Whether this is a wireless device
    pub is_wireless: bool,
}

impl DeviceInfo {
    /// Create from C struct
    fn from_c(c: &wadjet_sys::wadjet_device_info_t) -> Self {
        let name = if c.name.is_null() {
            String::new()
        } else {
            unsafe { CStr::from_ptr(c.name).to_string_lossy().into_owned() }
        };

        let description = if c.description.is_null() {
            String::new()
        } else {
            unsafe { CStr::from_ptr(c.description).to_string_lossy().into_owned() }
        };

        Self {
            name,
            description,
            is_loopback: c.is_loopback,
            is_up: c.is_up,
            is_running: c.is_running,
            is_wireless: c.is_wireless,
        }
    }
}

/// List all available network capture devices.
pub(crate) fn list_devices() -> Result<Vec<DeviceInfo>> {
    let mut devices: *mut wadjet_sys::wadjet_device_info_t = ptr::null_mut();
    let mut count: usize = 0;

    let err = unsafe {
        wadjet_sys::wadjet_device_list(&mut devices, &mut count)
    };

    check_error(err)?;

    if devices.is_null() || count == 0 {
        return Ok(Vec::new());
    }

    // Convert C array to Vec
    let result: Vec<DeviceInfo> = unsafe {
        let slice = std::slice::from_raw_parts(devices, count);
        slice.iter().map(DeviceInfo::from_c).collect()
    };

    // Free the device list
    unsafe {
        wadjet_sys::wadjet_device_list_free(devices, count);
    }

    Ok(result)
}

/// Find the default capture device.
///
/// This returns the first device that is up, running, and not a loopback device.
pub fn default_device() -> Result<Option<DeviceInfo>> {
    let devices = list_devices()?;
    
    // Find first suitable device
    let device = devices.into_iter().find(|d| {
        d.is_up && d.is_running && !d.is_loopback
    });

    Ok(device)
}

/// Find a device by name.
pub fn find_device(name: &str) -> Result<Option<DeviceInfo>> {
    let devices = list_devices()?;
    Ok(devices.into_iter().find(|d| d.name == name))
}

#[cfg(test)]
mod tests {
    // Device tests require actual system access
}

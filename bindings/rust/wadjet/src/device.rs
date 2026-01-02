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
        }
    }
}

/// List all available network capture devices.
pub(crate) fn list_devices() -> Result<Vec<DeviceInfo>> {
    let mut list: wadjet_sys::wadjet_device_list_t = ptr::null_mut();

    let err = unsafe {
        wadjet_sys::wadjet_device_enumerate(&mut list)
    };

    check_error(err)?;

    if list.is_null() {
        return Ok(Vec::new());
    }

    let count = unsafe { wadjet_sys::wadjet_device_list_count(list) };
    
    let mut result = Vec::with_capacity(count);
    
    for i in 0..count {
        let mut info: wadjet_sys::wadjet_device_info_t = unsafe { std::mem::zeroed() };
        let err = unsafe {
            wadjet_sys::wadjet_device_list_get(list, i, &mut info)
        };
        
        if err == wadjet_sys::wadjet_error_t::WADJET_OK {
            result.push(DeviceInfo::from_c(&info));
        }
    }

    // Free the device list
    unsafe {
        wadjet_sys::wadjet_device_list_destroy(list);
    }

    Ok(result)
}

/// Find the default capture device.
///
/// This returns the first device that is up and not a loopback device.
pub fn default_device() -> Result<Option<DeviceInfo>> {
    let devices = list_devices()?;
    
    // Find first suitable device
    let device = devices.into_iter().find(|d| {
        d.is_up && !d.is_loopback
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

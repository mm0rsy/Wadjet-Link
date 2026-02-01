//! Cross-protocol validation functionality.

use crate::error::check_error;
use crate::Result;
use std::ptr;
use wadjet_sys;

/// Validation error handling mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ValidationMode {
    /// Fail on first validation error
    Strict,
    /// Log warnings but continue validation
    Lenient,
}

impl ValidationMode {
    fn to_c(&self) -> wadjet_sys::wadjet_validation_mode_t {
        match self {
            ValidationMode::Strict => wadjet_sys::wadjet_validation_mode_t::WADJET_VALIDATION_MODE_STRICT,
            ValidationMode::Lenient => wadjet_sys::wadjet_validation_mode_t::WADJET_VALIDATION_MODE_LENIENT,
        }
    }

    fn from_c(mode: wadjet_sys::wadjet_validation_mode_t) -> Self {
        match mode {
            wadjet_sys::wadjet_validation_mode_t::WADJET_VALIDATION_MODE_LENIENT => ValidationMode::Lenient,
            _ => ValidationMode::Strict,
        }
    }
}

/// Protocol layer information for validation
#[derive(Debug, Clone)]
pub struct ProtocolLayer {
    /// Layer name (e.g., "IPv4", "TCP")
    pub name: String,
    /// Offset in packet where layer starts
    pub offset: usize,
    /// Length of this layer's header
    pub header_length: usize,
    /// Length of payload carried by this layer
    pub payload_length: usize,
    /// EtherType or protocol number
    pub ethertype: u16,
    /// Checksum value (0 if none)
    pub checksum: u16,
    /// Whether this layer has a checksum field
    pub has_checksum: bool,
}

impl ProtocolLayer {
    /// Create a new protocol layer
    pub fn new(name: impl Into<String>, offset: usize, header_length: usize, payload_length: usize) -> Self {
        Self {
            name: name.into(),
            offset,
            header_length,
            payload_length,
            ethertype: 0,
            checksum: 0,
            has_checksum: false,
        }
    }

    /// Set the EtherType/protocol number
    pub fn with_ethertype(mut self, ethertype: u16) -> Self {
        self.ethertype = ethertype;
        self
    }

    /// Set the checksum information
    pub fn with_checksum(mut self, checksum: u16, has_checksum: bool) -> Self {
        self.checksum = checksum;
        self.has_checksum = has_checksum;
        self
    }

    fn to_c(&self) -> wadjet_sys::wadjet_protocol_layer_t {
        let name_cstr = std::ffi::CString::new(self.name.as_bytes()).unwrap_or_default();
        wadjet_sys::wadjet_protocol_layer_t {
            name: name_cstr.into_raw(),
            offset: self.offset,
            header_length: self.header_length,
            payload_length: self.payload_length,
            ethertype: self.ethertype,
            checksum: self.checksum,
            has_checksum: self.has_checksum,
        }
    }
}

/// Validation result containing all errors found
#[derive(Debug, Clone)]
pub struct ValidationResult {
    /// True if validation passed
    pub is_valid: bool,
    /// Mode used for validation
    pub mode: ValidationMode,
    /// Number of errors found
    pub error_count: usize,
}

impl ValidationResult {
    /// Check if validation passed
    pub fn passed(&self) -> bool {
        self.is_valid
    }

    /// Get the number of errors found
    pub fn errors(&self) -> usize {
        self.error_count
    }

    fn from_c(c: &wadjet_sys::wadjet_validation_result_t) -> Self {
        Self {
            is_valid: c.is_valid,
            mode: ValidationMode::from_c(c.mode),
            error_count: c.error_count,
        }
    }
}

/// Cross-protocol validator for packet analysis
///
/// Validates:
/// - Protocol stack layering (consistent protocol progression)
/// - Length consistency across layers (header + payload = total)
/// - Checksums (IPv4, UDP, TCP with pseudo-header support)
pub struct ProtocolValidator {
    validator: *mut wadjet_sys::wadjet_protocol_validator,
}

impl ProtocolValidator {
    /// Create a new protocol validator
    ///
    /// # Arguments
    /// * `mode` - Validation mode (strict or lenient)
    pub fn new(mode: ValidationMode) -> Result<Self> {
        let mut validator = ptr::null_mut();
        let err = unsafe {
            wadjet_sys::wadjet_protocol_validator_create(
                mode.to_c(),
                &mut validator,
            )
        };
        check_error(err)?;
        Ok(Self { validator })
    }

    /// Validate protocol stack layering integrity
    ///
    /// Checks for valid protocol progression (Ethernet → IPv4 → UDP/TCP → Application)
    /// and proper frame type matching.
    ///
    /// # Arguments
    /// * `layers` - Vector of protocol layers in order
    pub fn validate_layering(&self, layers: &[ProtocolLayer]) -> Result<ValidationResult> {
        let c_layers: Vec<_> = layers.iter().map(|l| l.to_c()).collect();
        let mut result = std::mem::MaybeUninit::uninit();

        let err = unsafe {
            wadjet_sys::wadjet_validate_layering(
                self.validator,
                c_layers.as_ptr(),
                c_layers.len(),
                result.as_mut_ptr() as *mut wadjet_sys::wadjet_validation_result_t,
            )
        };

        // Free allocated strings
        for c_layer in c_layers {
            if !c_layer.name.is_null() {
                unsafe { std::ffi::CString::from_raw(c_layer.name as *mut _) };
            }
        }

        check_error(err)?;

        Ok(ValidationResult::from_c(unsafe { result.assume_init_ref() }))
    }

    /// Validate length consistency across layers
    ///
    /// Checks that header + payload = total length for each layer,
    /// no gaps or overlaps between layers.
    ///
    /// # Arguments
    /// * `layers` - Vector of protocol layers in order
    /// * `total_packet_length` - Total packet length in bytes
    pub fn validate_lengths(
        &self,
        layers: &[ProtocolLayer],
        total_packet_length: usize,
    ) -> Result<ValidationResult> {
        let c_layers: Vec<_> = layers.iter().map(|l| l.to_c()).collect();
        let mut result = std::mem::MaybeUninit::uninit();

        let err = unsafe {
            wadjet_sys::wadjet_validate_lengths(
                self.validator,
                c_layers.as_ptr(),
                c_layers.len(),
                total_packet_length,
                result.as_mut_ptr() as *mut wadjet_sys::wadjet_validation_result_t,
            )
        };

        // Free allocated strings
        for c_layer in c_layers {
            if !c_layer.name.is_null() {
                unsafe { std::ffi::CString::from_raw(c_layer.name as *mut _) };
            }
        }

        check_error(err)?;

        Ok(ValidationResult::from_c(unsafe { result.assume_init_ref() }))
    }

    /// Validate checksums across protocol layers
    ///
    /// Checks IPv4, UDP, TCP checksums with pseudo-header support.
    ///
    /// # Arguments
    /// * `packet_data` - Complete packet data
    /// * `layers` - Vector of protocol layers with checksum information
    pub fn validate_checksums(
        &self,
        packet_data: &[u8],
        layers: &[ProtocolLayer],
    ) -> Result<ValidationResult> {
        let c_layers: Vec<_> = layers.iter().map(|l| l.to_c()).collect();
        let mut result = std::mem::MaybeUninit::uninit();

        let err = unsafe {
            wadjet_sys::wadjet_validate_checksums(
                self.validator,
                packet_data.as_ptr(),
                packet_data.len(),
                c_layers.as_ptr(),
                c_layers.len(),
                result.as_mut_ptr() as *mut wadjet_sys::wadjet_validation_result_t,
            )
        };

        // Free allocated strings
        for c_layer in c_layers {
            if !c_layer.name.is_null() {
                unsafe { std::ffi::CString::from_raw(c_layer.name as *mut _) };
            }
        }

        check_error(err)?;

        Ok(ValidationResult::from_c(unsafe { result.assume_init_ref() }))
    }

    /// Set validation mode
    pub fn set_mode(&mut self, mode: ValidationMode) -> Result<()> {
        let err = unsafe {
            wadjet_sys::wadjet_protocol_validator_set_mode(self.validator, mode.to_c())
        };
        check_error(err)?;
        Ok(())
    }

    /// Get current validation mode
    pub fn get_mode(&self) -> Result<ValidationMode> {
        let mut mode = wadjet_sys::wadjet_validation_mode_t::WADJET_VALIDATION_MODE_STRICT;
        let err = unsafe {
            wadjet_sys::wadjet_protocol_validator_get_mode(
                self.validator,
                &mut mode,
            )
        };
        check_error(err)?;
        Ok(ValidationMode::from_c(mode))
    }
}

impl Drop for ProtocolValidator {
    fn drop(&mut self) {
        if !self.validator.is_null() {
            unsafe {
                wadjet_sys::wadjet_protocol_validator_destroy(self.validator);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validation_mode() {
        let strict = ValidationMode::Strict;
        assert_eq!(strict, ValidationMode::Strict);
        assert_ne!(strict, ValidationMode::Lenient);
    }

    #[test]
    fn test_protocol_layer_new() {
        let layer = ProtocolLayer::new("IPv4", 14, 20, 1480);
        assert_eq!(layer.name, "IPv4");
        assert_eq!(layer.offset, 14);
        assert_eq!(layer.header_length, 20);
        assert_eq!(layer.payload_length, 1480);
    }

    #[test]
    fn test_protocol_layer_with_ethertype() {
        let layer = ProtocolLayer::new("IPv4", 14, 20, 1480)
            .with_ethertype(0x0800);
        assert_eq!(layer.ethertype, 0x0800);
    }

    #[test]
    fn test_protocol_layer_with_checksum() {
        let layer = ProtocolLayer::new("IPv4", 14, 20, 1480)
            .with_checksum(0x1234, true);
        assert_eq!(layer.checksum, 0x1234);
        assert!(layer.has_checksum);
    }

    #[test]
    fn test_validator_creation() {
        let validator = ProtocolValidator::new(ValidationMode::Strict);
        assert!(validator.is_ok());
    }

    #[test]
    fn test_validator_mode_get_set() {
        if let Ok(mut validator) = ProtocolValidator::new(ValidationMode::Strict) {
            assert!(validator.get_mode().is_ok());
            let result = validator.set_mode(ValidationMode::Lenient);
            assert!(result.is_ok());
        }
    }

    #[test]
    fn test_validation_result_passed() {
        let result = ValidationResult {
            is_valid: true,
            mode: ValidationMode::Strict,
            error_count: 0,
        };
        assert!(result.passed());
        assert_eq!(result.errors(), 0);
    }
}

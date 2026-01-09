# Feature Specification: Rust FFI Bindings

**Feature Branch**: `milestone/007-rust-ffi`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M7 - Rust FFI (Optional Expansion)

## User Scenarios & Testing

### User Story 1 - Safe Rust API (Priority: P1)

As a Rust developer, I need safe, idiomatic Rust bindings for Wadjet-Link, so that I can use it in Rust projects without unsafe blocks.

**Acceptance Scenarios**:

1. **Given** wadjet crate installed, **When** I use `CaptureSession::new()`, **Then** I get safe Rust API with automatic cleanup
2. **Given** Rust bindings, **When** objects go out of scope, **Then** Drop trait automatically frees resources

### User Story 2 - C ABI Layer (Priority: P1)

As a Rust-FFI developer, I need a C99 ABI layer, so that Rust bindgen can generate FFI bindings from C headers.

**Acceptance Scenarios**:

1. **Given** wadjet_c.h, **When** bindgen processes it, **Then** valid Rust sys crate is generated
2. **Given** C ABI functions, **When** called from Rust, **Then** opaque handles work correctly without memory corruption

## Requirements

### Functional Requirements

- **FR-001**: C ABI layer (bindings/c/) with opaque handle types for all C++ classes
- **FR-002**: wadjet-sys crate with bindgen-generated bindings
- **FR-003**: wadjet crate with safe Rust wrappers implementing Drop trait
- **FR-004**: Builder pattern for CaptureOptions in Rust
- **FR-005**: Iterator support for PcapReader
- **FR-006**: Full protocol decoding support (all decoders)
- **FR-007**: Examples in Rust (list_devices, capture, read_pcap, someip_analysis)
- **FR-008**: Thread-safe Send markers where appropriate

## Success Criteria

- **SC-001**: All Rust examples compile and run successfully
- **SC-002**: No memory leaks in Rust bindings (verified with Valgrind)
- **SC-003**: Safe API has zero `unsafe` blocks in user-facing code

## Dependencies

- **External**: Rust 1.70+, bindgen
- **Internal**: C ABI layer built on top of all C++ APIs

## Out of Scope

- Pure Rust implementation (bindings only)
- Async Rust support (tokio/async-std)

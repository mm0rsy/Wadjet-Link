//! Build script for wadjet-sys
//!
//! This script uses bindgen to generate Rust FFI bindings from wadjet_c.h

use std::env;
use std::path::PathBuf;

fn main() {
    // Check if we should use pre-generated bindings
    if cfg!(feature = "use-pregenerated") {
        println!("cargo:warning=Using pre-generated bindings");
        return;
    }

    // Find the wadjet_c header
    let header_path = find_header();
    println!("cargo:rerun-if-changed={}", header_path.display());

    // Find and link the library
    link_library();

    // Generate bindings with bindgen
    let bindings = bindgen::Builder::default()
        .header(header_path.to_string_lossy())
        // Tell cargo to invalidate if header changes
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Use core types instead of std where possible
        .use_core()
        // Generate Debug trait implementations
        .derive_debug(true)
        // Generate Default trait implementations
        .derive_default(true)
        // Generate Copy and Clone traits
        .derive_copy(true)
        // Allowlist only wadjet_ prefixed items
        .allowlist_function("wadjet_.*")
        .allowlist_type("wadjet_.*")
        .allowlist_var("WADJET_.*")
        // Convert C enums to Rust enums
        .rustified_enum("wadjet_error_t")
        .rustified_enum("wadjet_protocol_t")
        .rustified_enum("wadjet_someip_message_type_t")
        .rustified_enum("wadjet_someip_return_code_t")
        .rustified_enum("wadjet_doip_payload_type_t")
        .rustified_enum("wadjet_gptp_message_type_t")
        // Generate documentation comments
        .generate_comments(true)
        // Block certain items that might cause issues
        .blocklist_type("__.*")
        .generate()
        .expect("Unable to generate bindings");

    // Write bindings to OUT_DIR
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

fn find_header() -> PathBuf {
    // Check environment variable first
    if let Ok(path) = env::var("WADJET_C_HEADER") {
        return PathBuf::from(path);
    }

    // Try relative paths from the crate
    let possible_paths = [
        // From bindings/rust/wadjet-sys/ to bindings/c/include/
        PathBuf::from("../../c/include/wadjet_c.h"),
        // Installed location
        PathBuf::from("/usr/local/include/wadjet_c.h"),
        PathBuf::from("/usr/include/wadjet_c.h"),
    ];

    for path in &possible_paths {
        if path.exists() {
            return path.clone();
        }
    }

    // Default to relative path (will fail with clear error if not found)
    PathBuf::from("../../c/include/wadjet_c.h")
}

fn link_library() {
    // Check environment variable for library path
    if let Ok(path) = env::var("WADJET_C_LIB_DIR") {
        println!("cargo:rustc-link-search=native={}", path);
    }

    // Try pkg-config first
    if pkg_config::probe_library("wadjet_c").is_ok() {
        return;
    }

    // Try common build directories
    let possible_lib_paths = [
        // Build directory (when building from source)
        PathBuf::from("../../../../build/bindings/c"),
        PathBuf::from("../../../../build/lib"),
        // Installed locations
        PathBuf::from("/usr/local/lib"),
        PathBuf::from("/usr/lib"),
    ];

    for path in &possible_lib_paths {
        if path.exists() {
            println!("cargo:rustc-link-search=native={}", path.display());
        }
    }

    // Link the library
    println!("cargo:rustc-link-lib=dylib=wadjet_c");
    
    // Also need to link wadjet itself (the C++ library)
    println!("cargo:rustc-link-lib=dylib=wadjet");
    
    // Link C++ standard library
    #[cfg(target_os = "linux")]
    println!("cargo:rustc-link-lib=dylib=stdc++");
    
    #[cfg(target_os = "macos")]
    println!("cargo:rustc-link-lib=dylib=c++");
}

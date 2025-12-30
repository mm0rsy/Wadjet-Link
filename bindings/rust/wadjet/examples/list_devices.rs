//! Example: List all available capture devices
//!
//! This example shows how to enumerate network interfaces available for capture.
//!
//! Run with: cargo run --example list_devices

fn main() -> wadjet::Result<()> {
    // Initialize the library
    wadjet::init()?;

    println!("Wadjet version: {}\n", wadjet::version());
    println!("Available capture devices:");
    println!("{:-<60}", "");

    for device in wadjet::list_devices()? {
        println!("Device: {}", device.name);
        if !device.description.is_empty() {
            println!("  Description: {}", device.description);
        }
        println!("  Status: {}", 
            if device.is_up { "UP" } else { "DOWN" }
        );
        println!("  Running: {}", 
            if device.is_running { "Yes" } else { "No" }
        );
        println!("  Loopback: {}", 
            if device.is_loopback { "Yes" } else { "No" }
        );
        println!("  Wireless: {}", 
            if device.is_wireless { "Yes" } else { "No" }
        );
        println!();
    }

    // Cleanup
    wadjet::cleanup();
    Ok(())
}

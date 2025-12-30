//! Example: Analyze SOME/IP traffic
//!
//! This example reads a PCAP file and analyzes SOME/IP traffic,
//! showing service/method statistics and message patterns.
//!
//! Run with: cargo run --example someip_analysis -- automotive.pcap

use std::collections::HashMap;
use std::env;
use wadjet::{PcapReader, Protocol};

#[derive(Default)]
struct ServiceStats {
    request_count: u64,
    response_count: u64,
    notification_count: u64,
    error_count: u64,
    methods: HashMap<u16, u64>,
}

fn main() -> wadjet::Result<()> {
    // Get filename from command line
    let args: Vec<String> = env::args().collect();
    let filename = args.get(1).expect("Usage: someip_analysis <filename.pcap>");

    // Initialize
    wadjet::init()?;

    println!("SOME/IP Analysis Example");
    println!("========================");
    println!("File: {}\n", filename);

    // Open the PCAP file
    let mut reader = PcapReader::open(filename)?;

    // Statistics
    let mut total_packets = 0u64;
    let mut someip_packets = 0u64;
    let mut services: HashMap<u16, ServiceStats> = HashMap::new();
    let mut client_sessions: HashMap<(u16, u16), u64> = HashMap::new(); // (client_id, session_id) -> count

    // Read all packets
    while let Some(packet) = reader.next_packet()? {
        total_packets += 1;

        if let Some(decode_result) = packet.decode() {
            if let Some(layer) = decode_result.find_layer(Protocol::SomeIp) {
                if let Some(someip) = layer.someip() {
                    someip_packets += 1;

                    // Update service statistics
                    let stats = services.entry(someip.service_id).or_default();
                    
                    // Count by message type
                    match someip.message_type {
                        0x00 => stats.request_count += 1,      // REQUEST
                        0x01 => stats.request_count += 1,      // REQUEST_NO_RETURN
                        0x02 => stats.notification_count += 1, // NOTIFICATION
                        0x80 => stats.response_count += 1,     // RESPONSE
                        0x81 => stats.error_count += 1,        // ERROR
                        _ => {}
                    }

                    // Count methods
                    *stats.methods.entry(someip.method_id).or_insert(0) += 1;

                    // Track client sessions
                    let session_key = (someip.client_id, someip.session_id);
                    *client_sessions.entry(session_key).or_insert(0) += 1;
                }
            }
        }

        if total_packets % 1000 == 0 {
            eprint!("\rProcessed {} packets...", total_packets);
        }
    }

    eprintln!("\r                                    \r");

    // Print results
    println!("Summary");
    println!("-------");
    println!("Total packets: {}", total_packets);
    println!("SOME/IP packets: {} ({:.1}%)", 
        someip_packets,
        if total_packets > 0 {
            (someip_packets as f64 / total_packets as f64) * 100.0
        } else {
            0.0
        }
    );
    println!("Unique services: {}", services.len());
    println!("Unique client sessions: {}", client_sessions.len());
    println!();

    // Print per-service statistics
    println!("Service Statistics");
    println!("------------------");
    
    let mut service_ids: Vec<_> = services.keys().collect();
    service_ids.sort();

    for &service_id in &service_ids {
        let stats = &services[&service_id];
        println!("\nService 0x{:04x}:", service_id);
        println!("  Requests: {}", stats.request_count);
        println!("  Responses: {}", stats.response_count);
        println!("  Notifications: {}", stats.notification_count);
        println!("  Errors: {}", stats.error_count);
        
        if !stats.methods.is_empty() {
            println!("  Methods:");
            let mut methods: Vec<_> = stats.methods.iter().collect();
            methods.sort_by(|a, b| b.1.cmp(a.1));
            for (method_id, count) in methods.iter().take(10) {
                println!("    0x{:04x}: {} calls", method_id, count);
            }
            if methods.len() > 10 {
                println!("    ... and {} more methods", methods.len() - 10);
            }
        }
    }

    // Print top clients
    println!("\nTop Client Sessions (by message count)");
    println!("--------------------------------------");
    let mut sessions: Vec<_> = client_sessions.iter().collect();
    sessions.sort_by(|a, b| b.1.cmp(a.1));
    for ((client_id, session_id), count) in sessions.iter().take(10) {
        println!("  Client {} / Session {}: {} messages", 
            client_id, session_id, count);
    }

    // Cleanup
    wadjet::cleanup();
    Ok(())
}

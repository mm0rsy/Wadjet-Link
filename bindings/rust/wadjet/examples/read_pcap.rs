//! Example: Read and analyze a PCAP file
//!
//! This example reads a PCAP file and prints information about each packet.
//!
//! Run with: cargo run --example read_pcap -- capture.pcap

use std::env;
use wadjet::{PcapReader, Protocol};

fn main() -> wadjet::Result<()> {
    // Get filename from command line
    let args: Vec<String> = env::args().collect();
    let filename = args.get(1).expect("Usage: read_pcap <filename.pcap>");

    // Initialize
    wadjet::init()?;

    println!("PCAP Reader Example");
    println!("===================");
    println!("File: {}\n", filename);

    // Open the PCAP file
    let mut reader = PcapReader::open(filename)?;
    
    println!("Link type: {}", reader.link_type());
    println!("Snap length: {}", reader.snaplen());
    println!();

    // Statistics
    let mut total_packets = 0u64;
    let mut total_bytes = 0u64;
    let mut protocol_counts: std::collections::HashMap<String, u64> = 
        std::collections::HashMap::new();

    // Read all packets
    while let Some(packet) = reader.next_packet()? {
        total_packets += 1;
        total_bytes += packet.len() as u64;

        // Decode the packet
        if let Some(decode_result) = packet.decode() {
            // Count protocols
            for layer in decode_result.layers() {
                let proto_name = format!("{:?}", layer.protocol());
                *protocol_counts.entry(proto_name).or_insert(0) += 1;
            }

            // Print first 5 packets in detail
            if total_packets <= 5 {
                println!("Packet #{}: {} bytes", total_packets, packet.len());
                println!("  Timestamp: {}", packet.timestamp());
                println!("  Summary: {}", decode_result.summary());
                
                // Print SOME/IP or DoIP details if present
                if decode_result.has_protocol(Protocol::SomeIp) {
                    if let Some(layer) = decode_result.find_layer(Protocol::SomeIp) {
                        if let Some(someip) = layer.someip() {
                            println!("  SOME/IP:");
                            println!("    Service ID: 0x{:04x}", someip.service_id);
                            println!("    Method ID: 0x{:04x}", someip.method_id);
                            println!("    Client ID: {}", someip.client_id);
                            println!("    Session ID: {}", someip.session_id);
                            println!("    Message Type: 0x{:02x}", someip.message_type);
                        }
                    }
                }

                if decode_result.has_protocol(Protocol::DoIp) {
                    if let Some(layer) = decode_result.find_layer(Protocol::DoIp) {
                        if let Some(doip) = layer.doip() {
                            println!("  DoIP:");
                            println!("    Version: 0x{:02x}", doip.protocol_version);
                            println!("    Payload Type: 0x{:04x} ({})", 
                                doip.payload_type, doip.payload_type_name());
                            println!("    Payload Length: {}", doip.payload_length);
                        }
                    }
                }
                println!();
            }
        }

        // Progress indicator
        if total_packets % 1000 == 0 {
            eprint!("\rProcessed {} packets...", total_packets);
        }
    }

    eprintln!("\r                                    \r");

    // Print summary
    println!("Summary");
    println!("=======");
    println!("Total packets: {}", total_packets);
    println!("Total bytes: {} ({:.2} MB)", 
        total_bytes, 
        total_bytes as f64 / (1024.0 * 1024.0)
    );
    println!();

    println!("Protocol Distribution:");
    let mut counts: Vec<_> = protocol_counts.iter().collect();
    counts.sort_by(|a, b| b.1.cmp(a.1));
    for (proto, count) in counts {
        let percentage = (*count as f64 / total_packets as f64) * 100.0;
        println!("  {}: {} ({:.1}%)", proto, count, percentage);
    }

    // Cleanup
    wadjet::cleanup();
    Ok(())
}

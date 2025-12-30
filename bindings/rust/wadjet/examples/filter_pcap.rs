//! Example: Filter and export packets to a new PCAP file
//!
//! This example reads a PCAP file, filters packets by protocol,
//! and writes matching packets to a new file.
//!
//! Run with: cargo run --example filter_pcap -- input.pcap output.pcap someip

use std::env;
use wadjet::{PcapReader, PcapWriter, Protocol};

fn parse_protocol(name: &str) -> Option<Protocol> {
    match name.to_lowercase().as_str() {
        "ethernet" | "eth" => Some(Protocol::Ethernet),
        "ipv4" | "ip" => Some(Protocol::Ipv4),
        "udp" => Some(Protocol::Udp),
        "tcp" => Some(Protocol::Tcp),
        "someip" | "some-ip" => Some(Protocol::SomeIp),
        "doip" | "do-ip" => Some(Protocol::DoIp),
        "arp" => Some(Protocol::Arp),
        "icmp" => Some(Protocol::Icmp),
        _ => None,
    }
}

fn main() -> wadjet::Result<()> {
    // Parse command line
    let args: Vec<String> = env::args().collect();
    if args.len() < 4 {
        eprintln!("Usage: filter_pcap <input.pcap> <output.pcap> <protocol>");
        eprintln!("Protocols: ethernet, ipv4, udp, tcp, someip, doip, arp, icmp");
        std::process::exit(1);
    }

    let input_file = &args[1];
    let output_file = &args[2];
    let filter_protocol = parse_protocol(&args[3]).expect("Unknown protocol");

    // Initialize
    wadjet::init()?;

    println!("PCAP Filter Example");
    println!("===================");
    println!("Input: {}", input_file);
    println!("Output: {}", output_file);
    println!("Filter: {:?}", filter_protocol);
    println!();

    // Open input file
    let mut reader = PcapReader::open(input_file)?;
    let link_type = reader.link_type();

    // Create output file with same link type
    let mut writer = PcapWriter::create_with_link_type(output_file, link_type)?;

    let mut total_packets = 0u64;
    let mut matched_packets = 0u64;

    // Process packets
    while let Some(packet) = reader.next_packet()? {
        total_packets += 1;

        // Decode and check for protocol
        if let Some(decode_result) = packet.decode() {
            if decode_result.has_protocol(filter_protocol) {
                // Write matching packet to output
                writer.write_packet(&packet)?;
                matched_packets += 1;
            }
        }

        // Progress
        if total_packets % 1000 == 0 {
            eprint!("\rProcessed {} packets, matched {}...", 
                total_packets, matched_packets);
        }
    }

    // Flush output
    writer.flush()?;

    eprintln!("\r                                              \r");

    // Summary
    println!("Complete!");
    println!("  Total packets: {}", total_packets);
    println!("  Matched packets: {} ({:.1}%)", 
        matched_packets,
        if total_packets > 0 {
            (matched_packets as f64 / total_packets as f64) * 100.0
        } else {
            0.0
        }
    );
    println!("  Output written to: {}", output_file);

    // Cleanup
    wadjet::cleanup();
    Ok(())
}

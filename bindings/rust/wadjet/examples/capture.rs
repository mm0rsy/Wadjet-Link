//! Example: Live packet capture with decoding
//!
//! This example captures packets from a network interface and decodes them.
//!
//! Run with (requires root/admin): sudo cargo run --example capture -- eth0

use std::env;
use wadjet::{CaptureSession, CaptureOptions, Protocol};

fn main() -> wadjet::Result<()> {
    // Get device from command line
    let args: Vec<String> = env::args().collect();
    let device = args.get(1).map(|s| s.as_str()).unwrap_or("eth0");

    // Initialize
    wadjet::init()?;

    println!("Wadjet Capture Example");
    println!("======================");
    println!("Version: {}", wadjet::version());
    println!("Device: {}", device);
    println!();

    // Create capture options
    let options = CaptureOptions::new()
        .snaplen(65535)
        .promiscuous(true)
        .timeout_ms(1000)
        .immediate_mode(true);

    // Open capture session
    println!("Opening capture session...");
    let mut session = CaptureSession::open(device, &options)?;

    println!("Link type: {}", session.link_type());
    println!("Capturing packets (Ctrl+C to stop)...\n");

    let mut packet_count = 0u64;

    // Capture loop
    loop {
        match session.next_packet()? {
            Some(packet) => {
                packet_count += 1;
                
                println!("Packet #{}: {} bytes @ {}", 
                    packet_count, 
                    packet.len(),
                    packet.timestamp()
                );

                // Decode the packet
                if let Some(decode_result) = packet.decode() {
                    println!("  Summary: {}", decode_result.summary());
                    
                    for layer in decode_result.layers() {
                        print!("  [{:?}] offset={}, len={}", 
                            layer.protocol(),
                            layer.offset(),
                            layer.length()
                        );

                        // Print protocol-specific info
                        match layer.protocol() {
                            Protocol::Ethernet => {
                                if let Some(eth) = layer.ethernet() {
                                    print!(" {} -> {}, type=0x{:04x}",
                                        eth.src_mac, eth.dst_mac, eth.ether_type);
                                }
                            }
                            Protocol::Ipv4 => {
                                if let Some(ip) = layer.ipv4() {
                                    print!(" {} -> {}, proto={}",
                                        ip.src_ip, ip.dst_ip, ip.protocol);
                                }
                            }
                            Protocol::Udp => {
                                if let Some(udp) = layer.udp() {
                                    print!(" {}:{} -> {}:{}",
                                        "src", udp.src_port,
                                        "dst", udp.dst_port);
                                }
                            }
                            Protocol::Tcp => {
                                if let Some(tcp) = layer.tcp() {
                                    print!(" {}:{} -> {}:{} seq={} flags=0x{:02x}",
                                        "src", tcp.src_port,
                                        "dst", tcp.dst_port,
                                        tcp.seq_num, tcp.flags);
                                }
                            }
                            Protocol::SomeIp => {
                                if let Some(someip) = layer.someip() {
                                    print!(" service=0x{:04x} method=0x{:04x} client={}",
                                        someip.service_id, someip.method_id, someip.client_id);
                                }
                            }
                            Protocol::DoIp => {
                                if let Some(doip) = layer.doip() {
                                    print!(" type=0x{:04x} ({})",
                                        doip.payload_type, doip.payload_type_name());
                                }
                            }
                            _ => {}
                        }
                        println!();
                    }

                    // Print payload info
                    let payload = decode_result.payload();
                    if !payload.is_empty() {
                        println!("  Payload: {} bytes", payload.len());
                    }
                }
                println!();

                // Limit output for demo
                if packet_count >= 10 {
                    println!("Captured 10 packets, stopping...");
                    break;
                }
            }
            None => {
                // Timeout, continue
            }
        }
    }

    // Print statistics
    let stats = session.statistics()?;
    println!("\nCapture Statistics:");
    println!("  Received: {}", stats.packets_received);
    println!("  Dropped: {}", stats.packets_dropped);
    println!("  Dropped (interface): {}", stats.packets_dropped_interface);

    // Cleanup
    wadjet::cleanup();
    Ok(())
}

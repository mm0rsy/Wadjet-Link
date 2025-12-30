#!/usr/bin/env python3
"""
Example: Live capture with SOME/IP filtering.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

Note: Requires root/admin privileges for packet capture.
"""

import wadjet
import signal
import sys


def handle_sigint(sig, frame):
    """Handle Ctrl+C gracefully."""
    print("\n\nCapture stopped.")
    sys.exit(0)


def live_capture_example(interface: str, duration_s: int = 30):
    """Capture and display SOME/IP traffic in real-time."""
    
    print(f"\n𓆓 Wadjet Live SOME/IP Monitor")
    print(f"{'=' * 50}")
    print(f"Interface: {interface}")
    print(f"Filter: UDP port 30490 (SOME/IP)")
    print(f"Duration: {duration_s} seconds")
    print(f"\nPress Ctrl+C to stop...\n")
    
    signal.signal(signal.SIGINT, handle_sigint)
    
    packet_count = 0
    
    # Use LiveCapture context manager with BPF filter
    with wadjet.LiveCapture(
        interface,
        filter="udp port 30490",
        promiscuous=True,
    ) as cap:
        # Stream packets with timeout
        for packet in cap.stream(timeout_ms=1000, max_packets=None):
            packet_count += 1
            
            # Decode packet
            result = wadjet.decode(packet)
            
            if result.has_someip():
                hdr = result.someip()
                
                # Get IP info if available
                ip_info = ""
                if result.has_ipv4():
                    ipv4 = result.ipv4()
                    ip_info = f"{ipv4.src_ip_str()} -> {ipv4.dst_ip_str()}"
                
                # Print packet info
                print(f"[{packet_count:5d}] {ip_info}")
                print(f"        Service: 0x{hdr.service_id:04x} "
                      f"Method: 0x{hdr.method_id:04x} "
                      f"Type: {hdr.message_type}")
                
                # Check for SOME/IP-SD
                if result.has_someip_sd():
                    sd = result.someip_sd()
                    print(f"        SD: reboot={sd.reboot_flag} "
                          f"unicast={sd.unicast_flag} "
                          f"entries_len={sd.entries_length}")
    
    print(f"\nTotal packets captured: {packet_count}")


def list_interfaces():
    """List available capture interfaces."""
    print("\nAvailable interfaces:")
    print("-" * 40)
    for device in wadjet.available_devices():
        print(f"  {device}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Live SOME/IP traffic monitor"
    )
    parser.add_argument(
        "interface",
        nargs="?",
        help="Network interface to capture on"
    )
    parser.add_argument(
        "-l", "--list",
        action="store_true",
        help="List available interfaces"
    )
    parser.add_argument(
        "-d", "--duration",
        type=int,
        default=30,
        help="Capture duration in seconds (default: 30)"
    )
    
    args = parser.parse_args()
    
    if args.list:
        list_interfaces()
        return
    
    if not args.interface:
        print("Error: No interface specified.")
        print("Use -l to list available interfaces.")
        parser.print_help()
        sys.exit(1)
    
    live_capture_example(args.interface, args.duration)


if __name__ == "__main__":
    main()

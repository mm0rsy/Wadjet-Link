#!/usr/bin/env python3
"""
Example: Read and analyze SOME/IP traffic from a PCAP file.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

import wadjet
from collections import defaultdict


def analyze_someip_traffic(pcap_path: str):
    """Analyze SOME/IP traffic from a PCAP file."""
    
    print(f"\n𓆓 Wadjet SOME/IP Traffic Analyzer")
    print(f"{'=' * 50}")
    print(f"File: {pcap_path}\n")
    
    # Statistics
    total_packets = 0
    someip_packets = 0
    services = defaultdict(lambda: defaultdict(int))
    message_types = defaultdict(int)
    
    # Process packets
    for packet in wadjet.iter_pcap(pcap_path):
        total_packets += 1
        
        result = wadjet.decode(packet)
        if result.has_someip():
            someip_packets += 1
            header = result.someip()
            
            # Count by service/method
            key = f"0x{header.service_id:04x}"
            method = f"0x{header.method_id:04x}"
            services[key][method] += 1
            
            # Count message types
            msg_type = str(header.message_type)
            message_types[msg_type] += 1
    
    # Print results
    print(f"Total packets:    {total_packets}")
    print(f"SOME/IP packets:  {someip_packets}")
    print(f"SOME/IP ratio:    {100*someip_packets/max(total_packets,1):.1f}%\n")
    
    print("Services discovered:")
    print("-" * 40)
    for service_id, methods in sorted(services.items()):
        print(f"  Service {service_id}:")
        for method_id, count in sorted(methods.items()):
            print(f"    Method {method_id}: {count} packets")
    
    print("\nMessage types:")
    print("-" * 40)
    for msg_type, count in sorted(message_types.items()):
        print(f"  {msg_type}: {count}")


def main():
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python analyze_someip.py <pcap_file>")
        print("\nExample:")
        print("  python analyze_someip.py capture.pcap")
        sys.exit(1)
    
    pcap_path = sys.argv[1]
    analyze_someip_traffic(pcap_path)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Generate SOME/IP-SD PCAP files with various entry and option combinations.
"""

import struct
import socket
import sys
from pathlib import Path

def write_pcap_header(f):
    """Write PCAP file header"""
    # Magic number (little-endian), version 2.4
    f.write(struct.pack('<I', 0xa1b2c3d4))
    f.write(struct.pack('<I', 2))  # Major version
    f.write(struct.pack('<I', 4))  # Minor version
    f.write(struct.pack('<i', 0))  # Timezone offset
    f.write(struct.pack('<I', 0))  # Timestamp accuracy
    f.write(struct.pack('<I', 65535))  # Snaplen
    f.write(struct.pack('<I', 1))  # Network (Ethernet)


def write_pcap_packet(f, packet_data, ts=0):
    """Write PCAP packet header and data"""
    ts_sec = ts // 1000000
    ts_usec = ts % 1000000
    f.write(struct.pack('<I', ts_sec))
    f.write(struct.pack('<I', ts_usec))
    f.write(struct.pack('<I', len(packet_data)))
    f.write(struct.pack('<I', len(packet_data)))
    f.write(packet_data)


def create_ethernet_frame(src_mac, dst_mac, payload):
    """Create Ethernet frame"""
    frame = b''
    # Destination MAC (6 bytes)
    frame += bytes([int(x, 16) for x in dst_mac.split(':')])
    # Source MAC (6 bytes)
    frame += bytes([int(x, 16) for x in src_mac.split(':')])
    # EtherType (0x0800 for IPv4)
    frame += struct.pack('>H', 0x0800)
    # Payload
    frame += payload
    return frame


def calculate_ipv4_checksum(data):
    """Calculate IPv4 header checksum"""
    checksum = 0
    for i in range(0, len(data), 2):
        word = (data[i] << 8) + data[i + 1]
        checksum += word
    while checksum >> 16:
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    return ~checksum & 0xFFFF


def create_ipv4_header(src_ip, dst_ip, payload_len):
    """Create IPv4 header"""
    header = b''
    # Version + IHL (4 bits each): 0x45
    header += bytes([0x45])
    # DSCP + ECN
    header += bytes([0x00])
    # Total length
    header += struct.pack('>H', 20 + payload_len)
    # Identification
    header += struct.pack('>H', 0x0000)
    # Flags + Fragment offset
    header += struct.pack('>H', 0x4000)
    # TTL
    header += bytes([0x40])
    # Protocol (17 for UDP)
    header += bytes([0x11])
    # Checksum (will be calculated)
    header += struct.pack('>H', 0x0000)
    # Source IP
    header += socket.inet_aton(src_ip)
    # Destination IP
    header += socket.inet_aton(dst_ip)
    
    # Calculate and insert checksum
    checksum = calculate_ipv4_checksum(header)
    header = header[:10] + struct.pack('>H', checksum) + header[12:]
    return header


def calculate_udp_checksum(src_ip, dst_ip, udp_data):
    """Calculate UDP checksum including pseudo-header"""
    # Pseudo-header
    pseudo = b''
    pseudo += socket.inet_aton(src_ip)
    pseudo += socket.inet_aton(dst_ip)
    pseudo += bytes([0x00])
    pseudo += bytes([0x11])  # UDP protocol
    pseudo += struct.pack('>H', len(udp_data))
    
    # Full data for checksum
    data = pseudo + udp_data
    
    checksum = 0
    for i in range(0, len(data), 2):
        if i + 1 < len(data):
            word = (data[i] << 8) + data[i + 1]
        else:
            word = (data[i] << 8)
        checksum += word
    
    while checksum >> 16:
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    
    checksum = ~checksum & 0xFFFF
    return checksum if checksum != 0 else 0xFFFF


def create_udp_header(src_port, dst_port, payload):
    """Create UDP header with checksum"""
    header = b''
    header += struct.pack('>H', src_port)
    header += struct.pack('>H', dst_port)
    header += struct.pack('>H', 8 + len(payload))
    header += struct.pack('>H', 0x0000)  # Checksum placeholder
    header += payload
    return header


def create_someip_header(service_id, method_id, client_id, session_id, payload_len, flags=0x00):
    """Create SOME/IP header"""
    header = b''
    header += struct.pack('>H', service_id)
    header += struct.pack('>H', method_id)
    header += struct.pack('>I', 8 + payload_len)  # Length
    header += struct.pack('>H', client_id)
    header += struct.pack('>H', session_id)
    header += bytes([0x10])  # Protocol version
    header += bytes([0x01])  # Interface version
    header += bytes([flags])  # Message type / Return code
    header += bytes([0x00])  # Padding
    return header


def create_sd_header(flags, entries_len, options_len):
    """Create SOME/IP-SD header (after SOME/IP header)"""
    header = b''
    header += bytes([flags])  # Flags (Reboot, Unicast)
    header += bytes([0x00, 0x00, 0x00])  # Reserved
    header += struct.pack('>I', entries_len)  # Entries length
    header += struct.pack('>I', options_len)  # Options length
    return header


def create_sd_entry(entry_type, index1, index2_options, service_id, instance_id, major_ver, ttl, data):
    """Create 16-byte SOME/IP-SD entry"""
    entry = b''
    entry += bytes([entry_type])
    entry += bytes([index1])
    entry += bytes([index2_options >> 4])  # Index2 high nibble
    entry += bytes([(index2_options & 0x0F)])  # Options count in low nibble
    entry += struct.pack('>H', service_id)
    entry += struct.pack('>H', instance_id)
    entry += bytes([major_ver])
    # TTL (24-bit)
    entry += bytes([(ttl >> 16) & 0xFF])
    entry += bytes([(ttl >> 8) & 0xFF])
    entry += bytes([ttl & 0xFF])
    # Remaining 4 bytes depend on entry type
    entry += data[:4]
    return entry


def create_sd_option(opt_type, data):
    """Create SOME/IP-SD option (variable length)"""
    option = b''
    # Length (not including length field itself)
    option += struct.pack('>H', 1 + len(data))
    # Type
    option += bytes([opt_type])
    # Data
    option += data
    return option


def create_someip_packet(src_ip, dst_ip, src_port, dst_port,
                        service_id, entries, options):
    """Create complete UDP+IPv4+Ethernet packet with SOME/IP-SD"""
    # Build SD payload
    entries_data = b''.join(entries)
    options_data = b''.join(options)
    
    sd_header = create_sd_header(0x00, len(entries_data), len(options_data))
    sd_payload = sd_header + entries_data + options_data
    
    # SOME/IP header
    someip_header = create_someip_header(
        0xFFFF,  # Service ID (0xFFFF for SD)
        0x0001,  # Method ID
        0x0000,  # Client ID
        0x0000,  # Session ID
        len(sd_payload),
        0x20  # Message type (Request)
    )
    
    someip_payload = someip_header + sd_payload
    
    # UDP header
    udp_payload = create_udp_header(src_port, dst_port, someip_payload)
    
    # IPv4 header
    ipv4_header = create_ipv4_header(src_ip, dst_ip, len(udp_payload))
    
    # Ethernet frame
    packet = create_ethernet_frame('00:11:22:33:44:55', '00:55:44:33:22:11',
                                   ipv4_header + udp_payload)
    
    return packet


def create_someip_sd_pcap():
    """Create PCAP with various SOME/IP-SD messages"""
    output_path = Path('pcap_samples/protocol-completeness/someip_sd_complex.pcap')
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'wb') as f:
        write_pcap_header(f)
        
        # Scenario 1: Service Offer with IPv4 Endpoint option
        print("Creating SOME/IP-SD packets...")
        print("Scenario 1: Service offer with IPv4 endpoint option")
        entries = []
        
        # OfferService entry: service 0x1234, instance 0x5678, major ver 1, TTL 300
        entry = create_sd_entry(0x01, 0, 1, 0x1234, 0x5678, 0x01, 300, b'\x00\x00\x00\x00')
        entries.append(entry)
        
        # IPv4Endpoint option: 192.168.1.100:30490, UDP
        ipv4_addr = socket.inet_aton('192.168.1.100')
        option_data = ipv4_addr + struct.pack('>H', 30490) + bytes([0x11])  # UDP protocol
        option = create_sd_option(0x04, option_data)
        
        packet = create_someip_packet('192.168.1.100', '255.255.255.255',
                                     30490, 30490, 0xFFFF, entries, [option])
        write_pcap_packet(f, packet, 0)
        
        # Scenario 2: Multiple entries (Find + Offer)
        print("Scenario 2: Find service + offer service")
        entries = []
        
        # FindService: service 0xABCD, instance 0xEF01
        entry1 = create_sd_entry(0x00, 0, 0, 0xABCD, 0xEF01, 0x01, 300, b'\x00\x00\x00\x00')
        entries.append(entry1)
        
        # OfferService: service 0xABCD, instance 0xEF01
        entry2 = create_sd_entry(0x01, 0, 0, 0xABCD, 0xEF01, 0x02, 300, b'\x00\x00\x00\x00')
        entries.append(entry2)
        
        packet = create_someip_packet('192.168.1.100', '255.255.255.255',
                                     30490, 30490, 0xFFFF, entries, [])
        write_pcap_packet(f, packet, 1000000)
        
        # Scenario 3: Subscribe Eventgroup with Multiple Options
        print("Scenario 3: Subscribe eventgroup with multicast option")
        entries = []
        
        # SubscribeEventgroup: service 0x1111, instance 0x2222, eventgroup 0x0001
        entry = create_sd_entry(0x06, 0, 2, 0x1111, 0x2222, 0x01, 300, b'\x00\x00\x00\x01')
        entries.append(entry)
        
        # IPv4Endpoint option for subscription
        ipv4_addr = socket.inet_aton('192.168.1.200')
        opt1_data = ipv4_addr + struct.pack('>H', 30491) + bytes([0x11])
        opt1 = create_sd_option(0x04, opt1_data)
        
        # IPv4Multicast option
        mcast_addr = socket.inet_aton('224.224.224.245')
        opt2_data = mcast_addr + struct.pack('>H', 30490) + bytes([0x11])
        opt2 = create_sd_option(0x14, opt2_data)
        
        packet = create_someip_packet('192.168.1.200', '255.255.255.255',
                                     30490, 30490, 0xFFFF, entries, [opt1, opt2])
        write_pcap_packet(f, packet, 2000000)
        
        # Scenario 4: Configuration Option
        print("Scenario 4: Service with configuration option")
        entries = []
        
        entry = create_sd_entry(0x01, 0, 1, 0x5555, 0x6666, 0x01, 600, b'\x00\x00\x00\x00')
        entries.append(entry)
        
        # Configuration option
        config_data = b'CFG\x00\x00\x00'
        opt = create_sd_option(0x01, config_data)
        
        packet = create_someip_packet('192.168.1.100', '255.255.255.255',
                                     30490, 30490, 0xFFFF, entries, [opt])
        write_pcap_packet(f, packet, 3000000)
        
        # Scenario 5: Load Balancing Option
        print("Scenario 5: Service with load balancing option")
        entries = []
        
        entry = create_sd_entry(0x01, 0, 1, 0x7777, 0x8888, 0x01, 300, b'\x00\x00\x00\x00')
        entries.append(entry)
        
        # LoadBalancing option: priority, weight
        lb_data = struct.pack('>HH', 100, 50) + b'\x00' * 2
        opt = create_sd_option(0x02, lb_data)
        
        packet = create_someip_packet('192.168.1.100', '255.255.255.255',
                                     30490, 30490, 0xFFFF, entries, [opt])
        write_pcap_packet(f, packet, 4000000)
        
        # Scenario 6: StopOffer (TTL=0)
        print("Scenario 6: Stop service offer (TTL=0)")
        entries = []
        
        entry = create_sd_entry(0x81, 0, 0, 0x1234, 0x5678, 0x01, 0, b'\x00\x00\x00\x00')
        entries.append(entry)
        
        packet = create_someip_packet('192.168.1.100', '255.255.255.255',
                                     30490, 30490, 0xFFFF, entries, [])
        write_pcap_packet(f, packet, 5000000)


if __name__ == '__main__':
    create_someip_sd_pcap()
    print(f"\nCreated SOME/IP-SD PCAP: pcap_samples/protocol-completeness/someip_sd_complex.pcap")
    print("Scenarios included:")
    print("  1. Service offer with IPv4 endpoint")
    print("  2. Find + Offer services")
    print("  3. Subscribe eventgroup with multicast")
    print("  4. Service with configuration option")
    print("  5. Service with load balancing option")
    print("  6. Stop service offer (TTL=0)")

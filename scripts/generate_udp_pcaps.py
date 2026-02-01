#!/usr/bin/env python3
"""
Generate UDP PCAP files with valid and invalid checksums.
"""

import struct
import socket

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
    f.write(struct.pack('<I', ts >> 32))  # Timestamp seconds
    f.write(struct.pack('<I', ts & 0xFFFFFFFF))  # Timestamp microseconds
    f.write(struct.pack('<I', len(packet_data)))  # Packet length (incl)
    f.write(struct.pack('<I', len(packet_data)))  # Packet length (orig)
    f.write(packet_data)

def calculate_ipv4_checksum(data):
    """Calculate IPv4 header checksum"""
    checksum = 0
    for i in range(0, len(data), 2):
        word = (data[i] << 8) + data[i + 1] if i + 1 < len(data) else (data[i] << 8)
        checksum += word
    
    checksum = (checksum >> 16) + (checksum & 0xFFFF)
    checksum = (checksum >> 16) + (checksum & 0xFFFF)
    return (~checksum) & 0xFFFF

def calculate_udp_checksum(src_ip, dst_ip, udp_data):
    """Calculate UDP checksum including pseudo-header"""
    # Pseudo-header
    pseudo = struct.pack('!4s4sBBH',
        socket.inet_aton(src_ip),
        socket.inet_aton(dst_ip),
        0,  # Zero
        17,  # UDP protocol
        len(udp_data)
    )
    
    data = pseudo + udp_data
    checksum = 0
    
    for i in range(0, len(data), 2):
        word = (data[i] << 8) + (data[i + 1] if i + 1 < len(data) else 0)
        checksum += word
    
    checksum = (checksum >> 16) + (checksum & 0xFFFF)
    checksum = (checksum >> 16) + (checksum & 0xFFFF)
    return (~checksum) & 0xFFFF

def create_ethernet_frame():
    """Create basic Ethernet frame"""
    # Destination MAC: FF:FF:FF:FF:FF:FF
    # Source MAC: 00:11:22:33:44:55
    # EtherType: IPv4 (0x0800)
    return bytes([
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x08, 0x00
    ])

def create_ipv4_header(src_ip, dst_ip, payload_len):
    """Create IPv4 header"""
    total_len = 20 + payload_len
    
    header = bytearray([
        0x45,  # Version 4, IHL 5
        0x00,  # DSCP/ECN
        (total_len >> 8) & 0xFF,
        total_len & 0xFF,
        0x12, 0x34,  # Identification
        0x40, 0x00,  # Flags, Fragment offset
        0x40,  # TTL
        0x11,  # Protocol (UDP)
        0x00, 0x00,  # Checksum placeholder
    ])
    
    # Add IPs
    header.extend(socket.inet_aton(src_ip))
    header.extend(socket.inet_aton(dst_ip))
    
    # Calculate and set checksum
    checksum = calculate_ipv4_checksum(bytes(header))
    header[10:12] = struct.pack('!H', checksum)
    
    return bytes(header)

def create_udp_header(src_port, dst_port, payload):
    """Create UDP header with checksum"""
    udp_len = 8 + len(payload)
    
    header = bytearray([
        (src_port >> 8) & 0xFF,
        src_port & 0xFF,
        (dst_port >> 8) & 0xFF,
        dst_port & 0xFF,
        (udp_len >> 8) & 0xFF,
        udp_len & 0xFF,
        0x00, 0x00,  # Checksum placeholder
    ])
    
    return bytes(header), udp_len

def create_udp_packet(src_ip, dst_ip, src_port, dst_port, payload, invalid_checksum=False):
    """Create complete UDP packet with Ethernet and IPv4 headers"""
    eth = create_ethernet_frame()
    
    payload_len = 8 + len(payload)
    ipv4 = create_ipv4_header(src_ip, dst_ip, payload_len)
    
    udp_hdr, udp_len = create_udp_header(src_port, dst_port, payload)
    
    # Calculate UDP checksum
    udp_data = bytearray(udp_hdr)
    udp_data.extend(payload)
    
    if invalid_checksum:
        # Use an obviously wrong checksum
        udp_data[6:8] = struct.pack('!H', 0xDEAD)
    else:
        checksum = calculate_udp_checksum(src_ip, dst_ip, bytes(udp_data))
        udp_data[6:8] = struct.pack('!H', checksum)
    
    packet = eth + ipv4 + bytes(udp_data)
    return packet

def create_valid_checksum_pcap():
    """Create PCAP with valid UDP checksums"""
    with open('pcap_samples/protocol-completeness/udp_checksum_valid.pcap', 'wb') as f:
        write_pcap_header(f)
        
        # Packet 1: DNS query (port 53)
        packet1 = create_udp_packet('192.168.1.100', '8.8.8.8', 53443, 53,
                                     bytes([0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]))
        write_pcap_packet(f, packet1, 0)
        
        # Packet 2: NTP query (port 123)
        packet2 = create_udp_packet('192.168.1.100', '129.6.15.28', 5005, 123,
                                     bytes([0x23]) + bytes(47))
        write_pcap_packet(f, packet2, 1000000)
        
        # Packet 3: Empty payload
        packet3 = create_udp_packet('10.0.0.1', '10.0.0.2', 1234, 5678, bytes())
        write_pcap_packet(f, packet3, 2000000)
        
        # Packet 4: Larger payload
        packet4 = create_udp_packet('172.16.0.1', '172.16.0.255', 9999, 9999,
                                     bytes(range(256)))
        write_pcap_packet(f, packet4, 3000000)

def create_invalid_checksum_pcap():
    """Create PCAP with invalid UDP checksums"""
    with open('pcap_samples/protocol-completeness/udp_checksum_invalid.pcap', 'wb') as f:
        write_pcap_header(f)
        
        # Packet 1: Valid header but corrupted checksum
        packet1 = create_udp_packet('192.168.1.100', '8.8.8.8', 53443, 53,
                                     bytes([0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
                                     invalid_checksum=True)
        write_pcap_packet(f, packet1, 0)
        
        # Packet 2: Flipped bits in checksum
        packet2 = create_udp_packet('10.0.0.1', '10.0.0.2', 1234, 5678,
                                     bytes([0xAA, 0xBB, 0xCC, 0xDD]),
                                     invalid_checksum=True)
        write_pcap_packet(f, packet2, 1000000)

if __name__ == '__main__':
    create_valid_checksum_pcap()
    print("Created udp_checksum_valid.pcap")
    
    create_invalid_checksum_pcap()
    print("Created udp_checksum_invalid.pcap")

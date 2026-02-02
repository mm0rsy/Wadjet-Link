#!/usr/bin/env python3
"""
Generate SOME/IP-TP (Transport Protocol) PCAP files with segmented messages.
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

def calculate_ipv4_checksum(data):
    """Calculate IPv4 header checksum"""
    checksum = 0
    for i in range(0, len(data), 2):
        word = (data[i] << 8) + data[i + 1] if i + 1 < len(data) else (data[i] << 8)
        checksum += word
    
    checksum = (checksum >> 16) + (checksum & 0xFFFF)
    checksum = (checksum >> 16) + (checksum & 0xFFFF)
    return (~checksum) & 0xFFFF

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

def create_someip_tp_segment(service_id, method_id, segment_offset, more_segments, payload):
    """Create SOME/IP-TP message with segmentation"""
    
    # SOME/IP Header (16 bytes)
    header = bytearray([
        (service_id >> 8) & 0xFF,
        service_id & 0xFF,
        (method_id >> 8) & 0xFF,
        method_id & 0xFF,
    ])
    
    # Length field (includes 8 bytes from RequestID onwards + TP header + payload)
    tp_header_len = 4  # TP header is 4 bytes
    length = 8 + tp_header_len + len(payload)
    header.extend(struct.pack('!I', length))
    
    # Request ID (4 bytes)
    header.extend([0x01, 0x00, 0x00, 0x01])
    
    # Protocol version
    header.append(0x01)
    
    # Interface version
    header.append(0x00)
    
    # Message type (0x00 = Request, with TP flag if needed)
    msg_type = 0x00
    if more_segments or segment_offset > 0:
        msg_type |= 0x20  # TP flag
    header.append(msg_type)
    
    # Return code
    header.append(0x00)
    
    # TP Header (4 bytes)
    # Reserved + more_segments flag (1 bit) + offset (31 bits)
    tp_hdr = bytearray()
    reserve_flags = 0x01 if more_segments else 0x00
    tp_hdr.append(reserve_flags)
    
    # Offset in 4-byte units
    offset_units = segment_offset // 4
    tp_hdr.append((offset_units >> 16) & 0xFF)
    tp_hdr.append((offset_units >> 8) & 0xFF)
    tp_hdr.append(offset_units & 0xFF)
    
    return bytes(header) + bytes(tp_hdr) + payload

def create_someip_tp_packet(src_ip, dst_ip, src_port, dst_port, 
                           service_id, method_id, segment_offset, 
                           more_segments, payload):
    """Create complete UDP+IPv4+Ethernet packet with SOME/IP-TP"""
    eth = create_ethernet_frame()
    
    someip_tp = create_someip_tp_segment(service_id, method_id, segment_offset, 
                                         more_segments, payload)
    
    udp_hdr, udp_len = create_udp_header(src_port, dst_port, someip_tp)
    
    # Calculate UDP checksum
    udp_data = bytearray(udp_hdr)
    udp_data.extend(someip_tp)
    checksum = calculate_udp_checksum(src_ip, dst_ip, bytes(udp_data))
    udp_data[6:8] = struct.pack('!H', checksum)
    
    ipv4 = create_ipv4_header(src_ip, dst_ip, len(bytes(udp_data)))
    
    packet = eth + ipv4 + bytes(udp_data)
    return packet

def create_someip_tp_pcap():
    """Create PCAP with SOME/IP-TP segmented messages"""
    with open('pcap_samples/protocol-completeness/someip_tp_large.pcap', 'wb') as f:
        write_pcap_header(f)
        
        # Scenario 1: 5000-byte message split into 3 segments (1000 + 1000 + 3000)
        print("Creating SOME/IP-TP segments for 5000-byte message...")
        
        # Segment 1: Offset 0, more_segments=true, 1500 bytes
        payload1 = bytes([0x11] * 1500)
        packet1 = create_someip_tp_packet('192.168.1.100', '192.168.1.200', 
                                         30490, 30490,  # SOME/IP-SD port
                                         0x1234, 0x0001,
                                         0, True, payload1)
        write_pcap_packet(f, packet1, 0)
        print(f"  Segment 1: offset=0, more=True, size={len(payload1)}")
        
        # Segment 2: Offset 1500, more_segments=true, 1500 bytes
        payload2 = bytes([0x22] * 1500)
        packet2 = create_someip_tp_packet('192.168.1.100', '192.168.1.200',
                                         30490, 30490,
                                         0x1234, 0x0001,
                                         1500, True, payload2)
        write_pcap_packet(f, packet2, 1000000)
        print(f"  Segment 2: offset=1500, more=True, size={len(payload2)}")
        
        # Segment 3: Offset 3000, more_segments=false, 2000 bytes
        payload3 = bytes([0x33] * 2000)
        packet3 = create_someip_tp_packet('192.168.1.100', '192.168.1.200',
                                         30490, 30490,
                                         0x1234, 0x0001,
                                         3000, False, payload3)
        write_pcap_packet(f, packet3, 2000000)
        print(f"  Segment 3: offset=3000, more=False, size={len(payload3)}")
        
        # Scenario 2: Large message with 10 segments (10KB total)
        print("Creating SOME/IP-TP segments for 10KB message...")
        segment_size = 1024
        total_size = 10240
        
        for i in range(0, total_size, segment_size):
            more_segs = (i + segment_size) < total_size
            payload = bytes([(0x40 + i // 256) & 0xFF] * min(segment_size, total_size - i))
            
            packet = create_someip_tp_packet('192.168.1.100', '192.168.1.200',
                                            30491, 30491,  # Different session
                                            0x5678, 0x0002,
                                            i, more_segs, payload)
            write_pcap_packet(f, packet, 3000000 + (i // 256) * 100000)
            print(f"  Segment {i//segment_size + 1}: offset={i}, more={more_segs}, size={len(payload)}")

if __name__ == '__main__':
    create_someip_tp_pcap()
    print("\nCreated someip_tp_large.pcap with SOME/IP-TP segmented messages")

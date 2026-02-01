#!/usr/bin/env python3
"""
Generate DoIP PCAP files with power mode state transitions.
"""

import struct
import socket
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


def create_doip_header(protocol_version, payload_type, payload):
    """Create DoIP header (8 bytes)"""
    header = b''
    # Protocol version
    header += bytes([protocol_version])
    # Inverse of protocol version
    header += bytes([protocol_version ^ 0xFF])
    # Payload type (big-endian)
    header += struct.pack('>H', payload_type)
    # Payload length (big-endian)
    header += struct.pack('>I', len(payload))
    return header + payload


def create_doip_packet(src_ip, dst_ip, src_port, dst_port, payload_type, payload):
    """Create complete UDP+IPv4+Ethernet packet with DoIP"""
    doip_payload = create_doip_header(0x02, payload_type, payload)
    udp_payload = create_udp_header(src_port, dst_port, doip_payload)
    ipv4_header = create_ipv4_header(src_ip, dst_ip, len(udp_payload))
    packet = create_ethernet_frame('00:11:22:33:44:55', '00:55:44:33:22:11',
                                   ipv4_header + udp_payload)
    return packet


def create_doip_power_mode_pcap():
    """Create PCAP with DoIP power mode state transitions"""
    output_path = Path('pcap_samples/protocol-completeness/doip_power_mode.pcap')
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'wb') as f:
        write_pcap_header(f)
        
        print("Creating DoIP power mode PCAP...")
        
        # Scenario 1: Entity Status Request
        print("Scenario 1: Entity Status Request")
        packet = create_doip_packet('192.168.1.100', '192.168.1.200',
                                   13400, 13400,
                                   0x4001,  # Entity Status Request
                                   b'')     # No payload
        write_pcap_packet(f, packet, 0)
        
        # Scenario 2: Entity Status Response (Ready state)
        print("Scenario 2: Entity Status Response (Ready)")
        # Entity Status Response: node type (1) + max concurrent sockets (1) + 
        # current concurrent sockets (1) + max connections (2) + reserved (2)
        entity_status_payload = bytes([
            0x00,  # Node type: Not-a-tester
            0x10,  # Max concurrent sockets: 16
            0x02,  # Current concurrent sockets: 2
            0x00, 0x20,  # Max connections: 32
        ])
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x4002,  # Entity Status Response
                                   entity_status_payload)
        write_pcap_packet(f, packet, 100000)
        
        # Scenario 3: Diagnostic Power Mode Request
        print("Scenario 3: Power Mode Request")
        packet = create_doip_packet('192.168.1.100', '192.168.1.200',
                                   13400, 13400,
                                   0x4003,  # Diagnostic Power Mode Request
                                   b'')     # No payload
        write_pcap_packet(f, packet, 200000)
        
        # Scenario 4: Diagnostic Power Mode Response - Ready (0x00)
        print("Scenario 4: Power Mode Response - Ready (0x00)")
        power_mode_payload = bytes([0x00])  # Ready state
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x4004,  # Diagnostic Power Mode Response
                                   power_mode_payload)
        write_pcap_packet(f, packet, 300000)
        
        # Scenario 5: Alive Check Request (from tester)
        print("Scenario 5: Alive Check Request")
        packet = create_doip_packet('192.168.1.100', '192.168.1.200',
                                   13400, 13400,
                                   0x0007,  # Alive Check Request
                                   b'')     # No payload
        write_pcap_packet(f, packet, 400000)
        
        # Scenario 6: Alive Check Response
        print("Scenario 6: Alive Check Response")
        alive_check_payload = struct.pack('>H', 0x0100)  # Tester source address
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x0008,  # Alive Check Response
                                   alive_check_payload)
        write_pcap_packet(f, packet, 500000)
        
        # Scenario 7: Transition - Power Mode Request
        print("Scenario 7: Power Mode Transition - Request for NotReady")
        packet = create_doip_packet('192.168.1.100', '192.168.1.200',
                                   13400, 13400,
                                   0x4003,  # Diagnostic Power Mode Request
                                   b'')
        write_pcap_packet(f, packet, 600000)
        
        # Scenario 8: Transition - Power Mode Response - NotReady (0x01)
        print("Scenario 8: Power Mode Response - NotReady (0x01)")
        power_mode_payload = bytes([0x01])  # NotReady state
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x4004,  # Diagnostic Power Mode Response
                                   power_mode_payload)
        write_pcap_packet(f, packet, 700000)
        
        # Scenario 9: Transition - Power Mode Request
        print("Scenario 9: Power Mode Transition - Request for NotSupported")
        packet = create_doip_packet('192.168.1.100', '192.168.1.200',
                                   13400, 13400,
                                   0x4003,  # Diagnostic Power Mode Request
                                   b'')
        write_pcap_packet(f, packet, 800000)
        
        # Scenario 10: Transition - Power Mode Response - NotSupported (0x02)
        print("Scenario 10: Power Mode Response - NotSupported (0x02)")
        power_mode_payload = bytes([0x02])  # NotSupported state
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x4004,  # Diagnostic Power Mode Response
                                   power_mode_payload)
        write_pcap_packet(f, packet, 900000)
        
        # Scenario 11: Recovery - Transition back to Ready
        print("Scenario 11: Power Mode Recovery - Back to Ready (0x00)")
        power_mode_payload = bytes([0x00])  # Ready state
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x4004,  # Diagnostic Power Mode Response
                                   power_mode_payload)
        write_pcap_packet(f, packet, 1000000)
        
        # Scenario 12: Generic NACK
        print("Scenario 12: Generic NACK (UnknownPayloadType)")
        nack_payload = bytes([0x01])  # UnknownPayloadType
        packet = create_doip_packet('192.168.1.200', '192.168.1.100',
                                   13400, 13400,
                                   0x0000,  # Generic NACK
                                   nack_payload + struct.pack('>H', 0x9999))  # Unknown payload type
        write_pcap_packet(f, packet, 1100000)


if __name__ == '__main__':
    create_doip_power_mode_pcap()
    print(f"\nCreated DoIP Power Mode PCAP: pcap_samples/protocol-completeness/doip_power_mode.pcap")
    print("Scenarios included:")
    print("  1. Entity Status Request")
    print("  2. Entity Status Response (Ready)")
    print("  3. Power Mode Request")
    print("  4. Power Mode Response (Ready - 0x00)")
    print("  5. Alive Check Request")
    print("  6. Alive Check Response")
    print("  7. Power Mode Request (transition)")
    print("  8. Power Mode Response (NotReady - 0x01)")
    print("  9. Power Mode Request (transition)")
    print(" 10. Power Mode Response (NotSupported - 0x02)")
    print(" 11. Power Mode Response (Recovery to Ready - 0x00)")
    print(" 12. Generic NACK (UnknownPayloadType)")

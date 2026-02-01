#!/usr/bin/env python3
"""
Generate gPTP PCAP files with all TLV types.

Creates packets containing gPTP Follow_Up and Announce messages with various TLV types:
- Follow_Up messages with Organization Extension TLV (FOLLOW_UP_INFO subtype)
  containing cumulative scaled rate offset and GM time base information
- Announce messages with Path Trace TLV containing clock identities
- Messages demonstrating graceful handling of unknown TLV types
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
    # EtherType (0x88F7 for PTP)
    frame += struct.pack('>H', 0x88F7)
    # Payload
    frame += payload
    return frame


def create_gptp_header(message_type, source_port_number, sequence_number, 
                       domain_number=0, log_message_interval=5, source_clock_identity=None):
    """Create gPTP common header (34 bytes for IEEE 802.1AS)"""
    if source_clock_identity is None:
        source_clock_identity = b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00'  # Default 8 bytes
    
    header = b''
    
    # Byte 0: Transport specific and message type
    # Bits 7:4 = message type, bits 3:0 = transport specific (0 for Ethernet)
    transport_specific = (message_type << 4) | 0
    header += bytes([transport_specific])
    
    # Byte 1: Version (4) and reserved (4)
    # gPTP uses PTP version 2
    header += bytes([0x02])
    
    # Bytes 2-3: Message length (total including header, default 44 for most)
    # Will be set by caller
    header += struct.pack('>H', 44)
    
    # Byte 4: Domain number
    header += bytes([domain_number])
    
    # Byte 5: Reserved
    header += bytes([0x00])
    
    # Bytes 6-7: Flags
    # Bit 0: TWO_STEP flag
    # For gPTP Sync, we set TWO_STEP; for Follow_Up it's clear
    two_step = 1 if message_type in [0, 1] else 0  # Sync and Follow_Up
    correction_field_bit = 0
    leap_second_bit = 0
    
    flags = (two_step << 8)
    header += struct.pack('>H', flags)
    
    # Bytes 8-15: CorrectionField (64-bit nanoseconds, 48-bit integer + 16-bit fractional)
    header += struct.pack('>q', 0)
    
    # Bytes 16-19: ClockIdentity source (first 4 bytes)
    header += source_clock_identity[:4]
    
    # Bytes 20-21: SourcePortNumber
    header += struct.pack('>H', source_port_number)
    
    # Bytes 22-23: SequenceId
    header += struct.pack('>H', sequence_number)
    
    # Byte 24: LogMessageInterval (signed byte, power of 2)
    # Positive values mean intervals of 2^value seconds
    header += bytes([log_message_interval])
    
    # Bytes 25-31: ClockIdentity source (last 4 bytes)
    header += source_clock_identity[4:]
    
    # Byte 32: Version major (4 bits) and minor (4 bits)
    # For gPTP, typically 0x20 (version 2.0)
    header += bytes([0x20])
    
    # Byte 33: Reserved
    header += bytes([0x00])
    
    return header


def create_follow_up_message_body():
    """Create Follow_Up message body (10 bytes for timestamp)"""
    # Precise origin timestamp (8 bytes seconds + 2 bytes reserved)
    ts_seconds = struct.pack('>Q', 1000000000000000)[2:]  # Take last 6 bytes
    ts_nanoseconds = struct.pack('>I', 500000000)[:2]  # 500ms in nanoseconds, take first 2 bytes
    
    body = b''
    # Epoch seconds (6 bytes)
    body += b'\x00\x00\x00\x01\x00\x00'
    # Nanoseconds (4 bytes)
    body += struct.pack('>I', 500000000)
    
    return body


def create_follow_up_tlv(rate_offset=0, gm_time_base=0):
    """Create Follow_Up information TLV (organization extension)
    
    TLV format:
    - Type (2 bytes): 0x0003 for ORGANIZATION_EXTENSION
    - Length (2 bytes): 28 bytes for Follow_Up info
    - OUI (3 bytes): 0x00 0x80 0xC2 (IEEE 802.1)
    - Subtype (3 bytes): 0x00 0x00 0x01 (Follow_Up info)
    - Rate offset (4 bytes): signed, cumulative scaled rate offset
    - GM time base (2 bytes): unsigned
    - Phase change (12 bytes): last GM phase change nanoseconds
    - Freq change (4 bytes): signed, scaled last GM freq change
    """
    tlv = b''
    
    # Type: ORGANIZATION_EXTENSION (3)
    tlv += struct.pack('>H', 3)
    
    # Length: 28 bytes (OUI 3 + Subtype 3 + Rate 4 + TB 2 + Phase 12 + Freq 4)
    tlv += struct.pack('>H', 28)
    
    # OUI: IEEE 802.1
    tlv += bytes([0x00, 0x80, 0xC2])
    
    # Subtype: FOLLOW_UP_INFO (1)
    tlv += bytes([0x00, 0x00, 0x01])
    
    # Cumulative scaled rate offset (4 bytes, signed)
    # Sample: +0.5% offset = 5000000 in scaled format (parts per billion)
    tlv += struct.pack('>i', rate_offset)
    
    # GM time base indicator (2 bytes)
    tlv += struct.pack('>H', gm_time_base)
    
    # Last GM phase change (12 bytes: 4 bytes MSB + 8 bytes LSB)
    # Nanosecond offset from epoch
    tlv += struct.pack('>I', 0)  # MSB
    tlv += struct.pack('>Q', 1000000000)  # LSB (1 second)
    
    # Scaled last GM freq change (4 bytes, signed)
    tlv += struct.pack('>i', 0)
    
    return tlv


def create_announce_message_body():
    """Create Announce message body (30 bytes minimum)"""
    body = b''
    
    # Origin timestamp (10 bytes)
    body += struct.pack('>H', 1000)  # Seconds (6 bytes, using last 2)
    body += b'\x00\x00\x00\x00'  # Seconds (4 bytes)
    body += struct.pack('>I', 0)  # Nanoseconds
    
    # Current UTC offset (2 bytes)
    body += struct.pack('>h', 37)
    
    # Reserved (1 byte)
    body += bytes([0x00])
    
    # Grandmaster priority1 (1 byte)
    body += bytes([248])
    
    # Grandmaster clock quality (4 bytes)
    body += bytes([6])  # Clock class (6 = locked)
    body += bytes([0x00])  # Clock accuracy
    body += struct.pack('>H', 200)  # Offset scaled log variance
    
    # Grandmaster priority2 (1 byte)
    body += bytes([248])
    
    # Grandmaster clock identity (8 bytes)
    body += b'\x00\x1A\x2B\xFF\xFE\x3C\x4D\x5E'
    
    # Steps removed (2 bytes)
    body += struct.pack('>H', 1)
    
    # Time source (1 byte)
    body += bytes([0x20])  # InternalOscillator
    
    return body


def create_path_trace_tlv(num_clocks=3):
    """Create Path Trace TLV (list of clock identities)
    
    TLV format:
    - Type (2 bytes): 0x0008 for PATH_TRACE
    - Length (2 bytes): 8 * num_clocks
    - Clock identities: 8 bytes each
    """
    tlv = b''
    
    # Type: PATH_TRACE (8)
    tlv += struct.pack('>H', 8)
    
    # Length: 8 bytes per clock identity
    tlv += struct.pack('>H', 8 * num_clocks)
    
    # Add clock identities
    for i in range(num_clocks):
        # Create unique clock identity for each hop
        identity = b'\x00\x1A\x2B\xFF\xFE'
        identity += bytes([0x30 + i, 0x4D, 0x5E])
        tlv += identity
    
    return tlv


def create_unknown_tlv(tlv_type=0xFFFE, data=b'\x01\x02\x03\x04'):
    """Create an unknown TLV for testing graceful handling
    
    TLV format:
    - Type (2 bytes): proprietary/unknown type
    - Length (2 bytes): length of data
    - Data: arbitrary bytes
    """
    tlv = b''
    
    # Type: Unknown proprietary type
    tlv += struct.pack('>H', tlv_type)
    
    # Length
    tlv += struct.pack('>H', len(data))
    
    # Data
    tlv += data
    
    return tlv


def create_follow_up_pcap():
    """Create PCAP with Follow_Up messages containing TLVs"""
    pcap_path = Path('pcap_samples/protocol-completeness/gptp_tlv_rich.pcap')
    pcap_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(pcap_path, 'wb') as f:
        write_pcap_header(f)
        
        timestamp = 0
        
        # Packet 1: Follow_Up with FollowUpInfo TLV (0% rate offset)
        header = create_gptp_header(1, 1, 1, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        # Update message length
        body = create_follow_up_message_body()
        tlv = create_follow_up_tlv(rate_offset=0, gm_time_base=0)
        
        msg_length = 34 + len(body) + len(tlv)
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        timestamp += 100000
        
        # Packet 2: Follow_Up with FollowUpInfo TLV (+0.5% rate offset)
        rate_offset_scaled = int(5000000)  # +0.5% as ppb
        tlv = create_follow_up_tlv(rate_offset=rate_offset_scaled, gm_time_base=1)
        msg_length = 34 + len(body) + len(tlv)
        header = create_gptp_header(1, 1, 2, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        timestamp += 100000
        
        # Packet 3: Follow_Up with FollowUpInfo TLV (-1.0% rate offset)
        rate_offset_scaled = int(-10000000)  # -1.0% as ppb
        tlv = create_follow_up_tlv(rate_offset=rate_offset_scaled, gm_time_base=2)
        msg_length = 34 + len(body) + len(tlv)
        header = create_gptp_header(1, 1, 3, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        timestamp += 100000
        
        # Packet 4: Announce with Path Trace TLV (3 clocks in path)
        header = create_gptp_header(11, 1, 1, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        body = create_announce_message_body()
        tlv = create_path_trace_tlv(num_clocks=3)
        
        msg_length = 34 + len(body) + len(tlv)
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        timestamp += 100000
        
        # Packet 5: Announce with Path Trace TLV (5 clocks in path)
        header = create_gptp_header(11, 1, 2, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        body = create_announce_message_body()
        tlv = create_path_trace_tlv(num_clocks=5)
        
        msg_length = 34 + len(body) + len(tlv)
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        timestamp += 100000
        
        # Packet 6: Follow_Up with multiple TLVs (FollowUpInfo + unknown)
        header = create_gptp_header(1, 1, 4, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        body = create_follow_up_message_body()
        tlv1 = create_follow_up_tlv(rate_offset=0, gm_time_base=0)
        tlv2 = create_unknown_tlv(tlv_type=0xFFFE, data=b'\x00\x11\x22\x33')
        tlv_combined = tlv1 + tlv2
        
        msg_length = 34 + len(body) + len(tlv_combined)
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv_combined
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        timestamp += 100000
        
        # Packet 7: Announce with multiple TLVs (Path Trace + unknown)
        header = create_gptp_header(11, 1, 3, source_clock_identity=b'\x00\x1A\x2B\x3C\x4D\x5E\x6F\x00')
        body = create_announce_message_body()
        tlv1 = create_path_trace_tlv(num_clocks=2)
        tlv2 = create_unknown_tlv(tlv_type=0xFFFF, data=b'\xAA\xBB\xCC\xDD')
        tlv_combined = tlv1 + tlv2
        
        msg_length = 34 + len(body) + len(tlv_combined)
        header = header[:2] + struct.pack('>H', msg_length) + header[4:]
        
        payload = header + body + tlv_combined
        packet = create_ethernet_frame('00:1A:2B:3C:4D:5E', 'FF:FF:FF:FF:FF:FF', payload)
        write_pcap_packet(f, packet, timestamp)
        
    return pcap_path


if __name__ == '__main__':
    try:
        pcap_file = create_follow_up_pcap()
        print(f"✓ Created {pcap_file} with gPTP TLV samples")
        print(f"  - 3 Follow_Up messages with FollowUpInfo TLV (0%, +0.5%, -1.0% rate offsets)")
        print(f"  - 2 Announce messages with Path Trace TLV (3 and 5 clocks)")
        print(f"  - 2 messages with multiple TLVs (testing graceful unknown TLV handling)")
        print(f"  Total: 7 packets demonstrating all TLV types and scenarios")
    except Exception as e:
        print(f"✗ Error creating PCAP: {e}", file=sys.stderr)
        sys.exit(1)

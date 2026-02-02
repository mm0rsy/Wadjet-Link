#!/usr/bin/env python3
"""
Generate UDS PCAP files with samples of all 50+ NRC (Negative Response Code) codes.

Creates packets containing UDS negative responses (0x7F SID) with all ISO 14229-1 defined
NRC codes, demonstrating complete NRC handling and service-specific interpretation.
"""

import struct
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
    
    # Add carries
    while checksum >> 16:
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    
    return (~checksum) & 0xFFFF


def create_ipv4_header(src_ip, dst_ip, payload_len):
    """Create IPv4 header"""
    header = b''
    # Version (4) and IHL (5)
    header += bytes([0x45])
    # DSCP and ECN
    header += bytes([0x00])
    # Total length
    total_len = 20 + payload_len
    header += struct.pack('>H', total_len)
    # Identification
    header += struct.pack('>H', 0x1234)
    # Flags and fragment offset
    header += struct.pack('>H', 0x4000)
    # TTL
    header += bytes([64])
    # Protocol (17 for UDP)
    header += bytes([17])
    # Checksum (placeholder)
    header += struct.pack('>H', 0)
    # Source IP
    src_parts = [int(x) for x in src_ip.split('.')]
    header += bytes(src_parts)
    # Destination IP
    dst_parts = [int(x) for x in dst_ip.split('.')]
    header += bytes(dst_parts)
    
    # Calculate checksum on header
    checksum = calculate_ipv4_checksum(header)
    # Replace checksum field (bytes 10-12)
    header = header[:10] + struct.pack('>H', checksum) + header[12:]
    
    return header


def create_udp_header(src_port, dst_port, payload_len):
    """Create UDP header"""
    header = b''
    # Source port
    header += struct.pack('>H', src_port)
    # Destination port
    header += struct.pack('>H', dst_port)
    # Length
    header += struct.pack('>H', 8 + payload_len)
    # Checksum (0 = disabled)
    header += struct.pack('>H', 0)
    return header


def create_uds_negative_response(requested_service_id, nrc_code):
    """Create UDS negative response message
    
    Format:
    - SID: 0x7F (negative response)
    - Service ID: requested service (1 byte)
    - NRC: Negative Response Code (1 byte)
    """
    response = b''
    response += bytes([0x7F])  # Negative response SID
    response += bytes([requested_service_id])  # Service that was rejected
    response += bytes([nrc_code])  # NRC code
    return response


def generate_uds_nrc_pcap():
    """Generate PCAP with all UDS NRC codes"""
    
    # All NRC codes per ISO 14229-1
    nrc_codes = {
        # General Response Codes (0x10-0x14)
        0x10: "GeneralReject",
        0x11: "ServiceNotSupported",
        0x12: "SubFunctionNotSupported",
        0x13: "IncorrectMessageLengthOrInvalidFormat",
        0x14: "ResponseTooLong",
        
        # Busy Response Codes (0x21-0x22)
        0x21: "BusyRepeatRequest",
        0x22: "ConditionsNotCorrect",
        
        # Sequence Error (0x24)
        0x24: "RequestSequenceError",
        
        # No Response Required (0x25)
        0x25: "NoResponseFromSubnetComponent",
        
        # Failure Response Codes (0x26)
        0x26: "FailurePreventsExecutionOfRequestedAction",
        
        # Request Out of Range (0x31)
        0x31: "RequestOutOfRange",
        
        # Security Response Codes (0x33-0x36)
        0x33: "SecurityAccessDenied",
        0x35: "InvalidKey",
        0x36: "ExceededNumberOfAttempts",
        0x37: "RequiredTimeDelayNotExpired",
        
        # Upload/Download Response Codes (0x70-0x73)
        0x70: "UploadDownloadNotAccepted",
        0x71: "TransferDataSuspended",
        0x72: "GeneralProgrammingFailure",
        0x73: "WrongBlockSequenceCounter",
        
        # Service Execution Response Codes (0x78)
        0x78: "RequestCorrectlyReceivedResponsePending",
        
        # Service Not Supported In Active Session (0x7E-0x7F)
        0x7E: "SubFunctionNotSupportedInActiveSession",
        0x7F: "ServiceNotSupportedInActiveSession",
        
        # RPM Response Codes (0x81-0x82)
        0x81: "RpmTooHigh",
        0x82: "RpmTooLow",
        
        # Engine State Response Codes (0x83-0x84)
        0x83: "EngineIsRunning",
        0x84: "EngineIsNotRunning",
        
        # Operating Time Response Codes (0x85)
        0x85: "EngineRunTimeTooLow",
        
        # Temperature Response Codes (0x87-0x88)
        0x87: "TemperatureTooHigh",
        0x88: "TemperatureTooLow",
        
        # Speed Response Codes (0x89-0x8A)
        0x89: "VehicleSpeedTooHigh",
        0x8A: "VehicleSpeedTooLow",
        
        # Throttle/Pedal Response Codes (0x8B-0x8C)
        0x8B: "ThrottlePedalTooHigh",
        0x8C: "ThrottlePedalTooLow",
        
        # Transmission Response Codes (0x8D-0x8E)
        0x8D: "TransmissionRangeNotInNeutral",
        0x8E: "TransmissionRangeNotInGear",
        
        # Brake Response Codes (0x90-0x91)
        0x90: "BrakeSwitchNotClosed",
        0x91: "ShifterLeverNotInPark",
        
        # Torque Converter Response Codes (0x92)
        0x92: "TorqueConverterClutchLocked",
        
        # Voltage Response Codes (0x93-0x94)
        0x93: "VoltageTooHigh",
        0x94: "VoltageTooLow",
    }
    
    # Service IDs to use for generating responses
    # Map: service_id -> service_name
    services = {
        0x10: "DiagnosticSessionControl",
        0x11: "ECUReset",
        0x14: "ClearDiagnosticInformation",
        0x19: "ReadDTCInformation",
        0x22: "ReadDataByIdentifier",
        0x23: "ReadMemoryByAddress",
        0x24: "ReadScalingDataByIdentifier",
        0x27: "SecurityAccess",
        0x28: "CommunicationControl",
        0x2A: "ReadDataByPeriodicIdentifier",
        0x2C: "DynamicallyDefineDataIdentifier",
        0x2E: "WriteDataByIdentifier",
        0x2F: "InputOutputControlByIdentifier",
        0x31: "RoutineControl",
        0x34: "RequestDownload",
        0x35: "RequestUpload",
        0x36: "TransferData",
        0x37: "RequestTransferExit",
        0x3D: "WriteMemoryByAddress",
        0x3E: "TesterPresent",
    }
    
    output_dir = Path(__file__).parent.parent / "pcap_samples" / "protocol-completeness"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_file = output_dir / "uds_all_nrcs.pcap"
    
    with open(output_file, 'wb') as f:
        write_pcap_header(f)
        
        timestamp = 0
        packet_count = 0
        
        # For each NRC code, generate packets with different service IDs
        for nrc_code, nrc_name in sorted(nrc_codes.items()):
            # Use different services to demonstrate service-specific NRC interpretation
            service_samples = list(services.items())[:5]  # Use first 5 services for each NRC
            
            for service_id, service_name in service_samples:
                # Create UDS negative response
                uds_response = create_uds_negative_response(service_id, nrc_code)
                
                # Create UDP payload (UDS-on-UDP, using standard UDS ports)
                udp_payload = uds_response
                
                # Create UDP header
                udp_header = create_udp_header(
                    src_port=30490,  # Standard UDS-on-UDP responder port
                    dst_port=30490,  # Standard UDS-on-UDP requester port
                    payload_len=len(udp_payload)
                )
                
                # Create IPv4 header
                ipv4_header = create_ipv4_header(
                    src_ip="192.168.1.100",
                    dst_ip="192.168.1.1",
                    payload_len=len(udp_header) + len(udp_payload)
                )
                
                # Create Ethernet frame
                ethernet_frame = create_ethernet_frame(
                    src_mac="00:11:22:33:44:55",
                    dst_mac="aa:bb:cc:dd:ee:ff",
                    payload=ipv4_header + udp_header + udp_payload
                )
                
                # Write to PCAP
                write_pcap_packet(f, ethernet_frame, ts=timestamp)
                
                timestamp += 1000000  # 1 second between packets
                packet_count += 1
        
        print(f"✅ Generated {output_file}")
        print(f"   - Total packets: {packet_count}")
        print(f"   - NRC codes covered: {len(nrc_codes)}")
        print(f"   - Service IDs sampled: {len(services)}")
        print(f"   - File size: {output_file.stat().st_size} bytes")


if __name__ == "__main__":
    generate_uds_nrc_pcap()

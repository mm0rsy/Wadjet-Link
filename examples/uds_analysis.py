#!/usr/bin/env python3
"""
UDS Protocol Analysis Script

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

This script demonstrates how to use Wadjet-Link Python bindings for
UDS protocol analysis, including:
- Reading PCAP files with DoIP+UDS traffic
- Decoding UDS messages
- Tracking session state
- Generating analysis reports

Usage:
    python uds_analysis.py capture.pcap
    python uds_analysis.py --json capture.pcap
    python uds_analysis.py --filter-ecu 0x1234 capture.pcap
"""

import argparse
import json
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional

try:
    import wadjet
    from wadjet import UdsDecoder, DoIPDecoder, PcapReader
    from wadjet import UdsServiceId, UdsSessionType, UdsNRC
except ImportError:
    print("Error: Wadjet-Link Python bindings not installed.")
    print("Install with: pip install wadjet")
    sys.exit(1)


# =============================================================================
# Data Classes
# =============================================================================

@dataclass
class UdsMessage:
    """Represents a decoded UDS message."""
    timestamp: datetime
    source: int
    target: int
    service_id: int
    service_name: str
    payload: bytes
    is_response: bool
    is_negative: bool = False
    nrc: Optional[int] = None
    nrc_name: Optional[str] = None


@dataclass
class EcuStatistics:
    """Statistics for a single ECU."""
    address: int
    request_count: int = 0
    response_count: int = 0
    negative_count: int = 0
    services_used: set = field(default_factory=set)
    nrc_codes: list = field(default_factory=list)
    session_history: list = field(default_factory=list)
    first_seen: Optional[datetime] = None
    last_seen: Optional[datetime] = None
    
    def to_dict(self):
        return {
            "address": f"0x{self.address:04X}",
            "request_count": self.request_count,
            "response_count": self.response_count,
            "negative_count": self.negative_count,
            "services_used": list(self.services_used),
            "nrc_codes": self.nrc_codes,
            "session_history": self.session_history,
            "first_seen": self.first_seen.isoformat() if self.first_seen else None,
            "last_seen": self.last_seen.isoformat() if self.last_seen else None,
        }


# =============================================================================
# UDS Analyzer
# =============================================================================

class UdsAnalyzer:
    """Analyzes UDS traffic from PCAP files."""
    
    # Common DID names
    DID_NAMES = {
        0xF186: "ActiveSession",
        0xF187: "SparePartNumber",
        0xF188: "SoftwareNumber",
        0xF189: "SoftwareVersion",
        0xF18A: "SupplierID",
        0xF18B: "ManufacturingDate",
        0xF18C: "SerialNumber",
        0xF190: "VIN",
        0xF191: "HardwareNumber",
        0xF192: "SupplierHardware",
        0xF193: "HardwareVersion",
        0xF194: "SupplierSoftware",
        0xF195: "SupplierSWVersion",
        0xF197: "SystemName",
        0xF199: "ProgrammingDate",
    }
    
    def __init__(self, filter_ecu: Optional[int] = None):
        self.uds_decoder = UdsDecoder()
        self.doip_decoder = DoIPDecoder()
        self.filter_ecu = filter_ecu
        self.messages: list[UdsMessage] = []
        self.ecu_stats: dict[int, EcuStatistics] = defaultdict(
            lambda: EcuStatistics(address=0)
        )
        self.packet_count = 0
        self.uds_count = 0
        
    def analyze_pcap(self, pcap_path: str):
        """Analyze a PCAP file and extract UDS messages."""
        reader = PcapReader(pcap_path)
        
        for packet in reader:
            self.packet_count += 1
            self._process_packet(packet)
            
    def _process_packet(self, packet):
        """Process a single packet."""
        # Decode DoIP layer
        doip = self.doip_decoder.decode(packet.data)
        if not doip:
            return
            
        # Check for diagnostic message
        if doip.payload_type not in (
            0x8001,  # Diagnostic message
            0x8002,  # Diagnostic positive ack
            0x8003,  # Diagnostic negative ack
        ):
            return
            
        if len(doip.payload) < 4:
            return
            
        # Extract addressing
        source = (doip.payload[0] << 8) | doip.payload[1]
        target = (doip.payload[2] << 8) | doip.payload[3]
        
        # Apply filter
        if self.filter_ecu and source != self.filter_ecu and target != self.filter_ecu:
            return
            
        # Extract UDS payload
        uds_data = doip.payload[4:]
        if not uds_data:
            return
            
        # Decode UDS
        uds = self.uds_decoder.decode(uds_data)
        if not uds:
            return
            
        self.uds_count += 1
        
        # Determine message type
        service_id = uds.service_id
        is_response = (service_id & 0x40) != 0
        is_negative = (service_id == 0x7F)
        
        msg = UdsMessage(
            timestamp=packet.timestamp,
            source=source,
            target=target,
            service_id=service_id,
            service_name=self._get_service_name(service_id),
            payload=bytes(uds.payload),
            is_response=is_response,
            is_negative=is_negative,
        )
        
        # Handle negative response
        if is_negative and len(uds.payload) >= 2:
            msg.nrc = uds.payload[1]
            msg.nrc_name = self._get_nrc_name(msg.nrc)
            
        self.messages.append(msg)
        self._update_statistics(msg)
        
    def _update_statistics(self, msg: UdsMessage):
        """Update ECU statistics with message."""
        # Determine ECU address (source for responses, target for requests)
        ecu_addr = msg.source if msg.is_response else msg.target
        stats = self.ecu_stats[ecu_addr]
        stats.address = ecu_addr
        
        # Update counts
        if msg.is_negative:
            stats.negative_count += 1
            if msg.nrc:
                stats.nrc_codes.append({
                    "service": self._get_service_name(msg.payload[0] if msg.payload else 0),
                    "nrc": msg.nrc_name or f"0x{msg.nrc:02X}",
                    "timestamp": msg.timestamp.isoformat(),
                })
        elif msg.is_response:
            stats.response_count += 1
        else:
            stats.request_count += 1
            
        # Track services
        base_service = msg.service_id & 0xBF  # Remove response bit
        stats.services_used.add(self._get_service_name(base_service))
        
        # Track session changes
        if msg.service_id in (0x50, 0x10):  # DSC response/request
            if msg.payload:
                session = msg.payload[0]
                stats.session_history.append({
                    "session": self._get_session_name(session),
                    "timestamp": msg.timestamp.isoformat(),
                })
                
        # Update timestamps
        if stats.first_seen is None or msg.timestamp < stats.first_seen:
            stats.first_seen = msg.timestamp
        if stats.last_seen is None or msg.timestamp > stats.last_seen:
            stats.last_seen = msg.timestamp
            
    def _get_service_name(self, sid: int) -> str:
        """Get human-readable service name."""
        names = {
            0x10: "DiagnosticSessionControl",
            0x11: "ECUReset",
            0x14: "ClearDiagnosticInformation",
            0x19: "ReadDTCInformation",
            0x22: "ReadDataByIdentifier",
            0x23: "ReadMemoryByAddress",
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
            0x38: "RequestFileTransfer",
            0x3E: "TesterPresent",
            0x7F: "NegativeResponse",
            0x85: "ControlDTCSetting",
            0x86: "ResponseOnEvent",
            0x87: "LinkControl",
        }
        
        # Handle responses (SID + 0x40)
        if sid & 0x40:
            base = sid & 0xBF
            base_name = names.get(base, f"Unknown_0x{base:02X}")
            return f"{base_name}_Response"
            
        return names.get(sid, f"Unknown_0x{sid:02X}")
        
    def _get_session_name(self, session: int) -> str:
        """Get human-readable session name."""
        names = {
            0x01: "Default",
            0x02: "Programming",
            0x03: "Extended",
            0x04: "SafetySystemDiag",
        }
        return names.get(session, f"OEM_0x{session:02X}")
        
    def _get_nrc_name(self, nrc: int) -> str:
        """Get human-readable NRC name."""
        names = {
            0x10: "GeneralReject",
            0x11: "ServiceNotSupported",
            0x12: "SubFunctionNotSupported",
            0x13: "IncorrectMessageLength",
            0x14: "ResponseTooLong",
            0x21: "BusyRepeatRequest",
            0x22: "ConditionsNotCorrect",
            0x24: "RequestSequenceError",
            0x25: "NoResponseFromSubnetComponent",
            0x26: "FailurePreventsExecution",
            0x31: "RequestOutOfRange",
            0x33: "SecurityAccessDenied",
            0x35: "InvalidKey",
            0x36: "ExceedNumberOfAttempts",
            0x37: "RequiredTimeDelayNotExpired",
            0x70: "UploadDownloadNotAccepted",
            0x71: "TransferDataSuspended",
            0x72: "GeneralProgrammingFailure",
            0x73: "WrongBlockSequenceCounter",
            0x78: "ResponsePending",
            0x7E: "SubFunctionNotSupportedInActiveSession",
            0x7F: "ServiceNotSupportedInActiveSession",
        }
        return names.get(nrc, f"Unknown_0x{nrc:02X}")
        
    def get_did_name(self, did: int) -> str:
        """Get human-readable DID name."""
        return self.DID_NAMES.get(did, f"OEM_0x{did:04X}")
        
    def generate_report(self) -> dict:
        """Generate analysis report."""
        # DID analysis
        did_reads = defaultdict(int)
        did_writes = defaultdict(int)
        
        for msg in self.messages:
            if msg.service_id == 0x22 and len(msg.payload) >= 2:  # RDBI
                did = (msg.payload[0] << 8) | msg.payload[1]
                did_reads[did] += 1
            elif msg.service_id == 0x2E and len(msg.payload) >= 2:  # WDBI
                did = (msg.payload[0] << 8) | msg.payload[1]
                did_writes[did] += 1
                
        # Service analysis
        service_counts = defaultdict(int)
        for msg in self.messages:
            base_service = msg.service_id & 0xBF
            service_counts[self._get_service_name(base_service)] += 1
            
        # NRC analysis
        nrc_counts = defaultdict(int)
        for msg in self.messages:
            if msg.is_negative and msg.nrc:
                nrc_counts[msg.nrc_name or f"0x{msg.nrc:02X}"] += 1
                
        return {
            "summary": {
                "total_packets": self.packet_count,
                "uds_messages": self.uds_count,
                "ecus_found": len(self.ecu_stats),
                "negative_responses": sum(s.negative_count for s in self.ecu_stats.values()),
            },
            "ecus": {
                f"0x{addr:04X}": stats.to_dict()
                for addr, stats in self.ecu_stats.items()
            },
            "services": dict(service_counts),
            "dids_read": {
                f"0x{did:04X} ({self.get_did_name(did)})": count
                for did, count in sorted(did_reads.items())
            },
            "dids_written": {
                f"0x{did:04X} ({self.get_did_name(did)})": count
                for did, count in sorted(did_writes.items())
            },
            "negative_responses": dict(nrc_counts),
        }


# =============================================================================
# Report Formatters
# =============================================================================

def print_text_report(report: dict):
    """Print report in human-readable text format."""
    print("\n" + "=" * 70)
    print("                𓆓 Wadjet-Link UDS Analysis Report")
    print("=" * 70)
    
    # Summary
    print("\n📊 Summary")
    print("-" * 40)
    summary = report["summary"]
    print(f"  Total packets:       {summary['total_packets']}")
    print(f"  UDS messages:        {summary['uds_messages']}")
    print(f"  ECUs discovered:     {summary['ecus_found']}")
    print(f"  Negative responses:  {summary['negative_responses']}")
    
    # ECU details
    print("\n📡 ECU Statistics")
    print("-" * 40)
    for addr, ecu in report["ecus"].items():
        print(f"\n  {addr}:")
        print(f"    Requests:  {ecu['request_count']}")
        print(f"    Responses: {ecu['response_count']}")
        print(f"    NRCs:      {ecu['negative_count']}")
        if ecu['services_used']:
            print(f"    Services:  {', '.join(sorted(ecu['services_used']))}")
        if ecu['session_history']:
            sessions = [s['session'] for s in ecu['session_history']]
            print(f"    Sessions:  {' → '.join(sessions)}")
            
    # Services
    print("\n🔧 Service Usage")
    print("-" * 40)
    for service, count in sorted(report["services"].items(), key=lambda x: -x[1]):
        print(f"  {service:40s} {count:5d}")
        
    # DIDs read
    if report["dids_read"]:
        print("\n📖 DIDs Read")
        print("-" * 40)
        for did, count in report["dids_read"].items():
            print(f"  {did:45s} {count:5d}")
            
    # DIDs written
    if report["dids_written"]:
        print("\n✏️  DIDs Written")
        print("-" * 40)
        for did, count in report["dids_written"].items():
            print(f"  {did:45s} {count:5d}")
            
    # Negative responses
    if report["negative_responses"]:
        print("\n❌ Negative Response Codes")
        print("-" * 40)
        for nrc, count in sorted(report["negative_responses"].items(), key=lambda x: -x[1]):
            print(f"  {nrc:40s} {count:5d}")
            
    print("\n" + "=" * 70)


def print_json_report(report: dict):
    """Print report in JSON format."""
    print(json.dumps(report, indent=2, default=str))


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="UDS Protocol Analysis Tool"
    )
    parser.add_argument(
        "pcap_file",
        help="PCAP file to analyze"
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output in JSON format"
    )
    parser.add_argument(
        "--filter-ecu", "-e",
        type=lambda x: int(x, 0),
        help="Filter by ECU address (hex)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    # Validate input file
    pcap_path = Path(args.pcap_file)
    if not pcap_path.exists():
        print(f"Error: File not found: {pcap_path}", file=sys.stderr)
        sys.exit(1)
        
    # Analyze
    analyzer = UdsAnalyzer(filter_ecu=args.filter_ecu)
    
    if args.verbose and not args.json:
        print(f"Analyzing: {pcap_path}")
        if args.filter_ecu:
            print(f"Filtering for ECU: 0x{args.filter_ecu:04X}")
            
    analyzer.analyze_pcap(str(pcap_path))
    
    # Generate and print report
    report = analyzer.generate_report()
    
    if args.json:
        print_json_report(report)
    else:
        print_text_report(report)
        

if __name__ == "__main__":
    main()

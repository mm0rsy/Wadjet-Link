#!/usr/bin/env python3
"""
𓆓 Wadjet-Link — DDS/RTPS Traffic Analysis Example

This example demonstrates how to analyze DDS/RTPS traffic using Wadjet-Link's
Python bindings. It shows how to:

1. Capture and decode RTPS packets
2. Track DDS participants and endpoints
3. Analyze topic discovery patterns
4. Monitor QoS parameters and data exchange
5. Detect multi-vendor interoperability issues

Requirements:
    pip install wadjet

Usage:
    python dds_analysis.py capture eth0           # Live capture
    python dds_analysis.py analyze trace.pcap    # Analyze pcap file
    python dds_analysis.py monitor eth0 --ros2   # ROS2-focused monitoring
"""

import argparse
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from typing import Dict, List, Optional, Set

try:
    import wadjet
    from wadjet.protocols import (
        DdsVendor,
        DdsSubmessageKind,
        RtpsInfo,
        is_rtps,
        get_rtps,
        rtps_filter,
    )
except ImportError:
    print("Error: wadjet package not installed.")
    print("Install with: pip install wadjet")
    sys.exit(1)


@dataclass
class DdsParticipant:
    """Represents a DDS domain participant."""
    guid_prefix: str
    vendor: DdsVendor
    first_seen: datetime
    last_seen: datetime
    src_ip: str
    endpoints: Set[str] = field(default_factory=set)
    topics_published: Set[str] = field(default_factory=set)
    topics_subscribed: Set[str] = field(default_factory=set)
    packet_count: int = 0
    
    def is_ros2(self) -> bool:
        """Check if participant is likely ROS2-based."""
        return self.vendor == DdsVendor.FASTDDS or self.vendor == DdsVendor.CYCLONEDDS


@dataclass
class DdsTopic:
    """Represents a discovered DDS topic."""
    name: str
    type_name: str
    publishers: Set[str] = field(default_factory=set)
    subscribers: Set[str] = field(default_factory=set)
    message_count: int = 0
    last_message: Optional[datetime] = None
    

@dataclass
class DdsAnalysisStats:
    """Statistics for DDS traffic analysis."""
    total_packets: int = 0
    rtps_packets: int = 0
    discovery_packets: int = 0
    data_packets: int = 0
    heartbeat_count: int = 0
    acknack_count: int = 0
    gap_count: int = 0
    vendors_seen: Dict[DdsVendor, int] = field(default_factory=lambda: defaultdict(int))


class DdsAnalyzer:
    """
    DDS/RTPS traffic analyzer.
    
    Provides comprehensive analysis of DDS traffic including:
    - Participant discovery tracking
    - Topic enumeration
    - QoS pattern analysis
    - Reliability monitoring
    """
    
    def __init__(self, ros2_mode: bool = False):
        self.ros2_mode = ros2_mode
        self.participants: Dict[str, DdsParticipant] = {}
        self.topics: Dict[str, DdsTopic] = {}
        self.stats = DdsAnalysisStats()
        self._discovery_endpoints: Set[str] = set()
        
    def process_packet(self, packet: wadjet.Packet) -> Optional[RtpsInfo]:
        """
        Process a single packet and extract DDS information.
        
        Args:
            packet: Wadjet packet to process
            
        Returns:
            RtpsInfo if packet is RTPS, None otherwise
        """
        self.stats.total_packets += 1
        
        if not is_rtps(packet):
            return None
            
        self.stats.rtps_packets += 1
        rtps = get_rtps(packet)
        
        if rtps is None:
            return None
            
        # Update vendor statistics
        self.stats.vendors_seen[rtps.vendor] += 1
        
        # Track participant
        self._track_participant(rtps, packet)
        
        # Analyze submessages
        self._analyze_submessages(rtps)
        
        return rtps
        
    def _track_participant(self, rtps: RtpsInfo, packet: wadjet.Packet) -> None:
        """Track or update a DDS participant."""
        guid_prefix = rtps.guid_prefix
        now = datetime.now()
        
        if guid_prefix not in self.participants:
            # Extract source IP if available
            src_ip = ""
            if hasattr(packet, 'ipv4') and packet.ipv4:
                src_ip = packet.ipv4.src_ip
                
            self.participants[guid_prefix] = DdsParticipant(
                guid_prefix=guid_prefix,
                vendor=rtps.vendor,
                first_seen=now,
                last_seen=now,
                src_ip=src_ip,
            )
        else:
            self.participants[guid_prefix].last_seen = now
            
        self.participants[guid_prefix].packet_count += 1
        
    def _analyze_submessages(self, rtps: RtpsInfo) -> None:
        """Analyze RTPS submessages for traffic patterns."""
        for submsg in rtps.submessages:
            kind = submsg.kind
            
            if kind == DdsSubmessageKind.DATA:
                self.stats.data_packets += 1
            elif kind == DdsSubmessageKind.HEARTBEAT:
                self.stats.heartbeat_count += 1
            elif kind == DdsSubmessageKind.ACKNACK:
                self.stats.acknack_count += 1
            elif kind == DdsSubmessageKind.GAP:
                self.stats.gap_count += 1
            elif kind == DdsSubmessageKind.INFO_TS:
                pass  # Timestamp info
                
        # Check if discovery traffic
        if rtps.is_discovery:
            self.stats.discovery_packets += 1
            
    def get_reliability_ratio(self, guid_prefix: str) -> float:
        """
        Calculate reliability ratio for a participant.
        
        Higher ratio indicates more reliable communication patterns
        (more HEARTBEATs and ACKNACKs relative to DATA).
        """
        if self.stats.data_packets == 0:
            return 0.0
            
        reliability_msgs = self.stats.heartbeat_count + self.stats.acknack_count
        return reliability_msgs / self.stats.data_packets
        
    def detect_ros2_nodes(self) -> List[str]:
        """Detect likely ROS2 nodes based on vendor and patterns."""
        ros2_nodes = []
        for guid, participant in self.participants.items():
            if participant.is_ros2():
                ros2_nodes.append(guid)
        return ros2_nodes
        
    def print_summary(self) -> None:
        """Print analysis summary to console."""
        print("\n" + "=" * 60)
        print("𓆓 Wadjet-Link DDS/RTPS Analysis Summary")
        print("=" * 60)
        
        print(f"\nPacket Statistics:")
        print(f"  Total packets:     {self.stats.total_packets}")
        print(f"  RTPS packets:      {self.stats.rtps_packets}")
        print(f"  Discovery packets: {self.stats.discovery_packets}")
        print(f"  DATA submessages:  {self.stats.data_packets}")
        print(f"  HEARTBEATs:        {self.stats.heartbeat_count}")
        print(f"  ACKNACKs:          {self.stats.acknack_count}")
        print(f"  GAPs:              {self.stats.gap_count}")
        
        print(f"\nVendors Detected:")
        for vendor, count in sorted(self.stats.vendors_seen.items(), key=lambda x: -x[1]):
            print(f"  {vendor.name}: {count} packets")
            
        print(f"\nParticipants ({len(self.participants)}):")
        for guid, participant in self.participants.items():
            ros2_marker = " [ROS2]" if participant.is_ros2() else ""
            print(f"  {guid[:16]}...{ros2_marker}")
            print(f"    Vendor: {participant.vendor.name}")
            print(f"    IP: {participant.src_ip or 'unknown'}")
            print(f"    Packets: {participant.packet_count}")
            duration = (participant.last_seen - participant.first_seen).total_seconds()
            print(f"    Active: {duration:.1f}s")
            
        if self.ros2_mode:
            ros2_nodes = self.detect_ros2_nodes()
            print(f"\nROS2 Nodes Detected: {len(ros2_nodes)}")
            for node in ros2_nodes:
                print(f"  - {node[:24]}...")
                
        # Reliability analysis
        if self.stats.data_packets > 0:
            reliability = self.get_reliability_ratio("")
            print(f"\nReliability Ratio: {reliability:.2f}")
            if reliability > 1.0:
                print("  -> High reliability (many HB/AN per DATA)")
            elif reliability > 0.1:
                print("  -> Normal reliability")
            else:
                print("  -> Best-effort dominant")
                
        print("\n" + "=" * 60)


def analyze_pcap_file(filepath: str, ros2_mode: bool = False) -> None:
    """
    Analyze a pcap file for DDS/RTPS traffic.
    
    Args:
        filepath: Path to pcap file
        ros2_mode: Enable ROS2-specific analysis
    """
    print(f"Analyzing: {filepath}")
    
    analyzer = DdsAnalyzer(ros2_mode=ros2_mode)
    
    try:
        reader = wadjet.PcapReader(filepath)
        
        for packet in reader:
            analyzer.process_packet(packet)
            
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading pcap: {e}")
        sys.exit(1)
        
    analyzer.print_summary()


def live_capture(interface: str, ros2_mode: bool = False, duration: int = 30) -> None:
    """
    Capture and analyze live DDS/RTPS traffic.
    
    Args:
        interface: Network interface name
        ros2_mode: Enable ROS2-specific analysis
        duration: Capture duration in seconds
    """
    print(f"Capturing on {interface} for {duration}s...")
    print("Press Ctrl+C to stop early")
    
    analyzer = DdsAnalyzer(ros2_mode=ros2_mode)
    
    # BPF filter for DDS port range
    dds_filter = "udp and portrange 7400-7500"
    
    try:
        capture = wadjet.Capture(interface, filter=dds_filter)
        capture.set_timeout(1000)  # 1 second timeout
        
        import time
        start_time = time.time()
        
        while time.time() - start_time < duration:
            try:
                packet = capture.next_packet()
                if packet:
                    rtps = analyzer.process_packet(packet)
                    if rtps and analyzer.stats.rtps_packets % 100 == 0:
                        print(f"\rProcessed {analyzer.stats.rtps_packets} RTPS packets...", 
                              end="", flush=True)
            except wadjet.TimeoutError:
                continue
                
    except KeyboardInterrupt:
        print("\nCapture stopped by user")
    except PermissionError:
        print(f"Error: Permission denied. Run with sudo or as root.")
        sys.exit(1)
    except Exception as e:
        print(f"Error during capture: {e}")
        sys.exit(1)
        
    print()  # New line after progress
    analyzer.print_summary()


def monitor_mode(interface: str, ros2_mode: bool = False) -> None:
    """
    Continuous monitoring mode with real-time updates.
    
    Args:
        interface: Network interface name
        ros2_mode: Enable ROS2-specific analysis
    """
    print(f"Monitoring DDS traffic on {interface}")
    print("Press Ctrl+C to stop")
    print()
    
    analyzer = DdsAnalyzer(ros2_mode=ros2_mode)
    dds_filter = "udp and portrange 7400-7500"
    
    try:
        capture = wadjet.Capture(interface, filter=dds_filter)
        capture.set_timeout(100)  # 100ms for responsive display
        
        last_print = datetime.now()
        print_interval = 2.0  # Print stats every 2 seconds
        
        while True:
            try:
                packet = capture.next_packet()
                if packet:
                    rtps = analyzer.process_packet(packet)
                    
                    # Real-time event printing
                    if rtps:
                        now = datetime.now()
                        if rtps.is_discovery:
                            print(f"[{now.strftime('%H:%M:%S.%f')[:-3]}] "
                                  f"DISCOVERY from {rtps.vendor.name} "
                                  f"({rtps.guid_prefix[:12]}...)")
                                  
                        # Check for new participants
                        if rtps.guid_prefix not in analyzer.participants:
                            print(f"[{now.strftime('%H:%M:%S.%f')[:-3]}] "
                                  f"NEW PARTICIPANT: {rtps.vendor.name} "
                                  f"({rtps.guid_prefix[:12]}...)")
                                  
            except wadjet.TimeoutError:
                pass
                
            # Periodic stats
            now = datetime.now()
            if (now - last_print).total_seconds() > print_interval:
                active = len(analyzer.participants)
                rate = analyzer.stats.rtps_packets / max(1, (now - last_print).total_seconds())
                print(f"--- {active} participants, "
                      f"{analyzer.stats.rtps_packets} packets, "
                      f"{rate:.1f} pkt/s ---")
                last_print = now
                
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped")
        analyzer.print_summary()
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="𓆓 Wadjet-Link DDS/RTPS Traffic Analyzer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s analyze capture.pcap          Analyze a pcap file
  %(prog)s capture eth0                  Capture live traffic for 30s
  %(prog)s capture eth0 --duration 60    Capture for 60 seconds
  %(prog)s monitor eth0                  Continuous monitoring
  %(prog)s analyze trace.pcap --ros2     ROS2-specific analysis
        """
    )
    
    subparsers = parser.add_subparsers(dest="command", help="Command to run")
    
    # Analyze subcommand
    analyze_parser = subparsers.add_parser("analyze", help="Analyze pcap file")
    analyze_parser.add_argument("file", help="Path to pcap file")
    analyze_parser.add_argument("--ros2", action="store_true",
                                help="Enable ROS2-specific analysis")
    
    # Capture subcommand
    capture_parser = subparsers.add_parser("capture", help="Live capture")
    capture_parser.add_argument("interface", help="Network interface")
    capture_parser.add_argument("--duration", "-d", type=int, default=30,
                                help="Capture duration in seconds (default: 30)")
    capture_parser.add_argument("--ros2", action="store_true",
                                help="Enable ROS2-specific analysis")
    
    # Monitor subcommand
    monitor_parser = subparsers.add_parser("monitor", help="Continuous monitoring")
    monitor_parser.add_argument("interface", help="Network interface")
    monitor_parser.add_argument("--ros2", action="store_true",
                                help="Enable ROS2-specific analysis")
    
    args = parser.parse_args()
    
    if args.command == "analyze":
        analyze_pcap_file(args.file, ros2_mode=args.ros2)
    elif args.command == "capture":
        live_capture(args.interface, ros2_mode=args.ros2, duration=args.duration)
    elif args.command == "monitor":
        monitor_mode(args.interface, ros2_mode=args.ros2)
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()

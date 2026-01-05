#!/usr/bin/env python3
"""
Diagnostic Analysis Example using Wadjet-Link Python Bindings

This example demonstrates using the DiagnosticSessionManager to analyze
automotive diagnostic traffic from Python.

Usage:
    python diagnostic_analysis.py capture.pcap
    python diagnostic_analysis.py --ecu 0x1234 capture.pcap
"""

import sys
import argparse
from datetime import datetime
from typing import Dict, List, Optional
from dataclasses import dataclass, field

# Import wadjet bindings
try:
    import wadjet
    from wadjet import (
        DiagnosticSessionManager,
        DiagnosticEvent,
        DiagnosticTiming,
        RequestCorrelator,
        SessionType,
    )
except ImportError:
    print("Error: wadjet Python bindings not found.")
    print("Build with: cmake -DWADJET_BUILD_PYTHON_BINDINGS=ON ..")
    sys.exit(1)


@dataclass
class ECUSummary:
    """Summary statistics for an ECU."""
    address: int
    requests: int = 0
    responses: int = 0
    negative_responses: int = 0
    session_changes: int = 0
    security_unlocks: int = 0
    dtcs_read: int = 0
    current_session: SessionType = SessionType.DefaultSession
    security_level: int = 0
    events: List[str] = field(default_factory=list)


def event_name(event: DiagnosticEvent) -> str:
    """Convert DiagnosticEvent to human-readable string."""
    names = {
        DiagnosticEvent.SessionStarted: "SessionStarted",
        DiagnosticEvent.SessionEnded: "SessionEnded",
        DiagnosticEvent.SessionChanged: "SessionChanged",
        DiagnosticEvent.SessionTimeout: "SessionTimeout",
        DiagnosticEvent.SecurityUnlocked: "SecurityUnlocked",
        DiagnosticEvent.SecurityLocked: "SecurityLocked",
        DiagnosticEvent.SecurityAccessFailed: "SecurityAccessFailed",
        DiagnosticEvent.RequestSent: "RequestSent",
        DiagnosticEvent.ResponseReceived: "ResponseReceived",
        DiagnosticEvent.NegativeResponse: "NegativeResponse",
        DiagnosticEvent.ResponsePending: "ResponsePending",
        DiagnosticEvent.P2Timeout: "P2Timeout",
        DiagnosticEvent.DTCRead: "DTCRead",
        DiagnosticEvent.DTCCleared: "DTCCleared",
        DiagnosticEvent.FlashStarted: "FlashStarted",
        DiagnosticEvent.FlashCompleted: "FlashCompleted",
        DiagnosticEvent.FlashFailed: "FlashFailed",
    }
    return names.get(event, f"Unknown({event})")


class DiagnosticAnalyzer:
    """Analyzer for automotive diagnostic traffic."""
    
    def __init__(self, target_ecu: Optional[int] = None, verbose: bool = False):
        self.target_ecu = target_ecu
        self.verbose = verbose
        self.ecu_summaries: Dict[int, ECUSummary] = {}
        
        # Configure diagnostic manager
        timing = DiagnosticTiming.defaults()
        self.manager = DiagnosticSessionManager(timing)
        
        # Register event callback
        self.manager.on_event(self._handle_event)
        
        # Track timing violations
        self.p2_violations = 0
        self.p2_star_violations = 0
    
    def _get_ecu_summary(self, ecu_address: int) -> ECUSummary:
        """Get or create ECU summary."""
        if ecu_address not in self.ecu_summaries:
            self.ecu_summaries[ecu_address] = ECUSummary(address=ecu_address)
        return self.ecu_summaries[ecu_address]
    
    def _handle_event(self, event: DiagnosticEvent, state, pair) -> None:
        """Handle diagnostic events from the session manager."""
        ecu_address = state.ecu_address
        
        # Filter by target ECU if specified
        if self.target_ecu is not None and ecu_address != self.target_ecu:
            return
        
        summary = self._get_ecu_summary(ecu_address)
        
        # Update summary based on event
        if event == DiagnosticEvent.RequestSent:
            summary.requests += 1
        elif event == DiagnosticEvent.ResponseReceived:
            summary.responses += 1
        elif event == DiagnosticEvent.NegativeResponse:
            summary.negative_responses += 1
        elif event == DiagnosticEvent.SessionChanged:
            summary.session_changes += 1
            summary.current_session = state.session_type
        elif event == DiagnosticEvent.SecurityUnlocked:
            summary.security_unlocks += 1
            summary.security_level = state.security_level
        elif event == DiagnosticEvent.DTCRead:
            summary.dtcs_read += 1
        elif event == DiagnosticEvent.P2Timeout:
            self.p2_violations += 1
        
        # Log event
        event_str = f"ECU 0x{ecu_address:04X} | {event_name(event)}"
        if pair and pair.response_time:
            event_str += f" | {pair.response_time}ms"
        summary.events.append(event_str)
        
        if self.verbose:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] {event_str}")
    
    def process_pcap(self, pcap_path: str) -> None:
        """Process a PCAP file for diagnostic traffic."""
        print(f"Processing: {pcap_path}")
        
        # Note: In real implementation, use wadjet PCAP reader
        # For now, show placeholder
        print("PCAP processing not fully implemented in this example.")
        print("Use the C++ wadjet replay functionality to process PCAP files.")
    
    def print_summary(self) -> None:
        """Print analysis summary."""
        print("\n" + "=" * 60)
        print("       DIAGNOSTIC ANALYSIS SUMMARY")
        print("=" * 60)
        
        if not self.ecu_summaries:
            print("\nNo diagnostic traffic analyzed.")
            return
        
        # Correlator statistics
        stats = self.manager.correlator.statistics
        print(f"\nRequest Correlation:")
        print(f"  Requests recorded:  {stats.requests_recorded}")
        print(f"  Responses matched:  {stats.responses_matched}")
        print(f"  Match rate:         {stats.match_rate * 100:.1f}%")
        
        print(f"\nTiming Violations:")
        print(f"  P2 violations:      {self.p2_violations}")
        print(f"  P2* violations:     {self.p2_star_violations}")
        
        # Per-ECU summary
        print(f"\nECU Summary ({len(self.ecu_summaries)} ECUs tracked):")
        print("-" * 60)
        
        for ecu_addr in sorted(self.ecu_summaries.keys()):
            summary = self.ecu_summaries[ecu_addr]
            print(f"\n  ECU 0x{summary.address:04X}:")
            print(f"    Requests:         {summary.requests}")
            print(f"    Responses:        {summary.responses}")
            print(f"    Negative:         {summary.negative_responses}")
            print(f"    Session changes:  {summary.session_changes}")
            print(f"    Security unlocks: {summary.security_unlocks}")
            print(f"    DTCs read:        {summary.dtcs_read}")
            print(f"    Current session:  {summary.current_session}")
            print(f"    Security level:   {summary.security_level}")
        
        print("\n" + "=" * 60)


def main():
    parser = argparse.ArgumentParser(
        description="Analyze automotive diagnostic traffic using Wadjet-Link"
    )
    parser.add_argument(
        "source",
        help="PCAP file or network interface"
    )
    parser.add_argument(
        "-e", "--ecu",
        type=lambda x: int(x, 0),
        help="Target ECU address (hex)"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Verbose output (show all events)"
    )
    
    args = parser.parse_args()
    
    print("𓆓 Wadjet-Link Diagnostic Analyzer (Python)")
    print("=" * 45)
    print(f"Source: {args.source}")
    if args.ecu:
        print(f"Target ECU: 0x{args.ecu:04X}")
    print()
    
    analyzer = DiagnosticAnalyzer(
        target_ecu=args.ecu,
        verbose=args.verbose
    )
    
    if args.source.endswith((".pcap", ".pcapng")):
        analyzer.process_pcap(args.source)
    else:
        print(f"Live capture on interface '{args.source}' not implemented.")
        print("Use C++ wadjet library for live capture.")
    
    analyzer.print_summary()


if __name__ == "__main__":
    main()

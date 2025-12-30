"""
High-level capture API with Pythonic context managers and iterators.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

from __future__ import annotations

import time
from contextlib import contextmanager
from typing import Iterator, Optional, Callable, List

from ._wadjet import (
    CaptureSession,
    CaptureSessionOptions,
    Packet,
    PcapReader,
)


class LiveCapture:
    """
    High-level live packet capture with context manager support.
    
    Example:
        >>> with LiveCapture("eth0", filter="udp port 30490") as cap:
        ...     for packet in cap.stream(timeout_ms=1000):
        ...         if packet.has_someip():
        ...             print(packet.someip().service_id)
    
    Or capture a fixed number of packets:
        >>> with LiveCapture("eth0") as cap:
        ...     packets = cap.collect(count=100, timeout_ms=5000)
    """
    
    def __init__(
        self,
        interface: str,
        filter: Optional[str] = None,
        promiscuous: bool = True,
        snaplen: int = 65535,
        timeout_ms: int = 100,
        buffer_size: int = 2 * 1024 * 1024,
    ):
        """
        Initialize a live capture session.
        
        Args:
            interface: Network interface name (e.g., "eth0")
            filter: BPF filter expression (e.g., "udp port 30490")
            promiscuous: Enable promiscuous mode
            snaplen: Maximum bytes to capture per packet
            timeout_ms: Poll timeout in milliseconds
            buffer_size: Ring buffer size
        """
        self.interface = interface
        self.filter_expr = filter
        
        self._options = CaptureSessionOptions()
        self._options.promiscuous = promiscuous
        self._options.snaplen = snaplen
        self._options.timeout_ms = timeout_ms
        self._options.buffer_size = buffer_size
        
        self._session: Optional[CaptureSession] = None
    
    def __enter__(self) -> "LiveCapture":
        """Start capture session."""
        self._session = CaptureSession.create(self.interface, self._options)
        if self.filter_expr:
            self._session.set_filter(self.filter_expr)
        self._session.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Stop capture session."""
        if self._session:
            self._session.stop()
            self._session = None
        return False
    
    def stream(
        self,
        timeout_ms: int = 1000,
        max_packets: Optional[int] = None,
    ) -> Iterator[Packet]:
        """
        Iterate over captured packets.
        
        Args:
            timeout_ms: Timeout for each packet capture
            max_packets: Maximum number of packets to yield (None = unlimited)
        
        Yields:
            Captured Packet objects
        """
        if not self._session:
            raise RuntimeError("Capture session not started. Use 'with' statement.")
        
        count = 0
        while max_packets is None or count < max_packets:
            packet = self._session.next_packet(timeout_ms)
            if packet:
                yield packet
                count += 1
    
    def collect(
        self,
        count: Optional[int] = None,
        timeout_ms: int = 5000,
        duration_ms: Optional[int] = None,
    ) -> List[Packet]:
        """
        Collect packets into a list.
        
        Args:
            count: Number of packets to collect (None = until timeout/duration)
            timeout_ms: Timeout per packet in milliseconds
            duration_ms: Total collection duration (None = use count)
        
        Returns:
            List of captured Packet objects
        """
        if not self._session:
            raise RuntimeError("Capture session not started. Use 'with' statement.")
        
        packets = []
        start_time = time.monotonic()
        
        while True:
            # Check duration limit
            if duration_ms is not None:
                elapsed_ms = (time.monotonic() - start_time) * 1000
                if elapsed_ms >= duration_ms:
                    break
            
            # Check count limit
            if count is not None and len(packets) >= count:
                break
            
            # Try to get next packet
            packet = self._session.next_packet(timeout_ms)
            if packet:
                packets.append(packet)
            elif count is None and duration_ms is None:
                # No packet and no limits set - stop after one timeout
                break
        
        return packets
    
    def wait_for(
        self,
        predicate: Callable[[Packet], bool],
        timeout_ms: int = 5000,
    ) -> Optional[Packet]:
        """
        Wait for a packet matching a predicate.
        
        Args:
            predicate: Function that returns True for matching packets
            timeout_ms: Maximum time to wait
        
        Returns:
            First matching packet, or None if timeout
        """
        if not self._session:
            raise RuntimeError("Capture session not started. Use 'with' statement.")
        
        start_time = time.monotonic()
        
        while True:
            elapsed_ms = (time.monotonic() - start_time) * 1000
            if elapsed_ms >= timeout_ms:
                return None
            
            remaining_ms = int(timeout_ms - elapsed_ms)
            packet = self._session.next_packet(min(remaining_ms, 100))
            
            if packet and predicate(packet):
                return packet
    
    @property
    def stats(self):
        """Get capture statistics."""
        if not self._session:
            raise RuntimeError("Capture session not started.")
        return self._session.stats()


class ReplayCapture:
    """
    Replay packets from a PCAP file with timing control.
    
    Example:
        >>> with ReplayCapture("capture.pcap") as replay:
        ...     for packet in replay:
        ...         process(packet)
    
    Or with timing replay:
        >>> with ReplayCapture("capture.pcap", realtime=True) as replay:
        ...     for packet in replay:
        ...         # Packets are yielded at original timing
        ...         process(packet)
    """
    
    def __init__(
        self,
        path: str,
        realtime: bool = False,
        speed: float = 1.0,
    ):
        """
        Initialize a replay capture.
        
        Args:
            path: Path to PCAP file
            realtime: If True, replay with original timing
            speed: Speed multiplier (only used with realtime=True)
        """
        self.path = path
        self.realtime = realtime
        self.speed = speed
        self._reader: Optional[PcapReader] = None
        self._last_timestamp = None
    
    def __enter__(self) -> "ReplayCapture":
        """Open PCAP file."""
        self._reader = PcapReader.open(self.path)
        self._last_timestamp = None
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Close PCAP file."""
        self._reader = None
        self._last_timestamp = None
        return False
    
    def __iter__(self) -> Iterator[Packet]:
        """Iterate over packets in the PCAP file."""
        if not self._reader:
            raise RuntimeError("PCAP file not opened. Use 'with' statement.")
        
        for packet in self._reader:
            if self.realtime and self._last_timestamp is not None:
                # Calculate delay based on timestamp difference
                current_ts = packet.timestamp.total_nanoseconds()
                delay_ns = current_ts - self._last_timestamp
                if delay_ns > 0:
                    delay_s = (delay_ns / 1e9) / self.speed
                    time.sleep(delay_s)
            
            self._last_timestamp = packet.timestamp.total_nanoseconds()
            yield packet
    
    def all(self) -> List[Packet]:
        """Read all packets into a list."""
        if not self._reader:
            raise RuntimeError("PCAP file not opened. Use 'with' statement.")
        return list(self._reader)


@contextmanager
def capture(interface: str, **kwargs):
    """
    Convenience function for creating a LiveCapture context.
    
    Example:
        >>> with capture("eth0", filter="udp") as cap:
        ...     packets = cap.collect(count=10)
    """
    cap = LiveCapture(interface, **kwargs)
    with cap:
        yield cap

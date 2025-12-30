"""
Testing utilities and pytest fixtures for Wadjet.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

from __future__ import annotations

from typing import Optional, List, Callable, Any, Union
from dataclasses import dataclass
import json

from ._wadjet import (
    Packet,
    PacketView,
    DecodeResult,
    decode_packet,
    SomeIpHeader,
    SomeIpSdHeader,
    DoIPHeader,
    MessageType as SomeIpMessageType,
    ReturnCode as SomeIpReturnCode,
    PayloadType as DoIPPayloadType,
)

from .protocols import decode, parse, ProtocolStack


# ---------------------------------------------------------------------------
# Assertion Helpers
# ---------------------------------------------------------------------------

class AssertionError(Exception):
    """Custom assertion error with detailed messages."""
    pass


def assert_someip(
    packet: Union[Packet, PacketView, bytes],
    service_id: Optional[int] = None,
    method_id: Optional[int] = None,
    client_id: Optional[int] = None,
    session_id: Optional[int] = None,
    message_type: Optional[SomeIpMessageType] = None,
    return_code: Optional[SomeIpReturnCode] = None,
) -> SomeIpHeader:
    """
    Assert that a packet contains a valid SOME/IP header with expected values.
    
    Args:
        packet: The packet to check
        service_id: Expected service ID (None = any)
        method_id: Expected method ID (None = any)
        client_id: Expected client ID (None = any)
        session_id: Expected session ID (None = any)
        message_type: Expected message type (None = any)
        return_code: Expected return code (None = any)
    
    Returns:
        The SOME/IP header for further inspection
    
    Raises:
        AssertionError: If packet doesn't contain SOME/IP or values don't match
    
    Example:
        >>> header = assert_someip(packet, service_id=0x1234, method_id=0x0001)
    """
    result = decode(packet)
    if not result.has_someip():
        raise AssertionError("Packet does not contain SOME/IP header")
    
    header = result.someip()
    
    if service_id is not None and header.service_id != service_id:
        raise AssertionError(
            f"SOME/IP service_id mismatch: expected 0x{service_id:04x}, "
            f"got 0x{header.service_id:04x}"
        )
    
    if method_id is not None and header.method_id != method_id:
        raise AssertionError(
            f"SOME/IP method_id mismatch: expected 0x{method_id:04x}, "
            f"got 0x{header.method_id:04x}"
        )
    
    if client_id is not None and header.client_id != client_id:
        raise AssertionError(
            f"SOME/IP client_id mismatch: expected 0x{client_id:04x}, "
            f"got 0x{header.client_id:04x}"
        )
    
    if session_id is not None and header.session_id != session_id:
        raise AssertionError(
            f"SOME/IP session_id mismatch: expected 0x{session_id:04x}, "
            f"got 0x{header.session_id:04x}"
        )
    
    if message_type is not None and header.message_type != message_type:
        raise AssertionError(
            f"SOME/IP message_type mismatch: expected {message_type}, "
            f"got {header.message_type}"
        )
    
    if return_code is not None and header.return_code != return_code:
        raise AssertionError(
            f"SOME/IP return_code mismatch: expected {return_code}, "
            f"got {header.return_code}"
        )
    
    return header


def assert_someip_sd(
    packet: Union[Packet, PacketView, bytes],
    reboot_flag: Optional[bool] = None,
    unicast_flag: Optional[bool] = None,
) -> SomeIpSdHeader:
    """
    Assert that a packet contains a valid SOME/IP-SD header.
    
    Args:
        packet: The packet to check
        reboot_flag: Expected reboot flag (None = any)
        unicast_flag: Expected unicast flag (None = any)
    
    Returns:
        The SOME/IP-SD header for further inspection
    
    Raises:
        AssertionError: If packet doesn't contain SOME/IP-SD or values don't match
    """
    result = decode(packet)
    if not result.has_someip_sd():
        raise AssertionError("Packet does not contain SOME/IP-SD header")
    
    header = result.someip_sd()
    
    if reboot_flag is not None and header.reboot_flag != reboot_flag:
        raise AssertionError(
            f"SOME/IP-SD reboot_flag mismatch: expected {reboot_flag}, "
            f"got {header.reboot_flag}"
        )
    
    if unicast_flag is not None and header.unicast_flag != unicast_flag:
        raise AssertionError(
            f"SOME/IP-SD unicast_flag mismatch: expected {unicast_flag}, "
            f"got {header.unicast_flag}"
        )
    
    return header


def assert_doip(
    packet: Union[Packet, PacketView, bytes],
    payload_type: Optional[DoIPPayloadType] = None,
    source_address: Optional[int] = None,
    target_address: Optional[int] = None,
) -> DoIPHeader:
    """
    Assert that a packet contains a valid DoIP header.
    
    Args:
        packet: The packet to check
        payload_type: Expected payload type (None = any)
        source_address: Expected source address (None = any)
        target_address: Expected target address (None = any)
    
    Returns:
        The DoIP header for further inspection
    
    Raises:
        AssertionError: If packet doesn't contain DoIP or values don't match
    """
    result = decode(packet)
    if not result.has_doip():
        raise AssertionError("Packet does not contain DoIP header")
    
    header = result.doip()
    
    if payload_type is not None and header.payload_type != payload_type:
        raise AssertionError(
            f"DoIP payload_type mismatch: expected {payload_type}, "
            f"got {header.payload_type}"
        )
    
    if source_address is not None and header.source_address != source_address:
        raise AssertionError(
            f"DoIP source_address mismatch: expected 0x{source_address:04x}, "
            f"got 0x{header.source_address:04x}"
        )
    
    if target_address is not None and header.target_address != target_address:
        raise AssertionError(
            f"DoIP target_address mismatch: expected 0x{target_address:04x}, "
            f"got 0x{header.target_address:04x}"
        )
    
    return header


def assert_payload(
    packet: Union[Packet, PacketView, bytes],
    expected: bytes,
    offset: int = 0,
) -> bytes:
    """
    Assert that the packet payload matches expected bytes.
    
    Args:
        packet: The packet to check
        expected: Expected payload bytes
        offset: Offset into payload to start comparison
    
    Returns:
        The full payload bytes
    
    Raises:
        AssertionError: If payload doesn't match
    """
    result = decode(packet)
    payload = bytes(result.payload())
    
    actual = payload[offset:offset + len(expected)]
    if actual != expected:
        raise AssertionError(
            f"Payload mismatch at offset {offset}:\n"
            f"  expected: {expected.hex()}\n"
            f"  actual:   {actual.hex()}"
        )
    
    return payload


def assert_payload_contains(
    packet: Union[Packet, PacketView, bytes],
    pattern: bytes,
) -> int:
    """
    Assert that the packet payload contains a byte pattern.
    
    Args:
        packet: The packet to check
        pattern: Byte pattern to search for
    
    Returns:
        Offset where pattern was found
    
    Raises:
        AssertionError: If pattern not found
    """
    result = decode(packet)
    payload = bytes(result.payload())
    
    offset = payload.find(pattern)
    if offset < 0:
        raise AssertionError(
            f"Pattern {pattern.hex()} not found in payload ({len(payload)} bytes)"
        )
    
    return offset


# ---------------------------------------------------------------------------
# Packet Matchers (for use with capture filters and assertions)
# ---------------------------------------------------------------------------

@dataclass
class PacketMatcher:
    """A composable packet matcher for filtering and assertions."""
    
    name: str
    predicate: Callable[[Union[Packet, PacketView]], bool]
    
    def __call__(self, packet: Union[Packet, PacketView]) -> bool:
        return self.predicate(packet)
    
    def __and__(self, other: "PacketMatcher") -> "PacketMatcher":
        return PacketMatcher(
            f"({self.name} AND {other.name})",
            lambda p: self.predicate(p) and other.predicate(p)
        )
    
    def __or__(self, other: "PacketMatcher") -> "PacketMatcher":
        return PacketMatcher(
            f"({self.name} OR {other.name})",
            lambda p: self.predicate(p) or other.predicate(p)
        )
    
    def __invert__(self) -> "PacketMatcher":
        return PacketMatcher(
            f"NOT {self.name}",
            lambda p: not self.predicate(p)
        )


def has_someip(
    service_id: Optional[int] = None,
    method_id: Optional[int] = None,
) -> PacketMatcher:
    """Create a matcher for SOME/IP packets."""
    def _match(packet):
        result = decode(packet)
        if not result.has_someip():
            return False
        if service_id is not None or method_id is not None:
            hdr = result.someip()
            if service_id is not None and hdr.service_id != service_id:
                return False
            if method_id is not None and hdr.method_id != method_id:
                return False
        return True
    
    name = "SOME/IP"
    if service_id is not None:
        name += f"(service=0x{service_id:04x})"
    if method_id is not None:
        name += f"(method=0x{method_id:04x})"
    
    return PacketMatcher(name, _match)


def has_doip(
    payload_type: Optional[int] = None,
) -> PacketMatcher:
    """Create a matcher for DoIP packets."""
    def _match(packet):
        result = decode(packet)
        if not result.has_doip():
            return False
        if payload_type is not None:
            hdr = result.doip()
            if hdr.payload_type_value() != payload_type:
                return False
        return True
    
    name = "DoIP"
    if payload_type is not None:
        name += f"(type=0x{payload_type:04x})"
    
    return PacketMatcher(name, _match)


def has_ip(
    src: Optional[str] = None,
    dst: Optional[str] = None,
) -> PacketMatcher:
    """Create a matcher for IP packets."""
    def _match(packet):
        result = decode(packet)
        if not result.has_ipv4():
            return False
        if src is not None or dst is not None:
            hdr = result.ipv4()
            if src is not None and hdr.src_ip_str() != src:
                return False
            if dst is not None and hdr.dst_ip_str() != dst:
                return False
        return True
    
    name = "IP"
    if src:
        name += f"(src={src})"
    if dst:
        name += f"(dst={dst})"
    
    return PacketMatcher(name, _match)


def has_port(
    src: Optional[int] = None,
    dst: Optional[int] = None,
) -> PacketMatcher:
    """Create a matcher for UDP/TCP packets by port."""
    def _match(packet):
        result = decode(packet)
        if result.has_udp():
            hdr = result.udp()
        elif result.has_tcp():
            hdr = result.tcp()
        else:
            return False
        
        if src is not None and hdr.src_port != src:
            return False
        if dst is not None and hdr.dst_port != dst:
            return False
        return True
    
    name = "Port"
    if src:
        name += f"(src={src})"
    if dst:
        name += f"(dst={dst})"
    
    return PacketMatcher(name, _match)


# ---------------------------------------------------------------------------
# Pytest Fixtures (if pytest is available)
# ---------------------------------------------------------------------------

try:
    import pytest
    
    @pytest.fixture
    def pcap_reader():
        """Fixture that provides a function to read PCAP files."""
        from .pcap import read_pcap
        return read_pcap
    
    @pytest.fixture
    def packet_decoder():
        """Fixture that provides the decode function."""
        return decode
    
    @pytest.fixture
    def packet_parser():
        """Fixture that provides the parse function."""
        return parse
    
    @pytest.fixture
    def someip_matcher():
        """Fixture that provides the has_someip matcher constructor."""
        return has_someip
    
    @pytest.fixture
    def doip_matcher():
        """Fixture that provides the has_doip matcher constructor."""
        return has_doip
    
except ImportError:
    # pytest not installed
    pass


# ---------------------------------------------------------------------------
# Test Result Collection
# ---------------------------------------------------------------------------

@dataclass
class TestResult:
    """Result of a packet test."""
    passed: bool
    packet_index: int
    message: str
    details: Optional[dict] = None


class PacketTestRunner:
    """
    Run tests against a sequence of packets.
    
    Example:
        >>> runner = PacketTestRunner(packets)
        >>> runner.expect(has_someip(service_id=0x1234))
        >>> runner.expect(has_doip())
        >>> results = runner.run()
    """
    
    def __init__(self, packets: List[Union[Packet, PacketView]]):
        self.packets = packets
        self.expectations: List[tuple] = []
    
    def expect(
        self,
        matcher: PacketMatcher,
        at_index: Optional[int] = None,
        within: Optional[int] = None,
    ) -> "PacketTestRunner":
        """
        Add an expectation.
        
        Args:
            matcher: PacketMatcher to apply
            at_index: Exact packet index to check (None = any)
            within: Maximum packets to search (None = all)
        
        Returns:
            Self for chaining
        """
        self.expectations.append((matcher, at_index, within))
        return self
    
    def run(self) -> List[TestResult]:
        """Run all expectations and return results."""
        results = []
        current_index = 0
        
        for matcher, at_index, within in self.expectations:
            if at_index is not None:
                # Check specific index
                if at_index >= len(self.packets):
                    results.append(TestResult(
                        passed=False,
                        packet_index=at_index,
                        message=f"Packet index {at_index} out of range",
                    ))
                elif matcher(self.packets[at_index]):
                    results.append(TestResult(
                        passed=True,
                        packet_index=at_index,
                        message=f"Packet at index {at_index} matches {matcher.name}",
                    ))
                else:
                    results.append(TestResult(
                        passed=False,
                        packet_index=at_index,
                        message=f"Packet at index {at_index} does not match {matcher.name}",
                    ))
            else:
                # Search from current position
                search_end = len(self.packets)
                if within is not None:
                    search_end = min(current_index + within, len(self.packets))
                
                found = False
                for i in range(current_index, search_end):
                    if matcher(self.packets[i]):
                        results.append(TestResult(
                            passed=True,
                            packet_index=i,
                            message=f"Found {matcher.name} at index {i}",
                        ))
                        current_index = i + 1
                        found = True
                        break
                
                if not found:
                    results.append(TestResult(
                        passed=False,
                        packet_index=-1,
                        message=f"No packet matching {matcher.name} found",
                    ))
        
        return results
    
    def assert_all(self) -> None:
        """Run all expectations and raise on first failure."""
        results = self.run()
        for result in results:
            if not result.passed:
                raise AssertionError(result.message)

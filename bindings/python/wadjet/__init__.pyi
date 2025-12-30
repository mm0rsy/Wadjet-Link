"""
Type stubs for Wadjet Python package.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

from typing import (
    Callable,
    Iterator,
    List,
    Optional,
    Tuple,
    Union,
    Dict,
    Any,
)

# Re-export from native module
from ._wadjet import (
    __version__ as __version__,
    version_string as version_string,
    version_major as version_major,
    version_minor as version_minor,
    version_patch as version_patch,
    Timestamp as Timestamp,
    Packet as Packet,
    PacketView as PacketView,
    CaptureSession as CaptureSession,
    CaptureSessionOptions as CaptureSessionOptions,
    CaptureStats as CaptureStats,
    TimestampSource as TimestampSource,
    available_devices as available_devices,
    PcapReader as PcapReader,
    PcapWriter as PcapWriter,
    EthernetHeader as EthernetHeader,
    VlanTag as VlanTag,
    IPv4Header as IPv4Header,
    UdpHeader as UdpHeader,
    TcpHeader as TcpHeader,
    SomeIpHeader as SomeIpHeader,
    SomeIpSdHeader as SomeIpSdHeader,
    DoIPHeader as DoIPHeader,
    MessageType as SomeIpMessageType,
    ReturnCode as SomeIpReturnCode,
    PayloadType as DoIPPayloadType,
    DecodeResult as DecodeResult,
    ProtocolDispatcher as ProtocolDispatcher,
    decode_packet as decode_packet,
    bytes_to_hex as bytes_to_hex,
    hex_to_bytes as hex_to_bytes,
)

# ---------------------------------------------------------------------------
# capture.py
# ---------------------------------------------------------------------------

class LiveCapture:
    """High-level live packet capture with context manager support."""
    
    interface: str
    filter_expr: Optional[str]
    
    def __init__(
        self,
        interface: str,
        filter: Optional[str] = None,
        promiscuous: bool = True,
        snaplen: int = 65535,
        timeout_ms: int = 100,
        buffer_size: int = 2097152,
    ) -> None: ...
    
    def __enter__(self) -> LiveCapture: ...
    def __exit__(
        self,
        exc_type: Optional[type],
        exc_val: Optional[BaseException],
        exc_tb: Optional[Any],
    ) -> bool: ...
    
    def stream(
        self,
        timeout_ms: int = 1000,
        max_packets: Optional[int] = None,
    ) -> Iterator[Packet]: ...
    
    def collect(
        self,
        count: Optional[int] = None,
        timeout_ms: int = 5000,
        duration_ms: Optional[int] = None,
    ) -> List[Packet]: ...
    
    def wait_for(
        self,
        predicate: Callable[[Packet], bool],
        timeout_ms: int = 5000,
    ) -> Optional[Packet]: ...
    
    @property
    def stats(self) -> CaptureStats: ...


class ReplayCapture:
    """Replay packets from a PCAP file with timing control."""
    
    path: str
    realtime: bool
    speed: float
    
    def __init__(
        self,
        path: str,
        realtime: bool = False,
        speed: float = 1.0,
    ) -> None: ...
    
    def __enter__(self) -> ReplayCapture: ...
    def __exit__(
        self,
        exc_type: Optional[type],
        exc_val: Optional[BaseException],
        exc_tb: Optional[Any],
    ) -> bool: ...
    
    def __iter__(self) -> Iterator[Packet]: ...
    def all(self) -> List[Packet]: ...


def capture(interface: str, **kwargs: Any) -> LiveCapture:
    """Convenience function for creating a LiveCapture context."""
    ...

# ---------------------------------------------------------------------------
# protocols.py
# ---------------------------------------------------------------------------

class ProtocolStack:
    """Structured representation of decoded protocol layers."""
    
    ethernet: Optional[EthernetHeader]
    vlan: Optional[VlanTag]
    ipv4: Optional[IPv4Header]
    udp: Optional[UdpHeader]
    tcp: Optional[TcpHeader]
    someip: Optional[SomeIpHeader]
    someip_sd: Optional[SomeIpSdHeader]
    doip: Optional[DoIPHeader]
    payload: bytes
    
    @classmethod
    def from_result(cls, result: DecodeResult) -> ProtocolStack: ...
    def layers(self) -> List[str]: ...
    def to_dict(self) -> Dict[str, Any]: ...
    def __str__(self) -> str: ...


def decode(packet: Union[Packet, PacketView, bytes]) -> DecodeResult:
    """Decode a packet's protocol stack."""
    ...

def parse(packet: Union[Packet, PacketView, bytes]) -> ProtocolStack:
    """Parse a packet into a ProtocolStack structure."""
    ...

def is_someip(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains SOME/IP."""
    ...

def is_someip_sd(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains SOME/IP-SD."""
    ...

def is_doip(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains DoIP."""
    ...

def get_someip(packet: Union[Packet, PacketView, bytes]) -> Optional[SomeIpHeader]:
    """Extract SOME/IP header from packet, or None."""
    ...

def get_someip_sd(packet: Union[Packet, PacketView, bytes]) -> Optional[SomeIpSdHeader]:
    """Extract SOME/IP-SD header from packet, or None."""
    ...

def get_doip(packet: Union[Packet, PacketView, bytes]) -> Optional[DoIPHeader]:
    """Extract DoIP header from packet, or None."""
    ...

def someip_filter(
    service_id: Optional[int] = None,
    method_id: Optional[int] = None,
    client_id: Optional[int] = None,
) -> Callable[[Union[Packet, PacketView]], bool]:
    """Create a SOME/IP packet filter function."""
    ...

def doip_filter(
    payload_type: Optional[int] = None,
    source_address: Optional[int] = None,
) -> Callable[[Union[Packet, PacketView]], bool]:
    """Create a DoIP packet filter function."""
    ...

# ---------------------------------------------------------------------------
# pcap.py
# ---------------------------------------------------------------------------

def read_pcap(path: Union[str, Any]) -> List[Packet]:
    """Read all packets from a PCAP file."""
    ...

def iter_pcap(path: Union[str, Any]) -> Iterator[Packet]:
    """Iterate over packets in a PCAP file."""
    ...

def write_pcap(
    path: Union[str, Any],
    packets: List[Packet],
    link_type: int = 1,
) -> None:
    """Write packets to a PCAP file."""
    ...

def filter_pcap(
    input_path: Union[str, Any],
    output_path: Union[str, Any],
    predicate: Callable[[Packet], bool],
) -> int:
    """Filter packets from one PCAP file to another."""
    ...

def merge_pcaps(
    input_paths: List[Union[str, Any]],
    output_path: Union[str, Any],
    sort_by_time: bool = True,
) -> int:
    """Merge multiple PCAP files into one."""
    ...

def split_pcap(
    input_path: Union[str, Any],
    output_prefix: str,
    packets_per_file: int,
) -> List[str]:
    """Split a PCAP file into multiple smaller files."""
    ...

# ---------------------------------------------------------------------------
# testing.py
# ---------------------------------------------------------------------------

class AssertionError(Exception):
    """Custom assertion error with detailed messages."""
    ...


def assert_someip(
    packet: Union[Packet, PacketView, bytes],
    service_id: Optional[int] = None,
    method_id: Optional[int] = None,
    client_id: Optional[int] = None,
    session_id: Optional[int] = None,
    message_type: Optional[SomeIpMessageType] = None,
    return_code: Optional[SomeIpReturnCode] = None,
) -> SomeIpHeader:
    """Assert that a packet contains a valid SOME/IP header."""
    ...

def assert_someip_sd(
    packet: Union[Packet, PacketView, bytes],
    reboot_flag: Optional[bool] = None,
    unicast_flag: Optional[bool] = None,
) -> SomeIpSdHeader:
    """Assert that a packet contains a valid SOME/IP-SD header."""
    ...

def assert_doip(
    packet: Union[Packet, PacketView, bytes],
    payload_type: Optional[DoIPPayloadType] = None,
    source_address: Optional[int] = None,
    target_address: Optional[int] = None,
) -> DoIPHeader:
    """Assert that a packet contains a valid DoIP header."""
    ...

def assert_payload(
    packet: Union[Packet, PacketView, bytes],
    expected: bytes,
    offset: int = 0,
) -> bytes:
    """Assert that the packet payload matches expected bytes."""
    ...

def assert_payload_contains(
    packet: Union[Packet, PacketView, bytes],
    pattern: bytes,
) -> int:
    """Assert that the packet payload contains a byte pattern."""
    ...


class PacketMatcher:
    """A composable packet matcher for filtering and assertions."""
    
    name: str
    
    def __call__(self, packet: Union[Packet, PacketView]) -> bool: ...
    def __and__(self, other: PacketMatcher) -> PacketMatcher: ...
    def __or__(self, other: PacketMatcher) -> PacketMatcher: ...
    def __invert__(self) -> PacketMatcher: ...


def has_someip(
    service_id: Optional[int] = None,
    method_id: Optional[int] = None,
) -> PacketMatcher:
    """Create a matcher for SOME/IP packets."""
    ...

def has_doip(
    payload_type: Optional[int] = None,
) -> PacketMatcher:
    """Create a matcher for DoIP packets."""
    ...

def has_ip(
    src: Optional[str] = None,
    dst: Optional[str] = None,
) -> PacketMatcher:
    """Create a matcher for IP packets."""
    ...

def has_port(
    src: Optional[int] = None,
    dst: Optional[int] = None,
) -> PacketMatcher:
    """Create a matcher for UDP/TCP packets by port."""
    ...


class TestResult:
    """Result of a packet test."""
    
    passed: bool
    packet_index: int
    message: str
    details: Optional[Dict[str, Any]]


class PacketTestRunner:
    """Run tests against a sequence of packets."""
    
    packets: List[Union[Packet, PacketView]]
    
    def __init__(self, packets: List[Union[Packet, PacketView]]) -> None: ...
    
    def expect(
        self,
        matcher: PacketMatcher,
        at_index: Optional[int] = None,
        within: Optional[int] = None,
    ) -> PacketTestRunner: ...
    
    def run(self) -> List[TestResult]: ...
    def assert_all(self) -> None: ...

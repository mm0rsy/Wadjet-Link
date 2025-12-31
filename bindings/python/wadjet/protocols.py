"""
Protocol decoding utilities with a Pythonic interface.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

from __future__ import annotations

from typing import Optional, Dict, Any, List, Union
from dataclasses import dataclass
from enum import IntEnum

from ._wadjet import (
    Packet,
    PacketView,
    DecodeResult,
    ProtocolDispatcher,
    decode_packet,
    EthernetHeader,
    VlanTag,
    IPv4Header,
    UdpHeader,
    TcpHeader,
    SomeIpHeader,
    SomeIpSdHeader,
    DoIPHeader,
    MessageType as SomeIpMessageType,
    ReturnCode as SomeIpReturnCode,
    PayloadType as DoIPPayloadType,
)

# Try to import DDS types (may not be available in all builds)
try:
    from ._wadjet import (
        RtpsHeader,
        RtpsVendorId,
        RtpsSubmessageKind,
        GuidPrefix,
        EntityId,
    )
    _DDS_AVAILABLE = True
except ImportError:
    _DDS_AVAILABLE = False
    RtpsHeader = None
    RtpsVendorId = None
    RtpsSubmessageKind = None
    GuidPrefix = None
    EntityId = None


# DDS Vendor ID enum for Python users
class DdsVendor(IntEnum):
    """DDS/RTPS Vendor identifiers."""
    UNKNOWN = 0x0000
    FAST_DDS = 0x0101
    RTI_CONNEXT = 0x0102
    OPEN_SPLICE = 0x0103
    CYCLONE_DDS = 0x0105
    OPEN_DDS = 0x0106


# DDS Submessage Kind enum
class DdsSubmessageKind(IntEnum):
    """RTPS Submessage types."""
    PAD = 0x01
    ACKNACK = 0x06
    HEARTBEAT = 0x07
    GAP = 0x08
    INFO_TS = 0x09
    INFO_SRC = 0x0C
    INFO_REPLY_IP4 = 0x0D
    INFO_DST = 0x0E
    INFO_REPLY = 0x0F
    NACK_FRAG = 0x12
    HEARTBEAT_FRAG = 0x13
    DATA = 0x15
    DATA_FRAG = 0x16


def decode(packet: Union[Packet, PacketView, bytes]) -> DecodeResult:
    """
    Decode a packet's protocol stack.
    
    Args:
        packet: A Packet, PacketView, or raw bytes
    
    Returns:
        DecodeResult with protocol headers
    
    Example:
        >>> result = decode(packet)
        >>> if result.has_someip():
        ...     print(f"Service: 0x{result.someip().service_id:04x}")
    """
    if isinstance(packet, bytes):
        # Create a temporary packet from raw bytes
        pkt = Packet(packet)
        return decode_packet(pkt.view())
    elif isinstance(packet, Packet):
        return decode_packet(packet.view())
    else:
        return decode_packet(packet)


@dataclass
class RtpsInfo:
    """Decoded RTPS/DDS header information."""
    version_major: int
    version_minor: int
    vendor_id: int
    vendor_name: str
    guid_prefix: str
    submessage_count: int
    submessage_kinds: List[str]
    
    @property
    def vendor(self) -> DdsVendor:
        """Get vendor as enum."""
        try:
            return DdsVendor(self.vendor_id)
        except ValueError:
            return DdsVendor.UNKNOWN
    
    def has_data(self) -> bool:
        """Check if message contains DATA submessage."""
        return "DATA" in self.submessage_kinds
    
    def has_heartbeat(self) -> bool:
        """Check if message contains HEARTBEAT submessage."""
        return "HEARTBEAT" in self.submessage_kinds
    
    def has_acknack(self) -> bool:
        """Check if message contains ACKNACK submessage."""
        return "ACKNACK" in self.submessage_kinds
    
    def is_discovery(self) -> bool:
        """Check if this appears to be discovery traffic."""
        # Discovery uses INFO_TS + DATA with specific entity IDs
        return self.has_data() and "INFO_TS" in self.submessage_kinds


@dataclass
class ProtocolStack:
    """
    A structured representation of decoded protocol layers.
    
    Provides attribute access to protocol headers.
    """
    ethernet: Optional[EthernetHeader] = None
    vlan: Optional[VlanTag] = None
    ipv4: Optional[IPv4Header] = None
    udp: Optional[UdpHeader] = None
    tcp: Optional[TcpHeader] = None
    someip: Optional[SomeIpHeader] = None
    someip_sd: Optional[SomeIpSdHeader] = None
    doip: Optional[DoIPHeader] = None
    rtps: Optional[RtpsInfo] = None
    payload: bytes = b""
    
    @classmethod
    def from_result(cls, result: DecodeResult) -> "ProtocolStack":
        """Create a ProtocolStack from a DecodeResult."""
        rtps_info = None
        if _DDS_AVAILABLE and hasattr(result, 'has_rtps') and result.has_rtps():
            rtps = result.rtps()
            submsg_kinds = [str(s.kind) for s in rtps.submessages] if hasattr(rtps, 'submessages') else []
            rtps_info = RtpsInfo(
                version_major=rtps.version.major if hasattr(rtps, 'version') else 2,
                version_minor=rtps.version.minor if hasattr(rtps, 'version') else 4,
                vendor_id=rtps.vendor_id.value if hasattr(rtps, 'vendor_id') else 0,
                vendor_name=rtps.vendor_id.name if hasattr(rtps, 'vendor_id') else "Unknown",
                guid_prefix=rtps.guid_prefix.to_string() if hasattr(rtps, 'guid_prefix') else "",
                submessage_count=len(rtps.submessages) if hasattr(rtps, 'submessages') else 0,
                submessage_kinds=submsg_kinds,
            )
        
        return cls(
            ethernet=result.ethernet() if result.has_ethernet() else None,
            vlan=result.vlan() if result.has_vlan() else None,
            ipv4=result.ipv4() if result.has_ipv4() else None,
            udp=result.udp() if result.has_udp() else None,
            tcp=result.tcp() if result.has_tcp() else None,
            someip=result.someip() if result.has_someip() else None,
            someip_sd=result.someip_sd() if result.has_someip_sd() else None,
            doip=result.doip() if result.has_doip() else None,
            rtps=rtps_info,
            payload=bytes(result.payload()),
        )
    
    def layers(self) -> List[str]:
        """Get list of present protocol layers."""
        present = []
        if self.ethernet:
            present.append("ethernet")
        if self.vlan:
            present.append("vlan")
        if self.ipv4:
            present.append("ipv4")
        if self.udp:
            present.append("udp")
        if self.tcp:
            present.append("tcp")
        if self.someip:
            present.append("someip")
        if self.someip_sd:
            present.append("someip_sd")
        if self.doip:
            present.append("doip")
        if self.rtps:
            present.append("rtps")
        return present
    
    def __str__(self) -> str:
        layers = self.layers()
        return f"ProtocolStack({' / '.join(layers)})"
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to a dictionary for JSON serialization."""
        result = {}
        
        if self.ethernet:
            result["ethernet"] = {
                "src_mac": self.ethernet.src_mac_str(),
                "dst_mac": self.ethernet.dst_mac_str(),
                "ethertype": self.ethernet.ethertype,
            }
        
        if self.vlan:
            result["vlan"] = {
                "vlan_id": self.vlan.vlan_id,
                "priority": self.vlan.priority,
            }
        
        if self.ipv4:
            result["ipv4"] = {
                "src_ip": self.ipv4.src_ip_str(),
                "dst_ip": self.ipv4.dst_ip_str(),
                "protocol": self.ipv4.protocol,
                "ttl": self.ipv4.ttl,
            }
        
        if self.udp:
            result["udp"] = {
                "src_port": self.udp.src_port,
                "dst_port": self.udp.dst_port,
            }
        
        if self.tcp:
            result["tcp"] = {
                "src_port": self.tcp.src_port,
                "dst_port": self.tcp.dst_port,
                "seq": self.tcp.sequence_number,
                "ack": self.tcp.acknowledgment_number,
                "flags": self.tcp.flags,
            }
        
        if self.someip:
            result["someip"] = {
                "service_id": self.someip.service_id,
                "method_id": self.someip.method_id,
                "client_id": self.someip.client_id,
                "session_id": self.someip.session_id,
                "message_type": str(self.someip.message_type),
                "return_code": str(self.someip.return_code),
            }
        
        if self.someip_sd:
            result["someip_sd"] = {
                "reboot_flag": self.someip_sd.reboot_flag,
                "unicast_flag": self.someip_sd.unicast_flag,
                "entries_length": self.someip_sd.entries_length,
            }
        
        if self.doip:
            result["doip"] = {
                "protocol_version": self.doip.protocol_version,
                "payload_type": str(self.doip.payload_type),
                "payload_length": self.doip.payload_length,
            }
        
        if self.rtps:
            result["rtps"] = {
                "version": f"{self.rtps.version_major}.{self.rtps.version_minor}",
                "vendor_id": self.rtps.vendor_id,
                "vendor_name": self.rtps.vendor_name,
                "guid_prefix": self.rtps.guid_prefix,
                "submessage_count": self.rtps.submessage_count,
                "submessage_kinds": self.rtps.submessage_kinds,
            }
        
        if self.payload:
            result["payload_length"] = len(self.payload)
        
        return result


def parse(packet: Union[Packet, PacketView, bytes]) -> ProtocolStack:
    """
    Parse a packet into a ProtocolStack structure.
    
    This provides a more Pythonic interface than decode().
    
    Args:
        packet: A Packet, PacketView, or raw bytes
    
    Returns:
        ProtocolStack with all decoded headers
    
    Example:
        >>> stack = parse(packet)
        >>> if stack.someip:
        ...     print(f"Service: 0x{stack.someip.service_id:04x}")
    """
    result = decode(packet)
    return ProtocolStack.from_result(result)


def is_someip(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains SOME/IP."""
    return decode(packet).has_someip()


def is_someip_sd(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains SOME/IP-SD."""
    return decode(packet).has_someip_sd()


def is_doip(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains DoIP."""
    return decode(packet).has_doip()


def get_someip(packet: Union[Packet, PacketView, bytes]) -> Optional[SomeIpHeader]:
    """Extract SOME/IP header from packet, or None."""
    result = decode(packet)
    return result.someip() if result.has_someip() else None


def get_someip_sd(packet: Union[Packet, PacketView, bytes]) -> Optional[SomeIpSdHeader]:
    """Extract SOME/IP-SD header from packet, or None."""
    result = decode(packet)
    return result.someip_sd() if result.has_someip_sd() else None


def get_doip(packet: Union[Packet, PacketView, bytes]) -> Optional[DoIPHeader]:
    """Extract DoIP header from packet, or None."""
    result = decode(packet)
    return result.doip() if result.has_doip() else None


# Protocol-specific filters for use with capture
def someip_filter(
    service_id: Optional[int] = None,
    method_id: Optional[int] = None,
    client_id: Optional[int] = None,
):
    """
    Create a SOME/IP packet filter function.
    
    Args:
        service_id: Filter by service ID (None = any)
        method_id: Filter by method ID (None = any)
        client_id: Filter by client ID (None = any)
    
    Returns:
        A predicate function for use with capture filters
    
    Example:
        >>> with capture("eth0") as cap:
        ...     pkt = cap.wait_for(someip_filter(service_id=0x1234))
    """
    def _filter(packet):
        result = decode(packet)
        if not result.has_someip():
            return False
        
        hdr = result.someip()
        if service_id is not None and hdr.service_id != service_id:
            return False
        if method_id is not None and hdr.method_id != method_id:
            return False
        if client_id is not None and hdr.client_id != client_id:
            return False
        
        return True
    
    return _filter


def doip_filter(
    payload_type: Optional[int] = None,
    source_address: Optional[int] = None,
):
    """
    Create a DoIP packet filter function.
    
    Args:
        payload_type: Filter by payload type (None = any)
        source_address: Filter by source address (None = any)
    
    Returns:
        A predicate function for use with capture filters
    """
    def _filter(packet):
        result = decode(packet)
        if not result.has_doip():
            return False
        
        hdr = result.doip()
        if payload_type is not None and hdr.payload_type_value() != payload_type:
            return False
        if source_address is not None and hdr.source_address != source_address:
            return False
        
        return True
    
    return _filter

# =============================================================================
# DDS/RTPS Helper Functions
# =============================================================================

def is_rtps(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Check if packet contains RTPS/DDS traffic."""
    if not _DDS_AVAILABLE:
        return False
    result = decode(packet)
    return hasattr(result, 'has_rtps') and result.has_rtps()


def is_dds(packet: Union[Packet, PacketView, bytes]) -> bool:
    """Alias for is_rtps - check if packet contains DDS traffic."""
    return is_rtps(packet)


def get_rtps(packet: Union[Packet, PacketView, bytes]) -> Optional[RtpsInfo]:
    """
    Extract RTPS/DDS information from packet.
    
    Args:
        packet: A Packet, PacketView, or raw bytes
        
    Returns:
        RtpsInfo with decoded RTPS header info, or None if not RTPS
        
    Example:
        >>> rtps = get_rtps(packet)
        >>> if rtps:
        ...     print(f"Vendor: {rtps.vendor_name}")
        ...     print(f"Submessages: {rtps.submessage_count}")
    """
    stack = parse(packet)
    return stack.rtps


def rtps_filter(
    vendor: Optional[DdsVendor] = None,
    has_data: bool = False,
    has_heartbeat: bool = False,
    has_acknack: bool = False,
    is_discovery: bool = False,
):
    """
    Create an RTPS/DDS packet filter function.
    
    Args:
        vendor: Filter by DDS vendor (None = any)
        has_data: Require DATA submessage
        has_heartbeat: Require HEARTBEAT submessage
        has_acknack: Require ACKNACK submessage
        is_discovery: Require discovery traffic (SPDP/SEDP)
    
    Returns:
        A predicate function for use with capture filters
    
    Example:
        >>> # Filter for FastDDS data packets
        >>> with capture("eth0") as cap:
        ...     pkt = cap.wait_for(rtps_filter(vendor=DdsVendor.FAST_DDS, has_data=True))
    """
    def _filter(packet):
        rtps = get_rtps(packet)
        if rtps is None:
            return False
        
        if vendor is not None and rtps.vendor != vendor:
            return False
        if has_data and not rtps.has_data():
            return False
        if has_heartbeat and not rtps.has_heartbeat():
            return False
        if has_acknack and not rtps.has_acknack():
            return False
        if is_discovery and not rtps.is_discovery():
            return False
        
        return True
    
    return _filter


def dds_filter(
    vendor: Optional[DdsVendor] = None,
    has_data: bool = False,
    is_discovery: bool = False,
):
    """
    Alias for rtps_filter - create a DDS packet filter.
    
    See rtps_filter for full documentation.
    """
    return rtps_filter(vendor=vendor, has_data=has_data, is_discovery=is_discovery)
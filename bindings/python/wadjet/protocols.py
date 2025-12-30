"""
Protocol decoding utilities with a Pythonic interface.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

from __future__ import annotations

from typing import Optional, Dict, Any, List, Union
from dataclasses import dataclass

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
    payload: bytes = b""
    
    @classmethod
    def from_result(cls, result: DecodeResult) -> "ProtocolStack":
        """Create a ProtocolStack from a DecodeResult."""
        return cls(
            ethernet=result.ethernet() if result.has_ethernet() else None,
            vlan=result.vlan() if result.has_vlan() else None,
            ipv4=result.ipv4() if result.has_ipv4() else None,
            udp=result.udp() if result.has_udp() else None,
            tcp=result.tcp() if result.has_tcp() else None,
            someip=result.someip() if result.has_someip() else None,
            someip_sd=result.someip_sd() if result.has_someip_sd() else None,
            doip=result.doip() if result.has_doip() else None,
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

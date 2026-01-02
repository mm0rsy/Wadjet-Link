"""
𓆓 Wadjet-Link Python Bindings

Automotive Ethernet validation framework for Python.

Example usage:

    import wadjet

    # Live packet capture
    with wadjet.LiveCapture("eth0", filter="udp port 30490") as cap:
        for packet in cap.stream(timeout_ms=1000):
            result = wadjet.decode(packet)
            if result.has_someip():
                header = result.someip()
                print(f"Service: {header.service_id:#06x}")

    # PCAP file analysis
    for packet in wadjet.read_pcap("capture.pcap"):
        stack = wadjet.parse(packet)
        if stack.someip:
            print(stack.someip)

    # Protocol decoding
    result = wadjet.decode(packet_data)
    if result.has_doip():
        print(result.doip())

"""

__version__ = "0.1.0"
__author__ = "Wadjet-Link Contributors"

# Import native extension (may not be available before build)
_NATIVE_AVAILABLE = False
try:
    from ._wadjet import (
        # Version info
        version_string,
        version_major,
        version_minor,
        version_patch,
        
        # Core types
        Timestamp,
        
        # Packet types
        Packet,
        PacketView,
        
        # Capture
        CaptureSession,
        CaptureSessionOptions,
        CaptureStats,
        TimestampSource,
        list_interfaces as available_devices,
        
        # PCAP I/O
        PcapReader,
        PcapWriter,
        
        # Protocol types
        EthernetHeader,
        VlanTag,
        IPv4Header,
        UdpHeader,
        TcpHeader,
        SomeIpHeader,
        SomeIpSdHeader,
        DoIPHeader,
        
        # Protocol enums
        MessageType as SomeIpMessageType,
        ReturnCode as SomeIpReturnCode,
        PayloadType as DoIPPayloadType,
        
        # Decoder
        ProtocolDispatcher,
        DecodeResult,
        decode_packet,
        
        # Utilities
        bytes_to_hex,
        hex_to_bytes,
    )
    _NATIVE_AVAILABLE = True
except ImportError:
    # Native module not built yet - provide placeholders
    version_string = "0.1.0-dev"
    version_major = 0
    version_minor = 1
    version_patch = 0

# Import high-level Python wrappers only if native module is available
if _NATIVE_AVAILABLE:
    from .capture import LiveCapture, ReplayCapture, capture
    from .protocols import (
        decode,
        parse,
        ProtocolStack,
        is_someip,
        is_someip_sd,
        is_doip,
        get_someip,
        get_someip_sd,
        get_doip,
        someip_filter,
        doip_filter,
    )
    from .pcap import (
        read_pcap,
        iter_pcap,
        write_pcap,
        filter_pcap,
        merge_pcaps,
        split_pcap,
    )
    from .testing import (
        assert_someip,
        assert_someip_sd,
        assert_doip,
        assert_payload,
        assert_payload_contains,
        PacketMatcher,
        has_someip,
        has_doip,
        has_ip,
        has_port,
        PacketTestRunner,
    )


__all__ = [
    # Version
    "__version__",
    "version_string",
    "version_major",
    "version_minor",
    "version_patch",
    
    # Core types
    "Timestamp",
    "Packet",
    "PacketView",
    
    # Capture
    "CaptureSession",
    "CaptureSessionOptions",
    "CaptureStats",
    "TimestampSource",
    "available_devices",
    "LiveCapture",
    "ReplayCapture",
    "capture",
    
    # PCAP
    "PcapReader",
    "PcapWriter",
    "read_pcap",
    "iter_pcap",
    "write_pcap",
    "filter_pcap",
    "merge_pcaps",
    "split_pcap",
    
    # Protocols
    "EthernetHeader",
    "VlanTag",
    "IPv4Header",
    "UdpHeader",
    "TcpHeader",
    "SomeIpHeader",
    "SomeIpSdHeader",
    "DoIPHeader",
    "SomeIpMessageType",
    "SomeIpReturnCode",
    "DoIPPayloadType",
    
    # Decoding
    "ProtocolDispatcher",
    "DecodeResult",
    "decode_packet",
    "decode",
    "parse",
    "ProtocolStack",
    "is_someip",
    "is_someip_sd",
    "is_doip",
    "get_someip",
    "get_someip_sd",
    "get_doip",
    "someip_filter",
    "doip_filter",
    
    # Testing
    "assert_someip",
    "assert_someip_sd",
    "assert_doip",
    "assert_payload",
    "assert_payload_contains",
    "PacketMatcher",
    "has_someip",
    "has_doip",
    "has_ip",
    "has_port",
    "PacketTestRunner",
    
    # Utilities
    "bytes_to_hex",
    "hex_to_bytes",
]

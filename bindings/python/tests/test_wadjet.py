"""
Python tests for Wadjet bindings.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

import pytest
import os
import tempfile
from pathlib import Path


# Skip all tests if native module not available
pytestmark = pytest.mark.skipif(
    not os.environ.get("WADJET_TEST_BINDINGS"),
    reason="Native module not built. Set WADJET_TEST_BINDINGS=1 to run."
)


class TestPacket:
    """Tests for Packet and PacketView classes."""
    
    def test_packet_from_bytes(self):
        """Test creating a Packet from raw bytes."""
        import wadjet
        
        data = bytes([0x01, 0x02, 0x03, 0x04])
        packet = wadjet.Packet(data)
        
        assert len(packet) == 4
        assert bytes(packet) == data
    
    def test_packet_view_slicing(self):
        """Test PacketView slicing."""
        import wadjet
        
        data = bytes(range(100))
        packet = wadjet.Packet(data)
        view = packet.view()
        
        # Test slicing
        subview = view[10:20]
        assert len(subview) == 10
    
    def test_packet_hex_conversion(self):
        """Test hex string conversion."""
        import wadjet
        
        hex_str = wadjet.bytes_to_hex(bytes([0xDE, 0xAD, 0xBE, 0xEF]))
        assert hex_str.lower() == "deadbeef"
        
        data = wadjet.hex_to_bytes("DEADBEEF")
        assert data == bytes([0xDE, 0xAD, 0xBE, 0xEF])


class TestTimestamp:
    """Tests for Timestamp class."""
    
    def test_timestamp_creation(self):
        """Test Timestamp creation."""
        import wadjet
        
        ts = wadjet.Timestamp(1234567890, 123456789)
        assert ts.seconds == 1234567890
        assert ts.nanoseconds == 123456789
    
    def test_timestamp_now(self):
        """Test Timestamp.now()."""
        import wadjet
        
        ts = wadjet.Timestamp.now()
        assert ts.seconds > 0
    
    def test_timestamp_comparison(self):
        """Test Timestamp comparison."""
        import wadjet
        
        ts1 = wadjet.Timestamp(100, 0)
        ts2 = wadjet.Timestamp(200, 0)
        
        assert ts1 < ts2
        assert ts2 > ts1
        assert ts1 != ts2


class TestProtocolDecode:
    """Tests for protocol decoding."""
    
    def test_decode_ethernet(self):
        """Test decoding Ethernet header."""
        import wadjet
        
        # Ethernet frame: dst_mac, src_mac, ethertype (IPv4 = 0x0800)
        ethernet_frame = bytes([
            0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  # dst MAC
            0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,  # src MAC
            0x08, 0x00,                          # EtherType (IPv4)
            # ... payload would follow
        ])
        
        packet = wadjet.Packet(ethernet_frame)
        result = wadjet.decode(packet)
        
        assert result.has_ethernet()
        eth = result.ethernet()
        assert eth.ethertype == 0x0800
    
    def test_decode_ipv4_udp_someip(self):
        """Test decoding full SOME/IP packet."""
        import wadjet
        
        # Build a complete SOME/IP over UDP/IPv4/Ethernet packet
        # This is a minimal valid SOME/IP packet
        someip_packet = build_someip_packet(
            service_id=0x1234,
            method_id=0x0001,
            client_id=0x5678,
            session_id=0x0001,
        )
        
        packet = wadjet.Packet(someip_packet)
        result = wadjet.decode(packet)
        
        assert result.has_someip()
        someip = result.someip()
        assert someip.service_id == 0x1234
        assert someip.method_id == 0x0001
        assert someip.client_id == 0x5678
    
    def test_parse_protocol_stack(self):
        """Test high-level parse function."""
        import wadjet
        
        someip_packet = build_someip_packet(service_id=0xABCD)
        stack = wadjet.parse(someip_packet)
        
        assert stack.ethernet is not None
        assert stack.ipv4 is not None
        assert stack.udp is not None
        assert stack.someip is not None
        assert stack.someip.service_id == 0xABCD


class TestPcapIO:
    """Tests for PCAP file I/O."""
    
    def test_write_and_read_pcap(self):
        """Test writing and reading a PCAP file."""
        import wadjet
        
        # Create some test packets
        packets = [
            wadjet.Packet(bytes(range(i, i + 64)))
            for i in range(5)
        ]
        
        with tempfile.NamedTemporaryFile(suffix=".pcap", delete=False) as f:
            pcap_path = f.name
        
        try:
            # Write packets
            wadjet.write_pcap(pcap_path, packets)
            
            # Read packets back
            read_packets = wadjet.read_pcap(pcap_path)
            
            assert len(read_packets) == 5
        finally:
            os.unlink(pcap_path)
    
    def test_iter_pcap(self):
        """Test iterating over PCAP file."""
        import wadjet
        
        packets = [
            wadjet.Packet(bytes([i] * 64))
            for i in range(10)
        ]
        
        with tempfile.NamedTemporaryFile(suffix=".pcap", delete=False) as f:
            pcap_path = f.name
        
        try:
            wadjet.write_pcap(pcap_path, packets)
            
            count = 0
            for packet in wadjet.iter_pcap(pcap_path):
                count += 1
            
            assert count == 10
        finally:
            os.unlink(pcap_path)
    
    def test_filter_pcap(self):
        """Test filtering PCAP file."""
        import wadjet
        
        # Create packets with different sizes
        packets = [
            wadjet.Packet(bytes([0] * (50 + i * 10)))
            for i in range(10)
        ]
        
        with tempfile.NamedTemporaryFile(suffix=".pcap", delete=False) as f:
            input_path = f.name
        with tempfile.NamedTemporaryFile(suffix=".pcap", delete=False) as f:
            output_path = f.name
        
        try:
            wadjet.write_pcap(input_path, packets)
            
            # Filter packets with length > 100
            count = wadjet.filter_pcap(
                input_path,
                output_path,
                lambda p: len(p) > 100
            )
            
            assert count > 0
            assert count < 10
        finally:
            os.unlink(input_path)
            os.unlink(output_path)


class TestMatchers:
    """Tests for packet matchers."""
    
    def test_has_someip_matcher(self):
        """Test has_someip matcher."""
        import wadjet
        
        matcher = wadjet.has_someip(service_id=0x1234)
        
        # Test with matching packet
        packet = build_someip_packet(service_id=0x1234)
        assert matcher(wadjet.Packet(packet))
        
        # Test with non-matching packet
        packet = build_someip_packet(service_id=0x5678)
        assert not matcher(wadjet.Packet(packet))
    
    def test_matcher_composition(self):
        """Test composing matchers with AND/OR."""
        import wadjet
        
        m1 = wadjet.has_someip(service_id=0x1234)
        m2 = wadjet.has_port(dst=30490)
        
        # AND composition
        combined = m1 & m2
        
        packet = build_someip_packet(service_id=0x1234, dst_port=30490)
        assert combined(wadjet.Packet(packet))


class TestAssertions:
    """Tests for assertion helpers."""
    
    def test_assert_someip_success(self):
        """Test assert_someip with matching packet."""
        import wadjet
        
        packet = build_someip_packet(
            service_id=0x1234,
            method_id=0x0001,
        )
        
        header = wadjet.assert_someip(
            wadjet.Packet(packet),
            service_id=0x1234,
            method_id=0x0001,
        )
        
        assert header.service_id == 0x1234
    
    def test_assert_someip_failure(self):
        """Test assert_someip with non-matching packet."""
        import wadjet
        
        packet = build_someip_packet(service_id=0x1234)
        
        with pytest.raises(wadjet.testing.AssertionError):
            wadjet.assert_someip(
                wadjet.Packet(packet),
                service_id=0x5678,  # Wrong service ID
            )


# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------

def build_someip_packet(
    service_id: int = 0x1234,
    method_id: int = 0x0001,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    src_port: int = 30490,
    dst_port: int = 30490,
    payload: bytes = b"",
) -> bytes:
    """Build a complete SOME/IP over UDP/IPv4/Ethernet packet."""
    
    # SOME/IP header (16 bytes)
    someip_length = 8 + len(payload)  # length field includes client/session/etc.
    someip = bytes([
        (service_id >> 8) & 0xFF, service_id & 0xFF,
        (method_id >> 8) & 0xFF, method_id & 0xFF,
        (someip_length >> 24) & 0xFF, (someip_length >> 16) & 0xFF,
        (someip_length >> 8) & 0xFF, someip_length & 0xFF,
        (client_id >> 8) & 0xFF, client_id & 0xFF,
        (session_id >> 8) & 0xFF, session_id & 0xFF,
        0x01,  # Protocol version
        0x01,  # Interface version
        0x00,  # Message type: REQUEST
        0x00,  # Return code: E_OK
    ]) + payload
    
    # UDP header (8 bytes)
    udp_length = 8 + len(someip)
    udp = bytes([
        (src_port >> 8) & 0xFF, src_port & 0xFF,
        (dst_port >> 8) & 0xFF, dst_port & 0xFF,
        (udp_length >> 8) & 0xFF, udp_length & 0xFF,
        0x00, 0x00,  # Checksum (0 = not computed)
    ]) + someip
    
    # IPv4 header (20 bytes, no options)
    ip_length = 20 + len(udp)
    ipv4 = bytes([
        0x45,  # Version 4, IHL 5 (20 bytes)
        0x00,  # DSCP/ECN
        (ip_length >> 8) & 0xFF, ip_length & 0xFF,
        0x00, 0x01,  # Identification
        0x00, 0x00,  # Flags/Fragment offset
        0x40,  # TTL = 64
        0x11,  # Protocol = UDP (17)
        0x00, 0x00,  # Checksum (would need calculation)
        192, 168, 1, 100,  # Source IP
        192, 168, 1, 200,  # Destination IP
    ]) + udp
    
    # Ethernet header (14 bytes)
    ethernet = bytes([
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  # Dst MAC
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,  # Src MAC
        0x08, 0x00,  # EtherType = IPv4
    ]) + ipv4
    
    return ethernet


def build_doip_packet(
    payload_type: int = 0x8001,
    source_address: int = 0x0E80,
    target_address: int = 0x1000,
    payload: bytes = b"",
) -> bytes:
    """Build a complete DoIP over TCP/IPv4/Ethernet packet."""
    
    # DoIP header (8 bytes) + addresses (4 bytes) + payload
    doip_payload_len = 4 + len(payload)  # SA + TA + payload
    doip = bytes([
        0x02,  # Protocol version
        0xFD,  # Inverse protocol version
        (payload_type >> 8) & 0xFF, payload_type & 0xFF,
        (doip_payload_len >> 24) & 0xFF, (doip_payload_len >> 16) & 0xFF,
        (doip_payload_len >> 8) & 0xFF, doip_payload_len & 0xFF,
        (source_address >> 8) & 0xFF, source_address & 0xFF,
        (target_address >> 8) & 0xFF, target_address & 0xFF,
    ]) + payload
    
    # TCP header (20 bytes, no options)
    tcp = bytes([
        0x34, 0xD8,  # Src port (13528 - DoIP default)
        0x34, 0xD8,  # Dst port
        0x00, 0x00, 0x00, 0x01,  # Sequence number
        0x00, 0x00, 0x00, 0x00,  # Ack number
        0x50,  # Data offset (5 * 4 = 20 bytes), no flags
        0x10,  # Flags: ACK
        0xFF, 0xFF,  # Window
        0x00, 0x00,  # Checksum
        0x00, 0x00,  # Urgent pointer
    ]) + doip
    
    # IPv4 header (20 bytes)
    ip_length = 20 + len(tcp)
    ipv4 = bytes([
        0x45, 0x00,
        (ip_length >> 8) & 0xFF, ip_length & 0xFF,
        0x00, 0x01, 0x00, 0x00,
        0x40, 0x06,  # TTL=64, Protocol=TCP
        0x00, 0x00,
        192, 168, 1, 100,
        192, 168, 1, 200,
    ]) + tcp
    
    # Ethernet header
    ethernet = bytes([
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
        0x08, 0x00,
    ]) + ipv4
    
    return ethernet


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

#!/usr/bin/env python3
"""
Example: Using Wadjet with pytest for protocol testing.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

Run with: pytest test_protocol.py -v
"""

import pytest
import wadjet


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def sample_someip_packet():
    """Create a sample SOME/IP packet for testing."""
    return build_someip_packet(
        service_id=0x1234,
        method_id=0x0001,
        client_id=0x5678,
        session_id=0x0001,
    )


@pytest.fixture
def sample_pcap(tmp_path):
    """Create a temporary PCAP file with test packets."""
    pcap_path = tmp_path / "test.pcap"
    
    packets = [
        wadjet.Packet(build_someip_packet(
            service_id=0x1000 + i,
            method_id=i,
        ))
        for i in range(10)
    ]
    
    wadjet.write_pcap(str(pcap_path), packets)
    return pcap_path


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestSomeIPDecoding:
    """Test SOME/IP protocol decoding."""
    
    def test_decode_service_id(self, sample_someip_packet):
        """Verify service ID is decoded correctly."""
        packet = wadjet.Packet(sample_someip_packet)
        
        # Use assertion helper
        header = wadjet.assert_someip(packet, service_id=0x1234)
        assert header is not None
    
    def test_decode_method_id(self, sample_someip_packet):
        """Verify method ID is decoded correctly."""
        packet = wadjet.Packet(sample_someip_packet)
        
        header = wadjet.assert_someip(packet, method_id=0x0001)
        assert header is not None
    
    def test_decode_client_session(self, sample_someip_packet):
        """Verify client and session IDs."""
        packet = wadjet.Packet(sample_someip_packet)
        
        header = wadjet.assert_someip(
            packet,
            client_id=0x5678,
            session_id=0x0001,
        )
        assert header is not None
    
    def test_wrong_service_id_fails(self, sample_someip_packet):
        """Verify assertion fails on wrong service ID."""
        packet = wadjet.Packet(sample_someip_packet)
        
        with pytest.raises(wadjet.testing.AssertionError) as exc_info:
            wadjet.assert_someip(packet, service_id=0xFFFF)
        
        assert "service_id mismatch" in str(exc_info.value)


class TestProtocolStack:
    """Test protocol stack parsing."""
    
    def test_parse_returns_all_layers(self, sample_someip_packet):
        """Verify parse() returns all protocol layers."""
        packet = wadjet.Packet(sample_someip_packet)
        stack = wadjet.parse(packet)
        
        # Check all layers are present
        assert stack.ethernet is not None
        assert stack.ipv4 is not None
        assert stack.udp is not None
        assert stack.someip is not None
        
        # Check no unexpected layers
        assert stack.tcp is None
        assert stack.doip is None
    
    def test_layer_list(self, sample_someip_packet):
        """Verify layers() returns correct list."""
        packet = wadjet.Packet(sample_someip_packet)
        stack = wadjet.parse(packet)
        
        layers = stack.layers()
        assert "ethernet" in layers
        assert "ipv4" in layers
        assert "udp" in layers
        assert "someip" in layers
    
    def test_to_dict(self, sample_someip_packet):
        """Verify to_dict() produces valid dict."""
        packet = wadjet.Packet(sample_someip_packet)
        stack = wadjet.parse(packet)
        
        d = stack.to_dict()
        
        assert "ethernet" in d
        assert "ipv4" in d
        assert "someip" in d
        assert d["someip"]["service_id"] == 0x1234


class TestPacketMatchers:
    """Test packet matcher functionality."""
    
    def test_has_someip_matcher(self, sample_someip_packet):
        """Test basic SOME/IP matcher."""
        packet = wadjet.Packet(sample_someip_packet)
        
        matcher = wadjet.has_someip()
        assert matcher(packet)
    
    def test_has_someip_with_filter(self, sample_someip_packet):
        """Test SOME/IP matcher with service filter."""
        packet = wadjet.Packet(sample_someip_packet)
        
        # Matching service
        matcher = wadjet.has_someip(service_id=0x1234)
        assert matcher(packet)
        
        # Non-matching service
        matcher = wadjet.has_someip(service_id=0x9999)
        assert not matcher(packet)
    
    def test_matcher_and_composition(self, sample_someip_packet):
        """Test AND composition of matchers."""
        packet = wadjet.Packet(sample_someip_packet)
        
        # Both conditions true
        m1 = wadjet.has_someip(service_id=0x1234)
        m2 = wadjet.has_port(dst=30490)
        combined = m1 & m2
        
        assert combined(packet)
    
    def test_matcher_or_composition(self, sample_someip_packet):
        """Test OR composition of matchers."""
        packet = wadjet.Packet(sample_someip_packet)
        
        # One condition true
        m1 = wadjet.has_someip(service_id=0x1234)
        m2 = wadjet.has_someip(service_id=0x9999)
        combined = m1 | m2
        
        assert combined(packet)
    
    def test_matcher_not(self, sample_someip_packet):
        """Test NOT inversion of matcher."""
        packet = wadjet.Packet(sample_someip_packet)
        
        matcher = ~wadjet.has_doip()
        assert matcher(packet)  # Packet doesn't have DoIP


class TestPcapAnalysis:
    """Test PCAP file analysis."""
    
    def test_read_all_packets(self, sample_pcap):
        """Verify reading all packets from PCAP."""
        packets = wadjet.read_pcap(str(sample_pcap))
        assert len(packets) == 10
    
    def test_iterate_packets(self, sample_pcap):
        """Verify iterating over PCAP."""
        count = 0
        for packet in wadjet.iter_pcap(str(sample_pcap)):
            count += 1
            assert wadjet.is_someip(packet)
        
        assert count == 10
    
    def test_filter_packets(self, sample_pcap, tmp_path):
        """Verify filtering PCAP file."""
        output_path = tmp_path / "filtered.pcap"
        
        # Filter for specific service
        count = wadjet.filter_pcap(
            str(sample_pcap),
            str(output_path),
            wadjet.has_someip(service_id=0x1005),
        )
        
        assert count == 1  # Only one packet with service 0x1005


class TestPacketTestRunner:
    """Test the PacketTestRunner utility."""
    
    def test_runner_basic(self, sample_pcap):
        """Test basic runner functionality."""
        packets = wadjet.read_pcap(str(sample_pcap))
        
        runner = wadjet.PacketTestRunner(packets)
        runner.expect(wadjet.has_someip())
        runner.expect(wadjet.has_someip(service_id=0x1005))
        
        results = runner.run()
        
        assert len(results) == 2
        assert all(r.passed for r in results)
    
    def test_runner_assert_all(self, sample_pcap):
        """Test assert_all raises on failure."""
        packets = wadjet.read_pcap(str(sample_pcap))
        
        runner = wadjet.PacketTestRunner(packets)
        runner.expect(wadjet.has_someip())
        runner.expect(wadjet.has_doip())  # No DoIP packets
        
        with pytest.raises(wadjet.testing.AssertionError):
            runner.assert_all()


# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------

def build_someip_packet(
    service_id: int = 0x1234,
    method_id: int = 0x0001,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    dst_port: int = 30490,
    payload: bytes = b"",
) -> bytes:
    """Build a SOME/IP packet for testing."""
    
    # SOME/IP header
    someip_length = 8 + len(payload)
    someip = bytes([
        (service_id >> 8) & 0xFF, service_id & 0xFF,
        (method_id >> 8) & 0xFF, method_id & 0xFF,
        (someip_length >> 24) & 0xFF, (someip_length >> 16) & 0xFF,
        (someip_length >> 8) & 0xFF, someip_length & 0xFF,
        (client_id >> 8) & 0xFF, client_id & 0xFF,
        (session_id >> 8) & 0xFF, session_id & 0xFF,
        0x01, 0x01, 0x00, 0x00,
    ]) + payload
    
    # UDP header
    udp_length = 8 + len(someip)
    udp = bytes([
        0x77, 0x0A,  # Src port 30474
        (dst_port >> 8) & 0xFF, dst_port & 0xFF,
        (udp_length >> 8) & 0xFF, udp_length & 0xFF,
        0x00, 0x00,
    ]) + someip
    
    # IPv4 header
    ip_length = 20 + len(udp)
    ipv4 = bytes([
        0x45, 0x00,
        (ip_length >> 8) & 0xFF, ip_length & 0xFF,
        0x00, 0x01, 0x00, 0x00,
        0x40, 0x11,
        0x00, 0x00,
        192, 168, 1, 100,
        192, 168, 1, 200,
    ]) + udp
    
    # Ethernet header
    ethernet = bytes([
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
        0x08, 0x00,
    ]) + ipv4
    
    return ethernet


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

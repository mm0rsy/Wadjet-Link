# 𓆓 Wadjet-Link Python Bindings

Python bindings for the Wadjet-Link automotive Ethernet validation framework.

## Features

- **Live Packet Capture** — Real-time network traffic capture with BPF filtering
- **Protocol Decoding** — SOME/IP, SOME/IP-SD, DoIP, and lower layers
- **PCAP I/O** — Read, write, filter, merge, and split PCAP files
- **Test Assertions** — pytest-friendly matchers and assertions
- **Zero-Copy** — Buffer protocol support for NumPy integration

## Installation

### From Source (CMake)

```bash
# Clone the repository
git clone https://github.com/your-org/wadjet-link.git
cd wadjet-link

# Build with Python bindings enabled
mkdir build && cd build
cmake -DWADJET_BUILD_PYTHON_BINDINGS=ON ..
make -j$(nproc)

# Install the Python package
cd ../bindings/python
pip install -e .
```

### Requirements

- Python 3.8+
- pybind11 2.10+
- C++20 compiler (GCC 10+, Clang 12+)
- libpcap-dev (for capture functionality)

## Quick Start

### Reading a PCAP File

```python
import wadjet

# Read all packets
packets = wadjet.read_pcap("capture.pcap")
print(f"Read {len(packets)} packets")

# Iterate efficiently (memory-friendly for large files)
for packet in wadjet.iter_pcap("large_capture.pcap"):
    result = wadjet.decode(packet)
    if result.has_someip():
        hdr = result.someip()
        print(f"Service: 0x{hdr.service_id:04x}, Method: 0x{hdr.method_id:04x}")
```

### Live Capture

```python
import wadjet

# Capture with context manager (auto-cleanup)
with wadjet.LiveCapture("eth0", filter="udp port 30490") as cap:
    # Stream packets in real-time
    for packet in cap.stream(timeout_ms=1000, max_packets=100):
        result = wadjet.decode(packet)
        if result.has_someip():
            print(result.someip())

# Or collect packets into a list
with wadjet.LiveCapture("eth0") as cap:
    packets = cap.collect(count=50, timeout_ms=5000)
```

### Protocol Decoding

```python
import wadjet

# Low-level decode (returns DecodeResult)
result = wadjet.decode(packet)
if result.has_ethernet():
    eth = result.ethernet()
    print(f"Src MAC: {eth.src_mac_str()}")

if result.has_ipv4():
    ip = result.ipv4()
    print(f"Src IP: {ip.src_ip_str()} -> Dst IP: {ip.dst_ip_str()}")

if result.has_someip():
    someip = result.someip()
    print(f"SOME/IP Service: 0x{someip.service_id:04x}")

# High-level parse (returns ProtocolStack dataclass)
stack = wadjet.parse(packet)
if stack.someip:
    print(f"Service: 0x{stack.someip.service_id:04x}")
    
# Convert to dictionary (for JSON serialization)
data = stack.to_dict()
```

### Writing PCAP Files

```python
import wadjet

# Write packets to a file
wadjet.write_pcap("output.pcap", packets)

# Filter packets between files
count = wadjet.filter_pcap(
    "input.pcap",
    "someip_only.pcap",
    wadjet.is_someip  # Predicate function
)
print(f"Wrote {count} SOME/IP packets")

# Merge multiple files
wadjet.merge_pcaps(["a.pcap", "b.pcap"], "merged.pcap", sort_by_time=True)

# Split large files
files = wadjet.split_pcap("large.pcap", "chunk", packets_per_file=1000)
```

### Packet Matching

```python
import wadjet

# Create composable matchers
someip_matcher = wadjet.has_someip(service_id=0x1234)
port_matcher = wadjet.has_port(dst=30490)

# Compose with AND/OR/NOT
combined = someip_matcher & port_matcher
inverted = ~wadjet.has_doip()

# Use with capture
with wadjet.LiveCapture("eth0") as cap:
    packet = cap.wait_for(combined, timeout_ms=5000)
    if packet:
        print("Found matching packet!")
```

## Testing with pytest

### Assertions

```python
import wadjet
import pytest

def test_someip_service_id():
    packets = wadjet.read_pcap("test_capture.pcap")
    
    for packet in packets:
        if wadjet.is_someip(packet):
            # Assert specific values
            header = wadjet.assert_someip(
                packet,
                service_id=0x1234,
                method_id=0x0001,
            )
            assert header.client_id > 0

def test_payload_content():
    packet = packets[0]
    
    # Assert payload matches
    wadjet.assert_payload(packet, expected=b"\x01\x02\x03", offset=0)
    
    # Assert payload contains pattern
    offset = wadjet.assert_payload_contains(packet, pattern=b"\xDE\xAD")
```

### Packet Test Runner

```python
import wadjet

def test_packet_sequence():
    packets = wadjet.read_pcap("sequence.pcap")
    
    runner = wadjet.PacketTestRunner(packets)
    runner.expect(wadjet.has_someip(service_id=0x1000))
    runner.expect(wadjet.has_someip(service_id=0x1001))
    runner.expect(wadjet.has_doip())
    
    # Run and assert all expectations
    runner.assert_all()
```

### Using Fixtures

```python
import pytest
import wadjet

@pytest.fixture
def capture_session():
    """Fixture for live capture tests."""
    with wadjet.LiveCapture("lo", filter="udp") as cap:
        yield cap

def test_live_traffic(capture_session):
    packets = capture_session.collect(count=10, timeout_ms=1000)
    assert len(packets) > 0
```

## API Reference

### Core Types

| Class | Description |
|-------|-------------|
| `Packet` | Mutable packet buffer with timestamp |
| `PacketView` | Immutable view into packet data |
| `Timestamp` | High-precision timestamp (seconds + nanoseconds) |

### Capture

| Class/Function | Description |
|----------------|-------------|
| `LiveCapture` | Context manager for live packet capture |
| `ReplayCapture` | Replay packets from PCAP with timing |
| `CaptureSession` | Low-level capture session |
| `available_devices()` | List available network interfaces |

### Protocol Headers

| Class | Protocol |
|-------|----------|
| `EthernetHeader` | Ethernet II |
| `VlanTag` | 802.1Q VLAN |
| `IPv4Header` | IPv4 |
| `UdpHeader` | UDP |
| `TcpHeader` | TCP |
| `SomeIpHeader` | SOME/IP |
| `SomeIpSdHeader` | SOME/IP Service Discovery |
| `DoIPHeader` | DoIP |

### Decoding

| Function/Class | Description |
|----------------|-------------|
| `decode(packet)` | Decode packet, returns `DecodeResult` |
| `parse(packet)` | Parse packet, returns `ProtocolStack` |
| `is_someip(packet)` | Check if packet contains SOME/IP |
| `is_doip(packet)` | Check if packet contains DoIP |
| `get_someip(packet)` | Extract SOME/IP header or None |

### PCAP I/O

| Function | Description |
|----------|-------------|
| `read_pcap(path)` | Read all packets from PCAP |
| `iter_pcap(path)` | Iterate over packets (memory-efficient) |
| `write_pcap(path, packets)` | Write packets to PCAP |
| `filter_pcap(in, out, pred)` | Filter packets to new file |
| `merge_pcaps(inputs, output)` | Merge multiple PCAP files |
| `split_pcap(input, prefix, n)` | Split PCAP into chunks |

### Testing

| Function/Class | Description |
|----------------|-------------|
| `assert_someip(...)` | Assert SOME/IP header values |
| `assert_doip(...)` | Assert DoIP header values |
| `assert_payload(...)` | Assert payload content |
| `has_someip(...)` | Create SOME/IP matcher |
| `has_doip(...)` | Create DoIP matcher |
| `has_port(...)` | Create port matcher |
| `PacketMatcher` | Composable packet matcher |
| `PacketTestRunner` | Sequential expectation runner |

## NumPy Integration

The `Packet` and `PacketView` classes support the Python buffer protocol:

```python
import numpy as np
import wadjet

packet = wadjet.read_pcap("capture.pcap")[0]

# Zero-copy conversion to NumPy array
data = np.frombuffer(packet, dtype=np.uint8)
print(f"Packet bytes: {data[:14]}")  # First 14 bytes (Ethernet header)
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Python Application                        │
├─────────────────────────────────────────────────────────────┤
│  wadjet package                                              │
│  ├── __init__.py      (exports)                             │
│  ├── capture.py       (LiveCapture, ReplayCapture)          │
│  ├── protocols.py     (decode, parse, ProtocolStack)        │
│  ├── pcap.py          (read_pcap, write_pcap, etc.)         │
│  └── testing.py       (assertions, matchers, runner)        │
├─────────────────────────────────────────────────────────────┤
│  _wadjet (pybind11 native module)                           │
│  ├── module.cpp           (PYBIND11_MODULE)                 │
│  ├── core_bindings.cpp    (Timestamp, utilities)            │
│  ├── packet_bindings.cpp  (Packet, PacketView)              │
│  ├── capture_bindings.cpp (CaptureSession)                  │
│  ├── pcap_bindings.cpp    (PcapReader, PcapWriter)          │
│  ├── protocol_bindings.cpp (all protocol headers)           │
│  └── decoder_bindings.cpp (DecodeResult, dispatcher)        │
├─────────────────────────────────────────────────────────────┤
│  Wadjet C++ Core Library (libwadjet)                        │
└─────────────────────────────────────────────────────────────┘
```

## Examples

See the `examples/` directory for complete examples:

- **`analyze_someip.py`** — Analyze SOME/IP traffic from PCAP files
- **`live_capture.py`** — Real-time SOME/IP traffic monitor
- **`test_protocol.py`** — pytest integration examples

## License

Polyform Noncommercial 1.0.0 — See [LICENSE](../../LICENSE) for details. For commercial licensing, contact the author.

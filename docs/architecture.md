# 𓆓 Wadjet-Link Architecture

This document describes the high-level architecture of Wadjet-Link.

## System Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           User Applications                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ GoogleTest  │  │ wadjet-run  │  │   Python    │  │   Custom C++ App    │ │
│  │   Tests     │  │    CLI      │  │  Bindings   │  │                     │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
└─────────┼────────────────┼────────────────┼────────────────────┼────────────┘
          │                │                │                    │
          ▼                ▼                ▼                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Wadjet-Link Core Library                              │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                         Testing Subsystem                                ││
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────────┐  ││
│  │  │   Matchers   │ │  Fixtures    │ │  Generators  │ │ Scenario       │  ││
│  │  │  (gMock)     │ │ LiveCapture  │ │ (Property)   │ │ Runner (YAML)  │  ││
│  │  └──────────────┘ └──────────────┘ └──────────────┘ └────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                       Protocol Subsystem                                 ││
│  │  ┌──────────────────────────────────────────────────────────────────┐   ││
│  │  │                    ProtocolDispatcher                             │   ││
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────┐ │   ││
│  │  │  │Ethernet │→│  IPv4   │→│UDP/TCP  │→│ SOME/IP │→│ SOME/IP-SD  │ │   ││
│  │  │  │ Parser  │ │ Parser  │ │ Parser  │ │ Parser  │ │   Parser    │ │   ││
│  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────────┘ │   ││
│  │  │                                      ┌─────────┐ ┌─────────────┐ │   ││
│  │  │                                      │  DoIP   │→│    UDS      │ │   ││
│  │  │                                      │ Parser  │ │  Decoder    │ │   ││
│  │  │                                      └─────────┘ └─────────────┘ │   ││
│  │  │                      ┌─────────┐                                  │   ││
│  │  │                      │  gPTP   │                                  │   ││
│  │  │                      │ Parser  │                                  │   ││
│  │  │                      └─────────┘                                  │   ││
│  │  └──────────────────────────────────────────────────────────────────┘   ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                          I/O Subsystem                                   ││
│  │  ┌──────────────────────┐  ┌──────────────────────┐                     ││
│  │  │   CaptureSession     │  │    ReplaySession     │                     ││
│  │  │  ┌────────────────┐  │  │  ┌────────────────┐  │                     ││
│  │  │  │  AF_PACKET     │  │  │  │  Timing Ctrl   │  │                     ││
│  │  │  │ (TPACKET_V3)   │  │  │  │  (Realtime)    │  │                     ││
│  │  │  └────────────────┘  │  │  └────────────────┘  │                     ││
│  │  │  ┌────────────────┐  │  └──────────────────────┘                     ││
│  │  │  │    libpcap     │  │                                               ││
│  │  │  │   (Optional)   │  │  ┌──────────────────────┐                     ││
│  │  │  └────────────────┘  │  │   PCAP Reader/Writer │                     ││
│  │  └──────────────────────┘  └──────────────────────┘                     ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                         Core Types                                       ││
│  │  ┌─────────┐  ┌─────────────┐  ┌─────────┐  ┌───────────┐  ┌─────────┐  ││
│  │  │ Packet  │  │ PacketView  │  │Timestamp│  │Result<T,E>│  │ Logger  │  ││
│  │  │(Mutable)│  │ (Zero-Copy) │  │(Precise)│  │ (Error)   │  │         │  ││
│  │  └─────────┘  └─────────────┘  └─────────┘  └───────────┘  └─────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Operating System                                  │
│  ┌──────────────────┐  ┌──────────────────┐  ┌─────────────────────────────┐│
│  │   Linux Kernel   │  │   Network Stack  │  │      File System            ││
│  │   AF_PACKET      │  │   (TCP/IP)       │  │      (.pcap files)          ││
│  └──────────────────┘  └──────────────────┘  └─────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
```

## Namespace Structure

```
wadjet::
├── io::                    # I/O and capture
│   ├── CaptureSession      # Live packet capture
│   ├── ReplaySession       # Packet replay with timing
│   ├── Packet              # Mutable packet buffer
│   ├── PacketView          # Immutable zero-copy view
│   └── FrameFilter         # BPF filter wrapper
│
├── pcap::                  # PCAP file handling
│   ├── PcapReader          # Read .pcap files
│   ├── PcapWriter          # Write .pcap files
│   └── PcapngWriter        # Write .pcapng files
│
├── net::                   # Network primitives
│   ├── MAC                 # MAC address type
│   ├── IPv4                # IPv4 address type
│   ├── UDP                 # UDP utilities
│   └── TCP                 # TCP utilities
│
├── protocols::             # Protocol decoders
│   ├── ethernet::          # Ethernet II, VLAN
│   ├── ipv4::              # IPv4 header
│   ├── udp::               # UDP header
│   ├── tcp::               # TCP header
│   ├── someip::            # SOME/IP header
│   ├── someip_sd::         # SOME/IP Service Discovery
│   ├── doip::              # DoIP header
│   ├── gptp::              # gPTP (IEEE 802.1AS)
│   ├── uds::               # UDS (ISO 14229)
│   │   ├── UdsDecoder      # Message decoder
│   │   ├── UdsSession      # Session state tracking
│   │   └── UdsSessionManager # Multi-ECU support
│   └── ProtocolDispatcher  # Full stack decoder
│
├── testing::               # Test utilities
│   ├── matchers.hpp        # gMock-style matchers
│   ├── LiveCaptureFixture  # GoogleTest fixture
│   ├── LoopbackFixture     # Self-contained tests
│   ├── generators.hpp      # Property-based testing
│   ├── live_assert.hpp     # Real-time assertions
│   └── record_replay.hpp   # Record-then-assert
│
├── scenario::              # Test automation
│   ├── Scenario            # Test scenario model
│   ├── ScenarioParser      # YAML/JSON parser
│   ├── ScenarioRunner      # Execution engine
│   └── ReportGenerator     # JUnit/JSON/TAP output
│
└── utils::                 # Utilities
    ├── Logger              # Logging facility
    ├── Result<T,E>         # Error handling
    └── TimeUtils           # Time utilities
```

## Data Flow

### Capture Pipeline

```
┌─────────────┐     ┌─────────────────┐     ┌──────────────┐     ┌───────────┐
│   Network   │────▶│  CaptureSession │────▶│ PacketView   │────▶│  Protocol │
│  Interface  │     │  (Ring Buffer)  │     │ (Zero-Copy)  │     │ Dispatcher│
└─────────────┘     └─────────────────┘     └──────────────┘     └─────┬─────┘
                                                                       │
                                                                       ▼
                    ┌──────────────────────────────────────────────────────────┐
                    │                    DecodeResult                           │
                    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │
                    │  │Ethernet │ │  IPv4   │ │  UDP    │ │ SOME/IP / DoIP  │ │
                    │  │ Header  │ │ Header  │ │ Header  │ │     Header      │ │
                    │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │
                    │                                        + Payload Span    │
                    └──────────────────────────────────────────────────────────┘
```

### Test Flow

```
┌───────────────┐     ┌─────────────────┐     ┌───────────────┐
│   Test Case   │────▶│ LiveCapture     │────▶│   Matchers    │
│  (GoogleTest) │     │ TestFixture     │     │  (Assertions) │
└───────────────┘     └────────┬────────┘     └───────┬───────┘
                               │                      │
                               ▼                      ▼
                    ┌─────────────────┐     ┌─────────────────┐
                    │  Captured       │     │   PASS/FAIL     │
                    │  Packets        │     │   + PCAP dump   │
                    └─────────────────┘     └─────────────────┘
```

## Key Design Decisions

### Zero-Copy Architecture

`PacketView` provides immutable, non-owning access to packet data, avoiding copies during decoding:

```cpp
// PacketView is a lightweight span-like type
class PacketView {
    const std::byte* data_;
    std::size_t size_;
    Timestamp timestamp_;
};

// Decoders return views into the original packet
auto result = dispatcher.decode(packet.view());
auto someip = result.someip();  // References packet data
```

### CRTP-Based Decoders

Protocol decoders use CRTP for compile-time polymorphism:

```cpp
template<typename Derived>
class DecoderBase {
public:
    auto decode(PacketView view) {
        return static_cast<Derived*>(this)->decode_impl(view);
    }
};

class SomeIpDecoder : public DecoderBase<SomeIpDecoder> {
    // Implementation
};
```

### Error Handling

Uses `Result<T, E>` for explicit error handling without exceptions:

```cpp
Result<SomeIpHeader, DecodeError> SomeIpDecoder::decode(PacketView view) {
    if (view.size() < MIN_HEADER_SIZE) {
        return Error(DecodeError::TooShort);
    }
    // ... decode logic
    return Ok(header);
}
```

### Ring Buffer Capture

TPACKET_V3 provides high-performance zero-copy capture:

```cpp
// Memory-mapped ring buffer
// Kernel writes packets directly to shared memory
// No system call per packet
┌─────────────────────────────────────────────────┐
│ Block 0 │ Block 1 │ Block 2 │ ... │ Block N-1  │
│ [pkt]   │ [pkt]   │ [pkt]   │     │ [pkt]      │
│ [pkt]   │ [pkt]   │         │     │            │
└─────────────────────────────────────────────────┘
     ▲         ▲
     │         └── Kernel writes here
     └──────────── User reads here
```

## Protocol Stack

### Supported Protocols

| Layer | Protocol | Status |
|-------|----------|--------|
| L2 | Ethernet II | ✅ Complete |
| L2 | 802.1Q VLAN | ✅ Complete |
| L2 | QinQ (Stacked VLAN) | ✅ Complete |
| L3 | IPv4 | ✅ Complete |
| L4 | UDP | ✅ Complete |
| L4 | TCP | ✅ Complete |
| L7 | SOME/IP | ✅ Complete |
| L7 | SOME/IP-SD | ✅ Complete |
| L7 | DoIP | ✅ Complete |
| L7 | gPTP (IEEE 802.1AS) | ✅ Complete |
| L7 | UDS (ISO 14229) | ✅ Complete |
| L7 | DDS/RTPS | ✅ Complete |

### Decode Tree

```text
Ethernet Frame
├── EtherType: 0x0800 (IPv4)
│   └── IPv4 Packet
│       ├── Protocol: 17 (UDP)
│       │   └── UDP Datagram
│       │       ├── Port 30490-30491 → SOME/IP
│       │       │   └── Service ID 0xFFFF → SOME/IP-SD
│       │       ├── Port 13400 → DoIP
│       │       │   └── Payload Type 0x8001 → UDS
│       │       └── Port 7400-7500 → DDS/RTPS
│       │           ├── Port 7400 → SPDP Discovery
│       │           └── Port 7401+ → User Traffic
│       └── Protocol: 6 (TCP)
│           └── TCP Segment
│               └── Port 13400 → DoIP
│                   └── Payload Type 0x8001 → UDS
├── EtherType: 0x8100 (VLAN)
│   └── VLAN Tag + Inner EtherType
│       └── (recurse)
└── EtherType: 0x88F7 (PTP) → gPTP
```

## Testing Architecture

### Matcher Hierarchy

```
testing::
├── Ethernet Matchers
│   ├── HasEthertype(uint16_t)
│   ├── HasSourceMac(MAC)
│   ├── HasDestMac(MAC)
│   └── HasVlanId(uint16_t)
│
├── IP Matchers
│   ├── HasSourceIP(IPv4)
│   ├── HasDestIP(IPv4)
│   ├── IsUDP()
│   └── IsTCP()
│
├── Port Matchers
│   ├── HasSourcePort(uint16_t)
│   └── HasDestPort(uint16_t)
│
├── SOME/IP Matchers
│   ├── HasSOMEIPServiceId(uint16_t)
│   ├── HasSOMEIPMethodId(uint16_t)
│   ├── HasSOMEIPMessageType(MessageType)
│   ├── IsSOMEIPRequest()
│   ├── IsSOMEIPResponse()
│   └── IsSOMEIPNotification()
│
├── DoIP Matchers
│   ├── HasDoIPPayloadType(PayloadType)
│   ├── IsDoIPDiagnosticMessage()
│   └── IsDoIPRoutingActivation*()
│
├── gPTP Matchers
│   ├── IsGptp()
│   ├── HasGptpMessageType(MessageType)
│   ├── IsGptpSync()
│   ├── IsGptpFollowUp()
│   ├── IsGptpPdelayReq()
│   ├── IsGptpPdelayResp()
│   ├── IsGptpAnnounce()
│   ├── HasGptpDomain(uint8_t)
│   ├── HasGptpSequenceId(uint16_t)
│   ├── GptpFromPort(PortIdentity)
│   ├── GptpFromClock(ClockIdentity)
│   ├── IsGptpEventMessage()
│   └── IsGptpTwoStep()
│
├── DDS/RTPS Matchers
│   ├── IsRtps() / IsDds()
│   ├── HasRtpsVersion(major, minor)
│   ├── HasRtpsVendor(VendorId)
│   ├── IsFromFastDDS()
│   ├── IsFromRTI()
│   ├── IsFromCycloneDDS()
│   ├── IsFromOpenDDS()
│   ├── HasRtpsGuidPrefix(GuidPrefix)
│   ├── HasRtpsSubmessage(SubmessageKind)
│   ├── HasRtpsData()
│   ├── HasRtpsHeartbeat()
│   ├── HasRtpsAckNack()
│   ├── HasRtpsGap()
│   ├── HasRtpsInfoTs()
│   ├── HasRtpsInfoDst()
│   ├── HasRtpsSubmessageCount(size_t)
│   └── IsRtpsDiscovery() / IsSpdpOrSedp()
│
├── UDS Matchers
│   ├── IsUds()
│   ├── HasUdsServiceId(ServiceID)
│   ├── IsUdsRequest()
│   ├── IsUdsPositiveResponse()
│   ├── IsUdsNegativeResponse()
│   ├── HasUdsNrc(NRC)
│   ├── IsUdsDiagnosticSessionControl()
│   ├── IsUdsSecurityAccess()
│   ├── IsUdsReadDataByIdentifier()
│   ├── IsUdsWriteDataByIdentifier()
│   ├── IsUdsRoutineControl()
│   └── IsUdsTesterPresent()
│
└── Payload Matchers
    ├── PayloadContains(bytes)
    ├── HasPayloadSize(size)
    └── DecodesSuccessfully()
```

## File Organization

```
wadjet-link/
├── include/wadjet/           # Public headers
│   ├── io/                   # Capture, replay, packets
│   ├── pcap/                 # PCAP reader/writer
│   ├── net/                  # Network primitives
│   ├── protocols/            # Protocol decoders
│   ├── testing/              # Test utilities
│   ├── scenario/             # YAML/JSON scenarios
│   └── utils/                # Utilities
├── src/                      # Implementation
├── tests/                    # Unit and integration tests
├── examples/                 # Example code and scenarios
├── bindings/                 # Language bindings
│   └── python/               # Python (pybind11)
├── tools/                    # CLI tools
│   └── wadjet-run.cpp        # Scenario runner
└── docs/                     # Documentation
```

---

*𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.*

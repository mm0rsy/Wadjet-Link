# Feature Specification: IPv6 Protocol Support

**Feature Branch**: `milestone/018-ipv6-support`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M18 - IPv6 Protocol Support

## Overview

Implement IPv6 protocol support for next-generation automotive Ethernet networks. IPv6 provides expanded address space, improved security (IPsec), and better support for automotive multi-domain networking. This milestone adds IPv6 header parsing, extension header handling, ICMPv6, and IPv6 address utilities.

## User Scenarios & Testing

### User Story 1 - IPv6 Header Parsing (Priority: P1)

As a network engineer, I need IPv6 header parsing, so that I can analyze IPv6 traffic in next-gen automotive networks.

**Why this priority**: IPv6 header parsing is foundational for all IPv6 feature support.

**Independent Test**: Capture and decode IPv6 packets with standard headers.

**Acceptance Scenarios**:

1. **Given** IPv6 packet, **When** decoded, **Then** version (6), traffic class, flow label, payload length, next header, hop limit are extracted
2. **Given** IPv6 source and destination addresses, **When** parsed, **Then** 128-bit addresses are correctly represented
3. **Given** IPv6 payload, **When** identified, **Then** next header field determines upper-layer protocol (TCP=6, UDP=17, ICMPv6=58)
4. **Given** IPv6 hop limit, **When** validated, **Then** packets with hop limit 0 are flagged
5. **Given** IPv6 traffic class (DSCP+ECN), **When** extracted, **Then** QoS markings are available

### User Story 2 - IPv6 Extension Headers (Priority: P1)

As a protocol analyst, I need IPv6 extension header parsing, so that I can analyze fragmentation, routing, and security headers.

**Why this priority**: Extension headers carry critical IPv6 metadata - needed for complete decode.

**Independent Test**: Parse IPv6 packets with extension headers (Hop-by-Hop, Routing, Fragment, Destination Options).

**Acceptance Scenarios**:

1. **Given** IPv6 with Hop-by-Hop extension header, **When** parsed, **Then** options (Pad1, PadN, Router Alert) are extracted
2. **Given** IPv6 with Fragment extension header, **When** decoded, **Then** fragment offset, M flag, identification are available
3. **Given** fragmented IPv6 packets, **When** reassembled, **Then** original datagram is reconstructed
4. **Given** IPv6 with Routing extension header, **When** parsed, **Then** routing type and segments are extracted
5. **Given** IPv6 with Destination Options, **When** decoded, **Then** options array is available

### User Story 3 - ICMPv6 Protocol Support (Priority: P1)

As a diagnostics engineer, I need ICMPv6 parsing, so that I can analyze neighbor discovery and error messages.

**Why this priority**: ICMPv6 is essential for IPv6 - replaces ARP, provides error handling.

**Independent Test**: Decode ICMPv6 messages (Echo, Router/Neighbor Solicitation/Advertisement, Redirect).

**Acceptance Scenarios**:

1. **Given** ICMPv6 Echo Request/Reply, **When** decoded, **Then** identifier and sequence number are extracted
2. **Given** ICMPv6 Router Advertisement, **When** parsed, **Then** router lifetime, reachable time, retrans timer, prefix information are extracted
3. **Given** ICMPv6 Neighbor Solicitation, **When** decoded, **Then** target address and source link-layer address option are available
4. **Given** ICMPv6 Neighbor Advertisement, **When** parsed, **Then** target address, flags (Router, Solicited, Override), and target link-layer address are extracted
5. **Given** ICMPv6 error messages (Destination Unreachable, Time Exceeded), **When** decoded, **Then** error code and original packet header are available

### User Story 4 - IPv6 Address Utilities (Priority: P2)

As a test automation engineer, I need IPv6 address manipulation utilities, so that I can generate filters and match addresses.

**Why this priority**: IPv6 address operations are needed for filtering and test case generation.

**Independent Test**: Test IPv6 address parsing, formatting, and comparison.

**Acceptance Scenarios**:

1. **Given** IPv6 address string "2001:db8::1", **When** parsed, **Then** 128-bit representation is created
2. **Given** IPv6 address, **When** formatted, **Then** compressed notation (::) is used correctly
3. **Given** IPv6 address range, **When** tested, **Then** membership check works (e.g., fe80::/10 for link-local)
4. **Given** IPv6 multicast address, **When** identified, **Then** multicast scope (link-local, site-local, global) is determined
5. **Given** IPv6 address type detection, **When** applied, **Then** loopback (::1), unspecified (::), link-local (fe80::/10), and ULA (fc00::/7) are recognized

### User Story 5 - SOME/IP and DoIP over IPv6 (Priority: P1)

As an automotive protocol engineer, I need SOME/IP and DoIP over IPv6, so that I can validate services in IPv6 networks.

**Why this priority**: Automotive protocols must work over IPv6 for next-gen vehicles.

**Independent Test**: Capture SOME/IP-SD and DoIP over IPv6 and verify decode chain.

**Acceptance Scenarios**:

1. **Given** SOME/IP-SD over IPv6 UDP, **When** decoded, **Then** full decode chain (Ethernet → IPv6 → UDP → SOME/IP-SD) works
2. **Given** DoIP over IPv6 TCP, **When** decoded, **Then** diagnostic messages are correctly extracted
3. **Given** SOME/IP service discovery with IPv6 endpoints, **When** parsed, **Then** IPv6 endpoint options are correctly extracted
4. **Given** IPv6 multicast for SOME/IP-SD, **When** detected, **Then** multicast address is identified (ff02::X, ff05::X)
5. **Given** UDS over DoIP over IPv6, **When** decoded, **Then** UDS service and parameters are accessible

## Edge Cases

- What happens with IPv6 jumbograms (payload > 65535 bytes)?
- How are unknown IPv6 extension headers handled?
- What if ICMPv6 checksum validation fails?
- How does system handle IPv6 packets with multiple extension headers in arbitrary order?
- What happens when IPv6 fragmentation interacts with IPsec (encrypted fragments)?
- How are IPv6 tunneled packets (IPv4 over IPv6, IPv6 over IPv4) processed?

## Requirements

### Functional Requirements

#### IPv6 Header Parsing

- **FR-001**: System MUST parse IPv6 fixed header (40 bytes)
- **FR-002**: System MUST extract version (6), traffic class, flow label, payload length, next header, hop limit
- **FR-003**: System MUST parse IPv6 source and destination addresses (128 bits each)
- **FR-004**: System MUST validate IPv6 version field (must be 6)
- **FR-005**: System MUST support IPv6 over Ethernet (EtherType 0x86DD)

#### IPv6 Extension Headers

- **FR-006**: System MUST parse Hop-by-Hop Options extension header (Next Header = 0)
- **FR-007**: System MUST parse Routing extension header (Next Header = 43)
- **FR-008**: System MUST parse Fragment extension header (Next Header = 44)
- **FR-009**: System MUST parse Destination Options extension header (Next Header = 60)
- **FR-010**: System MUST parse Authentication Header (AH) (Next Header = 51) - metadata only
- **FR-011**: System MUST parse Encapsulating Security Payload (ESP) (Next Header = 50) - metadata only
- **FR-012**: System MUST handle extension header chains (multiple headers in sequence)
- **FR-013**: System MUST detect unknown extension headers and report gracefully

#### IPv6 Fragmentation

- **FR-014**: System MUST extract fragment offset, M (More Fragments) flag, and identification from Fragment header
- **FR-015**: System MUST support IPv6 fragment reassembly
- **FR-016**: System MUST timeout incomplete IPv6 fragments (60s default)
- **FR-017**: System MUST detect fragmented IPv6 packets (Fragment header present)

#### ICMPv6 Protocol

- **FR-018**: System MUST parse ICMPv6 header (type, code, checksum)
- **FR-019**: System MUST parse ICMPv6 Echo Request (type 128) and Echo Reply (type 129)
- **FR-020**: System MUST parse ICMPv6 Destination Unreachable (type 1) with all codes
- **FR-021**: System MUST parse ICMPv6 Packet Too Big (type 2)
- **FR-022**: System MUST parse ICMPv6 Time Exceeded (type 3)
- **FR-023**: System MUST parse ICMPv6 Parameter Problem (type 4)
- **FR-024**: System MUST parse ICMPv6 Router Solicitation (type 133) and Router Advertisement (type 134)
- **FR-025**: System MUST parse ICMPv6 Neighbor Solicitation (type 135) and Neighbor Advertisement (type 136)
- **FR-026**: System MUST parse ICMPv6 Redirect (type 137)
- **FR-027**: System MUST validate ICMPv6 checksum

#### Neighbor Discovery Protocol (NDP)

- **FR-028**: System MUST extract NDP options (Source/Target Link-Layer Address, Prefix Information, Redirected Header, MTU)
- **FR-029**: System MUST parse Prefix Information option (prefix, prefix length, valid/preferred lifetime, flags)
- **FR-030**: System MUST parse Router Advertisement flags (Managed, Other, Home Agent, Proxy)
- **FR-031**: System MUST parse Neighbor Advertisement flags (Router, Solicited, Override)

#### IPv6 Address Utilities

- **FR-032**: System MUST provide IPv6 address parsing from string (RFC 4291 format)
- **FR-033**: System MUST provide IPv6 address formatting to string with compression (::)
- **FR-034**: System MUST support IPv6 address comparison and equality
- **FR-035**: System MUST detect IPv6 address types (loopback, unspecified, link-local, ULA, multicast, global)
- **FR-036**: System MUST extract IPv6 multicast scope (node, link, site, organization, global)
- **FR-037**: System MUST support IPv6 prefix matching (address in subnet check)

#### Protocol Integration

- **FR-038**: IPv6 decoder MUST integrate with ProtocolDispatcher (EtherType 0x86DD)
- **FR-039**: System MUST support SOME/IP over IPv6 UDP
- **FR-040**: System MUST support DoIP over IPv6 TCP
- **FR-041**: System MUST parse SOME/IP-SD IPv6 Endpoint options
- **FR-042**: System MUST support ICMPv6 in protocol stack decode

#### Matchers & Testing

- **FR-043**: System MUST provide gMock matchers: HasIpv6Src(), HasIpv6Dst(), IsIcmpv6Type()
- **FR-044**: System MUST provide ICMPv6-specific matchers: IsIcmpv6Echo(), IsIcmpv6RouterAdvertisement(), etc.
- **FR-045**: Python bindings MUST expose IPv6 and ICMPv6 classes
- **FR-046**: Example program (ipv6_analyzer.cpp) MUST demonstrate IPv6 traffic analysis

### Key Entities

- **Ipv6Header**: IPv6 fixed header (40 bytes)
- **Ipv6Address**: 128-bit address with utilities
- **Ipv6ExtensionHeader**: Base class for extension headers
- **Ipv6HopByHopOptions**: Hop-by-Hop options header
- **Ipv6RoutingHeader**: Routing extension header
- **Ipv6FragmentHeader**: Fragmentation header
- **Ipv6DestinationOptions**: Destination options header
- **Icmpv6Header**: ICMPv6 message header
- **Icmpv6EchoMessage**: Echo Request/Reply
- **Icmpv6RouterAdvertisement**: Router Advertisement with options
- **Icmpv6NeighborSolicitation**: Neighbor Solicitation
- **Icmpv6NeighborAdvertisement**: Neighbor Advertisement
- **Ipv6FragmentReassembler**: Fragment reassembly engine

## Success Criteria

### Measurable Outcomes

- **SC-001**: IPv6 header parsing correctly extracts all fields from test packets
- **SC-002**: All IPv6 extension headers (Hop-by-Hop, Routing, Fragment, Destination Options) correctly parsed
- **SC-003**: IPv6 fragmentation reassembly correctly reconstructs datagrams up to 64KB
- **SC-004**: All ICMPv6 message types (Echo, Router/Neighbor Discovery, errors) correctly parsed
- **SC-005**: IPv6 address utilities correctly parse, format, and classify all address types
- **SC-006**: SOME/IP-SD over IPv6 UDP correctly decoded with IPv6 endpoint extraction
- **SC-007**: DoIP over IPv6 TCP correctly decoded with full UDS support
- **SC-008**: IPv6 decoder handles 10K packets/sec with ≤5ms latency overhead vs IPv4
- **SC-009**: 45+ unit tests cover all IPv6 and ICMPv6 features
- **SC-010**: Fuzz testing with 1M+ iterations shows no crashes on malformed IPv6 packets

## Assumptions

- Target automotive networks using IPv6 per ISO/SAE 21434 and UNECE WP.29
- Focus on stateless IPv6 (no DHCPv6 server simulation)
- IPsec headers (AH, ESP) parsed for metadata only (no decryption)
- IPv6 multicast addresses follow RFC 4291 and automotive-specific allocations
- NDP (Neighbor Discovery Protocol) is primary address resolution mechanism (replaces ARP)

## Dependencies

- **External**: None (pure protocol parsing)
- **Internal**: M2 (Protocol Decoders for IPv4/UDP/TCP integration)

## Out of Scope

- IPv6 routing or forwarding
- DHCPv6 server/client implementation
- IPsec encryption/decryption (only header metadata)
- IPv6 mobility (Mobile IPv6)
- IPv6 transition mechanisms (6to4, Teredo) - only native IPv6
- IPv6 multicast routing protocols (MLD, PIM)
- Full Neighbor Discovery state machine (only parsing)

## Implementation Notes

### Recommended Approach

**Phase 1 - IPv6 Header Parsing** (0.5 weeks)
- Implement Ipv6Header structure
- Parse fixed 40-byte header
- Integrate with EtherType 0x86DD
- Tests: 8+ for header parsing

**Phase 2 - IPv6 Extension Headers** (1 week)
- Implement extension header base class
- Parse Hop-by-Hop, Routing, Destination Options
- Handle extension header chains
- Tests: 12+ for all extension headers

**Phase 3 - IPv6 Fragmentation** (0.5 weeks)
- Parse Fragment extension header
- Implement fragment reassembly
- Tests: 8+ for fragmentation

**Phase 4 - ICMPv6 Core** (1 week)
- Implement ICMPv6 header parsing
- Parse Echo, error messages
- Checksum validation
- Tests: 10+ for ICMPv6 core

**Phase 5 - Neighbor Discovery Protocol** (1 week)
- Parse Router/Neighbor Solicitation/Advertisement
- Extract NDP options
- Tests: 12+ for NDP

**Phase 6 - IPv6 Address Utilities** (0.5 weeks)
- Implement Ipv6Address class
- Parsing, formatting, classification
- Tests: 10+ for address utilities

**Phase 7 - Protocol Integration** (0.5 weeks)
- Integrate with ProtocolDispatcher
- SOME/IP and DoIP over IPv6
- Tests: 8+ for integration

**Phase 8 - Matchers, Examples, Bindings** (1 week)
- gMock matchers for IPv6 and ICMPv6
- Example program (ipv6_analyzer.cpp)
- Python bindings
- Rust bindings
- Tests: 10+ for matchers and bindings

**Total Duration**: ~6 weeks

### IPv6 Header Structure

```cpp
struct Ipv6Header {
    uint8_t version : 4;           // Always 6
    uint8_t traffic_class;         // DSCP + ECN
    uint32_t flow_label : 20;      // Flow identification
    uint16_t payload_length;       // Payload length (excluding header)
    uint8_t next_header;           // Next header type
    uint8_t hop_limit;             // TTL equivalent
    Ipv6Address source;            // 128-bit source
    Ipv6Address destination;       // 128-bit destination
};

class Ipv6Address {
    std::array<uint8_t, 16> bytes_;  // 128 bits
public:
    static std::optional<Ipv6Address> from_string(std::string_view str);
    std::string to_string(bool compress = true) const;
    bool is_loopback() const;        // ::1
    bool is_link_local() const;      // fe80::/10
    bool is_multicast() const;       // ff00::/8
    uint8_t multicast_scope() const; // 1=node, 2=link, 5=site, 8=org, e=global
};
```

### ICMPv6 Neighbor Discovery Example

```cpp
struct Icmpv6NeighborAdvertisement {
    uint8_t type;               // 136
    uint8_t code;               // 0
    uint16_t checksum;
    bool router_flag;
    bool solicited_flag;
    bool override_flag;
    Ipv6Address target_address;
    std::optional<MacAddress> target_link_address;  // From option
};
```

### Test Count Target

**45+ tests** covering:
- IPv6 header: 8 tests
- Extension headers: 12 tests
- Fragmentation: 8 tests
- ICMPv6 core: 10 tests
- NDP: 12 tests
- Address utilities: 10 tests
- Protocol integration: 8 tests
- Matchers and bindings: 10 tests

# API Contracts: Protocol Completeness

**Feature**: M13 Protocol Completeness  
**Date**: 2026-01-15  
**Status**: Phase 1 - Design  
**Purpose**: API specifications for protocol enhancement features

---

## Overview

This document specifies the public API contracts for all protocol completeness features. All APIs are **additive only** (Constitution Principle IV) - no breaking changes to existing decoder interfaces.

---

## 1. TCP Connection Tracker API

**File**: `include/wadjet/protocols/tcp_connection_tracker.hpp`

### 1.1 TcpConnectionTracker Class

```cpp
namespace wadjet::protocols::tcp {

/// @brief TCP connection tracker for stateful analysis
class TcpConnectionTracker {
public:
    /// @brief Configuration
    struct Config {
        std::chrono::seconds timeout_incomplete = std::chrono::seconds(120);  ///< 2 minutes
        std::chrono::seconds timeout_timewait = std::chrono::seconds(30);     ///< 30 seconds
        bool track_retransmissions = true;
        bool track_out_of_order = true;
    };
    
    /// @brief Construct tracker with configuration
    explicit TcpConnectionTracker(Config cfg = Config{});
    
    /// @brief Track TCP packet and update connection state
    /// @param hdr Decoded TCP header
    /// @param ctx Decode context (contains IP addresses)
    /// @return Reference to updated connection state
    TcpConnection& track_packet(const TcpHeader& hdr, const DecodeContext& ctx);
    
    /// @brief Get connection state (read-only)
    /// @param key Connection 5-tuple
    /// @return Pointer to connection, or nullptr if not found
    const TcpConnection* get_connection(const ConnectionKey& key) const;
    
    /// @brief Get all active connections
    /// @return Map of all connections (read-only)
    const std::unordered_map<ConnectionKey, TcpConnection, ConnectionKey::Hash>& 
    connections() const;
    
    /// @brief Cleanup expired connections
    /// @return Number of connections removed
    std::size_t cleanup_expired();
    
    /// @brief Get tracker statistics
    struct Stats {
        std::size_t total_connections = 0;
        std::size_t active_connections = 0;
        std::size_t expired_connections = 0;
        std::size_t retransmissions_detected = 0;
    };
    Stats get_stats() const;
    
private:
    Config config_;
    std::unordered_map<ConnectionKey, TcpConnection, ConnectionKey::Hash> connections_;
    Stats stats_;
    
    void update_state_machine(TcpConnection& conn, const TcpHeader& hdr, bool is_client_to_server);
    void detect_retransmission(TcpConnection& conn, const TcpHeader& hdr, bool is_client_to_server);
    void handle_out_of_order(TcpConnection& conn, const TcpHeader& hdr, bool is_client_to_server);
};

}  // namespace wadjet::protocols::tcp
```

### 1.2 Usage Example

```cpp
#include <wadjet/protocols/tcp_connection_tracker.hpp>

// Create tracker
tcp::TcpConnectionTracker tracker;

// Track packets
for (const auto& packet : packets) {
    auto tcp_result = tcp_decoder.decode(packet.data, ctx);
    if (tcp_result) {
        auto& conn = tracker.track_packet(*tcp_result, ctx);
        
        if (conn.state == tcp::TcpState::ESTABLISHED) {
            std::cout << "Connection established\n";
        }
        
        if (conn.retransmissions_client > 0) {
            std::cout << "Client retransmissions: " << conn.retransmissions_client << "\n";
        }
    }
}

// Periodic cleanup
tracker.cleanup_expired();
```

---

## 2. IPv4 Fragment Reassembler API

**File**: `include/wadjet/protocols/ipv4_fragment_reassembler.hpp`

### 2.1 Ipv4FragmentReassembler Class

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief IPv4 fragment reassembler
class Ipv4FragmentReassembler {
public:
    /// @brief Configuration
    struct Config {
        std::chrono::seconds timeout = std::chrono::seconds(30);  ///< 30s automotive-optimized
        std::size_t max_concurrent_datagrams = 1024;              ///< DoS prevention
    };
    
    /// @brief Construct reassembler with configuration
    explicit Ipv4FragmentReassembler(Config cfg = Config{});
    
    /// @brief Add fragment to reassembly buffer
    /// @param hdr Decoded IPv4 header
    /// @param fragment_data Fragment payload
    /// @param ctx Decode context
    /// @return Complete datagram if reassembly finished, std::nullopt otherwise
    std::optional<Packet> add_fragment(
        const IPv4Header& hdr,
        PacketView fragment_data,
        const DecodeContext& ctx
    );
    
    /// @brief Cleanup expired fragments
    /// @return Number of incomplete datagrams discarded
    std::size_t cleanup_expired();
    
    /// @brief Get reassembler statistics
    struct Stats {
        std::size_t total_fragments = 0;
        std::size_t datagrams_reassembled = 0;
        std::size_t datagrams_expired = 0;
        std::size_t active_reassembly_buffers = 0;
    };
    Stats get_stats() const;
    
private:
    Config config_;
    std::unordered_map<FragmentKey, FragmentBuffer, FragmentKey::Hash> fragments_;
    Stats stats_;
    
    void insert_fragment(FragmentBuffer& buf, std::uint16_t offset, PacketView data);
    bool is_complete(const FragmentBuffer& buf) const;
    Packet reconstruct_datagram(FragmentBuffer& buf);
};

}  // namespace wadjet::protocols::ipv4
```

### 2.2 Usage Example

```cpp
#include <wadjet/protocols/ipv4_fragment_reassembler.hpp>

// Create reassembler
ipv4::Ipv4FragmentReassembler reassembler;

// Process packets
for (const auto& packet : packets) {
    auto ipv4_result = ipv4_decoder.decode(packet.data, ctx);
    if (ipv4_result && ipv4_result->is_fragmented()) {
        auto complete = reassembler.add_fragment(*ipv4_result, packet.payload, ctx);
        
        if (complete) {
            std::cout << "Datagram reassembled: " << complete->size() << " bytes\n";
            // Decode higher-layer protocol from reassembled data
        }
    }
}

// Periodic cleanup
reassembler.cleanup_expired();
```

---

## 3. SOME/IP-TP Reassembler API

**File**: `include/wadjet/protocols/someip_tp_reassembler.hpp`

### 3.1 SomeipTpReassembler Class

```cpp
namespace wadjet::protocols::someip {

/// @brief SOME/IP-TP message reassembler
class SomeipTpReassembler {
public:
    /// @brief Configuration
    struct Config {
        std::chrono::seconds timeout = std::chrono::seconds(5);  ///< 5s per PRS_SOMEIP_00191
        std::size_t max_message_size = 16 * 1024 * 1024;        ///< 16 MB
        std::size_t max_concurrent_messages = 256;              ///< DoS prevention
    };
    
    /// @brief Construct reassembler with configuration
    explicit SomeipTpReassembler(Config cfg = Config{});
    
    /// @brief Add TP segment to reassembly buffer
    /// @param hdr SOME/IP header
    /// @param tp_hdr TP segment header
    /// @param segment_data Segment payload
    /// @return Complete message if reassembly finished, std::nullopt otherwise
    std::optional<Packet> add_segment(
        const SomeIpHeader& hdr,
        const SomeipTpHeader& tp_hdr,
        PacketView segment_data
    );
    
    /// @brief Cleanup expired TP messages
    /// @return Number of incomplete messages discarded
    std::size_t cleanup_expired();
    
    /// @brief Get reassembler statistics
    struct Stats {
        std::size_t total_segments = 0;
        std::size_t messages_reassembled = 0;
        std::size_t messages_expired = 0;
        std::size_t active_reassembly_buffers = 0;
    };
    Stats get_stats() const;
    
private:
    Config config_;
    
    struct TpMessageKey {
        std::uint16_t service_id;
        std::uint16_t method_id;
        std::uint16_t client_id;
        std::uint16_t session_id;
        bool operator==(const TpMessageKey&) const = default;
        struct Hash { std::size_t operator()(const TpMessageKey&) const noexcept; };
    };
    
    std::unordered_map<TpMessageKey, SomeipTpMessage, TpMessageKey::Hash> messages_;
    Stats stats_;
    
    void insert_segment(SomeipTpMessage& msg, std::uint32_t offset, PacketView data);
    bool is_complete(const SomeipTpMessage& msg) const;
};

}  // namespace wadjet::protocols::someip
```

### 3.2 Usage Example

```cpp
#include <wadjet/protocols/someip_tp_reassembler.hpp>

// Create reassembler
someip::SomeipTpReassembler reassembler;

// Process SOME/IP messages
for (const auto& packet : packets) {
    auto someip_result = someip_decoder.decode(packet.data, ctx);
    if (someip_result && someip::SomeipTpHeader::is_tp_message(*someip_result)) {
        auto tp_hdr = someip::SomeipTpHeader::parse(*someip_result, packet.payload);
        if (tp_hdr) {
            auto complete = reassembler.add_segment(*someip_result, *tp_hdr, packet.payload);
            
            if (complete) {
                std::cout << "TP message reassembled: " << complete->size() << " bytes\n";
            }
        }
    }
}
```

---

## 4. UDP Checksum Validator API

**File**: `include/wadjet/protocols/udp_checksum_validator.hpp`

### 4.1 UdpChecksumValidator Class

```cpp
namespace wadjet::protocols::udp {

/// @brief UDP checksum validator
class UdpChecksumValidator {
public:
    /// @brief Validate UDP checksum
    /// @param hdr UDP header
    /// @param payload UDP payload data
    /// @param src_ip Source IP address (for pseudo-header)
    /// @param dst_ip Destination IP address (for pseudo-header)
    /// @return Checksum validation result
    static UdpChecksumResult validate(
        const UdpHeader& hdr,
        PacketView payload,
        IPv4Address src_ip,
        IPv4Address dst_ip
    );
    
    /// @brief Validate UDP checksum (IPv6)
    static UdpChecksumResult validate_ipv6(
        const UdpHeader& hdr,
        PacketView payload,
        const IPv6Address& src_ip,
        const IPv6Address& dst_ip
    );
    
private:
    /// @brief Compute 16-bit 1's complement checksum
    static std::uint16_t compute_checksum(
        IPv4Address src_ip,
        IPv4Address dst_ip,
        std::uint8_t protocol,
        std::uint16_t udp_length,
        const UdpHeader& hdr,
        PacketView payload
    );
};

}  // namespace wadjet::protocols::udp
```

### 4.2 Updated UdpDecoder Options

```cpp
namespace wadjet::protocols::udp {

/// @brief UDP decoder options (UPDATED)
struct Options {
    ChecksumMode checksum_mode = ChecksumMode::Warning;  ///< Default: warning-only
};

class UdpDecoder : public DecoderBase<UdpDecoder, UdpHeader> {
public:
    explicit UdpDecoder(Options opts = Options());
    
    // ... existing methods ...
    
    /// @brief Decode UDP header with optional checksum validation
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;
};

}  // namespace wadjet::protocols::udp
```

### 4.3 Usage Example

```cpp
#include <wadjet/protocols/udp_checksum_validator.hpp>

// Configure decoder with warning-only mode (default)
udp::Options opts;
opts.checksum_mode = udp::ChecksumMode::Warning;
udp::UdpDecoder decoder(opts);

// Decode UDP packet
auto result = decoder.decode(packet.data, ctx);
if (result) {
    if (!result->checksum_valid) {
        std::cout << "WARNING: UDP checksum failed\n";
    }
}
```

---

## 5. Protocol Validator API

**File**: `include/wadjet/protocols/protocol_validator.hpp`

### 5.1 ProtocolLayerValidator Class

```cpp
namespace wadjet::protocols {

/// @brief Cross-layer protocol validator
class ProtocolLayerValidator {
public:
    /// @brief Construct validator with mode
    explicit ProtocolLayerValidator(ValidationMode mode = ValidationMode::Lenient);
    
    /// @brief Validate full protocol stack
    /// @param ctx Decode context (contains all decoded layers)
    /// @return Validation result with warnings/errors
    ValidationResult validate_stack(const DecodeContext& ctx) const;
    
    /// @brief Validate Ethernet → IPv4 layering
    ValidationResult validate_ethernet_ipv4(const DecodeContext& ctx) const;
    
    /// @brief Validate IPv4 → UDP/TCP layering
    ValidationResult validate_ipv4_transport(const DecodeContext& ctx) const;
    
    /// @brief Validate checksum chain (IPv4 + UDP/TCP)
    ValidationResult validate_checksum_chain(const DecodeContext& ctx) const;
    
    /// @brief Validate length chain (Ethernet frame ≥ IPv4 ≥ UDP/TCP)
    ValidationResult validate_length_chain(const DecodeContext& ctx) const;
    
private:
    ValidationMode mode_;
};

}  // namespace wadjet::protocols
```

### 5.2 Usage Example

```cpp
#include <wadjet/protocols/protocol_validator.hpp>

// Create validator in strict mode
protocols::ProtocolLayerValidator validator(protocols::ValidationMode::Strict);

// Validate decoded packet
auto result = validator.validate_stack(ctx);
if (!result.valid) {
    for (const auto& error : result.errors) {
        std::cerr << "ERROR: " << error << "\n";
    }
}

for (const auto& warning : result.warnings) {
    std::cout << "WARNING: " << warning << "\n";
}
```

---

## 6. Enhanced Decoder Options

### 6.1 IPv4Decoder (UPDATED)

```cpp
namespace wadjet::protocols::ipv4 {

class IPv4Decoder : public DecoderBase<IPv4Decoder, IPv4Header> {
public:
    /// @brief Decoder options (UPDATED)
    struct Options {
        bool validate_checksum = true;
        bool allow_bad_checksum = false;
        bool parse_options = true;        // NEW: Parse IPv4 options
        bool validate_fragmentation = true;  // NEW: Validate fragment fields
    };
    
    explicit IPv4Decoder(Options opts = Options());
    
    // ... existing methods unchanged ...
};

}  // namespace wadjet::protocols::ipv4
```

### 6.2 TcpDecoder (UPDATED)

```cpp
namespace wadjet::protocols::tcp {

class TcpDecoder : public DecoderBase<TcpDecoder, TcpHeader> {
public:
    /// @brief Decoder options (UPDATED)
    struct Options {
        bool parse_options = true;         // NEW: Parse TCP options
        bool validate_checksum = false;    // Requires pseudo-header
    };
    
    explicit TcpDecoder(Options opts = Options());
    
    // ... existing methods unchanged ...
};

}  // namespace wadjet::protocols::tcp
```

### 6.3 SomeIpDecoder (UPDATED)

```cpp
namespace wadjet::protocols::someip {

class SomeIpDecoder : public DecoderBase<SomeIpDecoder, SomeIpHeader> {
public:
    /// @brief Decoder options (UPDATED)
    struct Options {
        bool validate_message_length = true;  // NEW: Validate length field
        bool parse_tp_headers = true;         // NEW: Parse TP headers
    };
    
    explicit SomeIpDecoder(Options opts = Options());
    
    // ... existing methods unchanged ...
};

}  // namespace wadjet::protocols::someip
```

### 6.4 DoipDecoder (UPDATED)

```cpp
namespace wadjet::protocols::doip {

class DoipDecoder : public DecoderBase<DoipDecoder, DoipHeader> {
public:
    /// @brief Decoder options (UPDATED)
    struct Options {
        bool parse_power_mode = true;     // NEW: Parse power mode messages
        bool parse_entity_status = true;  // NEW: Parse entity status messages
    };
    
    explicit DoipDecoder(Options opts = Options());
    
    // ... existing methods unchanged ...
};

}  // namespace wadjet::protocols::doip
```

---

## 7. Helper Functions API

### 7.1 TCP Option Parsing

```cpp
namespace wadjet::protocols::tcp {

/// @brief Parse TCP options from header
/// @param options Raw option bytes
/// @return Vector of parsed options
std::vector<TcpOption> parse_tcp_options(std::span<const std::byte> options);

/// @brief Find specific option by kind
/// @param options Parsed options vector
/// @param kind Option kind to find
/// @return Pointer to option, or nullptr if not found
const TcpOption* find_option(const std::vector<TcpOption>& options, TcpOptionKind kind);

}  // namespace wadjet::protocols::tcp
```

### 7.2 IPv4 Option Parsing

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief Parse IPv4 options from header
/// @param options Raw option bytes
/// @return Vector of parsed options
std::vector<Ipv4Option> parse_ipv4_options(std::span<const std::byte> options);

/// @brief Find specific option by type
const Ipv4Option* find_option(const std::vector<Ipv4Option>& options, Ipv4OptionType type);

}  // namespace wadjet::protocols::ipv4
```

### 7.3 SOME/IP-SD Entry/Option Parsing

```cpp
namespace wadjet::protocols::someip_sd {

/// @brief Parse SD entries from message
std::vector<SdEntry> parse_sd_entries(PacketView data, std::size_t num_entries);

/// @brief Parse SD options from message
std::vector<SdOption> parse_sd_options(PacketView data, std::size_t num_options);

/// @brief Link options to specific entry
std::vector<SdOption> get_options_for_entry(
    const std::vector<SdOption>& all_options,
    const SdEntryBase& entry
);

}  // namespace wadjet::protocols::someip_sd
```

### 7.4 UDS NRC Lookup

```cpp
namespace wadjet::protocols::uds {

/// @brief Lookup NRC metadata by code
const NrcMetadata* lookup_nrc(NegativeResponseCode code);

/// @brief Get human-readable NRC description
std::string_view get_nrc_description(NegativeResponseCode code);

/// @brief Classify NRC as temporary or permanent
NrcClass classify_nrc(NegativeResponseCode code);

}  // namespace wadjet::protocols::uds
```

### 7.5 gPTP TLV Parsing

```cpp
namespace wadjet::protocols::gptp {

/// @brief Parse TLV from packet data
/// @param data Raw TLV data
/// @return Parsed TLV variant
GptpTlv parse_tlv(PacketView data);

/// @brief Parse multiple TLVs from message
std::vector<GptpTlv> parse_tlv_array(PacketView data);

}  // namespace wadjet::protocols::gptp
```

---

## 8. Backward Compatibility Guarantees

**Constitution Principle IV Compliance**:

1. **No Breaking Changes**: All existing decoder APIs remain unchanged
2. **Additive Only**: New functionality via:
   - New classes (`TcpConnectionTracker`, `Ipv4FragmentReassembler`, etc.)
   - New options fields (existing options have defaults)
   - New helper functions (existing code unaffected)

3. **Opt-In Features**: 
   - Connection tracking: Explicitly create tracker instance
   - Fragment reassembly: Explicitly create reassembler instance
   - Checksum validation: Default mode is warning-only (non-breaking)

4. **Default Behavior Unchanged**:
   - `IPv4Decoder`: Still decodes headers, options parsing is new feature
   - `TcpDecoder`: Still decodes headers, options parsing is new feature
   - `UdpDecoder`: Checksum validation is warning-only by default

---

## 9. API Versioning

**API Version**: 1.0.0 (M13 Protocol Completeness)

**Semantic Versioning**:
- **Major**: Breaking changes (API removal, signature changes)
- **Minor**: New features (additive only)
- **Patch**: Bug fixes, documentation

**This Milestone**: Minor version bump (1.0.0 → 1.1.0)
- Additive features only (trackers, reassemblers, validators)
- No breaking changes to existing APIs

---

## 10. Thread Safety

**Thread Safety Guarantees**:

| Class | Thread Safety | Notes |
|-------|---------------|-------|
| `TcpConnectionTracker` | **Not thread-safe** | Use one tracker per thread or add external mutex |
| `Ipv4FragmentReassembler` | **Not thread-safe** | Use one reassembler per thread or add external mutex |
| `SomeipTpReassembler` | **Not thread-safe** | Use one reassembler per thread or add external mutex |
| `UdpChecksumValidator` | **Thread-safe** | Static methods, no shared state |
| `ProtocolLayerValidator` | **Thread-safe** | Immutable after construction |
| All decoders | **Thread-safe** | Immutable after construction (per Constitution) |

**Rationale**: Automotive capture is typically single-threaded (Constitution notes), trackers avoid locking overhead.

---

## 11. Performance Contracts

**Computational Complexity**:

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `TcpConnectionTracker::track_packet()` | O(1) average | Hash table lookup |
| `Ipv4FragmentReassembler::add_fragment()` | O(N) | N = number of holes (typically ≤4) |
| `SomeipTpReassembler::add_segment()` | O(1) | Direct offset insertion |
| `UdpChecksumValidator::validate()` | O(M) | M = payload size |
| `ProtocolLayerValidator::validate_stack()` | O(L) | L = number of layers |

**Memory Contracts**:
- TCP tracker: O(C) where C = concurrent connections (bounded by config)
- IPv4 reassembler: O(F) where F = concurrent fragmented datagrams (bounded)
- TP reassembler: O(M) where M = concurrent TP messages (bounded)

---

**Contracts Complete**: 2026-01-15  
**Next Phase**: Phase 1 - Quickstart (usage examples and getting started guide)

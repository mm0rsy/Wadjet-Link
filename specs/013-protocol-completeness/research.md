# Protocol Completeness Research

**Feature**: M13 Protocol Completeness  
**Date**: 2026-01-15  
**Status**: Phase 0 - Research Complete  
**Purpose**: Architectural analysis and design decisions for protocol gap completion

---

## Executive Summary

This research document analyzes architectural options and tradeoffs for implementing complete protocol support across IPv4, TCP, UDP, SOME/IP, SOME/IP-SD, DoIP, UDS, and gPTP. Research focuses on:

1. **TCP State Machine**: Connection tracking patterns from industry implementations
2. **IPv4 Fragmentation**: Reassembly algorithms balancing correctness with automotive constraints
3. **SOME/IP-TP Segmentation**: Large message handling per AUTOSAR specifications
4. **UDP Checksum Validation**: Performance vs. integrity tradeoffs
5. **Protocol Compliance Gaps**: Standards-based implementation requirements

**Key Decisions Made**:
- TCP: Hash-based connection tracking with 5-tuple key, bounded out-of-order buffering
- IPv4: 30s reassembly timeout, first-fragment-wins overlap strategy
- SOME/IP-TP: 16 MB max message size, 5s timeout, segment offset tracking
- UDP: Warning-only checksum mode by default (automotive backward compatibility)
- Cross-protocol: Strict validation mode with lenient fallback option

---

## 1. TCP State Machine Implementation

### 1.1 Research Questions

1. How do production network analyzers (Wireshark, tcpdump) implement TCP state tracking?
2. What data structures optimize connection tracking for automotive packet rates?
3. How to handle connection timeouts without blocking packet processing?
4. What is the minimal state machine for passive analysis (no active connection)?

### 1.2 Industry Analysis

#### Wireshark TCP Analysis (`epan/dissectors/packet-tcp.c`)

**Key Findings**:
- Uses hash table keyed by 5-tuple: `(src_ip, src_port, dst_ip, dst_port, protocol)`
- Tracks TCP state per RFC 793 but simplified for passive analysis (no retransmission timers)
- Out-of-order segment tracking with configurable buffer depth (default: relative sequence numbers)
- Retransmission detection via duplicate sequence numbers + timestamp comparison

**Code Pattern** (from Wireshark source):
```c
/* TCP connection tracking key */
typedef struct {
    address src_addr;
    address dst_addr;
    guint32 src_port;
    guint32 dst_port;
} tcp_conversation_key_t;

/* Per-connection state */
typedef struct {
    guint32 base_seq;          /* Initial sequence number (ISN from SYN) */
    guint32 next_seq;          /* Next expected sequence number */
    guint32 max_seq_seen;      /* Highest sequence number observed */
    nstime_t ts_last_pkt;      /* Timestamp of last packet */
    wmem_tree_t *segments_ooo; /* Out-of-order segments tree */
} tcp_stream_state_t;
```

**Wireshark Design Decisions**:
- **No active state transitions**: Doesn't enforce strict RFC 793 state machine (passive observer)
- **Bi-directional tracking**: Separate state for each direction (client→server, server→client)
- **Memory management**: Uses wmem (Wireshark memory allocator) with packet lifetime scoping
- **Performance**: O(1) hash lookup, O(log N) out-of-order segment insertion

#### Linux Kernel TCP (`net/ipv4/tcp_input.c`)

**Key Findings** (architectural patterns, not direct code reuse):
- Uses RB-tree for out-of-order queue (not simple array)
- Aggressive memory limits: `tcp_rmem` sysctl controls per-socket buffer
- Connection timeout: `TCP_TIMEWAIT_LEN = 60s` (2*MSL), configurable via sysctl
- Fast path optimization: inline likely cases (in-order segments)

**Automotive Adaptation**:
- Kernel is active endpoint (connection management); Wadjet is passive observer
- Kernel needs full state machine; Wadjet needs subset for diagnostic validation
- Kernel optimizes for throughput; Wadjet optimizes for analysis completeness

### 1.3 Decision: TCP Connection Tracking Architecture

**Selected Approach**: Hash-based connection map with bounded out-of-order buffering

#### Data Structure Design

```cpp
namespace wadjet::protocols::tcp {

/// @brief TCP connection states (RFC 793)
enum class TcpState : std::uint8_t {
    CLOSED = 0,
    LISTEN,         // Passive open (not typically seen in passive capture)
    SYN_SENT,       // Active open (client sends SYN)
    SYN_RECEIVED,   // Server receives SYN, sends SYN-ACK
    ESTABLISHED,    // Connection open (data transfer)
    FIN_WAIT_1,     // Active close initiated
    FIN_WAIT_2,     // FIN acknowledged, waiting for remote FIN
    CLOSE_WAIT,     // Remote initiated close
    CLOSING,        // Simultaneous close
    LAST_ACK,       // Waiting for final ACK
    TIME_WAIT,      // 2*MSL timeout (30s for automotive)
};

/// @brief 5-tuple connection key
struct ConnectionKey {
    IPv4Address src_ip;
    IPv4Address dst_ip;
    std::uint16_t src_port;
    std::uint16_t dst_port;
    std::uint8_t protocol;  // Always 6 for TCP, but included for future extensibility
    
    bool operator==(const ConnectionKey&) const = default;
    
    // Hash function for std::unordered_map
    struct Hash {
        std::size_t operator()(const ConnectionKey& key) const noexcept {
            // FNV-1a hash (fast, good distribution)
            std::size_t h = 2166136261u;
            h = (h ^ std::hash<std::uint32_t>{}(key.src_ip.value)) * 16777619u;
            h = (h ^ std::hash<std::uint32_t>{}(key.dst_ip.value)) * 16777619u;
            h = (h ^ key.src_port) * 16777619u;
            h = (h ^ key.dst_port) * 16777619u;
            return h;
        }
    };
};

/// @brief Out-of-order TCP segment
struct TcpSegment {
    std::uint32_t seq_num;
    std::uint32_t length;
    std::chrono::steady_clock::time_point arrival_time;
    PacketView data;  // Zero-copy view (complies with Constitution Principle II)
};

/// @brief TCP connection state
struct TcpConnection {
    TcpState state = TcpState::CLOSED;
    
    // Sequence tracking (bi-directional)
    std::uint32_t client_seq_next = 0;  // Next expected client→server seq
    std::uint32_t server_seq_next = 0;  // Next expected server→client seq
    std::uint32_t client_isn = 0;       // Initial Sequence Number (client)
    std::uint32_t server_isn = 0;       // Initial Sequence Number (server)
    
    // Window tracking
    std::uint16_t client_window = 0;
    std::uint16_t server_window = 0;
    std::uint8_t client_window_scale = 0;  // From TCP option (if SYN)
    std::uint8_t server_window_scale = 0;
    
    // Out-of-order buffering (bounded to 16 segments per Constitution clarifications)
    std::array<std::optional<TcpSegment>, 16> ooo_buffer_client;
    std::array<std::optional<TcpSegment>, 16> ooo_buffer_server;
    
    // Timing
    std::chrono::steady_clock::time_point first_seen;
    std::chrono::steady_clock::time_point last_activity;
    
    // Statistics
    std::uint64_t packets_client = 0;
    std::uint64_t packets_server = 0;
    std::uint64_t retransmissions_client = 0;
    std::uint64_t retransmissions_server = 0;
};

/// @brief TCP connection tracker
class TcpConnectionTracker {
public:
    struct Config {
        std::chrono::seconds timeout_incomplete = std::chrono::seconds(120);  // 2 minutes
        std::chrono::seconds timeout_timewait = std::chrono::seconds(30);     // 30 seconds
        bool track_retransmissions = true;
        bool track_out_of_order = true;
    };
    
    explicit TcpConnectionTracker(Config cfg = Config{});
    
    /// @brief Update connection state with new TCP packet
    /// @return Connection state after update
    TcpConnection& track_packet(const TcpHeader& hdr, const DecodeContext& ctx);
    
    /// @brief Get connection state (read-only)
    const TcpConnection* get_connection(const ConnectionKey& key) const;
    
    /// @brief Cleanup stale connections (call periodically)
    std::size_t cleanup_expired();
    
private:
    Config config_;
    std::unordered_map<ConnectionKey, TcpConnection, ConnectionKey::Hash> connections_;
    
    void update_state_machine(TcpConnection& conn, const TcpHeader& hdr, bool is_client_to_server);
    void detect_retransmission(TcpConnection& conn, const TcpHeader& hdr, bool is_client_to_server);
    void handle_out_of_order(TcpConnection& conn, const TcpHeader& hdr, bool is_client_to_server);
};

}  // namespace wadjet::protocols::tcp
```

#### Rationale for Design Decisions

**1. Hash Map vs. RB-Tree**:
- **Hash map chosen**: O(1) average case lookup (critical for high packet rates)
- Automotive captures are typically <10k concurrent connections (much smaller than web server)
- Trade-off: Slightly higher memory overhead vs. faster lookups

**2. Fixed 16-segment Out-of-Order Buffer**:
- **Array chosen over dynamic tree**: Predictable memory usage (AUTOSAR compliance)
- Automotive networks have low latency (<10ms typical) → minimal reordering
- 16 segments handles typical reordering window without unbounded growth
- Trade-off: May drop excessive out-of-order segments, but logs warning

**3. Timeout Values**:
- **2 minutes for incomplete connections**: Balances diagnostic session length with memory cleanup
- **30 seconds for TIME_WAIT**: Automotive-optimized (1*MSL instead of 2*MSL)
- Per RFC 793, MSL = 2 minutes, but automotive ECUs have faster teardown cycles
- Trade-off: Slight risk of sequence number reuse, acceptable for passive analysis

**4. Bi-directional Tracking**:
- Separate sequence tracking for each direction (client→server, server→client)
- Required for DoIP (diagnostic tester ↔ ECU) session validation
- Trade-off: Double the state storage, but essential for diagnostic correctness

**5. Zero-Copy Compliance** (Constitution Principle II):
- Out-of-order segments store `PacketView` (reference), not copied data
- Reassembly operates on views, only copies on final delivery
- Trade-off: Caller must keep original packet alive during analysis

### 1.4 TCP Options Parsing

#### Research: TCP Option Format (RFC 793, RFC 7323, RFC 2018)

**TLV Structure**:
```
+--------+--------+---------+
|  Kind  | Length |  Data   |
+--------+--------+---------+
```

**Common Options**:
- **Kind 0**: End of Option List (EOL) — 1 byte, signals end
- **Kind 1**: No Operation (NOP) — 1 byte, padding for alignment
- **Kind 2**: Maximum Segment Size (MSS) — 4 bytes total (kind + length + 2-byte MSS)
- **Kind 3**: Window Scale — 3 bytes total (RFC 7323)
- **Kind 4**: SACK Permitted — 2 bytes total (RFC 2018)
- **Kind 5**: SACK — variable length, contains block pairs (RFC 2018)
- **Kind 8**: Timestamps — 10 bytes total (RFC 7323)

**Parsing Algorithm**:
```cpp
std::vector<TcpOption> parse_tcp_options(std::span<const std::byte> options) {
    std::vector<TcpOption> parsed;
    std::size_t offset = 0;
    
    while (offset < options.size()) {
        auto kind = static_cast<TcpOptionKind>(options[offset]);
        
        // Handle single-byte options (EOL, NOP)
        if (kind == TcpOptionKind::EndOfOptions) {
            break;  // No more options
        }
        if (kind == TcpOptionKind::NoOperation) {
            offset++;
            continue;  // Skip padding
        }
        
        // Multi-byte options have length field
        if (offset + 1 >= options.size()) {
            break;  // Malformed (no length byte)
        }
        
        std::uint8_t length = std::to_integer<std::uint8_t>(options[offset + 1]);
        if (length < 2 || offset + length > options.size()) {
            break;  // Malformed (invalid length)
        }
        
        TcpOption opt;
        opt.kind = kind;
        opt.data.assign(options.begin() + offset + 2, options.begin() + offset + length);
        parsed.push_back(std::move(opt));
        
        offset += length;
    }
    
    return parsed;
}
```

**Alternative Considered**: Pre-parsed option struct with fixed fields
- **Rejected**: Too rigid, doesn't handle unknown/future options gracefully
- **Selected approach**: Generic TLV parser + accessor methods (`mss()`, `window_scale()`)

---

## 2. IPv4 Fragmentation and Reassembly

### 2.1 Research Questions

1. What is the optimal reassembly timeout for automotive networks?
2. How to handle overlapping fragments (security vs. performance)?
3. What fragment cache size prevents DoS while supporting legitimate use?
4. How do industry tools (tcpdump, Wireshark, FreeBSD) implement reassembly?

### 2.2 Standards Analysis

#### RFC 791 (IPv4 Specification) - Section 3.2 Fragmentation

**Key Requirements**:
- **Fragment Offset**: 13-bit field (in 8-byte units) → max 64KB datagram
- **Identification Field**: 16-bit value to correlate fragments
- **More Fragments (MF) Flag**: Indicates more fragments follow
- **Reassembly Timeout**: "If insufficient time (60 seconds) has elapsed..." (RFC 791 §3.2)

**RFC 791 Algorithm**:
```
1. Create fragment buffer indexed by (src, dst, protocol, id)
2. Store fragments by offset
3. Wait for all fragments (MF=0 and no gaps)
4. Timeout after 60 seconds, discard incomplete datagram
```

**Automotive Adaptation**:
- **30-second timeout** (specified in clarifications) vs. 60s RFC default
- Rationale: Automotive networks are local (no WAN latency), faster cleanup acceptable
- Trade-off: Legitimate slow fragments may timeout, but rare in automotive context

#### RFC 815 (IP Datagram Reassembly Algorithm)

**Improved Algorithm**:
- Uses "hole descriptors" to track missing fragments
- More efficient than scanning full buffer

**Pseudocode**:
```
Fragment Buffer:
  - Data buffer (max 64KB)
  - Hole list: [(offset_start, offset_end), ...]
  
On fragment arrival:
  1. Insert data at fragment_offset
  2. Update hole list (remove filled regions)
  3. If hole list empty: reassembly complete
  4. If timeout expired: discard buffer
```

**Decision**: Implement RFC 815 hole descriptor approach
- More memory-efficient than bitmap (64KB → 8KB bitmap)
- Faster completion check (hole list empty vs. bitmap scan)
- Trade-off: Slightly more complex code, but better performance

### 2.3 Industry Implementation Analysis

#### FreeBSD `ip_input.c` - `ip_reass()`

**Key Findings**:
```c
/* FreeBSD fragment queue entry */
struct ipq {
    struct ipq *next, *prev;    /* Linked list of fragments */
    u_char ipq_ttl;             /* Time to live counter */
    u_char ipq_p;               /* Protocol (TCP, UDP, etc.) */
    u_short ipq_id;             /* Sequence ID for fragments */
    struct in_addr ipq_src, ipq_dst;
    struct mbuf *ipq_frags;     /* Fragment chain */
};

/* Reassembly timeout */
#define IPFRAGTTL 60  /* 60 seconds (RFC 791) */
```

**FreeBSD Design**:
- Per-fragment `ipq` structure with linked list
- Timer-based cleanup (slow timer callback every 500ms)
- **Overlap handling**: First fragment wins (drops overlapping data from later fragments)

**Automotive Implication**:
- First-fragment-wins is correct for legitimate traffic
- Prevents certain IP fragmentation attacks (Teardrop, etc.)
- Performance: No need to re-validate overlaps

#### Wireshark Fragment Reassembly (`epan/reassemble.c`)

**Key Findings**:
```c
typedef struct {
    guint32 id;       /* Fragment identification */
    guint32 offset;   /* Fragment offset */
    guint32 len;      /* Fragment length */
    guint32 datalen;  /* Actual data length */
    const guint8 *data;
} fragment_item;

/* Reassembly table */
typedef struct {
    GHashTable *fragment_table;  /* Hash of fragment chains */
    GHashTable *reassembled_table;
} reassembly_table;
```

**Wireshark Design**:
- Hash table keyed by `(src, dst, protocol, id)`
- Stores all fragments, reassembles on last fragment arrival
- **No timeout in default operation** (post-capture analysis)
- **Overlap handling**: Configurable (first-fragment, last-fragment, or error)

**Decision**: Use first-fragment-wins (security + simplicity)

### 2.4 Decision: IPv4 Fragment Reassembly Architecture

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief Fragment cache key
struct FragmentKey {
    IPv4Address src_ip;
    IPv4Address dst_ip;
    std::uint8_t protocol;
    std::uint16_t identification;
    
    bool operator==(const FragmentKey&) const = default;
    
    struct Hash {
        std::size_t operator()(const FragmentKey& key) const noexcept {
            std::size_t h = 2166136261u;
            h = (h ^ std::hash<std::uint32_t>{}(key.src_ip.value)) * 16777619u;
            h = (h ^ std::hash<std::uint32_t>{}(key.dst_ip.value)) * 16777619u;
            h = (h ^ key.protocol) * 16777619u;
            h = (h ^ key.identification) * 16777619u;
            return h;
        }
    };
};

/// @brief Hole descriptor (RFC 815)
struct FragmentHole {
    std::uint16_t start_offset;  // In bytes
    std::uint16_t end_offset;    // In bytes (exclusive)
};

/// @brief Fragment reassembly buffer
struct FragmentBuffer {
    std::array<std::byte, 65536> data;  // Max IP datagram size
    std::vector<FragmentHole> holes;    // Holes to fill
    std::uint16_t total_length = 0;     // Final datagram length (known when last fragment arrives)
    bool have_last_fragment = false;
    std::chrono::steady_clock::time_point first_arrival;
    std::chrono::steady_clock::time_point last_arrival;
};

/// @brief IPv4 fragment reassembler
class Ipv4FragmentReassembler {
public:
    struct Config {
        std::chrono::seconds timeout = std::chrono::seconds(30);  // Automotive-optimized
        std::size_t max_concurrent_datagrams = 1024;  // Prevent DoS
    };
    
    explicit Ipv4FragmentReassembler(Config cfg = Config{});
    
    /// @brief Add fragment to reassembly buffer
    /// @return Complete datagram if reassembly finished, nullopt otherwise
    std::optional<Packet> add_fragment(
        const IPv4Header& hdr,
        PacketView fragment_data,
        const DecodeContext& ctx
    );
    
    /// @brief Cleanup expired fragments
    std::size_t cleanup_expired();
    
private:
    Config config_;
    std::unordered_map<FragmentKey, FragmentBuffer, FragmentKey::Hash> fragments_;
    
    void insert_fragment(FragmentBuffer& buf, std::uint16_t offset, PacketView data);
    bool is_complete(const FragmentBuffer& buf) const;
};

}  // namespace wadjet::protocols::ipv4
```

#### Rationale for Design Decisions

**1. Timeout: 30 seconds**:
- RFC 791 specifies 60s minimum
- Automotive networks: low latency (<10ms), local communication
- FreeBSD uses 60s for Internet traffic (multi-hop, WAN delays)
- **Decision**: 30s balances standard compliance with automotive resource constraints
- Trade-off: Legitimate slow fragments may timeout (rare in automotive)

**2. Overlap Strategy: First-fragment-wins**:
- **Security**: Prevents fragment-based attacks (Teardrop, etc.)
- **Simplicity**: No need to track and compare overlapping data
- **Standard compliance**: Most OS implementations use first-fragment-wins
- Trade-off: Later fragments with corrected data are ignored (non-issue for legitimate traffic)

**3. Max Concurrent Datagrams: 1024**:
- **DoS prevention**: Limits memory usage (1024 * 64KB = 64MB max)
- **Automotive realistic load**: Typical ECU sends <10 fragmented datagrams concurrently
- Trade-off: Excessive fragmentation (attack or misconfiguration) drops oldest fragments

**4. Hole Descriptor (RFC 815) vs. Bitmap**:
- **Memory**: Typical fragmentation has 2-4 holes → ~32 bytes vs. 8KB bitmap
- **Performance**: O(N) hole insertion vs. O(1) bitmap set, but N is small (typically N≤4)
- Trade-off: Slightly slower insertion, but much better memory efficiency

### 2.5 IPv4 Options Parsing

#### Research: IPv4 Option Types (IANA Registry)

**Common Options** (from IANA IP Option Numbers):
- **0**: End of Options List (EOL) — 1 byte
- **1**: No Operation (NOP) — 1 byte (padding)
- **7**: Record Route — variable length (records router addresses)
- **68**: Timestamp — variable length (records timestamps)
- **131**: Loose Source Route — variable length
- **137**: Strict Source Route — variable length
- **148**: Router Alert — 4 bytes (RFC 2113, used by IGMP, RSVP)

**Format** (Type-Length-Value):
```
+--------+--------+--------+--------+
| Type   | Length | Pointer| Data   |
+--------+--------+--------+--------+
```

**Parsing Algorithm**:
```cpp
struct Ipv4Option {
    std::uint8_t type;
    std::vector<std::byte> data;  // Excludes type and length bytes
};

std::vector<Ipv4Option> parse_ipv4_options(std::span<const std::byte> options) {
    std::vector<Ipv4Option> parsed;
    std::size_t offset = 0;
    
    while (offset < options.size()) {
        std::uint8_t type = std::to_integer<std::uint8_t>(options[offset]);
        
        // Handle single-byte options
        if (type == 0 || type == 1) {  // EOL or NOP
            if (type == 0) break;  // EOL terminates options
            offset++;
            continue;
        }
        
        // Multi-byte options
        if (offset + 1 >= options.size()) break;  // Malformed
        
        std::uint8_t length = std::to_integer<std::uint8_t>(options[offset + 1]);
        if (length < 2 || offset + length > options.size()) break;
        
        Ipv4Option opt;
        opt.type = type;
        opt.data.assign(options.begin() + offset + 2, options.begin() + offset + length);
        parsed.push_back(std::move(opt));
        
        offset += length;
    }
    
    return parsed;
}
```

**Automotive Relevance**:
- **Router Alert**: Used by some automotive multicast protocols (DDS discovery)
- **Timestamp**: Rare in automotive, but diagnostic tools may use for latency measurement
- **Source Routing**: Security risk, typically disabled in automotive networks

**Decision**: Parse all option types generically, provide type-specific accessors
- Generic parser handles future/unknown options
- Type-specific methods (`router_alert_value()`, `timestamp_data()`) for common options
- Trade-off: Slightly less efficient than hard-coded parsers, but more maintainable

---

## 3. SOME/IP Transport Protocol (TP) Segmentation

### 3.1 Research Questions

1. What is the SOME/IP-TP header format per AUTOSAR PRS_SOMEIP?
2. How to handle out-of-order TP segments in automotive networks?
3. What is a realistic maximum message size (firmware updates, diagnostics)?
4. How does segment timeout balance completeness vs. memory usage?

### 3.2 Standards Analysis

#### AUTOSAR PRS_SOMEIP (Protocol Specification) - Section 4.2.1

**TP Header Format** (extends standard SOME/IP header):
```
Standard SOME/IP Header (16 bytes):
+------------------+------------------+
| Service ID (16b) | Method ID (16b)  |  Message ID
+------------------+------------------+
| Length (32b)                        |  Total length (including this field)
+-------------------------------------+
| Client ID (16b)  | Session ID (16b) |  Request ID
+------------------+------------------+
| Proto Ver | Iface Ver | Msg Type   |
+------------------+------------------+
| Return Code       | ...             |
+-------------------------------------+

TP-specific fields (in Message Type byte):
- Bit 5: TP flag (1 = TP message, 0 = non-TP)
- Offset field: 4 bytes after standard header (replaces payload start)
```

**TP Message Structure**:
```
+------------------+------------------+
| Standard SOME/IP Header (16 bytes) |
+-------------------------------------+
| TP Offset (32 bits)                 |  Byte offset in complete message
+-------------------------------------+
| TP Payload (variable)               |
+-------------------------------------+

More Segments flag: Bit 0 of Message Type byte
Offset: Absolute byte position in reassembled message
```

**PRS_SOMEIP_00191** (TP Timeout):
- "If segments are not received within a configurable timeout (default 5 seconds), the incomplete message SHALL be discarded."

**PRS_SOMEIP_00305** (Maximum Message Size):
- "TP SHALL support messages up to 4GB in theory (32-bit length field)"
- "Practical implementations MAY limit to smaller sizes"

**Automotive Realistic Maximum**:
- **Firmware update**: Typical ECU firmware 8-16 MB (compressed)
- **Diagnostic data upload**: DTC freeze frames, memory dumps up to 10 MB
- **Large method calls**: Complex configuration data up to 1 MB
- **Decision**: 16 MB maximum (balances realistic use cases with DoS prevention)

### 3.3 Implementation Architecture

```cpp
namespace wadjet::protocols::someip {

/// @brief SOME/IP-TP message type flag (bit 5 of message_type byte)
inline constexpr std::uint8_t TP_FLAG = 0x20;

/// @brief More segments flag (bit 0 of message_type byte, when TP_FLAG is set)
inline constexpr std::uint8_t MORE_SEGMENTS_FLAG = 0x01;

/// @brief TP segment header
struct SomeipTpHeader {
    std::uint32_t offset;        ///< Byte offset in complete message
    bool more_segments;          ///< True if more segments follow
    std::uint32_t segment_length;  ///< This segment's payload length
};

/// @brief TP reassembly buffer
struct SomeipTpMessage {
    std::uint16_t service_id;
    std::uint16_t method_id;
    std::uint16_t client_id;
    std::uint16_t session_id;
    
    std::vector<std::byte> data;  // Reassembled payload (up to 16 MB)
    std::uint32_t total_length = 0;  // Known when last segment (more_segments=false) arrives
    std::uint32_t bytes_received = 0;
    bool have_last_segment = false;
    
    std::chrono::steady_clock::time_point first_segment_time;
    std::chrono::steady_clock::time_point last_segment_time;
};

/// @brief SOME/IP-TP reassembler
class SomeipTpReassembler {
public:
    struct Config {
        std::chrono::seconds timeout = std::chrono::seconds(5);  // PRS_SOMEIP_00191
        std::size_t max_message_size = 16 * 1024 * 1024;  // 16 MB
        std::size_t max_concurrent_messages = 256;  // Prevent DoS
    };
    
    explicit SomeipTpReassembler(Config cfg = Config{});
    
    /// @brief Add TP segment to reassembly buffer
    /// @return Complete message if reassembly finished, nullopt otherwise
    std::optional<Packet> add_segment(
        const SomeIpHeader& hdr,
        const SomeipTpHeader& tp_hdr,
        PacketView segment_data
    );
    
    /// @brief Cleanup expired TP messages
    std::size_t cleanup_expired();
    
private:
    Config config_;
    
    struct TpMessageKey {
        std::uint16_t service_id;
        std::uint16_t method_id;
        std::uint16_t client_id;
        std::uint16_t session_id;
        bool operator==(const TpMessageKey&) const = default;
        
        struct Hash {
            std::size_t operator()(const TpMessageKey& key) const noexcept {
                std::size_t h = 2166136261u;
                h = (h ^ key.service_id) * 16777619u;
                h = (h ^ key.method_id) * 16777619u;
                h = (h ^ key.client_id) * 16777619u;
                h = (h ^ key.session_id) * 16777619u;
                return h;
            }
        };
    };
    
    std::unordered_map<TpMessageKey, SomeipTpMessage, TpMessageKey::Hash> messages_;
    
    void insert_segment(SomeipTpMessage& msg, std::uint32_t offset, PacketView data);
    bool is_complete(const SomeipTpMessage& msg) const;
};

}  // namespace wadjet::protocols::someip
```

#### Rationale for Design Decisions

**1. Maximum Message Size: 16 MB**:
- AUTOSAR allows up to 4GB (32-bit length field)
- **Realistic automotive use case**: Firmware updates (8-16 MB compressed)
- **DoS prevention**: 16 MB * 256 concurrent = 4 GB max memory
- Trade-off: Larger messages (>16 MB) rejected, but rare in automotive

**2. Timeout: 5 seconds**:
- **AUTOSAR PRS_SOMEIP_00191**: Explicitly specifies 5s default
- Automotive Ethernet: 100/1000 Mbps → 16 MB transfer in <200ms
- 5s allows for transient network congestion
- Trade-off: Stale messages hold memory longer, but prevents premature timeout

**3. Out-of-Order Segment Handling**:
- **Vector-based reassembly**: Insert at `offset` position (like IPv4 fragments)
- Segments can arrive in any order (UDP-based SOME/IP-SD)
- TCP-based SOME/IP is ordered, but TP segments may still be reordered by application
- Trade-off: More complex than sequential append, but handles realistic scenarios

**4. Key: (service_id, method_id, client_id, session_id)**:
- Matches SOME/IP Request ID semantics
- Different sessions of same method are independent
- Trade-off: More keys than just (client_id, session_id), but prevents cross-message confusion

---

## 4. UDP Checksum Validation

### 4.1 Research Questions

1. What is the performance impact of UDP checksum validation on automotive packet rates?
2. How do legacy automotive systems (pre-2015) handle UDP checksums?
3. What validation modes balance integrity vs. backward compatibility?

### 4.2 Standards Analysis

#### RFC 768 (UDP Specification)

**Checksum Algorithm**:
```
Pseudo-header (for checksum calculation):
+--------+--------+--------+--------+
| Source IP Address (32 bits)       |
+--------+--------+--------+--------+
| Destination IP Address (32 bits)  |
+--------+--------+--------+--------+
| Zero   | Protocol | UDP Length    |
+--------+--------+--------+--------+

1. Construct pseudo-header + UDP header + payload
2. Treat as sequence of 16-bit words
3. Sum all words with 1's complement arithmetic
4. Take 1's complement of sum
```

**Zero Checksum**:
- **IPv4**: Zero checksum = 0x0000 means "no checksum computed" (allowed)
- **IPv6**: Zero checksum is forbidden (RFC 2460)

**Automotive Context**:
- **Legacy ECUs** (pre-AUTOSAR 4.x): Some skip UDP checksum for performance
- **Modern ECUs** (AUTOSAR 4.x+): Checksums enabled by default
- **SOME/IP-SD**: Runs over UDP, checksum recommended but not mandatory

### 4.3 Implementation Architecture

```cpp
namespace wadjet::protocols::udp {

/// @brief UDP checksum validation modes
enum class ChecksumMode {
    Disabled,   ///< Skip checksum validation entirely
    Warning,    ///< Validate and log warnings, but don't drop packets (DEFAULT)
    Strict      ///< Validate and mark packets with bad checksum as invalid
};

/// @brief UDP checksum validator
class UdpChecksumValidator {
public:
    /// @brief Validate UDP checksum
    /// @param hdr UDP header
    /// @param payload UDP payload data
    /// @param src_ip Source IP address (for pseudo-header)
    /// @param dst_ip Destination IP address (for pseudo-header)
    /// @return true if checksum valid or zero (IPv4 only)
    static bool validate(
        const UdpHeader& hdr,
        PacketView payload,
        IPv4Address src_ip,
        IPv4Address dst_ip
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

/// @brief Updated UDP decoder options
struct Options {
    ChecksumMode checksum_mode = ChecksumMode::Warning;  // Default: warning-only
};

}  // namespace wadjet::protocols::udp
```

#### Rationale for Design Decisions

**1. Warning-Only Mode as Default**:
- **Backward compatibility**: Legacy ECUs without checksums remain analyzable
- **Integrity checking**: Logs warnings for corrupted packets (visibility)
- **Non-destructive**: Doesn't drop packets (passive analysis principle)
- Trade-off: Corrupted packets pass through to higher layers (but logged)

**2. Three Modes** (Disabled, Warning, Strict):
- **Disabled**: Performance-critical analysis (trusted network)
- **Warning**: Default (balance integrity + compatibility)
- **Strict**: Security-critical environments (drop bad packets)
- Trade-off: Mode complexity vs. flexibility for different use cases

**3. Zero Checksum Handling**:
- IPv4: 0x0000 checksum is valid (RFC 768)
- IPv6: 0x0000 checksum is invalid (RFC 2460)
- Decision: Check IP version in validator
- Trade-off: Slightly more complex logic, but standard-compliant

**4. Performance Optimization**:
- Checksum validation is O(N) where N = packet size
- **Fast path**: If mode=Disabled, skip entirely
- **SIMD opportunity**: 16-bit word summation can use SIMD (future optimization)
- Trade-off: Initial implementation uses scalar code (simpler, profile before optimizing)

---

## 5. Protocol Compliance Gap Analysis

### 5.1 DoIP (ISO 13400-2) Gaps

**Current Implementation** (from existing codebase review):
- Basic DoIP header parsing (payload type, length)
- Diagnostic message routing
- Vehicle identification

**Missing (per spec.md FR-032 to FR-037)**:
- ✗ Diagnostic Power Mode messages (0x4003 request, 0x4004 response)
- ✗ Entity Status messages (0x4001 request, 0x4002 response)
- ✗ Alive Check messages (request/response)
- ✗ Comprehensive NACK code handling (only basic NACKs implemented)

**Implementation Plan**:
```cpp
namespace wadjet::protocols::doip {

/// @brief DoIP diagnostic power mode states
enum class DiagnosticPowerMode : std::uint8_t {
    NotReady = 0x00,
    Ready = 0x01,
    NotSupported = 0x02
};

/// @brief DoIP entity status (node type)
enum class NodeType : std::uint8_t {
    DoipGateway = 0x00,
    DoipNode = 0x01
};

/// @brief DoIP NACK codes (ISO 13400-2 Table 21)
enum class NackCode : std::uint8_t {
    InvalidProtocolVersion = 0x00,
    InvalidPayloadLength = 0x01,
    OutOfMemory = 0x02,
    // ... (complete set per ISO 13400-2)
};

}  // namespace wadjet::protocols::doip
```

**Complexity**: Low (straightforward message parsing, no state machine)

### 5.2 UDS (ISO 14229-1) Gaps

**Current Implementation**:
- Basic UDS request/response parsing
- Service ID extraction
- Subset of NRC codes (~20 codes)

**Missing (per spec.md FR-038 to FR-043)**:
- ✗ Complete NRC code set (50+ codes from 0x10 to 0x93)
- ✗ NRC human-readable descriptions
- ✗ NRC classification (temporary vs. permanent)
- ✗ Service-specific NRC interpretation
- ✗ Positive response suppression bit handling

**Implementation Plan**:
```cpp
namespace wadjet::protocols::uds {

/// @brief Complete UDS NRC codes (ISO 14229-1 Table A.1)
enum class NegativeResponseCode : std::uint8_t {
    GeneralReject = 0x10,
    ServiceNotSupported = 0x11,
    SubFunctionNotSupported = 0x12,
    IncorrectMessageLengthOrInvalidFormat = 0x13,
    ResponseTooLong = 0x14,
    BusyRepeatRequest = 0x21,  // Temporary error
    ConditionsNotCorrect = 0x22,
    RequestSequenceError = 0x24,
    NoResponseFromSubnetComponent = 0x25,
    // ... (50+ total codes)
    RequestOutOfRange = 0x31,
    SecurityAccessDenied = 0x33,
    InvalidKey = 0x35,
    ExceedNumberOfAttempts = 0x36,
    RequiredTimeDelayNotExpired = 0x37,
    // ... (complete per ISO 14229-1 Table A.1)
};

/// @brief NRC classification
enum class NrcClass {
    Temporary,   ///< Retry may succeed (e.g., BusyRepeatRequest)
    Permanent    ///< Retry will fail (e.g., ServiceNotSupported)
};

/// @brief NRC metadata
struct NrcMetadata {
    NegativeResponseCode code;
    std::string_view description;
    NrcClass classification;
};

/// @brief NRC dictionary (static lookup table)
constexpr std::array<NrcMetadata, 50+> NRC_TABLE = {
    {NegativeResponseCode::GeneralReject, "General Reject", NrcClass::Permanent},
    {NegativeResponseCode::ServiceNotSupported, "Service Not Supported", NrcClass::Permanent},
    {NegativeResponseCode::BusyRepeatRequest, "Busy - Repeat Request", NrcClass::Temporary},
    // ... (complete table)
};

}  // namespace wadjet::protocols::uds
```

**Complexity**: Low (data-driven lookup table, no complex logic)

### 5.3 gPTP (IEEE 802.1AS) Gaps

**Current Implementation**:
- Basic gPTP message parsing (Sync, Follow_Up, Announce)
- Timestamp extraction
- Basic TLV parsing

**Missing (per spec.md FR-044 to FR-049)**:
- ✗ Follow_Up Information TLV (type 0x0003) with rate ratio extraction
- ✗ Organization Extension TLV parsing
- ✗ Graceful handling of unknown TLV types
- ✗ TLV length validation

**Implementation Plan**:
```cpp
namespace wadjet::protocols::gptp {

/// @brief gPTP TLV types (IEEE 1588 / 802.1AS)
enum class TlvType : std::uint16_t {
    Reserved = 0x0000,
    Management = 0x0001,
    ManagementErrorStatus = 0x0002,
    OrganizationExtension = 0x0003,
    // 802.1AS-specific:
    FollowUpInformation = 0x0003,  // Note: Same value as OrgExtension, differentiated by context
    // ... (complete per IEEE 1588 Table 32)
};

/// @brief Follow_Up Information TLV (802.1AS)
struct FollowUpInformationTlv {
    std::uint16_t tlv_type = 0x0003;
    std::uint16_t length_field;
    std::array<std::uint8_t, 3> organization_id;  // IEEE 802.1 OUI: 00-80-C2
    std::array<std::uint8_t, 3> organization_sub_type;
    std::int32_t cumulative_scaled_rate_offset;  // Rate ratio
    std::uint16_t gm_time_base_indicator;
    // ... (complete per 802.1AS)
};

/// @brief Generic TLV parser
std::variant<FollowUpInformationTlv, OrganizationExtensionTlv, UnknownTlv> 
parse_tlv(PacketView data);

}  // namespace wadjet::protocols::gptp
```

**Complexity**: Medium (TLV variant handling, need `std::variant` or tagged union)

---

## 6. Cross-Protocol Validation Architecture

### 6.1 Research Question

How to validate protocol layering (Ethernet → IP → UDP/TCP → App) without tight coupling?

### 6.2 Design Pattern: Chain of Responsibility

**Concept**: Each decoder validates its layer, passes context to next layer

```cpp
namespace wadjet::protocols {

/// @brief Validation result
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    void add_warning(std::string msg) { warnings.push_back(std::move(msg)); }
    void add_error(std::string msg) { 
        errors.push_back(std::move(msg)); 
        valid = false;
    }
};

/// @brief Protocol validator interface
class IProtocolValidator {
public:
    virtual ~IProtocolValidator() = default;
    
    /// @brief Validate protocol layer
    virtual ValidationResult validate(const DecodeContext& ctx) const = 0;
};

/// @brief Cross-layer validator (orchestrates validators)
class ProtocolLayerValidator {
public:
    /// @brief Validation modes
    enum class Mode {
        Lenient,  ///< Log warnings, continue
        Strict    ///< Stop on first error
    };
    
    explicit ProtocolLayerValidator(Mode mode = Mode::Lenient);
    
    /// @brief Validate full protocol stack
    ValidationResult validate_stack(const DecodeContext& ctx) const;
    
private:
    Mode mode_;
    
    // Layer-specific validators
    bool validate_ethernet_ipv4(const DecodeContext& ctx, ValidationResult& result) const;
    bool validate_ipv4_udp_tcp(const DecodeContext& ctx, ValidationResult& result) const;
    bool validate_checksum_chain(const DecodeContext& ctx, ValidationResult& result) const;
    bool validate_length_chain(const DecodeContext& ctx, ValidationResult& result) const;
};

}  // namespace wadjet::protocols
```

**Validation Checks**:
1. **Layering**: Ethernet EtherType → IPv4 → Protocol field → UDP/TCP
2. **Length consistency**: Ethernet frame length ≥ IPv4 total_length ≥ UDP/TCP length
3. **Checksum chain**: IPv4 checksum → UDP/TCP checksum (if enabled)
4. **Fragmentation**: IPv4 fragments must reassemble to valid higher-layer protocol

**Complexity**: Low (simple validation rules, no complex dependencies)

---

## 7. Fuzz Testing Strategy

### 7.1 Research: libFuzzer Best Practices

**AFL/libFuzzer Recommendations**:
- **Corpus seeding**: Start with valid PCAP samples (pcap_samples/)
- **Dictionary**: Protocol-specific dictionaries (magic numbers, service IDs, etc.)
- **Coverage-guided**: Use SanitizerCoverage to maximize edge coverage
- **Run duration**: Until 90% edge coverage or 24 hours, whichever first

**Fuzz Harness Pattern**:
```cpp
// fuzz/fuzz_tcp_options.cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < tcp::MIN_HEADER_SIZE || size > tcp::MAX_HEADER_SIZE) {
        return 0;  // Invalid size, skip
    }
    
    PacketView view(std::span{reinterpret_cast<const std::byte*>(data), size});
    DecodeContext ctx;
    ctx.layer_info.ip_protocol = static_cast<uint8_t>(IpProtocol::TCP);
    
    tcp::TcpDecoder decoder;
    auto result = decoder.decode(view, ctx);
    
    // If decode succeeds, validate options parsing
    if (result) {
        for (const auto& opt : result->options) {
            // Access option data (should not crash)
            [[maybe_unused]] auto mss = opt.mss();
            [[maybe_unused]] auto wscale = opt.window_scale();
            [[maybe_unused]] auto ts = opt.timestamps();
        }
    }
    
    return 0;
}
```

**Decision**: Run fuzzers until 90% edge coverage or 24 hours
- 90% edge coverage is realistic goal (100% may include unreachable error paths)
- 24 hours prevents infinite CI/CD jobs
- Trade-off: May miss rare bugs, but balances thoroughness with practicality

---

## 8. Summary of Architectural Decisions

### 8.1 TCP State Machine

| Decision | Choice | Rationale | Trade-off |
|----------|--------|-----------|-----------|
| **Data Structure** | Hash map (5-tuple key) | O(1) lookup, automotive <10k connections | Higher memory vs. RB-tree |
| **Out-of-Order Buffer** | Fixed 16-segment array | Bounded memory, handles typical reordering | May drop excessive OOO segments |
| **Connection Timeout** | 2 min incomplete, 30s TIME_WAIT | Balances diagnostic sessions with cleanup | 30s < 2*MSL (RFC 793), but acceptable for passive analysis |
| **State Machine** | Simplified RFC 793 | Passive observer (no retransmit timers) | Not a full TCP stack, only analysis |

### 8.2 IPv4 Fragmentation

| Decision | Choice | Rationale | Trade-off |
|----------|--------|-----------|-----------|
| **Reassembly Timeout** | 30 seconds | Automotive low-latency networks | <60s RFC 791, but faster cleanup |
| **Overlap Handling** | First-fragment-wins | Security + simplicity | Ignores later corrections (rare) |
| **Max Datagrams** | 1024 concurrent | DoS prevention (64 MB max memory) | Drops excessive fragmentation |
| **Algorithm** | RFC 815 hole descriptors | Memory-efficient vs. bitmap | Slightly complex vs. array |

### 8.3 SOME/IP-TP Segmentation

| Decision | Choice | Rationale | Trade-off |
|----------|--------|-----------|-----------|
| **Max Message Size** | 16 MB | Realistic firmware updates | <4GB AUTOSAR limit, but prevents DoS |
| **Timeout** | 5 seconds | AUTOSAR PRS_SOMEIP_00191 | Longer than needed (automotive <200ms transfer), but handles congestion |
| **Out-of-Order** | Vector reassembly | Handles UDP-based reordering | More complex than append-only |
| **Key** | (service, method, client, session) | Matches SOME/IP semantics | More keys than minimal, but prevents confusion |

### 8.4 UDP Checksum Validation

| Decision | Choice | Rationale | Trade-off |
|----------|--------|-----------|-----------|
| **Default Mode** | Warning-only | Legacy ECU compatibility + integrity | Corrupted packets pass through (but logged) |
| **Modes** | Disabled/Warning/Strict | Flexibility for different use cases | Mode complexity |
| **Zero Checksum** | Allow for IPv4, forbid for IPv6 | RFC 768 + RFC 2460 compliance | Version-specific logic |

### 8.5 Protocol Compliance

| Protocol | Gap | Complexity | Implementation Effort |
|----------|-----|------------|----------------------|
| **DoIP** | Power mode, entity status, alive check | Low | 3 days (data-driven) |
| **UDS** | Complete NRC set (50+ codes) | Low | 2 days (lookup table) |
| **gPTP** | TLV parsing (Follow_Up Info, Org Ext) | Medium | 3 days (variant handling) |

---

## 9. Open Questions & Risks

### 9.1 Open Questions for Implementation

1. **IPv4 Fragment Cache Eviction Policy**:
   - Q: FIFO, LRU, or random eviction when max_concurrent_datagrams exceeded?
   - **Recommendation**: FIFO (simpler, automotive fragments typically complete quickly)

2. **TCP Connection Cleanup Trigger**:
   - Q: Background timer thread or explicit cleanup calls?
   - **Recommendation**: Explicit cleanup (avoid threading complexity, caller controls timing)

3. **SOME/IP-TP Segment Deduplication**:
   - Q: Should duplicate segments (same offset) be accepted or rejected?
   - **Recommendation**: Reject duplicates with warning (prevents offset-based attacks)

4. **Cross-Protocol Validation Performance**:
   - Q: Run validations inline or post-processing?
   - **Recommendation**: Inline (immediate feedback), with optional disable flag for performance

### 9.2 Risks & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **Memory exhaustion** (fragmentation DoS) | High | Medium | Max concurrent limits (1024 fragments, 256 TP messages) |
| **Performance degradation** (>5% overhead) | Medium | Low | Profile before optimization, disable validation flags for perf-critical paths |
| **Standards deviation** (automotive vs. RFC) | Low | Low | Document all deviations (e.g., 30s timeout vs. 60s RFC 791) |
| **Backward compatibility break** (existing API) | High | Low | Constitution Principle IV enforced (no API changes, only additions) |

---

## 10. Next Steps (Phase 1)

Per plan.md Phase 1, the following artifacts should be created:

1. **data-model.md**: Entity definitions (TcpConnection, Ipv4Fragment, SomeipTpMessage, etc.)
2. **contracts/**: Updated decoder APIs (if any additions needed)
3. **quickstart.md**: Usage examples for new features

**Dependencies Resolved**: All research complete, ready to proceed to Phase 1 design.

---

**Research Complete**: 2026-01-15  
**Next Phase**: Phase 1 - Design & Contracts (data-model.md, quickstart.md, contracts/)

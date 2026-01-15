
# Data Model: Protocol Completeness

**Feature**: M13 Protocol Completeness  
**Date**: 2026-01-16  
**Status**: Phase 1 - Design
**Purpose**: Entity definitions and data structures for protocol gap completion

---

## Overview

This document defines all data entities, structures, and relationships for the protocol completeness milestone. Entities are organized by protocol layer and comply with Constitution Principle II (zero-copy where possible).

// ...existing content for TcpConnection, Ipv4Fragment, SomeipTpMessage, etc...

**Feature**: M13 Protocol Completeness  
**Date**: 2026-01-15  
**Status**: Phase 1 - Design  
**Purpose**: Entity definitions and data structures for protocol gap completion

---

## Overview

This document defines all data entities, structures, and relationships for the protocol completeness milestone. Entities are organized by protocol layer and comply with Constitution Principle II (zero-copy where possible).

---

## 1. TCP Protocol Entities

### 1.1 TcpState Enumeration

**Purpose**: RFC 793 TCP connection states for passive connection tracking

```cpp
namespace wadjet::protocols::tcp {

/// @brief TCP connection states (RFC 793 Section 3.2)
enum class TcpState : std::uint8_t {
    CLOSED = 0,       ///< No connection state
    LISTEN,           ///< Passive open (server waiting for SYN)
    SYN_SENT,         ///< Active open (client sent SYN, awaiting SYN-ACK)
    SYN_RECEIVED,     ///< Server received SYN, sent SYN-ACK, awaiting ACK
    ESTABLISHED,      ///< Connection established, data transfer phase
    FIN_WAIT_1,       ///< Active close initiated (sent FIN)
    FIN_WAIT_2,       ///< FIN acknowledged, waiting for remote FIN
    CLOSE_WAIT,       ///< Remote initiated close (received FIN)
    CLOSING,          ///< Simultaneous close (both sides sent FIN)
    LAST_ACK,         ///< Waiting for final ACK after sending FIN
    TIME_WAIT,        ///< 2*MSL timeout (30s automotive-optimized)
};

/// @brief Convert state to string
constexpr std::string_view to_string(TcpState state);

}  // namespace wadjet::protocols::tcp
```

**States Used in Passive Analysis**:
- **CLOSED**: Initial state, no packets seen
- **SYN_SENT**: Client sends SYN
- **SYN_RECEIVED**: Server sends SYN-ACK
- **ESTABLISHED**: Connection active (most common state for DoIP/UDS)
- **FIN_WAIT_1/2, CLOSE_WAIT, LAST_ACK**: Teardown states
- **TIME_WAIT**: 30-second cleanup (automotive-optimized, vs. 60s RFC 793)

**Not Used** (server-side only):
- **LISTEN**: Passive observer doesn't see listen() syscall

### 1.2 ConnectionKey Structure

**Purpose**: Unique identifier for TCP connections (5-tuple)

```cpp
namespace wadjet::protocols::tcp {

/// @brief 5-tuple connection key for hash table lookup
struct ConnectionKey {
    IPv4Address src_ip;           ///< Source IP address
    IPv4Address dst_ip;           ///< Destination IP address
    std::uint16_t src_port;       ///< Source port
    std::uint16_t dst_port;       ///< Destination port
    std::uint8_t protocol;        ///< Protocol (always 6 for TCP)
    
    /// @brief Equality comparison
    bool operator==(const ConnectionKey&) const = default;
    
    /// @brief Hash function for std::unordered_map
    struct Hash {
        std::size_t operator()(const ConnectionKey& key) const noexcept;
    };
};

}  // namespace wadjet::protocols::tcp
```

**Design Rationale**:
- **5-tuple**: Industry standard (Wireshark, tcpdump, Linux kernel)
- **Protocol field**: Included for future extensibility (SCTP, etc.)
- **Hash function**: FNV-1a for fast, well-distributed hashing

### 1.3 TcpSegment Structure

**Purpose**: Out-of-order TCP segment buffer entry

```cpp
namespace wadjet::protocols::tcp {

/// @brief Out-of-order TCP segment
struct TcpSegment {
    std::uint32_t seq_num;                               ///< Sequence number
    std::uint32_t length;                                ///< Payload length (bytes)
    std::chrono::steady_clock::time_point arrival_time;  ///< When segment arrived
    PacketView data;                                     ///< Zero-copy view of payload
    
    /// @brief Check if this segment overlaps with another
    bool overlaps(const TcpSegment& other) const;
    
    /// @brief Get end sequence number (seq_num + length)
    std::uint32_t end_seq() const { return seq_num + length; }
};

}  // namespace wadjet::protocols::tcp
```

**Zero-Copy Compliance**:
- `PacketView data`: Reference to original packet (no copy)
- Caller must ensure original packet lifetime exceeds analysis

### 1.4 TcpConnection Structure

**Purpose**: Complete TCP connection state and statistics

```cpp
namespace wadjet::protocols::tcp {

/// @brief TCP connection state and tracking data
struct TcpConnection {
    // Connection identification
    ConnectionKey key;
    
    // State machine
    TcpState state = TcpState::CLOSED;
    
    // Sequence number tracking (bi-directional)
    std::uint32_t client_seq_next = 0;   ///< Next expected seq (client→server)
    std::uint32_t server_seq_next = 0;   ///< Next expected seq (server→client)
    std::uint32_t client_isn = 0;        ///< Initial Sequence Number (client)
    std::uint32_t server_isn = 0;        ///< Initial Sequence Number (server)
    
    // Window tracking
    std::uint16_t client_window = 0;         ///< Advertised window (client)
    std::uint16_t server_window = 0;         ///< Advertised window (server)
    // Timing information
    std::chrono::steady_clock::time_point first_seen;      ///< First packet timestamp
    std::uint64_t retransmissions_client = 0;    ///< Detected retransmissions (client)
    std::uint64_t retransmissions_server = 0;    ///< Detected retransmissions (server)
    bool is_expired(std::chrono::steady_clock::time_point now,
                   std::chrono::seconds timeout) const;
    std::uint32_t effective_window_server() const;
    

}  // namespace wadjet::protocols::tcp
```

**Bi-Directional Tracking Rationale**:
- DoIP/UDS: Diagnostic tester ↔ ECU requires tracking both directions
- Each direction has independent sequence space

**Memory Usage**:
- Fixed-size structure: ~300 bytes per connection
- 10,000 concurrent connections: ~3 MB (acceptable for automotive)

### 1.5 TcpOption Structures

**Purpose**: Parsed TCP header options (RFC 793, RFC 7323, RFC 2018)

```cpp
namespace wadjet::protocols::tcp {

/// @brief TCP option kinds
enum class TcpOptionKind : std::uint8_t {
    EndOfOptions = 0,      ///< End of option list (1 byte)
    NoOperation = 1,       ///< Padding/alignment (1 byte)
    MaxSegmentSize = 2,    ///< MSS (4 bytes total)
    WindowScale = 3,       ///< Window scale factor (3 bytes total, RFC 7323)
    SackPermitted = 4,     ///< SACK permitted (2 bytes total, RFC 2018)
    Sack = 5,              ///< Selective ACK (variable length, RFC 2018)
    Timestamps = 8,        ///< Timestamps (10 bytes total, RFC 7323)
};

/// @brief Parsed TCP option (generic TLV)
struct TcpOption {
    TcpOptionKind kind;
    std::vector<std::byte> data;  ///< Option data (excludes kind and length bytes)
    
    // Type-specific accessors
    
    /// @brief Get MSS value (if kind == MaxSegmentSize)
    std::optional<std::uint16_t> mss() const;
    
    /// @brief Get window scale (if kind == WindowScale)
    std::optional<std::uint8_t> window_scale() const;
    
    /// @brief Get timestamps (if kind == Timestamps)
    /// @return {TSval, TSecr} pair
    std::optional<std::pair<std::uint32_t, std::uint32_t>> timestamps() const;
    
    /// @brief Get SACK blocks (if kind == Sack)
    std::vector<std::pair<std::uint32_t, std::uint32_t>> sack_blocks() const;
};

}  // namespace wadjet::protocols::tcp
```

**Design Choice**: Generic TLV + type-specific accessors
- Handles unknown/future options gracefully
- Type-safe access via `std::optional`

---

## 2. IPv4 Protocol Entities

### 2.1 FragmentKey Structure

**Purpose**: Unique identifier for IPv4 datagram fragments

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief Fragment cache key (RFC 791)
struct FragmentKey {
    IPv4Address src_ip;           ///< Source IP address
    IPv4Address dst_ip;           ///< Destination IP address
    std::uint8_t protocol;        ///< Protocol (TCP=6, UDP=17, etc.)
    std::uint16_t identification; ///< Fragment identification field
    
    bool operator==(const FragmentKey&) const = default;
    
    struct Hash {
        std::size_t operator()(const FragmentKey& key) const noexcept;
    };
};

}  // namespace wadjet::protocols::ipv4
```

**RFC 791 Requirement**: Fragments identified by (source, destination, protocol, identification)

### 2.2 FragmentHole Structure

**Purpose**: RFC 815 hole descriptor for efficient reassembly

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief Hole descriptor (RFC 815)
struct FragmentHole {
    std::uint16_t start_offset;  ///< Hole start (bytes)
    std::uint16_t end_offset;    ///< Hole end (bytes, exclusive)
    
    /// @brief Check if hole is filled by fragment at offset with length
    bool is_filled_by(std::uint16_t fragment_offset, std::uint16_t fragment_length) const;
};

}  // namespace wadjet::protocols::ipv4
```

**Algorithm**: RFC 815 IP Datagram Reassembly
- Start with single hole: [0, ∞)
- On fragment arrival: remove filled portion of hole
- Reassembly complete when hole list empty

### 2.3 FragmentBuffer Structure

**Purpose**: Reassembly buffer for fragmented IPv4 datagram

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief Fragment reassembly buffer
struct FragmentBuffer {
    // Data storage
    std::array<std::byte, 65536> data;  ///< Max IP datagram size (64KB)
    
    // Reassembly tracking
    std::vector<FragmentHole> holes;    ///< Holes to fill (RFC 815)
    std::uint16_t total_length = 0;     ///< Final datagram length (known when MF=0 fragment arrives)
    bool have_last_fragment = false;    ///< Received fragment with MF=0
    
    // Timing
    std::chrono::steady_clock::time_point first_arrival;  ///< First fragment timestamp
    std::chrono::steady_clock::time_point last_arrival;   ///< Most recent fragment timestamp
    
    // Statistics
    std::uint16_t fragments_received = 0;  ///< Count of fragments
    
    // Helper methods
    
    /// @brief Check if reassembly is complete
    bool is_complete() const { return have_last_fragment && holes.empty(); }
    
    /// @brief Check if buffer is expired
    bool is_expired(std::chrono::steady_clock::time_point now,
                   std::chrono::seconds timeout) const;
};

}  // namespace wadjet::protocols::ipv4
```

**Memory Efficiency**:
- 64 KB data buffer (max IPv4 datagram size)
- Hole list: typically 2-4 entries (small overhead)
- Total: ~64 KB per fragmented datagram

### 2.4 Ipv4Option Structures

**Purpose**: Parsed IPv4 header options

```cpp
namespace wadjet::protocols::ipv4 {

/// @brief IPv4 option types (IANA IP Option Numbers)
enum class Ipv4OptionType : std::uint8_t {
    EndOfOptions = 0,       ///< End of option list (1 byte)
    NoOperation = 1,        ///< Padding (1 byte)
    Security = 2,           ///< Security (deprecated, variable)
    RecordRoute = 7,        ///< Record Route (variable)
    Timestamp = 68,         ///< Internet Timestamp (variable)
    LooseSourceRoute = 131, ///< Loose Source Route (variable)
    StrictSourceRoute = 137,///< Strict Source Route (variable)
    RouterAlert = 148,      ///< Router Alert (RFC 2113, 4 bytes)
};

/// @brief Generic IPv4 option (TLV format)
struct Ipv4Option {
    Ipv4OptionType type;
    std::vector<std::byte> data;  ///< Option data (excludes type and length)
    
    // Type-specific accessors
    
    /// @brief Get Router Alert value (if type == RouterAlert)
    std::optional<std::uint16_t> router_alert_value() const;
    
    /// @brief Get recorded route addresses (if type == RecordRoute)
    std::vector<IPv4Address> record_route_addresses() const;
    
    /// @brief Get timestamp data (if type == Timestamp)
    struct TimestampData {
        std::uint8_t pointer;
        std::uint8_t overflow;
        std::uint8_t flags;
        std::vector<std::uint32_t> timestamps;
    };
    std::optional<TimestampData> timestamp_data() const;
};

}  // namespace wadjet::protocols::ipv4
```

**Automotive Relevance**:
- **Router Alert**: Used by IGMP, PIM (multicast protocols)
- **Timestamp**: Rare, but diagnostic tools may use for latency measurement

---

## 3. UDP Protocol Entities

### 3.1 ChecksumMode Enumeration

**Purpose**: UDP checksum validation modes for backward compatibility

```cpp
namespace wadjet::protocols::udp {

/// @brief UDP checksum validation modes
enum class ChecksumMode : std::uint8_t {
    Disabled,  ///< Skip checksum validation entirely
    Warning,   ///< Validate and log warnings, but don't drop packets (DEFAULT)
    Strict     ///< Validate and mark packets with bad checksum as invalid
};

}  // namespace wadjet::protocols::udp
```

**Default: Warning-Only Mode**:
- Logs checksum errors without dropping packets
- Balances integrity checking with legacy ECU compatibility

### 3.2 UdpChecksumResult Structure

**Purpose**: Checksum validation result with diagnostic information

```cpp
namespace wadjet::protocols::udp {

/// @brief UDP checksum validation result
struct UdpChecksumResult {
    bool valid = true;                ///< Checksum is valid
    bool zero_checksum = false;       ///< Checksum field is 0x0000 (allowed for IPv4)
    std::uint16_t computed = 0;       ///< Computed checksum value
    std::uint16_t received = 0;       ///< Received checksum value
    
    /// @brief Was validation performed?
    bool validated() const { return computed != 0 || zero_checksum; }
};

}  // namespace wadjet::protocols::udp
```

**Usage**: Returned by `UdpChecksumValidator::validate()`, attached to `UdpHeader`

---

## 4. SOME/IP Protocol Entities

### 4.1 SomeipTpHeader Structure

**Purpose**: SOME/IP-TP (Transport Protocol) header for segmented messages

```cpp
namespace wadjet::protocols::someip {

/// @brief SOME/IP-TP message type flag (bit 5 of message_type byte)
inline constexpr std::uint8_t TP_FLAG = 0x20;

/// @brief More segments flag (bit 0 of message_type byte, when TP_FLAG is set)
inline constexpr std::uint8_t MORE_SEGMENTS_FLAG = 0x01;

/// @brief SOME/IP-TP segment header
struct SomeipTpHeader {
    std::uint32_t offset;           ///< Byte offset in complete message
    bool more_segments;             ///< True if more segments follow
    std::uint32_t segment_length;   ///< This segment's payload length (bytes)
    
    /// @brief Parse TP header from SOME/IP message
    static std::optional<SomeipTpHeader> parse(const SomeIpHeader& hdr, PacketView data);
    
    /// @brief Check if SOME/IP message is TP
    static bool is_tp_message(const SomeIpHeader& hdr);
};

}  // namespace wadjet::protocols::someip
```

**AUTOSAR Compliance**: Per PRS_SOMEIP Section 4.2.1

### 4.2 SomeipTpMessage Structure

**Purpose**: Reassembly buffer for segmented SOME/IP-TP messages

```cpp
namespace wadjet::protocols::someip {

/// @brief SOME/IP-TP message reassembly buffer
struct SomeipTpMessage {
    // Message identification
    std::uint16_t service_id;
    std::uint16_t method_id;
    std::uint16_t client_id;
    std::uint16_t session_id;
    
    // Reassembly data
    std::vector<std::byte> data;         ///< Reassembled payload (up to 16 MB)
    std::uint32_t total_length = 0;      ///< Total message length (known when more_segments=false)
    std::uint32_t bytes_received = 0;    ///< Bytes received so far
    bool have_last_segment = false;      ///< Received segment with more_segments=false
    
    // Timing
    std::chrono::steady_clock::time_point first_segment_time;  ///< First segment arrival
    std::chrono::steady_clock::time_point last_segment_time;   ///< Most recent segment arrival
    
    // Statistics
    std::uint16_t segments_received = 0;
    
    // Helper methods
    
    /// @brief Check if reassembly is complete
    bool is_complete() const;
    
    /// @brief Check if message is expired (5s timeout per PRS_SOMEIP_00191)
    bool is_expired(std::chrono::steady_clock::time_point now,
                   std::chrono::seconds timeout) const;
};

}  // namespace wadjet::protocols::someip
```

**Maximum Message Size**: 16 MB (realistic automotive limit, prevents DoS)

---

## 5. SOME/IP-SD Protocol Entities

### 5.1 SdEntryType Enumeration

**Purpose**: SOME/IP Service Discovery entry types

```cpp
namespace wadjet::protocols::someip_sd {

/// @brief SD entry types (AUTOSAR PRS_SOMEIPSD Table 4.1)
enum class SdEntryType : std::uint8_t {
    FindService = 0x00,              ///< Find Service
    OfferService = 0x01,             ///< Offer Service
    StopOfferService = 0x01,         ///< Stop Offer Service (TTL=0)
    SubscribeEventgroup = 0x06,      ///< Subscribe Eventgroup
    StopSubscribeEventgroup = 0x06,  ///< Stop Subscribe (TTL=0)
    SubscribeEventgroupAck = 0x07,   ///< Subscribe Eventgroup Acknowledgment
};

}  // namespace wadjet::protocols::someip_sd
```

**Note**: `OfferService` vs. `StopOfferService` distinguished by TTL field (0 = stop)

### 5.2 SdEntry Structures

**Purpose**: Parsed Service Discovery entries

```cpp
namespace wadjet::protocols::someip_sd {

/// @brief Base SD entry (common fields)
struct SdEntryBase {
    SdEntryType type;
    std::uint8_t index_1st_option;   ///< Index of first option (0xFF = no options)
    std::uint8_t index_2nd_option;   ///< Index of second option (0xFF = no options)
    std::uint8_t num_options_1;      ///< Number of options referenced by index_1st
    std::uint8_t num_options_2;      ///< Number of options referenced by index_2nd
    std::uint16_t service_id;
    std::uint16_t instance_id;
    std::uint8_t major_version;
    std::uint32_t ttl;               ///< Time to live (0 = stop, 0xFFFFFF = infinite)
};

/// @brief Service entry (FindService, OfferService)
struct SdServiceEntry : SdEntryBase {
    std::uint32_t minor_version;
    
    /// @brief Is this a StopOffer? (TTL=0)
    bool is_stop_offer() const { return ttl == 0; }
};

/// @brief Eventgroup entry (SubscribeEventgroup, SubscribeEventgroupAck)
struct SdEventgroupEntry : SdEntryBase {
    std::uint16_t eventgroup_id;
    std::uint8_t counter;            ///< Counter for duplicate detection
    
    /// @brief Is this a StopSubscribe? (TTL=0)
    bool is_stop_subscribe() const { return ttl == 0; }
};

/// @brief Variant for all entry types
using SdEntry = std::variant<SdServiceEntry, SdEventgroupEntry>;

}  // namespace wadjet::protocols::someip_sd
```

### 5.3 SdOption Structures

**Purpose**: SOME/IP-SD configuration options

```cpp
namespace wadjet::protocols::someip_sd {

/// @brief SD option types
enum class SdOptionType : std::uint8_t {
    Configuration = 0x01,
    LoadBalancing = 0x02,
    IPv4Endpoint = 0x04,
    IPv6Endpoint = 0x06,
    IPv4Multicast = 0x14,
    IPv6Multicast = 0x16,
    IPv4SdEndpoint = 0x24,
    IPv6SdEndpoint = 0x26,
};

/// @brief IPv4 Endpoint option
struct SdIpv4EndpointOption {
    SdOptionType type = SdOptionType::IPv4Endpoint;
    IPv4Address address;
    std::uint16_t port;
    std::uint8_t protocol;  ///< 0x06=TCP, 0x11=UDP
};

/// @brief IPv4 Multicast option
struct SdIpv4MulticastOption {
    SdOptionType type = SdOptionType::IPv4Multicast;
    IPv4Address address;
    std::uint16_t port;
    std::uint8_t protocol;
};

/// @brief Configuration option
struct SdConfigurationOption {
    SdOptionType type = SdOptionType::Configuration;
    std::vector<std::byte> config_string;  ///< Configuration data
};

/// @brief Load Balancing option
struct SdLoadBalancingOption {
    SdOptionType type = SdOptionType::LoadBalancing;
    std::uint16_t priority;
    std::uint16_t weight;
};

/// @brief Variant for all option types
using SdOption = std::variant<
    SdIpv4EndpointOption,
    SdIpv4MulticastOption,
    SdConfigurationOption,
    SdLoadBalancingOption
>;

}  // namespace wadjet::protocols::someip_sd
```

### 5.4 SdMessage Structure

**Purpose**: Complete SD message with entries and options

```cpp
namespace wadjet::protocols::someip_sd {

/// @brief Complete SD message
struct SdMessage {
    // SD header flags
    bool reboot_flag;    ///< Reboot flag
    bool unicast_flag;   ///< Unicast flag
    
    // Entries and options
    std::vector<SdEntry> entries;
    std::vector<SdOption> options;
    
    // Helper methods
    
    /// @brief Get options linked to specific entry
    std::vector<SdOption> get_options_for_entry(std::size_t entry_index) const;
    
    /// @brief Validate entry-option consistency
    bool validate_entry_option_links() const;
};

}  // namespace wadjet::protocols::someip_sd
```

**Complexity**: Entry-option linking via index fields (Index1, Index2, NumOpt1, NumOpt2)

---

## 6. DoIP Protocol Entities

### 6.1 DiagnosticPowerMode Enumeration

**Purpose**: DoIP diagnostic power mode states (ISO 13400-2)

```cpp
namespace wadjet::protocols::doip {

/// @brief DoIP diagnostic power mode states (ISO 13400-2 Amendment 1)
enum class DiagnosticPowerMode : std::uint8_t {
    NotReady = 0x00,      ///< ECU not ready for diagnostics
    Ready = 0x01,         ///< ECU ready for diagnostics
    NotSupported = 0x02   ///< Power mode information not supported
};

}  // namespace wadjet::protocols::doip
```

### 6.2 DoipPowerModeMessage Structure

**Purpose**: Diagnostic power mode request/response

```cpp
namespace wadjet::protocols::doip {

/// @brief Diagnostic Power Mode Information (0x4003 request, 0x4004 response)
struct DoipPowerModeMessage {
    std::uint16_t payload_type;        ///< 0x4003 or 0x4004
    DiagnosticPowerMode power_mode;    ///< Power mode value (response only)
    
    /// @brief Is this a request?
    bool is_request() const { return payload_type == 0x4003; }
    
    /// @brief Is this a response?
    bool is_response() const { return payload_type == 0x4004; }
};

}  // namespace wadjet::protocols::doip
```

### 6.3 DoipEntityStatusMessage Structure

**Purpose**: DoIP entity status information

```cpp
namespace wadjet::protocols::doip {

/// @brief DoIP node type
enum class NodeType : std::uint8_t {
    DoipGateway = 0x00,
    DoipNode = 0x01
};

/// @brief DoIP Entity Status Information (0x4001 request, 0x4002 response)
struct DoipEntityStatusMessage {
    std::uint16_t payload_type;     ///< 0x4001 or 0x4002
    NodeType node_type;             ///< Node type (response only)
    std::uint8_t max_open_sockets;  ///< Maximum concurrent TCP sockets
    std::uint8_t current_open_sockets;
    std::uint32_t max_data_size;    ///< Maximum diagnostic message size
};

}  // namespace wadjet::protocols::doip
```

### 6.4 DoipNackCode Enumeration

**Purpose**: DoIP negative acknowledgment codes

```cpp
namespace wadjet::protocols::doip {

/// @brief DoIP NACK codes (ISO 13400-2 Table 21)
enum class DoipNackCode : std::uint8_t {
    IncorrectPatternFormat = 0x00,
    UnknownPayloadType = 0x01,
    MessageTooLarge = 0x02,
    OutOfMemory = 0x03,
    InvalidPayloadLength = 0x04,
};

/// @brief DoIP Generic Header NACK (0x0000)
struct DoipHeaderNack {
    DoipNackCode nack_code;
};

}  // namespace wadjet::protocols::doip
```

---

## 7. UDS Protocol Entities

### 7.1 UdsNegativeResponseCode Enumeration

**Purpose**: Complete UDS NRC set (ISO 14229-1 Table A.1)

```cpp
namespace wadjet::protocols::uds {

/// @brief UDS Negative Response Codes (ISO 14229-1 Table A.1)
enum class NegativeResponseCode : std::uint8_t {
    // General reject
    GeneralReject = 0x10,
    ServiceNotSupported = 0x11,
    SubFunctionNotSupported = 0x12,
    IncorrectMessageLengthOrInvalidFormat = 0x13,
    ResponseTooLong = 0x14,
    
    // Busy - temporary errors
    BusyRepeatRequest = 0x21,
    
    // Conditions not correct
    ConditionsNotCorrect = 0x22,
    RequestSequenceError = 0x24,
    NoResponseFromSubnetComponent = 0x25,
    FailurePreventsExecutionOfRequestedAction = 0x26,
    
    // Request out of range
    RequestOutOfRange = 0x31,
    
    // Security access
    SecurityAccessDenied = 0x33,
    InvalidKey = 0x35,
    ExceedNumberOfAttempts = 0x36,
    RequiredTimeDelayNotExpired = 0x37,
    
    // Upload/Download
    UploadDownloadNotAccepted = 0x70,
    TransferDataSuspended = 0x71,
    GeneralProgrammingFailure = 0x72,
    WrongBlockSequenceCounter = 0x73,
    
    // Subfunction not supported in active session
    SubFunctionNotSupportedInActiveSession = 0x7E,
    ServiceNotSupportedInActiveSession = 0x7F,
    
    // Voltage/RPM
    RpmTooHigh = 0x81,
    RpmTooLow = 0x82,
    EngineIsRunning = 0x83,
    EngineIsNotRunning = 0x84,
    EngineRunTimeTooLow = 0x85,
    TemperatureTooHigh = 0x86,
    TemperatureTooLow = 0x87,
    VehicleSpeedTooHigh = 0x88,
    VehicleSpeedTooLow = 0x89,
    ThrottlePedalTooHigh = 0x8A,
    ThrottlePedalTooLow = 0x8B,
    TransmissionRangeNotInNeutral = 0x8C,
    TransmissionRangeNotInGear = 0x8D,
    BrakeSwitchNotClosed = 0x8F,
    ShifterLeverNotInPark = 0x90,
    TorqueConverterClutchLocked = 0x91,
    VoltageTooHigh = 0x92,
    VoltageTooLow = 0x93,
};

}  // namespace wadjet::protocols::uds
```

**Total**: 50+ codes covering all ISO 14229-1 NRCs

### 7.2 NrcClass Enumeration

**Purpose**: Classify NRCs as temporary or permanent

```cpp
namespace wadjet::protocols::uds {

/// @brief NRC classification
enum class NrcClass : std::uint8_t {
    Temporary,   ///< Retry may succeed (e.g., BusyRepeatRequest, RpmTooHigh)
    Permanent    ///< Retry will fail (e.g., ServiceNotSupported, SecurityAccessDenied)
};

}  // namespace wadjet::protocols::uds
```

### 7.3 NrcMetadata Structure

**Purpose**: NRC lookup table entry

```cpp
namespace wadjet::protocols::uds {

/// @brief NRC metadata for lookup table
struct NrcMetadata {
    NegativeResponseCode code;
    std::string_view description;
    NrcClass classification;
    
    /// @brief Get human-readable description
    static const NrcMetadata* lookup(NegativeResponseCode code);
};

/// @brief Static NRC lookup table (defined in uds_decoder.cpp)
extern const std::array<NrcMetadata, 50+> NRC_TABLE;

}  // namespace wadjet::protocols::uds
```

### 7.4 UdsNegativeResponse Structure

**Purpose**: Parsed UDS negative response (0x7F SID NRC)

```cpp
namespace wadjet::protocols::uds {

/// @brief UDS Negative Response (0x7F SID NRC format)
struct UdsNegativeResponse {
    std::uint8_t response_code = 0x7F;    ///< Always 0x7F
    std::uint8_t service_id;              ///< Service ID that failed
    NegativeResponseCode nrc;             ///< Negative response code
    std::optional<std::uint8_t> sub_function;  ///< Sub-function (if applicable)
    
    /// @brief Get NRC metadata (description, classification)
    const NrcMetadata* get_metadata() const;
    
    /// @brief Is this a temporary error?
    bool is_temporary() const;
};

}  // namespace wadjet::protocols::uds
```

---

## 8. gPTP Protocol Entities

### 8.1 GptpTlvType Enumeration

**Purpose**: gPTP TLV types (IEEE 1588 / 802.1AS)

```cpp
namespace wadjet::protocols::gptp {

/// @brief gPTP TLV types (IEEE 1588 Table 32, 802.1AS extensions)
enum class TlvType : std::uint16_t {
    Reserved = 0x0000,
    Management = 0x0001,
    ManagementErrorStatus = 0x0002,
    OrganizationExtension = 0x0003,
    // Note: FollowUpInformation also uses 0x0003, differentiated by organization ID
};

}  // namespace wadjet::protocols::gptp
```

### 8.2 FollowUpInformationTlv Structure

**Purpose**: Follow_Up Information TLV with rate ratio (802.1AS)

```cpp
namespace wadjet::protocols::gptp {

/// @brief Follow_Up Information TLV (802.1AS)
struct FollowUpInformationTlv {
    std::uint16_t tlv_type = 0x0003;
    std::uint16_t length_field;
    std::array<std::uint8_t, 3> organization_id;     ///< IEEE 802.1 OUI: 00-80-C2
    std::array<std::uint8_t, 3> organization_sub_type;
    std::int32_t cumulative_scaled_rate_offset;      ///< Rate ratio (scaled)
    std::uint16_t gm_time_base_indicator;
    std::uint16_t last_gm_phase_change_high;
    std::uint32_t last_gm_phase_change_low;
    std::int32_t scaled_last_gm_freq_change;
    
    /// @brief Get rate ratio as double
    double get_rate_ratio() const;
};

}  // namespace wadjet::protocols::gptp
```

### 8.3 OrganizationExtensionTlv Structure

**Purpose**: Generic organization extension TLV

```cpp
namespace wadjet::protocols::gptp {

/// @brief Organization Extension TLV (IEEE 1588)
struct OrganizationExtensionTlv {
    std::uint16_t tlv_type = 0x0003;
    std::uint16_t length_field;
    std::array<std::uint8_t, 3> organization_id;
    std::array<std::uint8_t, 3> organization_sub_type;
    std::vector<std::byte> data;  ///< Organization-specific data
};

}  // namespace wadjet::protocols::gptp
```

### 8.4 GptpTlv Variant

**Purpose**: Polymorphic TLV container

```cpp
namespace wadjet::protocols::gptp {

/// @brief Unknown TLV (graceful fallback)
struct UnknownTlv {
    std::uint16_t tlv_type;
    std::uint16_t length_field;
    std::vector<std::byte> data;
};

/// @brief Variant for all TLV types
using GptpTlv = std::variant<
    FollowUpInformationTlv,
    OrganizationExtensionTlv,
    UnknownTlv
>;

}  // namespace wadjet::protocols::gptp
```

---

## 9. Cross-Protocol Validation Entities

### 9.1 ValidationResult Structure

**Purpose**: Protocol validation result with warnings and errors

```cpp
namespace wadjet::protocols {

/// @brief Protocol validation result
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    /// @brief Add warning (doesn't invalidate result)
    void add_warning(std::string msg);
    
    /// @brief Add error (invalidates result)
    void add_error(std::string msg);
    
    /// @brief Has any issues (warnings or errors)?
    bool has_issues() const { return !warnings.empty() || !errors.empty(); }
};

}  // namespace wadjet::protocols
```

### 9.2 ValidationMode Enumeration

**Purpose**: Validation strictness modes

```cpp
namespace wadjet::protocols {

/// @brief Protocol validation modes
enum class ValidationMode : std::uint8_t {
    Lenient,  ///< Log warnings, continue processing
    Strict    ///< Stop on first error
};

}  // namespace wadjet::protocols
```

---

## 10. Entity Relationships

### 10.1 TCP Connection Tracking

```
ConnectionKey (5-tuple)
    ↓
TcpConnection (state + statistics)
    ├─→ TcpState (enum)
    ├─→ TcpSegment[] (out-of-order buffer, max 16)
    └─→ TcpOption[] (parsed from header)
```

### 10.2 IPv4 Fragment Reassembly

```
FragmentKey (src, dst, protocol, id)
    ↓
FragmentBuffer (reassembly state)
    ├─→ FragmentHole[] (RFC 815 holes)
    └─→ std::array<std::byte, 65536> (data buffer)
```

### 10.3 SOME/IP-TP Reassembly

```
TpMessageKey (service, method, client, session)
    ↓
SomeipTpMessage (reassembly state)
    ├─→ std::vector<std::byte> (payload, max 16 MB)
    └─→ timing + statistics
```

### 10.4 SOME/IP-SD Entry-Option Linking

```
SdMessage
    ├─→ SdEntry[] (entries)
    │     ├─→ index_1st_option
    │     ├─→ index_2nd_option
    │     ├─→ num_options_1
    │     └─→ num_options_2
    └─→ SdOption[] (options)
          └─→ indexed by entry fields
```

---

## 11. Memory Budgets

**Per-Connection Memory Usage** (worst case):

| Entity | Size | Max Concurrent | Total Memory |
|--------|------|----------------|--------------|
| `TcpConnection` | ~300 bytes | 10,000 | ~3 MB |
| `FragmentBuffer` | ~64 KB | 1,024 | ~64 MB |
| `SomeipTpMessage` | ~16 MB | 256 | ~4 GB |

**Total Worst Case**: ~4.1 GB (with all limits maxed)

**Typical Automotive Load**: ~100 MB (realistic concurrent usage)

---

## 12. Constitution Compliance

**Principle II (Zero-Copy)**:
- ✅ `TcpSegment::data` uses `PacketView` (zero-copy reference)
- ✅ Fragment reassembly operates on views, copies only on final delivery
- ✅ TP reassembly buffers data (necessary for reassembly), but uses move semantics

**Principle IV (Pluggable Architecture)**:
- ✅ All entities are data structures, no decoder interface changes
- ✅ Trackers are separate classes, can be used independently

**Principle VII (Simplicity)**:
- ✅ Uses standard containers (`std::vector`, `std::array`, `std::unordered_map`)
- ✅ `std::variant` for polymorphic TLVs (modern C++17, simpler than inheritance)
- ✅ No custom memory allocators (uses default allocator)

---

**Data Model Complete**: 2026-01-15  
**Next Phase**: Phase 1 - Contracts (API specifications)

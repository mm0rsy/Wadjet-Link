# Data Model: Distributed Testing Infrastructure

**Feature**: 014-distributed-testing  
**Date**: 2026-02-03

## Entity Relationship Diagram

```text
┌─────────────────────┐       1:N       ┌─────────────────────┐
│   TestCoordinator   │─────────────────│      TestNode       │
├─────────────────────┤                 ├─────────────────────┤
│ - config_path       │                 │ - node_id           │
│ - nodes[]           │                 │ - address           │
│ - scenarios[]       │                 │ - interfaces[]      │
│ - results[]         │                 │ - clock_status      │
└─────────────────────┘                 │ - health_status     │
         │                              └─────────────────────┘
         │ 1:N                                    │
         ▼                                        │ 1:N
┌─────────────────────┐                          ▼
│ DistributedScenario │                 ┌─────────────────────┐
├─────────────────────┤                 │  NodeCaptureResult  │
│ - name              │                 ├─────────────────────┤
│ - description       │                 │ - node_id           │
│ - tags[]            │                 │ - packets[]         │
│ - nodes[]           │                 │ - start_time_ns     │
│ - steps[]           │                 │ - end_time_ns       │
└─────────────────────┘                 │ - pcap_path         │
         │                              └─────────────────────┘
         │ 1:N
         ▼
┌─────────────────────┐       uses      ┌─────────────────────┐
│   DistributedStep   │────────────────▶│ DistributedMatcher  │
├─────────────────────┤                 ├─────────────────────┤
│ - name              │                 │ - src_node          │
│ - type (enum)       │                 │ - dst_node          │
│ - node_id           │                 │ - inner_matcher     │
│ - timeout_ms        │                 │ - max_latency_ns    │
│ - matcher_config    │                 └─────────────────────┘
└─────────────────────┘
         │
         │ produces
         ▼
┌─────────────────────┐       1:N       ┌─────────────────────┐
│  AggregatedResult   │─────────────────│   AssertionResult   │
├─────────────────────┤                 ├─────────────────────┤
│ - scenario_name     │                 │ - matcher_id        │
│ - status            │                 │ - status            │
│ - start_time        │                 │ - src_node          │
│ - end_time          │                 │ - dst_node          │
│ - node_results[]    │                 │ - src_timestamp_ns  │
│ - assertions[]      │                 │ - dst_timestamp_ns  │
│ - pcap_paths[]      │                 │ - latency_ns        │
└─────────────────────┘                 │ - error_message     │
                                        └─────────────────────┘
```

## Core Entities

### TestCoordinator

Central orchestration service running on controller node.

```cpp
namespace wadjet::distributed {

struct CoordinatorConfig {
    std::filesystem::path config_path;              // YAML/JSON node config
    std::filesystem::path tls_cert_path;            // TLS certificate
    std::filesystem::path tls_key_path;             // TLS private key
    std::filesystem::path tls_ca_path;              // CA certificate
    uint16_t listen_port = 50051;                   // gRPC listen port
    std::chrono::milliseconds heartbeat_interval{1000};
    std::chrono::milliseconds heartbeat_timeout{5000};
    std::chrono::milliseconds barrier_timeout{10000};
};

class TestCoordinator {
public:
    static auto create(CoordinatorConfig config) -> Result<TestCoordinator>;
    
    // Node management
    auto register_node(const NodeInfo& info) -> Result<NodeId>;
    auto unregister_node(const NodeId& id) -> Result<void>;
    auto get_node_status(const NodeId& id) -> std::optional<NodeStatus>;
    auto list_nodes() -> std::vector<NodeInfo>;
    
    // Scenario execution
    auto load_scenario(const std::filesystem::path& path) -> Result<ScenarioId>;
    auto run_scenario(const ScenarioId& id) -> Result<AggregatedResult>;
    auto abort_scenario(const ScenarioId& id) -> Result<void>;
    
    // Barrier synchronization
    auto create_barrier(const std::string& barrier_id, 
                        const std::vector<NodeId>& nodes) -> Result<BarrierId>;
    auto wait_barrier(const BarrierId& id) -> Result<BarrierResult>;
    
    // Results
    auto collect_results(const ScenarioId& id) -> Result<AggregatedResult>;
    auto export_junit(const AggregatedResult& result,
                      const std::filesystem::path& path) -> Result<void>;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed
```

### TestNode

Worker agent running on each test node.

```cpp
namespace wadjet::distributed {

struct NodeConfig {
    std::string node_id;                            // Unique identifier
    std::string coordinator_address;                // Coordinator gRPC address
    std::filesystem::path tls_cert_path;            // Node TLS certificate
    std::filesystem::path tls_key_path;             // Node TLS private key
    std::filesystem::path tls_ca_path;              // CA certificate
    std::vector<std::string> capture_interfaces;    // Interfaces to capture on
    std::filesystem::path failure_capture_dir = "/tmp/wadjet_failures";
};

class TestNode {
public:
    static auto create(NodeConfig config) -> Result<TestNode>;
    
    // Connection management
    auto connect() -> Result<void>;
    auto disconnect() -> Result<void>;
    auto is_connected() const -> bool;
    
    // Capture control
    auto start_capture(const CaptureConfig& config) -> Result<void>;
    auto stop_capture() -> Result<NodeCaptureResult>;
    
    // Barrier participation
    auto wait_at_barrier(const std::string& barrier_id) -> Result<BarrierResult>;
    
    // Matcher evaluation
    auto evaluate_matcher(const MatcherConfig& config) -> Result<MatcherResult>;
    
    // Health reporting
    auto report_clock_status() -> ClockSyncStatus;
    auto report_health() -> NodeHealthStatus;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed
```

### DistributedScenario

YAML/JSON definition of multi-node test scenario.

```cpp
namespace wadjet::distributed {

enum class StepType {
    Barrier,        // Synchronization point
    Capture,        // Start/stop packet capture
    Expect,         // Distributed assertion
    Wait,           // Timed delay
    Log             // Log message
};

struct DistributedStep {
    std::string name;
    StepType type;
    std::optional<std::string> node_id;             // Which node executes (nullopt = coordinator)
    std::chrono::milliseconds timeout{5000};
    
    // Type-specific config (variant)
    std::variant<
        BarrierStepConfig,
        CaptureStepConfig,
        ExpectStepConfig,
        WaitStepConfig,
        LogStepConfig
    > config;
};

struct BarrierStepConfig {
    std::string barrier_id;
    std::vector<std::string> participating_nodes;
};

struct CaptureStepConfig {
    std::string interface;
    std::string filter;                             // BPF filter expression
    std::chrono::milliseconds duration{0};          // 0 = until explicitly stopped
};

struct ExpectStepConfig {
    std::string matcher_type;                       // "expect_message_flow", "within_latency", etc.
    std::string src_node;
    std::string dst_node;
    nlohmann::json matcher_params;                  // Type-specific parameters
    std::chrono::nanoseconds max_latency{0};        // For latency assertions
};

struct WaitStepConfig {
    std::chrono::milliseconds duration;
};

struct LogStepConfig {
    std::string message;
    std::string level = "INFO";
};

struct DistributedScenario {
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    std::vector<NodeDefinition> nodes;
    std::vector<DistributedStep> steps;
    
    // Parse from YAML/JSON file
    static auto parse(const std::filesystem::path& path) -> Result<DistributedScenario>;
    static auto parse_yaml(std::string_view content) -> Result<DistributedScenario>;
    static auto parse_json(std::string_view content) -> Result<DistributedScenario>;
};

struct NodeDefinition {
    std::string id;
    std::string address;
    std::vector<std::string> interfaces;
};

}  // namespace wadjet::distributed
```

### SyncBarrier

Barrier synchronization primitive.

```cpp
namespace wadjet::distributed {

struct BarrierResult {
    bool proceed;
    int64_t sync_timestamp_ns;                      // Agreed start time (nanoseconds since epoch)
    std::vector<std::string> participating_nodes;
    std::vector<std::string> missing_nodes;         // Nodes that didn't arrive (on timeout)
    std::chrono::milliseconds wait_duration;
    
    auto succeeded() const -> bool { return proceed && missing_nodes.empty(); }
};

class SyncBarrier {
public:
    struct Config {
        std::chrono::milliseconds timeout{5000};
        std::chrono::milliseconds sync_margin{10};  // Extra delay for network jitter
    };
    
    explicit SyncBarrier(std::string barrier_id);
    
    // Coordinator-side: wait for all nodes
    auto wait_for_nodes(const std::vector<std::string>& expected_nodes,
                        Config config = {}) -> BarrierResult;
    
    // Node-side: signal arrival and wait for proceed
    auto arrive_and_wait(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) 
        -> Result<BarrierResult>;
    
    auto barrier_id() const -> const std::string&;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed
```

### TimestampNormalizer

Converts between node-local and global timestamps.

```cpp
namespace wadjet::distributed {

enum class ClockSyncMethod {
    None,
    NTP,
    GPTP,
    Unknown
};

struct ClockSyncStatus {
    ClockSyncMethod method = ClockSyncMethod::None;
    bool is_synchronized = false;
    int64_t estimated_offset_ns = 0;
    int64_t max_error_ns = 0;
    std::string grandmaster_id;                     // For gPTP
};

class TimestampNormalizer {
public:
    // Detect current clock synchronization status
    static auto detect_sync_status() -> ClockSyncStatus;
    
    // Get current time in nanoseconds since Unix epoch (UTC)
    static auto now_utc_ns() -> int64_t;
    
    // Convert hardware timestamp to UTC nanoseconds
    static auto hardware_to_utc(const timespec& hw_ts) -> int64_t;
    
    // Normalize packet timestamp to UTC
    auto normalize(const Packet& packet) const -> int64_t;
    
    // Check if two timestamps are within acceptable drift
    auto within_drift(int64_t ts1, int64_t ts2, 
                      std::chrono::nanoseconds max_drift) const -> bool;
    
    // Get estimated precision based on sync method
    auto estimated_precision() const -> std::chrono::nanoseconds;
    
private:
    ClockSyncStatus status_;
};

}  // namespace wadjet::distributed
```

### MessageCorrelator

Links packets across nodes by content or identifiers.

```cpp
namespace wadjet::distributed {

struct CorrelatedPackets {
    std::string correlation_id;                     // Generated or extracted ID
    std::vector<std::pair<std::string, Packet>> node_packets;  // (node_id, packet) pairs
    std::chrono::nanoseconds max_time_delta;        // Max timestamp difference
};

class MessageCorrelator {
public:
    enum class CorrelationMethod {
        PayloadHash,        // SHA-256 of payload
        SequenceNumber,     // Protocol sequence number (SOME/IP, TCP, etc.)
        TransactionId,      // Protocol transaction ID (DoIP, UDS)
        Timestamp,          // Timestamp proximity
        Custom              // User-defined correlation function
    };
    
    using CorrelationFunc = std::function<std::optional<std::string>(const PacketView&)>;
    
    explicit MessageCorrelator(CorrelationMethod method = CorrelationMethod::PayloadHash);
    MessageCorrelator(CorrelationFunc custom_func);
    
    // Add packets from a node
    auto add_packets(const std::string& node_id, 
                     std::span<const Packet> packets) -> void;
    
    // Find correlated packets across nodes
    auto correlate() -> std::vector<CorrelatedPackets>;
    
    // Find specific packet correlation
    auto find_correlation(const Packet& source_packet,
                          const std::string& target_node) -> std::optional<Packet>;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed
```

### DistributedMatcher

Base class for distributed assertions.

```cpp
namespace wadjet::distributed {

struct DistributedMatchResult {
    bool matched = false;
    std::string src_node;
    std::string dst_node;
    int64_t src_timestamp_ns = 0;
    int64_t dst_timestamp_ns = 0;
    int64_t latency_ns = 0;
    std::optional<Packet> src_packet;
    std::optional<Packet> dst_packet;
    std::string error_message;
    
    static auto success(int64_t src_ts, int64_t dst_ts) -> DistributedMatchResult;
    static auto failure(std::string error) -> DistributedMatchResult;
};

struct DistributedCaptureContext {
    std::string node_id;
    std::vector<Packet> packets;
    int64_t start_timestamp_ns;
    int64_t end_timestamp_ns;
};

class DistributedMatcher {
public:
    virtual ~DistributedMatcher() = default;
    
    // Evaluate across multiple node captures
    virtual auto evaluate(
        const std::unordered_map<std::string, DistributedCaptureContext>& contexts
    ) -> DistributedMatchResult = 0;
    
    // Human-readable description
    virtual auto describe() const -> std::string = 0;
    
    // Clone for composition
    virtual auto clone() const -> std::unique_ptr<DistributedMatcher> = 0;
};

// Factory functions for creating matchers
auto ExpectMessageFlow(std::string src_node, 
                       std::string dst_node,
                       ::testing::Matcher<const PacketView&> inner_matcher)
    -> std::unique_ptr<DistributedMatcher>;

auto WithinLatency(std::unique_ptr<DistributedMatcher> inner,
                   std::chrono::nanoseconds max_latency)
    -> std::unique_ptr<DistributedMatcher>;

auto HappensBefore(std::string event_a_node, 
                   ::testing::Matcher<const PacketView&> event_a_matcher,
                   std::string event_b_node,
                   ::testing::Matcher<const PacketView&> event_b_matcher)
    -> std::unique_ptr<DistributedMatcher>;

auto MustNotSeeOn(std::string node,
                  ::testing::Matcher<const PacketView&> matcher)
    -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed
```

### AggregatedResult

Combined test results from all nodes.

```cpp
namespace wadjet::distributed {

enum class ResultStatus {
    Passed,
    Failed,
    Error,
    Skipped,
    Timeout
};

struct AssertionResult {
    std::string matcher_id;
    ResultStatus status;
    std::string src_node;
    std::string dst_node;
    int64_t src_timestamp_ns;
    int64_t dst_timestamp_ns;
    int64_t latency_ns;
    std::string expected;
    std::string actual;
    std::string error_message;
};

struct NodeResult {
    std::string node_id;
    ResultStatus status;
    std::vector<AssertionResult> assertions;
    std::filesystem::path pcap_path;
    std::chrono::milliseconds execution_time;
    std::string error_message;
};

struct AggregatedResult {
    std::string scenario_name;
    ResultStatus status;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds total_duration;
    
    std::vector<NodeResult> node_results;
    std::vector<AssertionResult> distributed_assertions;
    std::vector<std::filesystem::path> pcap_files;
    
    // Summary statistics
    int total_assertions = 0;
    int passed_assertions = 0;
    int failed_assertions = 0;
    
    // Export to various formats
    auto to_junit_xml() const -> std::string;
    auto to_json() const -> nlohmann::json;
    auto to_html_report() const -> std::string;
    
    // Merge results from multiple scenarios
    static auto merge(std::span<const AggregatedResult> results) -> AggregatedResult;
};

}  // namespace wadjet::distributed
```

## Enumerations

```cpp
namespace wadjet::distributed {

// Node health states
enum class NodeHealthStatus {
    Unknown,
    Healthy,
    Degraded,       // Clock drift detected or high latency
    Unhealthy,      // Heartbeat timeout
    Disconnected
};

// Capture states
enum class CaptureState {
    Idle,
    Starting,
    Running,
    Stopping,
    Stopped,
    Error
};

// Barrier states
enum class BarrierState {
    Waiting,
    AllArrived,
    Timeout,
    Cancelled
};

}  // namespace wadjet::distributed
```

## NetworkTopology (FR-019)

Represents the network layout for message flow visualization.

```cpp
namespace wadjet::distributed {

struct NetworkLink {
    std::string src_node;
    std::string dst_node;
    std::string interface;                          // Network interface name
    uint64_t packet_count = 0;                      // Packets observed on link
    std::chrono::nanoseconds avg_latency{0};        // Average observed latency
};

class NetworkTopology {
public:
    // Build topology from scenario definition
    static auto from_scenario(const DistributedScenario& scenario) -> NetworkTopology;
    
    // Build topology from observed packet flow
    static auto from_captures(
        const std::unordered_map<std::string, DistributedCaptureContext>& captures
    ) -> NetworkTopology;
    
    // Add observed packet flow between nodes
    auto add_observed_flow(const std::string& src_node, 
                           const std::string& dst_node,
                           const Packet& packet) -> void;
    
    // Visualization
    auto visualize_flow() const -> std::string;     // Returns Mermaid diagram
    auto to_dot() const -> std::string;             // Returns DOT/Graphviz format
    auto to_json() const -> nlohmann::json;         // Returns JSON representation
    
    // Query
    auto get_links() const -> std::span<const NetworkLink>;
    auto get_nodes() const -> std::vector<std::string>;
    auto has_path(const std::string& src, const std::string& dst) const -> bool;
    
private:
    std::vector<NetworkLink> links_;
    std::unordered_set<std::string> nodes_;
};

}  // namespace wadjet::distributed
```

## Validation Rules

1. **NodeId**: Non-empty string, alphanumeric + underscore, max 64 characters
2. **Address**: Valid IPv4/IPv6 address with port, or hostname:port format
3. **BarrierId**: Non-empty string, unique per scenario
4. **Timestamps**: Nanoseconds since Unix epoch, must be positive
5. **Latency**: Non-negative duration, max 1 hour for timeout values
6. **PCAP paths**: Must be writable directory, valid filename characters
7. **Interfaces**: Must exist on the node (validated at runtime)

## State Transitions

### Node Lifecycle

```text
                 ┌─────────────┐
                 │ Disconnected│
                 └──────┬──────┘
                        │ connect()
                        ▼
                 ┌─────────────┐
         ┌───────│   Healthy   │◄────────┐
         │       └──────┬──────┘         │
         │              │ clock drift    │ recovery
         │              ▼                │
         │       ┌─────────────┐         │
         │       │  Degraded   │─────────┘
         │       └──────┬──────┘
         │              │ heartbeat timeout
         │              ▼
         │       ┌─────────────┐
         │       │  Unhealthy  │
         │       └──────┬──────┘
         │              │ disconnect()
         │              ▼
         └──────▶┌─────────────┐
                 │ Disconnected│
                 └─────────────┘
```

### Barrier Lifecycle

```text
┌─────────────┐     arrive()      ┌─────────────┐
│   Waiting   │──────────────────▶│   Waiting   │
└─────────────┘                   └──────┬──────┘
                                         │ all nodes arrived
                                         ▼
                                  ┌─────────────┐
                                  │ AllArrived  │──▶ proceed
                                  └─────────────┘
                                         
┌─────────────┐     timeout       ┌─────────────┐
│   Waiting   │──────────────────▶│   Timeout   │──▶ abort
└─────────────┘                   └─────────────┘
```

# Research: Distributed Testing Infrastructure

**Feature**: 014-distributed-testing  
**Date**: 2026-02-03  
**Status**: Complete

## Research Tasks

### 1. gRPC C++ Integration

**Question**: Best practices for gRPC in CMake projects, async vs sync API, TLS mutual auth setup?

**Decision**: Use gRPC synchronous API with CMake FetchContent

**Rationale**:
- Synchronous API is simpler and sufficient for test coordination (not high-frequency trading)
- FetchContent ensures reproducible builds without system-wide gRPC installation
- TLS mutual authentication via gRPC's built-in `SslCredentials` with certificate/key files

**Alternatives Considered**:
- Async gRPC API: Adds complexity (completion queues, callbacks) without clear benefit for <10 nodes
- System-installed gRPC: Version conflicts, harder CI setup
- Raw TCP with custom protocol: Would reimplement connection management, auth, serialization

**CMake Integration Pattern**:
```cmake
include(FetchContent)
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.50.0
)
set(gRPC_BUILD_TESTS OFF)
set(gRPC_BUILD_CSHARP_EXT OFF)
FetchContent_MakeAvailable(gRPC)

target_link_libraries(wadjet_distributed
  PRIVATE
    grpc++
    protobuf::libprotobuf
)
```

**TLS Mutual Auth Pattern**:
```cpp
grpc::SslCredentialsOptions ssl_opts;
ssl_opts.pem_root_certs = read_file("ca.crt");
ssl_opts.pem_private_key = read_file("node.key");
ssl_opts.pem_cert_chain = read_file("node.crt");
auto creds = grpc::SslCredentials(ssl_opts);
auto channel = grpc::CreateChannel(address, creds);
```

---

### 2. Protocol Buffers Schema Design

**Question**: Message design for distributed testing coordination, streaming patterns?

**Decision**: Request-response for control, server streaming for results collection

**Rationale**:
- Control messages (RegisterNode, SyncBarrier) are naturally request-response
- Result reporting benefits from streaming to handle large PCAP attachments
- Bidirectional streaming adds unnecessary complexity

**Key Messages**:
```protobuf
// Node registration
message NodeInfo {
  string node_id = 1;
  string address = 2;
  repeated string interfaces = 3;
  ClockSyncStatus clock_status = 4;
}

// Barrier synchronization
message BarrierRequest {
  string barrier_id = 1;
  string node_id = 2;
  int64 timestamp_ns = 3;
}

message BarrierResponse {
  bool proceed = 1;
  int64 sync_timestamp_ns = 2;  // Agreed start time
  repeated string participating_nodes = 3;
}

// Matcher evaluation
message MatcherRequest {
  string matcher_id = 1;
  bytes serialized_matcher = 2;  // Protobuf-encoded matcher config
  int64 timeout_ms = 3;
}

message MatcherResult {
  bool matched = 1;
  int64 match_timestamp_ns = 2;
  bytes packet_data = 3;  // Captured packet if matched
  string error_message = 4;
}

// Result reporting (streaming)
message TestResult {
  string node_id = 1;
  string test_name = 2;
  ResultStatus status = 3;
  repeated AssertionResult assertions = 4;
  bytes pcap_data = 5;  // PCAP file content on failure
  map<string, string> metadata = 6;
}
```

---

### 3. Clock Synchronization Detection

**Question**: How to detect if system clock is gPTP/NTP synchronized?

**Decision**: Use `adjtimex()` for sync status + optional gPTP message decoding via M8

**Rationale**:
- `adjtimex()` returns clock state including STA_UNSYNC flag
- Chrony/ntpd set this flag based on sync status
- linuxptp (phc2sys) also updates system clock discipline status
- M8 gPTP decoder provides additional health verification by parsing Announce messages

**Implementation Pattern**:
```cpp
#include <sys/timex.h>

enum class ClockSyncMethod { None, NTP, GPTP, Unknown };

struct ClockSyncStatus {
    ClockSyncMethod method;
    bool is_synchronized;
    int64_t estimated_offset_ns;
    int64_t max_error_ns;
};

auto detect_clock_sync() -> ClockSyncStatus {
    struct timex tx = {};
    int result = adjtimex(&tx);
    
    ClockSyncStatus status;
    status.is_synchronized = !(tx.status & STA_UNSYNC);
    status.max_error_ns = tx.maxerror * 1000;  // µs to ns
    status.estimated_offset_ns = tx.offset;     // µs
    
    // Detect sync method (heuristic: PTP has much lower error)
    if (status.is_synchronized) {
        if (tx.maxerror < 1000) {  // <1µs suggests PTP
            status.method = ClockSyncMethod::GPTP;
        } else {
            status.method = ClockSyncMethod::NTP;
        }
    } else {
        status.method = ClockSyncMethod::None;
    }
    
    return status;
}
```

**gPTP Health Verification** (via M8 decoder):
```cpp
// Passively decode gPTP Announce to verify grandmaster
auto verify_gptp_health(CaptureSession& session) -> GptpHealthStatus {
    auto filter = IsGptpAnnounce();
    auto announce = session.wait_for_packet(filter, 5s);
    if (!announce) {
        return GptpHealthStatus::NoAnnounce;
    }
    
    auto header = decode<GptpHeader>(announce->view());
    // Check grandmaster identity, clock quality, etc.
    return GptpHealthStatus::Healthy;
}
```

---

### 4. Barrier Synchronization Patterns

**Question**: Distributed barrier algorithms, handling node failures during barrier wait?

**Decision**: Centralized barrier with coordinator timeout

**Rationale**:
- Centralized barrier is simpler than distributed consensus (Paxos, Raft)
- Coordinator already exists for test orchestration
- Timeout-based failure detection aligns with health check mechanism
- No need for Byzantine fault tolerance (trusted test environment)

**Algorithm**:
1. Each node sends `BarrierRequest` to coordinator with local timestamp
2. Coordinator waits for all expected nodes (with timeout)
3. If all nodes arrive: Coordinator computes agreed start time (max timestamp + margin)
4. If timeout: Coordinator aborts barrier, notifies arrived nodes, marks missing nodes as failed
5. Coordinator sends `BarrierResponse` to all nodes with proceed=true/false

**Failure Handling**:
```cpp
class SyncBarrier {
public:
    struct Config {
        std::chrono::milliseconds timeout{5000};
        std::chrono::milliseconds sync_margin{10};  // Extra delay for network jitter
    };
    
    // Called by coordinator
    auto wait_for_nodes(const std::vector<std::string>& expected_nodes, 
                        Config config) -> BarrierResult {
        auto deadline = std::chrono::steady_clock::now() + config.timeout;
        std::unordered_set<std::string> arrived;
        int64_t max_timestamp = 0;
        
        while (arrived.size() < expected_nodes.size()) {
            if (std::chrono::steady_clock::now() > deadline) {
                // Timeout - identify missing nodes
                std::vector<std::string> missing;
                for (const auto& node : expected_nodes) {
                    if (!arrived.contains(node)) {
                        missing.push_back(node);
                    }
                }
                return BarrierResult::timeout(missing);
            }
            
            // Wait for next arrival (with remaining timeout)
            auto arrival = receive_barrier_request(deadline);
            if (arrival) {
                arrived.insert(arrival->node_id);
                max_timestamp = std::max(max_timestamp, arrival->timestamp_ns);
            }
        }
        
        // All nodes arrived - compute agreed start time
        int64_t sync_time = max_timestamp + config.sync_margin.count() * 1000000;
        return BarrierResult::proceed(sync_time, arrived);
    }
};
```

---

### 5. PCAP Merge Algorithms

**Question**: Merging multiple PCAP files by timestamp, handling clock drift?

**Decision**: Timestamp-sorted merge with drift warning

**Rationale**:
- Simple merge-sort algorithm is sufficient (each PCAP already sorted)
- Clock drift detection via timestamp overlap analysis
- Warning if drift exceeds threshold (1ms for NTP, 1µs for gPTP)
- Use PCAPNG format to preserve interface metadata per node

**Algorithm**:
```cpp
class PcapMerger {
public:
    struct MergeConfig {
        std::chrono::nanoseconds max_drift_warning{1000000};  // 1ms default
        bool include_interface_comments{true};
    };
    
    auto merge(const std::vector<PcapSource>& sources,
               const std::filesystem::path& output,
               MergeConfig config) -> MergeResult {
        // Open all sources
        std::vector<PcapReader> readers;
        std::priority_queue<TimestampedPacket, 
                           std::vector<TimestampedPacket>,
                           std::greater<>> heap;
        
        for (size_t i = 0; i < sources.size(); ++i) {
            readers.emplace_back(sources[i].path);
            if (auto pkt = readers[i].next_packet()) {
                heap.push({pkt->timestamp(), i, std::move(*pkt)});
            }
        }
        
        PcapNgWriter writer(output);
        
        // Add interface blocks for each node
        for (const auto& src : sources) {
            writer.add_interface(src.node_id, src.interface_name);
        }
        
        // Merge-sort by timestamp
        int64_t last_timestamp = 0;
        while (!heap.empty()) {
            auto [ts, source_idx, packet] = heap.top();
            heap.pop();
            
            // Drift detection
            if (ts < last_timestamp) {
                auto drift = last_timestamp - ts;
                if (drift > config.max_drift_warning.count()) {
                    // Log warning about clock drift
                }
            }
            last_timestamp = std::max(last_timestamp, ts);
            
            // Write with source annotation
            writer.write_packet(packet, source_idx);
            
            // Refill from same source
            if (auto next = readers[source_idx].next_packet()) {
                heap.push({next->timestamp(), source_idx, std::move(*next)});
            }
        }
        
        return MergeResult::success(writer.packet_count());
    }
};
```

---

### 6. Existing M3 Matcher Extension

**Question**: How to extend `PacketMatcher` for distributed assertions?

**Decision**: New `DistributedMatcher` base class that aggregates results from multiple nodes

**Rationale**:
- Existing M3 matchers work on single packets
- Distributed matchers need multi-node context (packets from A, packets from B)
- Composition: DistributedMatcher contains M3 matchers for per-node filtering
- Preserves GoogleTest integration via gMock `Matcher<>` interface

**Existing M3 Structure** (from `include/wadjet/testing/matchers.hpp`):
```cpp
// Existing single-node matcher
template <typename T>
class PacketMatcher : public ::testing::MatcherInterface<const PacketView&> {
    // ...
};
```

**New Distributed Matcher Design**:
```cpp
// Multi-node packet collection for distributed assertions
struct DistributedCaptureContext {
    std::string node_id;
    std::vector<Packet> packets;
    int64_t start_timestamp_ns;
    int64_t end_timestamp_ns;
};

// Base class for distributed matchers
class DistributedMatcher {
public:
    virtual ~DistributedMatcher() = default;
    
    // Evaluate across multiple node captures
    virtual auto evaluate(
        const std::unordered_map<std::string, DistributedCaptureContext>& contexts
    ) -> DistributedMatchResult = 0;
    
    // Human-readable description
    virtual auto describe() const -> std::string = 0;
};

// Example: ExpectMessageFlow(node_a, node_b, inner_matcher)
class ExpectMessageFlow : public DistributedMatcher {
public:
    ExpectMessageFlow(std::string src_node, 
                      std::string dst_node,
                      ::testing::Matcher<const PacketView&> inner_matcher)
        : src_node_(std::move(src_node))
        , dst_node_(std::move(dst_node))
        , inner_matcher_(std::move(inner_matcher)) {}
    
    auto evaluate(const auto& contexts) -> DistributedMatchResult override {
        // Find packet matching inner_matcher on src_node
        const auto& src_ctx = contexts.at(src_node_);
        std::optional<Packet> src_packet;
        for (const auto& pkt : src_ctx.packets) {
            if (inner_matcher_.Matches(pkt.view())) {
                src_packet = pkt;
                break;
            }
        }
        if (!src_packet) {
            return DistributedMatchResult::failure(
                fmt::format("No matching packet on source node '{}'", src_node_));
        }
        
        // Find same packet on dst_node (by content or correlation ID)
        const auto& dst_ctx = contexts.at(dst_node_);
        for (const auto& pkt : dst_ctx.packets) {
            if (packets_correlate(*src_packet, pkt)) {
                return DistributedMatchResult::success(
                    src_packet->timestamp(), pkt.timestamp());
            }
        }
        
        return DistributedMatchResult::failure(
            fmt::format("Packet sent from '{}' not received on '{}'", 
                       src_node_, dst_node_));
    }
    
private:
    std::string src_node_;
    std::string dst_node_;
    ::testing::Matcher<const PacketView&> inner_matcher_;
};

// WithinLatency decorator
class WithinLatency : public DistributedMatcher {
public:
    WithinLatency(std::unique_ptr<DistributedMatcher> inner,
                  std::chrono::nanoseconds max_latency)
        : inner_(std::move(inner))
        , max_latency_(max_latency) {}
    
    auto evaluate(const auto& contexts) -> DistributedMatchResult override {
        auto result = inner_->evaluate(contexts);
        if (!result.matched) return result;
        
        auto latency = result.dst_timestamp_ns - result.src_timestamp_ns;
        if (latency > max_latency_.count()) {
            return DistributedMatchResult::failure(
                fmt::format("Latency {}ns exceeds maximum {}ns",
                           latency, max_latency_.count()));
        }
        return result;
    }
    
private:
    std::unique_ptr<DistributedMatcher> inner_;
    std::chrono::nanoseconds max_latency_;
};
```

---

## Summary

All research tasks completed. Key decisions:

| Topic | Decision |
|-------|----------|
| gRPC API | Synchronous API with CMake FetchContent |
| Protobuf | Request-response for control, streaming for results |
| Clock Detection | `adjtimex()` + optional M8 gPTP health verification |
| Barrier Sync | Centralized coordinator with timeout-based failure detection |
| PCAP Merge | Timestamp-sorted merge with drift warning |
| Matcher Extension | New `DistributedMatcher` base class composing M3 matchers |

Ready for Phase 1: Design.

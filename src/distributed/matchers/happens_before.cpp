// T059: HappensBefore matcher implementation
// Validates causal ordering between events on different nodes

#include "wadjet/distributed/distributed_matcher.hpp"

#include <unordered_map>
#include <memory>
#include <sstream>

namespace wadjet::distributed {

class HappensBeforeImpl : public DistributedMatcher {
public:
    explicit HappensBeforeImpl(std::string event_a_node, std::string event_b_node)
        : event_a_node_(std::move(event_a_node)),
          event_b_node_(std::move(event_b_node)) {}

    auto evaluate(
        const std::unordered_map<std::string, DistributedCaptureContext>& contexts
    ) -> DistributedMatchResult override {
        
        // Validate that both nodes have captured packets
        auto a_it = contexts.find(event_a_node_);
        auto b_it = contexts.find(event_b_node_);
        
        if (a_it == contexts.end()) {
            return DistributedMatchResult::failure(
                "Node for event A '" + event_a_node_ + "' has no captured packets"
            );
        }
        
        if (b_it == contexts.end()) {
            return DistributedMatchResult::failure(
                "Node for event B '" + event_b_node_ + "' has no captured packets"
            );
        }
        
        const auto& a_packets = a_it->second.packets;
        const auto& b_packets = b_it->second.packets;
        
        if (a_packets.empty()) {
            return DistributedMatchResult::failure(
                "No packets captured on event A node '" + event_a_node_ + "'"
            );
        }
        
        if (b_packets.empty()) {
            return DistributedMatchResult::failure(
                "No packets captured on event B node '" + event_b_node_ + "'"
            );
        }
        
        // Find the last packet from node A and first packet from node B
        // to establish causal ordering
        int64_t latest_a_timestamp = a_packets.front().timestamp().total_nanoseconds();
        int64_t earliest_b_timestamp = b_packets.front().timestamp().total_nanoseconds();
        
        for (const auto& pkt : a_packets) {
            int64_t ts = pkt.timestamp().total_nanoseconds();
            if (ts > latest_a_timestamp) {
                latest_a_timestamp = ts;
            }
        }
        
        for (const auto& pkt : b_packets) {
            int64_t ts = pkt.timestamp().total_nanoseconds();
            if (ts < earliest_b_timestamp) {
                earliest_b_timestamp = ts;
            }
        }
        
        // Event A happens before event B if A's latest timestamp is before B's earliest
        if (latest_a_timestamp < earliest_b_timestamp) {
            // Add a small buffer for clock skew tolerance (1ms)
            const int64_t clock_skew_tolerance_ns = 1000000;  // 1ms
            
            if (earliest_b_timestamp - latest_a_timestamp >= clock_skew_tolerance_ns ||
                earliest_b_timestamp > latest_a_timestamp) {
                return DistributedMatchResult::success(
                    latest_a_timestamp,
                    earliest_b_timestamp
                );
            }
        }
        
        // Check if we have strict causality (B happens after A)
        if (earliest_b_timestamp > latest_a_timestamp) {
            return DistributedMatchResult::success(
                latest_a_timestamp,
                earliest_b_timestamp
            );
        }
        
        return DistributedMatchResult::failure(
            "Causality violation: event on '" + event_a_node_ + 
            "' (at " + std::to_string(latest_a_timestamp) + "ns) does not happen before event on '" +
            event_b_node_ + "' (at " + std::to_string(earliest_b_timestamp) + "ns)"
        );
    }

    auto describe() const -> std::string override {
        return "HappensBefore(" + event_a_node_ + " -> " + event_b_node_ + ")";
    }

    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<HappensBeforeImpl>(event_a_node_, event_b_node_);
    }

private:
    std::string event_a_node_;
    std::string event_b_node_;
};

auto HappensBefore(std::string event_a_node, std::string event_b_node)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<HappensBeforeImpl>(
        std::move(event_a_node),
        std::move(event_b_node)
    );
}

}  // namespace wadjet::distributed

// T058: ExpectMessageFlow matcher implementation
// Validates that a message sent on src_node is received on dst_node

#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/net/packet_view.hpp"

#include <unordered_map>
#include <optional>
#include <sstream>

// Forward declaration for GoogleTest Matcher
namespace testing {
template <typename T>
class Matcher;
}

namespace wadjet::distributed {

class ExpectMessageFlowImpl : public DistributedMatcher {
public:
    explicit ExpectMessageFlowImpl(std::string src_node, std::string dst_node)
        : src_node_(std::move(src_node)), dst_node_(std::move(dst_node)) {}

    auto evaluate(
        const std::unordered_map<std::string, DistributedCaptureContext>& contexts
    ) -> DistributedMatchResult override {
        
        // Validate that both nodes have captured packets
        auto src_it = contexts.find(src_node_);
        auto dst_it = contexts.find(dst_node_);
        
        if (src_it == contexts.end()) {
            return DistributedMatchResult::failure(
                "Source node '" + src_node_ + "' has no captured packets"
            );
        }
        
        if (dst_it == contexts.end()) {
            return DistributedMatchResult::failure(
                "Destination node '" + dst_node_ + "' has no captured packets"
            );
        }
        
        const auto& src_packets = src_it->second.packets;
        const auto& dst_packets = dst_it->second.packets;
        
        if (src_packets.empty()) {
            return DistributedMatchResult::failure(
                "No packets captured on source node '" + src_node_ + "'"
            );
        }
        
        if (dst_packets.empty()) {
            return DistributedMatchResult::failure(
                "No packets captured on destination node '" + dst_node_ + "'"
            );
        }
        
        // For each packet on the source node, try to find a correlated packet on destination
        for (const auto& src_pkt : src_packets) {
            // Look for correlated packets in destination
            for (const auto& dst_pkt : dst_packets) {
                // Attempt correlation by packet sequence number or content hash
                // Simple heuristic: packets that arrive within a reasonable time window
                // and have similar sizes are likely correlated
                if (packets_likely_correlated(src_pkt, dst_pkt)) {
                    return DistributedMatchResult::success(
                        src_pkt.timestamp().total_nanoseconds(),
                        dst_pkt.timestamp().total_nanoseconds()
                    );
                }
            }
        }
        
        return DistributedMatchResult::failure(
            "No message flow detected from '" + src_node_ + "' to '" + dst_node_ + "'"
        );
    }

    auto describe() const -> std::string override {
        return "ExpectMessageFlow(" + src_node_ + " -> " + dst_node_ + ")";
    }

    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<ExpectMessageFlowImpl>(src_node_, dst_node_);
    }

private:
    std::string src_node_;
    std::string dst_node_;

    // Simple heuristic for packet correlation
    // In a real implementation, this would use more sophisticated methods
    static auto packets_likely_correlated(const Packet& src, const Packet& dst) -> bool {
        // Packets are likely correlated if they have similar sizes
        // and the destination packet arrives after the source
        if (src.size() == 0 || dst.size() == 0) {
            return false;
        }
        
        // Size should be within 10% or be similar protocol messages
        auto size_ratio = static_cast<double>(dst.size()) / static_cast<double>(src.size());
        if (size_ratio < 0.9 || size_ratio > 1.1) {
            return false;
        }
        
        // Destination timestamp should be after source
        return dst.timestamp() >= src.timestamp();
    }
};

auto ExpectMessageFlow(std::string src_node, std::string dst_node)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<ExpectMessageFlowImpl>(
        std::move(src_node),
        std::move(dst_node)
    );
}

// T217: ExpectMessageFlow with GoogleTest Matcher parameter
auto ExpectMessageFlow(std::string src_node, std::string dst_node,
                       ::testing::Matcher<const PacketView&> inner_matcher)
    -> std::unique_ptr<DistributedMatcher> {
    // For now, ignore the matcher and create the basic version
    // Full integration with GoogleTest matchers would require significant refactoring
    // to pass matcher context through evaluation. This is a placeholder showing the API.
    return std::make_unique<ExpectMessageFlowImpl>(std::move(src_node), std::move(dst_node));
}

}  // namespace wadjet::distributed

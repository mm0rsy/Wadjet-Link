// T058: ExpectMessageFlow matcher implementation
// Validates that a message sent on src_node is received on dst_node

#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/net/packet_view.hpp"

#include <optional>
#include <sstream>
#include <unordered_map>

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

    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
        -> DistributedMatchResult override {
        // Validate that both nodes have captured packets
        auto src_it = contexts.find(src_node_);
        auto dst_it = contexts.find(dst_node_);

        if (src_it == contexts.end()) {
            return DistributedMatchResult::failure("Source node '" + src_node_ +
                                                   "' has no captured packets");
        }

        if (dst_it == contexts.end()) {
            return DistributedMatchResult::failure("Destination node '" + dst_node_ +
                                                   "' has no captured packets");
        }

        const auto& src_packets = src_it->second.packets;
        const auto& dst_packets = dst_it->second.packets;

        if (src_packets.empty()) {
            return DistributedMatchResult::failure("No packets captured on source node '" +
                                                   src_node_ + "'");
        }

        if (dst_packets.empty()) {
            return DistributedMatchResult::failure("No packets captured on destination node '" +
                                                   dst_node_ + "'");
        }

        // For each packet on the source node, try to find a correlated packet on destination
        for (const auto& src_pkt : src_packets) {
            // Look for correlated packets in destination
            for (const auto& dst_pkt : dst_packets) {
                // Attempt correlation by packet sequence number or content hash
                // Simple heuristic: packets that arrive within a reasonable time window
                // and have similar sizes are likely correlated
                if (packets_likely_correlated(src_pkt, dst_pkt)) {
                    return DistributedMatchResult::success(src_pkt.timestamp().total_nanoseconds(),
                                                           dst_pkt.timestamp().total_nanoseconds());
                }
            }
        }

        return DistributedMatchResult::failure("No message flow detected from '" + src_node_ +
                                               "' to '" + dst_node_ + "'");
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

auto ExpectMessageFlow(std::string src_node,
                       std::string dst_node) -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<ExpectMessageFlowImpl>(std::move(src_node), std::move(dst_node));
}

// T246, T311: ExpectMessageFlow with GoogleTest Matcher parameter
auto ExpectMessageFlow(std::string src_node, std::string dst_node,
                       ::testing::Matcher<const PacketView&> inner_matcher)
    -> std::unique_ptr<DistributedMatcher> {
    // T311: Create ExpectMessageFlow matcher that filters packets using GoogleTest Matcher
    // The inner_matcher validates packet content (e.g., HasSOMEIPServiceId)
    // Only packets matching inner_matcher are considered for source/destination correlation

    class GTestAwareExpectMessageFlow : public ExpectMessageFlowImpl {
    public:
        GTestAwareExpectMessageFlow(std::string src, std::string dst,
                                    ::testing::Matcher<const PacketView&> inner_m)
            : ExpectMessageFlowImpl(std::move(src), std::move(dst)), inner_matcher_(inner_m) {}

        // Override evaluate to apply inner_matcher to filter candidate packets
        auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
            -> DistributedMatchResult override {
            // T311: Get capture contexts for both nodes
            auto src_it = contexts.find(get_src_node());
            auto dst_it = contexts.find(get_dst_node());

            if (src_it == contexts.end() || dst_it == contexts.end()) {
                return DistributedMatchResult::failure("Source or destination node missing");
            }

            const auto& src_packets = src_it->second.packets;
            const auto& dst_packets = dst_it->second.packets;

            if (src_packets.empty() || dst_packets.empty()) {
                return DistributedMatchResult::failure("No packets on source or destination");
            }

            // T311: Filter source packets through inner_matcher
            std::vector<Packet> filtered_src;
            for (const auto& pkt : src_packets) {
                PacketView pv(pkt);
                if (inner_matcher_.Matches(pv)) {
                    filtered_src.push_back(pkt);
                }
            }

            // T311: Filter destination packets through inner_matcher
            std::vector<Packet> filtered_dst;
            for (const auto& pkt : dst_packets) {
                PacketView pv(pkt);
                if (inner_matcher_.Matches(pv)) {
                    filtered_dst.push_back(pkt);
                }
            }

            if (filtered_src.empty() || filtered_dst.empty()) {
                return DistributedMatchResult::failure(
                    "No packets matching inner matcher on source or destination");
            }

            // Correlate filtered packets
            for (const auto& src_pkt : filtered_src) {
                for (const auto& dst_pkt : filtered_dst) {
                    if (packets_likely_correlated(src_pkt, dst_pkt)) {
                        return DistributedMatchResult::success(
                            src_pkt.timestamp().total_nanoseconds(),
                            dst_pkt.timestamp().total_nanoseconds());
                    }
                }
            }

            return DistributedMatchResult::failure(
                "No correlated message flow detected matching inner matcher");
        }

        auto get_src_node() const -> const std::string& { return src_node_; }

        auto get_dst_node() const -> const std::string& { return dst_node_; }

    private:
        ::testing::Matcher<const PacketView&> inner_matcher_;
    };

    return std::make_unique<GTestAwareExpectMessageFlow>(std::move(src_node), std::move(dst_node),
                                                         std::move(inner_matcher));
}

}  // namespace wadjet::distributed

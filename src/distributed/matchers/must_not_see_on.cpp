// T061: MustNotSeeOn matcher implementation
// Validates negative assertion: packets must NOT appear on specified node

#include "wadjet/distributed/distributed_matcher.hpp"

#include <memory>
#include <sstream>
#include <unordered_map>

// Forward declaration for GoogleTest Matcher
namespace testing {
template <typename T>
class Matcher;
}

namespace wadjet::distributed {

class MustNotSeeOnImpl : public DistributedMatcher {
public:
    explicit MustNotSeeOnImpl(std::string node) : node_(std::move(node)) {}

    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
        -> DistributedMatchResult override {
        // Check if the specified node has any captured packets
        auto it = contexts.find(node_);

        if (it == contexts.end()) {
            // Node not in contexts - this is success for a negative matcher
            // (if we're not monitoring the node, we can't see unwanted packets)
            return DistributedMatchResult::success(0, 0);
        }

        const auto& packets = it->second.packets;

        if (packets.empty()) {
            // No packets captured on the node - assertion passes
            return DistributedMatchResult::success(0, 0);
        }

        // We have packets on the node that shouldn't be there
        // Report failure with details about what was observed
        std::ostringstream oss;
        oss << "Negative assertion violated: " << packets.size() << " packet(s) observed on node '"
            << node_ << "' but should not have been present";

        return DistributedMatchResult::failure(oss.str());
    }

    auto describe() const -> std::string override { return "MustNotSeeOn(" + node_ + ")"; }

    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<MustNotSeeOnImpl>(node_);
    }

private:
    std::string node_;
};

auto MustNotSeeOn(std::string node) -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<MustNotSeeOnImpl>(std::move(node));
}

// T248, T313: MustNotSeeOn with GoogleTest Matcher parameter
auto MustNotSeeOn(std::string node, ::testing::Matcher<const PacketView&> matcher)
    -> std::unique_ptr<DistributedMatcher> {
    // T313: Create MustNotSeeOn matcher that filters packets using GoogleTest Matcher
    // The filter_matcher identifies which packets to verify for absence
    // Ensures that packets matching the filter do NOT appear on the specified node

    class GTestAwareMustNotSeeOn : public MustNotSeeOnImpl {
    public:
        GTestAwareMustNotSeeOn(std::string node_id, ::testing::Matcher<const PacketView&> filter_m)
            : MustNotSeeOnImpl(std::move(node_id)), filter_matcher_(filter_m) {}

        // Override evaluate to apply GoogleTest matcher to filter packets
        auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
            -> DistributedMatchResult override {
            // T313: Get capture context for the target node
            auto it = contexts.find(get_target_node());

            if (it == contexts.end()) {
                // Node not in contexts - success for negative matcher
                return DistributedMatchResult::success(0, 0);
            }

            const auto& packets = it->second.packets;

            if (packets.empty()) {
                // No packets captured - assertion passes
                return DistributedMatchResult::success(0, 0);
            }

            // T313: Filter packets through the GoogleTest matcher
            std::vector<Packet> matching_packets;
            for (const auto& pkt : packets) {
                PacketView pv(pkt);
                if (filter_matcher_.Matches(pv)) {
                    matching_packets.push_back(pkt);
                }
            }

            if (matching_packets.empty()) {
                // No packets matching the filter - assertion passes
                return DistributedMatchResult::success(0, 0);
            }

            // T313: Found packets matching the filter - this is a violation
            std::ostringstream oss;
            oss << "Negative assertion violated: " << matching_packets.size()
                << " packet(s) matching the filter were observed on node '" << get_target_node()
                << "' but should not have been present";

            return DistributedMatchResult::failure(oss.str());
        }

        auto get_target_node() const -> const std::string& { return node_; }

    private:
        ::testing::Matcher<const PacketView&> filter_matcher_;
    };

    return std::make_unique<GTestAwareMustNotSeeOn>(std::move(node), std::move(matcher));
}

}  // namespace wadjet::distributed

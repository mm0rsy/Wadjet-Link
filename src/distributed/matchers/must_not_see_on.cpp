// T061: MustNotSeeOn matcher implementation
// Validates negative assertion: packets must NOT appear on specified node

#include "wadjet/distributed/distributed_matcher.hpp"

#include <unordered_map>
#include <memory>
#include <sstream>

namespace wadjet::distributed {

class MustNotSeeOnImpl : public DistributedMatcher {
public:
    explicit MustNotSeeOnImpl(std::string node)
        : node_(std::move(node)) {}

    auto evaluate(
        const std::unordered_map<std::string, DistributedCaptureContext>& contexts
    ) -> DistributedMatchResult override {
        
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
        oss << "Negative assertion violated: " << packets.size() 
            << " packet(s) observed on node '" << node_ 
            << "' but should not have been present";
        
        return DistributedMatchResult::failure(oss.str());
    }

    auto describe() const -> std::string override {
        return "MustNotSeeOn(" + node_ + ")";
    }

    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<MustNotSeeOnImpl>(node_);
    }

private:
    std::string node_;
};

auto MustNotSeeOn(std::string node)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<MustNotSeeOnImpl>(std::move(node));
}

}  // namespace wadjet::distributed

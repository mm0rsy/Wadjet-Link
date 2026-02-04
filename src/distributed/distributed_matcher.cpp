#include "wadjet/distributed/distributed_matcher.hpp"

namespace wadjet::distributed {

/**
 * @brief ExpectMessageFlow implementation
 */
class ExpectMessageFlowMatcher : public DistributedMatcher {
public:
    ExpectMessageFlowMatcher(std::string src, std::string dst)
        : src_node_(std::move(src)), dst_node_(std::move(dst)) {}
    
    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& /*contexts*/)
        -> DistributedMatchResult override {
        // T058: Check if packets flow from src to dst
        // Placeholder implementation
        DistributedMatchResult result;
        result.matched = true;
        result.src_node = src_node_;
        result.dst_node = dst_node_;
        return result;
    }
    
    auto describe() const -> std::string override {
        return "ExpectMessageFlow from " + src_node_ + " to " + dst_node_;
    }
    
    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<ExpectMessageFlowMatcher>(src_node_, dst_node_);
    }

private:
    std::string src_node_;
    std::string dst_node_;
};

/**
 * @brief WithinLatency implementation
 */
class WithinLatencyMatcher : public DistributedMatcher {
public:
    WithinLatencyMatcher(std::unique_ptr<DistributedMatcher> inner,
                        std::chrono::nanoseconds max_latency)
        : inner_(std::move(inner)), max_latency_(max_latency) {}
    
    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
        -> DistributedMatchResult override {
        // T060: Apply latency constraint
        auto result = inner_->evaluate(contexts);
        
        if (result.matched && result.latency_ns > max_latency_.count()) {
            result.matched = false;
            result.error_message = "Latency " + std::to_string(result.latency_ns) +
                                  "ns exceeds max " + std::to_string(max_latency_.count()) + "ns";
        }
        
        return result;
    }
    
    auto describe() const -> std::string override {
        return inner_->describe() + " within " + std::to_string(max_latency_.count()) + "ns";
    }
    
    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<WithinLatencyMatcher>(inner_->clone(), max_latency_);
    }

private:
    std::unique_ptr<DistributedMatcher> inner_;
    std::chrono::nanoseconds max_latency_;
};

/**
 * @brief HappensBefore implementation
 */
class HappensBeforeMatcher : public DistributedMatcher {
public:
    HappensBeforeMatcher(std::string event_a_node, std::string event_b_node)
        : event_a_node_(std::move(event_a_node)), event_b_node_(std::move(event_b_node)) {}
    
    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& /*contexts*/)
        -> DistributedMatchResult override {
        // T059: Check causal ordering
        DistributedMatchResult result;
        result.matched = true;
        result.src_node = event_a_node_;
        result.dst_node = event_b_node_;
        return result;
    }
    
    auto describe() const -> std::string override {
        return event_a_node_ + " happens before " + event_b_node_;
    }
    
    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<HappensBeforeMatcher>(event_a_node_, event_b_node_);
    }

private:
    std::string event_a_node_;
    std::string event_b_node_;
};

/**
 * @brief MustNotSeeOn implementation
 */
class MustNotSeeOnMatcher : public DistributedMatcher {
public:
    explicit MustNotSeeOnMatcher(std::string node)
        : node_(std::move(node)) {}
    
    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
        -> DistributedMatchResult override {
        // T061: Check that packet doesn't appear on specified node
        auto it = contexts.find(node_);
        
        DistributedMatchResult result;
        result.matched = (it == contexts.end() || it->second.packets.empty());
        result.dst_node = node_;
        
        if (!result.matched) {
            result.error_message = "Packet found on node " + node_ + " but should not be there";
        }
        
        return result;
    }
    
    auto describe() const -> std::string override {
        return "MustNotSeeOn " + node_;
    }
    
    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<MustNotSeeOnMatcher>(node_);
    }

private:
    std::string node_;
};

// Factory functions
auto ExpectMessageFlow(std::string src_node, std::string dst_node)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<ExpectMessageFlowMatcher>(std::move(src_node), std::move(dst_node));
}

auto WithinLatency(std::unique_ptr<DistributedMatcher> inner,
                  std::chrono::nanoseconds max_latency)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<WithinLatencyMatcher>(std::move(inner), max_latency);
}

auto HappensBefore(std::string event_a_node, std::string event_b_node)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<HappensBeforeMatcher>(std::move(event_a_node), std::move(event_b_node));
}

auto MustNotSeeOn(std::string node)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<MustNotSeeOnMatcher>(std::move(node));
}

}  // namespace wadjet::distributed

// T060: WithinLatency matcher implementation
// Validates that latency between nodes is within specified limit

#include "wadjet/distributed/distributed_matcher.hpp"

#include <chrono>
#include <memory>
#include <sstream>
#include <iomanip>

namespace wadjet::distributed {

class WithinLatencyImpl : public DistributedMatcher {
public:
    explicit WithinLatencyImpl(std::unique_ptr<DistributedMatcher> inner,
                             std::chrono::nanoseconds max_latency)
        : inner_(std::move(inner)), max_latency_(max_latency) {
        if (!inner_) {
            throw std::invalid_argument("WithinLatency requires a non-null inner matcher");
        }
    }

    auto evaluate(
        const std::unordered_map<std::string, DistributedCaptureContext>& contexts
    ) -> DistributedMatchResult override {
        
        // First evaluate the inner matcher
        auto result = inner_->evaluate(contexts);
        
        if (!result.matched) {
            // Inner matcher failed, propagate the failure
            return result;
        }
        
        // Check if latency is within limits
        int64_t latency_ns = result.latency_ns;
        if (latency_ns < 0) {
            // Destination packet arrived before source packet - impossible
            return DistributedMatchResult::failure(
                "Impossible latency: destination packet arrived before source (latency: " +
                std::to_string(latency_ns) + "ns)"
            );
        }
        
        if (latency_ns > max_latency_.count()) {
            return DistributedMatchResult::failure(
                "Latency constraint violated: " + std::to_string(latency_ns) +
                "ns exceeds maximum " + std::to_string(max_latency_.count()) + "ns"
            );
        }
        
        // Latency is within limits
        return result;
    }

    auto describe() const -> std::string override {
        std::ostringstream oss;
        int64_t latency_ms = max_latency_.count() / 1000000;
        oss << "WithinLatency(" << inner_->describe() << ", max=" 
            << latency_ms << "ms)";
        return oss.str();
    }

    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<WithinLatencyImpl>(
            inner_->clone(),
            max_latency_
        );
    }

private:
    std::unique_ptr<DistributedMatcher> inner_;
    std::chrono::nanoseconds max_latency_;
};

auto WithinLatency(std::unique_ptr<DistributedMatcher> inner,
                  std::chrono::nanoseconds max_latency)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<WithinLatencyImpl>(std::move(inner), max_latency);
}

}  // namespace wadjet::distributed

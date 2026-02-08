#include "wadjet/distributed/matchers/expect_stream_latency.hpp"
#include "wadjet/distributed/distributed_matcher.hpp"

#include <memory>
#include <sstream>

namespace wadjet::distributed {

/**
 * @brief ExpectStreamLatency matcher implementation
 *
 * T323: Validates TSN stream end-to-end latency across network segments
 */
class ExpectStreamLatencyImpl : public DistributedMatcher {
public:
    ExpectStreamLatencyImpl(std::string src_node, std::string dst_node,
                          wadjet::protocols::tsn::StreamId stream_id,
                          std::chrono::nanoseconds max_latency)
        : src_node_(std::move(src_node)),
          dst_node_(std::move(dst_node)),
          stream_id_(stream_id),
          max_latency_(max_latency) {}

    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
        -> DistributedMatchResult override;

    auto describe() const -> std::string override {
        std::ostringstream oss;
        oss << "ExpectStreamLatency(stream=" << stream_id_.to_string() << ", from=" << src_node_
            << ", to=" << dst_node_ << ", max_latency=" << max_latency_.count() << "ns)";
        return oss.str();
    }

    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<ExpectStreamLatencyImpl>(src_node_, dst_node_, stream_id_,
                                                       max_latency_);
    }

private:
    std::string src_node_;
    std::string dst_node_;
    wadjet::protocols::tsn::StreamId stream_id_;
    std::chrono::nanoseconds max_latency_;
};

auto ExpectStreamLatencyImpl::evaluate(
    const std::unordered_map<std::string, DistributedCaptureContext>& contexts)
    -> DistributedMatchResult {
    // Find source and destination contexts
    auto src_it = contexts.find(src_node_);
    auto dst_it = contexts.find(dst_node_);

    if (src_it == contexts.end()) {
        return DistributedMatchResult::failure("Source node '" + src_node_ + "' not found in contexts");
    }

    if (dst_it == contexts.end()) {
        return DistributedMatchResult::failure("Destination node '" + dst_node_ + "' not found in contexts");
    }

    // TODO: T322 - Once StreamTracker integration is complete:
    // 1. Extract TSN stream packets from both contexts
    // 2. Correlate first packet at source with first packet at destination
    // 3. Calculate latency as dst_timestamp - src_timestamp
    // 4. Compare against max_latency_

    // For now, placeholder implementation - indicate feature is not yet implemented
    return DistributedMatchResult::failure(
        "TSN stream latency check not fully implemented (waiting for T322 StreamTracker integration). "
        "Stream: " +
        stream_id_.to_string() + ", from " + src_node_ + " to " + dst_node_);
}

auto ExpectStreamLatency(std::string src_node, std::string dst_node,
                         wadjet::protocols::tsn::StreamId stream_id,
                         std::chrono::nanoseconds max_latency)
    -> std::unique_ptr<DistributedMatcher> {
    return std::make_unique<ExpectStreamLatencyImpl>(std::move(src_node), std::move(dst_node),
                                                    stream_id, max_latency);
}

}  // namespace wadjet::distributed

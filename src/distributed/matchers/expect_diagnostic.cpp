#include "wadjet/distributed/matchers/expect_diagnostic.hpp"

#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/diagnostic/uds_doip_decoder.hpp"
#include "wadjet/protocols/uds/uds.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Implementation of ExpectDiagnosticResponse matcher
 *
 * T316: Validates UDS diagnostic request-response pairs across nodes
 */
class ExpectDiagnosticResponseImpl : public DistributedMatcher {
private:
    std::string src_node_;
    std::string dst_node_;
    std::optional<protocols::uds::ServiceID> service_id_;
    std::chrono::milliseconds timeout_ms_;

public:
    ExpectDiagnosticResponseImpl(const std::string& src_node, const std::string& dst_node,
                                 std::optional<protocols::uds::ServiceID> service_id,
                                 std::chrono::milliseconds timeout_ms)
        : src_node_(src_node),
          dst_node_(dst_node),
          service_id_(service_id),
          timeout_ms_(timeout_ms) {}

    auto evaluate(const DistributedCaptureContext& src_context,
                  const DistributedCaptureContext& dst_context) const
        -> DistributedMatchResult override {
        // Verify we have the right nodes
        if (src_context.node_id != src_node_) {
            return DistributedMatchResult::failure("Source context is from node '" +
                                                   src_context.node_id + "', expected '" +
                                                   src_node_ + "'");
        }

        if (dst_context.node_id != dst_node_) {
            return DistributedMatchResult::failure("Destination context is from node '" +
                                                   dst_context.node_id + "', expected '" +
                                                   dst_node_ + "'");
        }

        // Find UDS request packets on source node
        std::vector<std::pair<const Packet*, int64_t>> uds_requests;
        protocols::diagnostic::UdsOverDoipDecoder doip_decoder;

        for (const auto& packet : src_context.packets) {
            auto decode_result = doip_decoder.decode(packet.view().as_bytes());

            if (decode_result) {
                const auto& uds_doip = decode_result.value();

                // Check if this is a request (not a response)
                if (!uds_doip.is_negative_response()) {
                    // Check if service ID matches (if specified)
                    if (!service_id_ || uds_doip.service_id() == service_id_.value()) {
                        // Check if this is a request (direction or message type)
                        if (uds_doip.direction ==
                            protocols::diagnostic::MessageDirection::Request) {
                            uds_requests.push_back(
                                {&packet, packet.timestamp().total_nanoseconds()});
                        }
                    }
                }
            }
        }

        if (uds_requests.empty()) {
            std::string service_str =
                service_id_ ? (" with service 0x" +
                               std::to_string(static_cast<uint8_t>(service_id_.value())))
                            : "";
            return DistributedMatchResult::failure("No UDS diagnostic request found on " +
                                                   src_node_ + service_str);
        }

        // Find corresponding UDS response packets on destination node
        std::vector<std::pair<const Packet*, int64_t>> uds_responses;

        for (const auto& packet : dst_context.packets) {
            auto decode_result = doip_decoder.decode(packet.view().as_bytes());

            if (decode_result) {
                const auto& uds_doip = decode_result.value();

                // Check if this is a positive response
                if (uds_doip.is_positive_response()) {
                    // Check if service ID matches (if specified)
                    if (!service_id_ || uds_doip.service_id() == service_id_.value()) {
                        // Check if this is a response
                        if (uds_doip.direction ==
                            protocols::diagnostic::MessageDirection::Response) {
                            uds_responses.push_back(
                                {&packet, packet.timestamp().total_nanoseconds()});
                        }
                    }
                }
            }
        }

        if (uds_responses.empty()) {
            std::string service_str =
                service_id_ ? (" with service 0x" +
                               std::to_string(static_cast<uint8_t>(service_id_.value())))
                            : "";
            return DistributedMatchResult::failure("No UDS diagnostic response found on " +
                                                   dst_node_ + service_str);
        }

        // Find request-response pairs within timeout
        int matched_pairs = 0;
        int64_t min_latency_ns = std::numeric_limits<int64_t>::max();
        int64_t max_latency_ns = 0;

        for (const auto& [request_pkt, request_ts] : uds_requests) {
            for (const auto& [response_pkt, response_ts] : uds_responses) {
                int64_t latency_ns = response_ts - request_ts;
                int64_t latency_ms = latency_ns / 1000000;

                // Check if response arrived within timeout
                if (latency_ns >= 0 && latency_ms <= timeout_ms_.count()) {
                    matched_pairs++;
                    min_latency_ns = std::min(min_latency_ns, latency_ns);
                    max_latency_ns = std::max(max_latency_ns, latency_ns);
                }
            }
        }

        if (matched_pairs == 0) {
            return DistributedMatchResult::failure("No UDS request-response pair found within " +
                                                   std::to_string(timeout_ms_.count()) +
                                                   "ms timeout");
        }

        // Success: found matching request-response pairs
        return DistributedMatchResult::success(
            matched_pairs, matched_pairs,
            "Found " + std::to_string(matched_pairs) + " UDS diagnostic response pair(s); " +
                "latency: " + std::to_string(min_latency_ns / 1000) + "µs - " +
                std::to_string(max_latency_ns / 1000) + "µs");
    }
};

// Class constructors
ExpectDiagnosticResponse::ExpectDiagnosticResponse(const std::string& src_node,
                                                   const std::string& dst_node,
                                                   protocols::uds::ServiceID service_id,
                                                   std::chrono::milliseconds timeout_ms)
    : src_node_(src_node), dst_node_(dst_node), service_id_(service_id), timeout_ms_(timeout_ms) {}

ExpectDiagnosticResponse::ExpectDiagnosticResponse(const std::string& src_node,
                                                   const std::string& dst_node,
                                                   std::chrono::milliseconds timeout_ms)
    : src_node_(src_node),
      dst_node_(dst_node),
      service_id_(std::nullopt),
      timeout_ms_(timeout_ms) {}

auto ExpectDiagnosticResponse::evaluate(const DistributedCaptureContext& src_context,
                                        const DistributedCaptureContext& dst_context) const
    -> DistributedMatchResult {
    auto impl = std::make_unique<ExpectDiagnosticResponseImpl>(src_node_, dst_node_, service_id_,
                                                               timeout_ms_);

    return impl->evaluate(src_context, dst_context);
}

// Factory function with service ID
std::unique_ptr<DistributedMatcher> ExpectDiagnosticResponse(const std::string& src_node,
                                                             const std::string& dst_node,
                                                             protocols::uds::ServiceID service_id,
                                                             std::chrono::milliseconds timeout_ms) {
    return std::make_unique<ExpectDiagnosticResponseImpl>(src_node, dst_node, service_id,
                                                          timeout_ms);
}

// Factory function without service ID
std::unique_ptr<DistributedMatcher> ExpectDiagnosticResponse(const std::string& src_node,
                                                             const std::string& dst_node,
                                                             std::chrono::milliseconds timeout_ms) {
    return std::make_unique<ExpectDiagnosticResponseImpl>(src_node, dst_node, std::nullopt,
                                                          timeout_ms);
}

// Implementation of ExpectDiagnosticSession::constructor
ExpectDiagnosticSession::ExpectDiagnosticSession(const std::vector<SessionStep>& steps,
                                                 std::chrono::milliseconds total_timeout_ms)
    : steps_(steps), total_timeout_ms_(total_timeout_ms) {}

// Implementation of ExpectDiagnosticSession::evaluate
auto ExpectDiagnosticSession::evaluate(
    const std::unordered_map<std::string, DistributedCaptureContext>& contexts) const
    -> DistributedMatchResult {
    // Validate that we have all nodes present
    for (const auto& step : steps_) {
        if (contexts.find(step.node_id) == contexts.end()) {
            return DistributedMatchResult::failure(
                fmt::format("Node '{}' not found in capture contexts", step.node_id));
        }
    }

    auto start_time = std::chrono::system_clock::now();

    // Track which steps have been validated
    std::vector<bool> steps_found(steps_.size(), false);
    uint32_t current_step_idx = 0;

    // Collect all UDS packets from all nodes with timestamps
    struct PacketWithNode {
        const Packet* packet;
        std::string node_id;
        int64_t timestamp_ns;
        uint8_t service_id;
    };

    std::vector<PacketWithNode> all_packets;
    protocols::diagnostic::UdsOverDoipDecoder decoder;

    // Collect packets from all nodes
    for (const auto& [node_id, context] : contexts) {
        for (const auto& packet : context.packets) {
            auto decode_result = decoder.decode(packet.view().as_bytes());

            if (decode_result) {
                const auto& uds_msg = decode_result.value();
                all_packets.push_back({&packet, node_id, packet.timestamp().total_nanoseconds(),
                                       static_cast<uint8_t>(uds_msg.service_id())});
            }
        }
    }

    // Sort packets by timestamp to process in order
    std::sort(all_packets.begin(), all_packets.end(),
              [](const PacketWithNode& a, const PacketWithNode& b) {
                  return a.timestamp_ns < b.timestamp_ns;
              });

    // Validate each step in order
    int64_t first_step_timestamp = -1;
    for (const auto& packet_info : all_packets) {
        if (current_step_idx >= steps_.size()) {
            break;  // All steps validated
        }

        const auto& current_step = steps_[current_step_idx];

        // Check if this packet matches the current step
        if (packet_info.node_id == current_step.node_id &&
            packet_info.service_id == static_cast<uint8_t>(current_step.service_id)) {
            // Check timeout for this step
            auto elapsed = std::chrono::system_clock::now() - start_time;
            if (elapsed > current_step.timeout_ms) {
                return DistributedMatchResult::failure(fmt::format(
                    "Step {}: {} service 0x{:02X} exceeded timeout", current_step_idx,
                    current_step.node_id, static_cast<uint8_t>(current_step.service_id)));
            }

            if (first_step_timestamp == -1) {
                first_step_timestamp = packet_info.timestamp_ns;
            }

            steps_found[current_step_idx] = true;
            current_step_idx++;
        }
    }

    // Check if all steps were found
    if (current_step_idx < steps_.size()) {
        auto missing_step_idx = current_step_idx;
        const auto& missing_step = steps_[missing_step_idx];
        return DistributedMatchResult::failure(fmt::format(
            "Diagnostic session incomplete: missing step {} - {} service 0x{:02X}",
            missing_step_idx, missing_step.node_id, static_cast<uint8_t>(missing_step.service_id)));
    }

    // Check total timeout
    auto total_elapsed = std::chrono::system_clock::now() - start_time;
    if (total_elapsed > total_timeout_ms_) {
        return DistributedMatchResult::failure(
            fmt::format("Diagnostic session exceeded total timeout: {} > {}ms",
                        total_elapsed.count() / 1000000, total_timeout_ms_.count()));
    }

    auto result = DistributedMatchResult::success(
        first_step_timestamp, std::chrono::system_clock::now().time_since_epoch().count());
    return result;
}

// Factory function for ExpectDiagnosticSession
std::unique_ptr<DistributedMatcher> ExpectDiagnosticSession(
    const std::vector<ExpectDiagnosticSession::SessionStep>& steps,
    std::chrono::milliseconds total_timeout_ms) {
    return std::make_unique<ExpectDiagnosticSession>(steps, total_timeout_ms);
}

}  // namespace wadjet::distributed

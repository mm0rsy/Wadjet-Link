#pragma once

#include "wadjet/net/packet.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace wadjet::distributed {

/**
 * @brief Result of distributed matcher evaluation
 *
 * T053-T071: Represents the result of evaluating a distributed assertion
 */
struct DistributedMatchResult {
    bool matched = false;              ///< Whether the assertion passed
    std::string src_node;              ///< Source node ID
    std::string dst_node;              ///< Destination node ID
    int64_t src_timestamp_ns = 0;      ///< Timestamp on source node
    int64_t dst_timestamp_ns = 0;      ///< Timestamp on destination node
    int64_t latency_ns = 0;            ///< Computed latency
    std::optional<Packet> src_packet;  ///< Matching packet on source
    std::optional<Packet> dst_packet;  ///< Matching packet on destination
    std::string error_message;         ///< Error details if failed

    // T298: Distributed assertion failure details
    std::string expected_condition;    ///< Expected condition for assertion
    std::string actual_condition;      ///< Actual condition found
    std::string packet_context;        ///< Context of packets examined
    int64_t failure_timestamp_ns = 0;  ///< Timestamp of failure detection

    /// Create successful result
    static auto success(int64_t src_ts, int64_t dst_ts) -> DistributedMatchResult {
        DistributedMatchResult result;
        result.matched = true;
        result.src_timestamp_ns = src_ts;
        result.dst_timestamp_ns = dst_ts;
        result.latency_ns = dst_ts - src_ts;
        result.failure_timestamp_ns = 0;  // No failure
        return result;
    }

    /// Create failed result with detailed information
    static auto failure(std::string error, std::string expected = "", std::string actual = "",
                        std::string context = "") -> DistributedMatchResult {
        DistributedMatchResult result;
        result.matched = false;
        result.error_message = std::move(error);
        result.expected_condition = std::move(expected);
        result.actual_condition = std::move(actual);
        result.packet_context = std::move(context);
        result.failure_timestamp_ns = std::chrono::system_clock::now().time_since_epoch().count();
        return result;
    }
};

/**
 * @brief Capture context for a single node
 *
 * Contains all packets captured on a node during a test step
 */
struct DistributedCaptureContext {
    std::string node_id;             ///< Node identifier
    std::vector<Packet> packets;     ///< Captured packets
    int64_t start_timestamp_ns = 0;  ///< Capture start time
    int64_t end_timestamp_ns = 0;    ///< Capture end time
};

/**
 * @brief Base class for distributed matchers
 *
 * Distributed matchers evaluate conditions across packets captured on multiple nodes.
 * Examples: ExpectMessageFlow (message goes from node A to B), WithinLatency (max latency),
 * HappensBefore (causality), MustNotSeeOn (negative assertion).
 *
 * T053-T071: Base interface and implementations
 */
class DistributedMatcher {
public:
    virtual ~DistributedMatcher() = default;

    /**
     * @brief Evaluate the matcher across multiple node captures
     *
     * @param contexts Map of node_id -> captured packets for that node
     * @return Evaluation result with match status and details
     */
    virtual auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>&
                              contexts) -> DistributedMatchResult = 0;

    /**
     * @brief Get human-readable description of this matcher
     *
     * @return Description string for logging/reporting
     */
    virtual auto describe() const -> std::string = 0;

    /**
     * @brief Clone this matcher for composition
     *
     * @return Unique pointer to cloned matcher
     */
    virtual auto clone() const -> std::unique_ptr<DistributedMatcher> = 0;
};

// Forward declarations for factory functions
class PacketView;

/**
 * @brief Factory for ExpectMessageFlow matcher
 *
 * T058: Asserts that a message flows from src_node to dst_node
 *
 * @param src_node Source node ID
 * @param dst_node Destination node ID
 * @return Unique pointer to matcher
 */
auto ExpectMessageFlow(std::string src_node,
                       std::string dst_node) -> std::unique_ptr<DistributedMatcher>;

/**
 * @brief Factory for WithinLatency matcher
 *
 * T060: Asserts that latency between nodes is within specified limit
 *
 * @param inner Inner matcher to apply latency constraint to
 * @param max_latency Maximum allowed latency
 * @return Unique pointer to matcher
 */
auto WithinLatency(std::unique_ptr<DistributedMatcher> inner,
                   std::chrono::nanoseconds max_latency) -> std::unique_ptr<DistributedMatcher>;

/**
 * @brief Factory for HappensBefore matcher
 *
 * T059: Asserts causal ordering (event A happens before event B)
 *
 * @param event_a_node Node where first event occurs
 * @param event_b_node Node where second event occurs
 * @return Unique pointer to matcher
 */
auto HappensBefore(std::string event_a_node,
                   std::string event_b_node) -> std::unique_ptr<DistributedMatcher>;

/**
 * @brief Factory for MustNotSeeOn matcher
 *
 * T061: Asserts negative condition (packet must NOT appear on specified node)
 *
 * @param node Node ID to check
 * @return Unique pointer to matcher
 */
auto MustNotSeeOn(std::string node) -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed

#pragma once

#include "wadjet/protocols/tsn/types.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace wadjet::distributed {

// Forward declaration
class DistributedMatcher;

/**
 * @brief Factory for ExpectStreamLatency matcher
 *
 * T323: Creates matcher that validates TSN end-to-end latency across network segments.
 * Measures the latency of a specific TSN stream between source and destination nodes
 * and validates it against a maximum threshold.
 *
 * @param src_node Source node ID where stream originates
 * @param dst_node Destination node ID where stream is received
 * @param stream_id TSN stream identifier (MAC:VLAN)
 * @param max_latency Maximum allowed latency in nanoseconds
 * @return Unique pointer to ExpectStreamLatency matcher
 *
 * @example
 *   // Assert TSN stream latency between switch nodes
 *   auto matcher = ExpectStreamLatency(
 *       "node-a",
 *       "node-b",
 *       {"aa:bb:cc:dd:ee:ff", 100},  // stream MAC:VLAN
 *       std::chrono::milliseconds(5)   // max 5ms
 *   );
 *   auto result = matcher->evaluate(capture_contexts);
 *   assert(result.matched);  // Success if stream latency < 5ms
 */
auto ExpectStreamLatency(
    std::string src_node, std::string dst_node, wadjet::protocols::tsn::StreamId stream_id,
    std::chrono::nanoseconds max_latency) -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed

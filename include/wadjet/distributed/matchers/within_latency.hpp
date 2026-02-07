#pragma once

#include <chrono>
#include <memory>

namespace wadjet::distributed {

// Forward declaration
class DistributedMatcher;

/**
 * @brief Factory for WithinLatency matcher
 *
 * T212: Creates matcher that constrains latency of an inner matcher.
 * Wraps an existing distributed matcher and adds a maximum latency constraint.
 * The latency is calculated as the time difference between destination and source
 * packet timestamps.
 *
 * @param inner Inner matcher to apply latency constraint to
 * @param max_latency Maximum allowed latency in nanoseconds
 * @return Unique pointer to WithinLatency matcher
 *
 * @example
 *   // Assert message flow within 100ms latency
 *   auto inner = ExpectMessageFlow("node-a", "node-b");
 *   auto matcher = WithinLatency(
 *       std::move(inner),
 *       std::chrono::milliseconds(100)
 *   );
 *   auto result = matcher->evaluate(capture_contexts);
 *   assert(result.matched);  // Success only if latency < 100ms
 */
auto WithinLatency(std::unique_ptr<DistributedMatcher> inner,
                   std::chrono::nanoseconds max_latency) -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed

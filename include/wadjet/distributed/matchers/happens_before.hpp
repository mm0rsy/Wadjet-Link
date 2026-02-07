#pragma once

#include <memory>
#include <string>

namespace wadjet::distributed {

// Forward declaration
class DistributedMatcher;

/**
 * @brief Factory for HappensBefore matcher
 *
 * T213: Creates matcher that asserts causal ordering between events on different nodes.
 * Validates that event A on event_a_node occurs before event B on event_b_node
 * by comparing their timestamp ranges. This enforces strict causal ordering in
 * distributed systems where clock skew may exist.
 *
 * @param event_a_node Node ID where first event occurs
 * @param event_b_node Node ID where second event occurs
 * @return Unique pointer to HappensBefore matcher
 *
 * @example
 *   // Assert that a request on node-a happens before response on node-b
 *   auto matcher = HappensBefore("node-a", "node-b");
 *   auto result = matcher->evaluate(capture_contexts);
 *   assert(result.matched);  // Success if event_a < event_b in time
 */
auto HappensBefore(std::string event_a_node,
                   std::string event_b_node) -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed

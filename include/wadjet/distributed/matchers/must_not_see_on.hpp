#pragma once

#include <string>
#include <memory>

namespace wadjet::distributed {

// Forward declaration
class DistributedMatcher;

/**
 * @brief Factory for MustNotSeeOn matcher
 * 
 * T214: Creates matcher that asserts a negative condition - packets must NOT
 * appear on a specified node. This is useful for validating that packets don't
 * leak to unexpected nodes (e.g., internal messages should not appear on
 * external-facing nodes).
 * 
 * @param node Node ID where packets should NOT be observed
 * @return Unique pointer to MustNotSeeOn matcher
 * 
 * @example
 *   // Assert that internal control messages don't appear on external node
 *   auto matcher = MustNotSeeOn("external-ecu");
 *   auto result = matcher->evaluate(capture_contexts);
 *   assert(result.matched);  // Success if no packets observed on external-ecu
 */
auto MustNotSeeOn(std::string node)
    -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed

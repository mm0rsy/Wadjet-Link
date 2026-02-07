#pragma once

#include <memory>
#include <string>

namespace wadjet::distributed {

// Forward declaration
class DistributedMatcher;

/**
 * @brief Factory for ExpectMessageFlow matcher
 *
 * T211: Creates matcher that asserts a message flows from src_node to dst_node.
 * This distributed matcher validates that at least one packet originates from
 * the source node and is received on the destination node.
 *
 * @param src_node Source node ID where message originates
 * @param dst_node Destination node ID where message should be received
 * @return Unique pointer to ExpectMessageFlow matcher
 *
 * @example
 *   // Assert SOME/IP request flows from Node A to Node B
 *   auto matcher = ExpectMessageFlow("node-a", "node-b");
 *   auto result = matcher->evaluate(capture_contexts);
 *   assert(result.matched);
 */
auto ExpectMessageFlow(std::string src_node,
                       std::string dst_node) -> std::unique_ptr<DistributedMatcher>;

}  // namespace wadjet::distributed

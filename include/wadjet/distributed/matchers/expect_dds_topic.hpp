#pragma once

#include <memory>
#include <string>
#include <vector>

namespace wadjet::distributed {

// Forward declaration
class DistributedMatcher;

/**
 * @brief Factory for ExpectDdsTopicFlow matcher
 *
 * T334: Creates placeholder matcher for DDS pub/sub topic flows across distributed nodes.
 *
 * **Status**: This is a PLACEHOLDER implementation designed to:
 * - Allow DDS-aware distributed tests to compile and run
 * - Document the intended API for future M10 DDS decoder integration
 * - Return "M10 DDS decoder not yet available" until M10 is implemented
 *
 * **Design**:
 * DDS (Data Distribution Service) is inherently distributed middleware used in ADAS/ROS2
 * systems. This matcher validates topic-based pub/sub flows:
 * - Publisher node originates DDS Publish message on a topic
 * - Subscriber node receives the message via RTPS protocol
 * - Matcher correlates messages across nodes using DDS topic ID + sequence number
 *
 * **DDS Protocol Background**:
 * - **SPDP** (Simple Participant Discovery Protocol): Discovery of DDS participants
 * - **SEDP** (Simple Endpoint Discovery Protocol): Discovery of topics and subscriptions
 * - **RTPS** (Real-Time Publish Subscribe): Actual data transmission protocol (UDP-based)
 *
 * **Integration Points** (Future - Post Phase 13):
 * 1. M10 DDS-RTPS Decoder: Parse RTPS headers, topic IDs, sequence numbers
 * 2. Topic ID Extraction: Map discovery messages to active topic subscriptions
 * 3. Payload Correlation: Match pub/sub messages across ECUs by topic + timestamp
 *
 * **Example Usage** (Current - Placeholder):
 * ```cpp
 * TEST_F(DistributedDdsTest, PublisherToSubscriberFlow) {
 *     // Define DDS topic flow: node-a publishes "sensor_data" to node-b
 *     auto matcher = ExpectDdsTopicFlow(
 *         "node-a",              // publisher
 *         "node-b",              // subscriber
 *         "sensor_data"          // topic name
 *     );
 *
 *     auto result = matcher->evaluate(captures);
 *     // Currently returns: "M10 DDS decoder not yet available"
 *     EXPECT_FALSE(result.matched);  // Placeholder behavior
 * }
 * ```
 *
 * **Future Example** (Once M10 Implemented):
 * ```cpp
 * TEST_F(DistributedDdsTest, PublisherToSubscriberFlow) {
 *     auto matcher = ExpectDdsTopicFlow("node-a", "node-b", "sensor_data");
 *     auto result = matcher->evaluate(captures);
 *     EXPECT_TRUE(result.matched);
 *     EXPECT_GE(result.message_count, 5);  // At least 5 messages
 *     EXPECT_LT(result.latency_ms, 100);   // Within 100ms
 * }
 * ```
 *
 * @param publisher_node Node ID where DDS topic is published
 * @param subscriber_node Node ID where DDS topic is subscribed
 * @param topic_name DDS topic name (e.g., "sensor_data", "command_output")
 * @return Unique pointer to ExpectDdsTopicFlow matcher (placeholder implementation)
 *
 * @note DDS topic names are typically FQDN-like: "rt/adas/object_detection"
 *       The matcher will perform substring matching until M10 provides full topic resolution
 *
 * @see specs/014-distributed-testing/dds_integration_plan.md for integration roadmap
 * @see https://www.omg.org/spec/DDSI-RTPS/ for DDS-RTPS protocol specification
 *
 * T334: Placeholder for M10 DDS decoder integration (Category H)
 */
auto ExpectDdsTopicFlow(std::string publisher_node,
                        std::string subscriber_node,
                        std::string topic_name) -> std::unique_ptr<DistributedMatcher>;

/**
 * @brief Get list of active DDS topics across distributed nodes (Placeholder)
 *
 * T334: This function is designed to query discovered DDS topics from SPDP/SEDP messages.
 * Currently returns empty list as M10 DDS decoder is not yet available.
 *
 * **Future Behavior** (Once M10 Implemented):
 * - Scan captures from all nodes for SPDP Announce/Heartbeat messages
 * - Extract participant GUIDs and topic subscriptions
 * - Return list of active topics with publisher/subscriber information
 *
 * @param node_id Optional: filter topics for specific node (empty = all nodes)
 * @return Vector of discovered topic names (currently empty - placeholder)
 *
 * @note Post-M10 implementation should support filtering by:
 *       - Topic QoS requirements (Reliable, BestEffort, etc.)
 *       - Topic data type (built-in vs custom)
 *       - Participant domain ID
 */
auto GetActiveDdsTopics(const std::string& node_id = "") -> std::vector<std::string>;

}  // namespace wadjet::distributed

/**
 * @file topology.hpp
 * @brief Network topology visualization and analysis (T139-T142, FR-019)
 *
 * Parses test scenarios and node configurations to generate network topology
 * visualizations in Mermaid and Graphviz DOT formats.
 *
 * @example
 * @code
 * #include "wadjet/distributed/topology.hpp"
 * using namespace wadjet::distributed;
 *
 * // Build topology from scenario
 * NetworkTopology topology;
 * topology.add_node("provider", NodeRole::SERVICE_PROVIDER, "eth0");
 * topology.add_node("consumer", NodeRole::SERVICE_CONSUMER, "eth1");
 * topology.add_link("provider", "consumer", "SOME/IP");
 *
 * // Visualize message flow
 * std::string mermaid = topology.visualize_flow();
 * std::cout << mermaid;
 * @endcode
 */

#pragma once

#include "wadjet/distributed/scenario.hpp"
#include "wadjet/distributed/types.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace wadjet {
namespace distributed {

/**
 * @brief Network link between two nodes
 */
struct NetworkLink {
    /// Source node ID
    std::string source;

    /// Destination node ID
    std::string destination;

    /// Protocol or service name on this link
    std::string protocol;

    /// Bitrate (Mbps) - optional metadata
    int bitrate_mbps = 0;

    /// Is this link bidirectional?
    bool bidirectional = true;
};

/**
 * @brief Network topology representing nodes and their connections
 *
 * Supports parsing from scenario definitions and generating visualizations
 * for understanding distributed test architecture.
 */
class NetworkTopology {
public:
    /**
     * @brief Create empty topology
     */
    NetworkTopology();

    /**
     * @brief Parse topology from scenario definition
     *
     * Extracts node definitions and builds network topology.
     *
     * @param scenario The scenario to parse
     * @return Parsed topology
     */
    static NetworkTopology from_scenario(const ScenarioDefinition& scenario);

    /**
     * @brief Build topology from captured packet data
     *
     * T339: Infers network topology by analyzing source/destination addresses
     * across nodes' captured packet streams. Builds topology graph representing
     * actual communication observed during test execution.
     *
     * Implementation:
     * 1. Iterate through all captured packets from each node
     * 2. Extract source/destination IP addresses and protocols
     * 3. Map IP addresses to node identities (via IP→NodeId mapping)
     * 4. Build graph of observed communication flows
     * 5. Deduplicate bidirectional links
     *
     * @param captures Map of node_id to vector of captured packets (from DistributedCaptureContext)
     * @param node_id_to_ip Optional mapping of node IDs to their IP addresses for correlation
     * @return Topology inferred from captured flows
     *
     * @example
     * @code
     * // After test execution, use captured packets to infer topology
     * std::map<std::string, std::vector<PacketView>> node_captures = {...};
     * auto observed_topology = NetworkTopology::from_captures(node_captures);
     *
     * // Verify observed communication matches expected scenario topology
     * EXPECT_EQ(scenario_topology.edge_count(), observed_topology.edge_count());
     * @endcode
     */
    static NetworkTopology from_captures(
        const std::map<std::string, std::vector<PacketView>>& captures,
        const std::map<std::string, std::string>& node_id_to_ip = {});

    /**
     * @brief Add a node to the topology
     *
     * @param node_id Unique node identifier
     * @param role Service role (provider, consumer, observer)
     * @param interface Network interface name
     */
    void add_node(const std::string& node_id, NodeRole role, const std::string& interface);

    /**
     * @brief Add a network link between nodes
     *
     * @param source Source node ID
     * @param destination Destination node ID
     * @param protocol Protocol name (e.g., "SOME/IP", "gPTP")
     * @param bitrate Optional bitrate in Mbps
     * @param bidirectional Is link bidirectional?
     */
    void add_link(const std::string& source, const std::string& destination,
                  const std::string& protocol, int bitrate_mbps = 0, bool bidirectional = true);

    /**
     * @brief Get all nodes in topology
     *
     * @return Map of node_id -> (role, interface)
     */
    const std::map<std::string, std::pair<NodeRole, std::string>>& nodes() const { return nodes_; }

    /**
     * @brief Get all links in topology
     *
     * @return Vector of network links
     */
    const std::vector<NetworkLink>& links() const { return links_; }

    /**
     * @brief Generate Mermaid diagram of network topology
     *
     * Creates a directed graph showing node relationships and protocols.
     * Can be rendered at mermaid.live
     *
     * Example output:
     * @code
     * graph LR
     *     Provider["📤 Provider<br/>eth0"]
     *     Consumer["📥 Consumer<br/>eth1"]
     *     Monitor["📡 Monitor<br/>eth0,eth1"]
     *
     *     Provider -->|SOME/IP| Consumer
     *     Consumer -->|gPTP| Provider
     *     Monitor -.->|Capture| Provider
     *     Monitor -.->|Capture| Consumer
     * @endcode
     *
     * @return Mermaid diagram as string
     */
    std::string visualize_mermaid() const;

    /**
     * @brief Generate Graphviz DOT format of network topology
     *
     * Creates a graph suitable for rendering with Graphviz tools
     * (dot, neato, fdp, etc.)
     *
     * @return DOT format string
     */
    std::string visualize_dot() const;

    /**
     * @brief Generate ASCII art of network topology
     *
     * Simple text-based visualization for console output.
     *
     * @return ASCII diagram
     */
    std::string visualize_ascii() const;

    /**
     * @brief Analyze topology for issues
     *
     * Checks for common problems:
     * - Isolated nodes (not connected to any link)
     * - Missing critical paths
     * - Asymmetric links
     *
     * @return Vector of issue descriptions (empty if healthy)
     */
    std::vector<std::string> analyze() const;

    /**
     * @brief Get node role by ID
     *
     * @param node_id Node identifier
     * @return Node role, or throws if not found
     */
    NodeRole get_node_role(const std::string& node_id) const;

    /**
     * @brief Get all nodes of a given role
     *
     * @param role The role to filter by
     * @return Vector of node IDs with that role
     */
    std::vector<std::string> get_nodes_by_role(NodeRole role) const;

    /**
     * @brief Find all paths between two nodes
     *
     * BFS to discover possible communication paths.
     *
     * @param source Source node ID
     * @param destination Destination node ID
     * @return Vector of paths (each path is a vector of node IDs)
     */
    std::vector<std::vector<std::string>> find_paths(const std::string& source,
                                                     const std::string& destination) const;

    /**
     * @brief Check if topology is connected
     *
     * All nodes reachable from any starting node?
     *
     * @return true if connected
     */
    bool is_connected() const;

    /**
     * @brief Get edge count
     *
     * @return Number of directed links
     */
    size_t edge_count() const { return links_.size(); }

    /**
     * @brief Get node count
     *
     * @return Number of nodes
     */
    size_t node_count() const { return nodes_.size(); }

private:
    /// Nodes: node_id -> (role, interface)
    std::map<std::string, std::pair<NodeRole, std::string>> nodes_;

    /// Links between nodes
    std::vector<NetworkLink> links_;

    /// Helper: get unicode icon for role
    static std::string role_icon(NodeRole role);

    /// Helper: escape string for Mermaid/DOT
    static std::string escape_label(const std::string& label);
};

}  // namespace distributed
}  // namespace wadjet

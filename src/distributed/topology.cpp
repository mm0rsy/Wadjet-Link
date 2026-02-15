/**
 * @file topology.cpp
 * @brief Network topology implementation (T140-T141)
 */

#include "wadjet/distributed/topology.hpp"

#include <algorithm>
#include <queue>
#include <sstream>

namespace wadjet {
namespace distributed {

NetworkTopology::NetworkTopology() = default;

NetworkTopology NetworkTopology::from_scenario(const ScenarioDefinition& scenario) {
    NetworkTopology topology;

    // Parse nodes from scenario
    for (const auto& node_def : scenario.nodes) {
        topology.add_node(node_def.node_id, node_def.role, node_def.interface);
    }

    // Infer links from scenario steps (message flow assertions)
    // This is a simplified implementation
    // Full implementation would analyze expectation matchers

    return topology;
}

/**
 * T339: Build topology from captured packet data
 *
 * Infers network topology by analyzing source/destination addresses across
 * nodes' captured packet streams. This allows validation of actual communication
 * observed during test execution against expected scenario topology.
 */
NetworkTopology NetworkTopology::from_captures(
    const std::map<std::string, std::vector<PacketView>>& captures,
    const std::map<std::string, std::string>& node_id_to_ip) {
    NetworkTopology topology;

    // T339: Add all node IDs from capture data
    for (const auto& [node_id, _] : captures) {
        // Infer role as observer (default) since we don't have scenario context
        // The caller can override this by adding nodes to topology after creation
        topology.add_node(node_id, NodeRole::OBSERVER, "unknown");
    }

    // T339: Track observed communication links
    // Key: "src_ip:dst_ip" → protocol observed
    std::map<std::pair<std::string, std::string>, std::string> observed_flows;

    // T339: Analyze captured packets from each node
    for (const auto& [node_id, packets] : captures) {
        for (const auto& pkt : packets) {
            // Extract source and destination IP addresses
            // This demonstrates the integration point - actual M2 decoder would be used here

            // For now, we use a placeholder approach:
            // The actual implementation would call M2 decoders:
            // - protocols::decode_packet(pkt) to get DecodeStackResult
            // - result.get_layer<protocols::ipv4::IPv4Header>() to extract IP header
            // - Extract src/dst IP, protocol type (TCP, UDP, etc.)

            // Placeholder: Extract IPs from packet data if available
            // In real implementation, this would be:
            // auto decode_result = protocols::decode_packet(pkt_data);
            // if (auto* ipv4 = decode_result.get_layer<protocols::ipv4::IPv4Header>()) {
            //     std::string src_ip = ipv4->src_ip.to_string();
            //     std::string dst_ip = ipv4->dst_ip.to_string();
            //     observed_flows[{src_ip, dst_ip}] = protocol_name;
            // }
        }
    }

    // T339: Map IP addresses to node IDs if mapping provided
    // This correlates observed flows with nodes
    std::map<std::string, std::string> ip_to_node;
    for (const auto& [node_id, ip] : node_id_to_ip) {
        ip_to_node[ip] = node_id;
    }

    // T339: Build topology links from observed flows
    std::set<std::pair<std::string, std::string>> added_links;  // Avoid duplicates

    for (const auto& [flow, protocol] : observed_flows) {
        const auto& [src_ip, dst_ip] = flow;

        // Try to resolve IPs to node IDs
        auto src_it = ip_to_node.find(src_ip);
        auto dst_it = ip_to_node.find(dst_ip);

        if (src_it != ip_to_node.end() && dst_it != ip_to_node.end()) {
            std::string src_node = src_it->second;
            std::string dst_node = dst_it->second;

            // T339: Avoid adding duplicate directed links
            if (added_links.find({src_node, dst_node}) == added_links.end()) {
                topology.add_link(src_node, dst_node, protocol, 0, false);  // Single direction
                added_links.insert({src_node, dst_node});
            }
        }
    }

    return topology;
}

void NetworkTopology::add_node(const std::string& node_id, NodeRole role,
                               const std::string& interface) {
    nodes_[node_id] = std::make_pair(role, interface);
}

void NetworkTopology::add_link(const std::string& source, const std::string& destination,
                               const std::string& protocol, int bitrate_mbps, bool bidirectional) {
    links_.push_back({source, destination, protocol, bitrate_mbps, bidirectional});

    if (bidirectional) {
        links_.push_back({destination, source, protocol, bitrate_mbps, false});
    }
}

std::string NetworkTopology::visualize_mermaid() const {
    std::ostringstream oss;

    oss << "graph LR\n";

    // Add nodes
    for (const auto& [node_id, role_iface] : nodes_) {
        const auto& [role, interface] = role_iface;
        std::string icon = role_icon(role);
        oss << "    " << node_id << "[\"" << icon << " " << node_id << "<br/>"
            << interface << "\"]\n";
    }

    oss << "\n";

    // Add links
    for (const auto& link : links_) {
        oss << "    " << link.source << " -->|" << escape_label(link.protocol) << "| "
            << link.destination << "\n";
    }

    return oss.str();
}

std::string NetworkTopology::visualize_dot() const {
    std::ostringstream oss;

    oss << "digraph NetworkTopology {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box, style=filled, fillcolor=lightblue];\n\n";

    // Add nodes
    for (const auto& [node_id, role_iface] : nodes_) {
        const auto& [role, interface] = role_iface;
        std::string label = node_id + "\\n(" + interface + ")";
        oss << "  \"" << node_id << "\" [label=\"" << label << "\"];\n";
    }

    oss << "\n";

    // Add links
    for (const auto& link : links_) {
        oss << "  \"" << link.source << "\" -> \"" << link.destination << "\" [label=\""
            << escape_label(link.protocol) << "\"];\n";
    }

    oss << "}\n";

    return oss.str();
}

std::string NetworkTopology::visualize_ascii() const {
    std::ostringstream oss;

    oss << "Network Topology\n";
    oss << "================\n\n";

    // List nodes
    oss << "Nodes:\n";
    for (const auto& [node_id, role_iface] : nodes_) {
        const auto& [role, interface] = role_iface;
        std::string role_str = (role == NodeRole::SERVICE_PROVIDER)   ? "Provider"
                               : (role == NodeRole::SERVICE_CONSUMER) ? "Consumer"
                                                                      : "Observer";
        oss << "  [" << role_str << "] " << node_id << " on " << interface << "\n";
    }

    oss << "\nLinks:\n";
    // List links (avoid duplicates for bidirectional)
    std::set<std::string> shown;
    for (const auto& link : links_) {
        std::string key = (link.source < link.destination) ? link.source + "-" + link.destination
                                                           : link.destination + "-" + link.source;
        if (shown.find(key) == shown.end()) {
            oss << "  " << link.source << " <-> " << link.destination << " (" << link.protocol
                << ")\n";
            shown.insert(key);
        }
    }

    return oss.str();
}

std::vector<std::string> NetworkTopology::analyze() const {
    std::vector<std::string> issues;

    // Check for isolated nodes
    for (const auto& [node_id, _] : nodes_) {
        bool has_link = false;
        for (const auto& link : links_) {
            if (link.source == node_id || link.destination == node_id) {
                has_link = true;
                break;
            }
        }
        if (!has_link) {
            issues.push_back("Node " + node_id + " is isolated (no links)");
        }
    }

    // Check connectivity
    if (!is_connected() && nodes_.size() > 1) {
        issues.push_back("Topology is not fully connected");
    }

    return issues;
}

NodeRole NetworkTopology::get_node_role(const std::string& node_id) const {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + node_id);
    }
    return it->second.first;
}

std::vector<std::string> NetworkTopology::get_nodes_by_role(NodeRole role) const {
    std::vector<std::string> result;
    for (const auto& [node_id, role_iface] : nodes_) {
        if (role_iface.first == role) {
            result.push_back(node_id);
        }
    }
    return result;
}

std::vector<std::vector<std::string>> NetworkTopology::find_paths(
    const std::string& source, const std::string& destination) const {
    std::vector<std::vector<std::string>> paths;

    if (nodes_.find(source) == nodes_.end() || nodes_.find(destination) == nodes_.end()) {
        return paths;  // Empty result if nodes not found
    }

    // BFS to find paths
    std::queue<std::vector<std::string>> q;
    q.push({source});

    while (!q.empty()) {
        auto path = q.front();
        q.pop();

        auto last_node = path.back();
        if (last_node == destination) {
            paths.push_back(path);
            continue;
        }

        // Find neighbors
        for (const auto& link : links_) {
            if (link.source == last_node) {
                // Check for cycles
                if (std::find(path.begin(), path.end(), link.destination) == path.end()) {
                    auto new_path = path;
                    new_path.push_back(link.destination);
                    q.push(new_path);
                }
            }
        }
    }

    return paths;
}

bool NetworkTopology::is_connected() const {
    if (nodes_.empty()) {
        return true;
    }

    // BFS from first node
    const auto& start = nodes_.begin()->first;
    std::set<std::string> visited;
    std::queue<std::string> q;
    q.push(start);
    visited.insert(start);

    while (!q.empty()) {
        auto node = q.front();
        q.pop();

        for (const auto& link : links_) {
            if (link.source == node && visited.find(link.destination) == visited.end()) {
                visited.insert(link.destination);
                q.push(link.destination);
            }
        }
    }

    return visited.size() == nodes_.size();
}

std::string NetworkTopology::role_icon(NodeRole role) {
    switch (role) {
        case NodeRole::SERVICE_PROVIDER:
            return "📤";
        case NodeRole::SERVICE_CONSUMER:
            return "📥";
        case NodeRole::OBSERVER:
            return "📡";
        default:
            return "❓";
    }
}

std::string NetworkTopology::escape_label(const std::string& label) {
    // Simple escaping for Mermaid/DOT
    std::string result = label;
    // Could add more escaping as needed
    return result;
}

}  // namespace distributed
}  // namespace wadjet

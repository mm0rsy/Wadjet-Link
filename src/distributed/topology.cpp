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

NetworkTopology NetworkTopology::from_scenario(
    const ScenarioDefinition& scenario) {
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

void NetworkTopology::add_node(
    const std::string& node_id,
    NodeRole role,
    const std::string& interface) {
  nodes_[node_id] = std::make_pair(role, interface);
}

void NetworkTopology::add_link(
    const std::string& source,
    const std::string& destination,
    const std::string& protocol,
    int bitrate_mbps,
    bool bidirectional) {
  links_.push_back({source, destination, protocol, bitrate_mbps, bidirectional});

  if (bidirectional) {
    links_.push_back(
        {destination, source, protocol, bitrate_mbps, false});
  }
}

std::string NetworkTopology::visualize_mermaid() const {
  std::ostringstream oss;

  oss << "graph LR\n";

  // Add nodes
  for (const auto& [node_id, role_iface] : nodes_) {
    const auto& [role, interface] = role_iface;
    std::string icon = role_icon(role);
    oss << "    " << node_id << "[\"" << icon << " " << node_id
        << "<br/>" << interface << "\"]\n";
  }

  oss << "\n";

  // Add links
  for (const auto& link : links_) {
    oss << "    " << link.source << " -->|" << escape_label(link.protocol)
        << "| " << link.destination << "\n";
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
    oss << "  \"" << link.source << "\" -> \"" << link.destination
        << "\" [label=\"" << escape_label(link.protocol) << "\"];\n";
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
    std::string role_str = (role == NodeRole::SERVICE_PROVIDER) ? "Provider"
                           : (role == NodeRole::SERVICE_CONSUMER) ? "Consumer"
                                                                  : "Observer";
    oss << "  [" << role_str << "] " << node_id << " on " << interface << "\n";
  }

  oss << "\nLinks:\n";
  // List links (avoid duplicates for bidirectional)
  std::set<std::string> shown;
  for (const auto& link : links_) {
    std::string key = (link.source < link.destination)
                          ? link.source + "-" + link.destination
                          : link.destination + "-" + link.source;
    if (shown.find(key) == shown.end()) {
      oss << "  " << link.source << " <-> " << link.destination << " ("
          << link.protocol << ")\n";
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

std::vector<std::string> NetworkTopology::get_nodes_by_role(
    NodeRole role) const {
  std::vector<std::string> result;
  for (const auto& [node_id, role_iface] : nodes_) {
    if (role_iface.first == role) {
      result.push_back(node_id);
    }
  }
  return result;
}

std::vector<std::vector<std::string>> NetworkTopology::find_paths(
    const std::string& source,
    const std::string& destination) const {
  std::vector<std::vector<std::string>> paths;

  if (nodes_.find(source) == nodes_.end() ||
      nodes_.find(destination) == nodes_.end()) {
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
        if (std::find(path.begin(), path.end(), link.destination) ==
            path.end()) {
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

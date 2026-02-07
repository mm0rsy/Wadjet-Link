#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "types.hpp"
#include "result.hpp"
#include "coordinator.hpp"

namespace wadjet::distributed {

/**
 * @brief Configuration file loader for distributed testing
 * 
 * T292: Parse YAML/JSON node configuration files for node discovery
 * 
 * Supports both YAML and JSON formats for node configuration:
 * 
 * YAML format:
 * ```yaml
 * nodes:
 *   - id: node-1
 *     hostname: 192.168.1.1
 *     grpc_port: 50051
 *     capture_interfaces:
 *       - eth0
 *       - eth1
 *     metadata:
 *       location: "US-WEST"
 *       priority: "HIGH"
 *   - id: node-2
 *     hostname: 192.168.1.2
 *     grpc_port: 50051
 *     capture_interfaces:
 *       - eth0
 * ```
 * 
 * JSON format:
 * ```json
 * {
 *   "nodes": [
 *     {
 *       "id": "node-1",
 *       "hostname": "192.168.1.1",
 *       "grpc_port": 50051,
 *       "capture_interfaces": ["eth0", "eth1"],
 *       "metadata": {"location": "US-WEST", "priority": "HIGH"}
 *     }
 *   ]
 * }
 * ```
 */
class ConfigLoader {
public:
    /**
     * @brief Load node configuration from YAML or JSON file
     * 
     * Automatically detects format based on file extension:
     * - .yaml, .yml → YAML format
     * - .json → JSON format
     * 
     * @param config_path Path to configuration file
     * @return Result containing vector of NodeInfo or error
     */
    static auto load_nodes(const std::filesystem::path& config_path)
        -> Result<std::vector<NodeInfo>>;
    
    /**
     * @brief Load node configuration from YAML format
     * 
     * @param config_path Path to YAML configuration file
     * @return Result containing vector of NodeInfo or error
     */
    static auto load_nodes_yaml(const std::filesystem::path& config_path)
        -> Result<std::vector<NodeInfo>>;
    
    /**
     * @brief Load node configuration from JSON format
     * 
     * @param config_path Path to JSON configuration file
     * @return Result containing vector of NodeInfo or error
     */
    static auto load_nodes_json(const std::filesystem::path& config_path)
        -> Result<std::vector<NodeInfo>>;
    
    /**
     * @brief Load coordinator configuration from file
     * 
     * Supports coordinator-level settings like bind_address, grpc_port, timeouts
     * 
     * @param config_path Path to configuration file
     * @return Result containing CoordinatorConfig or error
     */
    static auto load_coordinator_config(const std::filesystem::path& config_path)
        -> Result<CoordinatorConfig>;

private:
    /**
     * @brief Get file extension and convert to lowercase
     * 
     * @param path File path
     * @return File extension without leading dot, or empty string if none
     */
    static auto get_file_extension(const std::filesystem::path& path) -> std::string;
    
    /**
     * @brief Parse YAML node entry into NodeInfo structure
     * 
     * @param node_entry YAML node entry
     * @return Result containing NodeInfo or error
     */
    static auto parse_yaml_node(void* node_entry) -> Result<NodeInfo>;
    
    /**
     * @brief Parse JSON node entry into NodeInfo structure
     * 
     * @param node_entry JSON object
     * @return Result containing NodeInfo or error
     */
    static auto parse_json_node(void* node_entry) -> Result<NodeInfo>;
};

}  // namespace wadjet::distributed

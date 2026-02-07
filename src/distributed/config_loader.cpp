#include "wadjet/distributed/config_loader.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace wadjet::distributed {

using json = nlohmann::json;

auto ConfigLoader::get_file_extension(const std::filesystem::path& path) -> std::string {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);  // Remove leading dot
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

auto ConfigLoader::load_nodes(const std::filesystem::path& config_path)
    -> Result<std::vector<NodeInfo>> {
    // Verify file exists
    if (!std::filesystem::exists(config_path)) {
        return Result<std::vector<NodeInfo>>(Error::make(
            "CONFIG_NOT_FOUND", "Configuration file not found: " + config_path.string()));
    }

    // Detect format by file extension
    std::string ext = get_file_extension(config_path);

    if (ext == "yaml" || ext == "yml") {
        return load_nodes_yaml(config_path);
    } else if (ext == "json") {
        return load_nodes_json(config_path);
    } else {
        return Result<std::vector<NodeInfo>>(Error::make(
            "UNSUPPORTED_FORMAT",
            "Unsupported configuration format: " + ext + ". Supported formats: yaml, yml, json"));
    }
}

auto ConfigLoader::load_nodes_yaml(const std::filesystem::path& config_path)
    -> Result<std::vector<NodeInfo>> {
    try {
        // Load YAML file
        YAML::Node config = YAML::LoadFile(config_path.string());

        if (!config["nodes"]) {
            return Result<std::vector<NodeInfo>>(
                Error::make("INVALID_CONFIG", "Configuration must contain 'nodes' section"));
        }

        YAML::Node nodes_section = config["nodes"];
        if (!nodes_section.IsSequence()) {
            return Result<std::vector<NodeInfo>>(
                Error::make("INVALID_CONFIG", "'nodes' section must be a list"));
        }

        std::vector<NodeInfo> nodes;

        for (size_t i = 0; i < nodes_section.size(); ++i) {
            YAML::Node node_entry = nodes_section[i];
            auto result = parse_yaml_node(&node_entry);
            if (result.is_err()) {
                return Result<std::vector<NodeInfo>>(result.err().value());
            }
            nodes.push_back(result.ok().value());
        }

        if (nodes.empty()) {
            return Result<std::vector<NodeInfo>>(
                Error::make("EMPTY_CONFIG", "No nodes defined in configuration file"));
        }

        return Result<std::vector<NodeInfo>>(nodes);

    } catch (const YAML::Exception& e) {
        return Result<std::vector<NodeInfo>>(
            Error::make("YAML_PARSE_ERROR", "Failed to parse YAML: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return Result<std::vector<NodeInfo>>(Error::make(
            "CONFIG_LOAD_ERROR", "Failed to load configuration: " + std::string(e.what())));
    }
}

auto ConfigLoader::parse_yaml_node(void* node_entry) -> Result<NodeInfo> {
    try {
        YAML::Node* node = static_cast<YAML::Node*>(node_entry);
        NodeInfo info;

        // Parse required fields
        if (!(*node)["id"]) {
            return Result<NodeInfo>(Error::make("MISSING_FIELD", "Node must have 'id' field"));
        }
        info.id = (*node)["id"].as<std::string>();

        if (!(*node)["hostname"]) {
            return Result<NodeInfo>(
                Error::make("MISSING_FIELD", "Node must have 'hostname' field"));
        }
        info.hostname = (*node)["hostname"].as<std::string>();

        if (!(*node)["grpc_port"]) {
            return Result<NodeInfo>(
                Error::make("MISSING_FIELD", "Node must have 'grpc_port' field"));
        }
        info.grpc_port = (*node)["grpc_port"].as<uint16_t>();

        // Parse optional fields
        if ((*node)["capture_interfaces"]) {
            info.capture_interfaces = (*node)["capture_interfaces"].as<std::vector<std::string>>();
        }

        if ((*node)["metadata"]) {
            info.metadata = (*node)["metadata"].as<std::map<std::string, std::string>>();
        }

        // Default version
        if ((*node)["version"]) {
            info.version = (*node)["version"].as<std::string>();
        } else {
            info.version = "1.0";
        }

        // Validate
        if (!info.is_valid()) {
            return Result<NodeInfo>(
                Error::make("INVALID_NODE", "Node information is invalid: " + info.id));
        }

        return Result<NodeInfo>(info);

    } catch (const YAML::Exception& e) {
        return Result<NodeInfo>(Error::make(
            "YAML_PARSE_ERROR", "Failed to parse node entry: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return Result<NodeInfo>(
            Error::make("NODE_PARSE_ERROR", "Failed to parse node: " + std::string(e.what())));
    }
}

auto ConfigLoader::load_nodes_json(const std::filesystem::path& config_path)
    -> Result<std::vector<NodeInfo>> {
    try {
        // Load JSON file
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return Result<std::vector<NodeInfo>>(Error::make(
                "FILE_OPEN_ERROR", "Failed to open configuration file: " + config_path.string()));
        }

        json config;
        file >> config;

        if (!config.contains("nodes")) {
            return Result<std::vector<NodeInfo>>(
                Error::make("INVALID_CONFIG", "Configuration must contain 'nodes' section"));
        }

        if (!config["nodes"].is_array()) {
            return Result<std::vector<NodeInfo>>(
                Error::make("INVALID_CONFIG", "'nodes' section must be an array"));
        }

        std::vector<NodeInfo> nodes;

        for (const auto& node_entry : config["nodes"]) {
            auto result = parse_json_node(const_cast<json*>(&node_entry));
            if (result.is_err()) {
                return Result<std::vector<NodeInfo>>(result.err().value());
            }
            nodes.push_back(result.ok().value());
        }

        if (nodes.empty()) {
            return Result<std::vector<NodeInfo>>(
                Error::make("EMPTY_CONFIG", "No nodes defined in configuration file"));
        }

        return Result<std::vector<NodeInfo>>(nodes);

    } catch (const json::exception& e) {
        return Result<std::vector<NodeInfo>>(
            Error::make("JSON_PARSE_ERROR", "Failed to parse JSON: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return Result<std::vector<NodeInfo>>(Error::make(
            "CONFIG_LOAD_ERROR", "Failed to load configuration: " + std::string(e.what())));
    }
}

auto ConfigLoader::parse_json_node(void* node_entry) -> Result<NodeInfo> {
    try {
        const json* node = static_cast<const json*>(node_entry);
        NodeInfo info;

        // Parse required fields
        if (!node->contains("id")) {
            return Result<NodeInfo>(Error::make("MISSING_FIELD", "Node must have 'id' field"));
        }
        info.id = node->at("id").get<std::string>();

        if (!node->contains("hostname")) {
            return Result<NodeInfo>(
                Error::make("MISSING_FIELD", "Node must have 'hostname' field"));
        }
        info.hostname = node->at("hostname").get<std::string>();

        if (!node->contains("grpc_port")) {
            return Result<NodeInfo>(
                Error::make("MISSING_FIELD", "Node must have 'grpc_port' field"));
        }
        info.grpc_port = node->at("grpc_port").get<uint16_t>();

        // Parse optional fields
        if (node->contains("capture_interfaces")) {
            info.capture_interfaces =
                node->at("capture_interfaces").get<std::vector<std::string>>();
        }

        if (node->contains("metadata")) {
            info.metadata = node->at("metadata").get<std::map<std::string, std::string>>();
        }

        // Default version
        if (node->contains("version")) {
            info.version = node->at("version").get<std::string>();
        } else {
            info.version = "1.0";
        }

        // Validate
        if (!info.is_valid()) {
            return Result<NodeInfo>(
                Error::make("INVALID_NODE", "Node information is invalid: " + info.id));
        }

        return Result<NodeInfo>(info);

    } catch (const json::exception& e) {
        return Result<NodeInfo>(Error::make(
            "JSON_PARSE_ERROR", "Failed to parse node entry: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return Result<NodeInfo>(
            Error::make("NODE_PARSE_ERROR", "Failed to parse node: " + std::string(e.what())));
    }
}

auto ConfigLoader::load_coordinator_config(const std::filesystem::path& config_path)
    -> Result<CoordinatorConfig> {
    // Verify file exists
    if (!std::filesystem::exists(config_path)) {
        return Result<CoordinatorConfig>(Error::make(
            "CONFIG_NOT_FOUND", "Configuration file not found: " + config_path.string()));
    }

    CoordinatorConfig config;
    std::string ext = get_file_extension(config_path);

    try {
        if (ext == "yaml" || ext == "yml") {
            YAML::Node yaml_config = YAML::LoadFile(config_path.string());

            // Parse coordinator settings
            if (yaml_config["coordinator"]) {
                YAML::Node coord = yaml_config["coordinator"];

                if (coord["bind_address"]) {
                    config.bind_address = coord["bind_address"].as<std::string>();
                }
                if (coord["grpc_port"]) {
                    config.grpc_port = coord["grpc_port"].as<uint16_t>();
                }
                if (coord["heartbeat_timeout_ms"]) {
                    config.heartbeat_timeout =
                        std::chrono::milliseconds(coord["heartbeat_timeout_ms"].as<int>());
                }
                if (coord["heartbeat_interval_ms"]) {
                    config.heartbeat_interval =
                        std::chrono::milliseconds(coord["heartbeat_interval_ms"].as<int>());
                }
                if (coord["barrier_timeout_ms"]) {
                    config.barrier_timeout =
                        std::chrono::milliseconds(coord["barrier_timeout_ms"].as<int>());
                }
                if (coord["max_nodes"]) {
                    config.max_nodes = coord["max_nodes"].as<int>();
                }
                if (coord["enable_partial_results"]) {
                    config.enable_partial_results = coord["enable_partial_results"].as<bool>();
                }
                if (coord["tls_cert_path"]) {
                    config.tls_cert_path = coord["tls_cert_path"].as<std::string>();
                }
                if (coord["tls_key_path"]) {
                    config.tls_key_path = coord["tls_key_path"].as<std::string>();
                }
                if (coord["tls_ca_path"]) {
                    config.tls_ca_path = coord["tls_ca_path"].as<std::string>();
                }
            }

        } else if (ext == "json") {
            std::ifstream file(config_path);
            json json_config;
            file >> json_config;

            if (json_config.contains("coordinator")) {
                const auto& coord = json_config["coordinator"];

                if (coord.contains("bind_address")) {
                    config.bind_address = coord["bind_address"].get<std::string>();
                }
                if (coord.contains("grpc_port")) {
                    config.grpc_port = coord["grpc_port"].get<uint16_t>();
                }
                if (coord.contains("heartbeat_timeout_ms")) {
                    config.heartbeat_timeout =
                        std::chrono::milliseconds(coord["heartbeat_timeout_ms"].get<int>());
                }
                if (coord.contains("heartbeat_interval_ms")) {
                    config.heartbeat_interval =
                        std::chrono::milliseconds(coord["heartbeat_interval_ms"].get<int>());
                }
                if (coord.contains("barrier_timeout_ms")) {
                    config.barrier_timeout =
                        std::chrono::milliseconds(coord["barrier_timeout_ms"].get<int>());
                }
                if (coord.contains("max_nodes")) {
                    config.max_nodes = coord["max_nodes"].get<int>();
                }
                if (coord.contains("enable_partial_results")) {
                    config.enable_partial_results = coord["enable_partial_results"].get<bool>();
                }
                if (coord.contains("tls_cert_path")) {
                    config.tls_cert_path = coord["tls_cert_path"].get<std::string>();
                }
                if (coord.contains("tls_key_path")) {
                    config.tls_key_path = coord["tls_key_path"].get<std::string>();
                }
                if (coord.contains("tls_ca_path")) {
                    config.tls_ca_path = coord["tls_ca_path"].get<std::string>();
                }
            }
        } else {
            return Result<CoordinatorConfig>(
                Error::make("UNSUPPORTED_FORMAT", "Unsupported configuration format: " + ext));
        }

        config.config_path = config_path;
        return Result<CoordinatorConfig>(config);

    } catch (const std::exception& e) {
        return Result<CoordinatorConfig>(Error::make(
            "CONFIG_LOAD_ERROR", "Failed to load coordinator config: " + std::string(e.what())));
    }
}

}  // namespace wadjet::distributed

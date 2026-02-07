#include "wadjet/distributed/config_loader.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>

namespace wadjet::distributed::testing {

using json = nlohmann::json;

/**
 * T292: Test ConfigLoader for YAML/JSON node configuration parsing
 */
class ConfigLoaderTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir_;

    void SetUp() override {
        // Create temporary directory for test files
        temp_dir_ = std::filesystem::temp_directory_path() / "wadjet_config_test";
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        // Clean up temporary files
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path create_yaml_config(const std::string& content) {
        auto config_path = temp_dir_ / "nodes.yaml";
        std::ofstream file(config_path);
        file << content;
        file.close();
        return config_path;
    }

    std::filesystem::path create_json_config(const std::string& content) {
        auto config_path = temp_dir_ / "nodes.json";
        std::ofstream file(config_path);
        file << content;
        file.close();
        return config_path;
    }
};

/**
 * T292: Test loading single node from YAML format
 */
TEST_F(ConfigLoaderTest, LoadSingleNodeYAML) {
    std::string yaml_content = R"(
nodes:
  - id: node-1
    hostname: 192.168.1.1
    grpc_port: 50051
    capture_interfaces:
      - eth0
      - eth1
    metadata:
      location: US-WEST
      priority: HIGH
)";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_TRUE(result);
    ASSERT_EQ(result.value().size(), 1);

    const auto& node = result.value()[0];
    EXPECT_EQ(node.id, "node-1");
    EXPECT_EQ(node.hostname, "192.168.1.1");
    EXPECT_EQ(node.grpc_port, 50051);
    EXPECT_EQ(node.capture_interfaces.size(), 2);
    EXPECT_EQ(node.capture_interfaces[0], "eth0");
    EXPECT_EQ(node.capture_interfaces[1], "eth1");
    EXPECT_EQ(node.metadata["location"], "US-WEST");
    EXPECT_EQ(node.metadata["priority"], "HIGH");
}

/**
 * T292: Test loading multiple nodes from YAML format
 */
TEST_F(ConfigLoaderTest, LoadMultipleNodesYAML) {
    std::string yaml_content = R"(
nodes:
  - id: node-1
    hostname: 192.168.1.1
    grpc_port: 50051
    capture_interfaces:
      - eth0
  - id: node-2
    hostname: 192.168.1.2
    grpc_port: 50052
    capture_interfaces:
      - eth0
  - id: node-3
    hostname: 192.168.1.3
    grpc_port: 50053
    capture_interfaces:
      - eth0
)";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_TRUE(result);
    ASSERT_EQ(result.value().size(), 3);

    EXPECT_EQ(result.value()[0].id, "node-1");
    EXPECT_EQ(result.value()[1].id, "node-2");
    EXPECT_EQ(result.value()[2].id, "node-3");

    EXPECT_EQ(result.value()[0].grpc_port, 50051);
    EXPECT_EQ(result.value()[1].grpc_port, 50052);
    EXPECT_EQ(result.value()[2].grpc_port, 50053);
}

/**
 * T292: Test loading nodes from JSON format
 */
TEST_F(ConfigLoaderTest, LoadNodesJSON) {
    json config_json;
    config_json["nodes"] = json::array();

    json node1;
    node1["id"] = "node-1";
    node1["hostname"] = "192.168.1.1";
    node1["grpc_port"] = 50051;
    node1["capture_interfaces"] = {"eth0", "eth1"};
    node1["metadata"]["location"] = "US-WEST";
    config_json["nodes"].push_back(node1);

    json node2;
    node2["id"] = "node-2";
    node2["hostname"] = "192.168.1.2";
    node2["grpc_port"] = 50052;
    node2["capture_interfaces"] = {"eth0"};
    config_json["nodes"].push_back(node2);

    auto config_path = create_json_config(config_json.dump(2));
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_TRUE(result);
    ASSERT_EQ(result.value().size(), 2);

    const auto& node = result.value()[0];
    EXPECT_EQ(node.id, "node-1");
    EXPECT_EQ(node.hostname, "192.168.1.1");
    EXPECT_EQ(node.grpc_port, 50051);
    EXPECT_EQ(node.metadata["location"], "US-WEST");
}

/**
 * T292: Test error handling for missing required fields
 */
TEST_F(ConfigLoaderTest, ErrorMissingRequiredField) {
    std::string yaml_content = R"(
nodes:
  - id: node-1
    hostname: 192.168.1.1
    # missing grpc_port
)";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), "MISSING_FIELD");
}

/**
 * T292: Test error handling for invalid node info
 */
TEST_F(ConfigLoaderTest, ErrorInvalidNodeInfo) {
    std::string yaml_content = R"(
nodes:
  - id: ""
    hostname: 192.168.1.1
    grpc_port: 50051
)";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), "INVALID_NODE");
}

/**
 * T292: Test error handling for empty configuration
 */
TEST_F(ConfigLoaderTest, ErrorEmptyConfiguration) {
    std::string yaml_content = "nodes: []";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), "EMPTY_CONFIG");
}

/**
 * T292: Test error handling for non-existent file
 */
TEST_F(ConfigLoaderTest, ErrorFileNotFound) {
    auto non_existent = temp_dir_ / "non_existent.yaml";
    auto result = ConfigLoader::load_nodes(non_existent);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), "CONFIG_NOT_FOUND");
}

/**
 * T292: Test error handling for unsupported file format
 */
TEST_F(ConfigLoaderTest, ErrorUnsupportedFormat) {
    auto config_path = temp_dir_ / "config.txt";
    std::ofstream file(config_path);
    file << "invalid format";
    file.close();

    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), "UNSUPPORTED_FORMAT");
}

/**
 * T292: Test default version assignment
 */
TEST_F(ConfigLoaderTest, DefaultVersionAssignment) {
    std::string yaml_content = R"(
nodes:
  - id: node-1
    hostname: 192.168.1.1
    grpc_port: 50051
)";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value()[0].version, "1.0");
}

/**
 * T292: Test explicit version in config
 */
TEST_F(ConfigLoaderTest, ExplicitVersionInConfig) {
    std::string yaml_content = R"(
nodes:
  - id: node-1
    hostname: 192.168.1.1
    grpc_port: 50051
    version: "2.0"
)";

    auto config_path = create_yaml_config(yaml_content);
    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value()[0].version, "2.0");
}

/**
 * T292: Test auto-detection of YAML format by extension
 */
TEST_F(ConfigLoaderTest, AutoDetectYAMLExtension) {
    std::string yaml_content = R"(
nodes:
  - id: node-1
    hostname: 192.168.1.1
    grpc_port: 50051
)";

    auto config_path = temp_dir_ / "nodes.yml";
    std::ofstream file(config_path);
    file << yaml_content;
    file.close();

    auto result = ConfigLoader::load_nodes(config_path);

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value()[0].id, "node-1");
}

}  // namespace wadjet::distributed::testing

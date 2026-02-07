/**
 * @file distributed_fixture.cpp
 * @brief GoogleTest fixture implementation for multi-node testing (T137)
 */

#include "wadjet/testing/distributed_fixture.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/scenario.hpp"

namespace wadjet {
namespace testing {

DistributedTestFixture::DistributedTestFixture()
    : coordinator_address_("127.0.0.1"),
      coordinator_port_(50051) {}

void DistributedTestFixture::SetUp() {
  // Create coordinator configuration
  distributed::CoordinatorConfig config;
  config.bind_address = coordinator_address_;
  config.grpc_port = coordinator_port_;
  config.heartbeat_timeout_ms = 5000;
  config.barrier_timeout_ms = 10000;
  config.enable_partial_results = true;
  config.log_level = "info";

  // Initialize coordinator
  coordinator_ = distributed::TestCoordinator::create(config);
  ASSERT_NE(coordinator_, nullptr) << "Failed to create coordinator";

  // Start coordinator (gRPC server)
  auto result = coordinator_->start();
  ASSERT_TRUE(result) << "Failed to start coordinator";
}

void DistributedTestFixture::TearDown() {
  // Stop all registered nodes
  for (auto& [node_id, node] : nodes_) {
    if (node) {
      node->disconnect();
    }
  }
  nodes_.clear();

  // Shutdown coordinator
  if (coordinator_) {
    coordinator_->stop();
    coordinator_.reset();
  }

  // Clean up temporary files
  for (const auto& file : temp_files_) {
    std::remove(file.c_str());
  }
  temp_files_.clear();
}

distributed::TestNode* DistributedTestFixture::AddNode(
    const std::string& node_id,
    distributed::NodeRole role,
    const std::string& interface_name) {
  if (nodes_.find(node_id) != nodes_.end()) {
    throw std::runtime_error("Node " + node_id + " already exists");
  }

  // Create node configuration
  distributed::NodeConfig node_config;
  node_config.node_id = node_id;
  node_config.role = role;
  node_config.interface = interface_name;
  node_config.coordinator_address = coordinator_address_;
  node_config.coordinator_port = coordinator_port_;
  node_config.heartbeat_interval_ms = 1000;
  node_config.log_level = "info";

  // Create and register node
  auto node = distributed::TestNode::create(node_config);
  if (!node) {
    throw std::runtime_error("Failed to create node: " + node_id);
  }

  // Connect to coordinator
  auto connect_result = node->connect();
  if (!connect_result) {
    throw std::runtime_error("Failed to connect node to coordinator: " + node_id);
  }

  // Store node and return raw pointer
  auto* node_ptr = node.get();
  nodes_[node_id] = std::move(node);

  return node_ptr;
}

distributed::ScenarioDefinition DistributedTestFixture::LoadScenario(
    const std::string& scenario_path) {
  // Try multiple search paths
  std::vector<std::string> search_paths = {
      scenario_path,
      std::string("examples/scenarios/") + scenario_path,
      std::string("../examples/scenarios/") + scenario_path,
  };

  for (const auto& path : search_paths) {
    std::ifstream file(path);
    if (file.good()) {
      // Parse scenario from YAML
      distributed::ScenarioDefinition scenario;
      // TODO: Implement YAML parser
      // For now, return empty scenario that can be populated by test
      scenario.scenario_id = scenario_path;
      return scenario;
    }
  }

  throw std::runtime_error("Scenario file not found: " + scenario_path);
}

distributed::ScenarioResult DistributedTestFixture::RunScenario(
    const distributed::ScenarioDefinition& scenario,
    int timeout_ms) {
  if (!coordinator_) {
    throw std::runtime_error("Coordinator not initialized");
  }

  if (nodes_.empty()) {
    throw std::runtime_error("No nodes registered for scenario execution");
  }

  // Execute scenario on coordinator
  auto result = coordinator_->run_scenario(scenario, timeout_ms);
  return result;
}

std::vector<distributed::ScenarioResult>
DistributedTestFixture::RunScenariosParallel(
    const std::vector<distributed::ScenarioDefinition>& scenarios) {
  if (!coordinator_) {
    throw std::runtime_error("Coordinator not initialized");
  }

  // Run scenarios in parallel (coordinator handles isolation)
  return coordinator_->run_scenarios_parallel(scenarios);
}

distributed::TestNode* DistributedTestFixture::GetNode(
    const std::string& node_id) {
  auto it = nodes_.find(node_id);
  if (it != nodes_.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool DistributedTestFixture::WaitForNodesHealthy(int timeout_ms) {
  auto start = std::chrono::steady_clock::now();

  while (true) {
    bool all_healthy = true;
    for (const auto& [node_id, node] : nodes_) {
      if (!node->is_healthy()) {
        all_healthy = false;
        break;
      }
    }

    if (all_healthy) {
      return true;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed > timeout_ms) {
      return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

bool DistributedTestFixture::HasNodeFailures() const {
  if (!coordinator_) {
    return false;
  }
  // Check coordinator's internal node status
  return coordinator_->has_failed_nodes();
}

void DistributedTestFixture::ResetTopology() {
  // Disconnect all nodes
  for (auto& [node_id, node] : nodes_) {
    if (node) {
      node->disconnect();
    }
  }
  nodes_.clear();
}

}  // namespace testing
}  // namespace wadjet

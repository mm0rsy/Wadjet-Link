#include "wadjet/distributed/scenario.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace wadjet::distributed {

/**
 * @brief Implementation of DistributedScenario
 * 
 * T075-T076: Handles YAML and JSON parsing
 * T077-T083: Scenario decomposition and execution
 */
class DistributedScenarioImpl : public DistributedScenario {
public:
    std::string scenario_id;
    std::string scenario_name;
    std::string description_text;
    std::vector<NodeAssignment> node_assignments_;
    std::vector<DistributedStep> steps_;
    
    auto id() const -> const std::string& override {
        return scenario_id;
    }
    
    auto name() const -> const std::string& override {
        return scenario_name;
    }
    
    auto node_assignments() const -> const std::vector<NodeAssignment>& override {
        return node_assignments_;
    }
    
    auto steps() const -> const std::vector<DistributedStep>& override {
        return steps_;
    }
    
    auto steps_for_node(const std::string& node_id) const
        -> std::vector<DistributedStep> override {
        // T077: Scenario decomposition - return steps targeted to this node
        std::vector<DistributedStep> result;
        
        for (const auto& step : steps_) {
            // Check if step targets this node or all nodes
            if (step.target_nodes.empty()) {
                // Empty target means all nodes
                result.push_back(step);
            } else {
                // Check if node_id is in target_nodes
                auto it = std::find(step.target_nodes.begin(), 
                                   step.target_nodes.end(), 
                                   node_id);
                if (it != step.target_nodes.end()) {
                    result.push_back(step);
                }
            }
        }
        
        return result;
    }
    
    auto description() const -> const std::string& override {
        return description_text;
    }
    
    auto validate() const -> bool override {
        // T074: Validate scenario consistency
        
        // Check required fields
        if (scenario_id.empty()) {
            return false;
        }
        
        // Check that all referenced nodes in steps exist in assignments
        std::unordered_set<std::string> assigned_nodes;
        for (const auto& assignment : node_assignments_) {
            assigned_nodes.insert(assignment.node_id);
        }
        
        for (const auto& step : steps_) {
            for (const auto& node_id : step.target_nodes) {
                if (assigned_nodes.find(node_id) == assigned_nodes.end()) {
                    return false;  // Referenced node not assigned
                }
            }
            
            // Check dependencies
            if (!step.depends_on.empty()) {
                auto it = std::find_if(steps_.begin(), steps_.end(),
                    [&step](const DistributedStep& s) { return s.step_id == step.depends_on; });
                if (it == steps_.end()) {
                    return false;  // Dependency not found
                }
            }
        }
        
        return true;
    }
};

// T075: Parse YAML scenario
auto DistributedScenario::from_yaml(const std::string& yaml_file)
    -> std::unique_ptr<DistributedScenario> {
    
    std::ifstream file(yaml_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open YAML file: " + yaml_file);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return from_yaml_string(buffer.str());
}

// T075: Parse YAML from string
auto DistributedScenario::from_yaml_string(const std::string& yaml_content)
    -> std::unique_ptr<DistributedScenario> {
    
    // T075: YAML parsing using yaml-cpp (placeholder implementation)
    // In production, would use yaml-cpp library:
    // yaml::Node root = yaml::Load(yaml_content);
    
    auto scenario = std::make_unique<DistributedScenarioImpl>();
    
    // Placeholder: Parse basic YAML structure
    // Real implementation would use yaml-cpp to parse:
    // - scenario_id, scenario_name, description
    // - node_assignments section
    // - steps section with all configurations
    
    scenario->scenario_id = "yaml-scenario-1";
    scenario->scenario_name = "Sample YAML Scenario";
    scenario->description_text = "Loaded from YAML";
    
    // Add sample node assignments
    NodeAssignment node_a{"node-a", "sender", {"eth0", "eth1"}};
    NodeAssignment node_b{"node-b", "receiver", {"eth0", "eth1"}};
    NodeAssignment node_c{"node-c", "observer", {"eth0", "eth1"}};
    
    scenario->node_assignments_ = {node_a, node_b, node_c};
    
    // Add sample steps
    DistributedStep barrier_step1;
    barrier_step1.step_id = "barrier-1";
    barrier_step1.step_name = "Initial Sync";
    barrier_step1.type = StepType::BARRIER;
    barrier_step1.barrier_config.barrier_id = "sync-1";
    barrier_step1.barrier_config.participating_nodes = {"node-a", "node-b", "node-c"};
    barrier_step1.target_nodes = {};  // All nodes
    
    DistributedStep capture_step;
    capture_step.step_id = "capture-1";
    capture_step.step_name = "Start Capture";
    capture_step.type = StepType::CAPTURE;
    capture_step.capture_config.capture_id = "cap-1";
    capture_step.capture_config.nodes = {"node-a", "node-b", "node-c"};
    capture_step.capture_config.interface = "eth0";
    capture_step.capture_config.duration_ms = std::chrono::milliseconds(5000);
    capture_step.target_nodes = {};
    capture_step.depends_on = "barrier-1";
    
    DistributedStep expect_step;
    expect_step.step_id = "expect-1";
    expect_step.step_name = "Verify Message Flow";
    expect_step.type = StepType::EXPECT;
    expect_step.expect_config.assertion_id = "assert-1";
    expect_step.expect_config.assertion_type = "message_flow";
    expect_step.expect_config.assertion_params = R"({"src_node":"node-a","dst_node":"node-b"})";
    expect_step.target_nodes = {};
    expect_step.depends_on = "capture-1";
    
    scenario->steps_ = {barrier_step1, capture_step, expect_step};
    
    if (!scenario->validate()) {
        throw std::runtime_error("Scenario validation failed");
    }
    
    return scenario;
}

// T076: Parse JSON scenario
auto DistributedScenario::from_json(const std::string& json_file)
    -> std::unique_ptr<DistributedScenario> {
    
    std::ifstream file(json_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open JSON file: " + json_file);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return from_json_string(buffer.str());
}

// T076: Parse JSON from string
auto DistributedScenario::from_json_string(const std::string& json_content)
    -> std::unique_ptr<DistributedScenario> {
    
    // T076: JSON parsing using nlohmann_json (placeholder implementation)
    // In production, would use nlohmann_json library:
    // json root = json::parse(json_content);
    
    auto scenario = std::make_unique<DistributedScenarioImpl>();
    
    // Placeholder: Parse basic JSON structure
    // Real implementation would use nlohmann_json to parse:
    // - scenario_id, scenario_name, description from root
    // - node_assignments array
    // - steps array with all configurations
    
    scenario->scenario_id = "json-scenario-1";
    scenario->scenario_name = "Sample JSON Scenario";
    scenario->description_text = "Loaded from JSON";
    
    // Add sample node assignments
    NodeAssignment node_a{"node-a", "sender", {"eth0"}};
    NodeAssignment node_b{"node-b", "receiver", {"eth0"}};
    
    scenario->node_assignments_ = {node_a, node_b};
    
    // Add sample steps
    DistributedStep barrier_step;
    barrier_step.step_id = "barrier-json";
    barrier_step.step_name = "JSON Barrier";
    barrier_step.type = StepType::BARRIER;
    barrier_step.barrier_config.barrier_id = "sync-json";
    barrier_step.barrier_config.participating_nodes = {"node-a", "node-b"};
    barrier_step.target_nodes = {};
    
    scenario->steps_ = {barrier_step};
    
    if (!scenario->validate()) {
        throw std::runtime_error("Scenario validation failed");
    }
    
    return scenario;
}

}  // namespace wadjet::distributed

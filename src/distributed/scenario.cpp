#include "wadjet/distributed/scenario.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace wadjet::distributed {

/**
 * @brief Helper function to safely get string from YAML node
 */
static auto get_string(const YAML::Node& node, const std::string& key,
                       const std::string& default_val = "") -> std::string {
    if (!node || !node[key]) {
        return default_val;
    }
    try {
        return node[key].as<std::string>();
    } catch (...) {
        return default_val;
    }
}

/**
 * @brief Helper function to safely get integer from YAML node
 */
static auto get_int(const YAML::Node& node, const std::string& key, int default_val = 0) -> int {
    if (!node || !node[key]) {
        return default_val;
    }
    try {
        return node[key].as<int>();
    } catch (...) {
        return default_val;
    }
}

/**
 * @brief Helper function to safely get bool from YAML node
 */
static auto get_bool(const YAML::Node& node, const std::string& key,
                     bool default_val = false) -> bool {
    if (!node || !node[key]) {
        return default_val;
    }
    try {
        return node[key].as<bool>();
    } catch (...) {
        return default_val;
    }
}

/**
 * @brief Helper function to parse string vector from YAML node
 */
static auto get_string_vector(const YAML::Node& node,
                              const std::string& key) -> std::vector<std::string> {
    std::vector<std::string> result;
    if (!node || !node[key] || !node[key].IsSequence()) {
        return result;
    }
    try {
        for (const auto& item : node[key]) {
            result.push_back(item.as<std::string>());
        }
    } catch (...) {}
    return result;
}

/**
 * @brief Parse barrier step configuration from YAML node
 */
static auto parse_barrier_step(const YAML::Node& step_node) -> DistributedStep {
    DistributedStep step;
    step.step_id = get_string(step_node, "id", "barrier-default");
    step.step_name = get_string(step_node, "name", "Barrier");
    step.type = StepType::BARRIER;

    BarrierStepConfig barrier_cfg;
    barrier_cfg.barrier_id = get_string(step_node, "barrier_id", "barrier-" + step.step_id);
    barrier_cfg.timeout_ms = std::chrono::milliseconds(get_int(step_node, "timeout_ms", 5000));
    barrier_cfg.participating_nodes = get_string_vector(step_node, "nodes");

    step.config = barrier_cfg;
    step.target_nodes = get_string_vector(step_node, "target_nodes");
    step.depends_on = get_string(step_node, "depends_on");
    step.parallel = get_bool(step_node, "parallel", false);
    step.timeout_ms = std::chrono::milliseconds(get_int(step_node, "timeout_ms", 5000));

    return step;
}

/**
 * @brief Parse capture step configuration from YAML node
 */
static auto parse_capture_step(const YAML::Node& step_node) -> DistributedStep {
    DistributedStep step;
    step.step_id = get_string(step_node, "id", "capture-default");
    step.step_name = get_string(step_node, "name", "Capture");
    step.type = StepType::CAPTURE;

    CaptureStepConfig capture_cfg;
    capture_cfg.capture_id = get_string(step_node, "capture_id", "cap-" + step.step_id);
    capture_cfg.nodes = get_string_vector(step_node, "nodes");
    capture_cfg.interface = get_string(step_node, "interface", "eth0");
    capture_cfg.bpf_filter = get_string(step_node, "filter");
    capture_cfg.duration_ms = std::chrono::milliseconds(get_int(step_node, "duration_ms", 0));
    capture_cfg.hardware_timestamps = get_bool(step_node, "hardware_timestamps", false);
    capture_cfg.snaplen = static_cast<uint32_t>(get_int(step_node, "snaplen", 65535));
    capture_cfg.buffer_size = static_cast<uint32_t>(get_int(step_node, "buffer_size", 1024 * 1024));

    step.config = capture_cfg;
    step.target_nodes = get_string_vector(step_node, "target_nodes");
    step.depends_on = get_string(step_node, "depends_on");
    step.parallel = get_bool(step_node, "parallel", false);
    step.timeout_ms = std::chrono::milliseconds(get_int(step_node, "timeout_ms", 5000));

    return step;
}

/**
 * @brief Parse expect/assertion step configuration from YAML node
 *
 * T336-T337: Enhanced to parse both generic and protocol-aware assertion modes
 */
static auto parse_expect_step(const YAML::Node& step_node) -> DistributedStep {
    DistributedStep step;
    step.step_id = get_string(step_node, "id", "expect-default");
    step.step_name = get_string(step_node, "name", "Expect");
    step.type = StepType::EXPECT;

    ExpectStepConfig expect_cfg;
    expect_cfg.assertion_id = get_string(step_node, "assertion_id", "assert-" + step.step_id);
    expect_cfg.assertion_type = get_string(step_node, "assertion_type", "message_flow");
    expect_cfg.timeout_ms = std::chrono::milliseconds(get_int(step_node, "timeout_ms", 5000));
    expect_cfg.should_fail = get_bool(step_node, "should_fail", false);
    
    // T336: Parse node context for distributed assertions
    expect_cfg.src_node = get_string(step_node, "src_node", "");
    expect_cfg.dst_node = get_string(step_node, "dst_node", "");

    // T336-T337: Check for protocol-aware assertion mode
    if (step_node["protocol"]) {
        std::string protocol_str = get_string(step_node, "protocol", "generic");
        
        // Parse protocol type (T336)
        if (protocol_str == "ethernet") {
            expect_cfg.protocol = ProtocolType::ETHERNET;
        } else if (protocol_str == "ipv4") {
            expect_cfg.protocol = ProtocolType::IPv4;
        } else if (protocol_str == "udp") {
            expect_cfg.protocol = ProtocolType::UDP;
        } else if (protocol_str == "tcp") {
            expect_cfg.protocol = ProtocolType::TCP;
        } else if (protocol_str == "someip") {
            expect_cfg.protocol = ProtocolType::SOMEIP;
        } else if (protocol_str == "doip") {
            expect_cfg.protocol = ProtocolType::DoIP;
        } else if (protocol_str == "uds") {
            expect_cfg.protocol = ProtocolType::UDS;
        } else {
            expect_cfg.protocol = ProtocolType::GENERIC;
        }
        
        // T336: Parse match_fields from YAML (T337 will use these for matcher instantiation)
        if (step_node["match_fields"]) {
            const auto& fields_node = step_node["match_fields"];
            for (const auto& kv : fields_node) {
                if (kv.first.IsScalar() && kv.second.IsScalar()) {
                    expect_cfg.match_fields[kv.first.as<std::string>()] = 
                        kv.second.as<std::string>();
                }
            }
        }
    }

    // Legacy mode: encode assertion parameters as JSON string if present
    if (!uses_protocol_aware_assertions(expect_cfg) && step_node["assertion_params"]) {
        try {
            auto params_node = step_node["assertion_params"];
            expect_cfg.assertion_params = YAML::Dump(params_node);
        } catch (...) {
            expect_cfg.assertion_params = "{}";
        }
    } else if (!uses_protocol_aware_assertions(expect_cfg)) {
        expect_cfg.assertion_params = "{}";
    }

    step.config = expect_cfg;
    step.target_nodes = get_string_vector(step_node, "target_nodes");
    step.depends_on = get_string(step_node, "depends_on");
    step.parallel = get_bool(step_node, "parallel", false);
    step.timeout_ms = std::chrono::milliseconds(get_int(step_node, "timeout_ms", 5000));

    return step;
}

/**
 * @brief Parse wait/delay step configuration from YAML node
 */
static auto parse_wait_step(const YAML::Node& step_node) -> DistributedStep {
    DistributedStep step;
    step.step_id = get_string(step_node, "id", "wait-default");
    step.step_name = get_string(step_node, "name", "Wait");
    step.type = StepType::WAIT;

    WaitStepConfig wait_cfg;
    wait_cfg.duration = std::chrono::milliseconds(get_int(step_node, "duration_ms", 1000));

    step.config = wait_cfg;
    step.target_nodes = get_string_vector(step_node, "target_nodes");
    step.depends_on = get_string(step_node, "depends_on");
    step.parallel = get_bool(step_node, "parallel", false);
    step.timeout_ms = std::chrono::milliseconds(get_int(step_node, "timeout_ms", 10000));

    return step;
}

/**
 * @brief Parse log step configuration from YAML node
 */
static auto parse_log_step(const YAML::Node& step_node) -> DistributedStep {
    DistributedStep step;
    step.step_id = get_string(step_node, "id", "log-default");
    step.step_name = get_string(step_node, "name", "Log");
    step.type = StepType::LOG;

    LogStepConfig log_cfg;
    log_cfg.message = get_string(step_node, "message", "");
    log_cfg.level = get_string(step_node, "level", "INFO");

    step.config = log_cfg;
    step.target_nodes = get_string_vector(step_node, "target_nodes");
    step.depends_on = get_string(step_node, "depends_on");
    step.parallel = get_bool(step_node, "parallel", false);
    step.timeout_ms = std::chrono::milliseconds(10000);

    return step;
}

class DistributedScenarioImpl : public DistributedScenario {
public:
    std::string scenario_id;
    std::string scenario_name;
    std::string description_text;
    std::vector<std::string> tags_;
    std::vector<NodeDefinition> nodes_;
    std::vector<NodeAssignment> node_assignments_;
    std::vector<DistributedStep> steps_;

    auto id() const -> const std::string& override { return scenario_id; }

    auto name() const -> const std::string& override { return scenario_name; }

    auto tags() const -> const std::vector<std::string>& override { return tags_; }

    auto nodes() const -> const std::vector<NodeDefinition>& override { return nodes_; }

    auto node_assignments() const -> const std::vector<NodeAssignment>& override {
        return node_assignments_;
    }

    auto steps() const -> const std::vector<DistributedStep>& override { return steps_; }

    auto steps_for_node(const std::string& node_id) const -> std::vector<DistributedStep> override {
        // T077: Scenario decomposition - return steps targeted to this node
        std::vector<DistributedStep> result;

        for (const auto& step : steps_) {
            // Check if step targets this node or all nodes
            if (step.target_nodes.empty()) {
                // Empty target means all nodes
                result.push_back(step);
            } else {
                // Check if node_id is in target_nodes
                auto it = std::find(step.target_nodes.begin(), step.target_nodes.end(), node_id);
                if (it != step.target_nodes.end()) {
                    result.push_back(step);
                }
            }
        }

        return result;
    }

    auto description() const -> const std::string& override { return description_text; }

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
                auto it = std::find_if(
                    steps_.begin(), steps_.end(),
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

// T244: Parse YAML from string
auto DistributedScenario::from_yaml_string(const std::string& yaml_content)
    -> std::unique_ptr<DistributedScenario> {
    // T300-T302: YAML parsing using yaml-cpp library
    // Parses: scenario metadata, node_assignments array, and all step types

    auto scenario = std::make_unique<DistributedScenarioImpl>();

    try {
        // Parse YAML content using yaml-cpp
        YAML::Node root = YAML::Load(yaml_content);

        // T300: Extract scenario metadata (id, name, description, tags)
        scenario->scenario_id = get_string(root, "scenario_id", "default-scenario");
        scenario->scenario_name = get_string(root, "scenario_name", "YAML Scenario");
        scenario->description_text = get_string(root, "description", "Loaded from YAML");
        scenario->tags_ = get_string_vector(root, "tags");

        // T301: Parse node_assignments array
        if (root["node_assignments"] && root["node_assignments"].IsSequence()) {
            for (const auto& node_obj : root["node_assignments"]) {
                NodeAssignment assign{get_string(node_obj, "node_id"), get_string(node_obj, "role"),
                                      get_string_vector(node_obj, "interfaces")};
                if (!assign.node_id.empty()) {
                    scenario->node_assignments_.push_back(assign);
                }
            }
        }

        // Parse node definitions if provided
        if (root["nodes"] && root["nodes"].IsSequence()) {
            for (const auto& node_obj : root["nodes"]) {
                NodeDefinition node_def{get_string(node_obj, "id"), get_string(node_obj, "address"),
                                        get_string_vector(node_obj, "interfaces")};
                if (!node_def.id.empty()) {
                    scenario->nodes_.push_back(node_def);
                }
            }
        }

        // T302: Parse steps array with all step types (barrier, capture, expect, wait, log)
        if (root["steps"] && root["steps"].IsSequence()) {
            for (const auto& step_node : root["steps"]) {
                std::string step_type = get_string(step_node, "type", "unknown");

                DistributedStep step;

                if (step_type == "barrier") {
                    step = parse_barrier_step(step_node);
                } else if (step_type == "capture") {
                    step = parse_capture_step(step_node);
                } else if (step_type == "expect" || step_type == "assertion") {
                    step = parse_expect_step(step_node);
                } else if (step_type == "wait") {
                    step = parse_wait_step(step_node);
                } else if (step_type == "log") {
                    step = parse_log_step(step_node);
                } else {
                    // Unknown step type, skip
                    continue;
                }

                scenario->steps_.push_back(step);
            }
        }

        // Validate scenario consistency
        if (!scenario->validate()) {
            throw std::runtime_error(
                "Scenario validation failed: missing nodes or broken dependencies");
        }

        return scenario;
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(std::string("YAML parsing error: ") + e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("YAML scenario parsing failed: ") + e.what());
    }
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
    // T304-T305: JSON parsing using nlohmann_json library with full step parsing

    auto scenario = std::make_unique<DistributedScenarioImpl>();

    try {
        // Parse JSON content using nlohmann_json
        auto root = nlohmann::json::parse(json_content);

        // Extract scenario metadata
        if (root.contains("scenario_id")) {
            scenario->scenario_id = root["scenario_id"].get<std::string>();
        } else {
            scenario->scenario_id = "default-scenario";
        }

        if (root.contains("scenario_name")) {
            scenario->scenario_name = root["scenario_name"].get<std::string>();
        } else {
            scenario->scenario_name = "JSON Scenario";
        }

        if (root.contains("description")) {
            scenario->description_text = root["description"].get<std::string>();
        } else {
            scenario->description_text = "Loaded from JSON";
        }

        // Parse tags array
        if (root.contains("tags") && root["tags"].is_array()) {
            for (const auto& tag : root["tags"]) {
                scenario->tags_.push_back(tag.get<std::string>());
            }
        }

        // Parse node assignments from array
        if (root.contains("node_assignments") && root["node_assignments"].is_array()) {
            for (const auto& node_obj : root["node_assignments"]) {
                NodeAssignment assign{node_obj["node_id"].get<std::string>(),
                                      node_obj.value("role", std::string("")),
                                      node_obj.value("interfaces", std::vector<std::string>{})};
                if (!assign.node_id.empty()) {
                    scenario->node_assignments_.push_back(assign);
                }
            }
        }

        // Parse node definitions if provided
        if (root.contains("nodes") && root["nodes"].is_array()) {
            for (const auto& node_obj : root["nodes"]) {
                NodeDefinition node_def{node_obj["id"].get<std::string>(),
                                        node_obj.value("address", std::string("")),
                                        node_obj.value("interfaces", std::vector<std::string>{})};
                if (!node_def.id.empty()) {
                    scenario->nodes_.push_back(node_def);
                }
            }
        }

        // Parse steps array with all step types (T304: Full step parsing)
        if (root.contains("steps") && root["steps"].is_array()) {
            for (const auto& step_obj : root["steps"]) {
                std::string step_type = step_obj.value("type", std::string("unknown"));

                DistributedStep step;
                step.step_id = step_obj.value(
                    "id", std::string("step-") + std::to_string(scenario->steps_.size()));
                step.step_name = step_obj.value("name", step_type);
                step.depends_on = step_obj.value("depends_on", std::string(""));
                step.parallel = step_obj.value("parallel", false);

                if (step_obj.contains("timeout_ms")) {
                    step.timeout_ms = std::chrono::milliseconds(step_obj["timeout_ms"].get<int>());
                } else {
                    step.timeout_ms = std::chrono::milliseconds(5000);
                }

                if (step_obj.contains("target_nodes") && step_obj["target_nodes"].is_array()) {
                    for (const auto& node_id : step_obj["target_nodes"]) {
                        step.target_nodes.push_back(node_id.get<std::string>());
                    }
                }

                // Parse step-specific configuration
                if (step_type == "barrier") {
                    BarrierStepConfig cfg;
                    cfg.barrier_id = step_obj.value("barrier_id", step.step_id);
                    cfg.timeout_ms = std::chrono::milliseconds(step_obj.value("timeout_ms", 5000));
                    if (step_obj.contains("nodes") && step_obj["nodes"].is_array()) {
                        for (const auto& node : step_obj["nodes"]) {
                            cfg.participating_nodes.push_back(node.get<std::string>());
                        }
                    }
                    step.config = cfg;
                    step.type = StepType::BARRIER;
                } else if (step_type == "capture") {
                    CaptureStepConfig cfg;
                    cfg.capture_id = step_obj.value("capture_id", step.step_id);
                    cfg.interface = step_obj.value("interface", std::string("eth0"));
                    cfg.bpf_filter = step_obj.value("filter", std::string(""));
                    cfg.duration_ms = std::chrono::milliseconds(step_obj.value("duration_ms", 0));
                    cfg.hardware_timestamps = step_obj.value("hardware_timestamps", false);
                    cfg.snaplen = static_cast<uint32_t>(step_obj.value("snaplen", 65535));
                    cfg.buffer_size =
                        static_cast<uint32_t>(step_obj.value("buffer_size", 1024 * 1024));
                    if (step_obj.contains("nodes") && step_obj["nodes"].is_array()) {
                        for (const auto& node : step_obj["nodes"]) {
                            cfg.nodes.push_back(node.get<std::string>());
                        }
                    }
                    step.config = cfg;
                    step.type = StepType::CAPTURE;
                } else if (step_type == "expect" || step_type == "assertion") {
                    ExpectStepConfig cfg;
                    cfg.assertion_id = step_obj.value("assertion_id", step.step_id);
                    cfg.assertion_type =
                        step_obj.value("assertion_type", std::string("message_flow"));
                    cfg.should_fail = step_obj.value("should_fail", false);
                    cfg.timeout_ms = std::chrono::milliseconds(step_obj.value("timeout_ms", 5000));
                    // Serialize assertion_params to JSON string
                    if (step_obj.contains("assertion_params")) {
                        cfg.assertion_params = step_obj["assertion_params"].dump();
                    } else {
                        cfg.assertion_params = "{}";
                    }
                    step.config = cfg;
                    step.type = StepType::EXPECT;
                } else if (step_type == "wait") {
                    WaitStepConfig cfg;
                    cfg.duration = std::chrono::milliseconds(step_obj.value("duration_ms", 1000));
                    step.config = cfg;
                    step.type = StepType::WAIT;
                } else if (step_type == "log") {
                    LogStepConfig cfg;
                    cfg.message = step_obj.value("message", std::string(""));
                    cfg.level = step_obj.value("level", std::string("INFO"));
                    step.config = cfg;
                    step.type = StepType::LOG;
                } else {
                    // Unknown step type, skip
                    continue;
                }

                scenario->steps_.push_back(step);
            }
        }

        // Validate scenario consistency
        if (!scenario->validate()) {
            throw std::runtime_error(
                "Scenario validation failed: missing nodes or broken dependencies");
        }

        return scenario;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("JSON parsing error: ") + e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("JSON scenario parsing failed: ") + e.what());
    }
}

}  // namespace wadjet::distributed

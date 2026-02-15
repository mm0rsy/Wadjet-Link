#include "wadjet/distributed/scenario_adapter.hpp"
#include "wadjet/scenario/scenario_types.hpp"

#include <sstream>

namespace wadjet::distributed {

ScenarioAdapter::ScenarioAdapter(const std::string& default_node_id)
    : default_node_id_(default_node_id) {}

auto ScenarioAdapter::adapt(const wadjet::scenario::Scenario& scenario,
                            const std::string& node_id) -> std::unique_ptr<DistributedScenario> {
    auto adapted = std::make_unique<AdaptedDistributedScenario>();

    // Set basic metadata
    adapted->set_id(scenario.name);  // Use scenario name as ID
    adapted->set_name(scenario.name);
    adapted->set_description(scenario.description);
    adapted->set_tags(scenario.tags);

    // Use provided node_id or fall back to default
    const auto target_node = node_id.empty() ? default_node_id_ : node_id;

    // Convert all steps to target the single node
    for (const auto& m4_step : scenario.steps) {
        auto distributed_step = convert_step(m4_step, target_node);
        if (distributed_step) {
            adapted->add_step(*distributed_step);
        }
    }

    return adapted;
}

auto ScenarioAdapter::adapt(const wadjet::scenario::Scenario& scenario)
    -> std::unique_ptr<DistributedScenario> {
    return adapt(scenario, "");
}

auto ScenarioAdapter::adapt_with_nodes(
    const wadjet::scenario::Scenario& scenario,
    const std::unordered_map<std::string, std::string>& node_assignments)
    -> std::unique_ptr<DistributedScenario> {
    
    // For now, use the first available node ID from assignments
    std::string target_node = default_node_id_;
    
    if (!node_assignments.empty()) {
        // Try to use "sender" node if available, otherwise use first entry
        if (node_assignments.count("sender") > 0) {
            target_node = node_assignments.at("sender");
        } else {
            target_node = node_assignments.begin()->second;
        }
    }

    return adapt(scenario, target_node);
}

auto ScenarioAdapter::convert_step(const wadjet::scenario::Step& m4_step,
                                   const std::string& target_node)
    -> std::optional<DistributedStep> {
    
    DistributedStep dist_step;
    static size_t step_counter = 0;
    dist_step.step_id = "step_" + std::to_string(step_counter++);
    dist_step.target_nodes = {target_node};

    // Convert based on step type using variant
    if (std::holds_alternative<wadjet::scenario::CaptureStep>(m4_step)) {
        const auto& capture = std::get<wadjet::scenario::CaptureStep>(m4_step);
        
        CaptureStepConfig capture_config;
        capture_config.capture_id = dist_step.step_id;
        capture_config.nodes = {target_node};
        capture_config.interface = capture.config.interface;
        capture_config.bpf_filter = capture.config.filter;
        capture_config.duration_ms = capture.config.timeout;
        
        dist_step.type = StepType::CAPTURE;
        dist_step.config = capture_config;
    } else if (std::holds_alternative<wadjet::scenario::SendStep>(m4_step)) {
        const auto& send = std::get<wadjet::scenario::SendStep>(m4_step);
        
        SendStepConfig send_config;
        send_config.send_id = dist_step.step_id;
        send_config.nodes = {target_node};
        send_config.interface = send.interface;
        send_config.delay_before_ms = send.delay;
        
        if (send.pcap_file.has_value() && !send.pcap_file.value().empty()) {
            send_config.pcap_file = send.pcap_file.value();
        }
        
        if (send.raw_data.has_value() && !send.raw_data.value().empty()) {
            send_config.raw_data = send.raw_data.value();
        }
        
        dist_step.type = StepType::SEND;
        dist_step.config = send_config;
    } else if (std::holds_alternative<wadjet::scenario::WaitStep>(m4_step)) {
        const auto& wait = std::get<wadjet::scenario::WaitStep>(m4_step);
        
        WaitStepConfig wait_config;
        wait_config.duration = wait.duration;
        
        dist_step.type = StepType::WAIT;
        dist_step.config = wait_config;
    } else if (std::holds_alternative<wadjet::scenario::ExpectStep>(m4_step)) {
        const auto& expect = std::get<wadjet::scenario::ExpectStep>(m4_step);
        
        ExpectStepConfig expect_config;
        expect_config.assertion_id = dist_step.step_id;
        expect_config.assertion_type = "packet_match";
        expect_config.assertion_params = "{}";  // Simplified
        
        dist_step.type = StepType::EXPECT;
        dist_step.config = expect_config;
    } else if (std::holds_alternative<wadjet::scenario::LogStep>(m4_step)) {
        const auto& log = std::get<wadjet::scenario::LogStep>(m4_step);
        
        LogStepConfig log_config;
        log_config.message = log.message;
        log_config.level = log.level;
        
        dist_step.type = StepType::LOG;
        dist_step.config = log_config;
    } else {
        // Unknown step type
        return std::nullopt;
    }

    return dist_step;
}

}  // namespace wadjet::distributed

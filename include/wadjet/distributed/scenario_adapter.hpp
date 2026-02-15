#pragma once

#include "wadjet/distributed/scenario.hpp"
#include "wadjet/scenario/scenario.hpp"

#include <memory>
#include <optional>
#include <string>

namespace wadjet::distributed {

/**
 * @brief Concrete implementation of DistributedScenario for adapted scenarios
 * 
 * Used internally by ScenarioAdapter to build adapted scenarios
 */
class AdaptedDistributedScenario : public DistributedScenario {
public:
    AdaptedDistributedScenario() = default;
    ~AdaptedDistributedScenario() override = default;

    auto id() const -> const std::string& override { return id_; }
    auto name() const -> const std::string& override { return name_; }
    auto description() const -> const std::string& override { return description_; }
    auto tags() const -> const std::vector<std::string>& override { return tags_; }
    auto nodes() const -> const std::vector<NodeDefinition>& override { return nodes_; }
    auto node_assignments() const -> const std::vector<NodeAssignment>& override { return assignments_; }
    auto steps() const -> const std::vector<DistributedStep>& override { return steps_; }

    auto steps_for_node(const std::string& node_id) const -> std::vector<DistributedStep> override {
        std::vector<DistributedStep> result;
        for (const auto& step : steps_) {
            for (const auto& node : step.target_nodes) {
                if (node == node_id) {
                    result.push_back(step);
                    break;
                }
            }
        }
        return result;
    }

    auto validate() const -> bool override { return true; }

    // Setters for building the adapted scenario
    void set_id(const std::string& id) { id_ = id; }
    void set_name(const std::string& name) { name_ = name; }
    void set_description(const std::string& desc) { description_ = desc; }
    void set_tags(const std::vector<std::string>& tags) { tags_ = tags; }
    void add_step(const DistributedStep& step) { steps_.push_back(step); }

private:
    std::string id_;
    std::string name_;
    std::string description_;
    std::vector<std::string> tags_;
    std::vector<NodeDefinition> nodes_;
    std::vector<NodeAssignment> assignments_;
    std::vector<DistributedStep> steps_;
};

/**
 * @brief Adapter for converting M4 single-node scenarios to M14 distributed scenarios
 *
 * T326: Provides a single-node-to-distributed upgrade path by converting
 * wadjet::scenario::Scenario to wadjet::distributed::DistributedScenario.
 *
 * Conversion rules:
 * - CaptureStep → CaptureStepConfig (on single node)
 * - SendStep → SendStepConfig (on single node)
 * - WaitStep → WaitStepConfig (unchanged)
 * - ExpectStep → ExpectStepConfig (on single node)
 * - LogStep → LogStepConfig (unchanged)
 *
 * Example usage:
 * ```cpp
 * auto m4_scenario = wadjet::scenario::Scenario{...};
 * ScenarioAdapter adapter;
 * auto m14_scenario = adapter.adapt(m4_scenario, "single-node");
 * ```
 */
class ScenarioAdapter {
public:
    /**
     * @brief Create a new ScenarioAdapter
     *
     * @param default_node_id Default node ID for converted steps
     */
    explicit ScenarioAdapter(const std::string& default_node_id = "default-node");

    ~ScenarioAdapter() = default;

    /**
     * @brief Convert M4 Scenario to M14 DistributedScenario
     *
     * T326: Performs conversion of single-node scenario to distributed format
     * Single capture/send/expect operations target the specified node.
     *
     * @param scenario M4 scenario to convert
     * @param node_id Node ID for single-node scenario (defaults to constructor value)
     * @return Converted DistributedScenario or error
     */
    [[nodiscard]] auto adapt(const wadjet::scenario::Scenario& scenario,
                            const std::string& node_id = "") -> std::unique_ptr<DistributedScenario>;

    /**
     * @brief Convert M4 Scenario using default node
     *
     * @param scenario M4 scenario to convert
     * @return Converted DistributedScenario using default node
     */
    [[nodiscard]] auto adapt(const wadjet::scenario::Scenario& scenario) 
        -> std::unique_ptr<DistributedScenario>;

    /**
     * @brief Convert M4 Scenario to M14 DistributedScenario with multi-node assignment
     *
     * T326: Advanced conversion supporting multiple nodes with role assignment
     *
     * @param scenario M4 scenario to convert
     * @param node_assignments Map of logical role ("sender", "receiver", etc.) to node IDs
     * @return Converted DistributedScenario or error
     */
    [[nodiscard]] auto adapt_with_nodes(
        const wadjet::scenario::Scenario& scenario,
        const std::unordered_map<std::string, std::string>& node_assignments) -> std::unique_ptr<DistributedScenario>;

private:
    std::string default_node_id_;

    /**
     * @brief Convert a single M4 step to M14 DistributedStep
     *
     * @param m4_step Step from M4 scenario
     * @param target_node Node ID for this step
     * @return Converted DistributedStep or error
     */
    [[nodiscard]] auto convert_step(const wadjet::scenario::Step& m4_step,
                                   const std::string& target_node) -> std::optional<DistributedStep>;
};

}  // namespace wadjet::distributed

/// @file runner.hpp
/// @brief Scenario runner for executing test scenarios
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#pragma once

#include "wadjet/scenario/scenario_types.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/dispatcher.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wadjet::scenario {

// =============================================================================
// Runner Options
// =============================================================================

/// @brief Options for scenario execution
struct RunnerOptions {
    bool verbose{false};                    ///< Print detailed progress
    bool save_pcap_on_failure{true};        ///< Save pcap when scenario fails
    std::string pcap_output_dir{"."};       ///< Directory for pcap files
    bool dry_run{false};                    ///< Parse and validate only
    bool stop_on_first_failure{false};      ///< Stop at first failed expectation
    Duration global_timeout{60000};         ///< Overall execution timeout
};

// =============================================================================
// Runner Callbacks
// =============================================================================

/// @brief Event callbacks for scenario execution
struct RunnerCallbacks {
    std::function<void(const std::string&)> on_scenario_start;
    std::function<void(const ScenarioResult&)> on_scenario_end;
    std::function<void(const Step&, std::size_t)> on_step_start;
    std::function<void(const ExpectResult&)> on_expect_result;
    std::function<void(const std::string&)> on_log;
    std::function<void(const Packet&)> on_packet_captured;
};

// =============================================================================
// Packet Matcher
// =============================================================================

/// @brief Interface for matching packets against expectations
class IPacketMatcher {
public:
    virtual ~IPacketMatcher() = default;
    
    /// @brief Check if packet matches the expectation
    [[nodiscard]] virtual bool matches(const Packet& packet) const = 0;
    
    /// @brief Get description of what we're matching
    [[nodiscard]] virtual std::string describe() const = 0;
};

/// @brief Create matcher from ExpectStep
[[nodiscard]] std::unique_ptr<IPacketMatcher> create_matcher(const ExpectStep& expect);

// =============================================================================
// Scenario Runner
// =============================================================================

/// @brief Executes test scenarios against live or recorded traffic
class ScenarioRunner {
public:
    explicit ScenarioRunner(RunnerOptions opts = RunnerOptions{});
    ~ScenarioRunner();
    
    // Non-copyable, movable
    ScenarioRunner(const ScenarioRunner&) = delete;
    ScenarioRunner& operator=(const ScenarioRunner&) = delete;
    ScenarioRunner(ScenarioRunner&&) noexcept;
    ScenarioRunner& operator=(ScenarioRunner&&) noexcept;
    
    /// @brief Set event callbacks
    void set_callbacks(RunnerCallbacks callbacks);
    
    /// @brief Run a single scenario
    [[nodiscard]] ScenarioResult run(const Scenario& scenario);
    
    /// @brief Run multiple scenarios
    [[nodiscard]] std::vector<ScenarioResult> run_all(const std::vector<Scenario>& scenarios);
    
    /// @brief Request graceful stop
    void stop();
    
    /// @brief Check if runner was stopped
    [[nodiscard]] bool stopped() const;
    
    /// @brief Get runner options
    [[nodiscard]] const RunnerOptions& options() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Batch Runner
// =============================================================================

/// @brief Summary of running multiple scenarios
struct BatchResult {
    std::vector<ScenarioResult> results;
    Duration total_elapsed{0};
    std::size_t passed{0};
    std::size_t failed{0};
    std::size_t skipped{0};
    
    [[nodiscard]] bool all_passed() const { return failed == 0; }
    [[nodiscard]] std::size_t total() const { return passed + failed + skipped; }
};

/// @brief Run all scenarios in a directory
[[nodiscard]] BatchResult run_scenarios_in_directory(
    const std::filesystem::path& dir,
    const RunnerOptions& opts = RunnerOptions{},
    const std::vector<std::string>& tags = {});

/// @brief Run scenarios from multiple files
[[nodiscard]] BatchResult run_scenario_files(
    const std::vector<std::filesystem::path>& files,
    const RunnerOptions& opts = RunnerOptions{});

}  // namespace wadjet::scenario

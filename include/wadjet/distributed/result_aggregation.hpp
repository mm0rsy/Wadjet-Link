#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

#include "types.hpp"
#include "result.hpp"

namespace wadjet::distributed {

using json = nlohmann::json;

/**
 * @brief Single assertion result from a node
 * 
 * T086: Represents one test assertion result with context
 */
struct AssertionResult {
    std::string assertion_id;           ///< Unique identifier for assertion
    std::string test_name;              ///< Name of test containing assertion
    bool passed;                        ///< Whether assertion passed
    std::string expression;             ///< The assertion expression
    std::string failure_message;        ///< Message if assertion failed
    int64_t timestamp_ns;               ///< UTC nanosecond timestamp when assertion ran
    std::chrono::milliseconds duration; ///< How long assertion evaluation took
    std::vector<std::string> context;   ///< Additional context (stack trace, etc.)
    
    /// Convert to JSON for serialization
    auto to_json() const -> json;
    
    /// Create from JSON
    static auto from_json(const json& j) -> Result<AssertionResult>;
};

/**
 * @brief Test results from a single node
 * 
 * T087: Aggregates all assertions, capture info, and execution metadata from one node
 */
struct NodeResult {
    NodeId node_id;                                    ///< Which node this result is from
    std::string node_name;                             ///< Human-readable node name
    bool healthy;                                      ///< Whether node completed successfully
    std::vector<AssertionResult> assertions;           ///< All assertions from this node
    int passed_count = 0;                              ///< Number of passed assertions
    int failed_count = 0;                              ///< Number of failed assertions
    std::chrono::nanoseconds total_duration{0};        ///< Total test duration on node
    std::string pcap_file_path;                        ///< Path to PCAP capture (if any)
    bool has_capture = false;                          ///< Whether capture was performed
    std::string failure_capture_dir;                   ///< Directory with failure captures (T095)
    std::optional<std::string> error_message;          ///< If unhealthy, why it failed
    int64_t start_time_ns = 0;                         ///< UTC nanosecond when tests started
    int64_t end_time_ns = 0;                           ///< UTC nanosecond when tests ended
    std::map<std::string, std::string> metadata;       ///< Custom metadata (version, config, etc.)
    
    /// Convert to JSON for serialization
    auto to_json() const -> json;
    
    /// Create from JSON
    static auto from_json(const json& j) -> Result<NodeResult>;
    
    /// Get total assertion count
    auto total_assertions() const -> int {
        return passed_count + failed_count;
    }
    
    /// Check if all assertions passed
    auto all_passed() const -> bool {
        return failed_count == 0;
    }
};

/**
 * @brief Aggregated results from all nodes in a distributed test
 * 
 * T088: Top-level result container with results from all nodes,
 * providing export to JUnit XML, JSON, and HTML formats
 */
struct AggregatedResult {
    std::string test_name;                           ///< Name of the distributed test
    std::vector<NodeResult> node_results;            ///< Results from each node
    int64_t test_start_time_ns = 0;                  ///< UTC nanosecond when test started
    int64_t test_end_time_ns = 0;                    ///< UTC nanosecond when test ended
    std::string failure_captures_dir;                ///< Root directory for failure captures (T094)
    bool has_failure_captures = false;               ///< Whether any failure PCAPs were saved
    std::map<std::string, std::string> global_metadata; ///< Global test metadata
    
    /// Convert to JSON representation
    /// 
    /// T092: Export complete results as JSON for machine-readable processing
    auto to_json() const -> json;
    
    /// Create from JSON
    static auto from_json(const json& j) -> Result<AggregatedResult>;
    
    /// Export as JUnit XML format
    /// 
    /// T091: Generate JUnit XML format compatible with CI systems (Jenkins, GitLab CI, etc.)
    /// Includes all node results as testcases with failure information
    /// 
    /// @return JUnit XML string
    auto to_junit_xml() const -> std::string;
    
    /// Export as HTML report
    /// 
    /// T093: Generate human-readable HTML dashboard showing:
    /// - Test execution timeline
    /// - Per-node pass/fail summary
    /// - Failed assertion details with full context
    /// - PCAP file references for debugging
    /// - Metadata summary
    /// 
    /// @param css_path Optional path to custom CSS file
    /// @return HTML document string
    auto to_html_report(const std::string& css_path = "") const -> std::string;
    
    /// Get overall test status
    auto all_passed() const -> bool;
    
    /// Get total assertion counts
    auto total_passed() const -> int;
    auto total_failed() const -> int;
    
    /// Get list of failed assertions across all nodes
    auto failed_assertions() const -> std::vector<std::pair<NodeId, AssertionResult>>;
    
    /// Get list of nodes with failures
    auto failed_nodes() const -> std::vector<NodeId>;
};

}  // namespace wadjet::distributed

#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>
#include <filesystem>
#include <span>
#include <climits>
#include <nlohmann/json.hpp>

#include "types.hpp"
#include "result.hpp"
#include "wadjet/net/packet.hpp"

namespace wadjet::distributed {

using json = nlohmann::json;

/**
 * @brief Test result status enumeration
 * 
 * T256: Enumeration of result status values per data-model.md
 */
enum class ResultStatus {
    Passed,         ///< Test passed
    Failed,         ///< Test failed
    Error,          ///< Test error
    Skipped,        ///< Test skipped
    Timeout         ///< Test timed out
};

/**
 * @brief Single assertion result from a node
 * 
 * T086: Represents one test assertion result with context
 * T275-T277: Extended with distributed context fields per data-model.md
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
    
    // T275: Source and destination node context per data-model.md
    std::string src_node;               ///< Source node ID for distributed assertion
    std::string dst_node;               ///< Destination node ID for distributed assertion
    
    // T276: Distributed timing information per data-model.md
    int64_t src_timestamp_ns = 0;       ///< Timestamp on source node
    int64_t dst_timestamp_ns = 0;       ///< Timestamp on destination node
    int64_t latency_ns = 0;             ///< Latency between nodes
    
    // T277: Expected vs actual for detailed comparison per data-model.md
    std::string expected;               ///< Expected value
    std::string actual;                 ///< Actual value
    
    /// Convert to JSON for serialization
    auto to_json() const -> json;
    
    /// Create from JSON
    static auto from_json(const json& j) -> Result<AssertionResult>;
};

/**
 * @brief Performance metrics for latency calculations
 * 
 * T289: Latency statistics (min, max, mean, p95, p99) per FR-037 and M12 LatencyStats pattern
 */
struct LatencyStats {
    int64_t min_ns = INT64_MAX;         ///< Minimum latency in nanoseconds
    int64_t max_ns = INT64_MIN;         ///< Maximum latency in nanoseconds
    double mean_ns = 0.0;               ///< Mean latency in nanoseconds
    int64_t p95_ns = 0;                 ///< 95th percentile latency
    int64_t p99_ns = 0;                 ///< 99th percentile latency
    int64_t count = 0;                  ///< Number of samples
};

/**
 * @brief Test results from a single node
 * 
 * T087: Aggregates all assertions, capture info, and execution metadata from one node
 * T289-T291: Extended with performance metrics (latency, throughput, packet loss)
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
    
    // T289: Latency statistics per FR-037 and M12 LatencyStats pattern
    LatencyStats latency_stats;                        ///< Latency min/max/mean/p95/p99
    
    // T290: Throughput metrics per FR-037
    double throughput_packets_per_sec = 0.0;           ///< Packets per second
    int64_t total_bytes_captured = 0;                  ///< Total bytes in all packets
    double throughput_mbps = 0.0;                      ///< Throughput in megabits per second
    
    // T291: Packet loss tracking per FR-037
    int64_t packet_loss_count = 0;                     ///< Number of packets lost
    double packet_loss_percent = 0.0;                  ///< Percentage of packets lost
    int64_t expected_packet_count = 0;                 ///< Expected packets for loss calculation
    
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
    
    /// Calculate performance metrics from capture data
    /// 
    /// T289-T291: Compute latency stats, throughput, and packet loss
    /// from packets captured during test
    /// 
    /// @param packets Vector of packets to analyze
    /// @param expected_count Expected packet count for loss calculation
    auto calculate_metrics(const std::vector<Packet>& packets, int64_t expected_count = 0) -> void;
};

/**
 * @brief Aggregated results from all nodes in a distributed test
 * 
 * T088: Top-level result container with results from all nodes,
 * providing export to JUnit XML, JSON, and HTML formats
 * T267-T272: Extended with status, duration, assertions, and merge method per data-model.md
 */
struct AggregatedResult {
    std::string test_name;                           ///< Name of the distributed test
    std::vector<NodeResult> node_results;            ///< Results from each node
    int64_t test_start_time_ns = 0;                  ///< UTC nanosecond when test started
    int64_t test_end_time_ns = 0;                    ///< UTC nanosecond when test ended
    std::string failure_captures_dir;                ///< Root directory for failure captures (T094)
    bool has_failure_captures = false;               ///< Whether any failure PCAPs were saved
    std::map<std::string, std::string> global_metadata; ///< Global test metadata
    
    // T267: Overall test status per data-model.md
    ResultStatus status = ResultStatus::Passed;      ///< Overall pass/fail/error status
    
    // T268: Total duration calculation per data-model.md
    std::chrono::milliseconds total_duration{0};     ///< Total test duration
    
    // T269: Aggregated distributed assertions per data-model.md
    std::vector<AssertionResult> distributed_assertions; ///< All distributed assertions
    
    // T270: PCAP file tracking per data-model.md
    std::vector<std::filesystem::path> pcap_files;   ///< Paths to all PCAP files
    
    // T271: Assertion statistics per data-model.md
    int total_assertions = 0;                        ///< Total assertion count
    int passed_assertions = 0;                       ///< Passed assertion count
    int failed_assertion_count = 0;                  ///< Failed assertion count
    
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
    
    // T272: Merge static method for combining multi-scenario results per data-model.md
    /// Merge results from multiple scenarios into a single aggregated result
    /// 
    /// @param results Vector of results to merge
    /// @return Merged aggregated result
    static auto merge(std::span<const AggregatedResult> results) -> AggregatedResult;
    
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

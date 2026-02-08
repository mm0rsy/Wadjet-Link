#include "wadjet/distributed/result_aggregation.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace wadjet::distributed {

// AssertionResult implementation

auto AssertionResult::to_json() const -> json {
    return json{{"assertion_id", assertion_id},
                {"test_name", test_name},
                {"passed", passed},
                {"expression", expression},
                {"failure_message", failure_message},
                {"timestamp_ns", timestamp_ns},
                {"duration_ms", duration.count()},
                {"context", context}};
}

auto AssertionResult::from_json(const json& j) -> Result<AssertionResult> {
    try {
        AssertionResult result;
        result.assertion_id = j.at("assertion_id").get<std::string>();
        result.test_name = j.at("test_name").get<std::string>();
        result.passed = j.at("passed").get<bool>();
        result.expression = j.at("expression").get<std::string>();
        result.failure_message = j.at("failure_message").get<std::string>();
        result.timestamp_ns = j.at("timestamp_ns").get<int64_t>();
        result.duration = std::chrono::milliseconds(j.at("duration_ms").get<int64_t>());
        result.context = j.at("context").get<std::vector<std::string>>();
        return Result<AssertionResult>(result);
    } catch (const std::exception& e) {
        return Result<AssertionResult>(Error::make("JSON_PARSE_ERROR", e.what()));
    }
}

// NodeResult implementation

auto NodeResult::to_json() const -> json {
    json assertions_array = json::array();
    for (const auto& assertion : assertions) {
        assertions_array.push_back(assertion.to_json());
    }

    return json{{"node_id", node_id},
                {"node_name", node_name},
                {"healthy", healthy},
                {"assertions", assertions_array},
                {"passed_count", passed_count},
                {"failed_count", failed_count},
                {"total_duration_ns", total_duration.count()},
                {"pcap_file_path", pcap_file_path},
                {"has_capture", has_capture},
                {"failure_capture_dir", failure_capture_dir},
                {"error_message", error_message.value_or("")},
                {"start_time_ns", start_time_ns},
                {"end_time_ns", end_time_ns},
                {"metadata", metadata},
                {"latency_stats", json{{"min_ns", latency_stats.min_ns},
                                       {"max_ns", latency_stats.max_ns},
                                       {"mean_ns", latency_stats.mean_ns},
                                       {"p95_ns", latency_stats.p95_ns},
                                       {"p99_ns", latency_stats.p99_ns},
                                       {"count", latency_stats.count}}},
                {"throughput_packets_per_sec", throughput_packets_per_sec},
                {"total_bytes_captured", total_bytes_captured},
                {"throughput_mbps", throughput_mbps},
                {"packet_loss_count", packet_loss_count},
                {"packet_loss_percent", packet_loss_percent},
                {"expected_packet_count", expected_packet_count}};
}

auto NodeResult::from_json(const json& j) -> Result<NodeResult> {
    try {
        NodeResult result;
        result.node_id = j.at("node_id").get<std::string>();
        result.node_name = j.at("node_name").get<std::string>();
        result.healthy = j.at("healthy").get<bool>();
        result.passed_count = j.at("passed_count").get<int>();
        result.failed_count = j.at("failed_count").get<int>();
        result.total_duration = std::chrono::nanoseconds(j.at("total_duration_ns").get<int64_t>());
        result.pcap_file_path = j.at("pcap_file_path").get<std::string>();
        result.has_capture = j.at("has_capture").get<bool>();
        result.failure_capture_dir = j.at("failure_capture_dir").get<std::string>();

        auto error_msg = j.at("error_message").get<std::string>();
        if (!error_msg.empty()) {
            result.error_message = error_msg;
        }

        result.start_time_ns = j.at("start_time_ns").get<int64_t>();
        result.end_time_ns = j.at("end_time_ns").get<int64_t>();
        result.metadata = j.at("metadata").get<std::map<std::string, std::string>>();

        // Parse performance metrics (T289-T291)
        if (j.contains("latency_stats")) {
            const auto& lat = j.at("latency_stats");
            result.latency_stats.min_ns = lat.at("min_ns").get<int64_t>();
            result.latency_stats.max_ns = lat.at("max_ns").get<int64_t>();
            result.latency_stats.mean_ns = lat.at("mean_ns").get<double>();
            result.latency_stats.p95_ns = lat.at("p95_ns").get<int64_t>();
            result.latency_stats.p99_ns = lat.at("p99_ns").get<int64_t>();
            result.latency_stats.count = lat.at("count").get<int64_t>();
        }

        if (j.contains("throughput_packets_per_sec")) {
            result.throughput_packets_per_sec = j.at("throughput_packets_per_sec").get<double>();
        }

        if (j.contains("total_bytes_captured")) {
            result.total_bytes_captured = j.at("total_bytes_captured").get<int64_t>();
        }

        if (j.contains("throughput_mbps")) {
            result.throughput_mbps = j.at("throughput_mbps").get<double>();
        }

        if (j.contains("packet_loss_count")) {
            result.packet_loss_count = j.at("packet_loss_count").get<int64_t>();
        }

        if (j.contains("packet_loss_percent")) {
            result.packet_loss_percent = j.at("packet_loss_percent").get<double>();
        }

        if (j.contains("expected_packet_count")) {
            result.expected_packet_count = j.at("expected_packet_count").get<int64_t>();
        }

        // Parse assertions
        for (const auto& assertion_json : j.at("assertions")) {
            auto assertion_result = AssertionResult::from_json(assertion_json);
            if (assertion_result.is_ok()) {
                result.assertions.push_back(assertion_result.unwrap());
            }
        }

        return Result<NodeResult>(result);
    } catch (const std::exception& e) {
        return Result<NodeResult>(Error::make("JSON_PARSE_ERROR", e.what()));
    }
}

// AggregatedResult implementation

auto AggregatedResult::to_json() const -> json {
    json node_results_array = json::array();
    for (const auto& node_result : node_results) {
        node_results_array.push_back(node_result.to_json());
    }

    return json{{"test_name", test_name},
                {"node_results", node_results_array},
                {"test_start_time_ns", test_start_time_ns},
                {"test_end_time_ns", test_end_time_ns},
                {"failure_captures_dir", failure_captures_dir},
                {"has_failure_captures", has_failure_captures},
                {"global_metadata", global_metadata}};
}

auto AggregatedResult::from_json(const json& j) -> Result<AggregatedResult> {
    try {
        AggregatedResult result;
        result.test_name = j.at("test_name").get<std::string>();
        result.test_start_time_ns = j.at("test_start_time_ns").get<int64_t>();
        result.test_end_time_ns = j.at("test_end_time_ns").get<int64_t>();
        result.failure_captures_dir = j.at("failure_captures_dir").get<std::string>();
        result.has_failure_captures = j.at("has_failure_captures").get<bool>();
        result.global_metadata = j.at("global_metadata").get<std::map<std::string, std::string>>();

        // Parse node results
        for (const auto& node_json : j.at("node_results")) {
            auto node_result = NodeResult::from_json(node_json);
            if (node_result.is_ok()) {
                result.node_results.push_back(node_result.unwrap());
            }
        }

        return Result<AggregatedResult>(result);
    } catch (const std::exception& e) {
        return Result<AggregatedResult>(Error::make("JSON_PARSE_ERROR", e.what()));
    }
}

auto AggregatedResult::to_junit_xml() const -> std::string {
    std::ostringstream xml;

    // XML header
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

    // Root testsuite element
    int total_tests = 0;
    int total_failures = 0;

    for (const auto& node_result : node_results) {
        total_tests += node_result.total_assertions();
        total_failures += node_result.failed_count;
    }

    xml << "<testsuite name=\"" << test_name << "\" tests=\"" << total_tests << "\" failures=\""
        << total_failures << "\" time=\""
        << static_cast<double>(test_end_time_ns - test_start_time_ns) / 1.0e9 << "\">\n";

    // Properties
    xml << "  <properties>\n";
    for (const auto& [key, value] : global_metadata) {
        xml << "    <property name=\"" << key << "\" value=\"" << value << "\"/>\n";
    }
    xml << "  </properties>\n";

    // Test cases from each node
    for (const auto& node_result : node_results) {
        for (const auto& assertion : node_result.assertions) {
            xml << "  <testcase name=\"" << assertion.assertion_id << "\" " << "classname=\""
                << node_result.node_name << "\" " << "time=\""
                << static_cast<double>(assertion.duration.count()) / 1000.0 << "\"";

            if (assertion.passed) {
                xml << "/>\n";
            } else {
                xml << ">\n"
                    << "    <failure message=\"" << assertion.failure_message << "\">"
                    << assertion.expression << "</failure>\n"
                    << "  </testcase>\n";
            }
        }
    }

    xml << "</testsuite>\n";
    return xml.str();
}

auto AggregatedResult::to_html_report(const std::string& css_path) const -> std::string {
    std::ostringstream html;

    // Convert timestamps to readable format
    auto ns_to_time_str = [](int64_t ns) -> std::string {
        auto seconds = ns / 1000000000LL;
        auto ms = (ns % 1000000000LL) / 1000000LL;
        std::time_t t = seconds;
        std::tm* tm = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3)
            << ms;
        return oss.str();
    };

    // HTML header
    html << "<!DOCTYPE html>\n<html>\n<head>\n";
    html << "  <title>" << test_name << " - Distributed Test Report</title>\n";
    html << "  <meta charset=\"UTF-8\">\n";
    html << "  <style>\n";

    if (!css_path.empty()) {
        html << "    @import url(\"" << css_path << "\");\n";
    }

    // Default CSS
    html << "    body { font-family: Arial, sans-serif; margin: 20px; }\n"
         << "    h1 { color: #333; }\n"
         << "    .summary { background-color: #f5f5f5; padding: 10px; border-radius: 5px; margin: "
            "20px 0; }\n"
         << "    .passed { color: #28a745; font-weight: bold; }\n"
         << "    .failed { color: #dc3545; font-weight: bold; }\n"
         << "    table { border-collapse: collapse; width: 100%; margin: 20px 0; }\n"
         << "    th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }\n"
         << "    th { background-color: #f8f9fa; }\n"
         << "    tr:hover { background-color: #f5f5f5; }\n"
         << "    .failure { background-color: #ffe6e6; }\n"
         << "  </style>\n";
    html << "</head>\n<body>\n";

    // Title and summary
    html << "<h1>" << test_name << "</h1>\n";
    html << "<div class=\"summary\">\n";
    html << "  <p><strong>Test Start:</strong> " << ns_to_time_str(test_start_time_ns) << "</p>\n";
    html << "  <p><strong>Test End:</strong> " << ns_to_time_str(test_end_time_ns) << "</p>\n";
    html << "  <p><strong>Duration:</strong> "
         << static_cast<double>(test_end_time_ns - test_start_time_ns) / 1.0e6 << " ms</p>\n";

    int total_passed = 0;
    int total_failed = 0;
    for (const auto& node_result : node_results) {
        total_passed += node_result.passed_count;
        total_failed += node_result.failed_count;
    }
    html << "  <p><strong>Total Assertions:</strong> <span class=\"passed\">" << total_passed
         << " passed</span>, <span class=\"failed\">" << total_failed << " failed</span></p>\n";
    html << "</div>\n";

    // Per-node results
    html << "<h2>Node Results</h2>\n";
    html << "<table>\n";
    html << "  "
            "<tr><th>Node</th><th>Status</th><th>Assertions</th><th>Passed</th><th>Failed</th></"
            "tr>\n";

    for (const auto& node_result : node_results) {
        std::string status_class = node_result.all_passed() ? "passed" : "failed";
        std::string status_text = node_result.all_passed() ? "✓ PASS" : "✗ FAIL";

        html << "  <tr" << (node_result.all_passed() ? "" : " class=\"failure\"") << ">\n"
             << "    <td>" << node_result.node_name << "</td>\n"
             << "    <td class=\"" << status_class << "\">" << status_text << "</td>\n"
             << "    <td>" << node_result.total_assertions() << "</td>\n"
             << "    <td>" << node_result.passed_count << "</td>\n"
             << "    <td>" << node_result.failed_count << "</td>\n"
             << "  </tr>\n";
    }

    html << "</table>\n";

    // Failed assertions detail
    auto failed = failed_assertions();
    if (!failed.empty()) {
        html << "<h2>Failed Assertions</h2>\n";
        html << "<table>\n";
        html << "  <tr><th>Node</th><th>Assertion</th><th>Expression</th><th>Failure</th></tr>\n";

        for (const auto& [node_id, assertion] : failed) {
            html << "  <tr class=\"failure\">\n"
                 << "    <td>" << node_id << "</td>\n"
                 << "    <td>" << assertion.assertion_id << "</td>\n"
                 << "    <td><code>" << assertion.expression << "</code></td>\n"
                 << "    <td>" << assertion.failure_message << "</td>\n"
                 << "  </tr>\n";
        }

        html << "</table>\n";
    }

    // Footer
    html << "</body>\n</html>\n";
    return html.str();
}

auto AggregatedResult::all_passed() const -> bool {
    return total_failed() == 0;
}

auto AggregatedResult::total_passed() const -> int {
    int total = 0;
    for (const auto& node_result : node_results) {
        total += node_result.passed_count;
    }
    return total;
}

auto AggregatedResult::total_failed() const -> int {
    int total = 0;
    for (const auto& node_result : node_results) {
        total += node_result.failed_count;
    }
    return total;
}

auto AggregatedResult::failed_assertions() const
    -> std::vector<std::pair<NodeId, AssertionResult>> {
    std::vector<std::pair<NodeId, AssertionResult>> failed;
    for (const auto& node_result : node_results) {
        for (const auto& assertion : node_result.assertions) {
            if (!assertion.passed) {
                failed.push_back({node_result.node_id, assertion});
            }
        }
    }
    return failed;
}

auto AggregatedResult::failed_nodes() const -> std::vector<NodeId> {
    std::vector<NodeId> failed;
    for (const auto& node_result : node_results) {
        if (!node_result.all_passed()) {
            failed.push_back(node_result.node_id);
        }
    }
    return failed;
}

// T289-T291: Calculate performance metrics from packet captures
auto NodeResult::calculate_metrics(const std::vector<Packet>& packets,
                                   int64_t expected_count) -> void {
    if (packets.empty()) {
        return;
    }

    // Calculate total bytes and throughput
    total_bytes_captured = 0;
    std::vector<int64_t> inter_packet_delays;

    int64_t prev_timestamp = 0;
    for (size_t i = 0; i < packets.size(); ++i) {
        const auto& packet = packets[i];

        // T290: Accumulate byte count
        total_bytes_captured += static_cast<int64_t>(packet.data().size());

        // T289: Calculate inter-packet delays for latency stats
        int64_t current_timestamp = packet.timestamp().total_nanoseconds();
        if (i > 0 && current_timestamp > prev_timestamp) {
            int64_t delay = current_timestamp - prev_timestamp;
            inter_packet_delays.push_back(delay);
        }
        prev_timestamp = current_timestamp;
    }

    // T289: Calculate latency statistics (treating inter-packet delays as "latency")
    if (!inter_packet_delays.empty()) {
        latency_stats.count = static_cast<int64_t>(inter_packet_delays.size());
        latency_stats.min_ns =
            *std::min_element(inter_packet_delays.begin(), inter_packet_delays.end());
        latency_stats.max_ns =
            *std::max_element(inter_packet_delays.begin(), inter_packet_delays.end());

        // Calculate mean
        int64_t sum = 0;
        for (int64_t delay : inter_packet_delays) {
            sum += delay;
        }
        latency_stats.mean_ns =
            static_cast<double>(sum) / static_cast<double>(inter_packet_delays.size());

        // Calculate percentiles (sorted)
        std::vector<int64_t> sorted_delays = inter_packet_delays;
        std::sort(sorted_delays.begin(), sorted_delays.end());

        // 95th percentile
        size_t p95_idx = (sorted_delays.size() * 95) / 100;
        if (p95_idx < sorted_delays.size()) {
            latency_stats.p95_ns = sorted_delays[p95_idx];
        }

        // 99th percentile
        size_t p99_idx = (sorted_delays.size() * 99) / 100;
        if (p99_idx < sorted_delays.size()) {
            latency_stats.p99_ns = sorted_delays[p99_idx];
        }
    }

    // T290: Calculate throughput metrics
    if (total_duration.count() > 0) {
        // Packets per second
        double duration_seconds = static_cast<double>(total_duration.count()) / 1e9;
        throughput_packets_per_sec = static_cast<double>(packets.size()) / duration_seconds;

        // Megabits per second
        int64_t total_bits = total_bytes_captured * 8;
        throughput_mbps = (static_cast<double>(total_bits) / 1e6) / duration_seconds;
    }

    // T291: Calculate packet loss metrics
    if (expected_count > 0) {
        expected_packet_count = expected_count;
        int64_t actual_count = static_cast<int64_t>(packets.size());
        packet_loss_count = expected_count - actual_count;

        if (packet_loss_count < 0) {
            packet_loss_count = 0;  // Can't have negative loss
        }

        packet_loss_percent =
            (packet_loss_count > 0)
                ? (static_cast<double>(packet_loss_count) / static_cast<double>(expected_count)) *
                      100.0
                : 0.0;
    }
}

}  // namespace wadjet::distributed

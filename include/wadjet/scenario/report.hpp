/// @file report.hpp
/// @brief Report generators for scenario results (JUnit XML, JSON)
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#pragma once

#include "wadjet/scenario/runner.hpp"
#include "wadjet/scenario/scenario_types.hpp"

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace wadjet::scenario {

// =============================================================================
// Report Formats
// =============================================================================

/// @brief Output format for reports
enum class ReportFormat {
    JUnitXML,  ///< JUnit XML format (for CI integration)
    JSON,      ///< JSON format (for programmatic access)
    Text,      ///< Human-readable text
    TAP,       ///< Test Anything Protocol
};

// =============================================================================
// Report Configuration
// =============================================================================

/// @brief Options for report generation
struct ReportOptions {
    bool include_timestamps{true};     ///< Include test timestamps
    bool include_pcap_paths{true};     ///< Include paths to failure pcaps
    bool pretty_print{true};           ///< Format output for readability
    std::string suite_name{"wadjet"};  ///< Test suite name
    std::string hostname;              ///< Hostname (auto-detected if empty)
};

// =============================================================================
// Report Generator Interface
// =============================================================================

/// @brief Interface for report generators
class IReportGenerator {
public:
    virtual ~IReportGenerator() = default;

    /// @brief Generate report from batch results
    virtual void generate(std::ostream& out, const BatchResult& results,
                          const ReportOptions& opts = ReportOptions{}) = 0;

    /// @brief Generate report from single scenario result
    virtual void generate(std::ostream& out, const ScenarioResult& result,
                          const ReportOptions& opts = ReportOptions{}) = 0;
};

// =============================================================================
// Report Generators
// =============================================================================

/// @brief Generate JUnit XML report
class JUnitXmlReportGenerator : public IReportGenerator {
public:
    void generate(std::ostream& out, const BatchResult& results,
                  const ReportOptions& opts = ReportOptions{}) override;

    void generate(std::ostream& out, const ScenarioResult& result,
                  const ReportOptions& opts = ReportOptions{}) override;
};

/// @brief Generate JSON report
class JsonReportGenerator : public IReportGenerator {
public:
    void generate(std::ostream& out, const BatchResult& results,
                  const ReportOptions& opts = ReportOptions{}) override;

    void generate(std::ostream& out, const ScenarioResult& result,
                  const ReportOptions& opts = ReportOptions{}) override;
};

/// @brief Generate human-readable text report
class TextReportGenerator : public IReportGenerator {
public:
    void generate(std::ostream& out, const BatchResult& results,
                  const ReportOptions& opts = ReportOptions{}) override;

    void generate(std::ostream& out, const ScenarioResult& result,
                  const ReportOptions& opts = ReportOptions{}) override;
};

/// @brief Generate TAP (Test Anything Protocol) report
class TapReportGenerator : public IReportGenerator {
public:
    void generate(std::ostream& out, const BatchResult& results,
                  const ReportOptions& opts = ReportOptions{}) override;

    void generate(std::ostream& out, const ScenarioResult& result,
                  const ReportOptions& opts = ReportOptions{}) override;
};

// =============================================================================
// Factory Function
// =============================================================================

/// @brief Create report generator for specified format
[[nodiscard]] std::unique_ptr<IReportGenerator> create_report_generator(ReportFormat format);

// =============================================================================
// Convenience Functions
// =============================================================================

/// @brief Generate report to file
void generate_report(const std::filesystem::path& path, const BatchResult& results,
                     ReportFormat format = ReportFormat::JUnitXML,
                     const ReportOptions& opts = ReportOptions{});

/// @brief Generate report to string
[[nodiscard]] std::string generate_report_string(const BatchResult& results,
                                                 ReportFormat format = ReportFormat::JUnitXML,
                                                 const ReportOptions& opts = ReportOptions{});

}  // namespace wadjet::scenario

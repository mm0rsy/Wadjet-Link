/// @file scenario.hpp
/// @brief Main include header for Wadjet scenario module
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This header provides YAML/JSON-driven test scenario support.
/// Include this single header to access all scenario functionality.
///
/// @example
/// @code
/// #include <wadjet/scenario/scenario.hpp>
///
/// using namespace wadjet::scenario;
///
/// // Parse scenario from file
/// auto result = parse_scenario_file("test.yaml");
/// if (!result) {
///     std::cerr << "Parse error: " << result.error().to_string() << "\n";
///     return 1;
/// }
///
/// // Run scenario
/// ScenarioRunner runner;
/// auto run_result = runner.run(*result);
///
/// // Generate report
/// generate_report("results.xml", {run_result}, ReportFormat::JUnitXML);
/// @endcode

#pragma once

#include "wadjet/scenario/parser.hpp"
#include "wadjet/scenario/report.hpp"
#include "wadjet/scenario/runner.hpp"
#include "wadjet/scenario/scenario_types.hpp"

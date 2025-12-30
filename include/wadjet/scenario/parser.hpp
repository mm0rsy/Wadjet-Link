/// @file parser.hpp
/// @brief Unified parser interface for YAML and JSON scenario files
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#pragma once

#include "wadjet/scenario/scenario_types.hpp"
#include "wadjet/core/result.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace wadjet::scenario {

/// @brief Parser error information
struct ParseError {
    std::string message;
    std::size_t line{0};
    std::size_t column{0};
    
    [[nodiscard]] std::string to_string() const {
        if (line > 0) {
            return message + " at line " + std::to_string(line) + 
                   ", column " + std::to_string(column);
        }
        return message;
    }
};

/// @brief Result type for parser operations
using ParseResult = Result<Scenario, ParseError>;

/// @brief Scenario parser interface
class IScenarioParser {
public:
    virtual ~IScenarioParser() = default;
    
    /// @brief Parse scenario from string content
    [[nodiscard]] virtual ParseResult parse(std::string_view content) const = 0;
    
    /// @brief Parse scenario from file
    [[nodiscard]] virtual ParseResult parse_file(const std::filesystem::path& path) const = 0;
    
    /// @brief Get supported file extensions
    [[nodiscard]] virtual std::vector<std::string> supported_extensions() const = 0;
};

/// @brief Detect file format and parse accordingly
/// @param path Path to scenario file (.yaml, .yml, .json)
/// @return Parsed scenario or error
[[nodiscard]] ParseResult parse_scenario_file(const std::filesystem::path& path);

/// @brief Parse YAML scenario content
/// @param content YAML string
/// @return Parsed scenario or error
[[nodiscard]] ParseResult parse_yaml(std::string_view content);

/// @brief Parse JSON scenario content
/// @param content JSON string
/// @return Parsed scenario or error
[[nodiscard]] ParseResult parse_json(std::string_view content);

}  // namespace wadjet::scenario

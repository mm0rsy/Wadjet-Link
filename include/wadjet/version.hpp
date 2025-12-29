#pragma once

/// @file version.hpp
/// @brief Version information for Wadjet-Link

namespace wadjet {

/// Major version number
constexpr int VERSION_MAJOR = 0;

/// Minor version number
constexpr int VERSION_MINOR = 1;

/// Patch version number
constexpr int VERSION_PATCH = 0;

/// Version string literal
#define WADJET_VERSION_STRING "0.1.0"

/// @brief Get the version string at runtime
/// @return Version string in "major.minor.patch" format
const char* version_string() noexcept;

}  // namespace wadjet

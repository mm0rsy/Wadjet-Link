#pragma once

/// @file frame_filter.hpp
/// @brief BPF-based packet filtering

#include "wadjet/core/result.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace wadjet::io {

/// @brief BPF (Berkeley Packet Filter) expression wrapper
///
/// Compiles and applies BPF filter expressions for packet filtering.
///
/// @code
/// auto filter = FrameFilter::compile("udp port 30490");
/// if (filter) {
///     session.set_filter(*filter);
/// }
/// @endcode
class FrameFilter {
public:
    /// @brief Compile a BPF filter expression
    /// @param expression BPF filter expression (e.g., "udp port 30490")
    /// @param link_type Link layer type (default: Ethernet)
    /// @return Compiled filter or error
    static auto compile(std::string_view expression, int link_type = 1) -> Result<FrameFilter>;

    /// @brief Create an "accept all" filter
    static auto accept_all() -> FrameFilter;

    FrameFilter(const FrameFilter&) = delete;
    FrameFilter& operator=(const FrameFilter&) = delete;
    FrameFilter(FrameFilter&&) noexcept;
    FrameFilter& operator=(FrameFilter&&) noexcept;
    ~FrameFilter();

    /// @brief Test if a packet matches the filter
    /// @param data Packet data
    /// @param len Packet length
    /// @return true if packet matches filter
    [[nodiscard]] bool matches(const void* data, std::size_t len) const;

    /// @brief Get the filter expression
    [[nodiscard]] const std::string& expression() const { return expression_; }

    /// @brief Get the compiled BPF program (for use with setsockopt)
    [[nodiscard]] const void* program() const;

    /// @brief Get the BPF program length
    [[nodiscard]] std::size_t program_length() const;

private:
    FrameFilter();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string expression_;
};

}  // namespace wadjet::io

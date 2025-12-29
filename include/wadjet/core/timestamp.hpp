#pragma once

/// @file timestamp.hpp
/// @brief High-precision timestamp for packet capture

#include <chrono>
#include <cstdint>
#include <string>

namespace wadjet {

/// @brief High-precision timestamp for packet capture
class Timestamp {
public:
    using Clock = std::chrono::system_clock;
    using Duration = std::chrono::nanoseconds;
    using TimePoint = std::chrono::time_point<Clock, Duration>;

    /// @brief Create timestamp from current time
    static Timestamp now() { return Timestamp(Clock::now()); }

    /// @brief Create timestamp from seconds and nanoseconds (e.g., from pcap)
    static Timestamp from_unix(std::int64_t seconds, std::int64_t nanoseconds) {
        auto duration = std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanoseconds);
        return Timestamp(TimePoint(std::chrono::duration_cast<Duration>(duration)));
    }

    /// @brief Create timestamp from seconds and microseconds (pcap format)
    static Timestamp from_unix_usec(std::int64_t seconds, std::int64_t microseconds) {
        return from_unix(seconds, microseconds * 1000);
    }

    /// @brief Default constructor (epoch)
    Timestamp() = default;

    /// @brief Construct from time point
    explicit Timestamp(TimePoint tp) : time_point_(tp) {}

    /// @brief Get Unix timestamp in seconds
    [[nodiscard]] std::int64_t seconds() const {
        auto duration = time_point_.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

    /// @brief Get nanoseconds component (0-999999999)
    [[nodiscard]] std::int64_t nanoseconds() const {
        auto duration = time_point_.time_since_epoch();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - secs);
        return nanos.count();
    }

    /// @brief Get microseconds component (0-999999)
    [[nodiscard]] std::int64_t microseconds() const { return nanoseconds() / 1000; }

    /// @brief Get the underlying time point
    [[nodiscard]] TimePoint time_point() const { return time_point_; }

    /// @brief Convert to ISO 8601 string
    [[nodiscard]] std::string to_string() const;

    /// @brief Calculate duration between timestamps
    [[nodiscard]] Duration operator-(const Timestamp& other) const {
        return time_point_ - other.time_point_;
    }

    /// @brief Add duration to timestamp
    [[nodiscard]] Timestamp operator+(Duration d) const { return Timestamp(time_point_ + d); }

    /// @brief Comparison operators
    auto operator<=>(const Timestamp& other) const = default;

private:
    TimePoint time_point_{};
};

}  // namespace wadjet

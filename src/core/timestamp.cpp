#include "wadjet/core/timestamp.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace wadjet {

auto Timestamp::to_string() const -> std::string {
    auto time_t_val = Clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(time_point_));

    std::tm tm_val{};
    gmtime_r(&time_t_val, &tm_val);

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(9) << nanoseconds();
    oss << 'Z';
    return oss.str();
}

}  // namespace wadjet

// Connection state tracking base for protocol completeness
#pragma once
#include <cstdint>

namespace wadjet {
namespace protocols {
namespace common {

class StateTrackerBase {
public:
    virtual ~StateTrackerBase() = default;
    // Update state based on packet
    virtual void update_state(const void* packet) = 0;
    // Get current state
    virtual int get_state() const = 0;
};

} // namespace common
} // namespace protocols
} // namespace wadjet

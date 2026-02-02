// Generic timeout manager implementation for protocol completeness
#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace wadjet {
namespace protocols {
namespace common {

class TimeoutManager {
public:
    using Clock = std::chrono::steady_clock;
    struct Entry {
        Clock::time_point expiry;
        void* data;
    };

    void set_timeout(uint64_t id, std::chrono::seconds duration, void* data) {
        entries_[id] = {Clock::now() + duration, data};
    }

    bool is_expired(uint64_t id) const {
        auto it = entries_.find(id);
        if (it == entries_.end())
            return false;
        return Clock::now() > it->second.expiry;
    }

    void remove(uint64_t id) { entries_.erase(id); }

private:
    std::unordered_map<uint64_t, Entry> entries_;
};

}  // namespace common
}  // namespace protocols
}  // namespace wadjet

#include "wadjet/protocols/ipv4.hpp"
// TimeoutManager usage removed; using internal timestamp for expirations

#include <unordered_map>
#include <map>
#include <vector>
#include <optional>
#include <chrono>

namespace wadjet::protocols::ipv4 {

struct FragmentKey {
    IPv4Address src;
    IPv4Address dst;
    std::uint8_t proto;
    std::uint16_t id;

    bool operator==(const FragmentKey& o) const noexcept {
        return src == o.src && dst == o.dst && proto == o.proto && id == o.id;
    }
};

struct FragmentKeyHash {
    std::size_t operator()(const FragmentKey& k) const noexcept {
        // Simple hash combining parts
        std::size_t h = 1469598103934665603ULL;
        auto mix = [&](std::uint32_t v) { h ^= v; h *= 1099511628211ULL; };
        mix(k.src.to_uint32()); mix(k.dst.to_uint32()); mix(k.proto); mix(k.id);
        return h;
    }
};

class Ipv4FragmentReassembler {
public:
    Ipv4FragmentReassembler(std::chrono::seconds timeout = std::chrono::seconds(30)) : timeout_(timeout) {}

    // Add fragment and attempt reassembly; returns reassembled payload if complete
    std::optional<std::vector<std::uint8_t>> add_fragment(const IPv4Header::Ipv4Fragment& frag) {
        cleanup_expired();
        FragmentKey key{frag.src_ip, frag.dst_ip, frag.protocol, frag.identification};
        auto& entry = cache_[key];

        // store fragment data by offset
        entry.fragments[frag.offset] = frag.payload;
        entry.last_update = std::chrono::steady_clock::now();
        if (!frag.mf) {
            entry.seen_last = true;
            entry.total_size = frag.offset + frag.payload.size();
        }
        // Quick completeness check: if we have seen last and total bytes covered
        if (entry.seen_last) {
            // compute accumulated size
            std::size_t acc = 0;
            std::size_t offset = 0;
            for (auto& [off, data] : entry.fragments) {
                if (off != offset) return std::nullopt; // gap detected
                acc += data.size();
                offset += data.size();
            }
            if (acc == entry.total_size) {
                // build data
                std::vector<std::uint8_t> out;
                out.reserve(acc);
                for (auto& [off, data] : entry.fragments) {
                    out.insert(out.end(), data.begin(), data.end());
                }
                cache_.erase(key); // cleanup
                return out;
            }
        }
        return std::nullopt;
    }

private:
    struct Entry {
        std::map<std::size_t, std::vector<std::uint8_t>> fragments;
        bool seen_last = false;
        std::size_t total_size = 0;
        std::chrono::steady_clock::time_point last_update; // for timeout cleanup
    };

    std::unordered_map<FragmentKey, Entry, FragmentKeyHash> cache_;
    std::chrono::seconds timeout_;

    // Cleanup expired entries before adding a new fragment
    void cleanup_expired() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache_.begin(); it != cache_.end();) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_update);
            if (age > timeout_) it = cache_.erase(it);
            else ++it;
        }
    }
};

} // namespace wadjet::protocols::ipv4

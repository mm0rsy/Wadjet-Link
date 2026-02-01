#include "wadjet/protocols/ipv4.hpp"

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

/// Implementation class for IPv4 fragment reassembly (Pimpl pattern)
class Ipv4FragmentReassembler::Impl {
public:
    explicit Impl(Config cfg) : config_(cfg) {}

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

    void cleanup_expired() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache_.begin(); it != cache_.end();) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_update);
            if (age > config_.timeout) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clear() {
        cache_.clear();
    }

    std::size_t size() const {
        return cache_.size();
    }

private:
    struct Entry {
        std::map<std::size_t, std::vector<std::uint8_t>> fragments;
        bool seen_last = false;
        std::size_t total_size = 0;
        std::chrono::steady_clock::time_point last_update;
    };

    Config config_;
    std::unordered_map<FragmentKey, Entry, FragmentKeyHash> cache_;
};

// Public class implementation

Ipv4FragmentReassembler::Ipv4FragmentReassembler(const Config& cfg)
    : config_(cfg), impl_(std::make_unique<Impl>(cfg)) {}

std::optional<std::vector<std::uint8_t>> Ipv4FragmentReassembler::add_fragment(const IPv4Header::Ipv4Fragment& frag) {
    return impl_->add_fragment(frag);
}

void Ipv4FragmentReassembler::cleanup_expired() {
    impl_->cleanup_expired();
}

void Ipv4FragmentReassembler::clear() {
    impl_->clear();
}

std::size_t Ipv4FragmentReassembler::size() const {
    return impl_->size();
}

Ipv4FragmentReassembler::~Ipv4FragmentReassembler() = default;

} // namespace wadjet::protocols::ipv4

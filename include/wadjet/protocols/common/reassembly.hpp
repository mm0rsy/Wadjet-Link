// Fragment/segment reassembly base class for protocol completeness
#pragma once
#include <vector>
#include <cstdint>

namespace wadjet {
namespace protocols {
namespace common {

class ReassemblyBase {
public:
    virtual ~ReassemblyBase() = default;
    // Add fragment/segment to reassembly
    virtual void add_fragment(const std::vector<uint8_t>& fragment, uint32_t offset, bool more_fragments) = 0;
    // Check if reassembly is complete
    virtual bool is_complete() const = 0;
    // Get reassembled data
    virtual std::vector<uint8_t> get_data() const = 0;
};

} // namespace common
} // namespace protocols
} // namespace wadjet

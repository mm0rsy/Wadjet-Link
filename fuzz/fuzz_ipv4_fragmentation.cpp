#include "wadjet/protocols/ipv4.hpp"
#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet::protocols::ipv4;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Interpret data as two fragments: simple scheme
    if (size < 2) return 0;
    std::size_t mid = size/2;
    IPv4Header::Ipv4Fragment f1, f2;
    f1.payload.assign(data, data + mid);
    f1.offset = 0;
    f1.mf = true;
    f2.payload.assign(data + mid, data + size);
    f2.offset = static_cast<uint16_t>(mid);
    f2.mf = false;

    Ipv4FragmentReassembler r;
    r.add_fragment(f1);
    r.add_fragment(f2);
    return 0;
}

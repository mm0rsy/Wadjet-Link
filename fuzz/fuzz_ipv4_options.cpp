#include "wadjet/protocols/ipv4.hpp"
#include <cstdint>
#include <cstddef>
#include <span>

using namespace wadjet::protocols::ipv4;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<std::byte> raw;
    raw.reserve(size);
    for (size_t i = 0; i < size; ++i) raw.push_back(static_cast<std::byte>(data[i]));

    // Ensure public parse function handles arbitrary input
    auto res = IPv4Header::parseIpv4Options(raw);
    (void)res.malformed;
    (void)res.options.size();
    return 0;
}

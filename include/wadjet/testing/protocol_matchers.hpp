// GoogleTest matchers for protocol completeness
#pragma once
#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

namespace wadjet {
namespace testing {

MATCHER(IsValidIpv4Header, "Valid IPv4 header") {
    // Example matcher: checks header length
    return arg.size() >= 20 && (arg[0] >> 4) == 4;
}

MATCHER(IsValidTcpSegment, "Valid TCP segment") {
    // Example matcher: checks minimum TCP header size
    return arg.size() >= 20;
}

MATCHER(IsValidUdpDatagram, "Valid UDP datagram") {
    // Example matcher: checks minimum UDP header size
    return arg.size() >= 8;
}

} // namespace testing
} // namespace wadjet

// Test fixtures for protocol samples (for GoogleTest)
#pragma once
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace wadjet {
namespace test {

class ProtocolSamples : public ::testing::Test {
protected:
    std::vector<uint8_t> ipv4_sample;
    std::vector<uint8_t> tcp_sample;
    std::vector<uint8_t> udp_sample;
    std::vector<uint8_t> someip_sample;

    void SetUp() override {
        // Initialize with dummy data for now
        ipv4_sample = {0x45, 0x00, 0x00, 0x54};
        tcp_sample = {0x00, 0x14, 0x00, 0x50};
        udp_sample = {0x00, 0x35, 0x00, 0x35};
        someip_sample = {0x12, 0x34, 0x56, 0x78};
    }
};

}  // namespace test
}  // namespace wadjet

#include "wadjet/protocols/ipv4.hpp"
#include <gtest/gtest.h>

using namespace wadjet::protocols::ipv4;
using wadjet::IPv4Address;

/// Minimal IPv4 extension tests for Phase 3 remediation
/// These complement existing IPv4DecoderTest tests

TEST(IPv4ExtensionTest, FragmentReassemblerConstruction) {
    // Test that reassembler can be constructed with default config
    Ipv4FragmentReassembler::Config cfg;
    cfg.timeout = std::chrono::seconds(30);
    
    Ipv4FragmentReassembler reassembler(cfg);
    EXPECT_EQ(reassembler.size(), 0);
}

TEST(IPv4ExtensionTest, FragmentReassemblerSimpleInOrder) {
    Ipv4FragmentReassembler::Config cfg;
    cfg.timeout = std::chrono::seconds(30);
    Ipv4FragmentReassembler r(cfg);

    IPv4Header::Ipv4Fragment f1;
    f1.src_ip = IPv4Address::from_string("10.0.0.1");
    f1.dst_ip = IPv4Address::from_string("10.0.0.2");
    f1.protocol = 6;
    f1.identification = 0x1234;
    f1.offset = 0;
    f1.mf = true;
    f1.payload = {1, 2, 3, 4};

    auto res = r.add_fragment(f1);
    EXPECT_FALSE(res);  // Incomplete, more fragments expected
    
    IPv4Header::Ipv4Fragment f2 = f1;
    f2.offset = 4;
    f2.mf = false;
    f2.payload = {5, 6};

    res = r.add_fragment(f2);
    ASSERT_TRUE(res);  // Complete now
    EXPECT_EQ(res->size(), 6);
}

TEST(IPv4ExtensionTest, FragmentReassemblerOutOfOrder) {
    Ipv4FragmentReassembler::Config cfg;
    Ipv4FragmentReassembler r(cfg);

    IPv4Header::Ipv4Fragment f2;
    f2.src_ip = IPv4Address::from_string("10.0.0.1");
    f2.dst_ip = IPv4Address::from_string("10.0.0.2");
    f2.protocol = 6;
    f2.identification = 0x1235;
    f2.offset = 4;
    f2.mf = false;
    f2.payload = {5, 6};

    auto res = r.add_fragment(f2);
    EXPECT_FALSE(res);  // Incomplete, waiting for first fragment

    IPv4Header::Ipv4Fragment f1 = f2;
    f1.offset = 0;
    f1.mf = true;
    f1.payload = {1, 2, 3, 4};

    res = r.add_fragment(f1);
    ASSERT_TRUE(res);  // Complete now
    EXPECT_EQ(res->size(), 6);
}

TEST(IPv4ExtensionTest, FragmentReassemblerClear) {
    Ipv4FragmentReassembler::Config cfg;
    Ipv4FragmentReassembler r(cfg);

    IPv4Header::Ipv4Fragment f;
    f.src_ip = IPv4Address::from_string("10.0.0.1");
    f.dst_ip = IPv4Address::from_string("10.0.0.2");
    f.protocol = 6;
    f.identification = 0x1234;
    f.offset = 0;
    f.mf = true;
    f.payload = {1, 2};

    auto _ = r.add_fragment(f);  // Suppress nodiscard warning
    (void)_;  // Suppress unused variable warning
    EXPECT_EQ(r.size(), 1);

    r.clear();
    EXPECT_EQ(r.size(), 0);
}

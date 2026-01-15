#include "wadjet/protocols/ipv4.hpp"
#include <gtest/gtest.h>

using namespace wadjet::protocols::ipv4;

TEST(IPv4FragmentationTest, SimpleReassemblyInOrder) {
    Ipv4FragmentReassembler r;

    IPv4Header::Ipv4Fragment f1;
    f1.src_ip = IPv4Address::from_string("10.0.0.1");
    f1.dst_ip = IPv4Address::from_string("10.0.0.2");
    f1.protocol = 6;
    f1.identification = 0x1234;
    f1.offset = 0;
    f1.mf = true;
    f1.payload = {1,2,3,4};

    IPv4Header::Ipv4Fragment f2 = f1;
    f2.offset = 4;
    f2.mf = false;
    f2.payload = {5,6};

    auto maybe = r.add_fragment(f1);
    EXPECT_FALSE(maybe);
    maybe = r.add_fragment(f2);
    ASSERT_TRUE(maybe);
    EXPECT_EQ(maybe->size(), 6);
    EXPECT_EQ((*maybe)[0], 1);
    EXPECT_EQ((*maybe)[5], 6);
}

TEST(IPv4FragmentationTest, OutOfOrderReassembly) {
    Ipv4FragmentReassembler r;
    IPv4Header::Ipv4Fragment f1;
    f1.src_ip = IPv4Address::from_string("10.0.0.1");
    f1.dst_ip = IPv4Address::from_string("10.0.0.2");
    f1.protocol = 6;
    f1.identification = 0x1235;
    f1.offset = 4;
    f1.mf = false;
    f1.payload = {5,6};

    IPv4Header::Ipv4Fragment f0 = f1;
    f0.offset = 0;
    f0.mf = true;
    f0.payload = {1,2,3,4};

    auto maybe = r.add_fragment(f1);
    EXPECT_FALSE(maybe);
    maybe = r.add_fragment(f0);
    ASSERT_TRUE(maybe);
    EXPECT_EQ(maybe->size(), 6);
    EXPECT_EQ((*maybe)[2], 3);
}

TEST(IPv4FragmentationTest, OverlappingFragments) {
    Ipv4FragmentReassembler r;
    IPv4Header::Ipv4Fragment f1;
    f1.src_ip = IPv4Address::from_string("10.0.0.1");
    f1.dst_ip = IPv4Address::from_string("10.0.0.2");
    f1.protocol = 6;
    f1.identification = 0x2000;
    f1.offset = 0;
    f1.mf = true;
    f1.payload = {1,2,3,4};

    IPv4Header::Ipv4Fragment f2 = f1;
    f2.offset = 2; // overlaps bytes 2..3
    f2.mf = false;
    f2.payload = {9,9,9};

    auto m1 = r.add_fragment(f1);
    EXPECT_FALSE(m1);
    auto m2 = r.add_fragment(f2);
    // Overlapping fragments should either fail reassembly or produce consistent result; here we expect no reassembly due to gap handling logic
    EXPECT_FALSE(m2);
}

TEST(IPv4FragmentationTest, TimeoutClearsState) {
    // Use very short timeout to force cleanup
    Ipv4FragmentReassembler r(std::chrono::seconds(0));
    IPv4Header::Ipv4Fragment f1;
    f1.src_ip = IPv4Address::from_string("10.0.0.1");
    f1.dst_ip = IPv4Address::from_string("10.0.0.2");
    f1.protocol = 6;
    f1.identification = 0x3000;
    f1.offset = 0;
    f1.mf = true;
    f1.payload = {1,2,3,4};

    IPv4Header::Ipv4Fragment f2 = f1;
    f2.offset = 4;
    f2.mf = false;
    f2.payload = {5,6};

    auto maybe = r.add_fragment(f1);
    EXPECT_FALSE(maybe);
    // adding second fragment should not result in reassembly because timeout cleared the first fragment entry
    auto maybe2 = r.add_fragment(f2);
    EXPECT_FALSE(maybe2);
}

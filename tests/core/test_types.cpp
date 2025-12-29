#include <gtest/gtest.h>
#include <wadjet/core/types.hpp>

namespace wadjet::test {

// ============================================================================
// MacAddress Tests
// ============================================================================

TEST(MacAddress, FromBytes) {
    auto mac = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    EXPECT_EQ(mac.bytes[0], 0x00);
    EXPECT_EQ(mac.bytes[1], 0x11);
    EXPECT_EQ(mac.bytes[2], 0x22);
    EXPECT_EQ(mac.bytes[3], 0x33);
    EXPECT_EQ(mac.bytes[4], 0x44);
    EXPECT_EQ(mac.bytes[5], 0x55);
}

TEST(MacAddress, ToString) {
    auto mac = MacAddress::from_bytes(0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff);
    EXPECT_EQ(mac.to_string(), "aa:bb:cc:dd:ee:ff");
}

TEST(MacAddress, FromString) {
    auto mac = MacAddress::from_string("12:34:56:78:9a:bc");
    EXPECT_EQ(mac.bytes[0], 0x12);
    EXPECT_EQ(mac.bytes[1], 0x34);
    EXPECT_EQ(mac.bytes[2], 0x56);
    EXPECT_EQ(mac.bytes[3], 0x78);
    EXPECT_EQ(mac.bytes[4], 0x9a);
    EXPECT_EQ(mac.bytes[5], 0xbc);
}

TEST(MacAddress, IsBroadcast) {
    auto broadcast = MacAddress::from_bytes(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
    auto unicast = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

    EXPECT_TRUE(broadcast.is_broadcast());
    EXPECT_FALSE(unicast.is_broadcast());
}

TEST(MacAddress, IsMulticast) {
    auto multicast = MacAddress::from_bytes(0x01, 0x00, 0x5e, 0x00, 0x00, 0x01);
    auto unicast = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

    EXPECT_TRUE(multicast.is_multicast());
    EXPECT_FALSE(unicast.is_multicast());
}

TEST(MacAddress, Comparison) {
    auto mac1 = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    auto mac2 = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    auto mac3 = MacAddress::from_bytes(0x00, 0x11, 0x22, 0x33, 0x44, 0x56);

    EXPECT_EQ(mac1, mac2);
    EXPECT_NE(mac1, mac3);
    EXPECT_LT(mac1, mac3);
}

// ============================================================================
// IPv4Address Tests
// ============================================================================

TEST(IPv4Address, FromBytes) {
    auto addr = IPv4Address::from_bytes(192, 168, 1, 100);
    EXPECT_EQ(addr.bytes[0], 192);
    EXPECT_EQ(addr.bytes[1], 168);
    EXPECT_EQ(addr.bytes[2], 1);
    EXPECT_EQ(addr.bytes[3], 100);
}

TEST(IPv4Address, ToString) {
    auto addr = IPv4Address::from_bytes(10, 20, 30, 40);
    EXPECT_EQ(addr.to_string(), "10.20.30.40");
}

TEST(IPv4Address, FromString) {
    auto addr = IPv4Address::from_string("192.168.1.1");
    EXPECT_EQ(addr.bytes[0], 192);
    EXPECT_EQ(addr.bytes[1], 168);
    EXPECT_EQ(addr.bytes[2], 1);
    EXPECT_EQ(addr.bytes[3], 1);
}

TEST(IPv4Address, ToUint32) {
    auto addr = IPv4Address::from_bytes(192, 168, 1, 1);
    // 192.168.1.1 in network byte order
    EXPECT_EQ(addr.to_uint32(), 0xC0A80101);
}

TEST(IPv4Address, Comparison) {
    auto addr1 = IPv4Address::from_bytes(192, 168, 1, 1);
    auto addr2 = IPv4Address::from_bytes(192, 168, 1, 1);
    auto addr3 = IPv4Address::from_bytes(192, 168, 1, 2);

    EXPECT_EQ(addr1, addr2);
    EXPECT_NE(addr1, addr3);
}

}  // namespace wadjet::test

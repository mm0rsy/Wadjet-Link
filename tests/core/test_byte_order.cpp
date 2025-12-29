#include <gtest/gtest.h>
#include <wadjet/core/byte_order.hpp>

namespace wadjet::test {

TEST(ByteOrder, ReadBe16) {
    std::byte data[] = {std::byte{0x12}, std::byte{0x34}};
    EXPECT_EQ(read_be16(data), 0x1234);
}

TEST(ByteOrder, ReadBe32) {
    std::byte data[] = {std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    EXPECT_EQ(read_be32(data), 0x12345678);
}

TEST(ByteOrder, ReadBe64) {
    std::byte data[] = {std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78},
                        std::byte{0x9a}, std::byte{0xbc}, std::byte{0xde}, std::byte{0xf0}};
    EXPECT_EQ(read_be64(data), 0x123456789abcdef0ULL);
}

TEST(ByteOrder, WriteBe16) {
    std::byte data[2] = {};
    write_be16(data, 0x1234);
    EXPECT_EQ(static_cast<std::uint8_t>(data[0]), 0x12);
    EXPECT_EQ(static_cast<std::uint8_t>(data[1]), 0x34);
}

TEST(ByteOrder, WriteBe32) {
    std::byte data[4] = {};
    write_be32(data, 0x12345678);
    EXPECT_EQ(static_cast<std::uint8_t>(data[0]), 0x12);
    EXPECT_EQ(static_cast<std::uint8_t>(data[1]), 0x34);
    EXPECT_EQ(static_cast<std::uint8_t>(data[2]), 0x56);
    EXPECT_EQ(static_cast<std::uint8_t>(data[3]), 0x78);
}

TEST(ByteOrder, ReadLe16) {
    std::byte data[] = {std::byte{0x34}, std::byte{0x12}};
    EXPECT_EQ(read_le16(data), 0x1234);
}

TEST(ByteOrder, ReadLe32) {
    std::byte data[] = {std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12}};
    EXPECT_EQ(read_le32(data), 0x12345678);
}

TEST(ByteOrder, WriteLe16) {
    std::byte data[2] = {};
    write_le16(data, 0x1234);
    EXPECT_EQ(static_cast<std::uint8_t>(data[0]), 0x34);
    EXPECT_EQ(static_cast<std::uint8_t>(data[1]), 0x12);
}

TEST(ByteOrder, RoundTrip) {
    std::byte be_data[4] = {};
    std::byte le_data[4] = {};

    write_be32(be_data, 0xDEADBEEF);
    write_le32(le_data, 0xDEADBEEF);

    EXPECT_EQ(read_be32(be_data), 0xDEADBEEF);
    EXPECT_EQ(read_le32(le_data), 0xDEADBEEF);
}

}  // namespace wadjet::test

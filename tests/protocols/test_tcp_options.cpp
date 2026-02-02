/// @file test_tcp_options.cpp
/// @brief Tests for TCP options parsing

#include "wadjet/protocols/tcp.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <vector>

namespace wadjet::protocols::tcp {

class TcpOptionsTest : public ::testing::Test {
protected:
    /// Helper to create TcpOption from raw data
    TcpOption create_option(TcpOptionKind kind, const std::vector<std::byte>& data) {
        TcpOption opt;
        opt.kind = kind;
        opt.data = data;
        return opt;
    }

    /// Helper to create MSS option
    TcpOption create_mss_option(std::uint16_t mss) {
        std::vector<std::byte> data(2);
        data[0] = std::byte((mss >> 8) & 0xFF);
        data[1] = std::byte(mss & 0xFF);
        return create_option(TcpOptionKind::MaxSegmentSize, data);
    }

    /// Helper to create Window Scale option
    TcpOption create_wscale_option(std::uint8_t scale) {
        return create_option(TcpOptionKind::WindowScale, {std::byte(scale)});
    }

    /// Helper to create Timestamps option
    TcpOption create_timestamps_option(std::uint32_t ts_val, std::uint32_t ts_ecr) {
        std::vector<std::byte> data(8);
        data[0] = std::byte((ts_val >> 24) & 0xFF);
        data[1] = std::byte((ts_val >> 16) & 0xFF);
        data[2] = std::byte((ts_val >> 8) & 0xFF);
        data[3] = std::byte(ts_val & 0xFF);
        data[4] = std::byte((ts_ecr >> 24) & 0xFF);
        data[5] = std::byte((ts_ecr >> 16) & 0xFF);
        data[6] = std::byte((ts_ecr >> 8) & 0xFF);
        data[7] = std::byte(ts_ecr & 0xFF);
        return create_option(TcpOptionKind::Timestamps, data);
    }
};

// ===== MSS (Maximum Segment Size) Option Tests =====

TEST_F(TcpOptionsTest, MSSOptionParsing) {
    auto opt = create_mss_option(1460);
    auto mss = opt.mss();

    ASSERT_TRUE(mss);
    EXPECT_EQ(*mss, 1460);
}

TEST_F(TcpOptionsTest, MSSOptionVariations) {
    struct TestCase {
        std::uint16_t mss;
        const char* description;
    };

    std::array test_cases{
        TestCase{536, "Minimal MSS"},  TestCase{1460, "Ethernet MSS"}, TestCase{1452, "PPPoE MSS"},
        TestCase{9000, "Jumbo frame"}, TestCase{65535, "Maximum MSS"},
    };

    for (const auto& tc : test_cases) {
        auto opt = create_mss_option(tc.mss);
        auto mss = opt.mss();
        ASSERT_TRUE(mss) << tc.description;
        EXPECT_EQ(*mss, tc.mss) << tc.description;
    }
}

TEST_F(TcpOptionsTest, MSSOptionWrongLength) {
    // MSS option with wrong data length
    TcpOption opt;
    opt.kind = TcpOptionKind::MaxSegmentSize;
    opt.data = {std::byte(0x00)};  // Only 1 byte, should be 2

    auto mss = opt.mss();
    EXPECT_FALSE(mss);
}

TEST_F(TcpOptionsTest, MSSOptionEmpty) {
    TcpOption opt;
    opt.kind = TcpOptionKind::MaxSegmentSize;
    opt.data.clear();

    auto mss = opt.mss();
    EXPECT_FALSE(mss);
}

// ===== Window Scale Option Tests =====

TEST_F(TcpOptionsTest, WindowScaleOptionParsing) {
    auto opt = create_wscale_option(7);
    auto scale = opt.window_scale();

    ASSERT_TRUE(scale);
    EXPECT_EQ(*scale, 7);
}

TEST_F(TcpOptionsTest, WindowScaleOptionVariations) {
    // Valid window scale values (0-14, RFC 1323)
    std::array scales{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

    for (auto scale_val : scales) {
        auto opt = create_wscale_option(static_cast<std::uint8_t>(scale_val));
        auto scale = opt.window_scale();

        ASSERT_TRUE(scale) << "Scale " << scale_val;
        EXPECT_EQ(*scale, scale_val) << "Scale " << scale_val;
    }
}

TEST_F(TcpOptionsTest, WindowScaleOptionMaximum) {
    // Maximum valid scale
    auto opt = create_wscale_option(14);
    auto scale = opt.window_scale();

    ASSERT_TRUE(scale);
    EXPECT_EQ(*scale, 14);
}

TEST_F(TcpOptionsTest, WindowScaleOptionWrongLength) {
    TcpOption opt;
    opt.kind = TcpOptionKind::WindowScale;
    opt.data = {std::byte(0x07), std::byte(0x00)};  // 2 bytes, should be 1

    auto scale = opt.window_scale();
    EXPECT_FALSE(scale);
}

TEST_F(TcpOptionsTest, WindowScaleOptionEmpty) {
    TcpOption opt;
    opt.kind = TcpOptionKind::WindowScale;
    opt.data.clear();

    auto scale = opt.window_scale();
    EXPECT_FALSE(scale);
}

// ===== Timestamps Option Tests =====

TEST_F(TcpOptionsTest, TimestampsOptionParsing) {
    auto opt = create_timestamps_option(0x12345678, 0x9ABCDEF0);
    auto ts = opt.timestamps();

    ASSERT_TRUE(ts);
    EXPECT_EQ(ts->first, 0x12345678);   // TS val
    EXPECT_EQ(ts->second, 0x9ABCDEF0);  // TS ecr
}

TEST_F(TcpOptionsTest, TimestampsOptionZeroValues) {
    auto opt = create_timestamps_option(0, 0);
    auto ts = opt.timestamps();

    ASSERT_TRUE(ts);
    EXPECT_EQ(ts->first, 0);
    EXPECT_EQ(ts->second, 0);
}

TEST_F(TcpOptionsTest, TimestampsOptionMaxValues) {
    auto opt = create_timestamps_option(0xFFFFFFFF, 0xFFFFFFFF);
    auto ts = opt.timestamps();

    ASSERT_TRUE(ts);
    EXPECT_EQ(ts->first, 0xFFFFFFFF);
    EXPECT_EQ(ts->second, 0xFFFFFFFF);
}

TEST_F(TcpOptionsTest, TimestampsOptionVariousValues) {
    struct TestCase {
        std::uint32_t ts_val;
        std::uint32_t ts_ecr;
        const char* description;
    };

    std::array test_cases{
        TestCase{0x00000001, 0x00000000, "Initial TS"},
        TestCase{0x12345678, 0x87654321, "Common values"},
        TestCase{0x80000000, 0x80000000, "High bit set"},
        TestCase{1000, 2000, "Small values"},
    };

    for (const auto& tc : test_cases) {
        auto opt = create_timestamps_option(tc.ts_val, tc.ts_ecr);
        auto ts = opt.timestamps();

        ASSERT_TRUE(ts) << tc.description;
        EXPECT_EQ(ts->first, tc.ts_val) << tc.description;
        EXPECT_EQ(ts->second, tc.ts_ecr) << tc.description;
    }
}

TEST_F(TcpOptionsTest, TimestampsOptionWrongLength) {
    // Timestamps option with wrong data length (only 4 bytes instead of 8)
    TcpOption opt;
    opt.kind = TcpOptionKind::Timestamps;
    opt.data = {std::byte(0x12), std::byte(0x34), std::byte(0x56), std::byte(0x78)};

    auto ts = opt.timestamps();
    EXPECT_FALSE(ts);
}

TEST_F(TcpOptionsTest, TimestampsOptionEmpty) {
    TcpOption opt;
    opt.kind = TcpOptionKind::Timestamps;
    opt.data.clear();

    auto ts = opt.timestamps();
    EXPECT_FALSE(ts);
}

// ===== SACK Permitted Option Tests =====

TEST_F(TcpOptionsTest, SackPermittedOption) {
    TcpOption opt;
    opt.kind = TcpOptionKind::SackPermitted;
    opt.data.clear();  // SACK Permitted has no data

    EXPECT_EQ(opt.kind, TcpOptionKind::SackPermitted);
    EXPECT_TRUE(opt.data.empty());
}

// ===== SACK Option Tests =====

TEST_F(TcpOptionsTest, SackOptionWithOnePair) {
    // SACK with one block (8 bytes: 2 x uint32_t)
    std::vector<std::byte> data(8);
    // Start: 1000
    data[0] = std::byte(0x00);
    data[1] = std::byte(0x00);
    data[2] = std::byte(0x03);
    data[3] = std::byte(0xE8);
    // End: 2000
    data[4] = std::byte(0x00);
    data[5] = std::byte(0x00);
    data[6] = std::byte(0x07);
    data[7] = std::byte(0xD0);

    TcpOption opt;
    opt.kind = TcpOptionKind::Sack;
    opt.data = data;

    EXPECT_EQ(opt.kind, TcpOptionKind::Sack);
    EXPECT_EQ(opt.data.size(), 8);
}

TEST_F(TcpOptionsTest, SackOptionWithMultipleBlocks) {
    // SACK with multiple blocks
    std::vector<std::byte> data(16);  // 2 blocks x 8 bytes

    TcpOption opt;
    opt.kind = TcpOptionKind::Sack;
    opt.data = data;

    EXPECT_EQ(opt.kind, TcpOptionKind::Sack);
    EXPECT_EQ(opt.data.size(), 16);
}

// ===== End of Options Tests =====

TEST_F(TcpOptionsTest, EndOfOptionsKind) {
    TcpOption opt;
    opt.kind = TcpOptionKind::EndOfOptions;
    opt.data.clear();

    EXPECT_EQ(opt.kind, TcpOptionKind::EndOfOptions);
}

// ===== No Operation Tests =====

TEST_F(TcpOptionsTest, NoOperationOption) {
    TcpOption opt;
    opt.kind = TcpOptionKind::NoOperation;
    opt.data.clear();  // NOP has no data

    EXPECT_EQ(opt.kind, TcpOptionKind::NoOperation);
    EXPECT_TRUE(opt.data.empty());
}

// ===== Option List Tests =====

TEST_F(TcpOptionsTest, OptionListWithMultipleOptions) {
    std::vector<TcpOption> options;

    // Add MSS
    options.push_back(create_mss_option(1460));

    // Add Window Scale
    options.push_back(create_wscale_option(7));

    // Add Timestamps
    options.push_back(create_timestamps_option(0x12345678, 0x00000000));

    EXPECT_EQ(options.size(), 3);
    EXPECT_EQ(options[0].kind, TcpOptionKind::MaxSegmentSize);
    EXPECT_EQ(options[1].kind, TcpOptionKind::WindowScale);
    EXPECT_EQ(options[2].kind, TcpOptionKind::Timestamps);
}

TEST_F(TcpOptionsTest, OptionListWithNops) {
    std::vector<TcpOption> options;

    // NOPs are used for padding
    TcpOption nop;
    nop.kind = TcpOptionKind::NoOperation;

    options.push_back(nop);
    options.push_back(create_mss_option(1460));
    options.push_back(nop);
    options.push_back(nop);

    EXPECT_EQ(options.size(), 4);
    EXPECT_EQ(options[0].kind, TcpOptionKind::NoOperation);
    EXPECT_EQ(options[1].kind, TcpOptionKind::MaxSegmentSize);
}

// ===== Option Kind Enum Tests =====

TEST_F(TcpOptionsTest, OptionKindValues) {
    EXPECT_EQ(static_cast<int>(TcpOptionKind::EndOfOptions), 0);
    EXPECT_EQ(static_cast<int>(TcpOptionKind::NoOperation), 1);
    EXPECT_EQ(static_cast<int>(TcpOptionKind::MaxSegmentSize), 2);
    EXPECT_EQ(static_cast<int>(TcpOptionKind::WindowScale), 3);
    EXPECT_EQ(static_cast<int>(TcpOptionKind::SackPermitted), 4);
    EXPECT_EQ(static_cast<int>(TcpOptionKind::Sack), 5);
    EXPECT_EQ(static_cast<int>(TcpOptionKind::Timestamps), 8);
}

// ===== Combined Option Scenarios =====

TEST_F(TcpOptionsTest, SynPacketOptions) {
    // Typical SYN packet options
    std::vector<TcpOption> options;

    options.push_back(create_mss_option(1460));
    options.push_back(create_wscale_option(7));

    TcpOption sack_perm;
    sack_perm.kind = TcpOptionKind::SackPermitted;

    options.push_back(sack_perm);
    options.push_back(create_timestamps_option(12345, 0));

    EXPECT_EQ(options.size(), 4);
}

TEST_F(TcpOptionsTest, AckPacketOptions) {
    // Typical ACK packet options (fewer than SYN)
    std::vector<TcpOption> options;

    options.push_back(create_timestamps_option(12346, 12345));

    EXPECT_EQ(options.size(), 1);
    EXPECT_EQ(options[0].kind, TcpOptionKind::Timestamps);
}

TEST_F(TcpOptionsTest, DataPacketNoOptions) {
    // Regular data packet may have no options
    std::vector<TcpOption> options;

    EXPECT_TRUE(options.empty());
}

}  // namespace wadjet::protocols::tcp

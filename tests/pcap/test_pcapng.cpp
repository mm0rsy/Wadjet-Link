/// @file test_pcapng.cpp
/// @brief Tests for PcapngWriter

#include <gtest/gtest.h>
#include <wadjet/core/timestamp.hpp>
#include <wadjet/net/packet.hpp>
#include <wadjet/pcap/pcapng_file.hpp>
#include <wadjet/pcap/pcapng_writer.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace wadjet::test {

namespace fs = std::filesystem;

/// @brief Test fixture for PCAPNG tests
class PcapngWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "wadjet_pcapng_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override { fs::remove_all(test_dir_); }

    fs::path test_file(const std::string& name) const { return test_dir_ / name; }

    /// @brief Create a simple Ethernet packet for testing
    static Packet create_test_packet(std::size_t payload_size = 64) {
        std::vector<std::byte> data;
        // Ethernet header (14 bytes)
        // Destination MAC: ff:ff:ff:ff:ff:ff (broadcast)
        for (int i = 0; i < 6; ++i)
            data.push_back(std::byte{0xff});
        // Source MAC: 00:11:22:33:44:55
        data.push_back(std::byte{0x00});
        data.push_back(std::byte{0x11});
        data.push_back(std::byte{0x22});
        data.push_back(std::byte{0x33});
        data.push_back(std::byte{0x44});
        data.push_back(std::byte{0x55});
        // EtherType: IPv4 (0x0800)
        data.push_back(std::byte{0x08});
        data.push_back(std::byte{0x00});
        // Payload
        for (std::size_t i = 0; i < payload_size; ++i) {
            data.push_back(static_cast<std::byte>(i & 0xff));
        }
        return Packet(ByteSpan(data.data(), data.size()), Timestamp::now());
    }

    /// @brief Read file contents into vector
    static std::vector<std::byte> read_file(const fs::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return {};

        auto size = file.tellg();
        file.seekg(0);

        std::vector<std::byte> data(static_cast<std::size_t>(size));
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }

    /// @brief Read uint32 from data at offset
    static std::uint32_t read_u32(const std::vector<std::byte>& data, std::size_t offset) {
        std::uint32_t value;
        std::memcpy(&value, &data[offset], 4);
        return value;
    }

    /// @brief Read uint16 from data at offset
    static std::uint16_t read_u16(const std::vector<std::byte>& data, std::size_t offset) {
        std::uint16_t value;
        std::memcpy(&value, &data[offset], 2);
        return value;
    }

    fs::path test_dir_;
};

//==============================================================================
// Section Header Block (SHB) Tests
//==============================================================================

TEST_F(PcapngWriterTest, CreateFile_WritesSHB) {
    auto path = test_file("shb_test.pcapng");

    pcap::PcapngWriterOptions opts;
    opts.user_application = "TestApp";

    {
        auto result = pcap::PcapngWriter::create(path, opts);
        ASSERT_TRUE(result.is_ok()) << result.error().message;

        // Close writer to flush when it goes out of scope
        result.value().flush();
    }

    // Read and verify
    auto data = read_file(path);
    ASSERT_GE(data.size(), 28u);  // Minimum SHB size

    // Check block type
    auto block_type = read_u32(data, 0);
    EXPECT_EQ(block_type, static_cast<std::uint32_t>(pcap::PcapngBlockType::SectionHeader));

    // Check byte order magic
    auto magic = read_u32(data, 8);
    EXPECT_EQ(magic, pcap::PCAPNG_BYTE_ORDER_MAGIC);

    // Check version
    auto major = read_u16(data, 12);
    auto minor = read_u16(data, 14);
    EXPECT_EQ(major, 1u);
    EXPECT_EQ(minor, 0u);
}

TEST_F(PcapngWriterTest, CreateFile_WithOptions) {
    auto path = test_file("shb_options.pcapng");

    pcap::PcapngWriterOptions opts;
    opts.comment = "Test capture";
    opts.hardware = "Test Hardware";
    opts.os = "TestOS";
    opts.user_application = "WadjetTest";

    auto result = pcap::PcapngWriter::create(path, opts);
    ASSERT_TRUE(result.is_ok());
    result.value().flush();

    auto data = read_file(path);
    EXPECT_GT(data.size(), 28u);  // Should have options
}

//==============================================================================
// Interface Description Block (IDB) Tests
//==============================================================================

TEST_F(PcapngWriterTest, AddInterface_WritesIDB) {
    auto path = test_file("idb_test.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    iface.description = "Test interface";
    iface.link_type = pcap::LinkType::Ethernet;
    iface.snap_len = 1500;
    iface.use_nanoseconds = true;

    auto id = writer.add_interface(iface);
    EXPECT_EQ(id, 0u);

    writer.flush();

    auto data = read_file(path);
    EXPECT_GT(data.size(), 40u);  // SHB + IDB minimum
}

TEST_F(PcapngWriterTest, AddMultipleInterfaces) {
    auto path = test_file("multi_idb.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface1;
    iface1.name = "eth0";
    auto id1 = writer.add_interface(iface1);
    EXPECT_EQ(id1, 0u);

    pcap::InterfaceInfo iface2;
    iface2.name = "eth1";
    auto id2 = writer.add_interface(iface2);
    EXPECT_EQ(id2, 1u);

    EXPECT_EQ(writer.interface_count(), 2u);
}

//==============================================================================
// Enhanced Packet Block (EPB) Tests
//==============================================================================

TEST_F(PcapngWriterTest, WritePacket_WritesEPB) {
    auto path = test_file("epb_test.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    auto pkt = create_test_packet(64);
    auto write_result = writer.write_packet(pkt.view());
    EXPECT_TRUE(write_result.is_ok()) << write_result.error().message;

    EXPECT_EQ(writer.packet_count(), 1u);
}

TEST_F(PcapngWriterTest, WritePacket_WithComment) {
    auto path = test_file("epb_comment.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    writer.add_interface(iface);

    auto pkt = create_test_packet(64);
    auto write_result = writer.write_packet(pkt.view(), 0, "Important packet");
    EXPECT_TRUE(write_result.is_ok());

    // File should be larger due to comment option
    writer.flush();
    auto data = read_file(path);
    EXPECT_GT(data.size(), 150u);  // Has comment overhead
}

TEST_F(PcapngWriterTest, WritePacket_MultipleInterfaces) {
    auto path = test_file("epb_multi_iface.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface1, iface2;
    iface1.name = "eth0";
    iface2.name = "eth1";
    writer.add_interface(iface1);
    writer.add_interface(iface2);

    auto pkt = create_test_packet(64);

    // Write to interface 0
    auto res1 = writer.write_packet(pkt.view(), 0);
    EXPECT_TRUE(res1.is_ok());

    // Write to interface 1
    auto res2 = writer.write_packet(pkt.view(), 1);
    EXPECT_TRUE(res2.is_ok());

    EXPECT_EQ(writer.packet_count(), 2u);
}

TEST_F(PcapngWriterTest, WritePacket_InvalidInterface) {
    auto path = test_file("epb_invalid_iface.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    writer.add_interface(iface);

    auto pkt = create_test_packet(64);

    // Try to write to invalid interface
    auto write_result = writer.write_packet(pkt.view(), 5);  // Invalid ID
    EXPECT_FALSE(write_result.is_ok());
}

TEST_F(PcapngWriterTest, WritePacket_AutoCreateInterface) {
    auto path = test_file("epb_auto_iface.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    // Don't add interface - should auto-create default
    auto pkt = create_test_packet(64);
    auto write_result = writer.write_packet(pkt.view());
    EXPECT_TRUE(write_result.is_ok());

    EXPECT_EQ(writer.interface_count(), 1u);
}

//==============================================================================
// Timestamp Tests
//==============================================================================

TEST_F(PcapngWriterTest, WritePacket_NanosecondTimestamp) {
    auto path = test_file("ns_timestamp.pcapng");

    pcap::PcapngWriterOptions opts;
    opts.use_nanoseconds = true;

    auto result = pcap::PcapngWriter::create(path, opts);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    iface.use_nanoseconds = true;
    writer.add_interface(iface);

    auto pkt = create_test_packet(64);
    auto write_result = writer.write_packet(pkt.view(), 0);
    EXPECT_TRUE(write_result.is_ok());
}

TEST_F(PcapngWriterTest, WritePacket_MicrosecondTimestamp) {
    auto path = test_file("us_timestamp.pcapng");

    pcap::PcapngWriterOptions opts;
    opts.use_nanoseconds = false;

    auto result = pcap::PcapngWriter::create(path, opts);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    iface.use_nanoseconds = false;
    writer.add_interface(iface);

    auto pkt = create_test_packet(64);
    auto write_result = writer.write_packet(pkt.view(), 0);
    EXPECT_TRUE(write_result.is_ok());
}

//==============================================================================
// Interface Statistics Block (ISB) Tests
//==============================================================================

TEST_F(PcapngWriterTest, WriteStatistics) {
    auto path = test_file("isb_test.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    writer.add_interface(iface);

    // Write some packets
    for (int i = 0; i < 10; ++i) {
        auto pkt = create_test_packet(64);
        writer.write_packet(pkt.view(), 0);
    }

    // Write statistics
    auto stats_result = writer.write_statistics(0, 1000, 50);
    EXPECT_TRUE(stats_result.is_ok());
}

//==============================================================================
// Large File Tests
//==============================================================================

TEST_F(PcapngWriterTest, WriteManyPackets) {
    auto path = test_file("many_packets.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    constexpr std::size_t NUM_PACKETS = 1000;

    for (std::size_t i = 0; i < NUM_PACKETS; ++i) {
        auto pkt = create_test_packet(64 + (i % 100));
        auto write_result = writer.write_packet(pkt.view());
        ASSERT_TRUE(write_result.is_ok()) << "Failed at packet " << i;
    }

    EXPECT_EQ(writer.packet_count(), NUM_PACKETS);
    EXPECT_GT(writer.bytes_written(), 0u);
}

TEST_F(PcapngWriterTest, WriteLargePacket) {
    auto path = test_file("large_packet.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    // Create jumbo frame sized packet (9000 bytes)
    auto pkt = create_test_packet(9000 - 14);  // Subtract Ethernet header
    auto write_result = writer.write_packet(pkt.view());
    EXPECT_TRUE(write_result.is_ok());
}

//==============================================================================
// Error Handling Tests
//==============================================================================

TEST_F(PcapngWriterTest, CreateFile_InvalidPath) {
    auto path = fs::path("/nonexistent/directory/test.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PcapngWriterTest, IPacketSink_Interface) {
    auto path = test_file("sink_test.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    // Use through IPacketSink interface
    IPacketSink& sink = result.value();

    auto pkt = create_test_packet(64);
    auto write_result = sink.write_packet(pkt.view());
    EXPECT_TRUE(write_result.is_ok());

    EXPECT_EQ(sink.packet_count(), 1u);
    EXPECT_FALSE(sink.description().empty());
}

//==============================================================================
// File Structure Validation Tests
//==============================================================================

TEST_F(PcapngWriterTest, ValidateBlockStructure) {
    auto path = test_file("structure_test.pcapng");

    auto result = pcap::PcapngWriter::create(path);
    ASSERT_TRUE(result.is_ok());

    auto& writer = result.value();

    pcap::InterfaceInfo iface;
    iface.name = "eth0";
    writer.add_interface(iface);

    auto pkt = create_test_packet(64);
    writer.write_packet(pkt.view(), 0);
    writer.flush();

    // Read and validate block structure
    auto data = read_file(path);
    ASSERT_GT(data.size(), 0u);

    std::size_t offset = 0;
    int block_count = 0;

    while (offset + 8 <= data.size()) {
        [[maybe_unused]] auto block_type = read_u32(data, offset);
        auto block_len = read_u32(data, offset + 4);

        // Block length must be at least 12 (header + trailer)
        EXPECT_GE(block_len, 12u) << "Block " << block_count << " has invalid length";

        // Block length must be 4-byte aligned
        EXPECT_EQ(block_len % 4, 0u) << "Block " << block_count << " length not aligned";

        if (offset + block_len > data.size())
            break;

        // Trailing block length should match
        auto trailing_len = read_u32(data, offset + block_len - 4);
        EXPECT_EQ(block_len, trailing_len) << "Block " << block_count << " trailer mismatch";

        offset += block_len;
        block_count++;
    }

    // Should have at least SHB, IDB, EPB
    EXPECT_GE(block_count, 3);
}

}  // namespace wadjet::test

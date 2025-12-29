#include <gtest/gtest.h>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/pcap/pcap_writer.hpp>

#include <cstdio>
#include <filesystem>

namespace wadjet::test {

class PcapTest : public ::testing::Test {
protected:
    std::filesystem::path temp_path_;

    void SetUp() override {
        temp_path_ = std::filesystem::temp_directory_path() / "wadjet_test.pcap";
    }

    void TearDown() override {
        if (std::filesystem::exists(temp_path_)) {
            std::filesystem::remove(temp_path_);
        }
    }
};

TEST_F(PcapTest, WriteAndRead) {
    // Create test packet
    std::vector<std::byte> data(64);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::byte>(i & 0xFF);
    }
    auto ts = Timestamp::from_unix(1234567890, 123456000);
    Packet original_packet(ByteSpan(data.data(), data.size()), ts);

    // Write to file
    {
        auto writer = pcap::PcapWriter::create(temp_path_);
        ASSERT_TRUE(writer.is_ok()) << writer.error().message;

        auto result = writer->write_packet(original_packet);
        ASSERT_TRUE(result.is_ok()) << result.error().message;
        EXPECT_EQ(writer->packet_count(), 1);
    }

    // Read back
    {
        auto reader = pcap::PcapReader::open(temp_path_);
        ASSERT_TRUE(reader.is_ok()) << reader.error().message;

        EXPECT_EQ(reader->link_type(), pcap::LinkType::Ethernet);

        auto packet = reader->next_packet();
        ASSERT_TRUE(packet.has_value());

        EXPECT_EQ(packet->size(), original_packet.size());
        EXPECT_EQ(packet->timestamp().seconds(), ts.seconds());

        // Verify data
        auto original_data = original_packet.data();
        auto read_data = packet->data();
        ASSERT_EQ(original_data.size(), read_data.size());
        for (std::size_t i = 0; i < original_data.size(); ++i) {
            EXPECT_EQ(original_data[i], read_data[i]) << "Mismatch at byte " << i;
        }

        // No more packets
        EXPECT_FALSE(reader->next_packet().has_value());
    }
}

TEST_F(PcapTest, WriteMultiplePackets) {
    constexpr std::size_t NUM_PACKETS = 10;

    // Write packets
    {
        auto writer = pcap::PcapWriter::create(temp_path_);
        ASSERT_TRUE(writer.is_ok());

        for (std::size_t i = 0; i < NUM_PACKETS; ++i) {
            std::vector<std::byte> data(64 + i);
            Packet pkt(ByteSpan(data.data(), data.size()),
                       Timestamp::from_unix(static_cast<std::int64_t>(i), 0));
            auto result = writer->write_packet(pkt);
            ASSERT_TRUE(result.is_ok());
        }
        EXPECT_EQ(writer->packet_count(), NUM_PACKETS);
    }

    // Read all packets
    {
        auto reader = pcap::PcapReader::open(temp_path_);
        ASSERT_TRUE(reader.is_ok());

        auto packets = reader->read_all();
        EXPECT_EQ(packets.size(), NUM_PACKETS);

        for (std::size_t i = 0; i < packets.size(); ++i) {
            EXPECT_EQ(packets[i].size(), 64 + i);
            EXPECT_EQ(packets[i].timestamp().seconds(), static_cast<std::int64_t>(i));
        }
    }
}

TEST_F(PcapTest, Reset) {
    // Write a packet
    {
        auto writer = pcap::PcapWriter::create(temp_path_);
        ASSERT_TRUE(writer.is_ok());

        std::vector<std::byte> data(64);
        Packet pkt(ByteSpan(data.data(), data.size()));
        writer->write_packet(pkt);
    }

    // Read, reset, read again
    {
        auto reader = pcap::PcapReader::open(temp_path_);
        ASSERT_TRUE(reader.is_ok());

        auto pkt1 = reader->next_packet();
        ASSERT_TRUE(pkt1.has_value());

        auto pkt2 = reader->next_packet();
        EXPECT_FALSE(pkt2.has_value());

        reader->reset();

        auto pkt3 = reader->next_packet();
        ASSERT_TRUE(pkt3.has_value());
    }
}

TEST_F(PcapTest, NanosecondPrecision) {
    auto ts = Timestamp::from_unix(1234567890, 123456789);
    std::vector<std::byte> data(64);
    Packet original_packet(ByteSpan(data.data(), data.size()), ts);

    // Write with nanosecond precision
    {
        pcap::PcapWriter::Options opts;
        opts.nanosecond_precision = true;
        auto writer = pcap::PcapWriter::create(temp_path_, opts);
        ASSERT_TRUE(writer.is_ok());
        writer->write_packet(original_packet);
    }

    // Read back
    {
        auto reader = pcap::PcapReader::open(temp_path_);
        ASSERT_TRUE(reader.is_ok());
        EXPECT_TRUE(reader->is_nanosecond());

        auto packet = reader->next_packet();
        ASSERT_TRUE(packet.has_value());
        EXPECT_EQ(packet->timestamp().nanoseconds(), ts.nanoseconds());
    }
}

TEST_F(PcapTest, InvalidFile) {
    auto result = pcap::PcapReader::open("/nonexistent/path.pcap");
    EXPECT_FALSE(result.is_ok());
}

}  // namespace wadjet::test

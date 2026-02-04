#include <gtest/gtest.h>

#include "wadjet/distributed/pcap_merger.hpp"
#include "wadjet/core/timestamp.hpp"

using namespace wadjet;
using namespace wadjet::distributed;

/**
 * @brief Unit tests for PcapMerger (T049)
 */
class PcapMergerTest : public ::testing::Test {
protected:
    void SetUp() override {
        merger_ = std::make_unique<PcapMerger>();
    }
    
    std::unique_ptr<PcapMerger> merger_;
};

/**
 * @brief Test adding packets from a single node
 */
TEST_F(PcapMergerTest, AddPacketsFromSingleNode) {
    std::vector<Packet> packets;
    
    // Create test packets
    for (int i = 0; i < 5; ++i) {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp::now());
        packets.push_back(pkt);
    }
    
    auto result = merger_->add_packets("node1", packets);
    EXPECT_TRUE(result);
    EXPECT_EQ(merger_->node_count(), 1);
    EXPECT_EQ(merger_->total_packets(), 5);
}

/**
 * @brief Test adding packets from multiple nodes
 */
TEST_F(PcapMergerTest, AddPacketsFromMultipleNodes) {
    std::vector<Packet> packets1, packets2, packets3;
    
    // Create packets for node 1
    for (int i = 0; i < 3; ++i) {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp::now());
        packets1.push_back(pkt);
    }
    
    // Create packets for node 2
    for (int i = 0; i < 4; ++i) {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp::now());
        packets2.push_back(pkt);
    }
    
    // Create packets for node 3
    for (int i = 0; i < 2; ++i) {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp::now());
        packets3.push_back(pkt);
    }
    
    auto result1 = merger_->add_packets("node1", packets1);
    auto result2 = merger_->add_packets("node2", packets2);
    auto result3 = merger_->add_packets("node3", packets3);
    
    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_TRUE(result3);
    EXPECT_EQ(merger_->node_count(), 3);
    EXPECT_EQ(merger_->total_packets(), 9);
}

/**
 * @brief Test that empty node ID is rejected
 */
TEST_F(PcapMergerTest, RejectEmptyNodeId) {
    std::vector<Packet> packets;
    Packet pkt(64);
    pkt.resize(64);
    packets.push_back(pkt);
    
    auto result = merger_->add_packets("", packets);
    EXPECT_FALSE(result);
}

/**
 * @brief Test merge operation
 */
TEST_F(PcapMergerTest, MergePackets) {
    std::vector<Packet> packets1, packets2;
    
    // Node 1 packets
    for (int i = 0; i < 3; ++i) {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp::now());
        packets1.push_back(pkt);
    }
    
    // Node 2 packets
    for (int i = 0; i < 2; ++i) {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp::now());
        packets2.push_back(pkt);
    }
    
    EXPECT_TRUE(merger_->add_packets("node1", packets1));
    EXPECT_TRUE(merger_->add_packets("node2", packets2));
    
    auto merge_result = merger_->merge(std::filesystem::temp_directory_path() / "test_merge.pcap");
    EXPECT_TRUE(merge_result);
    
    const auto& result = merge_result.value();
    EXPECT_EQ(result.total_packets, 5);
    EXPECT_EQ(result.source_nodes.size(), 2);
}

/**
 * @brief Test merge with timestamp sorting
 */
TEST_F(PcapMergerTest, MergeWithTimestampSorting) {
    PcapMergerOptions opts;
    opts.skip_reordering = false;  // Enable timestamp sorting
    merger_ = std::make_unique<PcapMerger>(opts);
    
    std::vector<Packet> packets;
    
    // Create packets with different timestamps
    auto now = std::chrono::system_clock::now();
    
    // Packet 1: now + 2 seconds
    {
        Packet pkt(64);
        pkt.resize(64);
        auto ts = now + std::chrono::seconds(2);
        pkt.set_timestamp(Timestamp(ts));
        packets.push_back(pkt);
    }
    
    // Packet 2: now
    {
        Packet pkt(64);
        pkt.resize(64);
        pkt.set_timestamp(Timestamp(now));
        packets.push_back(pkt);
    }
    
    // Packet 3: now + 1 second
    {
        Packet pkt(64);
        pkt.resize(64);
        auto ts = now + std::chrono::seconds(1);
        pkt.set_timestamp(Timestamp(ts));
        packets.push_back(pkt);
    }
    
    EXPECT_TRUE(merger_->add_packets("node1", packets));
    
    auto merge_result = merger_->merge(std::filesystem::temp_directory_path() / "test_sort.pcap");
    EXPECT_TRUE(merge_result);
    
    const auto& result = merge_result.value();
    EXPECT_EQ(result.total_packets, 3);
}

/**
 * @brief Test clear operation
 */
TEST_F(PcapMergerTest, ClearPackets) {
    std::vector<Packet> packets;
    Packet pkt(64);
    pkt.resize(64);
    packets.push_back(pkt);
    packets.push_back(pkt);
    packets.push_back(pkt);
    
    EXPECT_TRUE(merger_->add_packets("node1", packets));
    EXPECT_EQ(merger_->total_packets(), 3);
    
    merger_->clear();
    EXPECT_EQ(merger_->node_count(), 0);
    EXPECT_EQ(merger_->total_packets(), 0);
}

/**
 * @brief Test merge result metadata
 */
TEST_F(PcapMergerTest, MergeResultMetadata) {
    std::vector<Packet> packets1, packets2;
    
    // Node 1: 2 packets
    for (int i = 0; i < 2; ++i) {
        Packet pkt(32);
        pkt.resize(32);
        pkt.set_timestamp(Timestamp::now());
        packets1.push_back(pkt);
    }
    
    // Node 2: 3 packets
    for (int i = 0; i < 3; ++i) {
        Packet pkt(48);
        pkt.resize(48);
        pkt.set_timestamp(Timestamp::now());
        packets2.push_back(pkt);
    }
    
    EXPECT_TRUE(merger_->add_packets("node1", packets1));
    EXPECT_TRUE(merger_->add_packets("node2", packets2));
    
    auto merge_result = merger_->merge(std::filesystem::temp_directory_path() / "test_metadata.pcap");
    EXPECT_TRUE(merge_result);
    
    const auto& result = merge_result.value();
    EXPECT_EQ(result.total_packets, 5);
    EXPECT_EQ(result.total_bytes, 2 * 32 + 3 * 48);
    EXPECT_EQ(result.source_nodes.size(), 2);
}

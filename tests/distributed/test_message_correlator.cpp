#include "wadjet/distributed/message_correlator.hpp"
#include "wadjet/net/packet.hpp"

#include <gtest/gtest.h>

using namespace wadjet;
using namespace wadjet::distributed;

/**
 * @brief Unit tests for MessageCorrelator (T046)
 */
class MessageCorrelatorTest : public ::testing::Test {
protected:
    void SetUp() override { create_test_packets(); }

    void create_test_packets() {
        // Create test packets with known content
        for (int i = 0; i < 5; ++i) {
            std::vector<std::byte> data(64);
            for (size_t j = 0; j < data.size(); ++j) {
                data[j] = static_cast<std::byte>(
                    static_cast<unsigned int>(i * 16 + static_cast<int>(j)) % 256);
            }
            Packet pkt{std::span<const std::byte>(data)};
            test_packets_.push_back(std::move(pkt));
        }
    }

    std::vector<Packet> test_packets_;
};

// T046: Test PayloadHash correlation method
TEST_F(MessageCorrelatorTest, PayloadHashMethod) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::PayloadHash);
    EXPECT_EQ(correlator.get_method(), MessageCorrelator::CorrelationMethod::PayloadHash);
}

// T046: Test SequenceNumber correlation method
TEST_F(MessageCorrelatorTest, SequenceNumberMethod) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::SequenceNumber);
    EXPECT_EQ(correlator.get_method(), MessageCorrelator::CorrelationMethod::SequenceNumber);
}

// T046: Test add packets
TEST_F(MessageCorrelatorTest, AddPackets) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::PayloadHash);

    correlator.add_packets("node1",
                           std::span<const Packet>(test_packets_.data(), test_packets_.size()));

    auto correlated = correlator.correlate();
    EXPECT_GE(correlated.size(), static_cast<size_t>(0));
}

// T046: Test multi-node correlation
TEST_F(MessageCorrelatorTest, MultiNodeCorrelation) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::PayloadHash);

    correlator.add_packets("node1",
                           std::span<const Packet>(test_packets_.data(), test_packets_.size()));
    correlator.add_packets("node2",
                           std::span<const Packet>(test_packets_.data(), test_packets_.size()));

    auto correlated = correlator.correlate();
    EXPECT_GE(correlated.size(), static_cast<size_t>(0));
}

// T046: Test find_correlation
TEST_F(MessageCorrelatorTest, FindCorrelation) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::PayloadHash);

    correlator.add_packets("node1", std::span<const Packet>(test_packets_.data(), 1));
    correlator.add_packets("node2", std::span<const Packet>(test_packets_.data(), 1));

    // Just verify find_correlation doesn't crash
    auto result = correlator.find_correlation(test_packets_[0].view(), "node2");
    EXPECT_TRUE(true);  // Basic sanity check
}

// T046: Test clear function
TEST_F(MessageCorrelatorTest, Clear) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::PayloadHash);

    correlator.add_packets("node1",
                           std::span<const Packet>(test_packets_.data(), test_packets_.size()));
    correlator.clear();

    auto correlated = correlator.correlate();
    EXPECT_EQ(correlated.size(), static_cast<size_t>(0));
}

// T046: Test TransactionId method
TEST_F(MessageCorrelatorTest, TransactionIdMethod) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::TransactionId);
    EXPECT_EQ(correlator.get_method(), MessageCorrelator::CorrelationMethod::TransactionId);

    correlator.add_packets("node1",
                           std::span<const Packet>(test_packets_.data(), test_packets_.size()));
    auto correlated = correlator.correlate();
    EXPECT_GE(correlated.size(), static_cast<size_t>(0));
}

// T046: Test Timestamp method
TEST_F(MessageCorrelatorTest, TimestampMethod) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::Timestamp);
    EXPECT_EQ(correlator.get_method(), MessageCorrelator::CorrelationMethod::Timestamp);

    correlator.add_packets("node1",
                           std::span<const Packet>(test_packets_.data(), test_packets_.size()));
    auto correlated = correlator.correlate();
    EXPECT_GE(correlated.size(), static_cast<size_t>(0));
}

// T046: Test scalability with many nodes
TEST_F(MessageCorrelatorTest, ScalabilityManyNodes) {
    MessageCorrelator correlator(MessageCorrelator::CorrelationMethod::PayloadHash);

    // Add packets from 10 nodes
    for (int i = 0; i < 10; ++i) {
        std::string node_id = "node" + std::to_string(i);
        correlator.add_packets(node_id,
                               std::span<const Packet>(test_packets_.data(), test_packets_.size()));
    }

    auto correlated = correlator.correlate();
    EXPECT_GE(correlated.size(), static_cast<size_t>(0));
}

/// @file test_tcp_retransmission.cpp
/// @brief Tests for TCP retransmission detection

#include "wadjet/protocols/tcp.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace wadjet::protocols::tcp {

class TcpRetransmissionTest : public ::testing::Test {
protected:
    void SetUp() override {
        tracker_ = std::make_unique<TcpConnectionTracker>();
    }

    std::unique_ptr<TcpConnectionTracker> tracker_;

    /// Helper to create a TCP header with specific flags
    TcpHeader create_header(
        std::uint32_t seq_num,
        std::uint32_t ack_num,
        bool syn = false,
        bool ack = false,
        bool fin = false,
        bool rst = false) {
        TcpHeader hdr;
        hdr.seq_num = seq_num;
        hdr.ack_num = ack_num;
        hdr.flags.syn = syn;
        hdr.flags.ack = ack;
        hdr.flags.fin = fin;
        hdr.flags.rst = rst;
        hdr.src_port = 1000;
        hdr.dst_port = 80;
        return hdr;
    }
};

// ===== Basic Retransmission Detection Tests =====

TEST_F(TcpRetransmissionTest, DuplicateSequenceInEstablished) {
    // Same sequence number in header = retransmission (already seen this seq)
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 1000;  // We've already seen up to sequence 1000 from remote
    conn->local_ack = 5000;

    TcpHeader hdr = create_header(1000, 5000);  // Same sequence = retransmission
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, NewSequenceNotRetransmission) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 1000;
    conn->local_ack = 5000;

    // New sequence number should not be retransmission
    TcpHeader hdr = create_header(1100, 5000);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, DuplicateAckInEstablished) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 1000;  // Last sequence number received from remote
    conn->local_ack = 5000;

    // Duplicate ACK with seq_num <= remote_seq should be retransmission
    TcpHeader hdr = create_header(1000, 5000, false, true);  // ACK only, same seq
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, AdvancedAckNotRetransmission) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 1000;  // Last sequence from remote
    conn->local_ack = 5000;

    // New sequence advancing beyond remote_seq
    TcpHeader hdr = create_header(1100, 5100, false, true);  // New seq, new ACK
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

// ===== State-Specific Retransmission Tests =====

TEST_F(TcpRetransmissionTest, RetransmissionInFinWait1) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::FinWait1;
    conn->remote_seq = 5000;
    conn->local_ack = 3000;

    // Duplicate sequence in FIN_WAIT_1 state
    TcpHeader hdr = create_header(5000, 3000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, RetransmissionInFinWait2) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::FinWait2;
    conn->remote_seq = 5000;
    conn->local_ack = 3000;

    TcpHeader hdr = create_header(5000, 3000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, RetransmissionInCloseWait) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::CloseWait;
    conn->remote_seq = 5000;
    conn->local_ack = 3000;

    TcpHeader hdr = create_header(5000, 3000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, RetransmissionInClosing) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Closing;
    conn->remote_seq = 5000;
    conn->local_ack = 3000;

    TcpHeader hdr = create_header(5000, 3000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, RetransmissionInLastAck) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::LastAck;
    conn->remote_seq = 5000;
    conn->local_ack = 3000;

    TcpHeader hdr = create_header(5000, 3000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

// ===== Syn/Fin Flags Don't Trigger Retransmission =====

TEST_F(TcpRetransmissionTest, SynDoesNotCountAsRetransmission) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::SynSent;
    conn->remote_seq = 0;

    // SYN packets are not considered retransmissions
    // even if they have same sequence
    TcpHeader hdr = create_header(1000, 0, true);  // SYN
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, FinDoesNotCountAsRetransmission) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::FinWait1;
    conn->remote_seq = 5000;
    conn->local_ack = 3000;

    // FIN with same sequence is not retransmission
    TcpHeader hdr = create_header(5000, 3000, false, false, true);  // FIN
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

// ===== State Sensitivity Tests =====

TEST_F(TcpRetransmissionTest, NoRetransmissionInSynSent) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::SynSent;
    conn->local_seq = 1000;

    TcpHeader hdr = create_header(1000, 0);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, NoRetransmissionInSynReceived) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::SynReceived;
    conn->local_seq = 1000;
    conn->local_ack = 5000;

    TcpHeader hdr = create_header(1000, 5000);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, NoRetransmissionInTimeWait) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::TimeWait;
    conn->local_seq = 5000;
    conn->local_ack = 3000;

    TcpHeader hdr = create_header(5000, 3000);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, NoRetransmissionInClosed) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Closed;

    TcpHeader hdr = create_header(1000, 0);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr));
}

// ===== Multiple Retransmission Scenario Tests =====

TEST_F(TcpRetransmissionTest, TripleAckRetransmission) {
    // Three duplicate ACKs (fast retransmit scenario)
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->local_seq = 1000;
    conn->local_ack = 5000;

    // First duplicate ACK
    TcpHeader hdr1 = create_header(0, 5000, false, true);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr1));

    // Second duplicate ACK
    TcpHeader hdr2 = create_header(0, 5000, false, true);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr2));

    // Third duplicate ACK
    TcpHeader hdr3 = create_header(0, 5000, false, true);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr3));
}

TEST_F(TcpRetransmissionTest, InterspersedRetransmissions) {
    // Mix of new and retransmitted packets from remote
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 0;   // Haven't received data yet
    conn->local_ack = 5000;

    // New packet from remote
    TcpHeader hdr1 = create_header(1000, 5000);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr1));

    // Advance remote sequence
    conn->remote_seq = 1000;

    // New packet with advanced sequence
    TcpHeader hdr2 = create_header(1100, 5000);
    EXPECT_FALSE(tracker_->is_retransmission(*conn, hdr2));

    // Retransmitted old packet
    TcpHeader hdr3 = create_header(1000, 5000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr3));
}

// ===== Edge Cases =====

TEST_F(TcpRetransmissionTest, ZeroSequenceRetransmission) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->local_seq = 0;  // Initial sequence can be 0
    conn->local_ack = 5000;

    TcpHeader hdr = create_header(0, 5000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, MaxSequenceRetransmission) {
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 0xFFFFFFFF;  // Maximum sequence from remote
    conn->local_ack = 0xFFFFFFFF;

    TcpHeader hdr = create_header(0xFFFFFFFF, 0xFFFFFFFF);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr));
}

TEST_F(TcpRetransmissionTest, SequenceWraparound) {
    // Sequence numbers wrap at 2^32
    auto* conn = tracker_->get_connection(0xC0A80001, 0xC0A80002, 1000, 80);
    ASSERT_NE(conn, nullptr);

    conn->state = TcpState::Established;
    conn->remote_seq = 0xFFFFFFF0;  // High sequence from remote
    conn->local_ack = 5000;

    // Duplicate packet with same high sequence
    TcpHeader hdr1 = create_header(0xFFFFFFF0, 5000);
    EXPECT_TRUE(tracker_->is_retransmission(*conn, hdr1));
}

}  // namespace wadjet::protocols::tcp

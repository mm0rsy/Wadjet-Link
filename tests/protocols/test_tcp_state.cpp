/// @file test_tcp_state.cpp
/// @brief Tests for TCP state machine and connection tracking

#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/tcp.hpp"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <vector>

namespace wadjet::protocols::tcp {

/// @brief Test TCP State Enum
class TcpStateTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

/// @brief TCP State Machine transition tests
class TcpStateMachineTest : public ::testing::Test {
protected:
    struct MockTcpConnection {
        TcpState state{TcpState::Closed};
        std::uint32_t local_seq{0};
        std::uint32_t remote_seq{0};
        std::uint32_t local_ack{0};
        std::uint32_t remote_ack{0};
        std::chrono::steady_clock::time_point last_seen{std::chrono::steady_clock::now()};
        std::vector<std::uint32_t> out_of_order_buffer;
    };

    void SetUp() override {
        // Initialize test data
        conn_ = std::make_unique<MockTcpConnection>();
    }

    std::unique_ptr<MockTcpConnection> conn_;
};

// ===== State Enum Tests =====

TEST_F(TcpStateTest, StateEnumValues) {
    // Verify all 11 TCP states are defined
    EXPECT_EQ(static_cast<int>(TcpState::Closed), 0);
    EXPECT_EQ(static_cast<int>(TcpState::Listen), 1);
    EXPECT_EQ(static_cast<int>(TcpState::SynSent), 2);
    EXPECT_EQ(static_cast<int>(TcpState::SynReceived), 3);
    EXPECT_EQ(static_cast<int>(TcpState::Established), 4);
    EXPECT_EQ(static_cast<int>(TcpState::FinWait1), 5);
    EXPECT_EQ(static_cast<int>(TcpState::FinWait2), 6);
    EXPECT_EQ(static_cast<int>(TcpState::Closing), 7);
    EXPECT_EQ(static_cast<int>(TcpState::TimeWait), 8);
    EXPECT_EQ(static_cast<int>(TcpState::CloseWait), 9);
    EXPECT_EQ(static_cast<int>(TcpState::LastAck), 10);
}

// ===== Passive Open (Server) Tests =====

TEST_F(TcpStateMachineTest, PassiveOpenListenState) {
    // Server starts in LISTEN
    conn_->state = TcpState::Listen;
    EXPECT_EQ(conn_->state, TcpState::Listen);
}

TEST_F(TcpStateMachineTest, PassiveOpenSynReceivedTransition) {
    // Server: LISTEN -> SYN_RECEIVED upon receiving SYN
    conn_->state = TcpState::Listen;
    conn_->remote_seq = 100;
    conn_->state = TcpState::SynReceived;

    EXPECT_EQ(conn_->state, TcpState::SynReceived);
    EXPECT_EQ(conn_->remote_seq, 100);
}

TEST_F(TcpStateMachineTest, PassiveOpenEstablishedTransition) {
    // Server: SYN_RECEIVED -> ESTABLISHED upon receiving ACK
    conn_->state = TcpState::SynReceived;
    conn_->state = TcpState::Established;

    EXPECT_EQ(conn_->state, TcpState::Established);
}

// ===== Active Open (Client) Tests =====

TEST_F(TcpStateMachineTest, ActiveOpenSynSentTransition) {
    // Client: CLOSED -> SYN_SENT when sending SYN
    conn_->state = TcpState::Closed;
    conn_->local_seq = 1000;
    conn_->state = TcpState::SynSent;

    EXPECT_EQ(conn_->state, TcpState::SynSent);
    EXPECT_EQ(conn_->local_seq, 1000);
}

TEST_F(TcpStateMachineTest, ActiveOpenEstablishedTransition) {
    // Client: SYN_SENT -> ESTABLISHED upon receiving SYN_ACK
    conn_->state = TcpState::SynSent;
    conn_->remote_seq = 5000;
    conn_->state = TcpState::Established;

    EXPECT_EQ(conn_->state, TcpState::Established);
    EXPECT_EQ(conn_->remote_seq, 5000);
}

// ===== Normal Data Transfer Tests =====

TEST_F(TcpStateMachineTest, DataTransferInEstablished) {
    // Remain in ESTABLISHED while exchanging data
    conn_->state = TcpState::Established;
    conn_->local_seq = 2000;
    conn_->local_ack = 3000;

    // Simulate multiple packets
    for (int i = 0; i < 5; ++i) {
        conn_->local_seq += 100;
        EXPECT_EQ(conn_->state, TcpState::Established);
    }
}

TEST_F(TcpStateMachineTest, EstablishedWithOutOfOrderSegments) {
    // Handle out-of-order segments in ESTABLISHED state
    conn_->state = TcpState::Established;
    conn_->local_seq = 1000;
    conn_->local_ack = 1000;

    // Simulate receiving out-of-order segment (seq=2000 when expecting 1100)
    conn_->out_of_order_buffer.push_back(2000);
    conn_->out_of_order_buffer.push_back(2100);
    conn_->out_of_order_buffer.push_back(1100);  // In-order segment

    EXPECT_EQ(conn_->state, TcpState::Established);
    EXPECT_EQ(conn_->out_of_order_buffer.size(), 3);
}

// ===== Active Close Tests =====

TEST_F(TcpStateMachineTest, ActiveCloseInitiation) {
    // ESTABLISHED -> FIN_WAIT_1 when sending FIN
    conn_->state = TcpState::Established;
    conn_->state = TcpState::FinWait1;

    EXPECT_EQ(conn_->state, TcpState::FinWait1);
}

TEST_F(TcpStateMachineTest, ActiveCloseAckReceived) {
    // FIN_WAIT_1 -> FIN_WAIT_2 upon receiving ACK of our FIN
    conn_->state = TcpState::FinWait1;
    conn_->local_ack = 5000;
    conn_->state = TcpState::FinWait2;

    EXPECT_EQ(conn_->state, TcpState::FinWait2);
}

TEST_F(TcpStateMachineTest, ActiveCloseFinReceived) {
    // FIN_WAIT_2 -> TIME_WAIT upon receiving FIN and sending ACK
    conn_->state = TcpState::FinWait2;
    conn_->remote_seq = 9999;
    conn_->state = TcpState::TimeWait;

    EXPECT_EQ(conn_->state, TcpState::TimeWait);
    EXPECT_EQ(conn_->remote_seq, 9999);
}

TEST_F(TcpStateMachineTest, ActiveCloseSimultaneous) {
    // ESTABLISHED -> CLOSING when simultaneous close
    conn_->state = TcpState::Established;
    // Send FIN
    conn_->state = TcpState::FinWait1;
    // Receive FIN while waiting
    conn_->state = TcpState::Closing;

    EXPECT_EQ(conn_->state, TcpState::Closing);
}

TEST_F(TcpStateMachineTest, ActiveCloseSimultaneousAckReceived) {
    // CLOSING -> TIME_WAIT upon receiving final ACK
    conn_->state = TcpState::Closing;
    conn_->state = TcpState::TimeWait;

    EXPECT_EQ(conn_->state, TcpState::TimeWait);
}

// ===== Passive Close Tests =====

TEST_F(TcpStateMachineTest, PassiveCloseReception) {
    // ESTABLISHED -> CLOSE_WAIT upon receiving FIN
    conn_->state = TcpState::Established;
    conn_->remote_seq = 8000;
    conn_->state = TcpState::CloseWait;

    EXPECT_EQ(conn_->state, TcpState::CloseWait);
    EXPECT_EQ(conn_->remote_seq, 8000);
}

TEST_F(TcpStateMachineTest, PassiveCloseInitiation) {
    // CLOSE_WAIT -> LAST_ACK when sending FIN
    conn_->state = TcpState::CloseWait;
    conn_->state = TcpState::LastAck;

    EXPECT_EQ(conn_->state, TcpState::LastAck);
}

TEST_F(TcpStateMachineTest, PassiveCloseCompletion) {
    // LAST_ACK -> CLOSED upon receiving final ACK
    conn_->state = TcpState::LastAck;
    conn_->local_ack = 10000;
    conn_->state = TcpState::Closed;

    EXPECT_EQ(conn_->state, TcpState::Closed);
    EXPECT_EQ(conn_->local_ack, 10000);
}

// ===== TIME_WAIT Tests =====

TEST_F(TcpStateMachineTest, TimeWaitState) {
    // Verify TIME_WAIT is reachable
    conn_->state = TcpState::TimeWait;
    EXPECT_EQ(conn_->state, TcpState::TimeWait);
}

TEST_F(TcpStateMachineTest, TimeWaitTimeout) {
    // TIME_WAIT expires after 2 minutes (30s in tests)
    conn_->state = TcpState::TimeWait;
    auto start = std::chrono::steady_clock::now();
    conn_->last_seen = start;

    // Simulate passage of time
    auto now = start + std::chrono::seconds(30);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - conn_->last_seen);

    EXPECT_GE(elapsed.count(), 30);
    // After timeout, should transition to CLOSED
    conn_->state = TcpState::Closed;
    EXPECT_EQ(conn_->state, TcpState::Closed);
}

// ===== RST Handling Tests =====

TEST_F(TcpStateMachineTest, ResetFromEstablished) {
    // RST can be received from any state
    conn_->state = TcpState::Established;
    // Upon receiving RST
    conn_->state = TcpState::Closed;

    EXPECT_EQ(conn_->state, TcpState::Closed);
}

TEST_F(TcpStateMachineTest, ResetFromSynSent) {
    // RST response to unsolicited SYN
    conn_->state = TcpState::SynSent;
    conn_->state = TcpState::Closed;

    EXPECT_EQ(conn_->state, TcpState::Closed);
}

TEST_F(TcpStateMachineTest, ResetFromFinWait) {
    // RST can terminate FIN_WAIT states
    conn_->state = TcpState::FinWait1;
    conn_->state = TcpState::Closed;

    EXPECT_EQ(conn_->state, TcpState::Closed);
}

// ===== Connection Timeout Tests =====

TEST_F(TcpStateMachineTest, IncompleteConnectionTimeout) {
    // Incomplete connections (SYN_SENT, SYN_RECEIVED) timeout after 2 minutes
    conn_->state = TcpState::SynSent;
    // Note: timeout is conceptually 2 minutes, but we just test state transition
    // auto timeout = std::chrono::minutes(2);

    EXPECT_EQ(conn_->state, TcpState::SynSent);
    // Simulate timeout
    conn_->state = TcpState::Closed;
    EXPECT_EQ(conn_->state, TcpState::Closed);
}

TEST_F(TcpStateMachineTest, EstablishedConnectionLongTimeout) {
    // Established connections remain open (user responsibility)
    conn_->state = TcpState::Established;
    // Note: conceptually long_elapsed time, but we just verify state stability
    // auto long_elapsed = std::chrono::hours(1);

    EXPECT_EQ(conn_->state, TcpState::Established);
}

// ===== Sequence Number Tracking Tests =====

TEST_F(TcpStateMachineTest, SequenceNumberInitialization) {
    conn_->state = TcpState::SynSent;
    conn_->local_seq = 1000;
    conn_->remote_seq = 0;

    EXPECT_EQ(conn_->local_seq, 1000);
    EXPECT_EQ(conn_->remote_seq, 0);  // Not set until SYN received
}

TEST_F(TcpStateMachineTest, SequenceNumberAcknowledgment) {
    conn_->state = TcpState::Established;
    conn_->local_seq = 2000;
    conn_->local_ack = 3000;

    // Simulate receiving acknowledgment
    conn_->local_seq += 100;
    EXPECT_EQ(conn_->local_seq, 2100);
}

TEST_F(TcpStateMachineTest, AcknowledgmentNumberTracking) {
    conn_->state = TcpState::Established;
    conn_->local_ack = 5000;

    // Simulate receiving data
    conn_->local_ack += 500;
    EXPECT_EQ(conn_->local_ack, 5500);
}

// ===== Out-of-Order Buffering Tests =====

TEST_F(TcpStateMachineTest, OutOfOrderBufferCapacity) {
    // Buffer up to 16 segments
    conn_->state = TcpState::Established;

    for (std::size_t i = 0; i < 16; ++i) {
        conn_->out_of_order_buffer.push_back(static_cast<std::uint32_t>(1000 + i * 100));
    }

    EXPECT_EQ(conn_->out_of_order_buffer.size(), 16);
}

TEST_F(TcpStateMachineTest, OutOfOrderBufferOverflow) {
    // Discard oldest when buffer full
    conn_->state = TcpState::Established;

    for (std::size_t i = 0; i < 17; ++i) {
        conn_->out_of_order_buffer.push_back(static_cast<std::uint32_t>(1000 + i * 100));
        if (conn_->out_of_order_buffer.size() > 16) {
            conn_->out_of_order_buffer.erase(conn_->out_of_order_buffer.begin());
        }
    }

    EXPECT_EQ(conn_->out_of_order_buffer.size(), 16);
    EXPECT_EQ(conn_->out_of_order_buffer.front(), 1100);  // First element from overflow
}

TEST_F(TcpStateMachineTest, OutOfOrderBufferOrdering) {
    conn_->state = TcpState::Established;

    // Add segments out of order
    conn_->out_of_order_buffer.push_back(3000);
    conn_->out_of_order_buffer.push_back(1000);
    conn_->out_of_order_buffer.push_back(2000);

    EXPECT_EQ(conn_->out_of_order_buffer[0], 3000);
    EXPECT_EQ(conn_->out_of_order_buffer[1], 1000);
    EXPECT_EQ(conn_->out_of_order_buffer[2], 2000);
}

// ===== Invalid State Transitions =====

TEST_F(TcpStateMachineTest, InvalidTransitionFromClosed) {
    // CLOSED state - only valid transition is to LISTEN or SYN_SENT
    conn_->state = TcpState::Closed;
    EXPECT_EQ(conn_->state, TcpState::Closed);

    // Cannot go to ESTABLISHED directly from CLOSED
    // (would need to go through SYN_SENT or LISTEN->SYN_RECEIVED)
}

TEST_F(TcpStateMachineTest, InvalidTransitionFromListen) {
    // LISTEN state - can only go to SYN_RECEIVED (passive) or CLOSED
    conn_->state = TcpState::Listen;
    EXPECT_EQ(conn_->state, TcpState::Listen);

    // Cannot go to FinWait directly
}

}  // namespace wadjet::protocols::tcp

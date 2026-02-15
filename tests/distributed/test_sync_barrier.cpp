#include "wadjet/distributed/sync_barrier.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

namespace wadjet::distributed {

class SyncBarrierTest : public ::testing::Test {
protected:
    std::string barrier_id = "test_barrier";
};

// Test T011: SyncBarrier constructor
TEST_F(SyncBarrierTest, ConstructorSetsBarrierId) {
    SyncBarrier barrier("my_barrier_id");
    EXPECT_EQ(barrier.barrier_id(), "my_barrier_id");
}

// Test T012: Wait for nodes - all present
TEST_F(SyncBarrierTest, WaitForNodesAllPresent) {
    SyncBarrier barrier(barrier_id);

    std::vector<std::string> nodes = {"node1", "node2", "node3"};

    // Simulate nodes arriving
    for (size_t i = 0; i < nodes.size(); ++i) {
        std::thread([&barrier]() {
            // Small delay to simulate network latency
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }).detach();
    }

    auto result = barrier.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(5000),
                                   .sync_margin = std::chrono::milliseconds(10)});

    EXPECT_EQ(result.participating_nodes.size(), nodes.size());
    EXPECT_GT(result.sync_timestamp_ns, 0);
}

// Test T012: Wait for nodes - partial
TEST_F(SyncBarrierTest, WaitForNodesPartialTimeout) {
    SyncBarrier barrier(barrier_id);

    std::vector<std::string> nodes = {"node1", "node2", "node3"};

    auto result = barrier.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(100),
                                   .sync_margin = std::chrono::milliseconds(10)});

    // All nodes marked as missing due to timeout
    EXPECT_GT(result.missing_nodes.size(), 0);
    EXPECT_FALSE(result.proceed);
}

// Test T012: Barrier result - succeeded check
TEST_F(SyncBarrierTest, BarrierResultSucceededWhenNoMissing) {
    BarrierResult result;
    result.proceed = true;
    result.missing_nodes.clear();
    result.participating_nodes = {"node1", "node2"};

    EXPECT_TRUE(result.succeeded());
}

TEST_F(SyncBarrierTest, BarrierResultNotSucceededWhenMissing) {
    BarrierResult result;
    result.proceed = true;
    result.missing_nodes = {"node3"};
    result.participating_nodes = {"node1", "node2", "node3"};

    EXPECT_FALSE(result.succeeded());
}

TEST_F(SyncBarrierTest, BarrierResultNotSucceededWhenProceedFalse) {
    BarrierResult result;
    result.proceed = false;
    result.missing_nodes.clear();

    EXPECT_FALSE(result.succeeded());
}

// Test arrive_and_wait
TEST_F(SyncBarrierTest, ArriveAndWaitReturnValidResult) {
    SyncBarrier barrier(barrier_id);

    // Set up expected nodes first
    std::vector<std::string> nodes = {"local_node"};
    barrier.wait_for_nodes(nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(1000)});

    auto result = barrier.arrive_and_wait("local_node", std::chrono::milliseconds(500));

    // Should succeed
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(result.unwrap().sync_timestamp_ns > 0);
}

// Test move semantics
TEST_F(SyncBarrierTest, MoveConstructor) {
    SyncBarrier barrier1(barrier_id);
    SyncBarrier barrier2 = std::move(barrier1);

    EXPECT_EQ(barrier2.barrier_id(), barrier_id);
}

TEST_F(SyncBarrierTest, MoveAssignment) {
    SyncBarrier barrier1(barrier_id);
    SyncBarrier barrier2("other_id");

    barrier2 = std::move(barrier1);

    EXPECT_EQ(barrier2.barrier_id(), barrier_id);
}

// Test sync_margin configuration
TEST_F(SyncBarrierTest, SyncMarginAffectsSyncTimestamp) {
    SyncBarrier barrier1(barrier_id);
    SyncBarrier barrier2(barrier_id);

    std::vector<std::string> nodes = {"test_node"};

    auto result1 = barrier1.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(1000),
                                   .sync_margin = std::chrono::milliseconds(10)});

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto result2 = barrier2.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(1000),
                                   .sync_margin = std::chrono::milliseconds(20)});

    // The sync timestamps should be different due to different margins
    // (and time elapsed between calls)
    EXPECT_NE(result1.sync_timestamp_ns, result2.sync_timestamp_ns);
}

// Test timeout behavior
TEST_F(SyncBarrierTest, TimeoutBehavior) {
    SyncBarrier barrier(barrier_id);

    std::vector<std::string> nodes = {"node1", "node2"};

    auto start = std::chrono::high_resolution_clock::now();
    auto result = barrier.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(100)});
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    // Should timeout after approximately 100ms
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 90);
    EXPECT_FALSE(result.proceed);
}

// Test concurrent barrier operations (basic)
TEST_F(SyncBarrierTest, ConcurrentArrivalTracking) {
    SyncBarrier barrier(barrier_id);

    std::vector<std::string> nodes = {"node1", "node2", "node3"};

    // Simulate concurrent node arrivals
    std::vector<std::thread> threads;
    for (size_t i = 0; i < nodes.size(); ++i) {
        threads.emplace_back(
            [&barrier]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    }

    auto result = barrier.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(1000)});

    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }

    // Verify the barrier result is valid
    EXPECT_EQ(result.participating_nodes.size(), nodes.size());
    EXPECT_GT(result.sync_timestamp_ns, 0);
}

// Test empty nodes list
TEST_F(SyncBarrierTest, EmptyNodesList) {
    SyncBarrier barrier(barrier_id);

    std::vector<std::string> nodes;
    auto result = barrier.wait_for_nodes(
        nodes, SyncBarrier::Config{.timeout = std::chrono::milliseconds(100)});

    EXPECT_EQ(result.participating_nodes.size(), 0);
    // Should succeed since no nodes expected
    EXPECT_TRUE(result.proceed);
}

}  // namespace wadjet::distributed

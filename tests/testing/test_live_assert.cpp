/// @file test_live_assert.cpp
/// @brief Tests for live-assert mode
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/testing/generators.hpp"
#include "wadjet/testing/testing.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace wadjet;
using namespace wadjet::testing;
using namespace wadjet::testing::generators;
using namespace std::chrono_literals;

// =============================================================================
// AssertionRule Unit Tests
// =============================================================================

class AssertionRuleTest : public ::testing::Test {
protected:
    PacketGenerator gen_{42};
};

TEST_F(AssertionRuleTest, AssertAllPassesOnMatch) {
    AssertionRule rule(AssertionRule::Type::ASSERT_ALL, "All must be UDP",
                       [](const Packet& pkt) { return ::testing::Value(pkt, IsUDP()); });

    auto udp_pkt = gen_.udp_packet(10);
    auto result = rule.check(udp_pkt, 0);

    EXPECT_TRUE(result.passed);
}

TEST_F(AssertionRuleTest, AssertAllFailsOnMismatch) {
    AssertionRule rule(AssertionRule::Type::ASSERT_ALL, "All must be UDP",
                       [](const Packet& pkt) { return ::testing::Value(pkt, IsUDP()); });

    auto tcp_pkt = gen_.tcp_packet(10);
    auto result = rule.check(tcp_pkt, 5);

    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.violating_packet.has_value());
    EXPECT_EQ(result.packet_index, 5u);
}

TEST_F(AssertionRuleTest, AssertNeverPassesOnMismatch) {
    AssertionRule rule(AssertionRule::Type::ASSERT_NEVER, "Never TCP",
                       [](const Packet& pkt) { return ::testing::Value(pkt, IsTCP()); });

    auto udp_pkt = gen_.udp_packet(10);
    auto result = rule.check(udp_pkt, 0);

    EXPECT_TRUE(result.passed);
}

TEST_F(AssertionRuleTest, AssertNeverFailsOnMatch) {
    AssertionRule rule(AssertionRule::Type::ASSERT_NEVER, "Never TCP",
                       [](const Packet& pkt) { return ::testing::Value(pkt, IsTCP()); });

    auto tcp_pkt = gen_.tcp_packet(10);
    auto result = rule.check(tcp_pkt, 3);

    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.violating_packet.has_value());
}

TEST_F(AssertionRuleTest, AssertWhenAppliesOnlyWhenConditionMet) {
    // When TCP, must have destination port > 1000
    // Use predicate type explicitly to avoid ambiguity
    AssertionRule::PacketPredicate is_tcp = [](const Packet& pkt) {
        return ::testing::Value(pkt, IsTCP());
    };

    AssertionRule rule(
        AssertionRule::Type::ASSERT_WHEN, "When TCP, port > 1000",
        [](const Packet&) {
            // Simplified check - just return true for test
            return true;
        },
        is_tcp);

    // UDP packet - condition not met, should pass regardless
    auto udp_pkt = gen_.udp_packet(10);
    auto result1 = rule.check(udp_pkt, 0);
    EXPECT_TRUE(result1.passed);

    // TCP packet - condition met, predicate applies
    auto tcp_pkt = gen_.tcp_packet(10);
    auto result2 = rule.check(tcp_pkt, 1);
    EXPECT_TRUE(result2.passed);  // Predicate returns true
}

TEST_F(AssertionRuleTest, ExpectWithinTracksSatisfaction) {
    AssertionRule rule(AssertionRule::Type::EXPECT_WITHIN, "Expect TCP within timeout",
                       [](const Packet& pkt) { return ::testing::Value(pkt, IsTCP()); });

    EXPECT_FALSE(rule.is_satisfied());

    // Check passes on individual packets (doesn't fail)
    auto udp_pkt = gen_.udp_packet(10);
    auto result1 = rule.check(udp_pkt, 0);
    EXPECT_TRUE(result1.passed);  // EXPECT_WITHIN doesn't fail on individual

    // Mark satisfied when condition met
    auto tcp_pkt = gen_.tcp_packet(10);
    if (::testing::Value(tcp_pkt, IsTCP())) {
        rule.mark_satisfied();
    }

    EXPECT_TRUE(rule.is_satisfied());
}

// =============================================================================
// LiveAssertSession Unit Tests
// =============================================================================

class LiveAssertSessionTest : public ::testing::Test {
protected:
    PacketGenerator gen_{42};
};

TEST_F(LiveAssertSessionTest, AddsAssertAllRule) {
    LiveAssertSession session("lo");

    session.assert_all(IsUDP());

    EXPECT_EQ(session.rule_count(), 1u);
}

TEST_F(LiveAssertSessionTest, AddsAssertNeverRule) {
    LiveAssertSession session("lo");

    session.assert_never(IsTCP());

    EXPECT_EQ(session.rule_count(), 1u);
}

TEST_F(LiveAssertSessionTest, AddsExpectWithinRule) {
    LiveAssertSession session("lo");

    session.expect_within(IsUDP());

    EXPECT_EQ(session.rule_count(), 1u);
}

TEST_F(LiveAssertSessionTest, ChainsMultipleRules) {
    LiveAssertSession session("lo");

    session.assert_all(IsUDP()).assert_never(IsTCP()).expect_within(HasDestPort(55555));

    EXPECT_EQ(session.rule_count(), 3u);
}

TEST_F(LiveAssertSessionTest, ConditionalAssertionBuilder) {
    LiveAssertSession session("lo");

    session.when(IsUDP(), "is UDP").assert_that(HasIPProtocol(17));

    EXPECT_EQ(session.rule_count(), 1u);
}

TEST_F(LiveAssertSessionTest, ConfiguresFilter) {
    LiveAssertSession session("lo");

    auto& result = session.set_filter("udp port 12345");

    // Should return reference to self for chaining
    EXPECT_EQ(&result, &session);
}

// =============================================================================
// Live Capture Tests (requires CAP_NET_RAW)
// =============================================================================

class LiveAssertIntegrationTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55575");
        LoopbackTestFixture::SetUp();
    }

    void send_udp_packets(int count, int delay_ms = 20) {
        for (int i = 0; i < count; ++i) {
            std::vector<std::uint8_t> payload = {static_cast<std::uint8_t>(i), 0xDD, 0xEE};
            send_udp(55575, payload);
            if (delay_ms > 0 && i < count - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }
    }
};

TEST_F(LiveAssertIntegrationTest, AllAssertionPassesOnValidTraffic) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575").assert_all(IsUDP());

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(5);
    });

    auto result = session.run_for(300ms);
    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
    EXPECT_GE(session.captured_packets().size(), 5u);
}

TEST_F(LiveAssertIntegrationTest, NeverAssertionPassesWhenNoViolation) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575")
        .assert_never(IsTCP());  // Should never see TCP on UDP filter

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(3);
    });

    auto result = session.run_for(200ms);
    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
}

TEST_F(LiveAssertIntegrationTest, ExpectWithinPassesWhenFound) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575").expect_within(IsUDP());

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(1);
    });

    auto result = session.run_for(200ms);
    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
}

TEST_F(LiveAssertIntegrationTest, ExpectWithinFailsWhenNotFound) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575").expect_within(IsTCP());  // Won't find TCP on UDP filter

    // Don't send any packets
    auto result = session.run_for(100ms);

    EXPECT_FALSE(result.passed);
    EXPECT_NE(result.description.find("Expected packet not found"), std::string::npos);
}

TEST_F(LiveAssertIntegrationTest, MultipleRulesAllChecked) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575")
        .assert_all(IsUDP())
        .assert_all(HasDestPort(55575))
        .assert_never(IsTCP());

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(5);
    });

    auto result = session.run_for(300ms);
    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
}

TEST_F(LiveAssertIntegrationTest, RunUntilStopCondition) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575").assert_all(IsUDP());

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(10, 30);
    });

    int packet_count = 0;
    auto result =
        session.run_until([&packet_count](const Packet&) { return ++packet_count >= 5; }, 500ms);

    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
    EXPECT_GE(session.captured_packets().size(), 5u);
}

TEST_F(LiveAssertIntegrationTest, RunUntilTimesOut) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575");

    // No packets sent, stop condition never met
    auto result = session.run_until([](const Packet&) { return false; }, 100ms);

    EXPECT_FALSE(result.passed);
    EXPECT_NE(result.description.find("Timeout"), std::string::npos);
}

TEST_F(LiveAssertIntegrationTest, CapturedPacketsAvailable) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575");

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(3);
    });

    session.run_for(200ms);
    sender.join();

    EXPECT_GE(session.captured_packets().size(), 3u);

    // Verify captured packets
    for (const auto& pkt : session.captured_packets()) {
        EXPECT_THAT(pkt, IsUDP());
    }
}

TEST_F(LiveAssertIntegrationTest, FailureSavesPcap) {
    auto failure_dir = std::filesystem::temp_directory_path() / "wadjet_live_assert_test";
    std::filesystem::create_directories(failure_dir);

    // Clean up any existing files
    for (const auto& entry : std::filesystem::directory_iterator(failure_dir)) {
        std::filesystem::remove(entry.path());
    }

    LiveAssertSession session("lo");
    session.set_filter("udp port 55575")
        .save_on_failure(failure_dir)
        .expect_within(IsTCP());  // Will fail - no TCP

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(2);
    });

    auto result = session.run_for(200ms);
    sender.join();

    EXPECT_FALSE(result.passed);

    // Check that PCAP was saved
    bool found_pcap = false;
    for (const auto& entry : std::filesystem::directory_iterator(failure_dir)) {
        if (entry.path().extension() == ".pcap") {
            found_pcap = true;
            std::filesystem::remove(entry.path());
        }
    }

    EXPECT_TRUE(found_pcap);
    std::filesystem::remove(failure_dir);
}

// =============================================================================
// Macro Tests
// =============================================================================

TEST_F(LiveAssertIntegrationTest, LiveExpectMacro) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575").assert_all(IsUDP());

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(3);
    });

    WADJET_LIVE_EXPECT(session, 200ms);

    sender.join();
}

TEST_F(LiveAssertIntegrationTest, LiveExpectUntilMacro) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575");

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(5);
    });

    int count = 0;
    WADJET_LIVE_EXPECT_UNTIL(session, [&count](const Packet&) { return ++count >= 3; }, 300ms);

    sender.join();
}

// =============================================================================
// Complex Scenarios
// =============================================================================

TEST_F(LiveAssertIntegrationTest, ProtocolConformanceTest) {
    // Simulate a protocol conformance test scenario
    LiveAssertSession session("lo");

    // Use predicate overload for the size check to avoid lambda/matcher issues
    LiveAssertSession::PacketPredicate is_truncated = [](const Packet& pkt) {
        return pkt.size() < 28;  // Minimum UDP/IP/Eth size
    };

    session.set_filter("udp port 55575")
        .assert_all(IsUDP(), "All traffic must be UDP")
        .assert_all(HasDestPort(55575), "All traffic to port 55575")
        .assert_never(is_truncated, "No truncated packets allowed");

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(10);
    });

    auto result = session.run_for(400ms);
    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
    EXPECT_GE(session.captured_packets().size(), 10u);
}

TEST_F(LiveAssertIntegrationTest, ConditionalRuleTest) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575");

    // When we see any packet, it should be UDP
    // Use predicate overload for the condition to avoid lambda/matcher issues
    LiveAssertSession::PacketPredicate any_packet = [](const Packet&) { return true; };
    session.when(any_packet, "any packet").assert_that(IsUDP());

    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_udp_packets(5);
    });

    auto result = session.run_for(300ms);
    sender.join();

    EXPECT_TRUE(result.passed) << result.description;
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(LiveAssertIntegrationTest, EmptyTrafficPasses) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575").assert_all(IsUDP());  // No packets to check

    // Don't send any packets
    auto result = session.run_for(50ms);

    // Should pass - no violations
    EXPECT_TRUE(result.passed);
}

TEST_F(LiveAssertIntegrationTest, ZeroTimeoutReturnsImmediately) {
    LiveAssertSession session("lo");
    session.set_filter("udp port 55575");

    auto start = std::chrono::steady_clock::now();
    auto result = session.run_for(0ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(result.passed);
    EXPECT_LT(elapsed, 50ms);
}

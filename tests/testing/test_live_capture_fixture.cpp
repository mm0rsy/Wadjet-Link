/// @file test_live_capture_fixture.cpp
/// @brief Integration tests for LiveCaptureTestFixture
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// These tests verify the live capture fixture functionality including
/// time-bounded expectations, pattern matching, and failure PCAP storage.
///
/// @note These tests require CAP_NET_RAW capability or root privileges.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "wadjet/testing/testing.hpp"

using namespace wadjet;
using namespace wadjet::testing;
using namespace std::chrono_literals;

// =============================================================================
// Basic Fixture Tests
// =============================================================================

/// Test that the fixture correctly sets up on loopback
class BasicLoopbackTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp");
        LoopbackTestFixture::SetUp();
    }
};

TEST_F(BasicLoopbackTest, FixtureSetupSucceeds) {
    ASSERT_NE(session(), nullptr);
}

TEST_F(BasicLoopbackTest, SessionIsRunning) {
    ASSERT_NE(session(), nullptr);
    EXPECT_TRUE(session()->is_running());
}

// =============================================================================
// Time-Bounded Expectation Tests
// =============================================================================

class TimeBoundedTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55555");
        LoopbackTestFixture::SetUp();
    }
    
    // Helper to send test UDP packet
    void send_test_packet() {
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
        ASSERT_TRUE(send_udp(55555, payload));
    }
};

TEST_F(TimeBoundedTest, WaitForPacket_ReceivesWithinTimeout) {
    // Send packet in background thread after short delay
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    // Wait for packet with sufficient timeout
    auto packet = wait_for_packet([](const Packet&) { return true; }, 500ms);
    
    sender.join();
    
    ASSERT_TRUE(packet.has_value());
    EXPECT_GT(packet->size(), 0);
}

TEST_F(TimeBoundedTest, WaitForPacket_TimesOutWhenNoPacket) {
    // Don't send any packet
    auto packet = wait_for_packet([](const Packet&) { return true; }, 100ms);
    
    EXPECT_FALSE(packet.has_value());
}

TEST_F(TimeBoundedTest, WaitForPacket_IgnoresNonMatchingPackets) {
    // Send packet that won't match our predicate
    std::thread sender([this]() {
        std::this_thread::sleep_for(20ms);
        send_test_packet();
    });
    
    // Wait for packet that will never match (impossible predicate)
    auto packet = wait_for_packet([](const Packet& p) {
        // Looking for impossibly large packet
        return p.size() > 100000;
    }, 200ms);
    
    sender.join();
    
    EXPECT_FALSE(packet.has_value());
}

// =============================================================================
// gMock Matcher Integration Tests
// =============================================================================

class MatcherIntegrationTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55556");
        LoopbackTestFixture::SetUp();
    }
    
    void send_test_packet() {
        std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
        ASSERT_TRUE(send_udp(55556, payload));
    }
};

TEST_F(MatcherIntegrationTest, WaitForMatch_UsesGMockMatcher) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    // Use gMock matcher to find packet
    auto packet = wait_for_match(IsUDP(), 500ms);
    
    sender.join();
    
    ASSERT_TRUE(packet.has_value());
    EXPECT_THAT(*packet, IsUDP());
}

TEST_F(MatcherIntegrationTest, WaitForMatch_WithPortMatcher) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    // Use port matcher
    auto packet = wait_for_match(HasDestPort(55556), 500ms);
    
    sender.join();
    
    ASSERT_TRUE(packet.has_value());
    EXPECT_THAT(*packet, HasDestPort(55556));
}

TEST_F(MatcherIntegrationTest, WaitForMatch_WithPayloadMatcher) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    // Use payload contains matcher
    auto packet = wait_for_match(PayloadContains({0xDE, 0xAD, 0xBE, 0xEF}), 500ms);
    
    sender.join();
    
    ASSERT_TRUE(packet.has_value());
    EXPECT_THAT(*packet, PayloadContains({0xDE, 0xAD, 0xBE, 0xEF}));
}

TEST_F(MatcherIntegrationTest, WaitForMatch_WithCombinedMatchers) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    // Use combined matchers
    auto packet = wait_for_match(
        ::testing::AllOf(IsUDP(), HasDestPort(55556)), 
        500ms);
    
    sender.join();
    
    ASSERT_TRUE(packet.has_value());
    EXPECT_THAT(*packet, ::testing::AllOf(IsUDP(), HasDestPort(55556)));
}

// =============================================================================
// Packet Collection Tests
// =============================================================================

class PacketCollectionTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55557");
        LoopbackTestFixture::SetUp();
    }
    
    void send_test_packets(int count, std::chrono::milliseconds delay = 20ms) {
        for (int i = 0; i < count; ++i) {
            std::vector<uint8_t> payload = {
                static_cast<uint8_t>(i), 
                static_cast<uint8_t>(i + 1), 
                static_cast<uint8_t>(i + 2)
            };
            send_udp(55557, payload);
            if (i < count - 1) {
                std::this_thread::sleep_for(delay);
            }
        }
    }
};

TEST_F(PacketCollectionTest, CollectPackets_GathersAllWithinDuration) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(20ms);
        send_test_packets(5, 30ms);
    });
    
    // Collect for duration covering all sends
    auto packets = collect_packets(300ms);
    
    sender.join();
    
    // Should have collected all packets
    EXPECT_GE(packets.size(), 5u);
}

TEST_F(PacketCollectionTest, CollectUntil_StopsOnPredicate) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(20ms);
        // Send packets with incrementing first byte
        for (int i = 0; i < 10; ++i) {
            std::vector<uint8_t> payload = {static_cast<uint8_t>(i), 0x00, 0x00};
            send_udp(55557, payload);
            std::this_thread::sleep_for(20ms);
        }
    });
    
    // Collect until we see packet with payload starting with 0x05
    auto packets = collect_until([](const Packet& p) {
        auto data = p.data();
        // UDP payload starts after headers, search for 0x05 byte
        for (size_t i = 0; i < data.size(); ++i) {
            if (static_cast<uint8_t>(data[i]) == 0x05) {
                return true;
            }
        }
        return false;
    }, 500ms);
    
    sender.join();
    
    // Should have stopped after seeing the trigger packet
    EXPECT_LE(packets.size(), 10u);
    EXPECT_GE(packets.size(), 1u);
}

TEST_F(PacketCollectionTest, CountPackets_CountsMatchingPackets) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(20ms);
        send_test_packets(5, 20ms);
    });
    
    // Count UDP packets
    auto count = count_packets([](const Packet&) { return true; }, 300ms);
    
    sender.join();
    
    EXPECT_GE(count, 5u);
}

TEST_F(PacketCollectionTest, AnyPacketMatches_ReturnsTrue) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packets(1);
    });
    
    bool found = any_packet_matches([](const Packet&) { return true; }, 500ms);
    
    sender.join();
    
    EXPECT_TRUE(found);
}

TEST_F(PacketCollectionTest, AnyPacketMatches_ReturnsFalseOnNoMatch) {
    // Don't send any packets
    bool found = any_packet_matches([](const Packet&) { return true; }, 100ms);
    
    EXPECT_FALSE(found);
}

// =============================================================================
// Packet Storage Tests
// =============================================================================

class PacketStorageTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55558");
        LoopbackTestFixture::SetUp();
    }
    
    void send_test_packet() {
        std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC};
        send_udp(55558, payload);
    }
};

TEST_F(PacketStorageTest, StoredPackets_AccumulatesCaptures) {
    EXPECT_TRUE(stored_packets().empty());
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(20ms);
        send_test_packet();
        std::this_thread::sleep_for(20ms);
        send_test_packet();
    });
    
    // Wait for packets
    wait_for_packet([](const Packet&) { return true; }, 100ms);
    wait_for_packet([](const Packet&) { return true; }, 100ms);
    
    sender.join();
    
    EXPECT_GE(stored_packets().size(), 2u);
}

TEST_F(PacketStorageTest, SavePcap_CreatesFile) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(20ms);
        send_test_packet();
    });
    
    wait_for_packet([](const Packet&) { return true; }, 100ms);
    sender.join();
    
    // Save to temporary file
    auto path = std::filesystem::temp_directory_path() / "wadjet_test_save.pcap";
    save_pcap(path);
    
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 0);
    
    // Cleanup
    std::filesystem::remove(path);
}

// =============================================================================
// Failure PCAP Storage Tests
// =============================================================================

class FailurePcapTest : public LoopbackTestFixture {
protected:
    std::filesystem::path test_failure_dir_;
    
    void SetUp() override {
        test_failure_dir_ = std::filesystem::temp_directory_path() / "wadjet_failure_test";
        std::filesystem::create_directories(test_failure_dir_);
        
        set_filter("udp port 55559");
        set_failure_dir(test_failure_dir_);
        set_save_on_failure(true);
        LoopbackTestFixture::SetUp();
    }
    
    void TearDown() override {
        LoopbackTestFixture::TearDown();
        // Cleanup failure directory
        std::error_code ec;
        std::filesystem::remove_all(test_failure_dir_, ec);
    }
    
    void send_test_packet() {
        std::vector<uint8_t> payload = {0x11, 0x22, 0x33};
        send_udp(55559, payload);
    }
};

TEST_F(FailurePcapTest, ConfigurationIsSet) {
    EXPECT_EQ(config().failure_dir, test_failure_dir_);
    EXPECT_TRUE(config().save_on_failure);
}

// =============================================================================
// Macro Tests
// =============================================================================

class MacroTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55560");
        LoopbackTestFixture::SetUp();
    }
    
    void send_test_packet() {
        std::vector<uint8_t> payload = {0xCA, 0xFE};
        send_udp(55560, payload);
    }
};

TEST_F(MacroTest, WadjetAssertPacket_Succeeds) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    WADJET_ASSERT_PACKET(IsUDP(), 500ms);
    
    sender.join();
}

TEST_F(MacroTest, WadjetExpectPacket_Succeeds) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    WADJET_EXPECT_PACKET(HasDestPort(55560), 500ms);
    
    sender.join();
}

// =============================================================================
// Configuration Tests
// =============================================================================

class ConfigurationTest : public LiveCaptureTestFixture {
protected:
    void SetUp() override {
        set_interface("lo");
        set_filter("icmp");
        set_save_on_failure(false);
        set_failure_dir("/custom/path");
        config().max_stored_packets = 500;
        LiveCaptureTestFixture::SetUp();
    }
};

TEST_F(ConfigurationTest, ConfigurationIsApplied) {
    EXPECT_EQ(config().interface, "lo");
    EXPECT_EQ(config().filter, "icmp");
    EXPECT_FALSE(config().save_on_failure);
    EXPECT_EQ(config().failure_dir, "/custom/path");
    EXPECT_EQ(config().max_stored_packets, 500u);
}

// =============================================================================
// Edge Cases
// =============================================================================

class EdgeCaseTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55561");
        LoopbackTestFixture::SetUp();
    }
};

TEST_F(EdgeCaseTest, ZeroTimeout_ReturnsImmediately) {
    auto start = std::chrono::steady_clock::now();
    auto packet = wait_for_packet([](const Packet&) { return true; }, 0ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(packet.has_value());
    EXPECT_LT(elapsed, 50ms);
}

TEST_F(EdgeCaseTest, VeryShortTimeout_DoesNotHang) {
    auto start = std::chrono::steady_clock::now();
    auto packet = wait_for_packet([](const Packet&) { return true; }, 1ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(packet.has_value());
    EXPECT_LT(elapsed, 100ms);
}

TEST_F(EdgeCaseTest, CollectPackets_EmptyOnNoTraffic) {
    auto packets = collect_packets(50ms);
    EXPECT_TRUE(packets.empty());
}

// =============================================================================
// Protocol Dispatcher Integration
// =============================================================================

class DispatcherIntegrationTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55562");
        LoopbackTestFixture::SetUp();
    }
    
    void send_test_packet() {
        std::vector<uint8_t> payload = {0x12, 0x34, 0x56, 0x78};
        send_udp(55562, payload);
    }
};

TEST_F(DispatcherIntegrationTest, DispatcherDecodesCapture) {
    std::thread sender([this]() {
        std::this_thread::sleep_for(50ms);
        send_test_packet();
    });
    
    auto packet = wait_for_match(IsUDP(), 500ms);
    sender.join();
    
    ASSERT_TRUE(packet.has_value());
    
    // Use dispatcher to decode
    auto result = dispatcher().decode(packet->data());
    
    EXPECT_TRUE(result.has_layer<protocols::udp::UdpHeader>());
    
    auto* udp = result.get_layer<protocols::udp::UdpHeader>();
    ASSERT_NE(udp, nullptr);
    EXPECT_EQ(udp->dst_port, 55562);
}

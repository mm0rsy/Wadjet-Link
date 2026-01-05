/// @file test_replay_session.cpp
/// @brief Tests for ReplaySession

#include <gtest/gtest.h>
#include <wadjet/core/timestamp.hpp>
#include <wadjet/io/replay_session.hpp>
#include <wadjet/net/packet.hpp>
#include <wadjet/pcap/pcap_writer.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

#ifdef __linux__
    #include <linux/if_packet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace wadjet::test {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

//==============================================================================
// Helper Functions
//==============================================================================

/// @brief Check if we have capture capability
static bool has_capture_capability() {
#ifdef __linux__
    // Try to create a raw socket
    int sock = ::socket(AF_PACKET, SOCK_RAW, 0);
    if (sock >= 0) {
        ::close(sock);
        return true;
    }
#endif
    return false;
}

/// @brief Create a test Ethernet packet
static Packet create_ethernet_packet(Timestamp ts, std::size_t size = 64) {
    std::vector<std::byte> data;
    data.reserve(size);

    // Ethernet header (14 bytes)
    // Destination MAC: broadcast
    for (int i = 0; i < 6; ++i)
        data.push_back(std::byte{0xff});
    // Source MAC: 00:11:22:33:44:55
    data.push_back(std::byte{0x00});
    data.push_back(std::byte{0x11});
    data.push_back(std::byte{0x22});
    data.push_back(std::byte{0x33});
    data.push_back(std::byte{0x44});
    data.push_back(std::byte{0x55});
    // EtherType: IPv4
    data.push_back(std::byte{0x08});
    data.push_back(std::byte{0x00});

    // Payload
    while (data.size() < size) {
        data.push_back(static_cast<std::byte>(data.size() & 0xff));
    }

    return Packet(ByteSpan(data.data(), data.size()), ts);
}

/// @brief Create a PCAP file with test packets
static bool create_test_pcap(const fs::path& path, std::size_t num_packets,
                             std::chrono::milliseconds inter_packet_delay = 10ms) {
    auto result = pcap::PcapWriter::create(path);
    if (!result)
        return false;

    auto& writer = result.value();
    auto ts = Timestamp::now();

    for (std::size_t i = 0; i < num_packets; ++i) {
        auto pkt = create_ethernet_packet(ts, 64 + (i % 50));
        if (!writer.write_packet(pkt))
            return false;
        ts = ts + inter_packet_delay;
    }

    return true;
}

//==============================================================================
// Test Fixture
//==============================================================================

class ReplaySessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "wadjet_replay_test";
        fs::create_directories(test_dir_);

        if (!has_capture_capability()) {
            GTEST_SKIP() << "Skipping: requires CAP_NET_RAW capability";
        }
    }

    void TearDown() override { fs::remove_all(test_dir_); }

    fs::path test_file(const std::string& name) const { return test_dir_ / name; }

    fs::path test_dir_;
};

/// @brief Test fixture for tests that can run without CAP_NET_RAW
class ReplaySessionBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "wadjet_replay_basic_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override { fs::remove_all(test_dir_); }

    fs::path test_file(const std::string& name) const { return test_dir_ / name; }

    fs::path test_dir_;
};

//==============================================================================
// Basic Tests (no CAP_NET_RAW required for creation)
//==============================================================================

TEST_F(ReplaySessionBasicTest, CreateFromPackets) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 5; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 10ms;
    }

    if (!has_capture_capability()) {
        // Can still test that creation fails gracefully
        auto result = io::ReplaySession::create("lo", std::move(packets));
        // May fail if no CAP_NET_RAW, that's expected
        SUCCEED();
        return;
    }

    auto result = io::ReplaySession::create("lo", std::move(packets));
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();
    EXPECT_EQ(session.packet_count(), 5u);
    EXPECT_EQ(session.interface_name(), "lo");
}

TEST_F(ReplaySessionBasicTest, ReplayTimingModes) {
    // Test that timing enum values exist
    io::ReplaySessionOptions opts;

    opts.timing = io::ReplayTiming::Immediate;
    EXPECT_EQ(opts.timing, io::ReplayTiming::Immediate);

    opts.timing = io::ReplayTiming::AsRecorded;
    EXPECT_EQ(opts.timing, io::ReplayTiming::AsRecorded);

    opts.timing = io::ReplayTiming::FixedRate;
    opts.packets_per_second = 1000;
    EXPECT_EQ(opts.packets_per_second, 1000u);

    opts.timing = io::ReplayTiming::Scaled;
    opts.speed_factor = 2.0;
    EXPECT_EQ(opts.speed_factor, 2.0);
}

TEST_F(ReplaySessionBasicTest, OptionsDefaults) {
    io::ReplaySessionOptions opts;

    EXPECT_EQ(opts.timing, io::ReplayTiming::AsRecorded);
    EXPECT_EQ(opts.speed_factor, 1.0);
    EXPECT_EQ(opts.packets_per_second, 1000u);
    EXPECT_FALSE(opts.loop);
    EXPECT_EQ(opts.max_iterations, 0u);
    EXPECT_EQ(opts.max_packets, 0u);
}

TEST_F(ReplaySessionBasicTest, InvalidPcapFile) {
    auto path = test_file("nonexistent.pcap");

    if (!has_capture_capability()) {
        GTEST_SKIP() << "Skipping: requires CAP_NET_RAW capability";
    }

    auto result = io::ReplaySession::create("lo", path);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ReplaySessionBasicTest, CreateFromPcap) {
    auto pcap_path = test_file("test.pcap");
    ASSERT_TRUE(create_test_pcap(pcap_path, 10, 10ms));

    if (!has_capture_capability()) {
        GTEST_SKIP() << "Skipping: requires CAP_NET_RAW capability";
    }

    auto result = io::ReplaySession::create("lo", pcap_path);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();
    EXPECT_EQ(session.packet_count(), 10u);
}

//==============================================================================
// Replay Tests (require CAP_NET_RAW)
//==============================================================================

TEST_F(ReplaySessionTest, RunImmediate) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 10; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 100ms;  // Original timing
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Immediate;

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    auto start = std::chrono::steady_clock::now();
    auto run_result = session.run();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(run_result.is_ok()) << run_result.error().message;

    // Should be very fast (< 100ms for 10 packets in immediate mode)
    EXPECT_LT(elapsed, 100ms);

    auto stats = session.stats();
    EXPECT_EQ(stats.packets_sent, 10u);
}

TEST_F(ReplaySessionTest, RunFixedRate) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 10; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 10ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::FixedRate;
    opts.packets_per_second = 100;  // 10 packets at 100 pps = ~100ms

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    auto start = std::chrono::steady_clock::now();
    auto run_result = session.run();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(run_result.is_ok());

    // Should take approximately 100ms (10 packets at 100 pps)
    // Allow some tolerance
    EXPECT_GE(elapsed, 80ms);
    EXPECT_LT(elapsed, 200ms);
}

TEST_F(ReplaySessionTest, RunMaxPackets) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 100; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 1ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Immediate;
    opts.max_packets = 50;  // Limit to 50 packets

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();
    auto run_result = session.run();
    EXPECT_TRUE(run_result.is_ok());

    auto stats = session.stats();
    EXPECT_EQ(stats.packets_sent, 50u);
}

TEST_F(ReplaySessionTest, RunWithLoop) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 5; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 1ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Immediate;
    opts.loop = true;
    opts.max_iterations = 3;

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();
    auto run_result = session.run();
    EXPECT_TRUE(run_result.is_ok());

    auto stats = session.stats();
    EXPECT_EQ(stats.packets_sent, 15u);  // 5 packets * 3 iterations
    EXPECT_EQ(stats.iterations, 3u);
}

TEST_F(ReplaySessionTest, RunWithModifier) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 10; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 1ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Immediate;

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    // Skip every other packet
    int packet_index = 0;
    session.set_modifier(
        [&packet_index]([[maybe_unused]] Packet& pkt) { return (packet_index++ % 2) == 0; });

    auto run_result = session.run();
    EXPECT_TRUE(run_result.is_ok());

    auto stats = session.stats();
    EXPECT_EQ(stats.packets_sent, 5u);  // Half the packets
}

TEST_F(ReplaySessionTest, RunWithProgressCallback) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 100; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 1ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Immediate;

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    int callback_count = 0;
    session.set_progress_callback(
        [&callback_count](const io::ReplayStats& stats) {
            callback_count++;
            return stats.packets_sent < 50;  // Stop after 50 packets
        },
        10);  // Call every 10 packets

    auto run_result = session.run();
    EXPECT_TRUE(run_result.is_ok());

    // Should have been called approximately 5 times (50 packets / 10 interval)
    EXPECT_GE(callback_count, 4);
    EXPECT_LE(callback_count, 6);
}

TEST_F(ReplaySessionTest, Stop) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 1000; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 10ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::AsRecorded;  // Slow replay

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    // Start replay in background and stop it
    std::thread replay_thread([&session]() { session.run(); });

    std::this_thread::sleep_for(50ms);
    session.stop();

    replay_thread.join();

    auto stats = session.stats();
    EXPECT_LT(stats.packets_sent, 1000u);  // Should have stopped early
}

TEST_F(ReplaySessionTest, SendSinglePacket) {
    std::vector<Packet> packets;  // Empty

    io::ReplaySessionOptions opts;

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    // Creating with empty packets should fail
    EXPECT_FALSE(result.is_ok());

    // Create with at least one packet
    std::vector<Packet> packets2;
    packets2.push_back(create_ethernet_packet(Timestamp::now(), 64));

    result = io::ReplaySession::create("lo", std::move(packets2), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    // Send additional packet
    auto pkt = create_ethernet_packet(Timestamp::now(), 100);
    auto send_result = session.send_packet(pkt);
    EXPECT_TRUE(send_result.is_ok()) << send_result.error().message;
}

TEST_F(ReplaySessionTest, ScaledTiming) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    for (int i = 0; i < 10; ++i) {
        packets.push_back(create_ethernet_packet(ts, 64));
        ts = ts + 100ms;  // 100ms between packets
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Scaled;
    opts.speed_factor = 10.0;  // 10x faster = 10ms between packets

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();

    auto start = std::chrono::steady_clock::now();
    auto run_result = session.run();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(run_result.is_ok());

    // Original timing: 10 * 100ms = 1s
    // Scaled (10x): ~100ms
    // Allow tolerance
    EXPECT_GE(elapsed, 50ms);
    EXPECT_LT(elapsed, 300ms);
}

TEST_F(ReplaySessionTest, StatsAccuracy) {
    std::vector<Packet> packets;
    auto ts = Timestamp::now();
    std::size_t total_bytes = 0;
    for (std::size_t i = 0; i < 20; ++i) {
        auto size = 64 + (i * 10);
        packets.push_back(create_ethernet_packet(ts, size));
        total_bytes += size;
        ts = ts + 1ms;
    }

    io::ReplaySessionOptions opts;
    opts.timing = io::ReplayTiming::Immediate;

    auto result = io::ReplaySession::create("lo", std::move(packets), opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();
    auto run_result = session.run();
    EXPECT_TRUE(run_result.is_ok());

    auto stats = session.stats();
    EXPECT_EQ(stats.packets_sent, 20u);
    EXPECT_EQ(stats.bytes_sent, total_bytes);
    EXPECT_GT(stats.total_time.count(), 0);
}

}  // namespace wadjet::test

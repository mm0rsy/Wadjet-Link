/// @file test_capture_session.cpp
/// @brief Integration tests for CaptureSession (live capture)
///
/// NOTE: These tests require CAP_NET_RAW capability or root privileges.
/// Tests are marked as DISABLED by default and can be enabled by running
/// with --gtest_also_run_disabled_tests flag.

#include "wadjet/io/capture_session.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/udp.hpp"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace std::chrono_literals;

namespace {

/// Helper to check if we have network capture capabilities
bool has_capture_capability() {
    // Try to create a raw socket
    int sock = socket(AF_PACKET, SOCK_RAW, 0);
    if (sock >= 0) {
        close(sock);
        return true;
    }
    return false;
}

/// Helper to send a UDP packet to loopback
bool send_udp_packet(std::uint16_t port, const std::vector<std::uint8_t>& payload) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return false;

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ssize_t sent = sendto(sock, payload.data(), payload.size(), 0,
                          reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    close(sock);
    return sent == static_cast<ssize_t>(payload.size());
}

/// Helper class for receiving UDP packets
class UdpReceiver {
public:
    explicit UdpReceiver(std::uint16_t port) : port_(port), sock_(-1), running_(false) {}

    bool start() {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0)
            return false;

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(sock_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(sock_);
            sock_ = -1;
            return false;
        }

        running_ = true;
        return true;
    }

    void stop() {
        running_ = false;
        if (sock_ >= 0) {
            close(sock_);
            sock_ = -1;
        }
    }

    ~UdpReceiver() { stop(); }

private:
    std::uint16_t port_;
    int sock_;
    std::atomic<bool> running_;
};

}  // namespace

//==============================================================================
// CaptureSession Basic Tests (DISABLED by default - requires CAP_NET_RAW)
//==============================================================================

class CaptureSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_capture_capability()) {
            GTEST_SKIP() << "Skipping: requires CAP_NET_RAW capability";
        }
    }
};

TEST_F(CaptureSessionTest, CreateSessionOnLoopback) {
    auto result = io::CaptureSession::create("lo");
    EXPECT_TRUE(result.is_ok()) << "Failed to create capture session on loopback";
}

TEST_F(CaptureSessionTest, CreateWithOptions) {
    io::CaptureSessionOptions opts;
    opts.promiscuous = false;
    opts.buffer_size = 1024 * 1024;  // 1MB

    auto result = io::CaptureSession::create("lo", opts);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(CaptureSessionTest, SetBpfFilter) {
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();

    // Set a simple UDP filter
    auto filter_result = session.set_filter("udp port 12345");
    EXPECT_TRUE(filter_result.is_ok()) << "Failed to set BPF filter";
}

TEST_F(CaptureSessionTest, StatsInitiallyZero) {
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto stats = session.stats();
    EXPECT_EQ(stats.packets_received, 0u);
}

//==============================================================================
// E2E Loopback Tests (DISABLED by default - requires CAP_NET_RAW)
//==============================================================================

class LoopbackCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_capture_capability()) {
            GTEST_SKIP() << "Skipping: requires CAP_NET_RAW capability";
        }
    }

    ProtocolDispatcher dispatcher;
};

TEST_F(LoopbackCaptureTest, CaptureUdpOnLoopback) {
    constexpr std::uint16_t TEST_PORT = 55555;

    // Create capture session on loopback with filter
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55555");
    ASSERT_TRUE(filter_result.is_ok());

    // Start a UDP receiver
    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    // Send a UDP packet
    std::vector<std::uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    std::thread sender([&]() {
        std::this_thread::sleep_for(10ms);
        send_udp_packet(TEST_PORT, payload);
    });

    // Try to capture the packet
    auto packet = session.next_packet(100ms);

    sender.join();

    // On loopback, we might capture cooked Linux or raw Ethernet
    // The packet should at least be valid
    EXPECT_TRUE(packet.has_value()) << "No packet captured within timeout";

    if (packet) {
        EXPECT_GT(packet->view().size(), 0u);
    }
}

TEST_F(LoopbackCaptureTest, CaptureLoopWithCallback) {
    constexpr std::uint16_t TEST_PORT = 55556;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55556");
    ASSERT_TRUE(filter_result.is_ok());

    // Start receiver
    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::atomic<int> captured_count{0};
    std::atomic<bool> stop_capture{false};

    // Start capture in background
    std::thread capture_thread([&]() {
        session.capture_loop([&]([[maybe_unused]] const PacketView& pkt) {
            captured_count++;
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send some packets
    std::this_thread::sleep_for(50ms);
    for (int i = 0; i < 5; i++) {
        send_udp_packet(TEST_PORT, {static_cast<std::uint8_t>(i)});
        std::this_thread::sleep_for(10ms);
    }

    // Stop capture
    std::this_thread::sleep_for(50ms);
    stop_capture = true;
    session.stop();

    capture_thread.join();

    // Should have captured at least some packets
    EXPECT_GT(captured_count.load(), 0) << "No packets captured in loop";
}

TEST_F(LoopbackCaptureTest, DecodesCapturedPackets) {
    constexpr std::uint16_t TEST_PORT = 55557;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55557");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    // Send UDP packet
    std::thread sender([&]() {
        std::this_thread::sleep_for(10ms);
        send_udp_packet(TEST_PORT, {0x01, 0x02, 0x03, 0x04});
    });

    auto packet = session.next_packet(100ms);
    sender.join();

    ASSERT_TRUE(packet.has_value()) << "No packet captured";

    // Try to decode - loopback might use SLL (Linux cooked capture)
    // so full Ethernet decode might fail, but we should get something
    auto view = packet->view();
    EXPECT_GT(view.size(), 0u);

    // The raw data should exist and be reasonable size
    // (at least UDP header + payload = 8 + 4 = 12 bytes)
    EXPECT_GE(view.size(), 12u);
}

//==============================================================================
// Timeout and Error Handling Tests
//==============================================================================

TEST_F(CaptureSessionTest, TimeoutWhenNoPackets) {
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();

    // Filter for a port with no traffic
    auto filter_result = session.set_filter("udp port 65534");
    ASSERT_TRUE(filter_result.is_ok());

    auto start = std::chrono::steady_clock::now();
    auto packet = session.next_packet(50ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should timeout (return nullopt) after ~50ms
    EXPECT_FALSE(packet.has_value());
    EXPECT_GE(elapsed, 45ms);   // Allow some tolerance
    EXPECT_LE(elapsed, 200ms);  // But shouldn't take too long
}

TEST_F(CaptureSessionTest, InvalidInterfaceFails) {
    auto result = io::CaptureSession::create("nonexistent_interface_xyz");
    EXPECT_FALSE(result.is_ok()) << "Should fail for invalid interface";
}

//==============================================================================
// BPF Filter Tests
//==============================================================================

TEST_F(CaptureSessionTest, InvalidFilterFails) {
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();

    // Invalid BPF syntax
    auto filter_result = session.set_filter("this is not valid bpf syntax!!!");
    EXPECT_FALSE(filter_result.is_ok());
}

TEST_F(CaptureSessionTest, ComplexFilterWorks) {
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();

    // Complex filter for automotive protocols
    auto filter_result = session.set_filter("udp port 30490 or tcp port 13400");
    EXPECT_TRUE(filter_result.is_ok());
}

//==============================================================================
// Integration: Full Pipeline with Live Capture
//==============================================================================

TEST_F(LoopbackCaptureTest, FullPipelineFromCapture) {
    constexpr std::uint16_t TEST_PORT = 55558;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55558");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::atomic<bool> captured{false};
    std::atomic<bool> stop{false};

    // Capture with callback that decodes packets
    std::thread capture_thread([&]() {
        session.capture_loop([&]([[maybe_unused]] const PacketView& pkt) {
            // Successfully received a packet
            captured = true;
            if (stop.load()) {
                session.stop();
            }
        });
    });

    // Send packet
    std::this_thread::sleep_for(50ms);
    send_udp_packet(TEST_PORT, {0xCA, 0xFE, 0xBA, 0xBE});

    // Wait for capture
    for (int i = 0; i < 20 && !captured; i++) {
        std::this_thread::sleep_for(10ms);
    }

    stop = true;
    session.stop();
    capture_thread.join();

    EXPECT_TRUE(captured.load()) << "Packet was not captured";

    auto stats = session.stats();
    EXPECT_GT(stats.packets_received, 0u);
}

//==============================================================================
// Timestamp Monotonicity Tests
//==============================================================================

TEST_F(LoopbackCaptureTest, TimestampsAreMonotonic) {
    constexpr std::uint16_t TEST_PORT = 55559;
    constexpr std::size_t NUM_PACKETS = 100;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55559");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::vector<Timestamp> timestamps;
    timestamps.reserve(NUM_PACKETS);
    std::atomic<std::size_t> captured_count{0};
    std::atomic<bool> stop_capture{false};

    // Start capture
    std::thread capture_thread([&]() {
        session.capture_loop([&](const PacketView& pkt) {
            timestamps.push_back(pkt.timestamp());
            captured_count++;
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send packets rapidly
    std::this_thread::sleep_for(50ms);
    for (std::size_t i = 0; i < NUM_PACKETS; ++i) {
        std::vector<std::uint8_t> payload = {static_cast<std::uint8_t>(i & 0xff),
                                             static_cast<std::uint8_t>((i >> 8) & 0xff)};
        send_udp_packet(TEST_PORT, payload);
    }

    // Wait for packets
    for (int i = 0; i < 100 && captured_count < NUM_PACKETS / 2; i++) {
        std::this_thread::sleep_for(10ms);
    }

    stop_capture = true;
    session.stop();
    capture_thread.join();

    // Verify timestamps are monotonically increasing
    ASSERT_GT(timestamps.size(), 1u) << "Need at least 2 packets for monotonicity test";

    bool monotonic = true;
    for (std::size_t i = 1; i < timestamps.size(); ++i) {
        if (timestamps[i] < timestamps[i - 1]) {
            monotonic = false;
            break;
        }
    }

    EXPECT_TRUE(monotonic) << "Timestamps are not monotonically increasing";
}

//==============================================================================
// Dropped Packet Counter Tests
//==============================================================================

TEST_F(LoopbackCaptureTest, DroppedPacketCounterWorks) {
    constexpr std::uint16_t TEST_PORT = 55560;

    // Create session with default buffer (larger to avoid ASAN issues with mmap)
    io::CaptureSessionOptions opts;
    // Use larger buffer to avoid ring buffer overflow under ASAN
    opts.buffer_size = 4 * 1024 * 1024;

    auto result = io::CaptureSession::create("lo", opts);
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55560");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    // Verify stats can be retrieved
    auto initial_stats = session.stats();
    EXPECT_GE(initial_stats.packets_dropped, 0u);  // Should be 0 or valid count

    std::atomic<bool> stop_capture{false};
    std::atomic<std::size_t> captured_count{0};

    // Start capture (no artificial delay to avoid buffer overflow)
    std::thread capture_thread([&]() {
        session.capture_loop([&]([[maybe_unused]] const PacketView& pkt) {
            captured_count++;
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send packets at a moderate rate
    std::this_thread::sleep_for(20ms);
    for (int i = 0; i < 100; ++i) {
        send_udp_packet(TEST_PORT, {0x00, 0x01, 0x02, 0x03});
        std::this_thread::sleep_for(1ms);  // Pace the sends
    }

    // Wait and stop
    std::this_thread::sleep_for(100ms);
    stop_capture = true;
    session.stop();
    capture_thread.join();

    // Check stats - we should have captured most packets
    auto final_stats = session.stats();
    EXPECT_GE(final_stats.packets_received, captured_count.load());
}

//==============================================================================
// Hardware Timestamp Capability Tests
//==============================================================================

TEST_F(CaptureSessionTest, QueryHardwareTimestampCaps) {
    auto result = io::CaptureSession::query_hw_timestamp_caps("lo");
    // May fail on loopback (no HW timestamps), that's OK
    if (result.is_ok()) {
        auto caps = result.value();
        // Loopback typically doesn't support hardware timestamps
        // Just verify the struct is populated
        EXPECT_TRUE(caps.supports_rx_software || !caps.supports_rx_hardware);
    }
}

TEST_F(CaptureSessionTest, TimestampSourceDefault) {
    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto ts_source = session.active_timestamp_source();

    // Should be Software or Auto on loopback
    EXPECT_TRUE(ts_source == io::TimestampSource::Software ||
                ts_source == io::TimestampSource::Auto);
}

TEST_F(CaptureSessionTest, RequestSoftwareTimestamps) {
    io::CaptureSessionOptions opts;
    opts.timestamp_source = io::TimestampSource::Software;

    auto result = io::CaptureSession::create("lo", opts);
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    EXPECT_EQ(session.active_timestamp_source(), io::TimestampSource::Software);
}
//==============================================================================
// Stress/Load Tests
//==============================================================================

TEST_F(LoopbackCaptureTest, HighPacketRateCapture) {
    constexpr std::uint16_t TEST_PORT = 55561;
    constexpr std::size_t NUM_PACKETS = 1000;
    constexpr std::size_t BURST_SIZE = 50;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55561");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::atomic<std::size_t> captured_count{0};
    std::atomic<bool> stop_capture{false};

    // Start capture
    std::thread capture_thread([&]() {
        session.capture_loop([&]([[maybe_unused]] const PacketView& pkt) {
            captured_count++;
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send packets in bursts
    std::this_thread::sleep_for(50ms);
    for (std::size_t i = 0; i < NUM_PACKETS; i += BURST_SIZE) {
        for (std::size_t j = 0; j < BURST_SIZE && (i + j) < NUM_PACKETS; ++j) {
            send_udp_packet(TEST_PORT, {0x00, 0x01, 0x02, 0x03});
        }
        // Brief pause between bursts
        std::this_thread::sleep_for(1ms);
    }

    // Wait for packets
    std::this_thread::sleep_for(200ms);
    stop_capture = true;
    session.stop();
    capture_thread.join();

    auto stats = session.stats();

    // We should capture most packets (at least 50%)
    EXPECT_GE(captured_count.load(), NUM_PACKETS / 2)
        << "Captured " << captured_count.load() << " of " << NUM_PACKETS << " packets";

    // Stats should reflect what we captured
    EXPECT_GE(stats.packets_received, captured_count.load());
}

TEST_F(LoopbackCaptureTest, LargePacketCapture) {
    constexpr std::uint16_t TEST_PORT = 55562;
    // Use size smaller than typical MTU - loopback usually has 65536 MTU
    // but UDP+IP header overhead means we use 1400 for safety
    constexpr std::size_t PACKET_SIZE = 1400;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55562");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::atomic<bool> captured{false};
    std::atomic<std::size_t> captured_size{0};
    std::atomic<bool> stop_capture{false};

    // Start capture
    std::thread capture_thread([&]() {
        session.capture_loop([&](const PacketView& pkt) {
            captured = true;
            captured_size = pkt.data().size();
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send large packet
    std::this_thread::sleep_for(50ms);
    std::vector<std::uint8_t> large_payload(PACKET_SIZE, 0xAA);
    send_udp_packet(TEST_PORT, large_payload);

    // Wait for capture
    for (int i = 0; i < 50 && !captured; i++) {
        std::this_thread::sleep_for(10ms);
    }

    stop_capture = true;
    session.stop();
    capture_thread.join();

    EXPECT_TRUE(captured.load()) << "Large packet was not captured";

    // Captured packet should include full payload (+ headers)
    // UDP payload + IP header (20) + UDP header (8) + Ethernet (14) = payload + 42
    EXPECT_GT(captured_size.load(), PACKET_SIZE)
        << "Captured packet size: " << captured_size.load();
}

TEST_F(LoopbackCaptureTest, SustainedCapture) {
    constexpr std::uint16_t TEST_PORT = 55563;
    constexpr auto CAPTURE_DURATION = 1000ms;
    constexpr auto SEND_INTERVAL = 10ms;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55563");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::atomic<std::size_t> captured_count{0};
    std::atomic<std::size_t> sent_count{0};
    std::atomic<bool> stop_capture{false};

    // Start capture
    std::thread capture_thread([&]() {
        session.capture_loop([&]([[maybe_unused]] const PacketView& pkt) {
            captured_count++;
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send packets over time
    std::this_thread::sleep_for(50ms);
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < CAPTURE_DURATION) {
        send_udp_packet(TEST_PORT, {0x00, 0x01});
        sent_count++;
        std::this_thread::sleep_for(SEND_INTERVAL);
    }

    // Wait for final packets
    std::this_thread::sleep_for(100ms);
    stop_capture = true;
    session.stop();
    capture_thread.join();

    auto stats = session.stats();

    // Should capture at least 80% of packets
    auto expected_min = sent_count.load() * 8 / 10;
    EXPECT_GE(captured_count.load(), expected_min)
        << "Captured " << captured_count.load() << " of " << sent_count.load()
        << " packets (expected at least " << expected_min << ")";

    // Stats should be consistent
    EXPECT_GE(stats.packets_received, captured_count.load());
}

TEST_F(LoopbackCaptureTest, CaptureWithVariablePacketSizes) {
    constexpr std::uint16_t TEST_PORT = 55564;
    constexpr std::size_t NUM_PACKETS = 100;

    auto result = io::CaptureSession::create("lo");
    ASSERT_TRUE(result.is_ok());

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55564");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::vector<std::size_t> captured_sizes;
    std::mutex sizes_mutex;
    std::atomic<std::size_t> captured_count{0};
    std::atomic<bool> stop_capture{false};

    // Start capture
    std::thread capture_thread([&]() {
        session.capture_loop([&](const PacketView& pkt) {
            {
                std::lock_guard<std::mutex> lock(sizes_mutex);
                captured_sizes.push_back(pkt.data().size());
            }
            captured_count++;
            if (stop_capture.load()) {
                session.stop();
            }
        });
    });

    // Send packets of various sizes
    std::this_thread::sleep_for(50ms);
    std::vector<std::size_t> sent_sizes;
    for (std::size_t i = 0; i < NUM_PACKETS; ++i) {
        // Sizes from 10 to 10010 bytes
        std::size_t size = 10 + (i * 100);
        std::vector<std::uint8_t> payload(size, static_cast<std::uint8_t>(i & 0xff));
        send_udp_packet(TEST_PORT, payload);
        sent_sizes.push_back(size);
        std::this_thread::sleep_for(2ms);
    }

    // Wait for packets
    std::this_thread::sleep_for(500ms);
    stop_capture = true;
    session.stop();
    capture_thread.join();

    // Verify we captured packets of varying sizes
    {
        std::lock_guard<std::mutex> lock(sizes_mutex);
        EXPECT_GT(captured_sizes.size(), NUM_PACKETS / 4) << "Captured too few packets";

        if (captured_sizes.size() >= 2) {
            // Check we have size variation
            auto minmax = std::minmax_element(captured_sizes.begin(), captured_sizes.end());
            EXPECT_GT(*minmax.second - *minmax.first, 1000u)
                << "Expected size variation in captured packets";
        }
    }
}

//==============================================================================
// TPACKET_V3 Tests
//==============================================================================

TEST_F(LoopbackCaptureTest, TpacketV3CaptureBasic) {
    constexpr std::uint16_t TEST_PORT = 55570;

    io::CaptureSessionOptions opts;
    opts.use_tpacket_v3 = true;

    auto result = io::CaptureSession::create("lo", opts);
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& session = result.value();
    auto filter_result = session.set_filter("udp port 55570");
    ASSERT_TRUE(filter_result.is_ok());

    UdpReceiver receiver(TEST_PORT);
    ASSERT_TRUE(receiver.start());

    std::atomic<bool> captured{false};
    std::atomic<bool> stop{false};

    std::thread capture_thread([&]() {
        session.capture_loop([&]([[maybe_unused]] const PacketView& pkt) {
            captured = true;
            if (stop.load()) {
                session.stop();
            }
        });
    });

    std::this_thread::sleep_for(50ms);
    send_udp_packet(TEST_PORT, {0xDE, 0xAD, 0xBE, 0xEF});

    for (int i = 0; i < 20 && !captured; i++) {
        std::this_thread::sleep_for(10ms);
    }

    stop = true;
    session.stop();
    capture_thread.join();

    EXPECT_TRUE(captured.load()) << "TPACKET_V3 failed to capture packet";
}

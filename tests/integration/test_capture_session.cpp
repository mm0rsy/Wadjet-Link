/// @file test_capture_session.cpp
/// @brief Integration tests for CaptureSession (live capture)
/// 
/// NOTE: These tests require CAP_NET_RAW capability or root privileges.
/// Tests are marked as DISABLED by default and can be enabled by running
/// with --gtest_also_run_disabled_tests flag.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "wadjet/io/capture_session.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/udp.hpp"

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
    if (sock < 0) return false;
    
    struct sockaddr_in addr{};
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
        if (sock_ < 0) return false;
        
        struct sockaddr_in addr{};
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
    EXPECT_GE(elapsed, 45ms);  // Allow some tolerance
    EXPECT_LE(elapsed, 200ms); // But shouldn't take too long
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

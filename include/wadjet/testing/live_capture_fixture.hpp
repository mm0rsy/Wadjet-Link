/// @file live_capture_fixture.hpp
/// @brief GoogleTest fixture for live packet capture testing
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This fixture provides a base class for tests that need to capture live
/// network traffic. It handles session setup/teardown, BPF filtering, and
/// automatic PCAP storage on test failure.
///
/// Example:
/// @code
/// class MySOMEIPTest : public wadjet::testing::LiveCaptureTestFixture {
/// protected:
///     void SetUp() override {
///         set_interface("eth0");
///         set_filter("udp port 30490");
///         LiveCaptureTestFixture::SetUp();
///     }
/// };
///
/// TEST_F(MySOMEIPTest, ServiceOffersWithin100ms) {
///     auto packet = wait_for_packet(HasSOMEIPServiceId(0x1234), 100ms);
///     ASSERT_TRUE(packet.has_value());
/// }
/// @endcode

#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// Socket includes for Linux
#include "wadjet/io/capture_session.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/pcap/pcap_writer.hpp"
#include "wadjet/protocols/dispatcher.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace wadjet::testing {

/// Configuration options for LiveCaptureTestFixture
struct LiveCaptureConfig {
    /// Network interface to capture from (default: "lo" for loopback)
    std::string interface = "lo";

    /// BPF filter expression (empty = capture all)
    std::string filter;

    /// Whether to save packets to PCAP on test failure
    bool save_on_failure = true;

    /// Directory for failure PCAP files (default: /tmp/wadjet_failures)
    std::filesystem::path failure_dir = "/tmp/wadjet_failures";

    /// Maximum packets to keep in memory for failure dump
    std::size_t max_stored_packets = 1000;

    /// Capture session options
    io::CaptureSessionOptions session_options;
};

/// Base fixture class for live capture tests
///
/// This fixture provides:
/// - Automatic capture session setup/teardown
/// - Time-bounded packet waiting
/// - Pattern matching on captured packets
/// - Automatic PCAP storage on test failure
///
/// @note Tests using this fixture require CAP_NET_RAW capability or root privileges
class LiveCaptureTestFixture : public ::testing::Test {
protected:
    /// Set the network interface before SetUp
    void set_interface(const std::string& iface) { config_.interface = iface; }

    /// Set the BPF filter before SetUp
    void set_filter(const std::string& filter) { config_.filter = filter; }

    /// Set whether to save PCAP on failure
    void set_save_on_failure(bool save) { config_.save_on_failure = save; }

    /// Set the failure directory
    void set_failure_dir(const std::filesystem::path& dir) { config_.failure_dir = dir; }

    /// Access the full configuration
    LiveCaptureConfig& config() { return config_; }
    const LiveCaptureConfig& config() const { return config_; }

    /// SetUp - creates capture session
    void SetUp() override {
        // Check for capture capability
        if (!has_capture_capability()) {
            GTEST_SKIP() << "Skipping: requires CAP_NET_RAW capability or root privileges";
        }

        // Create capture session
        auto result = io::CaptureSession::create(config_.interface, config_.session_options);
        if (!result.is_ok()) {
            FAIL() << "Failed to create capture session on " << config_.interface << ": "
                   << result.error().message;
            return;
        }

        session_ = std::make_unique<io::CaptureSession>(std::move(result.value()));

        // Set BPF filter if provided
        if (!config_.filter.empty()) {
            auto filter_result = session_->set_filter(config_.filter);
            if (!filter_result.is_ok()) {
                FAIL() << "Failed to set BPF filter '" << config_.filter
                       << "': " << filter_result.error().message;
                return;
            }
        }

        // Start the capture session
        auto start_result = session_->start();
        if (!start_result.is_ok()) {
            FAIL() << "Failed to start capture session: " << start_result.error().message;
            return;
        }

        captured_packets_.clear();
    }

    /// TearDown - stores PCAP on failure
    void TearDown() override {
        if (config_.save_on_failure && HasFailure() && !captured_packets_.empty()) {
            save_failure_pcap();
        }

        stop_capture();
        session_.reset();
        captured_packets_.clear();
    }

    /// Get the capture session (for advanced usage)
    io::CaptureSession* session() { return session_.get(); }

    /// Get the protocol dispatcher
    protocols::ProtocolDispatcher& dispatcher() { return dispatcher_; }

    /// Wait for a packet matching the given predicate
    /// @param predicate Function that returns true if packet matches
    /// @param timeout Maximum time to wait
    /// @return The matching packet, or nullopt if timeout
    template <typename Predicate>
    std::optional<Packet> wait_for_packet(Predicate&& predicate,
                                          std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                break;

            auto packet = session_->next_packet(remaining);
            if (packet.has_value()) {
                store_packet(*packet);
                if (predicate(*packet)) {
                    return packet;
                }
            }
        }
        return std::nullopt;
    }

    /// Wait for a packet matching the given gMock matcher
    /// @param matcher gMock matcher for packet
    /// @param timeout Maximum time to wait
    /// @return The matching packet, or nullopt if timeout
    template <typename Matcher>
    std::optional<Packet> wait_for_match(const Matcher& matcher,
                                         std::chrono::milliseconds timeout) {
        return wait_for_packet(
            [&matcher](const Packet& pkt) { return ::testing::Value(pkt, matcher); }, timeout);
    }

    /// Collect all packets for a duration
    /// @param duration Time to capture
    /// @return Vector of captured packets
    std::vector<Packet> collect_packets(std::chrono::milliseconds duration) {
        std::vector<Packet> packets;
        auto deadline = std::chrono::steady_clock::now() + duration;

        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                break;

            auto packet = session_->next_packet(remaining);
            if (packet.has_value()) {
                store_packet(*packet);
                packets.push_back(std::move(*packet));
            }
        }
        return packets;
    }

    /// Collect packets until predicate returns true or timeout
    /// @param predicate Function that returns true when collection should stop
    /// @param timeout Maximum time to wait
    /// @return Vector of captured packets (including the one that triggered stop)
    template <typename Predicate>
    std::vector<Packet> collect_until(Predicate&& predicate, std::chrono::milliseconds timeout) {
        std::vector<Packet> packets;
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                break;

            auto packet = session_->next_packet(remaining);
            if (packet.has_value()) {
                store_packet(*packet);
                packets.push_back(*packet);
                if (predicate(*packet)) {
                    break;
                }
            }
        }
        return packets;
    }

    /// Count packets matching a predicate within a duration
    /// @param predicate Function that returns true if packet should be counted
    /// @param duration Time to capture
    /// @return Number of matching packets
    template <typename Predicate>
    std::size_t count_packets(Predicate&& predicate, std::chrono::milliseconds duration) {
        std::size_t count = 0;
        auto packets = collect_packets(duration);
        for (const auto& pkt : packets) {
            if (predicate(pkt)) {
                ++count;
            }
        }
        return count;
    }

    /// Check if any packet matches a predicate within a duration
    /// @param predicate Function that returns true if packet matches
    /// @param timeout Maximum time to wait
    /// @return true if at least one matching packet was found
    template <typename Predicate>
    bool any_packet_matches(Predicate&& predicate, std::chrono::milliseconds timeout) {
        return wait_for_packet(std::forward<Predicate>(predicate), timeout).has_value();
    }

    /// Get all stored packets (for inspection)
    const std::vector<Packet>& stored_packets() const { return captured_packets_; }

    /// Manually store current packets to PCAP
    void save_pcap(const std::filesystem::path& path) {
        auto writer = pcap::PcapWriter::create(path);
        if (writer.is_ok()) {
            for (const auto& pkt : captured_packets_) {
                writer->write_packet(pkt.view());
            }
        }
    }

protected:
    /// Check if we have capture capability
    static bool has_capture_capability() {
        int sock = socket(AF_PACKET, SOCK_RAW, 0);
        if (sock >= 0) {
            close(sock);
            return true;
        }
        return false;
    }

    /// Stop any background capture
    void stop_capture() {
        if (session_) {
            session_->stop();
        }
    }

    /// Store a packet (with size limit)
    void store_packet(const Packet& pkt) {
        if (captured_packets_.size() < config_.max_stored_packets) {
            captured_packets_.push_back(pkt);
        }
    }

    /// Save PCAP on failure
    void save_failure_pcap() {
        // Create failure directory
        std::error_code ec;
        std::filesystem::create_directories(config_.failure_dir, ec);

        // Generate filename from test info
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string filename =
            std::string(test_info->test_suite_name()) + "_" + std::string(test_info->name()) + "_" +
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".pcap";

        auto path = config_.failure_dir / filename;
        save_pcap(path);

        std::cerr << "\n[WADJET] Saved failure trace to: " << path << "\n";
    }

private:
    LiveCaptureConfig config_;
    std::unique_ptr<io::CaptureSession> session_;
    protocols::ProtocolDispatcher dispatcher_;
    std::vector<Packet> captured_packets_;
};

/// Fixture for loopback testing (no network required, but still needs CAP_NET_RAW)
class LoopbackTestFixture : public LiveCaptureTestFixture {
protected:
    void SetUp() override {
        set_interface("lo");
        LiveCaptureTestFixture::SetUp();
    }

    /// Send a UDP packet to loopback
    bool send_udp(std::uint16_t port, const std::vector<std::uint8_t>& payload) {
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

    /// Send a TCP packet to loopback (connection-oriented)
    bool connect_tcp(std::uint16_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
            return false;

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        int result = connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

        close(sock);
        return result == 0;
    }
};

}  // namespace wadjet::testing

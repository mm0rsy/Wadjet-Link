#pragma once

/// @file capture_session.hpp
/// @brief Live packet capture using AF_PACKET

#include "wadjet/core/packet_source.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/io/device.hpp"
#include "wadjet/io/frame_filter.hpp"
#include "wadjet/net/packet.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace wadjet::io {

/// @brief Capture session statistics
struct CaptureStats {
    std::uint64_t packets_received = 0;   ///< Packets received
    std::uint64_t packets_dropped = 0;    ///< Packets dropped (kernel)
    std::uint64_t packets_filtered = 0;   ///< Packets filtered out
    std::uint64_t bytes_received = 0;     ///< Total bytes received
};

/// @brief Capture session options
struct CaptureSessionOptions {
    std::uint32_t snaplen = 65535;     ///< Max bytes to capture per packet
    bool promiscuous = true;           ///< Enable promiscuous mode
    bool immediate_mode = true;        ///< Minimize latency
    std::size_t buffer_size = 2 * 1024 * 1024;  ///< Ring buffer size
    int timeout_ms = 100;              ///< Poll timeout in milliseconds
};

/// @brief Live packet capture session using Linux AF_PACKET
///
/// CaptureSession provides high-performance packet capture using
/// Linux AF_PACKET sockets with optional ring buffer support.
/// Implements IFilterablePacketSource for generic packet processing.
///
/// @code
/// auto session = CaptureSession::create("eth0");
/// if (session) {
///     while (auto packet = session->next_packet(100ms)) {
///         process(packet->view());
///     }
/// }
/// @endcode
class CaptureSession : public IFilterablePacketSource {
public:
    using Options = CaptureSessionOptions;

    /// @brief Packet callback type
    using PacketCallback = std::function<void(const PacketView&)>;

    /// @brief Create a capture session on the specified interface
    /// @param interface Network interface name (e.g., "eth0")
    /// @param options Capture options
    /// @return CaptureSession on success, error on failure
    static auto create(const std::string& interface,
                       Options options = Options{}) -> Result<CaptureSession>;

    CaptureSession(const CaptureSession&) = delete;
    CaptureSession& operator=(const CaptureSession&) = delete;
    CaptureSession(CaptureSession&&) noexcept;
    CaptureSession& operator=(CaptureSession&&) noexcept;
    ~CaptureSession() override;

    /// @brief Set a BPF filter (IFilterablePacketSource interface)
    /// @param expression BPF filter expression (e.g., "udp port 30490")
    /// @return Success or error
    auto set_filter(std::string_view expression) -> Result<void> override;

    /// @brief Set a BPF filter
    /// @param filter Compiled BPF filter
    /// @return Success or error
    auto set_filter(const FrameFilter& filter) -> Result<void>;

    /// @brief Start capturing (non-blocking mode)
    auto start() -> Result<void>;

    /// @brief Stop capturing
    void stop();

    /// @brief Check if capture is running
    [[nodiscard]] bool is_running() const;

    /// @brief Capture the next packet (blocking with timeout)
    /// @param timeout Maximum time to wait
    /// @return Packet if available, nullopt on timeout or error
    template <typename Rep, typename Period>
    [[nodiscard]] auto next_packet(std::chrono::duration<Rep, Period> timeout)
        -> std::optional<Packet> {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
        return next_packet_impl(static_cast<int>(ms.count()));
    }

    /// @brief Capture the next packet (IPacketSource interface)
    [[nodiscard]] auto next_packet() -> std::optional<Packet> override {
        return next_packet_impl(options_.timeout_ms);
    }

    /// @brief Check if more packets are available (IPacketSource interface)
    /// @note For live capture, this returns true if the session is running
    [[nodiscard]] bool has_more() const override { return running_; }

    /// @brief Get a description of this packet source (IPacketSource interface)
    [[nodiscard]] std::string description() const override {
        return "CaptureSession: " + interface_;
    }

    /// @brief Run capture loop with callback
    /// @param callback Function called for each packet
    /// @param max_packets Maximum packets to capture (0 = unlimited)
    /// @return Number of packets captured
    auto capture_loop(const PacketCallback& callback,
                      std::size_t max_packets = 0) -> std::size_t;

    /// @brief Get capture statistics
    [[nodiscard]] auto stats() const -> CaptureStats;

    /// @brief Get the interface name
    [[nodiscard]] const std::string& interface_name() const { return interface_; }

    /// @brief Get the socket file descriptor
    [[nodiscard]] int fd() const;

private:
    CaptureSession();

    auto next_packet_impl(int timeout_ms) -> std::optional<Packet>;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string interface_;
    Options options_;
    bool running_ = false;
};

}  // namespace wadjet::io

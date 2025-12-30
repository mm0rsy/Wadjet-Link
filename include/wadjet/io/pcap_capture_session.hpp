#pragma once

/// @file pcap_capture_session.hpp
/// @brief Packet capture using libpcap backend

#include "wadjet/core/packet_source.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/io/frame_filter.hpp"
#include "wadjet/net/packet.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace wadjet::io {

/// @brief Capture statistics for libpcap backend
struct PcapCaptureStats {
    std::uint64_t packets_received = 0;   ///< Packets received
    std::uint64_t packets_dropped = 0;    ///< Packets dropped (kernel)
    std::uint64_t packets_if_dropped = 0; ///< Packets dropped by interface
    std::uint64_t bytes_received = 0;     ///< Total bytes received
};

/// @brief Capture options for libpcap backend
struct PcapCaptureOptions {
    std::uint32_t snaplen = 65535;         ///< Max bytes to capture per packet
    bool promiscuous = true;               ///< Enable promiscuous mode
    bool immediate_mode = true;            ///< Minimize latency (pcap_set_immediate_mode)
    int timeout_ms = 100;                  ///< Read timeout in milliseconds
    std::size_t buffer_size = 2 * 1024 * 1024;  ///< Buffer size
    bool timestamp_nano = true;            ///< Use nanosecond timestamps if available
};

/// @brief Packet capture session using libpcap
///
/// PcapCaptureSession provides packet capture using the libpcap library,
/// offering cross-platform compatibility and access to libpcap-specific features.
///
/// @code
/// auto session = PcapCaptureSession::create("eth0");
/// if (session) {
///     session->set_filter("udp port 30490");
///     while (auto packet = session->next_packet(100ms)) {
///         process(packet->view());
///     }
/// }
/// @endcode
class PcapCaptureSession : public IFilterablePacketSource {
public:
    using Options = PcapCaptureOptions;

    /// @brief Packet callback type
    using PacketCallback = std::function<void(const PacketView&)>;

    /// @brief Create a capture session on the specified interface
    /// @param interface Network interface name (e.g., "eth0")
    /// @param options Capture options
    /// @return PcapCaptureSession on success, error on failure
    static auto create(const std::string& interface,
                       Options options = Options{}) -> Result<PcapCaptureSession>;

    /// @brief Open a pcap file for reading (offline capture)
    /// @param path Path to pcap file
    /// @return PcapCaptureSession on success, error on failure
    static auto open_offline(const std::filesystem::path& path) -> Result<PcapCaptureSession>;

    PcapCaptureSession(const PcapCaptureSession&) = delete;
    PcapCaptureSession& operator=(const PcapCaptureSession&) = delete;
    PcapCaptureSession(PcapCaptureSession&&) noexcept;
    PcapCaptureSession& operator=(PcapCaptureSession&&) noexcept;
    ~PcapCaptureSession() override;

    /// @brief Set a BPF filter (IFilterablePacketSource interface)
    /// @param expression BPF filter expression (e.g., "udp port 30490")
    /// @return Success or error
    auto set_filter(std::string_view expression) -> Result<void> override;

    /// @brief Start capturing (activates the pcap handle)
    auto start() -> Result<void>;

    /// @brief Stop capturing (breaks pcap_loop)
    void stop();

    /// @brief Check if capture is running
    [[nodiscard]] bool is_running() const { return running_; }

    /// @brief Capture the next packet (blocking with timeout)
    /// @param timeout Maximum time to wait
    /// @return Packet if available, nullopt on timeout or error
    template <typename Rep, typename Period>
    [[nodiscard]] auto next_packet(std::chrono::duration<Rep, Period> timeout)
        -> std::optional<Packet> {
        (void)timeout;  // Timeout is set in options
        return next_packet_impl();
    }

    /// @brief Capture the next packet (IPacketSource interface)
    [[nodiscard]] auto next_packet() -> std::optional<Packet> override {
        return next_packet_impl();
    }

    /// @brief Check if more packets are available (IPacketSource interface)
    [[nodiscard]] bool has_more() const override { return running_ || is_offline_; }

    /// @brief Get a description of this packet source (IPacketSource interface)
    [[nodiscard]] std::string description() const override {
        return "PcapCaptureSession: " + interface_;
    }

    /// @brief Run capture loop with callback
    /// @param callback Function called for each packet
    /// @param max_packets Maximum packets to capture (0 = unlimited)
    /// @return Number of packets captured
    auto capture_loop(const PacketCallback& callback,
                      std::size_t max_packets = 0) -> std::size_t;

    /// @brief Get capture statistics
    [[nodiscard]] auto stats() const -> PcapCaptureStats;

    /// @brief Get the interface name
    [[nodiscard]] const std::string& interface_name() const { return interface_; }

    /// @brief Get the libpcap handle (for advanced usage)
    [[nodiscard]] void* pcap_handle() const;

    /// @brief Get the data link type
    [[nodiscard]] int datalink() const;

    /// @brief Inject a packet (requires appropriate permissions)
    /// @param data Packet data to inject
    /// @return Success or error
    auto inject(const PacketView& data) -> Result<void>;

    /// @brief Get list of available capture devices
    static auto list_devices() -> Result<std::vector<std::string>>;

private:
    PcapCaptureSession();

    auto next_packet_impl() -> std::optional<Packet>;
    auto activate() -> Result<void>;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string interface_;
    Options options_;
    bool running_ = false;
    bool activated_ = false;
    bool is_offline_ = false;
    mutable PcapCaptureStats cached_stats_{};
};

}  // namespace wadjet::io

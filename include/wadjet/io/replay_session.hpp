#pragma once

/// @file replay_session.hpp
/// @brief Packet replay session using AF_PACKET TX

#include "wadjet/core/packet_source.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/pcap/pcap_reader.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::io {

/// @brief Replay timing mode
enum class ReplayTiming {
    AsRecorded,     ///< Preserve original inter-packet timing
    Immediate,      ///< Send packets as fast as possible
    FixedRate,      ///< Send at a fixed packets-per-second rate
    Scaled,         ///< Scale timing by a factor (0.5 = half speed, 2.0 = double speed)
};

/// @brief Replay session statistics
struct ReplayStats {
    std::uint64_t packets_sent = 0;       ///< Packets successfully sent
    std::uint64_t packets_failed = 0;     ///< Packets that failed to send
    std::uint64_t bytes_sent = 0;         ///< Total bytes sent
    std::uint64_t iterations = 0;         ///< Number of loop iterations completed
    std::chrono::nanoseconds total_time{0}; ///< Total replay time
};

/// @brief Replay session options
struct ReplaySessionOptions {
    ReplayTiming timing = ReplayTiming::AsRecorded;  ///< Timing mode
    double speed_factor = 1.0;                       ///< Speed multiplier (for Scaled mode)
    std::uint32_t packets_per_second = 1000;         ///< Rate for FixedRate mode
    bool loop = false;                               ///< Loop the replay
    std::size_t max_iterations = 0;                  ///< Max iterations (0 = unlimited)
    std::size_t max_packets = 0;                     ///< Max packets to send (0 = unlimited)
    bool promiscuous = false;                        ///< Enable promiscuous mode
};

/// @brief Packet modification callback
/// @param packet Mutable packet data
/// @return true to send packet, false to skip
using PacketModifier = std::function<bool(Packet&)>;

/// @brief Progress callback (called periodically during replay)
/// @param stats Current statistics
/// @return true to continue, false to stop
using ReplayProgressCallback = std::function<bool(const ReplayStats&)>;

/// @brief Packet replay session using Linux AF_PACKET
///
/// ReplaySession sends packets from a PCAP file or packet source
/// to a network interface, with configurable timing and rate control.
///
/// @code
/// // Replay a PCAP file
/// auto replay = ReplaySession::create("eth0", "capture.pcap");
/// if (replay) {
///     replay->run();
///     auto stats = replay->stats();
///     std::cout << "Sent " << stats.packets_sent << " packets\n";
/// }
/// @endcode
///
/// @code
/// // Replay with modification
/// replay->set_modifier([](Packet& pkt) {
///     // Modify source MAC
///     auto data = pkt.mutable_data();
///     data[6] = std::byte{0x02};  // Change source MAC
///     return true;
/// });
/// replay->run();
/// @endcode
class ReplaySession {
public:
    using Options = ReplaySessionOptions;

    /// @brief Create a replay session from a PCAP file
    /// @param interface Network interface to send packets on
    /// @param pcap_path Path to PCAP file
    /// @param options Replay options
    /// @return ReplaySession on success, error on failure
    static auto create(const std::string& interface,
                       const std::filesystem::path& pcap_path,
                       Options options = Options{}) -> Result<ReplaySession>;

    /// @brief Create a replay session from a packet source
    /// @param interface Network interface to send packets on
    /// @param source Packet source (takes ownership)
    /// @param options Replay options
    /// @return ReplaySession on success, error on failure
    static auto create(const std::string& interface,
                       PacketSourcePtr source,
                       Options options = Options{}) -> Result<ReplaySession>;

    /// @brief Create a replay session from pre-loaded packets
    /// @param interface Network interface to send packets on
    /// @param packets Vector of packets to replay
    /// @param options Replay options
    /// @return ReplaySession on success, error on failure
    static auto create(const std::string& interface,
                       std::vector<Packet> packets,
                       Options options = Options{}) -> Result<ReplaySession>;

    ReplaySession(const ReplaySession&) = delete;
    ReplaySession& operator=(const ReplaySession&) = delete;
    ReplaySession(ReplaySession&&) noexcept;
    ReplaySession& operator=(ReplaySession&&) noexcept;
    ~ReplaySession();

    /// @brief Set a packet modifier callback
    /// @param modifier Function to modify packets before sending
    void set_modifier(PacketModifier modifier) { modifier_ = std::move(modifier); }

    /// @brief Set a progress callback
    /// @param callback Function called during replay
    /// @param interval How often to call (in packets)
    void set_progress_callback(ReplayProgressCallback callback,
                               std::size_t interval = 100) {
        progress_callback_ = std::move(callback);
        progress_interval_ = interval;
    }

    /// @brief Run the replay synchronously
    /// @return Success or error
    auto run() -> Result<void>;

    /// @brief Stop a running replay
    void stop();

    /// @brief Check if replay is running
    [[nodiscard]] bool is_running() const { return running_; }

    /// @brief Get replay statistics
    [[nodiscard]] auto stats() const -> ReplayStats { return stats_; }

    /// @brief Get the interface name
    [[nodiscard]] const std::string& interface_name() const { return interface_; }

    /// @brief Get number of packets loaded
    [[nodiscard]] std::size_t packet_count() const { return packets_.size(); }

    /// @brief Send a single packet immediately
    /// @param packet Packet to send
    /// @return Success or error
    auto send_packet(const Packet& packet) -> Result<void>;

    /// @brief Send a single packet immediately
    /// @param view Packet view to send
    /// @return Success or error
    auto send_packet(const PacketView& view) -> Result<void>;

private:
    ReplaySession();

    /// @brief Initialize the AF_PACKET TX socket
    auto init_socket() -> Result<void>;

    /// @brief Load packets from source
    auto load_packets(PacketSourcePtr source) -> Result<void>;

    /// @brief Wait for the appropriate time before sending next packet
    void wait_for_timing(const Timestamp& current_ts, const Timestamp& prev_ts);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string interface_;
    Options options_;
    std::vector<Packet> packets_;
    PacketModifier modifier_;
    ReplayProgressCallback progress_callback_;
    std::size_t progress_interval_ = 100;
    ReplayStats stats_{};
    bool running_ = false;
};

}  // namespace wadjet::io

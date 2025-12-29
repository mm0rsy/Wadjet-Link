#pragma once

/// @file packet_source.hpp
/// @brief Abstract interface for packet sources

#include "wadjet/core/result.hpp"
#include "wadjet/core/timestamp.hpp"
#include "wadjet/net/packet.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace wadjet {

/// @brief Abstract interface for packet sources
///
/// Provides a common interface for any type that can produce packets,
/// whether from files (PCAP), live capture, or synthetic generation.
/// This enables polymorphic usage and simplifies testing.
///
/// @code
/// void process_packets(IPacketSource& source) {
///     while (auto pkt = source.next_packet()) {
///         analyze(pkt->view());
///     }
/// }
/// @endcode
class IPacketSource {
public:
    virtual ~IPacketSource() = default;

    /// @brief Read the next packet
    /// @return Packet if available, nullopt at end/timeout
    [[nodiscard]] virtual auto next_packet() -> std::optional<Packet> = 0;

    /// @brief Check if more packets may be available
    [[nodiscard]] virtual bool has_more() const = 0;

    /// @brief Get a description of the source
    [[nodiscard]] virtual std::string description() const = 0;

    // Prevent copying, allow moving through unique_ptr
    IPacketSource() = default;
    IPacketSource(const IPacketSource&) = delete;
    IPacketSource& operator=(const IPacketSource&) = delete;
    IPacketSource(IPacketSource&&) = default;
    IPacketSource& operator=(IPacketSource&&) = default;
};

/// @brief Unique pointer to packet source (for polymorphism)
using PacketSourcePtr = std::unique_ptr<IPacketSource>;

/// @brief Abstract interface for filtered packet sources
class IFilterablePacketSource : public IPacketSource {
public:
    /// @brief Set a BPF filter expression
    /// @param expression BPF filter (e.g., "udp port 30490")
    /// @return Success or error
    [[nodiscard]] virtual auto set_filter(std::string_view expression)
        -> Result<void> = 0;
};

/// @brief Abstract interface for packet sinks (writers)
class IPacketSink {
public:
    virtual ~IPacketSink() = default;

    /// @brief Write a packet
    /// @param view Packet data to write
    /// @return Success or error
    [[nodiscard]] virtual auto write_packet(const PacketView& view)
        -> Result<void> = 0;

    /// @brief Flush any buffered data (default: no-op)
    virtual void flush() {}

    /// @brief Get number of packets written (default: 0, override for tracking)
    [[nodiscard]] virtual std::size_t packet_count() const { return 0; }

    /// @brief Get a description of the sink
    [[nodiscard]] virtual std::string description() const { return "IPacketSink"; }

    // Prevent copying
    IPacketSink() = default;
    IPacketSink(const IPacketSink&) = delete;
    IPacketSink& operator=(const IPacketSink&) = delete;
    IPacketSink(IPacketSink&&) = default;
    IPacketSink& operator=(IPacketSink&&) = default;
};

/// @brief Unique pointer to packet sink
using PacketSinkPtr = std::unique_ptr<IPacketSink>;

/// @brief Utility to copy packets between source and sink
///
/// @param source Packet source
/// @param sink Packet destination
/// @param max_packets Maximum packets to copy (0 = unlimited)
/// @return Number of packets copied
inline std::size_t copy_packets(IPacketSource& source,
                                IPacketSink& sink,
                                std::size_t max_packets = 0) {
    std::size_t count = 0;
    while (auto pkt = source.next_packet()) {
        if (auto result = sink.write_packet(pkt->view()); !result) {
            break;
        }
        ++count;
        if (max_packets > 0 && count >= max_packets) {
            break;
        }
    }
    sink.flush();
    return count;
}

/// @brief Filter packets through a predicate
///
/// @tparam Predicate Callable with signature bool(const PacketView&)
/// @param source Packet source
/// @param pred Filter predicate
/// @param callback Function called for matching packets
/// @return Number of matching packets
template <typename Predicate, typename Callback>
std::size_t filter_packets(IPacketSource& source,
                           Predicate&& pred,
                           Callback&& callback) {
    std::size_t count = 0;
    while (auto pkt = source.next_packet()) {
        if (pred(pkt->view())) {
            callback(pkt->view());
            ++count;
        }
    }
    return count;
}

}  // namespace wadjet

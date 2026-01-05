#pragma once

/// @file packet.hpp
/// @brief Mutable packet buffer with ownership

#include "wadjet/core/timestamp.hpp"
#include "wadjet/core/types.hpp"
#include "wadjet/net/packet_view.hpp"

#include <cstring>
#include <vector>

namespace wadjet {

/// @brief Mutable packet buffer that owns its data
///
/// Packet provides an owning container for packet data, useful for:
/// - Constructing packets for transmission
/// - Storing captured packets
/// - Modifying packet contents
///
/// @code
/// Packet pkt(1500);
/// pkt.resize(64);
/// auto view = pkt.view();
/// @endcode
class Packet {
public:
    /// @brief Default constructor (empty packet)
    Packet() = default;

    /// @brief Construct with reserved capacity
    explicit Packet(std::size_t capacity) { data_.reserve(capacity); }

    /// @brief Construct from raw data (copies data)
    explicit Packet(ByteSpan data, Timestamp ts = {})
        : data_(data.begin(), data.end()), timestamp_(ts) {}

    /// @brief Construct from uint8_t span (copies data)
    explicit Packet(std::span<const std::uint8_t> data, Timestamp ts = {})
        : data_(reinterpret_cast<const std::byte*>(data.data()),
                reinterpret_cast<const std::byte*>(data.data() + data.size())),
          timestamp_(ts) {}

    /// @brief Construct from PacketView (copies data)
    explicit Packet(const PacketView& view)
        : data_(view.data().begin(), view.data().end()), timestamp_(view.timestamp()) {}

    /// @brief Get raw data as span
    [[nodiscard]] ByteSpan data() const { return ByteSpan(data_.data(), data_.size()); }

    /// @brief Get mutable raw data
    [[nodiscard]] MutableByteSpan mutable_data() {
        return MutableByteSpan(data_.data(), data_.size());
    }

    /// @brief Get packet size
    [[nodiscard]] std::size_t size() const { return data_.size(); }

    /// @brief Get buffer capacity
    [[nodiscard]] std::size_t capacity() const { return data_.capacity(); }

    /// @brief Check if empty
    [[nodiscard]] bool empty() const { return data_.empty(); }

    /// @brief Get timestamp
    [[nodiscard]] Timestamp timestamp() const { return timestamp_; }

    /// @brief Set timestamp
    void set_timestamp(Timestamp ts) { timestamp_ = ts; }

    /// @brief Create an immutable view of this packet
    [[nodiscard]] PacketView view() const { return PacketView(data(), timestamp_); }

    /// @brief Resize packet data
    void resize(std::size_t new_size) { data_.resize(new_size); }

    /// @brief Reserve capacity
    void reserve(std::size_t new_capacity) { data_.reserve(new_capacity); }

    /// @brief Clear packet data
    void clear() { data_.clear(); }

    /// @brief Append data to packet
    void append(ByteSpan data) { data_.insert(data_.end(), data.begin(), data.end()); }

    /// @brief Append single byte
    void push_back(std::byte b) { data_.push_back(b); }

    /// @brief Access byte at index
    [[nodiscard]] std::byte& operator[](std::size_t index) { return data_[index]; }
    [[nodiscard]] const std::byte& operator[](std::size_t index) const { return data_[index]; }

    /// @brief Get pointer to data
    [[nodiscard]] std::byte* begin() { return data_.data(); }
    [[nodiscard]] const std::byte* begin() const { return data_.data(); }
    [[nodiscard]] std::byte* end() { return data_.data() + data_.size(); }
    [[nodiscard]] const std::byte* end() const { return data_.data() + data_.size(); }

    /// @brief Swap contents with another packet
    void swap(Packet& other) noexcept {
        data_.swap(other.data_);
        std::swap(timestamp_, other.timestamp_);
    }

private:
    std::vector<std::byte> data_;
    Timestamp timestamp_;
};

}  // namespace wadjet

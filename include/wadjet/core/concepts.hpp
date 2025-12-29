#pragma once

/// @file concepts.hpp
/// @brief C++20 concepts for type constraints

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace wadjet {

// Forward declarations
class PacketView;
class Packet;

/// @brief Concept for types that can provide a PacketView
template <typename T>
concept PacketViewable = requires(const T& t) {
    { t.view() } -> std::convertible_to<PacketView>;
};

/// @brief Concept for types that can provide raw byte data
template <typename T>
concept ByteDataProvider = requires(const T& t) {
    { t.data() } -> std::convertible_to<std::span<const std::byte>>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

/// @brief Concept for types that represent network addresses
template <typename T>
concept NetworkAddress = requires(T t, const T& ct) {
    { T::from_string(std::string_view{}) } -> std::same_as<T>;
    { ct.to_string() } -> std::convertible_to<std::string>;
    { ct.bytes } -> std::convertible_to<decltype(ct.bytes)>;
    requires std::equality_comparable<T>;
};

/// @brief Concept for packet source types (PCAP reader, capture session, etc.)
template <typename T>
concept PacketSource = requires(T& t) {
    { t.next_packet() } -> std::same_as<std::optional<Packet>>;
};

/// @brief Concept for types that can be serialized to bytes
template <typename T>
concept ByteSerializable = requires(const T& t, std::span<std::byte> buffer) {
    { t.serialize(buffer) } -> std::convertible_to<std::size_t>;
    { T::serialized_size() } -> std::convertible_to<std::size_t>;
};

}  // namespace wadjet

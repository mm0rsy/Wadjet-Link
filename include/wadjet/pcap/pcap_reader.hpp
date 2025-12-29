#pragma once

/// @file pcap_reader.hpp
/// @brief PCAP file reader

#include "wadjet/core/packet_source.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/pcap/pcap_file.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::pcap {

/// @brief PCAP file reader
///
/// Reads packets from standard PCAP files (libpcap format).
/// Supports both microsecond and nanosecond timestamp resolution.
/// Implements IPacketSource for generic packet processing.
///
/// @code
/// auto reader = PcapReader::open("capture.pcap");
/// if (reader) {
///     while (auto packet = reader->next_packet()) {
///         process(packet->view());
///     }
/// }
/// @endcode
class PcapReader : public IPacketSource {
public:
    /// @brief Open a PCAP file for reading
    /// @param path Path to PCAP file
    /// @return PcapReader on success, error on failure
    static auto open(const std::filesystem::path& path) -> Result<PcapReader>;

    /// @brief Open a PCAP file from memory
    /// @param data PCAP file contents
    /// @return PcapReader on success, error on failure
    static auto from_memory(std::vector<std::byte> data) -> Result<PcapReader>;

    PcapReader(const PcapReader&) = delete;
    PcapReader& operator=(const PcapReader&) = delete;
    PcapReader(PcapReader&&) noexcept = default;
    PcapReader& operator=(PcapReader&&) noexcept = default;
    ~PcapReader() override = default;

    /// @brief Read the next packet (IPacketSource interface)
    /// @return Packet if available, nullopt at end of file
    [[nodiscard]] auto next_packet() -> std::optional<Packet> override;

    /// @brief Check if more packets are available (IPacketSource interface)
    [[nodiscard]] bool has_more() const override { return !eof_; }

    /// @brief Get a description of this packet source (IPacketSource interface)
    [[nodiscard]] std::string description() const override { return description_; }

    /// @brief Get the link layer type
    [[nodiscard]] LinkType link_type() const { return link_type_; }

    /// @brief Get snap length (max packet size)
    [[nodiscard]] std::uint32_t snaplen() const { return snaplen_; }

    /// @brief Check if timestamps are in nanoseconds
    [[nodiscard]] bool is_nanosecond() const { return is_nanosecond_; }

    /// @brief Check if byte order is swapped
    [[nodiscard]] bool is_swapped() const { return is_swapped_; }

    /// @brief Read all packets into a vector
    [[nodiscard]] auto read_all() -> std::vector<Packet>;

    /// @brief Get current file position
    [[nodiscard]] std::streampos position() const;

    /// @brief Reset to beginning of file (after header)
    void reset();

private:
    PcapReader() = default;

    std::ifstream file_;
    std::vector<std::byte> memory_data_;
    std::size_t memory_offset_ = 0;
    bool from_memory_ = false;
    bool eof_ = false;
    std::string description_ = "PcapReader";

    LinkType link_type_ = LinkType::Ethernet;
    std::uint32_t snaplen_ = 65535;
    bool is_nanosecond_ = false;
    bool is_swapped_ = false;
    std::streampos data_start_pos_ = 0;

    auto read_bytes(void* dest, std::size_t size) -> bool;
    auto read_header() -> Result<void>;
};

}  // namespace wadjet::pcap

#pragma once

/// @file pcap_writer.hpp
/// @brief PCAP file writer

#include "wadjet/core/result.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/net/packet_view.hpp"
#include "wadjet/pcap/pcap_file.hpp"

#include <filesystem>
#include <fstream>

namespace wadjet::pcap {

/// @brief PCAP writer options
struct PcapWriterOptions {
    LinkType link_type = LinkType::Ethernet;
    std::uint32_t snaplen = 65535;
    bool nanosecond_precision = false;
};

/// @brief PCAP file writer
///
/// Writes packets to standard PCAP files (libpcap format).
///
/// @code
/// auto writer = PcapWriter::create("output.pcap");
/// if (writer) {
///     writer->write_packet(packet.view());
/// }
/// @endcode
class PcapWriter {
public:
    using Options = PcapWriterOptions;

    /// @brief Create a new PCAP file for writing
    /// @param path Path to output file
    /// @param options Writer options
    /// @return PcapWriter on success, error on failure
    static auto create(const std::filesystem::path& path,
                       Options options = Options{}) -> Result<PcapWriter>;

    PcapWriter(const PcapWriter&) = delete;
    PcapWriter& operator=(const PcapWriter&) = delete;
    PcapWriter(PcapWriter&&) noexcept = default;
    PcapWriter& operator=(PcapWriter&&) noexcept = default;
    ~PcapWriter();

    /// @brief Write a packet to the file
    /// @param view Packet view to write
    /// @return Success or error
    auto write_packet(const PacketView& view) -> Result<void>;

    /// @brief Write a packet to the file
    /// @param packet Packet to write
    /// @return Success or error
    auto write_packet(const Packet& packet) -> Result<void> {
        return write_packet(packet.view());
    }

    /// @brief Flush buffered data to disk
    void flush();

    /// @brief Get number of packets written
    [[nodiscard]] std::size_t packet_count() const { return packet_count_; }

    /// @brief Get total bytes written (excluding header)
    [[nodiscard]] std::size_t bytes_written() const { return bytes_written_; }

private:
    PcapWriter() = default;

    std::ofstream file_;
    Options options_;
    std::size_t packet_count_ = 0;
    std::size_t bytes_written_ = 0;
};

}  // namespace wadjet::pcap

#pragma once

/// @file pcapng_writer.hpp
/// @brief PCAPNG file writer

#include "wadjet/core/packet_source.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/net/packet_view.hpp"
#include "wadjet/pcap/pcap_file.hpp"
#include "wadjet/pcap/pcapng_file.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace wadjet::pcap {

/// @brief Interface metadata for PCAPNG
struct InterfaceInfo {
    std::string name;                         ///< Interface name (e.g., "eth0")
    std::string description;                  ///< Interface description
    LinkType link_type = LinkType::Ethernet;  ///< Link layer type
    std::uint32_t snap_len = 65535;           ///< Snapshot length
    std::uint64_t speed = 0;                  ///< Interface speed in bits/sec (0 = unknown)
    bool use_nanoseconds = true;              ///< Use nanosecond timestamp resolution
};

/// @brief PCAPNG writer options
struct PcapngWriterOptions {
    std::string hardware;          ///< Hardware description (optional)
    std::string os;                ///< Operating system (optional)
    std::string user_application;  ///< User application name
    std::string comment;           ///< Section comment
    bool use_nanoseconds = true;   ///< Default timestamp resolution
};

/// @brief PCAPNG file writer
///
/// Writes packets to PCAPNG files with support for:
/// - Multiple interfaces
/// - Enhanced Packet Blocks (EPB) with nanosecond timestamps
/// - Interface and section metadata
/// - Comments at section, interface, and packet level
/// - Interface statistics blocks
///
/// @code
/// PcapngWriterOptions opts;
/// opts.user_application = "Wadjet-Link";
///
/// auto writer = PcapngWriter::create("output.pcapng", opts);
/// if (writer) {
///     // Add interface
///     InterfaceInfo iface;
///     iface.name = "eth0";
///     writer->add_interface(iface);
///
///     // Write packets
///     writer->write_packet(packet.view());
/// }
/// @endcode
class PcapngWriter : public IPacketSink {
public:
    using Options = PcapngWriterOptions;

    /// @brief Create a new PCAPNG file for writing
    /// @param path Path to output file
    /// @param options Writer options
    /// @return PcapngWriter on success, error on failure
    static auto create(const std::filesystem::path& path,
                       Options options = Options{}) -> Result<PcapngWriter>;

    PcapngWriter(const PcapngWriter&) = delete;
    PcapngWriter& operator=(const PcapngWriter&) = delete;
    PcapngWriter(PcapngWriter&&) noexcept = default;
    PcapngWriter& operator=(PcapngWriter&&) noexcept = default;
    ~PcapngWriter() override;

    /// @brief Add an interface to the capture
    /// @param info Interface information
    /// @return Interface ID (0-based index)
    auto add_interface(const InterfaceInfo& info) -> std::uint32_t;

    /// @brief Write a packet to the file (IPacketSink interface)
    /// @param view Packet view to write
    /// @param interface_id Interface ID (default 0)
    /// @return Success or error
    auto write_packet(const PacketView& view) -> Result<void> override;

    /// @brief Write a packet with specific interface
    /// @param view Packet view to write
    /// @param interface_id Interface ID
    /// @return Success or error
    auto write_packet(const PacketView& view, std::uint32_t interface_id) -> Result<void>;

    /// @brief Write a packet with comment
    /// @param view Packet view to write
    /// @param interface_id Interface ID
    /// @param comment Packet comment
    /// @return Success or error
    auto write_packet(const PacketView& view, std::uint32_t interface_id,
                      const std::string& comment) -> Result<void>;

    /// @brief Write a packet from Packet object
    /// @param packet Packet to write
    /// @param interface_id Interface ID (default 0)
    /// @return Success or error
    auto write_packet(const Packet& packet, std::uint32_t interface_id = 0) -> Result<void> {
        return write_packet(packet.view(), interface_id);
    }

    /// @brief Write interface statistics block
    /// @param interface_id Interface ID
    /// @param packets_received Total packets received
    /// @param packets_dropped Total packets dropped
    /// @return Success or error
    auto write_statistics(std::uint32_t interface_id, std::uint64_t packets_received,
                          std::uint64_t packets_dropped) -> Result<void>;

    /// @brief Get a description of this packet sink (IPacketSink interface)
    [[nodiscard]] std::string description() const override { return description_; }

    /// @brief Flush buffered data to disk (IPacketSink interface)
    void flush() override;

    /// @brief Get number of packets written (IPacketSink interface)
    [[nodiscard]] std::size_t packet_count() const override { return packet_count_; }

    /// @brief Get total bytes written
    [[nodiscard]] std::size_t bytes_written() const { return bytes_written_; }

    /// @brief Get number of interfaces added
    [[nodiscard]] std::size_t interface_count() const { return interfaces_.size(); }

private:
    PcapngWriter() = default;

    /// @brief Write the Section Header Block
    auto write_section_header() -> Result<void>;

    /// @brief Write an Interface Description Block
    auto write_interface_block(const InterfaceInfo& info) -> Result<void>;

    /// @brief Write Enhanced Packet Block
    auto write_enhanced_packet_block(const PacketView& view, std::uint32_t interface_id,
                                     const std::string* comment = nullptr) -> Result<void>;

    /// @brief Write options to file
    void write_options(
        const std::vector<std::pair<std::uint16_t, std::vector<std::byte>>>& options);

    /// @brief Write bytes with padding
    void write_padded(const void* data, std::size_t len);

    /// @brief Write a 32-bit value
    void write_u32(std::uint32_t value);

    /// @brief Write a 16-bit value
    void write_u16(std::uint16_t value);

    /// @brief Write a 64-bit value
    void write_u64(std::uint64_t value);

    std::ofstream file_;
    Options options_;
    std::vector<InterfaceInfo> interfaces_;
    std::size_t packet_count_ = 0;
    std::size_t bytes_written_ = 0;
    std::string description_ = "PcapngWriter";
};

}  // namespace wadjet::pcap

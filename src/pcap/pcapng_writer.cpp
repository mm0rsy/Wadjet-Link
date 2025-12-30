#include "wadjet/pcap/pcapng_writer.hpp"

#include <cstring>

#ifdef __linux__
#include <sys/utsname.h>
#endif

namespace wadjet::pcap {

namespace {

/// @brief Get current OS information
std::string get_os_info() {
#ifdef __linux__
    utsname info{};
    if (uname(&info) == 0) {
        return std::string(info.sysname) + " " + info.release;
    }
#endif
    return "Unknown";
}

/// @brief Convert string to vector of bytes
std::vector<std::byte> string_to_bytes(const std::string& str) {
    std::vector<std::byte> result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<std::byte>(c));
    }
    return result;
}

}  // namespace

auto PcapngWriter::create(const std::filesystem::path& path,
                          Options options) -> Result<PcapngWriter> {
    PcapngWriter writer;
    writer.options_ = options;
    writer.description_ = "PcapngWriter: " + path.string();

    // Fill in defaults
    if (writer.options_.os.empty()) {
        writer.options_.os = get_os_info();
    }
    if (writer.options_.user_application.empty()) {
        writer.options_.user_application = "Wadjet-Link";
    }

    writer.file_.open(path, std::ios::binary | std::ios::trunc);
    if (!writer.file_.is_open()) {
        return Result<PcapngWriter>::err(
            Error{-1, "Failed to create file: " + path.string()});
    }

    // Write Section Header Block
    auto result = writer.write_section_header();
    if (!result) {
        return Result<PcapngWriter>::err(std::move(result).error());
    }

    return Result<PcapngWriter>::ok(std::move(writer));
}

PcapngWriter::~PcapngWriter() {
    if (file_.is_open()) {
        flush();
    }
}

auto PcapngWriter::add_interface(const InterfaceInfo& info) -> std::uint32_t {
    auto id = static_cast<std::uint32_t>(interfaces_.size());
    interfaces_.push_back(info);
    
    // Write Interface Description Block
    auto result = write_interface_block(info);
    if (!result) {
        // Log error but continue - interface ID is still valid
    }
    
    return id;
}

auto PcapngWriter::write_packet(const PacketView& view) -> Result<void> {
    return write_packet(view, 0);
}

auto PcapngWriter::write_packet(const PacketView& view,
                                std::uint32_t interface_id) -> Result<void> {
    return write_enhanced_packet_block(view, interface_id, nullptr);
}

auto PcapngWriter::write_packet(const PacketView& view, std::uint32_t interface_id,
                                const std::string& comment) -> Result<void> {
    return write_enhanced_packet_block(view, interface_id, &comment);
}

auto PcapngWriter::write_statistics(std::uint32_t interface_id,
                                    std::uint64_t packets_received,
                                    std::uint64_t packets_dropped) -> Result<void> {
    if (!file_.is_open()) {
        return Result<void>::err(Error{-1, "File not open"});
    }

    // Build options
    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> options;
    
    // ISB_IFRecv (packets received)
    {
        std::vector<std::byte> value(8);
        std::memcpy(value.data(), &packets_received, 8);
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::ISB_IFRecv),
                            std::move(value));
    }
    
    // ISB_IFDrop (packets dropped)
    {
        std::vector<std::byte> value(8);
        std::memcpy(value.data(), &packets_dropped, 8);
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::ISB_IFDrop),
                            std::move(value));
    }

    // Calculate options size
    std::size_t options_size = 4;  // End-of-options
    for (const auto& opt : options) {
        options_size += 4 + opt.second.size() + pcapng_padding(static_cast<std::uint32_t>(opt.second.size()));
    }

    // Block size: header (20) + options + trailer (4)
    std::uint32_t block_len = static_cast<std::uint32_t>(20 + options_size + 4);

    // Get current timestamp
    auto now = Timestamp::now();
    std::uint64_t ts = static_cast<std::uint64_t>(now.total_nanoseconds());
    if (!options_.use_nanoseconds && interfaces_.empty()) {
        ts = static_cast<std::uint64_t>(now.total_microseconds());
    } else if (!interfaces_.empty() && !interfaces_[interface_id].use_nanoseconds) {
        ts = static_cast<std::uint64_t>(now.total_microseconds());
    }

    // Write ISB header
    write_u32(static_cast<std::uint32_t>(PcapngBlockType::InterfaceStatistics));
    write_u32(block_len);
    write_u32(interface_id);
    write_u32(static_cast<std::uint32_t>(ts >> 32));
    write_u32(static_cast<std::uint32_t>(ts & 0xFFFFFFFF));

    // Write options
    write_options(options);

    // Write block length trailer
    write_u32(block_len);

    bytes_written_ += block_len;

    if (!file_.good()) {
        return Result<void>::err(Error{-1, "Failed to write statistics block"});
    }

    return Result<void>::ok();
}

void PcapngWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

auto PcapngWriter::write_section_header() -> Result<void> {
    // Build options
    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> options;

    if (!options_.comment.empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::Comment),
                            string_to_bytes(options_.comment));
    }
    if (!options_.hardware.empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::SHB_Hardware),
                            string_to_bytes(options_.hardware));
    }
    if (!options_.os.empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::SHB_OS),
                            string_to_bytes(options_.os));
    }
    if (!options_.user_application.empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::SHB_UserAppl),
                            string_to_bytes(options_.user_application));
    }

    // Calculate options size
    std::size_t options_size = 4;  // End-of-options
    for (const auto& opt : options) {
        options_size += 4 + opt.second.size() + pcapng_padding(static_cast<std::uint32_t>(opt.second.size()));
    }

    // Block size: fixed header (24) + options + trailing block length (4)
    std::uint32_t block_len = static_cast<std::uint32_t>(24 + options_size + 4);

    // Write SHB fixed fields
    write_u32(static_cast<std::uint32_t>(PcapngBlockType::SectionHeader));
    write_u32(block_len);
    write_u32(PCAPNG_BYTE_ORDER_MAGIC);
    write_u16(1);   // Major version
    write_u16(0);   // Minor version
    write_u64(static_cast<std::uint64_t>(-1));  // Section length (unspecified)

    // Write options
    write_options(options);

    // Write block length trailer
    write_u32(block_len);

    bytes_written_ += block_len;

    if (!file_.good()) {
        return Result<void>::err(Error{-1, "Failed to write section header block"});
    }

    return Result<void>::ok();
}

auto PcapngWriter::write_interface_block(const InterfaceInfo& info) -> Result<void> {
    // Build options
    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> options;

    if (!info.name.empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::IDB_Name),
                            string_to_bytes(info.name));
    }
    if (!info.description.empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::IDB_Description),
                            string_to_bytes(info.description));
    }
    if (info.speed > 0) {
        std::vector<std::byte> value(8);
        std::memcpy(value.data(), &info.speed, 8);
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::IDB_Speed),
                            std::move(value));
    }

    // Timestamp resolution (if using nanoseconds)
    {
        std::vector<std::byte> value(1);
        value[0] = static_cast<std::byte>(info.use_nanoseconds ? PCAPNG_TSRESOL_NANO : PCAPNG_TSRESOL_MICRO);
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::IDB_TSResol),
                            std::move(value));
    }

    // Calculate options size
    std::size_t options_size = 4;  // End-of-options
    for (const auto& opt : options) {
        options_size += 4 + opt.second.size() + pcapng_padding(static_cast<std::uint32_t>(opt.second.size()));
    }

    // Block size: fixed header (16) + options + trailing block length (4)
    std::uint32_t block_len = static_cast<std::uint32_t>(16 + options_size + 4);

    // Write IDB fixed fields
    write_u32(static_cast<std::uint32_t>(PcapngBlockType::InterfaceDescription));
    write_u32(block_len);
    write_u16(static_cast<std::uint16_t>(info.link_type));
    write_u16(0);  // Reserved
    write_u32(info.snap_len);

    // Write options
    write_options(options);

    // Write block length trailer
    write_u32(block_len);

    bytes_written_ += block_len;

    if (!file_.good()) {
        return Result<void>::err(Error{-1, "Failed to write interface description block"});
    }

    return Result<void>::ok();
}

auto PcapngWriter::write_enhanced_packet_block(const PacketView& view,
                                               std::uint32_t interface_id,
                                               const std::string* comment) -> Result<void> {
    if (!file_.is_open()) {
        return Result<void>::err(Error{-1, "File not open"});
    }

    // Ensure we have at least one interface
    if (interfaces_.empty()) {
        // Add a default interface
        InterfaceInfo default_iface;
        default_iface.name = "default";
        default_iface.use_nanoseconds = options_.use_nanoseconds;
        add_interface(default_iface);
    }

    if (interface_id >= interfaces_.size()) {
        return Result<void>::err(Error{-1, "Invalid interface ID"});
    }

    auto ts = view.timestamp();
    auto data = view.data();
    const auto& iface = interfaces_[interface_id];

    // Convert timestamp to the appropriate resolution
    std::uint64_t timestamp;
    if (iface.use_nanoseconds) {
        timestamp = static_cast<std::uint64_t>(ts.total_nanoseconds());
    } else {
        timestamp = static_cast<std::uint64_t>(ts.total_microseconds());
    }

    // Truncate to snaplen if needed
    std::uint32_t captured_len = static_cast<std::uint32_t>(
        std::min(data.size(), static_cast<std::size_t>(iface.snap_len)));
    std::uint32_t original_len = static_cast<std::uint32_t>(data.size());
    std::uint32_t data_padding = pcapng_padding(captured_len);

    // Build options
    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> options;
    if (comment != nullptr && !comment->empty()) {
        options.emplace_back(static_cast<std::uint16_t>(PcapngOptionType::Comment),
                            string_to_bytes(*comment));
    }

    // Calculate options size
    std::size_t options_size = 4;  // End-of-options
    for (const auto& opt : options) {
        options_size += 4 + opt.second.size() + pcapng_padding(static_cast<std::uint32_t>(opt.second.size()));
    }

    // Block size: fixed header (28) + data + padding + options + trailing block length (4)
    std::uint32_t block_len = static_cast<std::uint32_t>(
        28 + captured_len + data_padding + options_size + 4);

    // Write EPB fixed fields
    write_u32(static_cast<std::uint32_t>(PcapngBlockType::EnhancedPacket));
    write_u32(block_len);
    write_u32(interface_id);
    write_u32(static_cast<std::uint32_t>(timestamp >> 32));
    write_u32(static_cast<std::uint32_t>(timestamp & 0xFFFFFFFF));
    write_u32(captured_len);
    write_u32(original_len);

    // Write packet data with padding
    file_.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(captured_len));
    if (data_padding > 0) {
        std::byte pad[4] = {};
        file_.write(reinterpret_cast<const char*>(pad),
                   static_cast<std::streamsize>(data_padding));
    }

    // Write options
    write_options(options);

    // Write block length trailer
    write_u32(block_len);

    packet_count_++;
    bytes_written_ += block_len;

    if (!file_.good()) {
        return Result<void>::err(Error{-1, "Failed to write packet block"});
    }

    return Result<void>::ok();
}

void PcapngWriter::write_options(
    const std::vector<std::pair<std::uint16_t, std::vector<std::byte>>>& options) {
    for (const auto& opt : options) {
        write_u16(opt.first);
        write_u16(static_cast<std::uint16_t>(opt.second.size()));
        if (!opt.second.empty()) {
            write_padded(opt.second.data(), opt.second.size());
        }
    }

    // End-of-options
    write_u16(0);
    write_u16(0);
}

void PcapngWriter::write_padded(const void* data, std::size_t len) {
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    std::uint32_t padding = pcapng_padding(static_cast<std::uint32_t>(len));
    if (padding > 0) {
        std::byte pad[4] = {};
        file_.write(reinterpret_cast<const char*>(pad), static_cast<std::streamsize>(padding));
    }
}

void PcapngWriter::write_u32(std::uint32_t value) {
    file_.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void PcapngWriter::write_u16(std::uint16_t value) {
    file_.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void PcapngWriter::write_u64(std::uint64_t value) {
    file_.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

}  // namespace wadjet::pcap

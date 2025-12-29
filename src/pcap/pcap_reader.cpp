#include "wadjet/pcap/pcap_reader.hpp"

#include <cstring>

namespace wadjet::pcap {

auto PcapReader::open(const std::filesystem::path& path) -> Result<PcapReader> {
    PcapReader reader;
    reader.from_memory_ = false;

    reader.file_.open(path, std::ios::binary);
    if (!reader.file_.is_open()) {
        return Result<PcapReader>::err(
            Error{-1, "Failed to open file: " + path.string()});
    }

    auto result = reader.read_header();
    if (!result) {
        return Result<PcapReader>::err(std::move(result).error());
    }

    reader.data_start_pos_ = reader.file_.tellg();
    return Result<PcapReader>::ok(std::move(reader));
}

auto PcapReader::from_memory(std::vector<std::byte> data) -> Result<PcapReader> {
    PcapReader reader;
    reader.from_memory_ = true;
    reader.memory_data_ = std::move(data);
    reader.memory_offset_ = 0;

    auto result = reader.read_header();
    if (!result) {
        return Result<PcapReader>::err(std::move(result).error());
    }

    return Result<PcapReader>::ok(std::move(reader));
}

auto PcapReader::read_header() -> Result<void> {
    PcapFileHeader header{};
    if (!read_bytes(&header, sizeof(header))) {
        return Result<void>::err(Error{-1, "Failed to read PCAP header"});
    }

    // Check magic number
    switch (header.magic_number) {
        case PCAP_MAGIC_NATIVE:
            is_swapped_ = false;
            is_nanosecond_ = false;
            break;
        case PCAP_MAGIC_SWAPPED:
            is_swapped_ = true;
            is_nanosecond_ = false;
            break;
        case PCAP_MAGIC_NSEC_NATIVE:
            is_swapped_ = false;
            is_nanosecond_ = true;
            break;
        case PCAP_MAGIC_NSEC_SWAPPED:
            is_swapped_ = true;
            is_nanosecond_ = true;
            break;
        default:
            return Result<void>::err(Error{-1, "Invalid PCAP magic number"});
    }

    // Handle byte swapping if needed
    if (is_swapped_) {
        auto swap16 = [](std::uint16_t v) -> std::uint16_t {
            return static_cast<std::uint16_t>((v >> 8) | (v << 8));
        };
        auto swap32 = [](std::uint32_t v) -> std::uint32_t {
            return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
                   ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
        };

        header.version_major = swap16(header.version_major);
        header.version_minor = swap16(header.version_minor);
        header.snaplen = swap32(header.snaplen);
        header.network = swap32(header.network);
    }

    // Validate version
    if (header.version_major != 2 || header.version_minor != 4) {
        return Result<void>::err(
            Error{-1, "Unsupported PCAP version: " +
                          std::to_string(header.version_major) + "." +
                          std::to_string(header.version_minor)});
    }

    snaplen_ = header.snaplen;
    link_type_ = static_cast<LinkType>(header.network);

    return Result<void>::ok();
}

auto PcapReader::read_bytes(void* dest, std::size_t size) -> bool {
    if (from_memory_) {
        if (memory_offset_ + size > memory_data_.size()) {
            return false;
        }
        std::memcpy(dest, memory_data_.data() + memory_offset_, size);
        memory_offset_ += size;
        return true;
    } else {
        file_.read(reinterpret_cast<char*>(dest), static_cast<std::streamsize>(size));
        return file_.good();
    }
}

auto PcapReader::next_packet() -> std::optional<Packet> {
    PcapPacketHeader pkt_header{};
    if (!read_bytes(&pkt_header, sizeof(pkt_header))) {
        return std::nullopt;
    }

    // Handle byte swapping
    if (is_swapped_) {
        auto swap32 = [](std::uint32_t v) -> std::uint32_t {
            return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
                   ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
        };
        pkt_header.ts_sec = swap32(pkt_header.ts_sec);
        pkt_header.ts_usec = swap32(pkt_header.ts_usec);
        pkt_header.incl_len = swap32(pkt_header.incl_len);
        pkt_header.orig_len = swap32(pkt_header.orig_len);
    }

    // Sanity check
    if (pkt_header.incl_len > snaplen_ || pkt_header.incl_len > MAX_FRAME_SIZE * 10) {
        return std::nullopt;
    }

    // Read packet data
    std::vector<std::byte> data(pkt_header.incl_len);
    if (!read_bytes(data.data(), pkt_header.incl_len)) {
        return std::nullopt;
    }

    // Create timestamp
    Timestamp ts;
    if (is_nanosecond_) {
        ts = Timestamp::from_unix(static_cast<std::int64_t>(pkt_header.ts_sec),
                                  static_cast<std::int64_t>(pkt_header.ts_usec));
    } else {
        ts = Timestamp::from_unix_usec(static_cast<std::int64_t>(pkt_header.ts_sec),
                                       static_cast<std::int64_t>(pkt_header.ts_usec));
    }

    return Packet(ByteSpan(data.data(), data.size()), ts);
}

auto PcapReader::read_all() -> std::vector<Packet> {
    std::vector<Packet> packets;
    while (auto pkt = next_packet()) {
        packets.push_back(std::move(*pkt));
    }
    return packets;
}

std::streampos PcapReader::position() const {
    if (from_memory_) {
        return static_cast<std::streampos>(static_cast<std::streamoff>(memory_offset_));
    }
    return const_cast<std::ifstream&>(file_).tellg();
}

void PcapReader::reset() {
    if (from_memory_) {
        memory_offset_ = sizeof(PcapFileHeader);
    } else {
        file_.clear();
        file_.seekg(data_start_pos_);
    }
}

}  // namespace wadjet::pcap

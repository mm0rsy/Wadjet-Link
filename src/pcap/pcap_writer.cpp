#include "wadjet/pcap/pcap_writer.hpp"

#include <cstring>

namespace wadjet::pcap {

auto PcapWriter::create(const std::filesystem::path& path, Options options) -> Result<PcapWriter> {
    PcapWriter writer;
    writer.options_ = options;
    writer.description_ = "PcapWriter: " + path.string();

    writer.file_.open(path, std::ios::binary | std::ios::trunc);
    if (!writer.file_.is_open()) {
        return Result<PcapWriter>::err(Error{-1, "Failed to create file: " + path.string()});
    }

    // Write file header
    PcapFileHeader header{};
    header.magic_number = options.nanosecond_precision ? PCAP_MAGIC_NSEC_NATIVE : PCAP_MAGIC_NATIVE;
    header.version_major = 2;
    header.version_minor = 4;
    header.thiszone = 0;
    header.sigfigs = 0;
    header.snaplen = options.snaplen;
    header.network = static_cast<std::uint32_t>(options.link_type);

    writer.file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!writer.file_.good()) {
        return Result<PcapWriter>::err(Error{-1, "Failed to write PCAP header"});
    }

    return Result<PcapWriter>::ok(std::move(writer));
}

PcapWriter::~PcapWriter() {
    if (file_.is_open()) {
        flush();
    }
}

auto PcapWriter::write_packet(const PacketView& view) -> Result<void> {
    if (!file_.is_open()) {
        return Result<void>::err(Error{-1, "File not open"});
    }

    auto ts = view.timestamp();
    auto data = view.data();

    // Truncate to snaplen if needed
    std::uint32_t incl_len = static_cast<std::uint32_t>(
        std::min(data.size(), static_cast<std::size_t>(options_.snaplen)));
    std::uint32_t orig_len = static_cast<std::uint32_t>(data.size());

    PcapPacketHeader pkt_header{};
    pkt_header.ts_sec = static_cast<std::uint32_t>(ts.seconds());
    if (options_.nanosecond_precision) {
        pkt_header.ts_usec = static_cast<std::uint32_t>(ts.nanoseconds());
    } else {
        pkt_header.ts_usec = static_cast<std::uint32_t>(ts.microseconds());
    }
    pkt_header.incl_len = incl_len;
    pkt_header.orig_len = orig_len;

    file_.write(reinterpret_cast<const char*>(&pkt_header), sizeof(pkt_header));
    file_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(incl_len));

    if (!file_.good()) {
        return Result<void>::err(Error{-1, "Failed to write packet"});
    }

    packet_count_++;
    bytes_written_ += sizeof(pkt_header) + incl_len;

    return Result<void>::ok();
}

void PcapWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

}  // namespace wadjet::pcap

#include "wadjet/io/pcap_capture_session.hpp"

#include <pcap/pcap.h>

#include <cstring>

namespace wadjet::io {

struct PcapCaptureSession::Impl {
    pcap_t* handle = nullptr;
    bpf_program filter{};
    bool filter_set = false;
    std::size_t packets_received = 0;
    std::size_t bytes_received = 0;

    // For capture_loop callback
    PacketCallback current_callback;
    std::size_t max_packets = 0;
    std::size_t captured_count = 0;
    bool stop_requested = false;
};

PcapCaptureSession::PcapCaptureSession() : impl_(std::make_unique<Impl>()) {}

PcapCaptureSession::PcapCaptureSession(PcapCaptureSession&&) noexcept = default;
PcapCaptureSession& PcapCaptureSession::operator=(PcapCaptureSession&&) noexcept = default;

PcapCaptureSession::~PcapCaptureSession() {
    stop();
    if (impl_) {
        if (impl_->filter_set) {
            pcap_freecode(&impl_->filter);
        }
        if (impl_->handle) {
            pcap_close(impl_->handle);
        }
    }
}

auto PcapCaptureSession::create(const std::string& interface,
                                Options options) -> Result<PcapCaptureSession> {
    PcapCaptureSession session;
    session.interface_ = interface;
    session.options_ = options;

    char errbuf[PCAP_ERRBUF_SIZE];

    // Create pcap handle
    session.impl_->handle = pcap_create(interface.c_str(), errbuf);
    if (!session.impl_->handle) {
        return Result<PcapCaptureSession>::err(
            Error{-1, std::string("Failed to create pcap handle: ") + errbuf});
    }

    // Set options before activation
    if (pcap_set_snaplen(session.impl_->handle, static_cast<int>(options.snaplen)) != 0) {
        pcap_close(session.impl_->handle);
        return Result<PcapCaptureSession>::err(Error{-1, "Failed to set snaplen"});
    }

    if (pcap_set_promisc(session.impl_->handle, options.promiscuous ? 1 : 0) != 0) {
        pcap_close(session.impl_->handle);
        return Result<PcapCaptureSession>::err(Error{-1, "Failed to set promiscuous mode"});
    }

    if (pcap_set_timeout(session.impl_->handle, options.timeout_ms) != 0) {
        pcap_close(session.impl_->handle);
        return Result<PcapCaptureSession>::err(Error{-1, "Failed to set timeout"});
    }

    if (pcap_set_buffer_size(session.impl_->handle, static_cast<int>(options.buffer_size)) != 0) {
        pcap_close(session.impl_->handle);
        return Result<PcapCaptureSession>::err(Error{-1, "Failed to set buffer size"});
    }

#ifdef PCAP_TSTAMP_PRECISION_NANO
    if (options.timestamp_nano) {
        // Try to set nanosecond precision (may not be supported)
        pcap_set_tstamp_precision(session.impl_->handle, PCAP_TSTAMP_PRECISION_NANO);
    }
#endif

    if (options.immediate_mode) {
#ifdef PCAP_IMMEDIATE_MODE
        pcap_set_immediate_mode(session.impl_->handle, 1);
#endif
    }

    return Result<PcapCaptureSession>::ok(std::move(session));
}

auto PcapCaptureSession::open_offline(const std::filesystem::path& path)
    -> Result<PcapCaptureSession> {
    PcapCaptureSession session;
    session.interface_ = path.string();
    session.is_offline_ = true;

    char errbuf[PCAP_ERRBUF_SIZE];

#ifdef PCAP_TSTAMP_PRECISION_NANO
    // Try to open with nanosecond precision
    session.impl_->handle =
        pcap_open_offline_with_tstamp_precision(path.c_str(), PCAP_TSTAMP_PRECISION_NANO, errbuf);
    if (!session.impl_->handle) {
        // Fall back to regular open
        session.impl_->handle = pcap_open_offline(path.c_str(), errbuf);
    }
#else
    session.impl_->handle = pcap_open_offline(path.c_str(), errbuf);
#endif

    if (!session.impl_->handle) {
        return Result<PcapCaptureSession>::err(
            Error{-1, std::string("Failed to open pcap file: ") + errbuf});
    }

    session.activated_ = true;  // Offline files are immediately "activated"

    return Result<PcapCaptureSession>::ok(std::move(session));
}

auto PcapCaptureSession::set_filter(std::string_view expression) -> Result<void> {
    if (!impl_->handle) {
        return Result<void>::err(Error{-1, "Handle not initialized"});
    }

    // Free previous filter if set
    if (impl_->filter_set) {
        pcap_freecode(&impl_->filter);
        impl_->filter_set = false;
    }

    // Compile the filter
    if (pcap_compile(impl_->handle, &impl_->filter, expression.data(), 1, PCAP_NETMASK_UNKNOWN) !=
        0) {
        return Result<void>::err(
            Error{-1, std::string("Failed to compile filter: ") + pcap_geterr(impl_->handle)});
    }
    impl_->filter_set = true;

    // Apply the filter (only if activated)
    if (activated_) {
        if (pcap_setfilter(impl_->handle, &impl_->filter) != 0) {
            return Result<void>::err(
                Error{-1, std::string("Failed to set filter: ") + pcap_geterr(impl_->handle)});
        }
    }

    return Result<void>::ok();
}

auto PcapCaptureSession::start() -> Result<void> {
    if (!activated_) {
        auto result = activate();
        if (!result) {
            return result;
        }
    }
    running_ = true;
    return Result<void>::ok();
}

void PcapCaptureSession::stop() {
    running_ = false;
    impl_->stop_requested = true;
    if (impl_->handle) {
        pcap_breakloop(impl_->handle);
    }
}

auto PcapCaptureSession::activate() -> Result<void> {
    if (activated_) {
        return Result<void>::ok();
    }

    if (is_offline_) {
        activated_ = true;
        return Result<void>::ok();
    }

    int result = pcap_activate(impl_->handle);
    if (result < 0) {
        return Result<void>::err(
            Error{result, std::string("Failed to activate: ") + pcap_geterr(impl_->handle)});
    }

    // If we have a pending filter, apply it now
    if (impl_->filter_set) {
        if (pcap_setfilter(impl_->handle, &impl_->filter) != 0) {
            return Result<void>::err(
                Error{-1, std::string("Failed to set filter: ") + pcap_geterr(impl_->handle)});
        }
    }

    activated_ = true;
    return Result<void>::ok();
}

auto PcapCaptureSession::next_packet_impl() -> std::optional<Packet> {
    if (!impl_->handle) {
        return std::nullopt;
    }

    if (!activated_) {
        auto result = activate();
        if (!result) {
            return std::nullopt;
        }
    }

    pcap_pkthdr* header = nullptr;
    const u_char* data = nullptr;

    int result = pcap_next_ex(impl_->handle, &header, &data);

    if (result == 1 && header && data) {
        // Packet received
        impl_->packets_received++;
        impl_->bytes_received += header->caplen;

        // Determine timestamp precision
#ifdef PCAP_TSTAMP_PRECISION_NANO
        int precision = pcap_get_tstamp_precision(impl_->handle);
        bool is_nano = (precision == PCAP_TSTAMP_PRECISION_NANO);
#else
        bool is_nano = false;
#endif

        Timestamp ts;
        if (is_nano) {
            ts = Timestamp::from_unix(header->ts.tv_sec, header->ts.tv_usec);
        } else {
            ts = Timestamp::from_unix(header->ts.tv_sec, header->ts.tv_usec * 1000);
        }

        return Packet(ByteSpan(reinterpret_cast<const std::byte*>(data), header->caplen), ts);
    }

    if (result == 0) {
        // Timeout
        return std::nullopt;
    }

    if (result == -2) {
        // EOF (offline) or breakloop
        running_ = false;
        return std::nullopt;
    }

    // Error
    return std::nullopt;
}

auto PcapCaptureSession::capture_loop(const PacketCallback& callback,
                                      std::size_t max_packets) -> std::size_t {
    if (!impl_->handle) {
        return 0;
    }

    if (!activated_) {
        auto result = activate();
        if (!result) {
            return 0;
        }
    }

    impl_->current_callback = callback;
    impl_->max_packets = max_packets;
    impl_->captured_count = 0;
    impl_->stop_requested = false;
    running_ = true;

    // Use pcap_loop with a callback
    auto loop_callback = [](u_char* user, const pcap_pkthdr* header, const u_char* data) {
        auto* impl = reinterpret_cast<Impl*>(user);

        if (impl->stop_requested) {
            return;
        }

        impl->packets_received++;
        impl->bytes_received += header->caplen;

        // Create timestamp
        Timestamp ts = Timestamp::from_unix(header->ts.tv_sec, header->ts.tv_usec * 1000);

        // Create packet view and call callback
        PacketView view(ByteSpan(reinterpret_cast<const std::byte*>(data), header->caplen), ts);
        impl->current_callback(view);

        impl->captured_count++;

        if (impl->max_packets > 0 && impl->captured_count >= impl->max_packets) {
            impl->stop_requested = true;
        }
    };

    int count = (max_packets > 0) ? static_cast<int>(max_packets) : -1;
    pcap_loop(impl_->handle, count, loop_callback, reinterpret_cast<u_char*>(impl_.get()));

    running_ = false;
    impl_->current_callback = nullptr;

    return impl_->captured_count;
}

auto PcapCaptureSession::stats() const -> PcapCaptureStats {
    PcapCaptureStats result;
    result.packets_received = impl_->packets_received;
    result.bytes_received = impl_->bytes_received;

    if (impl_->handle && activated_ && !is_offline_) {
        pcap_stat ps{};
        if (pcap_stats(impl_->handle, &ps) == 0) {
            result.packets_dropped = ps.ps_drop;
            result.packets_if_dropped = ps.ps_ifdrop;
        }
    }

    return result;
}

void* PcapCaptureSession::pcap_handle() const {
    return impl_->handle;
}

int PcapCaptureSession::datalink() const {
    if (!impl_->handle) {
        return -1;
    }
    return pcap_datalink(impl_->handle);
}

auto PcapCaptureSession::inject(const PacketView& data) -> Result<void> {
    if (!impl_->handle) {
        return Result<void>::err(Error{-1, "Handle not initialized"});
    }

    if (!activated_) {
        return Result<void>::err(Error{-1, "Session not activated"});
    }

    auto pkt_data = data.data();
    int result = pcap_inject(impl_->handle, pkt_data.data(), pkt_data.size());

    if (result < 0) {
        return Result<void>::err(
            Error{-1, std::string("Failed to inject packet: ") + pcap_geterr(impl_->handle)});
    }

    if (static_cast<std::size_t>(result) != pkt_data.size()) {
        return Result<void>::err(Error{-1, "Partial packet injection"});
    }

    return Result<void>::ok();
}

auto PcapCaptureSession::list_devices() -> Result<std::vector<std::string>> {
    pcap_if_t* alldevs = nullptr;
    char errbuf[PCAP_ERRBUF_SIZE];

    if (pcap_findalldevs(&alldevs, errbuf) != 0) {
        return Result<std::vector<std::string>>::err(
            Error{-1, std::string("Failed to list devices: ") + errbuf});
    }

    std::vector<std::string> devices;
    for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
        devices.push_back(d->name);
    }

    pcap_freealldevs(alldevs);
    return Result<std::vector<std::string>>::ok(std::move(devices));
}

}  // namespace wadjet::io

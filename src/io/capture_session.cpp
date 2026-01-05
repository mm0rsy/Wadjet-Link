#include "wadjet/io/capture_session.hpp"

#include <pcap/pcap.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <thread>

#ifdef __linux__
    #include <linux/ethtool.h>
    #include <linux/filter.h>
    #include <linux/if_ether.h>
    #include <linux/if_packet.h>
    #include <linux/net_tstamp.h>
    #include <linux/sockios.h>
    #include <net/if.h>
    #include <netinet/in.h>
#endif

namespace wadjet::io {

/// @brief TPACKET version in use
enum class TpacketVersion {
    V2,  ///< TPACKET_V2 (frame-based)
    V3   ///< TPACKET_V3 (block-based, better performance)
};

struct CaptureSession::Impl {
    int socket_fd = -1;
    void* ring_buffer = nullptr;
    std::size_t ring_size = 0;

    // V2 frame-based fields
    std::size_t frame_size = 0;
    std::size_t frame_count = 0;
    std::size_t current_frame = 0;

    // V3 block-based fields
    TpacketVersion tpacket_version = TpacketVersion::V2;
    std::size_t block_size = 0;
    std::size_t block_count = 0;
    std::size_t current_block = 0;
    std::uint8_t* current_packet_in_block = nullptr;
    std::size_t packets_remaining_in_block = 0;

    CaptureStats stats{};
    bool hw_timestamp_enabled = false;
};

CaptureSession::CaptureSession() : impl_(std::make_unique<Impl>()) {}

CaptureSession::CaptureSession(CaptureSession&& other) noexcept
    : impl_(std::move(other.impl_)),
      interface_(std::move(other.interface_)),
      options_(std::move(other.options_)),
      running_(other.running_.load(std::memory_order_relaxed)),
      in_capture_loop_(other.in_capture_loop_.load(std::memory_order_relaxed)),
      active_ts_source_(other.active_ts_source_) {}

CaptureSession& CaptureSession::operator=(CaptureSession&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        interface_ = std::move(other.interface_);
        options_ = std::move(other.options_);
        running_.store(other.running_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        in_capture_loop_.store(other.in_capture_loop_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
        active_ts_source_ = other.active_ts_source_;
    }
    return *this;
}

CaptureSession::~CaptureSession() {
    stop();

    // Wait for capture loop to fully exit before unmapping memory
    // This prevents SEGV when capture thread is still accessing ring buffer
    while (in_capture_loop_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    if (impl_) {
        // First close the socket to prevent new data from arriving
        if (impl_->socket_fd >= 0) {
            close(impl_->socket_fd);
            impl_->socket_fd = -1;
        }
        // Then unmap the ring buffer
        if (impl_->ring_buffer != nullptr && impl_->ring_buffer != MAP_FAILED) {
            munmap(impl_->ring_buffer, impl_->ring_size);
            impl_->ring_buffer = nullptr;
            impl_->ring_size = 0;
        }
    }
}

auto CaptureSession::create(const std::string& interface,
                            Options options) -> Result<CaptureSession> {
    CaptureSession session;
    session.interface_ = interface;
    session.options_ = options;

#ifdef __linux__
    // Create AF_PACKET socket
    session.impl_->socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (session.impl_->socket_fd < 0) {
        return Result<CaptureSession>::err(
            Error{errno, "Failed to create AF_PACKET socket (need root?)"});
    }

    // Get interface index
    ifreq ifr{};
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    if (ioctl(session.impl_->socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        close(session.impl_->socket_fd);
        return Result<CaptureSession>::err(
            Error{errno, "Failed to get interface index for: " + interface});
    }
    int if_index = ifr.ifr_ifindex;

    // Bind to interface
    sockaddr_ll sll{};
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_index;

    if (bind(session.impl_->socket_fd, reinterpret_cast<sockaddr*>(&sll), sizeof(sll)) < 0) {
        close(session.impl_->socket_fd);
        return Result<CaptureSession>::err(
            Error{errno, "Failed to bind to interface: " + interface});
    }

    // Enable promiscuous mode if requested
    if (options.promiscuous) {
        packet_mreq mreq{};
        mreq.mr_ifindex = if_index;
        mreq.mr_type = PACKET_MR_PROMISC;
        if (setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq,
                       sizeof(mreq)) < 0) {
            // Non-fatal, just log
        }
    }

    // Set up ring buffer if requested
    bool ring_setup_success = false;

    if (options.use_ring_buffer) {
        // Try TPACKET_V3 first if requested
        if (options.use_tpacket_v3) {
            // Set TPACKET version to V3
            int version = TPACKET_V3;
            if (setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_VERSION, &version,
                           sizeof(version)) == 0) {
                // Set up TPACKET_V3 ring buffer
                tpacket_req3 req3{};
                req3.tp_block_size = static_cast<unsigned int>(options.block_size);
                req3.tp_block_nr =
                    static_cast<unsigned int>(options.buffer_size / options.block_size);
                req3.tp_frame_size = 2048;  // Max frame size within block
                req3.tp_frame_nr = (req3.tp_block_size / req3.tp_frame_size) * req3.tp_block_nr;
                req3.tp_retire_blk_tov = static_cast<unsigned int>(options.retire_timeout_ms);
                req3.tp_sizeof_priv = 0;
                req3.tp_feature_req_word = 0;

                if (setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_RX_RING, &req3,
                               sizeof(req3)) == 0) {
                    session.impl_->ring_size =
                        static_cast<std::size_t>(req3.tp_block_size) * req3.tp_block_nr;
                    session.impl_->block_size = req3.tp_block_size;
                    session.impl_->block_count = req3.tp_block_nr;
                    session.impl_->tpacket_version = TpacketVersion::V3;

                    session.impl_->ring_buffer =
                        mmap(nullptr, session.impl_->ring_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                             session.impl_->socket_fd, 0);
                    if (session.impl_->ring_buffer != MAP_FAILED) {
                        ring_setup_success = true;
                    } else {
                        session.impl_->ring_buffer = nullptr;
                    }
                }
            }
        }

        // Fall back to TPACKET_V2 if V3 failed
        if (!ring_setup_success) {
            // Reset to V2
            int version = TPACKET_V2;
            setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_VERSION, &version,
                       sizeof(version));

            // Set up TPACKET_V2 ring buffer (PACKET_RX_RING)
            tpacket_req req{};
            req.tp_block_size = 4096 * 16;  // 64KB blocks
            req.tp_block_nr = static_cast<unsigned int>(options.buffer_size / req.tp_block_size);
            req.tp_frame_size = 2048;
            req.tp_frame_nr = (req.tp_block_size / req.tp_frame_size) * req.tp_block_nr;

            if (setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_RX_RING, &req,
                           sizeof(req)) == 0) {
                session.impl_->ring_size =
                    static_cast<std::size_t>(req.tp_block_size) * req.tp_block_nr;
                session.impl_->frame_size = req.tp_frame_size;
                session.impl_->frame_count = req.tp_frame_nr;
                session.impl_->tpacket_version = TpacketVersion::V2;

                session.impl_->ring_buffer =
                    mmap(nullptr, session.impl_->ring_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         session.impl_->socket_fd, 0);
                if (session.impl_->ring_buffer == MAP_FAILED) {
                    session.impl_->ring_buffer = nullptr;
                }
            }
        }
    }  // end if (options.use_ring_buffer)

    // Set socket to non-blocking for poll()
    int flags = 1;
    if (ioctl(session.impl_->socket_fd, FIONBIO, &flags) < 0) {
        // Non-fatal
    }

    // Configure hardware timestamping if requested
    auto ts_result = session.configure_timestamps();
    if (!ts_result) {
        // Non-fatal, will fall back to software timestamps
        session.active_ts_source_ = TimestampSource::Software;
    }

    return Result<CaptureSession>::ok(std::move(session));
#else
    return Result<CaptureSession>::err(Error{-1, "AF_PACKET capture only supported on Linux"});
#endif
}

auto CaptureSession::set_filter(const FrameFilter& filter) -> Result<void> {
#ifdef __linux__
    const auto* bpf_prog = static_cast<const bpf_program*>(filter.program());
    if (bpf_prog == nullptr || bpf_prog->bf_len == 0) {
        return Result<void>::ok();  // No filter = accept all
    }

    sock_fprog kernel_filter{};
    kernel_filter.len = static_cast<unsigned short>(bpf_prog->bf_len);
    kernel_filter.filter = reinterpret_cast<sock_filter*>(bpf_prog->bf_insns);

    if (setsockopt(impl_->socket_fd, SOL_SOCKET, SO_ATTACH_FILTER, &kernel_filter,
                   sizeof(kernel_filter)) < 0) {
        return Result<void>::err(Error{errno, "Failed to attach BPF filter"});
    }

    return Result<void>::ok();
#else
    (void)filter;
    return Result<void>::err(Error{-1, "BPF filtering not supported"});
#endif
}

auto CaptureSession::set_filter(std::string_view expression) -> Result<void> {
    auto filter = FrameFilter::compile(expression);
    if (!filter) {
        return Result<void>::err(std::move(filter).error());
    }
    return set_filter(*filter);
}

auto CaptureSession::start() -> Result<void> {
    running_ = true;
    return Result<void>::ok();
}

void CaptureSession::stop() {
    running_ = false;
}

bool CaptureSession::is_running() const {
    return running_;
}

auto CaptureSession::next_packet_impl(int timeout_ms) -> std::optional<Packet> {
    if (impl_->socket_fd < 0) {
        return std::nullopt;
    }

#ifdef __linux__
    // Use ring buffer if available
    if (impl_->ring_buffer != nullptr) {
        if (impl_->tpacket_version == TpacketVersion::V3) {
            return next_packet_v3(timeout_ms);
        }
        return next_packet_v2(timeout_ms);
    }

    // Fall back to recvfrom
    pollfd pfd{};
    pfd.fd = impl_->socket_fd;
    pfd.events = POLLIN;

    if (poll(&pfd, 1, timeout_ms) <= 0) {
        return std::nullopt;
    }

    std::array<std::byte, 65535> buffer;
    sockaddr_ll sll{};
    socklen_t sll_len = sizeof(sll);

    auto received = recvfrom(impl_->socket_fd, buffer.data(), buffer.size(), 0,
                             reinterpret_cast<sockaddr*>(&sll), &sll_len);

    if (received <= 0) {
        return std::nullopt;
    }

    impl_->stats.packets_received++;
    impl_->stats.bytes_received += static_cast<std::uint64_t>(received);

    return Packet(ByteSpan(buffer.data(), static_cast<std::size_t>(received)), Timestamp::now());
#else
    return std::nullopt;
#endif
}

auto CaptureSession::next_packet_v2(int timeout_ms) -> std::optional<Packet> {
#ifdef __linux__
    auto* frame =
        static_cast<std::uint8_t*>(impl_->ring_buffer) + impl_->current_frame * impl_->frame_size;
    auto* header = reinterpret_cast<volatile tpacket2_hdr*>(frame);

    // Check if frame is ready
    if ((header->tp_status & TP_STATUS_USER) == 0) {
        // Wait for data
        pollfd pfd{};
        pfd.fd = impl_->socket_fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, timeout_ms) <= 0) {
            return std::nullopt;
        }
        // Memory barrier to ensure we see the kernel's updates
        __sync_synchronize();
        // Re-check after poll - the status may still not be ready
        if ((header->tp_status & TP_STATUS_USER) == 0) {
            return std::nullopt;
        }
    }

    // Memory barrier before reading packet data
    __sync_synchronize();

    // Extract packet - read from volatile header (TPACKET_V2 uses tpacket2_hdr)
    auto* data = frame + header->tp_mac;
    std::size_t len = header->tp_snaplen;

    // Sanity check the length - packets on loopback can be quite large
    // tp_mac is typically around 66-70 bytes (tpacket2_hdr + sockaddr_ll + padding)
    // Maximum packet size should be bounded by snap_len (usually 65535 or buffer_size)
    static constexpr std::size_t MAX_PACKET_LEN = 65535;  // Standard max
    if (len == 0 || len > MAX_PACKET_LEN) {
        // Invalid frame data, release and skip
        const_cast<tpacket2_hdr*>(header)->tp_status = TP_STATUS_KERNEL;
        impl_->current_frame = (impl_->current_frame + 1) % impl_->frame_count;
        return std::nullopt;
    }

    Timestamp ts = Timestamp::from_unix(static_cast<std::int64_t>(header->tp_sec),
                                        static_cast<std::int64_t>(header->tp_nsec));

    Packet pkt(ByteSpan(reinterpret_cast<const std::byte*>(data), len), ts);

    // Release frame
    const_cast<tpacket2_hdr*>(header)->tp_status = TP_STATUS_KERNEL;

    // Move to next frame
    impl_->current_frame = (impl_->current_frame + 1) % impl_->frame_count;
    impl_->stats.packets_received++;
    impl_->stats.bytes_received += len;

    return pkt;
#else
    (void)timeout_ms;
#endif
    return std::nullopt;
}

auto CaptureSession::next_packet_v3(int timeout_ms) -> std::optional<Packet> {
#ifdef __linux__
    // Check if we have packets remaining in current block
    while (impl_->packets_remaining_in_block == 0) {
        // Get current block
        auto* block = static_cast<std::uint8_t*>(impl_->ring_buffer) +
                      impl_->current_block * impl_->block_size;
        auto* block_desc = reinterpret_cast<tpacket_block_desc*>(block);
        auto* bd_hdr = &block_desc->hdr.bh1;

        // Check if block is ready
        if ((bd_hdr->block_status & TP_STATUS_USER) == 0) {
            // Wait for data
            pollfd pfd{};
            pfd.fd = impl_->socket_fd;
            pfd.events = POLLIN;
            if (poll(&pfd, 1, timeout_ms) <= 0) {
                return std::nullopt;
            }

            // Re-check after poll
            if ((bd_hdr->block_status & TP_STATUS_USER) == 0) {
                return std::nullopt;
            }
        }

        // Block is ready - check if it has packets
        if (bd_hdr->num_pkts == 0) {
            // Empty block, release and move to next
            bd_hdr->block_status = TP_STATUS_KERNEL;
            impl_->current_block = (impl_->current_block + 1) % impl_->block_count;
            continue;
        }

        // Set up packet iteration
        impl_->packets_remaining_in_block = bd_hdr->num_pkts;
        impl_->current_packet_in_block = block + bd_hdr->offset_to_first_pkt;
    }

    // Extract current packet from block
    auto* pkt_hdr = reinterpret_cast<tpacket3_hdr*>(impl_->current_packet_in_block);

    // Validate packet header - check for reasonable values
    std::size_t len = pkt_hdr->tp_snaplen;
    if (len == 0 || len > impl_->block_size || pkt_hdr->tp_mac == 0) {
        // Invalid packet, skip entire block
        impl_->packets_remaining_in_block = 0;
        auto* block = static_cast<std::uint8_t*>(impl_->ring_buffer) +
                      impl_->current_block * impl_->block_size;
        auto* block_desc = reinterpret_cast<tpacket_block_desc*>(block);
        block_desc->hdr.bh1.block_status = TP_STATUS_KERNEL;
        impl_->current_block = (impl_->current_block + 1) % impl_->block_count;
        return std::nullopt;
    }

    auto* data = impl_->current_packet_in_block + pkt_hdr->tp_mac;

    // V3 provides nanosecond timestamps
    Timestamp ts = Timestamp::from_unix(static_cast<std::int64_t>(pkt_hdr->tp_sec),
                                        static_cast<std::int64_t>(pkt_hdr->tp_nsec));

    Packet pkt(ByteSpan(reinterpret_cast<const std::byte*>(data), len), ts);

    impl_->stats.packets_received++;
    impl_->stats.bytes_received += len;

    // Move to next packet in block
    impl_->packets_remaining_in_block--;

    if (impl_->packets_remaining_in_block > 0 && pkt_hdr->tp_next_offset != 0) {
        // Move to next packet within same block
        impl_->current_packet_in_block += pkt_hdr->tp_next_offset;
    } else {
        // Release block back to kernel
        auto* block = static_cast<std::uint8_t*>(impl_->ring_buffer) +
                      impl_->current_block * impl_->block_size;
        auto* block_desc = reinterpret_cast<tpacket_block_desc*>(block);
        block_desc->hdr.bh1.block_status = TP_STATUS_KERNEL;

        // Move to next block
        impl_->current_block = (impl_->current_block + 1) % impl_->block_count;
        impl_->packets_remaining_in_block = 0;
    }

    return pkt;
#else
    (void)timeout_ms;
    return std::nullopt;
#endif
}

auto CaptureSession::capture_loop(const PacketCallback& callback,
                                  std::size_t max_packets) -> std::size_t {
    std::size_t count = 0;
    running_ = true;
    in_capture_loop_ = true;

    while (running_ && (max_packets == 0 || count < max_packets)) {
        if (auto packet = next_packet()) {
            callback(packet->view());
            count++;
        }
    }

    in_capture_loop_ = false;
    return count;
}

auto CaptureSession::stats() const -> CaptureStats {
    CaptureStats result = impl_->stats;

#ifdef __linux__
    // Get kernel stats
    if (impl_->socket_fd >= 0) {
        tpacket_stats kstats{};
        socklen_t len = sizeof(kstats);
        if (getsockopt(impl_->socket_fd, SOL_PACKET, PACKET_STATISTICS, &kstats, &len) == 0) {
            result.packets_dropped = kstats.tp_drops;
        }
    }
#endif

    return result;
}

int CaptureSession::fd() const {
    return impl_ ? impl_->socket_fd : -1;
}

auto CaptureSession::query_hw_timestamp_caps(const std::string& interface)
    -> Result<HardwareTimestampCaps> {
#ifdef __linux__
    HardwareTimestampCaps caps{};

    // Create a temporary socket for the ioctl
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return Result<HardwareTimestampCaps>::err(
            Error{errno, "Failed to create socket for timestamp query"});
    }

    // Query hardware timestamp capabilities
    ifreq ifr{};
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    hwtstamp_config config{};
    ifr.ifr_data = reinterpret_cast<char*>(&config);

    // Try to get current config first (also tests if timestamping is supported)
    ethtool_ts_info ts_info{};
    ts_info.cmd = ETHTOOL_GET_TS_INFO;
    ifr.ifr_data = reinterpret_cast<char*>(&ts_info);

    if (ioctl(sock, SIOCETHTOOL, &ifr) == 0) {
        // Check supported capabilities
        if (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_HARDWARE) {
            caps.supports_tx_hardware = true;
        }
        if (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_SOFTWARE) {
            caps.supports_tx_software = true;
        }
        if (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_HARDWARE) {
            caps.supports_rx_hardware = true;
        }
        if (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_SOFTWARE) {
            caps.supports_rx_software = true;
        }
        if (ts_info.so_timestamping & SOF_TIMESTAMPING_RAW_HARDWARE) {
            caps.supports_raw_hardware = true;
        }
    }

    close(sock);
    return Result<HardwareTimestampCaps>::ok(caps);
#else
    (void)interface;
    return Result<HardwareTimestampCaps>::err(
        Error{-1, "Hardware timestamping query only supported on Linux"});
#endif
}

auto CaptureSession::configure_timestamps() -> Result<void> {
#ifdef __linux__
    if (impl_->socket_fd < 0) {
        return Result<void>::err(Error{-1, "Socket not initialized"});
    }

    // Default to software timestamps
    active_ts_source_ = TimestampSource::Software;

    if (options_.timestamp_source == TimestampSource::Software) {
        return Result<void>::ok();
    }

    // Query capabilities
    auto caps_result = query_hw_timestamp_caps(interface_);
    if (!caps_result) {
        if (options_.timestamp_source == TimestampSource::Auto) {
            return Result<void>::ok();  // Fall back to software
        }
        return Result<void>::err(std::move(caps_result).error());
    }
    auto caps = *caps_result;

    // Determine what to enable
    int timestamp_flags = 0;
    bool want_hardware =
        (options_.timestamp_source == TimestampSource::Hardware) ||
        (options_.timestamp_source == TimestampSource::Auto && caps.supports_rx_hardware);

    if (want_hardware && caps.supports_rx_hardware) {
        timestamp_flags |= SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
        active_ts_source_ = TimestampSource::Hardware;
    } else if (caps.supports_rx_software) {
        timestamp_flags |= SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
        active_ts_source_ = TimestampSource::Software;
    } else {
        // No timestamping available through SO_TIMESTAMPING
        return Result<void>::ok();
    }

    // Enable hardware timestamping on the NIC if needed
    if (want_hardware && caps.supports_rx_hardware) {
        ifreq ifr{};
        std::strncpy(ifr.ifr_name, interface_.c_str(), IFNAMSIZ - 1);

        hwtstamp_config config{};
        config.tx_type = HWTSTAMP_TX_OFF;
        config.rx_filter = HWTSTAMP_FILTER_ALL;
        ifr.ifr_data = reinterpret_cast<char*>(&config);

        if (ioctl(impl_->socket_fd, SIOCSHWTSTAMP, &ifr) < 0) {
            // Failed to enable hardware timestamping, try software
            if (options_.timestamp_source == TimestampSource::Auto) {
                timestamp_flags = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
                active_ts_source_ = TimestampSource::Software;
            } else {
                return Result<void>::err(Error{errno, "Failed to enable hardware timestamping"});
            }
        } else {
            impl_->hw_timestamp_enabled = true;
        }
    }

    // Set socket option for timestamping
    if (setsockopt(impl_->socket_fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags,
                   sizeof(timestamp_flags)) < 0) {
        if (options_.timestamp_source != TimestampSource::Auto) {
            return Result<void>::err(Error{errno, "Failed to set SO_TIMESTAMPING"});
        }
        // Fall back to default timestamps
        active_ts_source_ = TimestampSource::Software;
    }

    return Result<void>::ok();
#else
    return Result<void>::err(Error{-1, "Hardware timestamping only supported on Linux"});
#endif
}

}  // namespace wadjet::io

#include "wadjet/io/capture_session.hpp"

#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pcap/pcap.h>

#ifdef __linux__
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace wadjet::io {

struct CaptureSession::Impl {
    int socket_fd = -1;
    void* ring_buffer = nullptr;
    std::size_t ring_size = 0;
    std::size_t frame_size = 0;
    std::size_t frame_count = 0;
    std::size_t current_frame = 0;
    CaptureStats stats{};
};

CaptureSession::CaptureSession() : impl_(std::make_unique<Impl>()) {}

CaptureSession::CaptureSession(CaptureSession&&) noexcept = default;
CaptureSession& CaptureSession::operator=(CaptureSession&&) noexcept = default;

CaptureSession::~CaptureSession() {
    stop();
    if (impl_) {
        if (impl_->ring_buffer != nullptr && impl_->ring_buffer != MAP_FAILED) {
            munmap(impl_->ring_buffer, impl_->ring_size);
        }
        if (impl_->socket_fd >= 0) {
            close(impl_->socket_fd);
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

    if (bind(session.impl_->socket_fd, reinterpret_cast<sockaddr*>(&sll),
             sizeof(sll)) < 0) {
        close(session.impl_->socket_fd);
        return Result<CaptureSession>::err(
            Error{errno, "Failed to bind to interface: " + interface});
    }

    // Enable promiscuous mode if requested
    if (options.promiscuous) {
        packet_mreq mreq{};
        mreq.mr_ifindex = if_index;
        mreq.mr_type = PACKET_MR_PROMISC;
        if (setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
                       &mreq, sizeof(mreq)) < 0) {
            // Non-fatal, just log
        }
    }

    // Set up ring buffer (PACKET_RX_RING)
    tpacket_req req{};
    req.tp_block_size = 4096 * 16;  // 64KB blocks
    req.tp_block_nr = static_cast<unsigned int>(options.buffer_size / req.tp_block_size);
    req.tp_frame_size = 2048;
    req.tp_frame_nr = (req.tp_block_size / req.tp_frame_size) * req.tp_block_nr;

    if (setsockopt(session.impl_->socket_fd, SOL_PACKET, PACKET_RX_RING,
                   &req, sizeof(req)) < 0) {
        // Fall back to non-ring buffer mode
        session.impl_->ring_buffer = nullptr;
    } else {
        session.impl_->ring_size = static_cast<std::size_t>(req.tp_block_size) * req.tp_block_nr;
        session.impl_->frame_size = req.tp_frame_size;
        session.impl_->frame_count = req.tp_frame_nr;

        session.impl_->ring_buffer = mmap(nullptr, session.impl_->ring_size,
                                          PROT_READ | PROT_WRITE, MAP_SHARED,
                                          session.impl_->socket_fd, 0);
        if (session.impl_->ring_buffer == MAP_FAILED) {
            session.impl_->ring_buffer = nullptr;
        }
    }

    // Set socket to non-blocking for poll()
    int flags = 1;
    if (ioctl(session.impl_->socket_fd, FIONBIO, &flags) < 0) {
        // Non-fatal
    }

    return Result<CaptureSession>::ok(std::move(session));
#else
    return Result<CaptureSession>::err(
        Error{-1, "AF_PACKET capture only supported on Linux"});
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

    if (setsockopt(impl_->socket_fd, SOL_SOCKET, SO_ATTACH_FILTER,
                   &kernel_filter, sizeof(kernel_filter)) < 0) {
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
        auto* frame = static_cast<std::uint8_t*>(impl_->ring_buffer) +
                      impl_->current_frame * impl_->frame_size;
        auto* header = reinterpret_cast<tpacket_hdr*>(frame);

        // Check if frame is ready
        if ((header->tp_status & TP_STATUS_USER) == 0) {
            // Wait for data
            pollfd pfd{};
            pfd.fd = impl_->socket_fd;
            pfd.events = POLLIN;
            if (poll(&pfd, 1, timeout_ms) <= 0) {
                return std::nullopt;
            }
        }

        if ((header->tp_status & TP_STATUS_USER) != 0) {
            // Extract packet
            auto* data = frame + header->tp_mac;
            std::size_t len = header->tp_snaplen;

            Timestamp ts = Timestamp::from_unix(
                static_cast<std::int64_t>(header->tp_sec),
                static_cast<std::int64_t>(header->tp_usec) * 1000);

            Packet pkt(ByteSpan(reinterpret_cast<const std::byte*>(data), len), ts);

            // Release frame
            header->tp_status = TP_STATUS_KERNEL;

            // Move to next frame
            impl_->current_frame = (impl_->current_frame + 1) % impl_->frame_count;
            impl_->stats.packets_received++;
            impl_->stats.bytes_received += len;

            return pkt;
        }
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

    return Packet(ByteSpan(buffer.data(), static_cast<std::size_t>(received)),
                  Timestamp::now());
#else
    return std::nullopt;
#endif
}

auto CaptureSession::capture_loop(const PacketCallback& callback,
                                  std::size_t max_packets) -> std::size_t {
    std::size_t count = 0;
    running_ = true;

    while (running_ && (max_packets == 0 || count < max_packets)) {
        if (auto packet = next_packet()) {
            callback(packet->view());
            count++;
        }
    }

    return count;
}

auto CaptureSession::stats() const -> CaptureStats {
    CaptureStats result = impl_->stats;

#ifdef __linux__
    // Get kernel stats
    if (impl_->socket_fd >= 0) {
        tpacket_stats kstats{};
        socklen_t len = sizeof(kstats);
        if (getsockopt(impl_->socket_fd, SOL_PACKET, PACKET_STATISTICS,
                       &kstats, &len) == 0) {
            result.packets_dropped = kstats.tp_drops;
        }
    }
#endif

    return result;
}

int CaptureSession::fd() const {
    return impl_ ? impl_->socket_fd : -1;
}

}  // namespace wadjet::io

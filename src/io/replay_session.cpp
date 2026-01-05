#include "wadjet/io/replay_session.hpp"

#include <cstring>
#include <thread>

#ifdef __linux__
    #include <linux/if_ether.h>
    #include <linux/if_packet.h>
    #include <net/if.h>
    #include <netinet/in.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace wadjet::io {

struct ReplaySession::Impl {
    int socket_fd = -1;
    int if_index = 0;
    sockaddr_ll socket_addr{};
};

ReplaySession::ReplaySession() : impl_(std::make_unique<Impl>()) {}

ReplaySession::ReplaySession(ReplaySession&&) noexcept = default;
ReplaySession& ReplaySession::operator=(ReplaySession&&) noexcept = default;

ReplaySession::~ReplaySession() {
    stop();
    if (impl_ && impl_->socket_fd >= 0) {
#ifdef __linux__
        close(impl_->socket_fd);
#endif
    }
}

auto ReplaySession::create(const std::string& interface, const std::filesystem::path& pcap_path,
                           Options options) -> Result<ReplaySession> {
    // Open PCAP file
    auto reader_result = pcap::PcapReader::open(pcap_path);
    if (!reader_result) {
        return Result<ReplaySession>::err(std::move(reader_result).error());
    }

    // Create unique_ptr for the reader
    auto reader_ptr = std::make_unique<pcap::PcapReader>(std::move(*reader_result));

    return create(interface, std::move(reader_ptr), options);
}

auto ReplaySession::create(const std::string& interface, PacketSourcePtr source,
                           Options options) -> Result<ReplaySession> {
    ReplaySession session;
    session.interface_ = interface;
    session.options_ = options;

    // Load packets from source
    auto load_result = session.load_packets(std::move(source));
    if (!load_result) {
        return Result<ReplaySession>::err(std::move(load_result).error());
    }

    // Initialize socket
    auto init_result = session.init_socket();
    if (!init_result) {
        return Result<ReplaySession>::err(std::move(init_result).error());
    }

    return Result<ReplaySession>::ok(std::move(session));
}

auto ReplaySession::create(const std::string& interface, std::vector<Packet> packets,
                           Options options) -> Result<ReplaySession> {
    ReplaySession session;
    session.interface_ = interface;
    session.options_ = options;
    session.packets_ = std::move(packets);

    // Initialize socket
    auto init_result = session.init_socket();
    if (!init_result) {
        return Result<ReplaySession>::err(std::move(init_result).error());
    }

    return Result<ReplaySession>::ok(std::move(session));
}

auto ReplaySession::init_socket() -> Result<void> {
#ifdef __linux__
    // Create AF_PACKET socket for sending raw frames
    impl_->socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (impl_->socket_fd < 0) {
        return Result<void>::err(Error{errno, "Failed to create AF_PACKET socket (need root?)"});
    }

    // Get interface index
    ifreq ifr{};
    std::strncpy(ifr.ifr_name, interface_.c_str(), IFNAMSIZ - 1);
    if (ioctl(impl_->socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        return Result<void>::err(Error{errno, "Failed to get interface index for: " + interface_});
    }
    impl_->if_index = ifr.ifr_ifindex;

    // Get interface hardware address (for socket binding)
    if (ioctl(impl_->socket_fd, SIOCGIFHWADDR, &ifr) < 0) {
        // Non-fatal, continue without hardware address
    }

    // Set up socket address for sending
    std::memset(&impl_->socket_addr, 0, sizeof(impl_->socket_addr));
    impl_->socket_addr.sll_family = AF_PACKET;
    impl_->socket_addr.sll_protocol = htons(ETH_P_ALL);
    impl_->socket_addr.sll_ifindex = impl_->if_index;
    impl_->socket_addr.sll_halen = ETH_ALEN;
    // Destination MAC will be set per-packet from the frame data

    // Bind to the interface
    sockaddr_ll bind_addr{};
    bind_addr.sll_family = AF_PACKET;
    bind_addr.sll_protocol = htons(ETH_P_ALL);
    bind_addr.sll_ifindex = impl_->if_index;

    if (bind(impl_->socket_fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        close(impl_->socket_fd);
        impl_->socket_fd = -1;
        return Result<void>::err(Error{errno, "Failed to bind to interface: " + interface_});
    }

    // Enable promiscuous mode if requested
    if (options_.promiscuous) {
        packet_mreq mreq{};
        mreq.mr_ifindex = impl_->if_index;
        mreq.mr_type = PACKET_MR_PROMISC;
        setsockopt(impl_->socket_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        // Non-fatal if this fails
    }

    return Result<void>::ok();
#else
    return Result<void>::err(Error{-1, "AF_PACKET replay only supported on Linux"});
#endif
}

auto ReplaySession::load_packets(PacketSourcePtr source) -> Result<void> {
    packets_.clear();

    while (auto pkt = source->next_packet()) {
        packets_.push_back(std::move(*pkt));

        // Check max packets limit
        if (options_.max_packets > 0 && packets_.size() >= options_.max_packets) {
            break;
        }
    }

    if (packets_.empty()) {
        return Result<void>::err(Error{-1, "No packets loaded from source"});
    }

    return Result<void>::ok();
}

auto ReplaySession::run() -> Result<void> {
    if (packets_.empty()) {
        return Result<void>::err(Error{-1, "No packets to replay"});
    }

#ifdef __linux__
    running_ = true;
    stats_ = ReplayStats{};

    auto start_time = std::chrono::steady_clock::now();
    std::size_t iteration = 0;
    std::size_t total_packets_sent = 0;

    do {
        Timestamp prev_ts = packets_.front().timestamp();

        for (std::size_t i = 0; i < packets_.size() && running_; ++i) {
            Packet pkt = packets_[i];  // Copy for potential modification

            // Apply modifier if set
            if (modifier_) {
                if (!modifier_(pkt)) {
                    continue;  // Skip this packet
                }
            }

            // Wait for appropriate timing
            if (i > 0 || iteration > 0) {
                wait_for_timing(pkt.timestamp(), prev_ts);
            }
            prev_ts = pkt.timestamp();

            // Send the packet
            auto send_result = send_packet(pkt);
            if (send_result) {
                stats_.packets_sent++;
                stats_.bytes_sent += pkt.size();
            } else {
                stats_.packets_failed++;
            }

            total_packets_sent++;

            // Call progress callback
            if (progress_callback_ && (total_packets_sent % progress_interval_ == 0)) {
                if (!progress_callback_(stats_)) {
                    running_ = false;
                    break;
                }
            }

            // Check max packets limit
            if (options_.max_packets > 0 && total_packets_sent >= options_.max_packets) {
                running_ = false;
                break;
            }
        }

        iteration++;
        stats_.iterations = iteration;

        // Check iteration limit
        if (options_.max_iterations > 0 && iteration >= options_.max_iterations) {
            break;
        }

    } while (options_.loop && running_);

    auto end_time = std::chrono::steady_clock::now();
    stats_.total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    running_ = false;
    return Result<void>::ok();
#else
    return Result<void>::err(Error{-1, "Replay only supported on Linux"});
#endif
}

void ReplaySession::stop() {
    running_ = false;
}

auto ReplaySession::send_packet(const Packet& packet) -> Result<void> {
    return send_packet(packet.view());
}

auto ReplaySession::send_packet(const PacketView& view) -> Result<void> {
#ifdef __linux__
    if (impl_->socket_fd < 0) {
        return Result<void>::err(Error{-1, "Socket not initialized"});
    }

    auto data = view.data();
    if (data.size() < 14) {
        return Result<void>::err(Error{-1, "Packet too small (min 14 bytes for Ethernet)"});
    }

    // Set destination MAC in socket address (first 6 bytes of frame)
    std::memcpy(impl_->socket_addr.sll_addr, data.data(), ETH_ALEN);

    auto sent =
        sendto(impl_->socket_fd, data.data(), data.size(), 0,
               reinterpret_cast<sockaddr*>(&impl_->socket_addr), sizeof(impl_->socket_addr));

    if (sent < 0) {
        return Result<void>::err(Error{errno, "Failed to send packet"});
    }

    if (static_cast<std::size_t>(sent) != data.size()) {
        return Result<void>::err(Error{-1, "Partial packet send"});
    }

    return Result<void>::ok();
#else
    (void)view;
    return Result<void>::err(Error{-1, "Replay only supported on Linux"});
#endif
}

void ReplaySession::wait_for_timing(const Timestamp& current_ts, const Timestamp& prev_ts) {
    switch (options_.timing) {
        case ReplayTiming::Immediate:
            // No waiting
            break;

        case ReplayTiming::AsRecorded: {
            auto delta = current_ts.total_nanoseconds() - prev_ts.total_nanoseconds();
            if (delta > 0) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(delta));
            }
            break;
        }

        case ReplayTiming::Scaled: {
            auto delta = current_ts.total_nanoseconds() - prev_ts.total_nanoseconds();
            if (delta > 0 && options_.speed_factor > 0) {
                auto scaled =
                    static_cast<std::int64_t>(static_cast<double>(delta) / options_.speed_factor);
                std::this_thread::sleep_for(std::chrono::nanoseconds(scaled));
            }
            break;
        }

        case ReplayTiming::FixedRate: {
            if (options_.packets_per_second > 0) {
                auto interval_ns = 1'000'000'000ULL / options_.packets_per_second;
                std::this_thread::sleep_for(std::chrono::nanoseconds(interval_ns));
            }
            break;
        }
    }
}

}  // namespace wadjet::io

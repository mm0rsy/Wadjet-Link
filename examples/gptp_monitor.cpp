/// @file gptp_monitor.cpp
/// @brief Example: gPTP (IEEE 802.1AS) Time Synchronization Monitor
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to capture and decode gPTP (IEEE 802.1AS)
/// time synchronization messages to monitor clock synchronization on an
/// automotive Ethernet network.
///
/// gPTP is used in automotive systems for:
/// - Time-triggered communication (TSN)
/// - Audio/Video Bridging (AVB)
/// - AUTOSAR timing requirements
///
/// Usage:
///   ./gptp_monitor eth0               # Live capture on interface
///   ./gptp_monitor capture.pcap       # Analyze from PCAP file

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/gptp/gptp.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::gptp;
using namespace std::chrono_literals;

// =============================================================================
// Clock Tracker - Track gPTP clocks and timing information
// =============================================================================

struct ClockInfo {
    ClockIdentity clock_id;
    std::uint8_t domain = 0;
    std::uint16_t port_number = 0;
    std::chrono::system_clock::time_point last_seen;

    // Sync message tracking
    std::uint32_t sync_count = 0;
    std::uint16_t last_sync_seq = 0;
    GptpTimestamp last_sync_ts{};

    // Follow_Up tracking
    std::uint32_t follow_up_count = 0;

    // Pdelay tracking
    std::uint32_t pdelay_req_count = 0;
    std::uint32_t pdelay_resp_count = 0;

    // Announce tracking
    std::uint32_t announce_count = 0;
    std::uint8_t priority1 = 255;
    std::uint8_t priority2 = 255;
    ClockIdentity grandmaster_id{};

    // Rate ratio info from Follow_Up TLV
    std::optional<double> rate_ratio;
};

class ClockTracker {
public:
    void process_gptp(const GptpHeader& header) {
        auto& info = clocks_[header.source_port_identity.clock_identity];
        info.clock_id = header.source_port_identity.clock_identity;
        info.domain = header.domain_number;
        info.port_number = header.source_port_identity.port_number;
        info.last_seen = std::chrono::system_clock::now();

        switch (header.message_type) {
            case MessageType::Sync:
                process_sync(header, info);
                break;
            case MessageType::Follow_Up:
                process_follow_up(header, info);
                break;
            case MessageType::Pdelay_Req:
                info.pdelay_req_count++;
                break;
            case MessageType::Pdelay_Resp:
                info.pdelay_resp_count++;
                break;
            case MessageType::Announce:
                process_announce(header, info);
                break;
            default:
                break;
        }
    }

    void print_summary() const {
        std::cout << "\n╔══════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    gPTP Clock Synchronization Summary                 ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n\n";

        if (clocks_.empty()) {
            std::cout << "No gPTP clocks detected.\n";
            return;
        }

        std::cout << "🕐 Detected Clocks (" << clocks_.size() << "):\n\n";

        for (const auto& [id, info] : clocks_) {
            print_clock_info(info);
        }

        // Print grandmaster summary
        print_grandmaster_summary();
    }

private:
    void process_sync(const GptpHeader& header, ClockInfo& info) {
        info.sync_count++;
        info.last_sync_seq = header.sequence_id;

        if (header.body && std::holds_alternative<SyncMessage>(*header.body)) {
            const auto& sync = std::get<SyncMessage>(*header.body);
            info.last_sync_ts = sync.origin_timestamp;
        }
    }

    void process_follow_up(const GptpHeader& header, ClockInfo& info) {
        info.follow_up_count++;

        if (header.body && std::holds_alternative<FollowUpMessage>(*header.body)) {
            const auto& fu = std::get<FollowUpMessage>(*header.body);

            // Extract rate ratio from Follow_Up information TLV if present
            if (fu.follow_up_info) {
                // Rate ratio is scaled - convert to actual ratio
                // Cumulative rate offset is in 2^-41 units
                double rate_offset =
                    static_cast<double>(fu.follow_up_info->cumulative_scaled_rate_offset) /
                    (1LL << 41);
                info.rate_ratio = 1.0 + rate_offset;
            }
        }
    }

    void process_announce(const GptpHeader& header, ClockInfo& info) {
        info.announce_count++;

        if (header.body && std::holds_alternative<AnnounceMessage>(*header.body)) {
            const auto& ann = std::get<AnnounceMessage>(*header.body);
            info.priority1 = ann.grandmaster_priority1;
            info.priority2 = ann.grandmaster_priority2;
            info.grandmaster_id = ann.grandmaster_identity;
        }
    }

    void print_clock_info(const ClockInfo& info) const {
        std::cout << "┌────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ Clock: " << std::setw(55) << std::left
                  << info.clock_id.to_string() << " │\n";
        std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
        std::cout << "│ Domain: " << std::setw(5) << std::left
                  << static_cast<int>(info.domain)
                  << "  Port: " << std::setw(5) << info.port_number
                  << "                                       │\n";
        std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
        std::cout << "│ Message Statistics:                                                │\n";
        std::cout << "│   📤 Sync:        " << std::setw(10) << info.sync_count
                  << "   📨 Follow_Up:   " << std::setw(10) << info.follow_up_count << " │\n";
        std::cout << "│   📡 Pdelay_Req:  " << std::setw(10) << info.pdelay_req_count
                  << "   📥 Pdelay_Resp: " << std::setw(10) << info.pdelay_resp_count << " │\n";
        std::cout << "│   📢 Announce:    " << std::setw(10) << info.announce_count
                  << "                              │\n";

        if (info.announce_count > 0) {
            std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
            std::cout << "│ BMCA Parameters:                                                   │\n";
            std::cout << "│   Priority1: " << std::setw(3) << static_cast<int>(info.priority1)
                      << "  Priority2: " << std::setw(3) << static_cast<int>(info.priority2)
                      << "                                 │\n";
            std::cout << "│   Grandmaster: " << std::setw(48) << std::left
                      << info.grandmaster_id.to_string() << " │\n";
        }

        if (info.rate_ratio) {
            std::cout << "├────────────────────────────────────────────────────────────────────┤\n";
            double ppm = (*info.rate_ratio - 1.0) * 1e6;
            std::cout << "│ Rate Ratio: " << std::setprecision(9) << *info.rate_ratio
                      << " (" << std::showpos << std::setprecision(3) << ppm << std::noshowpos
                      << " ppm)                  │\n";
        }

        std::cout << "└────────────────────────────────────────────────────────────────────┘\n\n";
    }

    void print_grandmaster_summary() const {
        std::map<ClockIdentity, int> grandmaster_votes;
        for (const auto& [id, info] : clocks_) {
            if (info.announce_count > 0) {
                grandmaster_votes[info.grandmaster_id]++;
            }
        }

        if (!grandmaster_votes.empty()) {
            std::cout << "👑 Grandmaster Candidates:\n";
            for (const auto& [gm_id, votes] : grandmaster_votes) {
                std::cout << "   " << gm_id.to_string() << " (" << votes << " votes)\n";
            }
            std::cout << "\n";
        }
    }

    std::map<ClockIdentity, ClockInfo> clocks_;
};

// =============================================================================
// Packet Processor
// =============================================================================

class GptpPacketProcessor {
public:
    void process_packet(const PacketView& view) {
        packet_count_++;

        // Decode the packet stack
        auto result = decode_packet(view.data());
        if (!result.complete) {
            return;
        }

        // Check for gPTP header
        if (!result.has_layer<GptpHeader>()) {
            return;
        }

        gptp_count_++;
        const auto* header = result.get_layer<GptpHeader>();

        // Track message types
        message_counts_[header->message_type]++;

        // Print message info
        print_gptp_message(*header);

        // Update tracker
        tracker_.process_gptp(*header);
    }

    void print_stats() const {
        std::cout << "\n📊 Capture Statistics:\n";
        std::cout << "   Total packets:   " << packet_count_ << "\n";
        std::cout << "   gPTP packets:    " << gptp_count_ << "\n";
        std::cout << "\n   Message breakdown:\n";
        for (const auto& [type, count] : message_counts_) {
            std::cout << "     " << std::setw(20) << std::left
                      << message_type_string(type) << ": " << count << "\n";
        }
    }

    void print_summary() const {
        tracker_.print_summary();
    }

private:
    void print_gptp_message(const GptpHeader& header) const {
        const char* emoji = "📦";
        switch (header.message_type) {
            case MessageType::Sync:
                emoji = "🔄";
                break;
            case MessageType::Follow_Up:
                emoji = "📨";
                break;
            case MessageType::Pdelay_Req:
                emoji = "📡";
                break;
            case MessageType::Pdelay_Resp:
                emoji = "📥";
                break;
            case MessageType::Pdelay_Resp_Follow_Up:
                emoji = "📩";
                break;
            case MessageType::Announce:
                emoji = "📢";
                break;
            case MessageType::Signaling:
                emoji = "🔔";
                break;
            default:
                break;
        }

        std::cout << emoji << " [" << std::setw(15) << std::left
                  << message_type_string(header.message_type) << "] "
                  << "Domain=" << std::setw(3) << static_cast<int>(header.domain_number)
                  << " Seq=" << std::setw(5) << header.sequence_id << " "
                  << "From: " << header.source_port_identity.clock_identity.to_string()
                  << ":" << header.source_port_identity.port_number;

        // Print two-step flag if set
        if (header.is_two_step()) {
            std::cout << " [2-step]";
        }

        std::cout << "\n";

        // Print additional info for specific message types
        if (header.body && std::holds_alternative<AnnounceMessage>(*header.body)) {
            const auto& ann = std::get<AnnounceMessage>(*header.body);
            std::cout << "      └─ GM: " << ann.grandmaster_identity.to_string()
                      << " Pri1=" << static_cast<int>(ann.grandmaster_priority1)
                      << " Pri2=" << static_cast<int>(ann.grandmaster_priority2)
                      << " Steps=" << ann.steps_removed << "\n";
        }
    }

    ClockTracker tracker_;
    std::size_t packet_count_ = 0;
    std::size_t gptp_count_ = 0;
    std::map<MessageType, std::size_t> message_counts_;
};

// =============================================================================
// Main
// =============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <interface|pcap_file>\n"
              << "\n"
              << "Monitor gPTP (IEEE 802.1AS) time synchronization traffic.\n"
              << "\n"
              << "gPTP is used in automotive Ethernet for:\n"
              << "  - Time-Sensitive Networking (TSN)\n"
              << "  - Audio/Video Bridging (AVB)\n"
              << "  - AUTOSAR timing requirements\n"
              << "\n"
              << "Arguments:\n"
              << "  interface    Network interface for live capture (e.g., eth0)\n"
              << "  pcap_file    PCAP file for offline analysis\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " eth0           # Live capture\n"
              << "  " << prog << " gptp_traffic.pcap   # Analyze PCAP file\n";
}

int main(int argc, char* argv[]) {
    std::cout << "𓆓 Wadjet-Link gPTP (IEEE 802.1AS) Monitor\n\n";

    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string source = argv[1];
    GptpPacketProcessor processor;

    // Check if source is a file or interface
    if (std::filesystem::exists(source)) {
        // PCAP file mode
        std::cout << "📁 Reading from PCAP file: " << source << "\n\n";

        auto reader_result = pcap::PcapReader::open(source);
        if (!reader_result) {
            std::cerr << "Error opening PCAP file: " << reader_result.error().message << "\n";
            return 1;
        }

        auto& reader = reader_result.value();
        while (auto packet = reader.next_packet()) {
            processor.process_packet(packet->view());
        }
    } else {
        // Live capture mode
        std::cout << "🔴 Live capture on interface: " << source << "\n";
        std::cout << "   Filtering: ether proto 0x88f7 (gPTP/PTP)\n";
        std::cout << "   Press Ctrl+C to stop\n\n";

        io::CaptureSessionOptions opts;
        opts.promiscuous = true;

        auto session_result = io::CaptureSession::create(source, opts);
        if (!session_result) {
            std::cerr << "Error creating capture session: "
                      << session_result.error().message << "\n";
            return 1;
        }

        auto& session = session_result.value();

        // Set BPF filter for gPTP EtherType
        auto filter_result = session.set_filter("ether proto 0x88f7");
        if (!filter_result) {
            std::cerr << "Warning: Could not set filter: "
                      << filter_result.error().message << "\n";
        }

        // Start capture
        auto start_result = session.start();
        if (!start_result) {
            std::cerr << "Error starting capture: "
                      << start_result.error().message << "\n";
            return 1;
        }

        // Capture loop with 5 second timeout
        while (session.is_running()) {
            if (auto packet = session.next_packet(5s)) {
                processor.process_packet(packet->view());
            }
        }
    }

    // Print results
    processor.print_stats();
    processor.print_summary();

    return 0;
}

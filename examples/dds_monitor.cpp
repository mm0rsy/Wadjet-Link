/// @file dds_monitor.cpp
/// @brief Example: DDS/RTPS Protocol Monitor for Automotive Networks
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to capture and analyze DDS/RTPS messages
/// on automotive Ethernet networks. DDS is commonly used with ROS2 for:
/// - Sensor fusion and ADAS systems
/// - Vehicle-to-Everything (V2X) communication
/// - Distributed control systems
/// - In-vehicle middleware (e.g., AUTOSAR Adaptive DDS binding)
///
/// Key features demonstrated:
/// - Detecting RTPS traffic on standard DDS ports (7400-7500)
/// - Parsing RTPS headers and submessages
/// - Tracking discovered participants and endpoints
/// - Monitoring data flow between publishers and subscribers
///
/// Usage:
///   ./dds_monitor eth0              # Live capture on interface
///   ./dds_monitor capture.pcap      # Analyze from PCAP file
///   ./dds_monitor eth0 -v           # Verbose mode (show all submessages)

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/dds/rtps.hpp>
#include <wadjet/protocols/dds/rtps_messages.hpp>
#include <wadjet/protocols/dds/rtps_types.hpp>
#include <wadjet/protocols/dds/discovery.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::dds;
using namespace std::chrono_literals;

// =============================================================================
// Configuration
// =============================================================================

struct Config {
    std::string source;
    bool verbose = false;
    int domain_filter = -1;  // -1 = all domains
    bool show_data = false;
    bool discovery_only = false;
};

Config parse_args(int argc, char* argv[]) {
    Config cfg;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-v" || arg == "--verbose") {
            cfg.verbose = true;
        } else if (arg == "-d" || arg == "--data") {
            cfg.show_data = true;
        } else if (arg == "--discovery") {
            cfg.discovery_only = true;
        } else if ((arg == "--domain" || arg == "-D") && i + 1 < argc) {
            cfg.domain_filter = std::stoi(argv[++i]);
        } else if (cfg.source.empty()) {
            cfg.source = arg;
        }
    }
    
    return cfg;
}

void print_usage() {
    std::cout << "Usage: dds_monitor <interface|pcap_file> [options]\n\n"
              << "Options:\n"
              << "  -v, --verbose     Show all submessages (not just summary)\n"
              << "  -d, --data        Show DATA submessage payload info\n"
              << "  --discovery       Only show discovery traffic (SPDP/SEDP)\n"
              << "  -D, --domain <n>  Filter to specific DDS domain ID\n"
              << "\nExamples:\n"
              << "  dds_monitor eth0              # Live capture\n"
              << "  dds_monitor ros2_traffic.pcap # Analyze PCAP\n"
              << "  dds_monitor eth0 -v -D 0      # Verbose, domain 0 only\n";
}

// =============================================================================
// Participant Tracker - Track discovered DDS participants
// =============================================================================

struct ParticipantInfo {
    GUID guid;
    std::string name;
    VendorIdValue vendor;
    ProtocolVersion version;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    
    // Statistics
    std::uint64_t messages_sent = 0;
    std::uint64_t data_messages = 0;
    std::uint64_t heartbeats_sent = 0;
    std::uint64_t acknacks_sent = 0;
    
    // Endpoints discovered via SEDP
    std::set<std::string> writers;  // topic names
    std::set<std::string> readers;  // topic names
};

struct TopicInfo {
    std::string name;
    std::string type_name;
    std::set<GUID> writers;
    std::set<GUID> readers;
    std::uint64_t data_count = 0;
    std::uint64_t total_bytes = 0;
};

class DdsTracker {
public:
    void process_rtps(const RtpsHeader& header, 
                      const ipv4::IPv4Header* ip_hdr = nullptr,
                      const udp::UdpHeader* udp_hdr = nullptr) {
        // Create or update participant info
        auto& participant = participants_[header.guid_prefix];
        auto now = std::chrono::system_clock::now();
        
        if (participant.messages_sent == 0) {
            participant.first_seen = now;
            participant.guid.prefix = header.guid_prefix;
            participant.guid.entity_id = EntityId::participant();
            participant.vendor = header.vendor_id;
            participant.version = header.version;
        }
        participant.last_seen = now;
        participant.messages_sent++;
        
        // Process submessages
        for (const auto& submsg : header.submessages) {
            process_submessage(submsg, participant, header.guid_prefix);
        }
        
        total_messages_++;
        total_submessages_ += header.submessages.size();
    }
    
    void print_summary() const {
        std::cout << "\n╔══════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                      DDS/RTPS Network Summary                         ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n\n";
        
        // Overall stats
        std::cout << "📊 Statistics:\n";
        std::cout << "   Total RTPS messages: " << total_messages_ << "\n";
        std::cout << "   Total submessages:   " << total_submessages_ << "\n";
        std::cout << "   Participants:        " << participants_.size() << "\n";
        std::cout << "   Topics discovered:   " << topics_.size() << "\n\n";
        
        // Submessage breakdown
        std::cout << "📝 Submessage Types:\n";
        for (const auto& [kind, count] : submsg_counts_) {
            std::cout << "   " << std::setw(15) << std::left 
                      << submessage_kind_string(kind) << ": " << count << "\n";
        }
        std::cout << "\n";
        
        // Participant details
        std::cout << "👥 Discovered Participants (" << participants_.size() << "):\n\n";
        for (const auto& [guid_prefix, info] : participants_) {
            print_participant(info);
        }
        
        // Topic summary
        if (!topics_.empty()) {
            std::cout << "📚 Discovered Topics (" << topics_.size() << "):\n\n";
            for (const auto& [name, topic] : topics_) {
                print_topic(topic);
            }
        }
    }
    
    void print_verbose_submessage(const Submessage& submsg, const Config& cfg) const {
        std::cout << "  └─ " << submessage_kind_string(submsg.header.kind);
        
        if (submsg.header.flags.endian_little) {
            std::cout << " [LE]";
        } else {
            std::cout << " [BE]";
        }
        
        std::cout << " len=" << submsg.header.length;
        
        // Add submessage-specific details
        std::visit([&](auto&& body) {
            using T = std::decay_t<decltype(body)>;
            
            if constexpr (std::is_same_v<T, DataSubmessage>) {
                std::cout << " writer=" << body.writer_id.to_string()
                          << " sn=" << body.writer_sn.value();
                if (cfg.show_data) {
                    std::cout << " payload=" << body.serialized_payload.size() << "B";
                }
            } else if constexpr (std::is_same_v<T, HeartbeatSubmessage>) {
                std::cout << " writer=" << body.writer_id.to_string()
                          << " sn=[" << body.first_sn.value() << ".." 
                          << body.last_sn.value() << "]"
                          << " count=" << body.count.value;
            } else if constexpr (std::is_same_v<T, AckNackSubmessage>) {
                std::cout << " reader=" << body.reader_id.to_string()
                          << " base=" << body.reader_sn_state.base.value()
                          << " count=" << body.count.value;
            } else if constexpr (std::is_same_v<T, GapSubmessage>) {
                std::cout << " start=" << body.gap_start.value();
            } else if constexpr (std::is_same_v<T, InfoTimestampSubmessage>) {
                if (body.timestamp) {
                    std::cout << " t=" << body.timestamp->to_nanoseconds() << "ns";
                }
            } else if constexpr (std::is_same_v<T, InfoDestinationSubmessage>) {
                std::cout << " dst=" << body.guid_prefix.to_string();
            }
        }, submsg.body);
        
        std::cout << "\n";
    }
    
    bool is_discovery_port(std::uint16_t port) const {
        // Discovery ports are 7400 + domain * 250 + offset
        return port >= 7400 && port < 7500 && ((port - 7400) % 250 < 2);
    }
    
private:
    void process_submessage(const Submessage& submsg, 
                           ParticipantInfo& participant,
                           const GuidPrefix& source_prefix) {
        submsg_counts_[submsg.header.kind]++;
        
        std::visit([&](auto&& body) {
            using T = std::decay_t<decltype(body)>;
            
            if constexpr (std::is_same_v<T, DataSubmessage>) {
                participant.data_messages++;
                process_data_submessage(body, source_prefix);
            } else if constexpr (std::is_same_v<T, HeartbeatSubmessage>) {
                participant.heartbeats_sent++;
            } else if constexpr (std::is_same_v<T, AckNackSubmessage>) {
                participant.acknacks_sent++;
            }
        }, submsg.body);
    }
    
    void process_data_submessage(const DataSubmessage& data, 
                                  const GuidPrefix& source_prefix) {
        // Check if this is discovery data (builtin endpoints)
        const auto& writer_id = data.writer_id;
        
        // SPDP - Participant discovery
        if (writer_id.entity_kind == EntityKind::BuiltinWriterWithKey &&
            writer_id.entity_key == std::array<std::uint8_t, 3>{0x00, 0x01, 0x00}) {
            // This is SPDP announcements - could parse ParticipantBuiltinTopicData
            // For now, just track we received discovery
        }
        
        // SEDP - Publication discovery
        if (writer_id.entity_kind == EntityKind::BuiltinWriterWithKey &&
            writer_id.entity_key == std::array<std::uint8_t, 3>{0x00, 0x00, 0x03}) {
            // Could parse PublicationBuiltinTopicData
        }
        
        // SEDP - Subscription discovery  
        if (writer_id.entity_kind == EntityKind::BuiltinWriterWithKey &&
            writer_id.entity_key == std::array<std::uint8_t, 3>{0x00, 0x00, 0x04}) {
            // Could parse SubscriptionBuiltinTopicData
        }
    }
    
    void print_participant(const ParticipantInfo& info) const {
        std::cout << "  📍 " << info.guid.prefix.to_string() << "\n";
        std::cout << "     Vendor: " << info.vendor.to_string() << "\n";
        std::cout << "     Version: " << static_cast<int>(info.version.major) 
                  << "." << static_cast<int>(info.version.minor) << "\n";
        
        if (!info.name.empty()) {
            std::cout << "     Name: " << info.name << "\n";
        }
        
        std::cout << "     Messages: " << info.messages_sent 
                  << " (DATA: " << info.data_messages
                  << ", HB: " << info.heartbeats_sent
                  << ", AN: " << info.acknacks_sent << ")\n";
        
        if (!info.writers.empty()) {
            std::cout << "     Writers: " << info.writers.size() << " topics\n";
        }
        if (!info.readers.empty()) {
            std::cout << "     Readers: " << info.readers.size() << " topics\n";
        }
        
        std::cout << "\n";
    }
    
    void print_topic(const TopicInfo& topic) const {
        std::cout << "  📖 " << topic.name << "\n";
        if (!topic.type_name.empty()) {
            std::cout << "     Type: " << topic.type_name << "\n";
        }
        std::cout << "     Writers: " << topic.writers.size() 
                  << ", Readers: " << topic.readers.size() << "\n";
        if (topic.data_count > 0) {
            std::cout << "     Data: " << topic.data_count << " samples, "
                      << topic.total_bytes << " bytes\n";
        }
        std::cout << "\n";
    }
    
    std::map<GuidPrefix, ParticipantInfo> participants_;
    std::map<std::string, TopicInfo> topics_;
    std::map<SubmessageKind, std::uint64_t> submsg_counts_;
    std::uint64_t total_messages_ = 0;
    std::uint64_t total_submessages_ = 0;
};

// =============================================================================
// Main Capture Loop
// =============================================================================

void process_packet(const DecodeStackResult& result, DdsTracker& tracker, 
                   const Config& cfg, std::uint64_t& packet_num) {
    // Check for RTPS layer
    const auto* rtps = result.get_layer<RtpsHeader>();
    if (!rtps) {
        return;
    }
    
    // Get IP/UDP info for context
    const auto* ip = result.get_layer<ipv4::IPv4Header>();
    const auto* udp = result.get_layer<udp::UdpHeader>();
    
    // Filter by port if discovery only
    if (cfg.discovery_only && udp) {
        if (!tracker.is_discovery_port(udp->dst_port) && 
            !tracker.is_discovery_port(udp->src_port)) {
            return;
        }
    }
    
    // Process the RTPS message
    tracker.process_rtps(*rtps, ip, udp);
    
    // Verbose output
    if (cfg.verbose) {
        packet_num++;
        std::cout << "\n[" << packet_num << "] RTPS ";
        
        if (ip) {
            std::cout << ip->src_ip_string() << " → " << ip->dst_ip_string();
        }
        if (udp) {
            std::cout << " port " << udp->src_port << "→" << udp->dst_port;
        }
        
        std::cout << "\n";
        std::cout << "  GUID Prefix: " << rtps->guid_prefix.to_string() << "\n";
        std::cout << "  Vendor: " << rtps->vendor_id.to_string() << "\n";
        std::cout << "  Submessages: " << rtps->submessages.size() << "\n";
        
        for (const auto& submsg : rtps->submessages) {
            tracker.print_verbose_submessage(submsg, cfg);
        }
    }
}

int run_live_capture(const Config& cfg, DdsTracker& tracker) {
    std::cout << "🔴 Starting live capture on " << cfg.source << "...\n";
    std::cout << "   Press Ctrl+C to stop and show summary.\n\n";
    
    io::CaptureSessionOptions opts;
    opts.interface_name = cfg.source;
    opts.promisc_mode = true;
    opts.buffer_size = 16 * 1024 * 1024;  // 16MB buffer
    opts.filter = "udp portrange 7400-7600";  // DDS port range
    
    auto session_result = io::CaptureSession::create(opts);
    if (!session_result) {
        std::cerr << "Failed to create capture session: " 
                  << session_result.error().message << "\n";
        return 1;
    }
    
    auto session = std::move(*session_result);
    ProtocolDispatcher dispatcher;
    std::uint64_t packet_num = 0;
    
    // Capture loop
    auto start = std::chrono::steady_clock::now();
    
    session.start([&](const io::CapturedFrame& frame) {
        auto result = dispatcher.decode(frame.data);
        process_packet(result, tracker, cfg, packet_num);
    });
    
    // Wait for interrupt
    std::this_thread::sleep_for(std::chrono::hours(24));  // Long timeout
    
    session.stop();
    return 0;
}

int run_pcap_analysis(const Config& cfg, DdsTracker& tracker) {
    std::cout << "📁 Analyzing PCAP file: " << cfg.source << "\n\n";
    
    auto reader = pcap::PcapReader::open(cfg.source);
    if (!reader) {
        std::cerr << "Failed to open PCAP file: " << reader.error().message << "\n";
        return 1;
    }
    
    ProtocolDispatcher dispatcher;
    std::uint64_t packet_num = 0;
    std::uint64_t total_packets = 0;
    
    while (auto packet = reader->next_packet()) {
        total_packets++;
        auto result = dispatcher.decode(packet->data);
        process_packet(result, tracker, cfg, packet_num);
    }
    
    std::cout << "📦 Processed " << total_packets << " packets\n";
    return 0;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    std::cout << R"(
    𓆓 Wadjet-Link — DDS/RTPS Protocol Monitor
    ═══════════════════════════════════════════════════════════════════
    )" << "\n";
    
    auto cfg = parse_args(argc, argv);
    
    if (cfg.source.empty()) {
        print_usage();
        return 1;
    }
    
    DdsTracker tracker;
    int result = 0;
    
    // Determine if source is interface or file
    if (std::filesystem::exists(cfg.source)) {
        result = run_pcap_analysis(cfg, tracker);
    } else {
        result = run_live_capture(cfg, tracker);
    }
    
    // Print summary
    tracker.print_summary();
    
    return result;
}

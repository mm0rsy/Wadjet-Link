/// @file ros2_analyzer.cpp
/// @brief Example: ROS2 Traffic Analyzer using DDS/RTPS Protocol Decoder
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to analyze ROS2 network traffic by decoding
/// the underlying DDS/RTPS protocol. ROS2 uses DDS as its middleware layer,
/// allowing deep inspection of node communication patterns.
///
/// Features:
/// - Node discovery tracking via SPDP
/// - Topic and service enumeration via SEDP
/// - Message frequency analysis
/// - Publisher/subscriber relationship mapping
/// - QoS policy inspection
///
/// Usage:
///   ./ros2_analyzer eth0                # Live capture on interface
///   ./ros2_analyzer capture.pcap        # Analyze from PCAP file
///   ./ros2_analyzer eth0 --domain 1     # Specify ROS2 domain ID

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/dds/rtps.hpp>
#include <wadjet/protocols/dds/rtps_types.hpp>
#include <wadjet/protocols/dds/rtps_messages.hpp>
#include <wadjet/protocols/dds/discovery.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::dds;
using namespace std::chrono_literals;

// =============================================================================
// ROS2 Node Information
// =============================================================================

struct Ros2Node {
    std::string name;
    std::string namespace_;
    GUID participant_guid;
    VendorId vendor;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    std::uint32_t message_count = 0;
    
    // Associated endpoints
    std::set<std::string> published_topics;
    std::set<std::string> subscribed_topics;
    std::set<std::string> services;
    std::set<std::string> actions;
    
    std::string full_name() const {
        if (namespace_.empty() || namespace_ == "/") {
            return "/" + name;
        }
        return namespace_ + "/" + name;
    }
};

// =============================================================================
// ROS2 Topic Information
// =============================================================================

struct Ros2Topic {
    std::string name;
    std::string type_name;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    
    // Statistics
    std::uint64_t message_count = 0;
    std::uint64_t byte_count = 0;
    std::vector<std::chrono::system_clock::time_point> message_times;
    
    // Publishers and subscribers
    std::set<GUID> publishers;
    std::set<GUID> subscribers;
    
    // QoS
    DurabilityKind durability = DurabilityKind::Volatile;
    ReliabilityKind reliability = ReliabilityKind::BestEffort;
    HistoryKind history = HistoryKind::KeepLast;
    std::int32_t history_depth = 1;
    
    double frequency_hz() const {
        if (message_times.size() < 2) return 0.0;
        
        auto duration = message_times.back() - message_times.front();
        auto seconds = std::chrono::duration<double>(duration).count();
        
        if (seconds <= 0.0) return 0.0;
        return (message_times.size() - 1) / seconds;
    }
};

// =============================================================================
// ROS2 Traffic Analyzer
// =============================================================================

class Ros2Analyzer {
public:
    explicit Ros2Analyzer(std::uint32_t domain_id = 0) 
        : domain_id_(domain_id) {
        // Calculate expected ports for this domain
        // PB=7400, DG=250, PG=2
        discovery_multicast_port_ = 7400 + 250 * domain_id;
        user_multicast_port_ = 7401 + 250 * domain_id;
    }
    
    void process_packet(const Packet& packet) {
        auto result = dispatcher_.decode_packet(packet.view());
        
        if (auto* rtps = result.get_layer<RtpsHeader>()) {
            process_rtps(*rtps, packet.timestamp());
        }
    }
    
    void process_rtps(const RtpsHeader& header, const Timestamp& ts) {
        auto now = std::chrono::system_clock::now();
        
        // Track participant
        auto& node = get_or_create_node(header.guid_prefix);
        node.last_seen = now;
        node.vendor = header.vendor_id.to_vendor();
        node.message_count++;
        
        // Process submessages
        for (const auto& submsg : header.submessages) {
            process_submessage(submsg, header, now);
        }
    }
    
    void print_summary() const {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                     ROS2 Traffic Analysis                        ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        print_nodes();
        print_topics();
        print_communication_graph();
    }
    
    void print_nodes() const {
        std::cout << "┌────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ Discovered ROS2 Nodes                                          │\n";
        std::cout << "├────────────────────────────────────────────────────────────────┤\n";
        
        if (nodes_.empty()) {
            std::cout << "│ No nodes discovered yet                                        │\n";
        } else {
            for (const auto& [guid, node] : nodes_) {
                std::cout << "│ " << std::setw(30) << std::left << node.full_name();
                std::cout << " [" << to_string(node.vendor) << "]";
                std::cout << std::setw(10) << "" << "│\n";
                
                if (!node.published_topics.empty()) {
                    std::cout << "│   Publishers:                                                  │\n";
                    for (const auto& topic : node.published_topics) {
                        std::cout << "│     → " << std::setw(55) << std::left << topic << "│\n";
                    }
                }
                
                if (!node.subscribed_topics.empty()) {
                    std::cout << "│   Subscribers:                                                 │\n";
                    for (const auto& topic : node.subscribed_topics) {
                        std::cout << "│     ← " << std::setw(55) << std::left << topic << "│\n";
                    }
                }
            }
        }
        
        std::cout << "└────────────────────────────────────────────────────────────────┘\n\n";
    }
    
    void print_topics() const {
        std::cout << "┌────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ Active Topics                                                  │\n";
        std::cout << "├────────────────────────────────────────────────────────────────┤\n";
        
        if (topics_.empty()) {
            std::cout << "│ No topics discovered yet                                       │\n";
        } else {
            for (const auto& [name, topic] : topics_) {
                std::cout << "│ " << std::setw(40) << std::left << name;
                std::cout << std::fixed << std::setprecision(1) 
                          << std::setw(8) << std::right << topic.frequency_hz() << " Hz │\n";
                std::cout << "│   Type: " << std::setw(53) << std::left << topic.type_name << "│\n";
                std::cout << "│   Pubs: " << topic.publishers.size() 
                          << "  Subs: " << topic.subscribers.size();
                std::cout << "  Msgs: " << topic.message_count;
                std::cout << std::setw(25) << "" << "│\n";
                std::cout << "│   QoS: " << qos_string(topic) << std::setw(30) << "" << "│\n";
            }
        }
        
        std::cout << "└────────────────────────────────────────────────────────────────┘\n\n";
    }
    
    void print_communication_graph() const {
        std::cout << "┌────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ Communication Graph                                            │\n";
        std::cout << "├────────────────────────────────────────────────────────────────┤\n";
        
        // Find publisher-subscriber pairs
        for (const auto& [topic_name, topic] : topics_) {
            if (topic.publishers.empty() || topic.subscribers.empty()) {
                continue;
            }
            
            std::cout << "│ " << std::setw(62) << std::left << topic_name << "│\n";
            
            for (const auto& pub_guid : topic.publishers) {
                std::string pub_name = "Unknown";
                if (auto it = nodes_.find(pub_guid.prefix); it != nodes_.end()) {
                    pub_name = it->second.full_name();
                }
                
                for (const auto& sub_guid : topic.subscribers) {
                    std::string sub_name = "Unknown";
                    if (auto it = nodes_.find(sub_guid.prefix); it != nodes_.end()) {
                        sub_name = it->second.full_name();
                    }
                    
                    std::cout << "│   " << std::setw(20) << std::left << pub_name;
                    std::cout << " → ";
                    std::cout << std::setw(35) << std::left << sub_name << "│\n";
                }
            }
        }
        
        if (topics_.empty()) {
            std::cout << "│ No communication patterns detected                             │\n";
        }
        
        std::cout << "└────────────────────────────────────────────────────────────────┘\n";
    }

private:
    Ros2Node& get_or_create_node(const GuidPrefix& prefix) {
        if (auto it = nodes_.find(prefix); it != nodes_.end()) {
            return it->second;
        }
        
        Ros2Node node;
        node.participant_guid.prefix = prefix;
        node.first_seen = std::chrono::system_clock::now();
        node.last_seen = node.first_seen;
        node.name = "node_" + prefix.to_string().substr(0, 8);
        node.namespace_ = "/";
        
        return nodes_.emplace(prefix, std::move(node)).first->second;
    }
    
    Ros2Topic& get_or_create_topic(const std::string& name) {
        if (auto it = topics_.find(name); it != topics_.end()) {
            return it->second;
        }
        
        Ros2Topic topic;
        topic.name = name;
        topic.first_seen = std::chrono::system_clock::now();
        topic.last_seen = topic.first_seen;
        
        return topics_.emplace(name, std::move(topic)).first->second;
    }
    
    void process_submessage(const Submessage& submsg, const RtpsHeader& header,
                           std::chrono::system_clock::time_point now) {
        if (std::holds_alternative<DataSubmessage>(submsg.body)) {
            const auto& data = std::get<DataSubmessage>(submsg.body);
            process_data_submessage(data, header, now);
        }
    }
    
    void process_data_submessage(const DataSubmessage& data, const RtpsHeader& header,
                                 std::chrono::system_clock::time_point now) {
        // Check if this is discovery data
        if (is_spdp_writer(data.writer_id)) {
            process_spdp_data(data, header);
        } else if (is_sedp_publications_writer(data.writer_id)) {
            process_sedp_publications(data, header);
        } else if (is_sedp_subscriptions_writer(data.writer_id)) {
            process_sedp_subscriptions(data, header);
        } else {
            // User data
            process_user_data(data, header, now);
        }
    }
    
    void process_spdp_data(const DataSubmessage& data, const RtpsHeader& header) {
        DiscoveryParser parser;
        auto participant = parser.parse_spdp(data);
        
        if (participant) {
            auto& node = get_or_create_node(header.guid_prefix);
            
            // Extract ROS2 node name from participant name
            // ROS2 encodes node info in the participant name
            if (!participant->participant_name.empty()) {
                parse_ros2_node_name(participant->participant_name, node);
            }
        }
    }
    
    void process_sedp_publications(const DataSubmessage& data, const RtpsHeader& header) {
        DiscoveryParser parser;
        auto endpoint = parser.parse_sedp(data);
        
        if (endpoint) {
            auto& node = get_or_create_node(header.guid_prefix);
            node.published_topics.insert(endpoint->topic_name);
            
            auto& topic = get_or_create_topic(endpoint->topic_name);
            topic.type_name = endpoint->type_name;
            topic.publishers.insert(endpoint->endpoint_guid);
            topic.durability = endpoint->qos.durability.kind;
            topic.reliability = endpoint->qos.reliability.kind;
            topic.history = endpoint->qos.history.kind;
            topic.history_depth = endpoint->qos.history.depth;
        }
    }
    
    void process_sedp_subscriptions(const DataSubmessage& data, const RtpsHeader& header) {
        DiscoveryParser parser;
        auto endpoint = parser.parse_sedp(data);
        
        if (endpoint) {
            auto& node = get_or_create_node(header.guid_prefix);
            node.subscribed_topics.insert(endpoint->topic_name);
            
            auto& topic = get_or_create_topic(endpoint->topic_name);
            topic.type_name = endpoint->type_name;
            topic.subscribers.insert(endpoint->endpoint_guid);
        }
    }
    
    void process_user_data(const DataSubmessage& data, const RtpsHeader& header,
                          std::chrono::system_clock::time_point now) {
        // Track message statistics for known topics
        // In a full implementation, we would map writer_id to topic
        // For now, we update statistics for all topics from this participant
        
        auto& node = get_or_create_node(header.guid_prefix);
        
        for (const auto& topic_name : node.published_topics) {
            if (auto it = topics_.find(topic_name); it != topics_.end()) {
                it->second.message_count++;
                it->second.byte_count += data.serialized_payload.size();
                it->second.last_seen = now;
                it->second.message_times.push_back(now);
                
                // Keep only last 100 message times for frequency calculation
                if (it->second.message_times.size() > 100) {
                    it->second.message_times.erase(
                        it->second.message_times.begin(),
                        it->second.message_times.begin() + 50);
                }
            }
        }
    }
    
    void parse_ros2_node_name(const std::string& participant_name, Ros2Node& node) {
        // ROS2 participant names typically follow pattern:
        // /namespace/node_name or just /node_name
        // Sometimes encoded as: name=node_name;namespace=/ns
        
        if (participant_name.empty()) return;
        
        // Try to parse as path-style name
        if (participant_name[0] == '/') {
            auto last_slash = participant_name.rfind('/');
            if (last_slash != std::string::npos && last_slash > 0) {
                node.namespace_ = participant_name.substr(0, last_slash);
                node.name = participant_name.substr(last_slash + 1);
            } else {
                node.namespace_ = "/";
                node.name = participant_name.substr(1);
            }
        } else {
            // Use as-is
            node.name = participant_name;
        }
    }
    
    static bool is_spdp_writer(const EntityId& id) {
        // SPDP builtin participant writer: 0x000100c2
        return id.entity_key[0] == 0x00 && 
               id.entity_key[1] == 0x01 && 
               id.entity_key[2] == 0x00 &&
               id.entity_kind == EntityKind::BuiltinWriter;
    }
    
    static bool is_sedp_publications_writer(const EntityId& id) {
        // SEDP publications writer: 0x000003c2
        return id.entity_key[0] == 0x00 && 
               id.entity_key[1] == 0x00 && 
               id.entity_key[2] == 0x03 &&
               id.entity_kind == EntityKind::BuiltinWriter;
    }
    
    static bool is_sedp_subscriptions_writer(const EntityId& id) {
        // SEDP subscriptions writer: 0x000004c2
        return id.entity_key[0] == 0x00 && 
               id.entity_key[1] == 0x00 && 
               id.entity_key[2] == 0x04 &&
               id.entity_kind == EntityKind::BuiltinWriter;
    }
    
    static std::string qos_string(const Ros2Topic& topic) {
        std::string result;
        
        switch (topic.reliability) {
            case ReliabilityKind::Reliable:
                result += "Reliable";
                break;
            case ReliabilityKind::BestEffort:
                result += "BestEffort";
                break;
        }
        
        result += "/";
        
        switch (topic.durability) {
            case DurabilityKind::Volatile:
                result += "Volatile";
                break;
            case DurabilityKind::TransientLocal:
                result += "TransientLocal";
                break;
            case DurabilityKind::Transient:
                result += "Transient";
                break;
            case DurabilityKind::Persistent:
                result += "Persistent";
                break;
        }
        
        return result;
    }
    
    std::uint32_t domain_id_;
    std::uint16_t discovery_multicast_port_;
    std::uint16_t user_multicast_port_;
    
    ProtocolDispatcher dispatcher_;
    std::map<GuidPrefix, Ros2Node> nodes_;
    std::map<std::string, Ros2Topic> topics_;
};

// =============================================================================
// Main
// =============================================================================

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " <interface|pcap_file> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --domain <id>    ROS2 domain ID (default: 0)\n";
    std::cout << "  --duration <s>   Capture duration in seconds (default: 30)\n";
    std::cout << "  --help           Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program << " eth0                    # Live capture on eth0\n";
    std::cout << "  " << program << " ros2_traffic.pcap       # Analyze PCAP file\n";
    std::cout << "  " << program << " eth0 --domain 1         # Use domain ID 1\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string source = argv[1];
    std::uint32_t domain_id = 0;
    int duration_seconds = 30;
    
    // Parse options
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--domain" && i + 1 < argc) {
            domain_id = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--duration" && i + 1 < argc) {
            duration_seconds = std::stoi(argv[++i]);
        }
    }
    
    Ros2Analyzer analyzer(domain_id);
    
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              𓆓 Wadjet-Link ROS2 Traffic Analyzer                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Domain ID: " << domain_id << "\n";
    
    // Check if source is a file or interface
    if (std::filesystem::exists(source)) {
        // PCAP file analysis
        std::cout << "Analyzing PCAP file: " << source << "\n\n";
        
        pcap::PcapReader reader(source);
        std::uint64_t packet_count = 0;
        
        while (auto packet = reader.next_packet()) {
            analyzer.process_packet(*packet);
            packet_count++;
            
            if (packet_count % 1000 == 0) {
                std::cout << "\rProcessed " << packet_count << " packets..." << std::flush;
            }
        }
        
        std::cout << "\rProcessed " << packet_count << " packets total.\n";
    } else {
        // Live capture
        std::cout << "Starting live capture on interface: " << source << "\n";
        std::cout << "Capture duration: " << duration_seconds << " seconds\n\n";
        
        // Calculate BPF filter for DDS ports
        std::uint16_t port_base = 7400 + 250 * domain_id;
        std::string filter = "udp portrange " + std::to_string(port_base) + "-" +
                            std::to_string(port_base + 100);
        
        std::cout << "BPF filter: " << filter << "\n\n";
        
        io::CaptureSessionOptions options;
        options.promiscuous = true;
        options.immediate_mode = true;
        
        auto session_result = io::CaptureSession::open(source, options);
        if (!session_result) {
            std::cerr << "Error: Failed to open capture session on " << source << "\n";
            std::cerr << "Make sure you have permission (try running with sudo)\n";
            return 1;
        }
        
        auto& session = *session_result;
        session.set_filter(filter);
        
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        std::cout << "Capturing...\n";
        
        while (std::chrono::steady_clock::now() < end_time) {
            if (auto packet = session.next_packet(1000)) {
                analyzer.process_packet(*packet);
            }
            
            // Print progress
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            std::cout << "\rElapsed: " << elapsed_sec << "/" << duration_seconds << "s" << std::flush;
        }
        
        std::cout << "\n";
    }
    
    // Print analysis results
    analyzer.print_summary();
    
    return 0;
}

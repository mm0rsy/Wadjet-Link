/// @file someip_discovery.cpp
/// @brief Example: SOME/IP Service Discovery monitor
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to capture and decode SOME/IP Service Discovery
/// messages to monitor service offers, finds, and subscriptions on the network.
///
/// Usage:
///   ./someip_discovery eth0               # Live capture on interface
///   ./someip_discovery capture.pcap       # Analyze from PCAP file

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/someip.hpp>
#include <wadjet/protocols/someip_sd.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace std::chrono_literals;

// =============================================================================
// Service Registry - Track discovered services
// =============================================================================

struct ServiceInfo {
    std::uint16_t service_id;
    std::uint16_t instance_id;
    std::uint8_t major_version;
    std::uint32_t minor_version;
    std::uint32_t ttl;
    std::chrono::system_clock::time_point last_seen;
    std::uint32_t offer_count = 0;
};

class ServiceRegistry {
public:
    void record_offer(const someip_sd::ServiceEntry& entry) {
        auto key = make_key(entry.service_id, entry.instance_id);
        auto& info = services_[key];
        info.service_id = entry.service_id;
        info.instance_id = entry.instance_id;
        info.major_version = entry.major_version;
        info.minor_version = entry.minor_version;
        info.ttl = entry.ttl;
        info.last_seen = std::chrono::system_clock::now();
        info.offer_count++;
    }

    void record_find(std::uint16_t service_id, std::uint16_t instance_id) {
        find_requests_.insert(make_key(service_id, instance_id));
    }

    void record_subscription(std::uint16_t service_id,
                              std::uint16_t instance_id,
                              std::uint16_t eventgroup_id) {
        subscriptions_.insert({make_key(service_id, instance_id), eventgroup_id});
    }

    void print_summary() const {
        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║               SOME/IP Service Discovery Summary              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

        // Print discovered services
        std::cout << "📡 Discovered Services (" << services_.size() << "):\n";
        std::cout << "┌─────────────┬─────────────┬─────────┬─────────┬───────┬───────┐\n";
        std::cout << "│ Service ID  │ Instance ID │ Major   │ Minor   │ TTL   │ Count │\n";
        std::cout << "├─────────────┼─────────────┼─────────┼─────────┼───────┼───────┤\n";

        for (const auto& [key, info] : services_) {
            std::cout << "│ 0x" << std::hex << std::setw(8) << std::setfill('0')
                      << info.service_id << " │ 0x"
                      << std::setw(8) << std::setfill('0') << info.instance_id << " │ "
                      << std::dec << std::setw(7) << std::setfill(' ')
                      << static_cast<int>(info.major_version) << " │ "
                      << std::setw(7) << info.minor_version << " │ "
                      << std::setw(5) << info.ttl << " │ "
                      << std::setw(5) << info.offer_count << " │\n";
        }
        std::cout << "└─────────────┴─────────────┴─────────┴─────────┴───────┴───────┘\n\n";

        // Print find requests
        if (!find_requests_.empty()) {
            std::cout << "🔍 Find Requests (" << find_requests_.size() << "):\n";
            for (const auto& key : find_requests_) {
                auto [sid, iid] = parse_key(key);
                std::cout << "   Service 0x" << std::hex << std::setw(4) << std::setfill('0')
                          << sid << " Instance 0x" << std::setw(4) << iid << "\n";
            }
            std::cout << "\n";
        }

        // Print subscriptions
        if (!subscriptions_.empty()) {
            std::cout << "🔔 Subscriptions (" << subscriptions_.size() << "):\n";
            for (const auto& [key, egid] : subscriptions_) {
                auto [sid, iid] = parse_key(key);
                std::cout << "   Service 0x" << std::hex << std::setw(4) << std::setfill('0')
                          << sid << " Instance 0x" << std::setw(4) << iid
                          << " Eventgroup 0x" << std::setw(4) << egid << "\n";
            }
            std::cout << "\n";
        }
    }

private:
    static std::uint32_t make_key(std::uint16_t service_id, std::uint16_t instance_id) {
        return (static_cast<std::uint32_t>(service_id) << 16) | instance_id;
    }

    static std::pair<std::uint16_t, std::uint16_t> parse_key(std::uint32_t key) {
        return {static_cast<std::uint16_t>(key >> 16), static_cast<std::uint16_t>(key & 0xFFFF)};
    }

    std::map<std::uint32_t, ServiceInfo> services_;
    std::set<std::uint32_t> find_requests_;
    std::set<std::pair<std::uint32_t, std::uint16_t>> subscriptions_;
};

// =============================================================================
// Packet processor
// =============================================================================

class SdPacketProcessor {
public:
    void process_packet(const net::PacketView& view) {
        packet_count_++;

        // Decode the packet stack
        auto result = decode_packet(view.data());
        if (!result.success()) {
            return;
        }

        // Check for SOME/IP header
        if (!result.has_layer<someip::SomeIpHeader>()) {
            return;
        }

        const auto* someip_header = result.get_layer<someip::SomeIpHeader>();
        if (!someip_header->is_service_discovery()) {
            // Not SD, but valid SOME/IP - could track regular messages
            someip_count_++;
            return;
        }

        // Get SD header
        if (!result.has_layer<someip_sd::SomeIpSdHeader>()) {
            return;
        }

        sd_count_++;
        const auto* sd_header = result.get_layer<someip_sd::SomeIpSdHeader>();

        // Track reboot flag
        if (sd_header->is_reboot()) {
            reboot_count_++;
            std::cout << "⚡ Reboot flag detected\n";
        }

        // Process entries
        for (const auto& entry : sd_header->entries) {
            if (std::holds_alternative<someip_sd::ServiceEntry>(entry)) {
                process_service_entry(std::get<someip_sd::ServiceEntry>(entry));
            } else if (std::holds_alternative<someip_sd::EventgroupEntry>(entry)) {
                process_eventgroup_entry(std::get<someip_sd::EventgroupEntry>(entry));
            }
        }
    }

    void print_stats() const {
        std::cout << "\n📊 Statistics:\n";
        std::cout << "   Total packets:     " << packet_count_ << "\n";
        std::cout << "   SOME/IP messages:  " << someip_count_ << "\n";
        std::cout << "   SD messages:       " << sd_count_ << "\n";
        std::cout << "   Reboot flags:      " << reboot_count_ << "\n";
        std::cout << "   Service offers:    " << offer_count_ << "\n";
        std::cout << "   Find requests:     " << find_count_ << "\n";
        std::cout << "   Subscriptions:     " << subscribe_count_ << "\n";
    }

    ServiceRegistry& registry() { return registry_; }

private:
    void process_service_entry(const someip_sd::ServiceEntry& entry) {
        const char* type_str = "???";
        switch (entry.type) {
            case someip_sd::EntryType::FindService:
                type_str = "FIND";
                find_count_++;
                registry_.record_find(entry.service_id, entry.instance_id);
                break;
            case someip_sd::EntryType::OfferService:
                type_str = "OFFER";
                offer_count_++;
                registry_.record_offer(entry);
                break;
            case someip_sd::EntryType::StopOfferService:
                type_str = "STOP_OFFER";
                break;
            default:
                break;
        }

        std::cout << "📢 [" << type_str << "] Service 0x" << std::hex
                  << std::setw(4) << std::setfill('0') << entry.service_id
                  << " Instance 0x" << std::setw(4) << entry.instance_id
                  << " v" << std::dec << static_cast<int>(entry.major_version)
                  << "." << entry.minor_version
                  << " TTL=" << entry.ttl << "s\n";
    }

    void process_eventgroup_entry(const someip_sd::EventgroupEntry& entry) {
        const char* type_str = "???";
        switch (entry.type) {
            case someip_sd::EntryType::SubscribeEventgroup:
                type_str = "SUBSCRIBE";
                subscribe_count_++;
                registry_.record_subscription(
                    entry.service_id, entry.instance_id, entry.eventgroup_id);
                break;
            case someip_sd::EntryType::StopSubscribeEventgroup:
                type_str = "STOP_SUBSCRIBE";
                break;
            case someip_sd::EntryType::SubscribeEventgroupAck:
                type_str = "SUBSCRIBE_ACK";
                break;
            case someip_sd::EntryType::SubscribeEventgroupNack:
                type_str = "SUBSCRIBE_NACK";
                break;
            default:
                break;
        }

        std::cout << "🔔 [" << type_str << "] Service 0x" << std::hex
                  << std::setw(4) << std::setfill('0') << entry.service_id
                  << " Instance 0x" << std::setw(4) << entry.instance_id
                  << " Eventgroup 0x" << std::setw(4) << entry.eventgroup_id
                  << " TTL=" << std::dec << entry.ttl << "s\n";
    }

    ServiceRegistry registry_;
    std::size_t packet_count_ = 0;
    std::size_t someip_count_ = 0;
    std::size_t sd_count_ = 0;
    std::size_t reboot_count_ = 0;
    std::size_t offer_count_ = 0;
    std::size_t find_count_ = 0;
    std::size_t subscribe_count_ = 0;
};

// =============================================================================
// Main entry point
// =============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <interface|pcap_file>\n"
              << "\n"
              << "Monitor SOME/IP Service Discovery traffic.\n"
              << "\n"
              << "Arguments:\n"
              << "  interface    Network interface for live capture (e.g., eth0)\n"
              << "  pcap_file    PCAP file for offline analysis\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " eth0           # Live capture\n"
              << "  " << prog << " capture.pcap   # Analyze PCAP file\n";
}

int main(int argc, char* argv[]) {
    std::cout << "𓆓 Wadjet-Link SOME/IP Service Discovery Monitor\n\n";

    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string source = argv[1];
    SdPacketProcessor processor;

    // Check if source is a file or interface
    if (std::filesystem::exists(source)) {
        // PCAP file mode
        std::cout << "📁 Reading from PCAP file: " << source << "\n\n";

        auto reader_result = pcap::PcapReader::open(source);
        if (!reader_result) {
            std::cerr << "Error opening PCAP file: " << reader_result.error().message() << "\n";
            return 1;
        }

        auto& reader = reader_result.value();
        while (auto packet = reader.next_packet()) {
            processor.process_packet(packet->view());
        }
    } else {
        // Live capture mode
        std::cout << "🔴 Live capture on interface: " << source << "\n";
        std::cout << "   Filtering: udp port 30490 (SOME/IP-SD)\n";
        std::cout << "   Press Ctrl+C to stop\n\n";

        io::CaptureSessionOptions opts;
        opts.promiscuous = true;

        auto session_result = io::CaptureSession::create(source, opts);
        if (!session_result) {
            std::cerr << "Error creating capture session: "
                      << session_result.error().message() << "\n";
            return 1;
        }

        auto& session = session_result.value();

        // Set BPF filter for SD port
        auto filter_result = session.set_filter("udp port 30490");
        if (!filter_result) {
            std::cerr << "Warning: Could not set filter: "
                      << filter_result.error().message() << "\n";
        }

        // Start capture
        auto start_result = session.start();
        if (!start_result) {
            std::cerr << "Error starting capture: "
                      << start_result.error().message() << "\n";
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
    processor.registry().print_summary();

    return 0;
}

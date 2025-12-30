/// @file ecu_bootup.cpp
/// @brief Example: ECU Bootup Sequence Monitor
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to monitor ECU initialization traffic during
/// system bootup. It tracks the sequence of events including:
/// - SOME/IP Service Discovery announcements
/// - DoIP vehicle identification
/// - Service availability timeline
///
/// Usage:
///   ./ecu_bootup eth0               # Live capture on interface
///   ./ecu_bootup capture.pcap       # Analyze from PCAP file

#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/someip.hpp>
#include <wadjet/protocols/someip_sd.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace std::chrono;
using namespace std::chrono_literals;

// =============================================================================
// Event Timeline
// =============================================================================

enum class EventType {
    SdReboot,              ///< SD message with reboot flag
    ServiceOffer,          ///< Service offer
    ServiceStop,           ///< Stop offer
    ServiceFind,           ///< Find service request
    Subscription,          ///< Eventgroup subscription
    DoipAnnouncement,      ///< Vehicle announcement
    DoipRoutingActive,     ///< Routing activation successful
    DiagnosticMessage,     ///< Diagnostic message
    SomeipMessage,         ///< Regular SOME/IP message
};

std::string_view event_type_string(EventType type) {
    switch (type) {
        case EventType::SdReboot: return "SD_REBOOT";
        case EventType::ServiceOffer: return "SERVICE_OFFER";
        case EventType::ServiceStop: return "SERVICE_STOP";
        case EventType::ServiceFind: return "SERVICE_FIND";
        case EventType::Subscription: return "SUBSCRIPTION";
        case EventType::DoipAnnouncement: return "DOIP_ANNOUNCE";
        case EventType::DoipRoutingActive: return "DOIP_ACTIVE";
        case EventType::DiagnosticMessage: return "DIAGNOSTIC";
        case EventType::SomeipMessage: return "SOMEIP_MSG";
    }
    return "UNKNOWN";
}

std::string_view event_emoji(EventType type) {
    switch (type) {
        case EventType::SdReboot: return "⚡";
        case EventType::ServiceOffer: return "📢";
        case EventType::ServiceStop: return "🛑";
        case EventType::ServiceFind: return "🔍";
        case EventType::Subscription: return "🔔";
        case EventType::DoipAnnouncement: return "🚗";
        case EventType::DoipRoutingActive: return "🔓";
        case EventType::DiagnosticMessage: return "💬";
        case EventType::SomeipMessage: return "📨";
    }
    return "❓";
}

struct BootupEvent {
    steady_clock::time_point timestamp;
    EventType type;
    std::string description;
    std::optional<std::uint16_t> service_id;
    std::optional<std::uint16_t> instance_id;
};

// =============================================================================
// Bootup Monitor
// =============================================================================

class BootupMonitor {
public:
    void process_packet(const PacketView& view, steady_clock::time_point capture_time) {
        if (!start_time_) {
            start_time_ = capture_time;
        }
        packet_count_++;

        // Decode the packet stack
        auto result = decode_packet(view.data());
        if (!result.complete) {
            return;
        }

        // Check for DoIP
        if (result.has_layer<doip::DoIPHeader>()) {
            process_doip(result, capture_time);
        }

        // Check for SOME/IP
        if (result.has_layer<someip::SomeIpHeader>()) {
            process_someip(result, capture_time);
        }
    }

    void print_timeline() const {
        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                   ECU Bootup Timeline                        ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

        if (events_.empty()) {
            std::cout << "No bootup events captured.\n";
            return;
        }

        std::cout << "📊 Summary:\n";
        std::cout << "   Total packets:    " << packet_count_ << "\n";
        std::cout << "   Bootup events:    " << events_.size() << "\n";

        if (start_time_ && !events_.empty()) {
            auto duration = duration_cast<milliseconds>(
                events_.back().timestamp - *start_time_);
            std::cout << "   Timeline span:    " << duration.count() << " ms\n";
        }

        // Count events by type
        std::map<EventType, int> event_counts;
        for (const auto& event : events_) {
            event_counts[event.type]++;
        }

        std::cout << "\n   Event breakdown:\n";
        for (const auto& [type, count] : event_counts) {
            std::cout << "     " << event_emoji(type) << " "
                      << std::setw(18) << std::left << event_type_string(type)
                      << ": " << count << "\n";
        }

        // Print timeline
        std::cout << "\n⏱️  Event Timeline:\n";
        std::cout << "┌──────────────┬────────────────────┬─────────────────────────────────────────┐\n";
        std::cout << "│ Time (ms)    │ Event              │ Details                                 │\n";
        std::cout << "├──────────────┼────────────────────┼─────────────────────────────────────────┤\n";

        for (const auto& event : events_) {
            auto offset = start_time_
                ? duration_cast<milliseconds>(event.timestamp - *start_time_).count()
                : 0;

            std::cout << "│ " << std::setw(12) << std::right << offset << " │ "
                      << event_emoji(event.type) << " "
                      << std::setw(17) << std::left << event_type_string(event.type) << " │ "
                      << std::setw(39) << std::left
                      << (event.description.length() > 39
                              ? event.description.substr(0, 36) + "..."
                              : event.description)
                      << " │\n";
        }

        std::cout << "└──────────────┴────────────────────┴─────────────────────────────────────────┘\n";

        // Print service availability timeline
        print_service_timeline();
    }

private:
    void process_doip(const DecodeStackResult& result, steady_clock::time_point time) {
        const auto* doip_header = result.get_layer<doip::DoIPHeader>();

        switch (doip_header->payload_type) {
            case doip::PayloadType::VehicleAnnouncementOrIdentificationResponse: {
                auto payload = result.payload;
                std::string vin = "???";
                if (payload.size() >= 17) {
                    vin = std::string(reinterpret_cast<const char*>(payload.data()), 17);
                }
                add_event(time, EventType::DoipAnnouncement, "VIN: " + vin);
                break;
            }

            case doip::PayloadType::RoutingActivationResponse: {
                auto payload = result.payload;
                if (payload.size() >= 5 &&
                    static_cast<doip::RoutingActivationResponseCode>(payload[4]) ==
                        doip::RoutingActivationResponseCode::SuccessfullyActivated) {
                    add_event(time, EventType::DoipRoutingActive, "Routing activated");
                }
                break;
            }

            case doip::PayloadType::DiagnosticMessage: {
                auto payload = result.payload;
                if (payload.size() >= 4) {
                    std::uint16_t src = (static_cast<std::uint16_t>(payload[0]) << 8) |
                                        static_cast<std::uint16_t>(payload[1]);
                    std::uint16_t dst = (static_cast<std::uint16_t>(payload[2]) << 8) |
                                        static_cast<std::uint16_t>(payload[3]);
                    std::ostringstream ss;
                    ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << src
                       << " -> 0x" << std::setw(4) << dst;
                    add_event(time, EventType::DiagnosticMessage, ss.str());
                }
                break;
            }

            default:
                break;
        }
    }

    void process_someip(const DecodeStackResult& result, steady_clock::time_point time) {
        const auto* someip_header = result.get_layer<someip::SomeIpHeader>();

        if (someip_header->is_service_discovery()) {
            // Process SD
            if (result.has_layer<someip_sd::SomeIpSdHeader>()) {
                process_sd(result, time);
            }
        } else {
            // Regular SOME/IP message - track first message per service
            auto key = std::make_pair(someip_header->service_id, someip_header->method_id);
            if (first_someip_messages_.find(key) == first_someip_messages_.end()) {
                first_someip_messages_[key] = time;

                std::ostringstream ss;
                ss << "Service 0x" << std::hex << std::setw(4) << std::setfill('0')
                   << someip_header->service_id << " Method 0x"
                   << std::setw(4) << someip_header->method_id;
                add_event(time, EventType::SomeipMessage, ss.str(),
                          someip_header->service_id);
            }
        }
    }

    void process_sd(const DecodeStackResult& result, steady_clock::time_point time) {
        const auto* sd_header = result.get_layer<someip_sd::SomeIpSdHeader>();

        // Check reboot flag
        if (sd_header->is_reboot()) {
            add_event(time, EventType::SdReboot, "Reboot detected");
        }

        // Process entries
        for (const auto& entry : sd_header->entries) {
            if (std::holds_alternative<someip_sd::ServiceEntry>(entry)) {
                const auto& svc = std::get<someip_sd::ServiceEntry>(entry);
                process_service_entry(svc, time);
            } else if (std::holds_alternative<someip_sd::EventgroupEntry>(entry)) {
                const auto& eg = std::get<someip_sd::EventgroupEntry>(entry);
                process_eventgroup_entry(eg, time);
            }
        }
    }

    void process_service_entry(const someip_sd::ServiceEntry& entry,
                                steady_clock::time_point time) {
        std::ostringstream ss;
        ss << "0x" << std::hex << std::setw(4) << std::setfill('0')
           << entry.service_id << ":"
           << std::setw(4) << entry.instance_id
           << " v" << std::dec << static_cast<int>(entry.major_version)
           << "." << entry.minor_version;

        switch (entry.type) {
            case someip_sd::EntryType::OfferService:
                add_event(time, EventType::ServiceOffer, ss.str(),
                          entry.service_id, entry.instance_id);

                // Track first offer time
                {
                    auto key = std::make_pair(entry.service_id, entry.instance_id);
                    if (first_offer_times_.find(key) == first_offer_times_.end()) {
                        first_offer_times_[key] = time;
                    }
                }
                break;

            case someip_sd::EntryType::StopOfferService:
                add_event(time, EventType::ServiceStop, ss.str(),
                          entry.service_id, entry.instance_id);
                break;

            case someip_sd::EntryType::FindService:
                add_event(time, EventType::ServiceFind, ss.str(),
                          entry.service_id, entry.instance_id);
                break;

            default:
                break;
        }
    }

    void process_eventgroup_entry(const someip_sd::EventgroupEntry& entry,
                                   steady_clock::time_point time) {
        std::ostringstream ss;
        ss << "Service 0x" << std::hex << std::setw(4) << std::setfill('0')
           << entry.service_id << " EG 0x" << std::setw(4) << entry.eventgroup_id;

        if (entry.type == someip_sd::EntryType::SubscribeEventgroup) {
            add_event(time, EventType::Subscription, ss.str(),
                      entry.service_id, entry.instance_id);
        }
    }

    void add_event(steady_clock::time_point time, EventType type,
                   std::string description,
                   std::optional<std::uint16_t> service_id = std::nullopt,
                   std::optional<std::uint16_t> instance_id = std::nullopt) {
        events_.push_back({time, type, std::move(description), service_id, instance_id});

        // Print live event
        auto offset = start_time_
            ? duration_cast<milliseconds>(time - *start_time_).count()
            : 0;
        std::cout << "[" << std::setw(8) << offset << " ms] "
                  << event_emoji(type) << " " << events_.back().description << "\n";
    }

    void print_service_timeline() const {
        if (first_offer_times_.empty()) {
            return;
        }

        std::cout << "\n🕐 Service Availability Timeline:\n";
        std::cout << "┌────────────────────────┬──────────────────┐\n";
        std::cout << "│ Service:Instance       │ First Offer (ms) │\n";
        std::cout << "├────────────────────────┼──────────────────┤\n";

        // Sort by time
        std::vector<std::pair<std::pair<uint16_t, uint16_t>, steady_clock::time_point>> sorted(
            first_offer_times_.begin(), first_offer_times_.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        for (const auto& [key, time] : sorted) {
            auto offset = start_time_
                ? duration_cast<milliseconds>(time - *start_time_).count()
                : 0;

            std::cout << "│ 0x" << std::hex << std::setw(4) << std::setfill('0')
                      << key.first << ":0x" << std::setw(4) << key.second
                      << "        │ " << std::dec << std::setw(16) << std::setfill(' ')
                      << offset << " │\n";
        }

        std::cout << "└────────────────────────┴──────────────────┘\n";
    }

    std::optional<steady_clock::time_point> start_time_;
    std::vector<BootupEvent> events_;
    std::map<std::pair<std::uint16_t, std::uint16_t>, steady_clock::time_point> first_offer_times_;
    std::map<std::pair<std::uint16_t, std::uint16_t>, steady_clock::time_point> first_someip_messages_;
    std::size_t packet_count_ = 0;
};

// =============================================================================
// Main entry point
// =============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <interface|pcap_file>\n"
              << "\n"
              << "Monitor ECU bootup sequence and service availability.\n"
              << "\n"
              << "Arguments:\n"
              << "  interface    Network interface for live capture (e.g., eth0)\n"
              << "  pcap_file    PCAP file for offline analysis\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << " eth0           # Live capture\n"
              << "  " << prog << " bootup.pcap    # Analyze PCAP file\n";
}

int main(int argc, char* argv[]) {
    std::cout << "𓆓 Wadjet-Link ECU Bootup Monitor\n\n";

    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string source = argv[1];
    BootupMonitor monitor;

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

        // For PCAP, use packet timestamps relative to first packet
        auto start = steady_clock::now();
        while (auto packet = reader.next_packet()) {
            // Use packet timestamp to simulate time progression
            auto packet_time = start + microseconds(packet->timestamp().total_microseconds());
            monitor.process_packet(packet->view(), packet_time);
        }
    } else {
        // Live capture mode
        std::cout << "🔴 Live capture on interface: " << source << "\n";
        std::cout << "   Press Ctrl+C to stop\n\n";

        io::CaptureSessionOptions opts;
        opts.promiscuous = true;

        auto session_result = io::CaptureSession::create(source, opts);
        if (!session_result) {
            std::cerr << "Error creating capture session: " << session_result.error().message
                      << "\n";
            return 1;
        }

        auto& session = session_result.value();

        // Start capture
        auto start_result = session.start();
        if (!start_result) {
            std::cerr << "Error starting capture: " << start_result.error().message << "\n";
            return 1;
        }

        // Capture loop
        while (session.is_running()) {
            if (auto packet = session.next_packet(1s)) {
                monitor.process_packet(packet->view(), steady_clock::now());
            }
        }
    }

    // Print results
    monitor.print_timeline();

    return 0;
}

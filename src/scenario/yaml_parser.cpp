/// @file yaml_parser.cpp
/// @brief YAML parser implementation for scenario files
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/scenario/parser.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <sstream>

namespace wadjet::scenario {

namespace {

// =============================================================================
// Helper Functions
// =============================================================================

template <typename T>
std::optional<T> get_optional(const YAML::Node& node, const std::string& key) {
    if (node[key] && !node[key].IsNull()) {
        return node[key].as<T>();
    }
    return std::nullopt;
}

Duration parse_duration(const YAML::Node& node, Duration default_val = Duration{0}) {
    if (!node || node.IsNull()) {
        return default_val;
    }
    
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        
        // Try parsing as number with unit suffix
        std::size_t value = 0;
        std::size_t i = 0;
        while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i]))) {
            value = value * 10 + static_cast<std::size_t>(str[i] - '0');
            ++i;
        }
        
        std::string unit = str.substr(i);
        // Trim whitespace
        while (!unit.empty() && std::isspace(static_cast<unsigned char>(unit.front()))) {
            unit.erase(0, 1);
        }
        
        if (unit.empty() || unit == "ms") {
            return Duration{static_cast<long long>(value)};
        } else if (unit == "s") {
            return Duration{static_cast<long long>(value * 1000)};
        } else if (unit == "us" || unit == "µs") {
            return Duration{static_cast<long long>(value / 1000)};
        }
        
        // Fall back to integer milliseconds
        return Duration{static_cast<long long>(value)};
    }
    
    return Duration{node.as<long long>()};
}

std::vector<std::uint8_t> parse_hex_bytes(const std::string& str) {
    std::vector<std::uint8_t> result;
    std::string hex;
    
    for (char c : str) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            hex += c;
            if (hex.size() == 2) {
                result.push_back(static_cast<std::uint8_t>(
                    std::stoul(hex, nullptr, 16)));
                hex.clear();
            }
        }
    }
    
    return result;
}

// =============================================================================
// Parse Expectations
// =============================================================================

EthernetExpect parse_ethernet_expect(const YAML::Node& node) {
    EthernetExpect expect;
    expect.src_mac = get_optional<std::string>(node, "src_mac");
    expect.dst_mac = get_optional<std::string>(node, "dst_mac");
    expect.ethertype = get_optional<std::uint16_t>(node, "ethertype");
    expect.vlan_id = get_optional<std::uint16_t>(node, "vlan_id");
    
    // Alternative names
    if (!expect.src_mac && node["source"]) {
        expect.src_mac = node["source"].as<std::string>();
    }
    if (!expect.dst_mac && node["destination"]) {
        expect.dst_mac = node["destination"].as<std::string>();
    }
    if (!expect.ethertype && node["type"]) {
        expect.ethertype = node["type"].as<std::uint16_t>();
    }
    if (!expect.vlan_id && node["vlan"]) {
        expect.vlan_id = node["vlan"].as<std::uint16_t>();
    }
    
    return expect;
}

IPv4Expect parse_ipv4_expect(const YAML::Node& node) {
    IPv4Expect expect;
    expect.src_ip = get_optional<std::string>(node, "src_ip");
    expect.dst_ip = get_optional<std::string>(node, "dst_ip");
    expect.protocol = get_optional<std::uint8_t>(node, "protocol");
    expect.ttl = get_optional<std::uint8_t>(node, "ttl");
    
    // Alternative names
    if (!expect.src_ip && node["source"]) {
        expect.src_ip = node["source"].as<std::string>();
    }
    if (!expect.dst_ip && node["destination"]) {
        expect.dst_ip = node["destination"].as<std::string>();
    }
    
    return expect;
}

UDPExpect parse_udp_expect(const YAML::Node& node) {
    UDPExpect expect;
    expect.src_port = get_optional<std::uint16_t>(node, "src_port");
    expect.dst_port = get_optional<std::uint16_t>(node, "dst_port");
    
    // Alternative names
    if (!expect.src_port && node["source"]) {
        expect.src_port = node["source"].as<std::uint16_t>();
    }
    if (!expect.dst_port && node["destination"]) {
        expect.dst_port = node["destination"].as<std::uint16_t>();
    }
    if (!expect.dst_port && node["port"]) {
        expect.dst_port = node["port"].as<std::uint16_t>();
    }
    
    return expect;
}

TCPExpect parse_tcp_expect(const YAML::Node& node) {
    TCPExpect expect;
    expect.src_port = get_optional<std::uint16_t>(node, "src_port");
    expect.dst_port = get_optional<std::uint16_t>(node, "dst_port");
    expect.syn = get_optional<bool>(node, "syn");
    expect.ack = get_optional<bool>(node, "ack");
    expect.fin = get_optional<bool>(node, "fin");
    expect.rst = get_optional<bool>(node, "rst");
    
    // Alternative names
    if (!expect.src_port && node["source"]) {
        expect.src_port = node["source"].as<std::uint16_t>();
    }
    if (!expect.dst_port && node["destination"]) {
        expect.dst_port = node["destination"].as<std::uint16_t>();
    }
    if (!expect.dst_port && node["port"]) {
        expect.dst_port = node["port"].as<std::uint16_t>();
    }
    
    return expect;
}

SomeIpMessageTypeExpect parse_someip_message_type(const std::string& str) {
    if (str == "request") return SomeIpMessageTypeExpect::Request;
    if (str == "request_no_return") return SomeIpMessageTypeExpect::RequestNoReturn;
    if (str == "notification") return SomeIpMessageTypeExpect::Notification;
    if (str == "response") return SomeIpMessageTypeExpect::Response;
    if (str == "error") return SomeIpMessageTypeExpect::Error;
    return SomeIpMessageTypeExpect::Any;
}

SomeIpExpect parse_someip_expect(const YAML::Node& node) {
    SomeIpExpect expect;
    expect.service_id = get_optional<std::uint16_t>(node, "service_id");
    expect.method_id = get_optional<std::uint16_t>(node, "method_id");
    expect.client_id = get_optional<std::uint16_t>(node, "client_id");
    expect.session_id = get_optional<std::uint16_t>(node, "session_id");
    expect.return_code = get_optional<std::uint8_t>(node, "return_code");
    
    // Alternative names
    if (!expect.service_id && node["service"]) {
        expect.service_id = node["service"].as<std::uint16_t>();
    }
    if (!expect.method_id && node["method"]) {
        expect.method_id = node["method"].as<std::uint16_t>();
    }
    if (!expect.method_id && node["event"]) {
        expect.method_id = node["event"].as<std::uint16_t>();
    }
    
    if (node["message_type"]) {
        expect.message_type = parse_someip_message_type(
            node["message_type"].as<std::string>());
    } else if (node["type"]) {
        expect.message_type = parse_someip_message_type(
            node["type"].as<std::string>());
    }
    
    return expect;
}

SdEntryTypeExpect parse_sd_entry_type(const std::string& str) {
    if (str == "find" || str == "find_service") return SdEntryTypeExpect::FindService;
    if (str == "offer" || str == "offer_service") return SdEntryTypeExpect::OfferService;
    if (str == "subscribe") return SdEntryTypeExpect::Subscribe;
    if (str == "subscribe_ack") return SdEntryTypeExpect::SubscribeAck;
    return SdEntryTypeExpect::Any;
}

SomeIpSdExpect parse_someip_sd_expect(const YAML::Node& node) {
    SomeIpSdExpect expect;
    expect.service_id = get_optional<std::uint16_t>(node, "service_id");
    expect.instance_id = get_optional<std::uint16_t>(node, "instance_id");
    expect.major_version = get_optional<std::uint8_t>(node, "major_version");
    
    // Alternative names
    if (!expect.service_id && node["service"]) {
        expect.service_id = node["service"].as<std::uint16_t>();
    }
    if (!expect.instance_id && node["instance"]) {
        expect.instance_id = node["instance"].as<std::uint16_t>();
    }
    
    if (node["entry_type"]) {
        expect.entry_type = parse_sd_entry_type(node["entry_type"].as<std::string>());
    } else if (node["type"]) {
        expect.entry_type = parse_sd_entry_type(node["type"].as<std::string>());
    }
    
    return expect;
}

DoIpPayloadTypeExpect parse_doip_payload_type(const std::string& str) {
    if (str == "vehicle_identification_request") return DoIpPayloadTypeExpect::VehicleIdentificationRequest;
    if (str == "vehicle_identification_response") return DoIpPayloadTypeExpect::VehicleIdentificationResponse;
    if (str == "routing_activation_request") return DoIpPayloadTypeExpect::RoutingActivationRequest;
    if (str == "routing_activation_response") return DoIpPayloadTypeExpect::RoutingActivationResponse;
    if (str == "diagnostic_message" || str == "diagnostic") return DoIpPayloadTypeExpect::DiagnosticMessage;
    if (str == "diagnostic_positive_ack" || str == "positive_ack") return DoIpPayloadTypeExpect::DiagnosticPositiveAck;
    if (str == "diagnostic_negative_ack" || str == "negative_ack") return DoIpPayloadTypeExpect::DiagnosticNegativeAck;
    return DoIpPayloadTypeExpect::Any;
}

DoIpExpect parse_doip_expect(const YAML::Node& node) {
    DoIpExpect expect;
    expect.source_address = get_optional<std::uint16_t>(node, "source_address");
    expect.target_address = get_optional<std::uint16_t>(node, "target_address");
    
    // Alternative names
    if (!expect.source_address && node["source"]) {
        expect.source_address = node["source"].as<std::uint16_t>();
    }
    if (!expect.target_address && node["target"]) {
        expect.target_address = node["target"].as<std::uint16_t>();
    }
    
    if (node["payload_type"]) {
        expect.payload_type = parse_doip_payload_type(node["payload_type"].as<std::string>());
    } else if (node["type"]) {
        expect.payload_type = parse_doip_payload_type(node["type"].as<std::string>());
    } else if (node["message_type"]) {
        expect.payload_type = parse_doip_payload_type(node["message_type"].as<std::string>());
    }
    
    return expect;
}

PayloadExpect parse_payload_expect(const YAML::Node& node) {
    PayloadExpect expect;
    
    if (node["contains"]) {
        if (node["contains"].IsSequence()) {
            std::vector<std::uint8_t> bytes;
            for (const auto& b : node["contains"]) {
                bytes.push_back(b.as<std::uint8_t>());
            }
            expect.contains = std::move(bytes);
        } else {
            expect.contains = parse_hex_bytes(node["contains"].as<std::string>());
        }
    }
    
    if (node["equals"]) {
        if (node["equals"].IsSequence()) {
            std::vector<std::uint8_t> bytes;
            for (const auto& b : node["equals"]) {
                bytes.push_back(b.as<std::uint8_t>());
            }
            expect.equals = std::move(bytes);
        } else {
            expect.equals = parse_hex_bytes(node["equals"].as<std::string>());
        }
    }
    
    expect.min_size = get_optional<std::size_t>(node, "min_size");
    expect.max_size = get_optional<std::size_t>(node, "max_size");
    
    return expect;
}

// =============================================================================
// Parse Steps
// =============================================================================

CaptureConfig parse_capture_config(const YAML::Node& node) {
    CaptureConfig config;
    config.interface = node["interface"].as<std::string>("eth0");
    config.filter = get_optional<std::string>(node, "filter").value_or("");
    config.pcap_file = get_optional<std::string>(node, "pcap_file");
    config.timeout = parse_duration(node["timeout"], Duration{5000});
    config.promiscuous = node["promiscuous"].as<bool>(true);
    
    // Alternative names
    if (config.pcap_file->empty() && node["pcap"]) {
        config.pcap_file = node["pcap"].as<std::string>();
    }
    if (config.pcap_file->empty() && node["file"]) {
        config.pcap_file = node["file"].as<std::string>();
    }
    
    return config;
}

Step parse_capture_step(const YAML::Node& node) {
    CaptureStep step;
    step.config = parse_capture_config(node);
    return step;
}

Step parse_send_step(const YAML::Node& node) {
    SendStep step;
    step.interface = node["interface"].as<std::string>("eth0");
    step.pcap_file = get_optional<std::string>(node, "pcap_file");
    step.delay = parse_duration(node["delay"], Duration{0});
    
    if (node["raw"] || node["data"]) {
        const auto& data_node = node["raw"] ? node["raw"] : node["data"];
        if (data_node.IsSequence()) {
            std::vector<std::uint8_t> bytes;
            for (const auto& b : data_node) {
                bytes.push_back(b.as<std::uint8_t>());
            }
            step.raw_data = std::move(bytes);
        } else {
            step.raw_data = parse_hex_bytes(data_node.as<std::string>());
        }
    }
    
    return step;
}

Step parse_wait_step(const YAML::Node& node) {
    WaitStep step;
    if (node.IsScalar()) {
        step.duration = parse_duration(node);
    } else {
        step.duration = parse_duration(node["duration"], Duration{1000});
    }
    return step;
}

Step parse_expect_step(const YAML::Node& node) {
    ExpectStep step;
    
    // Parse protocol expectations
    if (node["ethernet"]) {
        step.ethernet = parse_ethernet_expect(node["ethernet"]);
    }
    if (node["ipv4"] || node["ip"]) {
        step.ipv4 = parse_ipv4_expect(node["ipv4"] ? node["ipv4"] : node["ip"]);
    }
    if (node["udp"]) {
        step.udp = parse_udp_expect(node["udp"]);
    }
    if (node["tcp"]) {
        step.tcp = parse_tcp_expect(node["tcp"]);
    }
    if (node["someip"]) {
        step.someip = parse_someip_expect(node["someip"]);
    }
    if (node["someip_sd"] || node["sd"]) {
        step.someip_sd = parse_someip_sd_expect(
            node["someip_sd"] ? node["someip_sd"] : node["sd"]);
    }
    if (node["doip"]) {
        step.doip = parse_doip_expect(node["doip"]);
    }
    if (node["payload"]) {
        step.payload = parse_payload_expect(node["payload"]);
    }
    
    // Parse timing and count
    step.within = parse_duration(node["within"], Duration{1000});
    if (!node["within"] && node["within_ms"]) {
        step.within = Duration{node["within_ms"].as<long long>()};
    }
    if (!node["within"] && node["timeout"]) {
        step.within = parse_duration(node["timeout"], Duration{1000});
    }
    
    if (node["count"]) {
        if (node["count"].IsScalar()) {
            auto expr = CountExpression::parse(node["count"].as<std::string>());
            if (expr) {
                step.count = *expr;
            }
        }
    }
    
    // Parse metadata
    step.description = get_optional<std::string>(node, "description").value_or("");
    step.required = node["required"].as<bool>(true);
    
    return step;
}

Step parse_log_step(const YAML::Node& node) {
    LogStep step;
    if (node.IsScalar()) {
        step.message = node.as<std::string>();
    } else {
        step.message = node["message"].as<std::string>("");
        step.level = node["level"].as<std::string>("info");
    }
    return step;
}

Step parse_step(const YAML::Node& node) {
    if (node["capture"]) {
        return parse_capture_step(node["capture"]);
    }
    if (node["send"]) {
        return parse_send_step(node["send"]);
    }
    if (node["wait"]) {
        return parse_wait_step(node["wait"]);
    }
    if (node["expect"]) {
        return parse_expect_step(node["expect"]);
    }
    if (node["log"]) {
        return parse_log_step(node["log"]);
    }
    
    // Default: try to parse as expect step (for simpler syntax)
    return parse_expect_step(node);
}

// =============================================================================
// Parse Scenario
// =============================================================================

Scenario parse_scenario(const YAML::Node& root) {
    Scenario scenario;
    
    scenario.name = root["name"].as<std::string>("Unnamed Scenario");
    scenario.description = get_optional<std::string>(root, "description").value_or("");
    scenario.version = root["version"].as<std::string>("1.0");
    scenario.timeout = parse_duration(root["timeout"], Duration{30000});
    
    // Parse tags
    if (root["tags"]) {
        for (const auto& tag : root["tags"]) {
            scenario.tags.push_back(tag.as<std::string>());
        }
    }
    
    // Parse steps
    if (root["steps"]) {
        for (const auto& step_node : root["steps"]) {
            scenario.steps.push_back(parse_step(step_node));
        }
    }
    
    return scenario;
}

}  // anonymous namespace

// =============================================================================
// YAML Parser Implementation
// =============================================================================

class YamlParser : public IScenarioParser {
public:
    [[nodiscard]] ParseResult parse(std::string_view content) const override {
        try {
            YAML::Node root = YAML::Load(std::string(content));
            return ParseResult::ok(parse_scenario(root));
        } catch (const YAML::ParserException& e) {
            return ParseResult::err(ParseError{
                e.what(),
                static_cast<std::size_t>(e.mark.line + 1),
                static_cast<std::size_t>(e.mark.column + 1)
            });
        } catch (const YAML::Exception& e) {
            return ParseResult::err(ParseError{e.what(), 0, 0});
        } catch (const std::exception& e) {
            return ParseResult::err(ParseError{e.what(), 0, 0});
        }
    }
    
    [[nodiscard]] ParseResult parse_file(const std::filesystem::path& path) const override {
        std::ifstream file(path);
        if (!file) {
            return ParseResult::err(ParseError{
                "Failed to open file: " + path.string(), 0, 0});
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }
    
    [[nodiscard]] std::vector<std::string> supported_extensions() const override {
        return {".yaml", ".yml"};
    }
};

ParseResult parse_yaml(std::string_view content) {
    YamlParser parser;
    return parser.parse(content);
}

}  // namespace wadjet::scenario

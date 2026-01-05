/// @file json_parser.cpp
/// @brief JSON parser implementation for scenario files
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/scenario/parser.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace wadjet::scenario {

using json = nlohmann::json;

namespace {

// =============================================================================
// Helper Functions
// =============================================================================

template <typename T>
std::optional<T> get_optional(const json& j, const std::string& key) {
    if (j.contains(key) && !j[key].is_null()) {
        return j[key].get<T>();
    }
    return std::nullopt;
}

Duration parse_duration(const json& j, Duration default_val = Duration{0}) {
    if (j.is_null()) {
        return default_val;
    }

    if (j.is_string()) {
        std::string str = j.get<std::string>();

        // Try parsing as number with unit suffix
        std::size_t value = 0;
        std::size_t i = 0;
        while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i]))) {
            value = value * 10 + static_cast<std::size_t>(str[i] - '0');
            ++i;
        }

        std::string unit = str.substr(i);
        while (!unit.empty() && std::isspace(static_cast<unsigned char>(unit.front()))) {
            unit.erase(0, 1);
        }

        if (unit.empty() || unit == "ms") {
            return Duration{static_cast<long long>(value)};
        } else if (unit == "s") {
            return Duration{static_cast<long long>(value * 1000)};
        } else if (unit == "us") {
            return Duration{static_cast<long long>(value / 1000)};
        }

        return Duration{static_cast<long long>(value)};
    }

    return Duration{j.get<long long>()};
}

std::vector<std::uint8_t> parse_hex_bytes(const std::string& str) {
    std::vector<std::uint8_t> result;
    std::string hex;

    for (char c : str) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            hex += c;
            if (hex.size() == 2) {
                result.push_back(static_cast<std::uint8_t>(std::stoul(hex, nullptr, 16)));
                hex.clear();
            }
        }
    }

    return result;
}

// =============================================================================
// Parse Expectations
// =============================================================================

EthernetExpect parse_ethernet_expect(const json& j) {
    EthernetExpect expect;
    expect.src_mac = get_optional<std::string>(j, "src_mac");
    expect.dst_mac = get_optional<std::string>(j, "dst_mac");
    expect.ethertype = get_optional<std::uint16_t>(j, "ethertype");
    expect.vlan_id = get_optional<std::uint16_t>(j, "vlan_id");

    if (!expect.src_mac && j.contains("source")) {
        expect.src_mac = j["source"].get<std::string>();
    }
    if (!expect.dst_mac && j.contains("destination")) {
        expect.dst_mac = j["destination"].get<std::string>();
    }
    if (!expect.ethertype && j.contains("type")) {
        expect.ethertype = j["type"].get<std::uint16_t>();
    }
    if (!expect.vlan_id && j.contains("vlan")) {
        expect.vlan_id = j["vlan"].get<std::uint16_t>();
    }

    return expect;
}

IPv4Expect parse_ipv4_expect(const json& j) {
    IPv4Expect expect;
    expect.src_ip = get_optional<std::string>(j, "src_ip");
    expect.dst_ip = get_optional<std::string>(j, "dst_ip");
    expect.protocol = get_optional<std::uint8_t>(j, "protocol");
    expect.ttl = get_optional<std::uint8_t>(j, "ttl");

    if (!expect.src_ip && j.contains("source")) {
        expect.src_ip = j["source"].get<std::string>();
    }
    if (!expect.dst_ip && j.contains("destination")) {
        expect.dst_ip = j["destination"].get<std::string>();
    }

    return expect;
}

UDPExpect parse_udp_expect(const json& j) {
    UDPExpect expect;
    expect.src_port = get_optional<std::uint16_t>(j, "src_port");
    expect.dst_port = get_optional<std::uint16_t>(j, "dst_port");

    if (!expect.src_port && j.contains("source")) {
        expect.src_port = j["source"].get<std::uint16_t>();
    }
    if (!expect.dst_port && j.contains("destination")) {
        expect.dst_port = j["destination"].get<std::uint16_t>();
    }
    if (!expect.dst_port && j.contains("port")) {
        expect.dst_port = j["port"].get<std::uint16_t>();
    }

    return expect;
}

TCPExpect parse_tcp_expect(const json& j) {
    TCPExpect expect;
    expect.src_port = get_optional<std::uint16_t>(j, "src_port");
    expect.dst_port = get_optional<std::uint16_t>(j, "dst_port");
    expect.syn = get_optional<bool>(j, "syn");
    expect.ack = get_optional<bool>(j, "ack");
    expect.fin = get_optional<bool>(j, "fin");
    expect.rst = get_optional<bool>(j, "rst");

    if (!expect.src_port && j.contains("source")) {
        expect.src_port = j["source"].get<std::uint16_t>();
    }
    if (!expect.dst_port && j.contains("destination")) {
        expect.dst_port = j["destination"].get<std::uint16_t>();
    }
    if (!expect.dst_port && j.contains("port")) {
        expect.dst_port = j["port"].get<std::uint16_t>();
    }

    return expect;
}

SomeIpMessageTypeExpect parse_someip_message_type(const std::string& str) {
    if (str == "request")
        return SomeIpMessageTypeExpect::Request;
    if (str == "request_no_return")
        return SomeIpMessageTypeExpect::RequestNoReturn;
    if (str == "notification")
        return SomeIpMessageTypeExpect::Notification;
    if (str == "response")
        return SomeIpMessageTypeExpect::Response;
    if (str == "error")
        return SomeIpMessageTypeExpect::Error;
    return SomeIpMessageTypeExpect::Any;
}

SomeIpExpect parse_someip_expect(const json& j) {
    SomeIpExpect expect;
    expect.service_id = get_optional<std::uint16_t>(j, "service_id");
    expect.method_id = get_optional<std::uint16_t>(j, "method_id");
    expect.client_id = get_optional<std::uint16_t>(j, "client_id");
    expect.session_id = get_optional<std::uint16_t>(j, "session_id");
    expect.return_code = get_optional<std::uint8_t>(j, "return_code");

    if (!expect.service_id && j.contains("service")) {
        expect.service_id = j["service"].get<std::uint16_t>();
    }
    if (!expect.method_id && j.contains("method")) {
        expect.method_id = j["method"].get<std::uint16_t>();
    }
    if (!expect.method_id && j.contains("event")) {
        expect.method_id = j["event"].get<std::uint16_t>();
    }

    if (j.contains("message_type")) {
        expect.message_type = parse_someip_message_type(j["message_type"].get<std::string>());
    } else if (j.contains("type")) {
        expect.message_type = parse_someip_message_type(j["type"].get<std::string>());
    }

    return expect;
}

SdEntryTypeExpect parse_sd_entry_type(const std::string& str) {
    if (str == "find" || str == "find_service")
        return SdEntryTypeExpect::FindService;
    if (str == "offer" || str == "offer_service")
        return SdEntryTypeExpect::OfferService;
    if (str == "subscribe")
        return SdEntryTypeExpect::Subscribe;
    if (str == "subscribe_ack")
        return SdEntryTypeExpect::SubscribeAck;
    return SdEntryTypeExpect::Any;
}

SomeIpSdExpect parse_someip_sd_expect(const json& j) {
    SomeIpSdExpect expect;
    expect.service_id = get_optional<std::uint16_t>(j, "service_id");
    expect.instance_id = get_optional<std::uint16_t>(j, "instance_id");
    expect.major_version = get_optional<std::uint8_t>(j, "major_version");

    if (!expect.service_id && j.contains("service")) {
        expect.service_id = j["service"].get<std::uint16_t>();
    }
    if (!expect.instance_id && j.contains("instance")) {
        expect.instance_id = j["instance"].get<std::uint16_t>();
    }

    if (j.contains("entry_type")) {
        expect.entry_type = parse_sd_entry_type(j["entry_type"].get<std::string>());
    } else if (j.contains("type")) {
        expect.entry_type = parse_sd_entry_type(j["type"].get<std::string>());
    }

    return expect;
}

DoIpPayloadTypeExpect parse_doip_payload_type(const std::string& str) {
    if (str == "vehicle_identification_request")
        return DoIpPayloadTypeExpect::VehicleIdentificationRequest;
    if (str == "vehicle_identification_response")
        return DoIpPayloadTypeExpect::VehicleIdentificationResponse;
    if (str == "routing_activation_request")
        return DoIpPayloadTypeExpect::RoutingActivationRequest;
    if (str == "routing_activation_response")
        return DoIpPayloadTypeExpect::RoutingActivationResponse;
    if (str == "diagnostic_message" || str == "diagnostic")
        return DoIpPayloadTypeExpect::DiagnosticMessage;
    if (str == "diagnostic_positive_ack" || str == "positive_ack")
        return DoIpPayloadTypeExpect::DiagnosticPositiveAck;
    if (str == "diagnostic_negative_ack" || str == "negative_ack")
        return DoIpPayloadTypeExpect::DiagnosticNegativeAck;
    return DoIpPayloadTypeExpect::Any;
}

DoIpExpect parse_doip_expect(const json& j) {
    DoIpExpect expect;
    expect.source_address = get_optional<std::uint16_t>(j, "source_address");
    expect.target_address = get_optional<std::uint16_t>(j, "target_address");

    if (!expect.source_address && j.contains("source")) {
        expect.source_address = j["source"].get<std::uint16_t>();
    }
    if (!expect.target_address && j.contains("target")) {
        expect.target_address = j["target"].get<std::uint16_t>();
    }

    if (j.contains("payload_type")) {
        expect.payload_type = parse_doip_payload_type(j["payload_type"].get<std::string>());
    } else if (j.contains("type")) {
        expect.payload_type = parse_doip_payload_type(j["type"].get<std::string>());
    } else if (j.contains("message_type")) {
        expect.payload_type = parse_doip_payload_type(j["message_type"].get<std::string>());
    }

    return expect;
}

PayloadExpect parse_payload_expect(const json& j) {
    PayloadExpect expect;

    if (j.contains("contains")) {
        if (j["contains"].is_array()) {
            std::vector<std::uint8_t> bytes;
            for (const auto& b : j["contains"]) {
                bytes.push_back(b.get<std::uint8_t>());
            }
            expect.contains = std::move(bytes);
        } else {
            expect.contains = parse_hex_bytes(j["contains"].get<std::string>());
        }
    }

    if (j.contains("equals")) {
        if (j["equals"].is_array()) {
            std::vector<std::uint8_t> bytes;
            for (const auto& b : j["equals"]) {
                bytes.push_back(b.get<std::uint8_t>());
            }
            expect.equals = std::move(bytes);
        } else {
            expect.equals = parse_hex_bytes(j["equals"].get<std::string>());
        }
    }

    expect.min_size = get_optional<std::size_t>(j, "min_size");
    expect.max_size = get_optional<std::size_t>(j, "max_size");

    return expect;
}

// =============================================================================
// Parse Steps
// =============================================================================

CaptureConfig parse_capture_config(const json& j) {
    CaptureConfig config;
    config.interface = j.value("interface", "eth0");
    config.filter = j.value("filter", "");
    config.pcap_file = get_optional<std::string>(j, "pcap_file");
    config.timeout = j.contains("timeout") ? parse_duration(j["timeout"]) : Duration{5000};
    config.promiscuous = j.value("promiscuous", true);

    if (!config.pcap_file && j.contains("pcap")) {
        config.pcap_file = j["pcap"].get<std::string>();
    }
    if (!config.pcap_file && j.contains("file")) {
        config.pcap_file = j["file"].get<std::string>();
    }

    return config;
}

Step parse_capture_step(const json& j) {
    CaptureStep step;
    step.config = parse_capture_config(j);
    return step;
}

Step parse_send_step(const json& j) {
    SendStep step;
    step.interface = j.value("interface", "eth0");
    step.pcap_file = get_optional<std::string>(j, "pcap_file");
    step.delay = j.contains("delay") ? parse_duration(j["delay"]) : Duration{0};

    if (j.contains("raw") || j.contains("data")) {
        const auto& data_node = j.contains("raw") ? j["raw"] : j["data"];
        if (data_node.is_array()) {
            std::vector<std::uint8_t> bytes;
            for (const auto& b : data_node) {
                bytes.push_back(b.get<std::uint8_t>());
            }
            step.raw_data = std::move(bytes);
        } else {
            step.raw_data = parse_hex_bytes(data_node.get<std::string>());
        }
    }

    return step;
}

Step parse_wait_step(const json& j) {
    WaitStep step;
    if (j.is_number()) {
        step.duration = Duration{j.get<long long>()};
    } else if (j.is_string()) {
        step.duration = parse_duration(j);
    } else {
        step.duration = j.contains("duration") ? parse_duration(j["duration"]) : Duration{1000};
    }
    return step;
}

Step parse_expect_step(const json& j) {
    ExpectStep step;

    if (j.contains("ethernet")) {
        step.ethernet = parse_ethernet_expect(j["ethernet"]);
    }
    if (j.contains("ipv4") || j.contains("ip")) {
        step.ipv4 = parse_ipv4_expect(j.contains("ipv4") ? j["ipv4"] : j["ip"]);
    }
    if (j.contains("udp")) {
        step.udp = parse_udp_expect(j["udp"]);
    }
    if (j.contains("tcp")) {
        step.tcp = parse_tcp_expect(j["tcp"]);
    }
    if (j.contains("someip")) {
        step.someip = parse_someip_expect(j["someip"]);
    }
    if (j.contains("someip_sd") || j.contains("sd")) {
        step.someip_sd = parse_someip_sd_expect(j.contains("someip_sd") ? j["someip_sd"] : j["sd"]);
    }
    if (j.contains("doip")) {
        step.doip = parse_doip_expect(j["doip"]);
    }
    if (j.contains("payload")) {
        step.payload = parse_payload_expect(j["payload"]);
    }

    // Parse timing and count
    if (j.contains("within")) {
        step.within = parse_duration(j["within"]);
    } else if (j.contains("within_ms")) {
        step.within = Duration{j["within_ms"].get<long long>()};
    } else if (j.contains("timeout")) {
        step.within = parse_duration(j["timeout"]);
    } else {
        step.within = Duration{1000};
    }

    if (j.contains("count")) {
        if (j["count"].is_string()) {
            auto expr = CountExpression::parse(j["count"].get<std::string>());
            if (expr) {
                step.count = *expr;
            }
        } else if (j["count"].is_number()) {
            step.count = CountExpression{CompareOp::GreaterEqual, j["count"].get<std::size_t>()};
        }
    }

    step.description = j.value("description", "");
    step.required = j.value("required", true);

    return step;
}

Step parse_log_step(const json& j) {
    LogStep step;
    if (j.is_string()) {
        step.message = j.get<std::string>();
    } else {
        step.message = j.value("message", "");
        step.level = j.value("level", "info");
    }
    return step;
}

Step parse_step(const json& j) {
    if (j.contains("capture")) {
        return parse_capture_step(j["capture"]);
    }
    if (j.contains("send")) {
        return parse_send_step(j["send"]);
    }
    if (j.contains("wait")) {
        return parse_wait_step(j["wait"]);
    }
    if (j.contains("expect")) {
        return parse_expect_step(j["expect"]);
    }
    if (j.contains("log")) {
        return parse_log_step(j["log"]);
    }

    // Default: try to parse as expect step
    return parse_expect_step(j);
}

// =============================================================================
// Parse Scenario
// =============================================================================

Scenario parse_scenario(const json& root) {
    Scenario scenario;

    scenario.name = root.value("name", "Unnamed Scenario");
    scenario.description = root.value("description", "");
    scenario.version = root.value("version", "1.0");
    scenario.timeout = root.contains("timeout") ? parse_duration(root["timeout"]) : Duration{30000};

    if (root.contains("tags") && root["tags"].is_array()) {
        for (const auto& tag : root["tags"]) {
            scenario.tags.push_back(tag.get<std::string>());
        }
    }

    if (root.contains("steps") && root["steps"].is_array()) {
        for (const auto& step_node : root["steps"]) {
            scenario.steps.push_back(parse_step(step_node));
        }
    }

    return scenario;
}

}  // anonymous namespace

// =============================================================================
// JSON Parser Implementation
// =============================================================================

class JsonParser : public IScenarioParser {
public:
    [[nodiscard]] ParseResult parse(std::string_view content) const override {
        try {
            json root = json::parse(content);
            return ParseResult::ok(parse_scenario(root));
        } catch (const json::parse_error& e) {
            return ParseResult::err(ParseError{e.what(), static_cast<std::size_t>(e.byte), 0});
        } catch (const json::exception& e) {
            return ParseResult::err(ParseError{e.what(), 0, 0});
        } catch (const std::exception& e) {
            return ParseResult::err(ParseError{e.what(), 0, 0});
        }
    }

    [[nodiscard]] ParseResult parse_file(const std::filesystem::path& path) const override {
        std::ifstream file(path);
        if (!file) {
            return ParseResult::err(ParseError{"Failed to open file: " + path.string(), 0, 0});
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }

    [[nodiscard]] std::vector<std::string> supported_extensions() const override {
        return {".json"};
    }
};

ParseResult parse_json(std::string_view content) {
    JsonParser parser;
    return parser.parse(content);
}

// =============================================================================
// Unified Parser
// =============================================================================

ParseResult parse_scenario_file(const std::filesystem::path& path) {
    std::string ext = path.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (ext == ".yaml" || ext == ".yml") {
        return parse_yaml([&]() {
            std::ifstream file(path);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }());
    } else if (ext == ".json") {
        JsonParser parser;
        return parser.parse_file(path);
    } else {
        return ParseResult::err(ParseError{
            "Unsupported file extension: " + ext + " (supported: .yaml, .yml, .json)", 0, 0});
    }
}

}  // namespace wadjet::scenario

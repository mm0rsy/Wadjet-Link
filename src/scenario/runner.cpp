/// @file runner.cpp
/// @brief Scenario runner implementation
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/scenario/runner.hpp"

#include "wadjet/io/capture_session.hpp"
#include "wadjet/pcap/pcap_reader.hpp"
#include "wadjet/pcap/pcap_writer.hpp"
#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/scenario/parser.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>

namespace wadjet::scenario {

// =============================================================================
// Packet Matcher Implementation
// =============================================================================

namespace {

/// @brief Matcher that combines all protocol expectations
class ExpectStepMatcher : public IPacketMatcher {
public:
    explicit ExpectStepMatcher(const ExpectStep& expect) : expect_(expect) {}

    [[nodiscard]] bool matches(const Packet& packet) const override {
        auto result = protocols::decode_packet(packet.data());

        // Check Ethernet expectations
        if (expect_.ethernet) {
            auto* eth = result.get_layer<protocols::ethernet::EthernetHeader>();
            if (!eth)
                return false;

            if (expect_.ethernet->ethertype && eth->ethertype != *expect_.ethernet->ethertype) {
                return false;
            }
            if (expect_.ethernet->vlan_id) {
                // Check both outer and inner VLAN tags
                bool found = false;
                if (eth->vlan && eth->vlan->vid() == *expect_.ethernet->vlan_id) {
                    found = true;
                }
                if (eth->vlan_inner && eth->vlan_inner->vid() == *expect_.ethernet->vlan_id) {
                    found = true;
                }
                if (!found)
                    return false;
            }
            // MAC address matching would go here
        }

        // Check IPv4 expectations
        if (expect_.ipv4) {
            auto* ip = result.get_layer<protocols::ipv4::IPv4Header>();
            if (!ip)
                return false;

            if (expect_.ipv4->protocol && ip->protocol != *expect_.ipv4->protocol) {
                return false;
            }
            if (expect_.ipv4->ttl && ip->ttl != *expect_.ipv4->ttl) {
                return false;
            }
            // IP address matching would go here
        }

        // Check UDP expectations
        if (expect_.udp) {
            auto* udp = result.get_layer<protocols::udp::UdpHeader>();
            if (!udp)
                return false;

            if (expect_.udp->src_port && udp->src_port != *expect_.udp->src_port) {
                return false;
            }
            if (expect_.udp->dst_port && udp->dst_port != *expect_.udp->dst_port) {
                return false;
            }
        }

        // Check TCP expectations
        if (expect_.tcp) {
            auto* tcp = result.get_layer<protocols::tcp::TcpHeader>();
            if (!tcp)
                return false;

            if (expect_.tcp->src_port && tcp->src_port != *expect_.tcp->src_port) {
                return false;
            }
            if (expect_.tcp->dst_port && tcp->dst_port != *expect_.tcp->dst_port) {
                return false;
            }
            if (expect_.tcp->syn && tcp->is_syn() != *expect_.tcp->syn) {
                return false;
            }
            if (expect_.tcp->ack && tcp->flags.ack != *expect_.tcp->ack) {
                return false;
            }
            if (expect_.tcp->fin && tcp->is_fin() != *expect_.tcp->fin) {
                return false;
            }
            if (expect_.tcp->rst && tcp->is_rst() != *expect_.tcp->rst) {
                return false;
            }
        }

        // Check SOME/IP expectations
        if (expect_.someip) {
            auto* someip = result.get_layer<protocols::someip::SomeIpHeader>();
            if (!someip)
                return false;

            if (expect_.someip->service_id && someip->service_id != *expect_.someip->service_id) {
                return false;
            }
            if (expect_.someip->method_id && someip->method_id != *expect_.someip->method_id) {
                return false;
            }
            if (expect_.someip->client_id && someip->client_id != *expect_.someip->client_id) {
                return false;
            }
            if (expect_.someip->session_id && someip->session_id != *expect_.someip->session_id) {
                return false;
            }
            if (expect_.someip->return_code &&
                static_cast<std::uint8_t>(someip->return_code) != *expect_.someip->return_code) {
                return false;
            }
            if (expect_.someip->message_type) {
                bool type_match = false;
                switch (*expect_.someip->message_type) {
                    case SomeIpMessageTypeExpect::Request:
                        type_match = someip->is_request();
                        break;
                    case SomeIpMessageTypeExpect::Response:
                        type_match = someip->is_response();
                        break;
                    case SomeIpMessageTypeExpect::Notification:
                        type_match = someip->is_notification();
                        break;
                    case SomeIpMessageTypeExpect::Error:
                        type_match = someip->is_error();
                        break;
                    case SomeIpMessageTypeExpect::RequestNoReturn:
                        type_match = (someip->message_type ==
                                      protocols::someip::MessageType::RequestNoReturn);
                        break;
                    case SomeIpMessageTypeExpect::Any:
                        type_match = true;
                        break;
                }
                if (!type_match)
                    return false;
            }
        }

        // Check SOME/IP-SD expectations
        if (expect_.someip_sd) {
            auto* sd = result.get_layer<protocols::someip_sd::SomeIpSdHeader>();
            if (!sd)
                return false;

            if (expect_.someip_sd->service_id || expect_.someip_sd->instance_id) {
                bool found = false;
                for (const auto& entry : sd->entries) {
                    if (auto* svc = std::get_if<protocols::someip_sd::ServiceEntry>(&entry)) {
                        bool matches_service = !expect_.someip_sd->service_id ||
                                               svc->service_id == *expect_.someip_sd->service_id;
                        bool matches_instance = !expect_.someip_sd->instance_id ||
                                                svc->instance_id == *expect_.someip_sd->instance_id;
                        if (matches_service && matches_instance) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found)
                    return false;
            }
        }

        // Check DoIP expectations
        if (expect_.doip) {
            auto* doip = result.get_layer<protocols::doip::DoIPHeader>();
            if (!doip)
                return false;

            if (expect_.doip->payload_type) {
                bool type_match = false;
                switch (*expect_.doip->payload_type) {
                    case DoIpPayloadTypeExpect::VehicleIdentificationRequest:
                        type_match = (doip->payload_type ==
                                      protocols::doip::PayloadType::VehicleIdentificationRequest);
                        break;
                    case DoIpPayloadTypeExpect::VehicleIdentificationResponse:
                        type_match =
                            (doip->payload_type == protocols::doip::PayloadType::
                                                       VehicleAnnouncementOrIdentificationResponse);
                        break;
                    case DoIpPayloadTypeExpect::RoutingActivationRequest:
                        type_match = (doip->payload_type ==
                                      protocols::doip::PayloadType::RoutingActivationRequest);
                        break;
                    case DoIpPayloadTypeExpect::RoutingActivationResponse:
                        type_match = (doip->payload_type ==
                                      protocols::doip::PayloadType::RoutingActivationResponse);
                        break;
                    case DoIpPayloadTypeExpect::DiagnosticMessage:
                        type_match = doip->is_diagnostic_message();
                        break;
                    case DoIpPayloadTypeExpect::DiagnosticPositiveAck:
                        type_match = (doip->payload_type ==
                                      protocols::doip::PayloadType::DiagnosticMessagePositiveAck);
                        break;
                    case DoIpPayloadTypeExpect::DiagnosticNegativeAck:
                        type_match = (doip->payload_type ==
                                      protocols::doip::PayloadType::DiagnosticMessageNegativeAck);
                        break;
                    case DoIpPayloadTypeExpect::Any:
                        type_match = true;
                        break;
                }
                if (!type_match)
                    return false;
            }
        }

        // Check payload expectations
        if (expect_.payload) {
            if (expect_.payload->min_size && result.payload.size() < *expect_.payload->min_size) {
                return false;
            }
            if (expect_.payload->max_size && result.payload.size() > *expect_.payload->max_size) {
                return false;
            }
            if (expect_.payload->contains) {
                // Check if payload contains the byte sequence
                const auto& needle = *expect_.payload->contains;
                auto it = std::search(
                    result.payload.begin(), result.payload.end(), needle.begin(), needle.end(),
                    [](std::byte a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; });
                if (it == result.payload.end()) {
                    return false;
                }
            }
            if (expect_.payload->equals) {
                const auto& expected = *expect_.payload->equals;
                if (result.payload.size() != expected.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < expected.size(); ++i) {
                    if (static_cast<std::uint8_t>(result.payload[i]) != expected[i]) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    [[nodiscard]] std::string describe() const override {
        if (!expect_.description.empty()) {
            return expect_.description;
        }

        std::string desc = "Expect";
        if (expect_.someip) {
            desc += " SOME/IP";
            if (expect_.someip->service_id) {
                desc += " service=" + std::to_string(*expect_.someip->service_id);
            }
        }
        if (expect_.doip) {
            desc += " DoIP";
        }
        if (expect_.udp) {
            desc += " UDP";
            if (expect_.udp->dst_port) {
                desc += " port=" + std::to_string(*expect_.udp->dst_port);
            }
        }
        return desc;
    }

private:
    const ExpectStep& expect_;
};

}  // anonymous namespace

std::unique_ptr<IPacketMatcher> create_matcher(const ExpectStep& expect) {
    return std::make_unique<ExpectStepMatcher>(expect);
}

// =============================================================================
// ScenarioRunner Implementation
// =============================================================================

class ScenarioRunner::Impl {
public:
    explicit Impl(RunnerOptions opts) : options_(std::move(opts)) {}

    void set_callbacks(RunnerCallbacks callbacks) { callbacks_ = std::move(callbacks); }

    ScenarioResult run(const Scenario& scenario) {
        ScenarioResult result;
        result.scenario_name = scenario.name;
        auto start_time = std::chrono::steady_clock::now();

        if (callbacks_.on_scenario_start) {
            callbacks_.on_scenario_start(scenario.name);
        }

        if (options_.dry_run) {
            result.passed = true;
            result.total_elapsed = Duration{0};
            return result;
        }

        // Current capture session
        std::unique_ptr<io::CaptureSession> capture;
        std::vector<Packet> captured_packets;

        try {
            std::size_t step_index = 0;
            for (const auto& step : scenario.steps) {
                if (stopped_) {
                    result.error_message = "Execution stopped by user";
                    break;
                }

                if (callbacks_.on_step_start) {
                    callbacks_.on_step_start(step, step_index);
                }

                // Execute step based on type
                std::visit(
                    [&](const auto& s) { execute_step(s, capture, captured_packets, result); },
                    step);

                // Check for early failure
                if (options_.stop_on_first_failure && !result.passed &&
                    !result.expect_results.empty() && !result.expect_results.back().passed) {
                    break;
                }

                ++step_index;
            }
        } catch (const std::exception& e) {
            result.error_message = e.what();
        }

        // Determine overall pass/fail
        result.passed = result.error_message.empty();
        for (const auto& er : result.expect_results) {
            if (!er.passed) {
                result.passed = false;
                break;
            }
        }

        // Save pcap on failure if requested
        if (!result.passed && options_.save_pcap_on_failure && !captured_packets.empty()) {
            auto pcap_path = save_failure_pcap(scenario.name, captured_packets);
            result.pcap_file = pcap_path;
        }

        auto end_time = std::chrono::steady_clock::now();
        result.total_elapsed = std::chrono::duration_cast<Duration>(end_time - start_time);

        if (callbacks_.on_scenario_end) {
            callbacks_.on_scenario_end(result);
        }

        return result;
    }

    std::vector<ScenarioResult> run_all(const std::vector<Scenario>& scenarios) {
        std::vector<ScenarioResult> results;
        results.reserve(scenarios.size());

        for (const auto& scenario : scenarios) {
            if (stopped_)
                break;
            results.push_back(run(scenario));
        }

        return results;
    }

    void stop() { stopped_ = true; }
    [[nodiscard]] bool stopped() const { return stopped_; }
    [[nodiscard]] const RunnerOptions& options() const { return options_; }

private:
    void execute_step(const CaptureStep& step, std::unique_ptr<io::CaptureSession>& capture,
                      std::vector<Packet>& captured_packets, ScenarioResult& /*result*/) {
        // Close existing capture if any
        capture.reset();
        captured_packets.clear();

        if (step.config.pcap_file) {
            // Use pcap file instead of live capture
            // Load packets from file
            auto reader = pcap::PcapReader::open(*step.config.pcap_file);
            if (reader) {
                while (auto pkt = reader->next_packet()) {
                    captured_packets.push_back(std::move(*pkt));
                }
            }
        } else {
            // Start live capture
            io::CaptureSessionOptions opts;
            opts.promiscuous = step.config.promiscuous;

            auto session = io::CaptureSession::create(step.config.interface, opts);
            if (session) {
                // Set filter if specified
                if (!step.config.filter.empty()) {
                    auto filter_result = session->set_filter(step.config.filter);
                    if (!filter_result) {
                        // Filter failed - continue without it
                    }
                }
                capture = std::make_unique<io::CaptureSession>(std::move(*session));
            }
        }
    }

    void execute_step(const SendStep& step, std::unique_ptr<io::CaptureSession>& /*capture*/,
                      std::vector<Packet>& /*captured_packets*/, ScenarioResult& /*result*/) {
        if (step.delay.count() > 0) {
            std::this_thread::sleep_for(step.delay);
        }

        // Packet injection would go here
        // For now, this is a placeholder
        (void)step;
    }

    void execute_step(const WaitStep& step, std::unique_ptr<io::CaptureSession>& /*capture*/,
                      std::vector<Packet>& /*captured_packets*/, ScenarioResult& /*result*/) {
        std::this_thread::sleep_for(step.duration);
    }

    void execute_step(const ExpectStep& step, std::unique_ptr<io::CaptureSession>& capture,
                      std::vector<Packet>& captured_packets, ScenarioResult& result) {
        ExpectResult expect_result;
        expect_result.description =
            step.description.empty() ? create_matcher(step)->describe() : step.description;

        auto start_time = std::chrono::steady_clock::now();
        auto deadline = start_time + step.within;
        auto matcher = create_matcher(step);
        std::size_t match_count = 0;

        if (capture) {
            // Live capture mode
            while (std::chrono::steady_clock::now() < deadline) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());

                auto pkt = capture->next_packet(remaining);
                if (pkt) {
                    captured_packets.push_back(*pkt);

                    if (callbacks_.on_packet_captured) {
                        callbacks_.on_packet_captured(*pkt);
                    }

                    if (matcher->matches(*pkt)) {
                        ++match_count;
                    }

                    // Check if we've met the count requirement
                    if (step.count.evaluate(match_count)) {
                        break;
                    }
                }
            }
        } else {
            // Offline mode (from pcap)
            for (const auto& pkt : captured_packets) {
                if (matcher->matches(pkt)) {
                    ++match_count;
                }
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        expect_result.elapsed = std::chrono::duration_cast<Duration>(end_time - start_time);
        expect_result.packets_matched = match_count;
        expect_result.passed = step.count.evaluate(match_count);

        if (!expect_result.passed) {
            expect_result.failure_reason = "Expected count " + count_expr_string(step.count) +
                                           " but got " + std::to_string(match_count);
        }

        if (callbacks_.on_expect_result) {
            callbacks_.on_expect_result(expect_result);
        }

        result.expect_results.push_back(std::move(expect_result));
    }

    void execute_step(const LogStep& step, std::unique_ptr<io::CaptureSession>& /*capture*/,
                      std::vector<Packet>& /*captured_packets*/, ScenarioResult& /*result*/) {
        if (callbacks_.on_log) {
            callbacks_.on_log("[" + step.level + "] " + step.message);
        }

        if (options_.verbose) {
            std::cerr << "[" << step.level << "] " << step.message << "\n";
        }
    }

    std::string save_failure_pcap(const std::string& scenario_name,
                                  const std::vector<Packet>& packets) {
        // Create filename from scenario name
        std::string safe_name = scenario_name;
        for (char& c : safe_name) {
            if (!std::isalnum(static_cast<unsigned char>(c))) {
                c = '_';
            }
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::ostringstream filename;
        filename << options_.pcap_output_dir << "/" << safe_name << "_" << time << ".pcap";

        auto writer = pcap::PcapWriter::create(filename.str());
        if (writer) {
            for (const auto& pkt : packets) {
                writer->write_packet(pkt);
            }
        }

        return filename.str();
    }

    static std::string count_expr_string(const CountExpression& expr) {
        std::string op_str;
        switch (expr.op) {
            case CompareOp::Equal:
                op_str = "==";
                break;
            case CompareOp::NotEqual:
                op_str = "!=";
                break;
            case CompareOp::GreaterThan:
                op_str = ">";
                break;
            case CompareOp::GreaterEqual:
                op_str = ">=";
                break;
            case CompareOp::LessThan:
                op_str = "<";
                break;
            case CompareOp::LessEqual:
                op_str = "<=";
                break;
        }
        return op_str + " " + std::to_string(expr.value);
    }

    RunnerOptions options_;
    RunnerCallbacks callbacks_;
    std::atomic<bool> stopped_{false};
};

// =============================================================================
// ScenarioRunner Public Interface
// =============================================================================

ScenarioRunner::ScenarioRunner(RunnerOptions opts)
    : impl_(std::make_unique<Impl>(std::move(opts))) {}

ScenarioRunner::~ScenarioRunner() = default;

ScenarioRunner::ScenarioRunner(ScenarioRunner&&) noexcept = default;
ScenarioRunner& ScenarioRunner::operator=(ScenarioRunner&&) noexcept = default;

void ScenarioRunner::set_callbacks(RunnerCallbacks callbacks) {
    impl_->set_callbacks(std::move(callbacks));
}

ScenarioResult ScenarioRunner::run(const Scenario& scenario) {
    return impl_->run(scenario);
}

std::vector<ScenarioResult> ScenarioRunner::run_all(const std::vector<Scenario>& scenarios) {
    return impl_->run_all(scenarios);
}

void ScenarioRunner::stop() {
    impl_->stop();
}

bool ScenarioRunner::stopped() const {
    return impl_->stopped();
}

const RunnerOptions& ScenarioRunner::options() const {
    return impl_->options();
}

// =============================================================================
// Batch Running Functions
// =============================================================================

BatchResult run_scenarios_in_directory(const std::filesystem::path& dir, const RunnerOptions& opts,
                                       const std::vector<std::string>& tags) {
    BatchResult batch;
    auto start_time = std::chrono::steady_clock::now();

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (ext == ".yaml" || ext == ".yml" || ext == ".json") {
            files.push_back(entry.path());
        }
    }

    ScenarioRunner runner(opts);

    for (const auto& file : files) {
        auto parse_result = parse_scenario_file(file);
        if (!parse_result) {
            ScenarioResult fail_result;
            fail_result.scenario_name = file.string();
            fail_result.passed = false;
            fail_result.error_message = "Parse error: " + parse_result.error().to_string();
            batch.results.push_back(std::move(fail_result));
            ++batch.failed;
            continue;
        }

        const auto& scenario = *parse_result;

        // Tag filtering
        if (!tags.empty()) {
            bool has_tag = false;
            for (const auto& tag : tags) {
                if (std::find(scenario.tags.begin(), scenario.tags.end(), tag) !=
                    scenario.tags.end()) {
                    has_tag = true;
                    break;
                }
            }
            if (!has_tag) {
                ++batch.skipped;
                continue;
            }
        }

        auto result = runner.run(scenario);
        if (result.passed) {
            ++batch.passed;
        } else {
            ++batch.failed;
        }
        batch.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    batch.total_elapsed = std::chrono::duration_cast<Duration>(end_time - start_time);

    return batch;
}

BatchResult run_scenario_files(const std::vector<std::filesystem::path>& files,
                               const RunnerOptions& opts) {
    BatchResult batch;
    auto start_time = std::chrono::steady_clock::now();

    ScenarioRunner runner(opts);

    for (const auto& file : files) {
        auto parse_result = parse_scenario_file(file);
        if (!parse_result) {
            ScenarioResult fail_result;
            fail_result.scenario_name = file.string();
            fail_result.passed = false;
            fail_result.error_message = "Parse error: " + parse_result.error().to_string();
            batch.results.push_back(std::move(fail_result));
            ++batch.failed;
            continue;
        }

        auto result = runner.run(*parse_result);
        if (result.passed) {
            ++batch.passed;
        } else {
            ++batch.failed;
        }
        batch.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    batch.total_elapsed = std::chrono::duration_cast<Duration>(end_time - start_time);

    return batch;
}

}  // namespace wadjet::scenario

/// @file live_assert.hpp
/// @brief Live-assert mode for real-time packet validation
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This module provides real-time assertion capabilities on live packet streams.
/// Assertions are checked as packets arrive, with immediate failure on violation.
/// This is useful for:
/// - Real-time protocol conformance testing
/// - Detecting unexpected packets immediately
/// - Monitoring for specific error conditions
///
/// Example:
/// @code
/// using namespace wadjet::testing;
///
/// // Create live assertion session
/// LiveAssertSession session("eth0");
/// session.set_filter("udp port 30490");
///
/// // Add assertions that must hold for all packets
/// session.assert_all(IsUDP());
/// session.assert_all(HasDestPort(30490));
///
/// // Add assertions that must hold for specific packet types
/// session.when(HasSOMEIPServiceId(0x1234))
///        .assert_that(IsSOMEIPRequest());
///
/// // Add assertion that must never match
/// session.assert_never(HasSOMEIPMessageType(MessageType::ERROR));
///
/// // Run for 5 seconds - fails immediately on any violation
/// ASSERT_TRUE(session.run_for(5s));
/// @endcode

#pragma once

#include "wadjet/io/capture_session.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/pcap/pcap_writer.hpp"
#include "wadjet/protocols/dispatcher.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace wadjet::testing {

/// Result of a live assertion check
struct AssertionResult {
    bool passed = true;
    std::string description;
    std::optional<Packet> violating_packet;
    std::size_t packet_index = 0;

    explicit operator bool() const { return passed; }
};

/// A single assertion rule
class AssertionRule {
public:
    using PacketPredicate = std::function<bool(const Packet&)>;

    enum class Type {
        ASSERT_ALL,    // Must hold for all packets
        ASSERT_NEVER,  // Must never match
        ASSERT_WHEN,   // When condition matches, assertion must hold
        EXPECT_WITHIN  // Must match within timeout
    };

    AssertionRule(Type type, std::string description, PacketPredicate predicate,
                  std::optional<PacketPredicate> condition = std::nullopt)
        : type_(type),
          description_(std::move(description)),
          predicate_(std::move(predicate)),
          condition_(std::move(condition)) {}

    /// Check the rule against a packet
    [[nodiscard]] AssertionResult check(const Packet& packet, std::size_t index) const {
        AssertionResult result;
        result.packet_index = index;
        result.description = description_;

        switch (type_) {
            case Type::ASSERT_ALL:
                if (!predicate_(packet)) {
                    result.passed = false;
                    result.violating_packet = packet;
                }
                break;

            case Type::ASSERT_NEVER:
                if (predicate_(packet)) {
                    result.passed = false;
                    result.violating_packet = packet;
                }
                break;

            case Type::ASSERT_WHEN:
                if (condition_ && (*condition_)(packet)) {
                    if (!predicate_(packet)) {
                        result.passed = false;
                        result.violating_packet = packet;
                    }
                }
                break;

            case Type::EXPECT_WITHIN:
                // This type doesn't fail on individual packets
                // It's checked at the end
                break;
        }

        return result;
    }

    [[nodiscard]] Type type() const { return type_; }
    [[nodiscard]] const std::string& description() const { return description_; }
    [[nodiscard]] const PacketPredicate& predicate() const { return predicate_; }

    /// Mark expect_within as satisfied
    void mark_satisfied() { satisfied_ = true; }
    [[nodiscard]] bool is_satisfied() const { return satisfied_; }

    /// Reset satisfaction state (for reuse)
    void reset_satisfied() { satisfied_ = false; }

private:
    Type type_;
    std::string description_;
    PacketPredicate predicate_;
    std::optional<PacketPredicate> condition_;
    bool satisfied_ = false;
};

/// Builder for conditional assertions (when-then pattern)
class ConditionalAssertionBuilder {
public:
    ConditionalAssertionBuilder(std::vector<AssertionRule>& rules,
                                AssertionRule::PacketPredicate condition,
                                std::string condition_desc)
        : rules_(rules),
          condition_(std::move(condition)),
          condition_desc_(std::move(condition_desc)) {}

    /// Add assertion that must hold when condition matches
    template <typename Matcher>
    ConditionalAssertionBuilder& assert_that(const Matcher& matcher) {
        std::ostringstream desc;
        desc << "When [" << condition_desc_ << "] then must match assertion";

        rules_.emplace_back(
            AssertionRule::Type::ASSERT_WHEN, desc.str(),
            [matcher](const Packet& pkt) { return ::testing::Value(pkt, matcher); }, condition_);
        return *this;
    }

    /// Add assertion with custom predicate
    ConditionalAssertionBuilder& assert_that(AssertionRule::PacketPredicate predicate,
                                             const std::string& desc = "custom predicate") {
        std::ostringstream full_desc;
        full_desc << "When [" << condition_desc_ << "] then " << desc;

        rules_.emplace_back(AssertionRule::Type::ASSERT_WHEN, full_desc.str(), std::move(predicate),
                            condition_);
        return *this;
    }

private:
    std::vector<AssertionRule>& rules_;
    AssertionRule::PacketPredicate condition_;
    std::string condition_desc_;
};

/// Live assertion session for real-time packet validation
class LiveAssertSession {
public:
    using PacketPredicate = AssertionRule::PacketPredicate;

    /// Create a live assertion session on the specified interface
    explicit LiveAssertSession(const std::string& interface = "lo") : interface_(interface) {}

    /// Set BPF filter
    LiveAssertSession& set_filter(const std::string& filter) {
        filter_ = filter;
        return *this;
    }

    /// Enable saving packets on failure
    LiveAssertSession& save_on_failure(const std::filesystem::path& dir) {
        save_on_failure_ = true;
        failure_dir_ = dir;
        return *this;
    }

    // ==========================================================================
    // Assertion builders
    // ==========================================================================

    /// Add assertion that must hold for all packets
    template <typename Matcher>
    LiveAssertSession& assert_all(const Matcher& matcher, const std::string& desc = "") {
        std::string description = desc.empty() ? "All packets must match" : desc;
        rules_.emplace_back(
            AssertionRule::Type::ASSERT_ALL, description,
            [matcher](const Packet& pkt) { return ::testing::Value(pkt, matcher); });
        return *this;
    }

    /// Add assertion with custom predicate that must hold for all packets
    LiveAssertSession& assert_all(PacketPredicate predicate,
                                  const std::string& desc = "custom predicate") {
        rules_.emplace_back(AssertionRule::Type::ASSERT_ALL, desc, std::move(predicate));
        return *this;
    }

    /// Add assertion that must never match any packet
    template <typename Matcher>
    LiveAssertSession& assert_never(const Matcher& matcher, const std::string& desc = "") {
        std::string description = desc.empty() ? "No packet should match" : desc;
        rules_.emplace_back(
            AssertionRule::Type::ASSERT_NEVER, description,
            [matcher](const Packet& pkt) { return ::testing::Value(pkt, matcher); });
        return *this;
    }

    /// Add assertion with custom predicate that must never match
    LiveAssertSession& assert_never(PacketPredicate predicate,
                                    const std::string& desc = "custom predicate") {
        rules_.emplace_back(AssertionRule::Type::ASSERT_NEVER, desc, std::move(predicate));
        return *this;
    }

    /// Add conditional assertion (when-then pattern)
    template <typename Matcher>
    ConditionalAssertionBuilder when(const Matcher& matcher,
                                     const std::string& desc = "condition") {
        return ConditionalAssertionBuilder(
            rules_, [matcher](const Packet& pkt) { return ::testing::Value(pkt, matcher); }, desc);
    }

    /// Add conditional assertion with custom predicate
    ConditionalAssertionBuilder when(PacketPredicate condition,
                                     const std::string& desc = "condition") {
        return ConditionalAssertionBuilder(rules_, std::move(condition), desc);
    }

    /// Add expectation that at least one packet must match within the run
    template <typename Matcher>
    LiveAssertSession& expect_within(const Matcher& matcher, const std::string& desc = "") {
        std::string description = desc.empty() ? "At least one packet must match" : desc;
        rules_.emplace_back(
            AssertionRule::Type::EXPECT_WITHIN, description,
            [matcher](const Packet& pkt) { return ::testing::Value(pkt, matcher); });
        return *this;
    }

    // ==========================================================================
    // Execution
    // ==========================================================================

    /// Run assertions for a specified duration
    template <typename Rep, typename Period>
    AssertionResult run_for(std::chrono::duration<Rep, Period> duration) {
        auto session = io::CaptureSession::create(interface_);
        if (!session.is_ok()) {
            AssertionResult result;
            result.passed = false;
            result.description = "Failed to create capture session: " + session.error().message;
            return result;
        }

        if (!filter_.empty()) {
            auto filter_result = session->set_filter(filter_);
            if (!filter_result.is_ok()) {
                AssertionResult result;
                result.passed = false;
                result.description = "Failed to set filter: " + filter_result.error().message;
                return result;
            }
        }

        auto start_result = session->start();
        if (!start_result.is_ok()) {
            AssertionResult result;
            result.passed = false;
            result.description = "Failed to start capture: " + start_result.error().message;
            return result;
        }

        auto deadline = std::chrono::steady_clock::now() + duration;
        std::size_t packet_count = 0;

        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                break;

            auto packet = session->next_packet(remaining);
            if (packet.has_value()) {
                captured_packets_.push_back(*packet);
                auto result = check_packet(*packet, packet_count);
                if (!result.passed) {
                    session->stop();
                    save_failure_trace();
                    return result;
                }
                ++packet_count;
            }
        }

        session->stop();

        // Check expect_within rules
        for (auto& rule : rules_) {
            if (rule.type() == AssertionRule::Type::EXPECT_WITHIN && !rule.is_satisfied()) {
                AssertionResult result;
                result.passed = false;
                result.description = "Expected packet not found: " + rule.description();
                save_failure_trace();
                return result;
            }
        }

        AssertionResult result;
        result.passed = true;
        result.description = "All assertions passed (" + std::to_string(packet_count) + " packets)";
        return result;
    }

    /// Run assertions until a stop condition is met
    template <typename StopPredicate, typename Rep, typename Period>
    AssertionResult run_until(StopPredicate&& stop_condition,
                              std::chrono::duration<Rep, Period> timeout) {
        auto session = io::CaptureSession::create(interface_);
        if (!session.is_ok()) {
            AssertionResult result;
            result.passed = false;
            result.description = "Failed to create capture session";
            return result;
        }

        if (!filter_.empty()) {
            auto filter_result = session->set_filter(filter_);
            if (!filter_result.is_ok()) {
                AssertionResult result;
                result.passed = false;
                result.description = "Failed to set filter";
                return result;
            }
        }

        auto start_result = session->start();
        if (!start_result.is_ok()) {
            AssertionResult result;
            result.passed = false;
            result.description = "Failed to start capture";
            return result;
        }

        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t packet_count = 0;

        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                break;

            auto packet = session->next_packet(remaining);
            if (packet.has_value()) {
                captured_packets_.push_back(*packet);
                auto result = check_packet(*packet, packet_count);
                if (!result.passed) {
                    session->stop();
                    save_failure_trace();
                    return result;
                }

                if (stop_condition(*packet)) {
                    session->stop();
                    // Check expect_within rules
                    for (auto& rule : rules_) {
                        if (rule.type() == AssertionRule::Type::EXPECT_WITHIN &&
                            !rule.is_satisfied()) {
                            AssertionResult fail_result;
                            fail_result.passed = false;
                            fail_result.description =
                                "Expected packet not found: " + rule.description();
                            save_failure_trace();
                            return fail_result;
                        }
                    }

                    AssertionResult success;
                    success.passed = true;
                    success.description = "Stop condition met, all assertions passed";
                    return success;
                }
                ++packet_count;
            }
        }

        session->stop();

        // Timeout without stop condition
        AssertionResult result;
        result.passed = false;
        result.description = "Timeout before stop condition was met";
        save_failure_trace();
        return result;
    }

    /// Get captured packets
    [[nodiscard]] const std::vector<Packet>& captured_packets() const { return captured_packets_; }

    /// Clear captured packets and reset state
    void reset() {
        captured_packets_.clear();
        // Reset expect_within satisfaction state on all rules
        for (auto& rule : rules_) {
            rule.reset_satisfied();
        }
    }

    /// Get the number of rules
    [[nodiscard]] std::size_t rule_count() const { return rules_.size(); }

private:
    AssertionResult check_packet(const Packet& packet, std::size_t index) {
        for (auto& rule : rules_) {
            auto result = rule.check(packet, index);
            if (!result.passed) {
                return result;
            }

            // Mark expect_within as satisfied if it matches
            if (rule.type() == AssertionRule::Type::EXPECT_WITHIN && rule.predicate()(packet)) {
                rule.mark_satisfied();
            }
        }

        AssertionResult success;
        success.passed = true;
        return success;
    }

    void save_failure_trace() {
        if (!save_on_failure_ || captured_packets_.empty())
            return;

        std::error_code ec;
        std::filesystem::create_directories(failure_dir_, ec);

        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        auto path = failure_dir_ / ("live_assert_failure_" + std::to_string(timestamp) + ".pcap");

        auto writer = pcap::PcapWriter::create(path);
        if (writer.is_ok()) {
            for (const auto& pkt : captured_packets_) {
                writer->write_packet(pkt.view());
            }
            std::cerr << "\n[WADJET] Saved failure trace to: " << path << "\n";
        }
    }

    std::string interface_;
    std::string filter_;
    std::vector<AssertionRule> rules_;
    std::vector<Packet> captured_packets_;
    bool save_on_failure_ = false;
    std::filesystem::path failure_dir_ = "/tmp/wadjet_failures";
};

// =============================================================================
// GoogleTest Macros for Live Assert
// =============================================================================

/// Run live assertion session and assert success
#define WADJET_LIVE_ASSERT(session, duration)               \
    do {                                                    \
        auto _result = (session).run_for(duration);         \
        ASSERT_TRUE(_result.passed) << _result.description; \
    } while (0)

/// Run live assertion session and expect success (non-fatal)
#define WADJET_LIVE_EXPECT(session, duration)               \
    do {                                                    \
        auto _result = (session).run_for(duration);         \
        EXPECT_TRUE(_result.passed) << _result.description; \
    } while (0)

/// Run live assertion session until condition, assert success
#define WADJET_LIVE_ASSERT_UNTIL(session, condition, timeout)   \
    do {                                                        \
        auto _result = (session).run_until(condition, timeout); \
        ASSERT_TRUE(_result.passed) << _result.description;     \
    } while (0)

/// Run live assertion session until condition, expect success
#define WADJET_LIVE_EXPECT_UNTIL(session, condition, timeout)   \
    do {                                                        \
        auto _result = (session).run_until(condition, timeout); \
        EXPECT_TRUE(_result.passed) << _result.description;     \
    } while (0)

}  // namespace wadjet::testing

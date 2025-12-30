/// @file record_replay.hpp
/// @brief Record-then-assert mode for packet capture testing
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This module provides the ability to record packets first, then run
/// assertions on the recorded stream. This is useful for:
/// - Offline analysis of captured traffic
/// - Replay testing with deterministic results
/// - Debugging failing tests with recorded traces
///
/// Example:
/// @code
/// using namespace wadjet::testing;
///
/// // Record packets from live capture
/// RecordSession recorder("eth0");
/// recorder.set_filter("udp port 30490");
/// recorder.record_for(5s);
///
/// // Get recorded stream and run assertions
/// auto stream = recorder.get_stream();
/// 
/// EXPECT_TRUE(stream.any_matches(HasSOMEIPServiceId(0x1234)));
/// EXPECT_GE(stream.count_matches(IsUDP()), 10);
/// 
/// auto services = stream.filter(HasSOMEIPServiceId(0x1234));
/// for (const auto& pkt : services) {
///     EXPECT_THAT(pkt, IsSOMEIPRequest());
/// }
/// @endcode

#pragma once

#include <vector>
#include <chrono>
#include <optional>
#include <functional>
#include <algorithm>
#include <filesystem>
#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "wadjet/net/packet.hpp"
#include "wadjet/io/capture_session.hpp"
#include "wadjet/pcap/pcap_reader.hpp"
#include "wadjet/pcap/pcap_writer.hpp"
#include "wadjet/protocols/dispatcher.hpp"

namespace wadjet::testing {

/// A recorded packet stream that can be queried and filtered
class RecordedStream {
public:
    using iterator = std::vector<Packet>::const_iterator;
    using PacketPredicate = std::function<bool(const Packet&)>;
    
    RecordedStream() = default;
    explicit RecordedStream(std::vector<Packet> packets) 
        : packets_(std::move(packets)) {}
    
    // ==========================================================================
    // Basic accessors
    // ==========================================================================
    
    /// Get all packets
    [[nodiscard]] const std::vector<Packet>& packets() const { return packets_; }
    
    /// Get packet count
    [[nodiscard]] std::size_t size() const { return packets_.size(); }
    
    /// Check if empty
    [[nodiscard]] bool empty() const { return packets_.empty(); }
    
    /// Iterators
    [[nodiscard]] iterator begin() const { return packets_.begin(); }
    [[nodiscard]] iterator end() const { return packets_.end(); }
    
    /// Get packet by index
    [[nodiscard]] const Packet& operator[](std::size_t idx) const { return packets_[idx]; }
    
    /// Get packet by index with bounds checking
    [[nodiscard]] const Packet& at(std::size_t idx) const { return packets_.at(idx); }
    
    /// Get first packet (if any)
    [[nodiscard]] std::optional<std::reference_wrapper<const Packet>> first() const {
        if (packets_.empty()) return std::nullopt;
        return std::cref(packets_.front());
    }
    
    /// Get last packet (if any)
    [[nodiscard]] std::optional<std::reference_wrapper<const Packet>> last() const {
        if (packets_.empty()) return std::nullopt;
        return std::cref(packets_.back());
    }
    
    // ==========================================================================
    // Filtering and querying
    // ==========================================================================
    
    /// Filter packets matching a predicate
    [[nodiscard]] RecordedStream filter(PacketPredicate predicate) const {
        std::vector<Packet> result;
        std::copy_if(packets_.begin(), packets_.end(), std::back_inserter(result),
                     std::move(predicate));
        return RecordedStream(std::move(result));
    }
    
    /// Filter packets matching a gMock matcher
    template <typename Matcher>
    [[nodiscard]] RecordedStream filter_matches(const Matcher& matcher) const {
        return filter([&matcher](const Packet& pkt) {
            return ::testing::Value(pkt, matcher);
        });
    }
    
    /// Check if any packet matches a predicate
    [[nodiscard]] bool any_matches(PacketPredicate predicate) const {
        return std::any_of(packets_.begin(), packets_.end(), std::move(predicate));
    }
    
    /// Check if any packet matches a gMock matcher
    template <typename Matcher>
    [[nodiscard]] bool filter_any_matches(const Matcher& matcher) const {
        return std::any_of(packets_.begin(), packets_.end(), 
            [&matcher](const Packet& pkt) {
                return ::testing::Value(pkt, matcher);
            });
    }
    
    /// Check if all packets match a predicate
    [[nodiscard]] bool all_match(PacketPredicate predicate) const {
        return std::all_of(packets_.begin(), packets_.end(), std::move(predicate));
    }
    
    /// Check if all packets match a gMock matcher
    template <typename Matcher>
    [[nodiscard]] bool filter_all_match(const Matcher& matcher) const {
        return std::all_of(packets_.begin(), packets_.end(), 
            [&matcher](const Packet& pkt) {
                return ::testing::Value(pkt, matcher);
            });
    }
    
    /// Check if no packets match a predicate
    [[nodiscard]] bool none_match(PacketPredicate predicate) const {
        return std::none_of(packets_.begin(), packets_.end(), std::move(predicate));
    }
    
    /// Count packets matching a predicate
    [[nodiscard]] std::size_t count_matches(PacketPredicate predicate) const {
        return static_cast<std::size_t>(
            std::count_if(packets_.begin(), packets_.end(), std::move(predicate)));
    }
    
    /// Count packets matching a gMock matcher
    template <typename Matcher>
    [[nodiscard]] std::size_t filter_count_matches(const Matcher& matcher) const {
        return static_cast<std::size_t>(
            std::count_if(packets_.begin(), packets_.end(), 
                [&matcher](const Packet& pkt) {
                    return ::testing::Value(pkt, matcher);
                }));
    }
    
    /// Find first packet matching a predicate
    [[nodiscard]] std::optional<std::reference_wrapper<const Packet>> 
    find_first(PacketPredicate predicate) const {
        auto it = std::find_if(packets_.begin(), packets_.end(), std::move(predicate));
        if (it == packets_.end()) return std::nullopt;
        return std::cref(*it);
    }
    
    /// Find first packet matching a gMock matcher
    template <typename Matcher>
    [[nodiscard]] std::optional<std::reference_wrapper<const Packet>>
    filter_find_first(const Matcher& matcher) const {
        auto it = std::find_if(packets_.begin(), packets_.end(), 
            [&matcher](const Packet& pkt) {
                return ::testing::Value(pkt, matcher);
            });
        if (it == packets_.end()) return std::nullopt;
        return std::cref(*it);
    }
    
    /// Find index of first packet matching a predicate
    [[nodiscard]] std::optional<std::size_t> 
    find_index(PacketPredicate predicate) const {
        auto it = std::find_if(packets_.begin(), packets_.end(), std::move(predicate));
        if (it == packets_.end()) return std::nullopt;
        return static_cast<std::size_t>(std::distance(packets_.begin(), it));
    }
    
    // ==========================================================================
    // Slicing operations
    // ==========================================================================
    
    /// Get first N packets
    [[nodiscard]] RecordedStream take(std::size_t n) const {
        auto count = std::min(n, packets_.size());
        return RecordedStream(std::vector<Packet>(packets_.begin(), 
            packets_.begin() + static_cast<std::ptrdiff_t>(count)));
    }
    
    /// Skip first N packets
    [[nodiscard]] RecordedStream skip(std::size_t n) const {
        if (n >= packets_.size()) return RecordedStream();
        return RecordedStream(std::vector<Packet>(
            packets_.begin() + static_cast<std::ptrdiff_t>(n), packets_.end()));
    }
    
    /// Take packets while predicate is true
    [[nodiscard]] RecordedStream take_while(PacketPredicate predicate) const {
        std::vector<Packet> result;
        for (const auto& pkt : packets_) {
            if (!predicate(pkt)) break;
            result.push_back(pkt);
        }
        return RecordedStream(std::move(result));
    }
    
    /// Skip packets while predicate is true
    [[nodiscard]] RecordedStream skip_while(PacketPredicate predicate) const {
        auto it = std::find_if_not(packets_.begin(), packets_.end(), std::move(predicate));
        return RecordedStream(std::vector<Packet>(it, packets_.end()));
    }
    
    /// Get slice [start, end)
    [[nodiscard]] RecordedStream slice(std::size_t start, std::size_t end) const {
        start = std::min(start, packets_.size());
        end = std::min(end, packets_.size());
        if (start >= end) return RecordedStream();
        return RecordedStream(std::vector<Packet>(
            packets_.begin() + static_cast<std::ptrdiff_t>(start), 
            packets_.begin() + static_cast<std::ptrdiff_t>(end)));
    }
    
    // ==========================================================================
    // Sequence assertions
    // ==========================================================================
    
    /// Check if packets appear in order (matchers must match in sequence)
    template <typename... Matchers>
    [[nodiscard]] bool has_sequence(const Matchers&... matchers) const {
        std::array<std::function<bool(const Packet&)>, sizeof...(Matchers)> checks = {
            [&matchers](const Packet& pkt) { return ::testing::Value(pkt, matchers); }...
        };
        
        std::size_t matcher_idx = 0;
        for (const auto& pkt : packets_) {
            if (matcher_idx < checks.size() && checks[matcher_idx](pkt)) {
                ++matcher_idx;
            }
        }
        return matcher_idx == checks.size();
    }
    
    /// Check that all consecutive pairs satisfy a relationship
    template <typename BinaryPredicate>
    [[nodiscard]] bool all_consecutive_satisfy(BinaryPredicate pred) const {
        if (packets_.size() < 2) return true;
        for (std::size_t i = 0; i + 1 < packets_.size(); ++i) {
            if (!pred(packets_[i], packets_[i + 1])) return false;
        }
        return true;
    }
    
    // ==========================================================================
    // Persistence
    // ==========================================================================
    
    /// Save to PCAP file
    bool save_to_pcap(const std::filesystem::path& path) const {
        auto writer = pcap::PcapWriter::create(path);
        if (!writer.is_ok()) return false;
        
        for (const auto& pkt : packets_) {
            auto result = writer->write_packet(pkt.view());
            if (!result.is_ok()) return false;
        }
        return true;
    }
    
    /// Load from PCAP file
    static std::optional<RecordedStream> load_from_pcap(const std::filesystem::path& path) {
        auto reader = pcap::PcapReader::open(path);
        if (!reader.is_ok()) return std::nullopt;
        
        std::vector<Packet> packets;
        while (auto pkt = reader->next_packet()) {
            packets.push_back(std::move(*pkt));
        }
        return RecordedStream(std::move(packets));
    }
    
    // ==========================================================================
    // Mutation (for building streams)
    // ==========================================================================
    
    /// Add a packet to the stream
    void add(Packet packet) { packets_.push_back(std::move(packet)); }
    
    /// Add multiple packets
    void add_all(const std::vector<Packet>& packets) {
        packets_.insert(packets_.end(), packets.begin(), packets.end());
    }
    
    /// Clear all packets
    void clear() { packets_.clear(); }
    
    /// Concatenate two streams
    [[nodiscard]] RecordedStream operator+(const RecordedStream& other) const {
        std::vector<Packet> result = packets_;
        result.insert(result.end(), other.packets_.begin(), other.packets_.end());
        return RecordedStream(std::move(result));
    }

private:
    std::vector<Packet> packets_;
};

/// Session for recording packets from live capture or PCAP files
class RecordSession {
public:
    /// Create a record session on the specified interface
    explicit RecordSession(const std::string& interface = "lo") 
        : interface_(interface) {}
    
    /// Set BPF filter
    RecordSession& set_filter(const std::string& filter) {
        filter_ = filter;
        return *this;
    }
    
    /// Record packets for a specified duration
    template <typename Rep, typename Period>
    bool record_for(std::chrono::duration<Rep, Period> duration) {
        auto session = io::CaptureSession::create(interface_);
        if (!session.is_ok()) return false;
        
        if (!filter_.empty()) {
            auto filter_result = session->set_filter(filter_);
            if (!filter_result.is_ok()) return false;
        }
        
        auto start_result = session->start();
        if (!start_result.is_ok()) return false;
        
        auto deadline = std::chrono::steady_clock::now() + duration;
        auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0) break;
            
            auto packet = session->next_packet(remaining);
            if (packet.has_value()) {
                stream_.add(std::move(*packet));
            }
        }
        
        session->stop();
        return true;
    }
    
    /// Record packets until predicate returns true
    template <typename Predicate, typename Rep, typename Period>
    bool record_until(Predicate&& predicate, 
                      std::chrono::duration<Rep, Period> timeout) {
        auto session = io::CaptureSession::create(interface_);
        if (!session.is_ok()) return false;
        
        if (!filter_.empty()) {
            auto filter_result = session->set_filter(filter_);
            if (!filter_result.is_ok()) return false;
        }
        
        auto start_result = session->start();
        if (!start_result.is_ok()) return false;
        
        auto deadline = std::chrono::steady_clock::now() + timeout;
        
        while (std::chrono::steady_clock::now() < deadline) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0) break;
            
            auto packet = session->next_packet(remaining);
            if (packet.has_value()) {
                stream_.add(*packet);
                if (predicate(*packet)) {
                    session->stop();
                    return true;
                }
            }
        }
        
        session->stop();
        return false;  // Timeout without predicate match
    }
    
    /// Record packets until N packets matching predicate are captured
    template <typename Predicate, typename Rep, typename Period>
    bool record_n_matching(std::size_t count, Predicate&& predicate,
                           std::chrono::duration<Rep, Period> timeout) {
        std::size_t matches = 0;
        return record_until([&](const Packet& pkt) {
            if (predicate(pkt)) {
                ++matches;
            }
            return matches >= count;
        }, timeout);
    }
    
    /// Load packets from PCAP file instead of live capture
    bool load_from_pcap(const std::filesystem::path& path) {
        auto loaded = RecordedStream::load_from_pcap(path);
        if (!loaded.has_value()) return false;
        stream_ = std::move(*loaded);
        return true;
    }
    
    /// Get the recorded stream
    [[nodiscard]] RecordedStream& stream() { return stream_; }
    [[nodiscard]] const RecordedStream& stream() const { return stream_; }
    
    /// Get the recorded stream (move)
    [[nodiscard]] RecordedStream get_stream() { return std::move(stream_); }
    
    /// Clear recorded packets
    void clear() { stream_.clear(); }
    
    /// Save recorded stream to PCAP
    bool save_to_pcap(const std::filesystem::path& path) const {
        return stream_.save_to_pcap(path);
    }

private:
    std::string interface_;
    std::string filter_;
    RecordedStream stream_;
};

// =============================================================================
// GoogleTest Macros for Record-then-Assert
// =============================================================================

/// Assert that the stream contains at least one packet matching the matcher
#define WADJET_ASSERT_STREAM_CONTAINS(stream, matcher) \
    ASSERT_TRUE((stream).filter_any_matches(matcher)) \
        << "Expected stream to contain packet matching: " << #matcher

/// Expect that the stream contains at least one packet matching the matcher
#define WADJET_EXPECT_STREAM_CONTAINS(stream, matcher) \
    EXPECT_TRUE((stream).filter_any_matches(matcher)) \
        << "Expected stream to contain packet matching: " << #matcher

/// Assert that all packets in the stream match the matcher
#define WADJET_ASSERT_ALL_MATCH(stream, matcher) \
    ASSERT_TRUE((stream).filter_all_match(matcher)) \
        << "Expected all packets to match: " << #matcher

/// Expect that all packets in the stream match the matcher
#define WADJET_EXPECT_ALL_MATCH(stream, matcher) \
    EXPECT_TRUE((stream).filter_all_match(matcher)) \
        << "Expected all packets to match: " << #matcher

/// Assert that no packets in the stream match the matcher
#define WADJET_ASSERT_NONE_MATCH(stream, matcher) \
    ASSERT_TRUE((stream).none_match([&](const auto& pkt) { \
        return ::testing::Value(pkt, matcher); \
    })) << "Expected no packets to match: " << #matcher

/// Expect that no packets in the stream match the matcher
#define WADJET_EXPECT_NONE_MATCH(stream, matcher) \
    EXPECT_TRUE((stream).none_match([&](const auto& pkt) { \
        return ::testing::Value(pkt, matcher); \
    })) << "Expected no packets to match: " << #matcher

/// Assert the count of matching packets
#define WADJET_ASSERT_COUNT(stream, matcher, expected_count) \
    ASSERT_EQ((stream).filter_count_matches(matcher), static_cast<std::size_t>(expected_count)) \
        << "Expected " << expected_count << " packets matching: " << #matcher

/// Expect the count of matching packets
#define WADJET_EXPECT_COUNT(stream, matcher, expected_count) \
    EXPECT_EQ((stream).filter_count_matches(matcher), static_cast<std::size_t>(expected_count)) \
        << "Expected " << expected_count << " packets matching: " << #matcher

/// Assert at least N matching packets
#define WADJET_ASSERT_AT_LEAST(stream, matcher, min_count) \
    ASSERT_GE((stream).filter_count_matches(matcher), static_cast<std::size_t>(min_count)) \
        << "Expected at least " << min_count << " packets matching: " << #matcher

/// Expect at least N matching packets
#define WADJET_EXPECT_AT_LEAST(stream, matcher, min_count) \
    EXPECT_GE((stream).filter_count_matches(matcher), static_cast<std::size_t>(min_count)) \
        << "Expected at least " << min_count << " packets matching: " << #matcher

}  // namespace wadjet::testing

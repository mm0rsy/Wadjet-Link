/// @file test_record_replay.cpp
/// @brief Tests for record-then-assert mode
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>

#include "wadjet/testing/testing.hpp"
#include "wadjet/testing/generators.hpp"

using namespace wadjet;
using namespace wadjet::testing;
using namespace wadjet::testing::generators;

// =============================================================================
// RecordedStream Unit Tests (no live capture needed)
// =============================================================================

class RecordedStreamTest : public ::testing::Test {
protected:
    PacketGenerator gen_{42};
    
    RecordedStream create_test_stream(std::size_t count) {
        std::vector<Packet> packets;
        for (std::size_t i = 0; i < count; ++i) {
            packets.push_back(gen_.udp_packet(i));
        }
        return RecordedStream(std::move(packets));
    }
    
    RecordedStream create_mixed_stream() {
        std::vector<Packet> packets;
        for (int i = 0; i < 5; ++i) {
            packets.push_back(gen_.udp_packet(10));
        }
        for (int i = 0; i < 3; ++i) {
            packets.push_back(gen_.tcp_packet(10));
        }
        for (int i = 0; i < 2; ++i) {
            packets.push_back(gen_.udp_packet(10));
        }
        return RecordedStream(std::move(packets));
    }
};

TEST_F(RecordedStreamTest, EmptyStream) {
    RecordedStream stream;
    
    EXPECT_TRUE(stream.empty());
    EXPECT_EQ(stream.size(), 0u);
    EXPECT_FALSE(stream.first().has_value());
    EXPECT_FALSE(stream.last().has_value());
}

TEST_F(RecordedStreamTest, BasicAccessors) {
    auto stream = create_test_stream(5);
    
    EXPECT_FALSE(stream.empty());
    EXPECT_EQ(stream.size(), 5u);
    EXPECT_TRUE(stream.first().has_value());
    EXPECT_TRUE(stream.last().has_value());
}

TEST_F(RecordedStreamTest, IndexAccess) {
    auto stream = create_test_stream(3);
    
    EXPECT_GT(stream[0].size(), 0u);
    EXPECT_GT(stream.at(1).size(), 0u);
    EXPECT_THROW((void)stream.at(10), std::out_of_range);
}

TEST_F(RecordedStreamTest, Iteration) {
    auto stream = create_test_stream(5);
    
    std::size_t count = 0;
    for (const auto& pkt : stream) {
        EXPECT_GT(pkt.size(), 0u);
        ++count;
    }
    EXPECT_EQ(count, 5u);
}

TEST_F(RecordedStreamTest, FilterWithPredicate) {
    auto stream = create_mixed_stream();
    
    auto udp_only = stream.filter([](const Packet& pkt) {
        return ::testing::Value(pkt, IsUDP());
    });
    
    EXPECT_EQ(udp_only.size(), 7u);  // 5 + 2 UDP packets
    EXPECT_TRUE(udp_only.filter_all_match(IsUDP()));
}

TEST_F(RecordedStreamTest, FilterWithMatcher) {
    auto stream = create_mixed_stream();
    
    auto tcp_only = stream.filter_matches(IsTCP());
    
    EXPECT_EQ(tcp_only.size(), 3u);
    EXPECT_TRUE(tcp_only.filter_all_match(IsTCP()));
}

TEST_F(RecordedStreamTest, AnyMatches) {
    auto stream = create_mixed_stream();
    
    EXPECT_TRUE(stream.filter_any_matches(IsUDP()));
    EXPECT_TRUE(stream.filter_any_matches(IsTCP()));
}

TEST_F(RecordedStreamTest, AllMatch) {
    auto udp_stream = create_test_stream(5);  // All UDP
    
    EXPECT_TRUE(udp_stream.filter_all_match(IsUDP()));
    EXPECT_FALSE(udp_stream.filter_all_match(IsTCP()));
}

TEST_F(RecordedStreamTest, NoneMatch) {
    auto udp_stream = create_test_stream(5);
    
    EXPECT_TRUE(udp_stream.none_match([](const Packet& pkt) {
        return ::testing::Value(pkt, IsTCP());
    }));
}

TEST_F(RecordedStreamTest, CountMatches) {
    auto stream = create_mixed_stream();
    
    EXPECT_EQ(stream.filter_count_matches(IsUDP()), 7u);
    EXPECT_EQ(stream.filter_count_matches(IsTCP()), 3u);
}

TEST_F(RecordedStreamTest, FindFirst) {
    auto stream = create_mixed_stream();
    
    auto first_tcp = stream.filter_find_first(IsTCP());
    ASSERT_TRUE(first_tcp.has_value());
    EXPECT_THAT(first_tcp->get(), IsTCP());
}

TEST_F(RecordedStreamTest, FindIndex) {
    auto stream = create_mixed_stream();
    
    auto tcp_idx = stream.find_index([](const Packet& pkt) {
        return ::testing::Value(pkt, IsTCP());
    });
    
    ASSERT_TRUE(tcp_idx.has_value());
    EXPECT_EQ(*tcp_idx, 5u);  // First TCP is at index 5 (after 5 UDP)
}

TEST_F(RecordedStreamTest, Take) {
    auto stream = create_test_stream(10);
    
    auto first_three = stream.take(3);
    EXPECT_EQ(first_three.size(), 3u);
    
    auto more_than_available = stream.take(100);
    EXPECT_EQ(more_than_available.size(), 10u);
}

TEST_F(RecordedStreamTest, Skip) {
    auto stream = create_test_stream(10);
    
    auto skip_five = stream.skip(5);
    EXPECT_EQ(skip_five.size(), 5u);
    
    auto skip_all = stream.skip(100);
    EXPECT_TRUE(skip_all.empty());
}

TEST_F(RecordedStreamTest, Slice) {
    auto stream = create_test_stream(10);
    
    auto middle = stream.slice(3, 7);
    EXPECT_EQ(middle.size(), 4u);
    
    auto empty_slice = stream.slice(5, 5);
    EXPECT_TRUE(empty_slice.empty());
}

TEST_F(RecordedStreamTest, TakeWhile) {
    auto stream = create_mixed_stream();  // 5 UDP, 3 TCP, 2 UDP
    
    auto first_udps = stream.take_while([](const Packet& pkt) {
        return ::testing::Value(pkt, IsUDP());
    });
    
    EXPECT_EQ(first_udps.size(), 5u);
}

TEST_F(RecordedStreamTest, SkipWhile) {
    auto stream = create_mixed_stream();
    
    auto after_first_udps = stream.skip_while([](const Packet& pkt) {
        return ::testing::Value(pkt, IsUDP());
    });
    
    EXPECT_EQ(after_first_udps.size(), 5u);  // 3 TCP + 2 UDP
    EXPECT_THAT(after_first_udps[0], IsTCP());
}

TEST_F(RecordedStreamTest, Concatenation) {
    auto stream1 = create_test_stream(3);
    auto stream2 = create_test_stream(2);
    
    auto combined = stream1 + stream2;
    EXPECT_EQ(combined.size(), 5u);
}

TEST_F(RecordedStreamTest, AddPacket) {
    RecordedStream stream;
    stream.add(gen_.udp_packet(10));
    stream.add(gen_.tcp_packet(10));
    
    EXPECT_EQ(stream.size(), 2u);
}

TEST_F(RecordedStreamTest, Clear) {
    auto stream = create_test_stream(5);
    EXPECT_EQ(stream.size(), 5u);
    
    stream.clear();
    EXPECT_TRUE(stream.empty());
}

// =============================================================================
// Sequence Tests
// =============================================================================

TEST_F(RecordedStreamTest, HasSequence) {
    auto stream = create_mixed_stream();  // 5 UDP, 3 TCP, 2 UDP
    
    // Should find UDP followed by TCP
    EXPECT_TRUE(stream.has_sequence(IsUDP(), IsTCP()));
    
    // Should find UDP, TCP, UDP sequence
    EXPECT_TRUE(stream.has_sequence(IsUDP(), IsTCP(), IsUDP()));
}

TEST_F(RecordedStreamTest, AllConsecutiveSatisfy) {
    auto stream = create_test_stream(5);  // All UDP
    
    // All consecutive pairs should both be UDP
    EXPECT_TRUE(stream.all_consecutive_satisfy([](const Packet& a, const Packet& b) {
        return ::testing::Value(a, IsUDP()) && ::testing::Value(b, IsUDP());
    }));
}

// =============================================================================
// Persistence Tests
// =============================================================================

TEST_F(RecordedStreamTest, SaveAndLoadPcap) {
    auto stream = create_test_stream(5);
    
    auto temp_path = std::filesystem::temp_directory_path() / "test_stream.pcap";
    
    // Save
    ASSERT_TRUE(stream.save_to_pcap(temp_path));
    EXPECT_TRUE(std::filesystem::exists(temp_path));
    
    // Load
    auto loaded = RecordedStream::load_from_pcap(temp_path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), 5u);
    
    // Cleanup
    std::filesystem::remove(temp_path);
}

TEST_F(RecordedStreamTest, LoadNonExistentFails) {
    auto loaded = RecordedStream::load_from_pcap("/nonexistent/path.pcap");
    EXPECT_FALSE(loaded.has_value());
}

// =============================================================================
// Macro Tests
// =============================================================================

TEST_F(RecordedStreamTest, StreamContainsMacro) {
    auto stream = create_mixed_stream();
    
    WADJET_EXPECT_STREAM_CONTAINS(stream, IsUDP());
    WADJET_EXPECT_STREAM_CONTAINS(stream, IsTCP());
}

TEST_F(RecordedStreamTest, AllMatchMacro) {
    auto udp_stream = create_test_stream(5);
    
    WADJET_EXPECT_ALL_MATCH(udp_stream, IsUDP());
}

TEST_F(RecordedStreamTest, CountMacro) {
    auto stream = create_mixed_stream();
    
    WADJET_EXPECT_COUNT(stream, IsTCP(), 3);
    WADJET_EXPECT_AT_LEAST(stream, IsUDP(), 5);
}

// =============================================================================
// RecordSession Tests (requires live capture)
// =============================================================================

class RecordSessionTest : public LoopbackTestFixture {
protected:
    void SetUp() override {
        set_filter("udp port 55570");
        LoopbackTestFixture::SetUp();
    }
    
    void send_test_packets(int count) {
        for (int i = 0; i < count; ++i) {
            std::vector<std::uint8_t> payload = {
                static_cast<std::uint8_t>(i), 
                0xAA, 0xBB
            };
            send_udp(55570, payload);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
};

TEST_F(RecordSessionTest, RecordForDuration) {
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(5);
    });
    
    ASSERT_TRUE(recorder.record_for(std::chrono::milliseconds(300)));
    sender.join();
    
    EXPECT_GE(recorder.stream().size(), 5u);
    EXPECT_TRUE(recorder.stream().filter_all_match(IsUDP()));
}

TEST_F(RecordSessionTest, RecordUntilPredicate) {
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(10);
    });
    
    // Record until we see 3rd packet (payload starts with 0x02)
    int packet_count = 0;
    bool found = recorder.record_until([&packet_count](const Packet&) {
        return ++packet_count >= 3;
    }, std::chrono::milliseconds(500));
    
    sender.join();
    
    EXPECT_TRUE(found);
    EXPECT_GE(recorder.stream().size(), 3u);
}

TEST_F(RecordSessionTest, RecordNMatching) {
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(10);
    });
    
    bool found = recorder.record_n_matching(5, 
        [](const Packet& pkt) { return ::testing::Value(pkt, IsUDP()); },
        std::chrono::milliseconds(500));
    
    sender.join();
    
    EXPECT_TRUE(found);
    EXPECT_GE(recorder.stream().filter_count_matches(IsUDP()), 5u);
}

TEST_F(RecordSessionTest, SaveRecordedStream) {
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(3);
    });
    
    recorder.record_for(std::chrono::milliseconds(200));
    sender.join();
    
    auto temp_path = std::filesystem::temp_directory_path() / "recorded_test.pcap";
    EXPECT_TRUE(recorder.save_to_pcap(temp_path));
    EXPECT_TRUE(std::filesystem::exists(temp_path));
    
    std::filesystem::remove(temp_path);
}

TEST_F(RecordSessionTest, GetStreamMovesOwnership) {
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(2);
    });
    
    recorder.record_for(std::chrono::milliseconds(150));
    sender.join();
    
    auto stream = recorder.get_stream();
    EXPECT_GE(stream.size(), 2u);
    
    // Original should be empty after move
    EXPECT_TRUE(recorder.stream().empty());
}

TEST_F(RecordSessionTest, ClearRecordedPackets) {
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(3);
    });
    
    recorder.record_for(std::chrono::milliseconds(200));
    sender.join();
    
    EXPECT_GE(recorder.stream().size(), 3u);
    
    recorder.clear();
    EXPECT_TRUE(recorder.stream().empty());
}

// =============================================================================
// Integration: Record then Assert
// =============================================================================

TEST_F(RecordSessionTest, RecordThenAssertWorkflow) {
    // Step 1: Record packets
    RecordSession recorder("lo");
    recorder.set_filter("udp port 55570");
    
    std::thread sender([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_test_packets(5);
    });
    
    recorder.record_for(std::chrono::milliseconds(250));
    sender.join();
    
    // Step 2: Get stream and run assertions
    auto stream = recorder.get_stream();
    
    WADJET_ASSERT_STREAM_CONTAINS(stream, IsUDP());
    WADJET_ASSERT_ALL_MATCH(stream, IsUDP());
    WADJET_ASSERT_AT_LEAST(stream, IsUDP(), 5);
    
    // Step 3: Filter and inspect
    auto udp_packets = stream.filter_matches(IsUDP());
    EXPECT_GE(udp_packets.size(), 5u);
    
    // Step 4: Verify sequence properties
    EXPECT_TRUE(stream.all_consecutive_satisfy([](const Packet& a, const Packet& b) {
        // Both should be UDP
        return ::testing::Value(a, IsUDP()) && ::testing::Value(b, IsUDP());
    }));
}

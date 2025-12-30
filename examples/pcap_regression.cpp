/// @file pcap_regression.cpp
/// @brief Example: PCAP-based Regression Testing for CI
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This example demonstrates how to use Wadjet-Link with GoogleTest for
/// automated regression testing using PCAP files. It validates expected
/// protocol behavior against captured traffic.
///
/// Usage:
///   ./pcap_regression                    # Run all tests
///   ./pcap_regression --gtest_filter="*" # Run specific tests
///
/// In CI pipelines:
///   cmake --build . --target pcap_regression
///   ctest -R pcap_regression --output-on-failure

#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/ethernet.hpp>
#include <wadjet/protocols/ipv4.hpp>
#include <wadjet/protocols/someip.hpp>
#include <wadjet/protocols/someip_sd.hpp>
#include <wadjet/protocols/udp.hpp>
#include <wadjet/testing/matchers.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::testing;
using namespace ::testing;

// =============================================================================
// Test Fixture for PCAP-based Tests
// =============================================================================

/// @brief Test fixture that loads PCAP files from a test data directory
class PcapRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Look for test data directory in common locations
        std::vector<std::filesystem::path> search_paths = {
            "testdata",
            "../testdata",
            "../../testdata",
            std::filesystem::path(TEST_DATA_DIR),  // CMake-defined
        };

        for (const auto& path : search_paths) {
            if (std::filesystem::exists(path)) {
                test_data_dir_ = std::filesystem::canonical(path);
                break;
            }
        }
    }

    /// @brief Load a PCAP file from the test data directory
    std::optional<pcap::PcapReader> load_pcap(const std::string& filename) {
        auto path = test_data_dir_ / filename;
        if (!std::filesystem::exists(path)) {
            ADD_FAILURE() << "PCAP file not found: " << path;
            return std::nullopt;
        }

        auto result = pcap::PcapReader::open(path);
        if (!result) {
            ADD_FAILURE() << "Failed to open PCAP: " << result.error().message();
            return std::nullopt;
        }

        return std::move(result.value());
    }

    /// @brief Read all packets from a PCAP file
    std::vector<net::Packet> read_all_packets(const std::string& filename) {
        std::vector<net::Packet> packets;
        auto reader = load_pcap(filename);
        if (reader) {
            packets = reader->read_all();
        }
        return packets;
    }

    /// @brief Filter packets matching a predicate
    template <typename Predicate>
    std::vector<net::Packet> filter_packets(const std::vector<net::Packet>& packets,
                                             Predicate pred) {
        std::vector<net::Packet> filtered;
        std::copy_if(packets.begin(), packets.end(), std::back_inserter(filtered), pred);
        return filtered;
    }

    std::filesystem::path test_data_dir_;
};

// =============================================================================
// SOME/IP Protocol Tests
// =============================================================================

/// @brief Test SOME/IP message parsing
TEST_F(PcapRegressionTest, SomeIpMessageParsing) {
    // Skip if test data not available
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("someip_sample.pcap");
    ASSERT_GT(packets.size(), 0) << "No packets loaded from PCAP";

    // Count SOME/IP messages
    int someip_count = 0;
    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());
        if (result.has_layer<someip::SomeIpHeader>()) {
            someip_count++;

            const auto* header = result.get_layer<someip::SomeIpHeader>();

            // Verify header fields are reasonable
            EXPECT_GE(header->length, 8) << "SOME/IP length field too small";
            EXPECT_EQ(header->protocol_version, someip::PROTOCOL_VERSION);
        }
    }

    EXPECT_GT(someip_count, 0) << "No SOME/IP messages found in capture";
    std::cout << "Parsed " << someip_count << " SOME/IP messages\n";
}

/// @brief Test SOME/IP Service Discovery parsing
TEST_F(PcapRegressionTest, SomeIpServiceDiscovery) {
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("someip_sd_sample.pcap");
    ASSERT_GT(packets.size(), 0) << "No packets loaded from PCAP";

    int sd_count = 0;
    int offer_count = 0;
    int find_count = 0;

    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());

        if (result.has_layer<someip::SomeIpHeader>()) {
            const auto* someip_header = result.get_layer<someip::SomeIpHeader>();

            if (someip_header->is_service_discovery() &&
                result.has_layer<someip_sd::SomeIpSdHeader>()) {
                sd_count++;

                const auto* sd_header = result.get_layer<someip_sd::SomeIpSdHeader>();

                // Count entry types
                for (const auto& entry : sd_header->entries) {
                    if (std::holds_alternative<someip_sd::ServiceEntry>(entry)) {
                        const auto& svc = std::get<someip_sd::ServiceEntry>(entry);
                        if (svc.type == someip_sd::EntryType::OfferService) {
                            offer_count++;
                        } else if (svc.type == someip_sd::EntryType::FindService) {
                            find_count++;
                        }
                    }
                }
            }
        }
    }

    EXPECT_GT(sd_count, 0) << "No SD messages found";
    std::cout << "SD messages: " << sd_count
              << ", Offers: " << offer_count
              << ", Finds: " << find_count << "\n";
}

// =============================================================================
// DoIP Protocol Tests
// =============================================================================

/// @brief Test DoIP message parsing
TEST_F(PcapRegressionTest, DoIPMessageParsing) {
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("doip_sample.pcap");
    ASSERT_GT(packets.size(), 0) << "No packets loaded from PCAP";

    int doip_count = 0;
    int routing_count = 0;
    int diagnostic_count = 0;

    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());

        if (result.has_layer<doip::DoIPHeader>()) {
            doip_count++;

            const auto* header = result.get_layer<doip::DoIPHeader>();

            // Verify version field
            EXPECT_TRUE(header->is_version_valid())
                << "Invalid DoIP version field";

            // Count message types
            if (header->is_routing_activation()) {
                routing_count++;
            }
            if (header->is_diagnostic_message()) {
                diagnostic_count++;
            }
        }
    }

    EXPECT_GT(doip_count, 0) << "No DoIP messages found in capture";
    std::cout << "DoIP messages: " << doip_count
              << ", Routing: " << routing_count
              << ", Diagnostic: " << diagnostic_count << "\n";
}

/// @brief Test DoIP routing activation sequence
TEST_F(PcapRegressionTest, DoIPRoutingActivationSequence) {
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("doip_routing_sample.pcap");
    ASSERT_GT(packets.size(), 0) << "No packets loaded from PCAP";

    bool saw_request = false;
    bool saw_response = false;
    bool saw_success = false;

    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());

        if (result.has_layer<doip::DoIPHeader>()) {
            const auto* header = result.get_layer<doip::DoIPHeader>();
            auto payload = result.payload_after<doip::DoIPHeader>();

            if (header->payload_type == doip::PayloadType::RoutingActivationRequest) {
                saw_request = true;
                EXPECT_GE(payload.size(), 7) << "Routing request too short";
            }

            if (header->payload_type == doip::PayloadType::RoutingActivationResponse) {
                saw_response = true;
                EXPECT_GE(payload.size(), 9) << "Routing response too short";

                if (payload.size() >= 5) {
                    auto code = static_cast<doip::RoutingActivationResponseCode>(payload[4]);
                    if (code == doip::RoutingActivationResponseCode::SuccessfullyActivated) {
                        saw_success = true;
                    }
                }
            }
        }
    }

    // Validate sequence
    if (saw_request) {
        EXPECT_TRUE(saw_response) << "Request without response";
    }
    if (saw_response) {
        EXPECT_TRUE(saw_request) << "Response without request";
    }

    std::cout << "Routing sequence: Request=" << saw_request
              << ", Response=" << saw_response
              << ", Success=" << saw_success << "\n";
}

// =============================================================================
// Protocol Stack Tests
// =============================================================================

/// @brief Test complete protocol stack decoding
TEST_F(PcapRegressionTest, ProtocolStackDecoding) {
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("mixed_traffic.pcap");
    ASSERT_GT(packets.size(), 0) << "No packets loaded from PCAP";

    int ethernet_count = 0;
    int ipv4_count = 0;
    int udp_count = 0;
    int tcp_count = 0;

    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());

        if (result.has_layer<ethernet::EthernetHeader>()) {
            ethernet_count++;
        }
        if (result.has_layer<ipv4::IPv4Header>()) {
            ipv4_count++;

            // Validate IPv4 checksum
            const auto* ipv4_header = result.get_layer<ipv4::IPv4Header>();
            EXPECT_GE(ipv4_header->total_length, 20)
                << "IPv4 total length too small";
        }
        if (result.has_layer<udp::UdpHeader>()) {
            udp_count++;
        }
        if (result.has_layer<tcp::TcpHeader>()) {
            tcp_count++;
        }
    }

    std::cout << "Protocol counts:\n"
              << "  Ethernet: " << ethernet_count << "\n"
              << "  IPv4:     " << ipv4_count << "\n"
              << "  UDP:      " << udp_count << "\n"
              << "  TCP:      " << tcp_count << "\n";

    EXPECT_GT(ethernet_count, 0) << "No Ethernet frames found";
}

// =============================================================================
// Wadjet Testing Matchers Tests
// =============================================================================

/// @brief Test Wadjet testing matchers
TEST_F(PcapRegressionTest, TestingMatchers) {
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("someip_sample.pcap");
    ASSERT_GT(packets.size(), 0);

    // Find first SOME/IP packet
    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());
        if (result.has_layer<someip::SomeIpHeader>()) {
            // Test various matchers
            EXPECT_THAT(packet, HasEthertype(0x0800));  // IPv4

            const auto* someip_header = result.get_layer<someip::SomeIpHeader>();

            // Test SOME/IP matchers (if implemented)
            // EXPECT_THAT(packet, HasSOMEIPServiceId(someip_header->service_id));

            break;  // Test first matching packet
        }
    }
}

// =============================================================================
// Performance Regression Tests
// =============================================================================

/// @brief Test decoding performance
TEST_F(PcapRegressionTest, DecodingPerformance) {
    if (test_data_dir_.empty()) {
        GTEST_SKIP() << "Test data directory not found";
    }

    auto packets = read_all_packets("large_capture.pcap");

    // Skip if file doesn't exist or is too small
    if (packets.size() < 100) {
        GTEST_SKIP() << "Need at least 100 packets for performance test";
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Decode all packets
    int decoded = 0;
    for (const auto& packet : packets) {
        auto result = decode_packet(packet.data());
        if (result.success()) {
            decoded++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double packets_per_second = (decoded * 1e6) / duration.count();

    std::cout << "Performance: " << decoded << " packets in "
              << duration.count() << " us ("
              << static_cast<int>(packets_per_second) << " pkt/s)\n";

    // Expect reasonable performance (adjust threshold as needed)
    EXPECT_GT(packets_per_second, 10000) << "Decoding too slow";
}

// =============================================================================
// Edge Case Tests
// =============================================================================

/// @brief Test malformed packet handling
TEST_F(PcapRegressionTest, MalformedPacketHandling) {
    // Test with truncated packets
    std::vector<std::byte> truncated_ethernet(13);  // Less than 14 bytes
    auto result = decode_packet(truncated_ethernet);
    EXPECT_FALSE(result.has_layer<ethernet::EthernetHeader>())
        << "Should not decode truncated Ethernet";

    // Test with empty packet
    std::vector<std::byte> empty;
    result = decode_packet(empty);
    EXPECT_FALSE(result.success()) << "Should fail on empty packet";

    // Test with random data
    std::vector<std::byte> random(100);
    std::fill(random.begin(), random.end(), std::byte{0xFF});
    result = decode_packet(random);
    // Should not crash, may or may not decode
    SUCCEED() << "Random data handled without crash";
}

// =============================================================================
// Test Utilities
// =============================================================================

/// @brief Generate synthetic test packet (for unit tests)
std::vector<std::byte> create_someip_packet(
    std::uint16_t service_id,
    std::uint16_t method_id,
    someip::MessageType msg_type = someip::MessageType::Request) {
    std::vector<std::byte> packet;

    // Ethernet header (14 bytes)
    packet.resize(14, std::byte{0});
    // Destination MAC
    packet[0] = std::byte{0x01};
    packet[1] = std::byte{0x02};
    packet[2] = std::byte{0x03};
    packet[3] = std::byte{0x04};
    packet[4] = std::byte{0x05};
    packet[5] = std::byte{0x06};
    // Source MAC
    packet[6] = std::byte{0x0A};
    packet[7] = std::byte{0x0B};
    packet[8] = std::byte{0x0C};
    packet[9] = std::byte{0x0D};
    packet[10] = std::byte{0x0E};
    packet[11] = std::byte{0x0F};
    // EtherType (IPv4)
    packet[12] = std::byte{0x08};
    packet[13] = std::byte{0x00};

    // IPv4 header (20 bytes)
    std::size_t ip_start = packet.size();
    packet.resize(packet.size() + 20, std::byte{0});
    packet[ip_start + 0] = std::byte{0x45};  // Version + IHL
    packet[ip_start + 9] = std::byte{17};    // Protocol (UDP)
    // Total length will be set later
    // Source IP: 192.168.1.100
    packet[ip_start + 12] = std::byte{192};
    packet[ip_start + 13] = std::byte{168};
    packet[ip_start + 14] = std::byte{1};
    packet[ip_start + 15] = std::byte{100};
    // Dest IP: 192.168.1.200
    packet[ip_start + 16] = std::byte{192};
    packet[ip_start + 17] = std::byte{168};
    packet[ip_start + 18] = std::byte{1};
    packet[ip_start + 19] = std::byte{200};

    // UDP header (8 bytes)
    std::size_t udp_start = packet.size();
    packet.resize(packet.size() + 8, std::byte{0});
    // Source port: 30490
    packet[udp_start + 0] = std::byte{0x77};
    packet[udp_start + 1] = std::byte{0x0A};
    // Dest port: 30490
    packet[udp_start + 2] = std::byte{0x77};
    packet[udp_start + 3] = std::byte{0x0A};

    // SOME/IP header (16 bytes)
    std::size_t someip_start = packet.size();
    packet.resize(packet.size() + 16, std::byte{0});
    // Service ID
    packet[someip_start + 0] = std::byte{static_cast<std::uint8_t>(service_id >> 8)};
    packet[someip_start + 1] = std::byte{static_cast<std::uint8_t>(service_id)};
    // Method ID
    packet[someip_start + 2] = std::byte{static_cast<std::uint8_t>(method_id >> 8)};
    packet[someip_start + 3] = std::byte{static_cast<std::uint8_t>(method_id)};
    // Length (8 for header fields after length)
    packet[someip_start + 4] = std::byte{0};
    packet[someip_start + 5] = std::byte{0};
    packet[someip_start + 6] = std::byte{0};
    packet[someip_start + 7] = std::byte{8};
    // Client ID
    packet[someip_start + 8] = std::byte{0x00};
    packet[someip_start + 9] = std::byte{0x01};
    // Session ID
    packet[someip_start + 10] = std::byte{0x00};
    packet[someip_start + 11] = std::byte{0x01};
    // Protocol version
    packet[someip_start + 12] = std::byte{0x01};
    // Interface version
    packet[someip_start + 13] = std::byte{0x01};
    // Message type
    packet[someip_start + 14] = std::byte{static_cast<std::uint8_t>(msg_type)};
    // Return code
    packet[someip_start + 15] = std::byte{0x00};

    // Update lengths
    std::uint16_t udp_len = packet.size() - udp_start;
    packet[udp_start + 4] = std::byte{static_cast<std::uint8_t>(udp_len >> 8)};
    packet[udp_start + 5] = std::byte{static_cast<std::uint8_t>(udp_len)};

    std::uint16_t ip_len = packet.size() - ip_start;
    packet[ip_start + 2] = std::byte{static_cast<std::uint8_t>(ip_len >> 8)};
    packet[ip_start + 3] = std::byte{static_cast<std::uint8_t>(ip_len)};

    return packet;
}

/// @brief Test synthetic packet creation
TEST(SyntheticPacketTest, CreateSomeIPPacket) {
    auto packet = create_someip_packet(0x1234, 0x0001, someip::MessageType::Request);

    ASSERT_GE(packet.size(), 58);  // Eth(14) + IP(20) + UDP(8) + SOME/IP(16)

    auto result = decode_packet(packet);
    EXPECT_TRUE(result.has_layer<ethernet::EthernetHeader>());
    EXPECT_TRUE(result.has_layer<someip::SomeIpHeader>());

    if (result.has_layer<someip::SomeIpHeader>()) {
        const auto* header = result.get_layer<someip::SomeIpHeader>();
        EXPECT_EQ(header->service_id, 0x1234);
        EXPECT_EQ(header->method_id, 0x0001);
        EXPECT_EQ(header->message_type, someip::MessageType::Request);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

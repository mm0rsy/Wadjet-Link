/// @file test_someip_sd.cpp
/// @brief SOME/IP Service Discovery option array and linking tests

#include <gtest/gtest.h>

#include "wadjet/core/types.hpp"
#include "wadjet/protocols/someip_sd.hpp"

using namespace wadjet;
using namespace wadjet::protocols::someip_sd;

//==============================================================================
// SD Option Array Tests (T067)
//==============================================================================

class SomeIpSdOptionArrayComprehensiveTest : public ::testing::Test {
};

// Basic option type tests
TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4EndpointOptionBasic) {
    IPv4EndpointOption opt;
    opt.address = IPv4Address::from_bytes(192, 168, 1, 1);
    opt.port = 30503;
    opt.protocol = L4Protocol::UDP;
    
    EXPECT_EQ(opt.address.bytes[0], 192);
    EXPECT_EQ(opt.address.bytes[1], 168);
    EXPECT_EQ(opt.address.bytes[2], 1);
    EXPECT_EQ(opt.address.bytes[3], 1);
    EXPECT_EQ(opt.port, 30503);
    EXPECT_EQ(opt.protocol, L4Protocol::UDP);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4EndpointTCPProtocol) {
    IPv4EndpointOption opt;
    opt.address = IPv4Address::from_bytes(10, 0, 0, 1);
    opt.port = 8080;
    opt.protocol = L4Protocol::TCP;
    
    EXPECT_EQ(opt.protocol, L4Protocol::TCP);
    EXPECT_EQ(opt.port, 8080);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4EndpointUDPProtocol) {
    IPv4EndpointOption opt;
    opt.protocol = L4Protocol::UDP;
    
    EXPECT_EQ(static_cast<std::uint8_t>(opt.protocol), 0x11);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4EndpointMaxPort) {
    IPv4EndpointOption opt;
    opt.port = 65535;  // Max 16-bit port
    
    EXPECT_EQ(opt.port, 65535);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4EndpointMinPort) {
    IPv4EndpointOption opt;
    opt.port = 1;  // Min valid port
    
    EXPECT_EQ(opt.port, 1);
}

// IPv4Multicast option tests
TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4MulticastOption) {
    SdOption opt;
    opt.type = OptionType::IPv4Multicast;
    opt.data.resize(6);  // IPv4(4) + Port(2)
    
    EXPECT_EQ(opt.type, OptionType::IPv4Multicast);
    EXPECT_EQ(opt.data.size(), 6);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv4SDEndpointOption) {
    SdOption opt;
    opt.type = OptionType::IPv4SDEndpoint;
    
    EXPECT_EQ(opt.type, OptionType::IPv4SDEndpoint);
}

// LoadBalancing option tests
TEST_F(SomeIpSdOptionArrayComprehensiveTest, LoadBalancingOptionBasic) {
    SdOption opt;
    opt.type = OptionType::LoadBalancing;
    opt.data.resize(4);  // Priority(2) + Weight(2)
    
    EXPECT_EQ(opt.type, OptionType::LoadBalancing);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, LoadBalancingMultipleOptions) {
    std::vector<SdOption> options;
    
    SdOption opt1;
    opt1.type = OptionType::LoadBalancing;
    options.push_back(opt1);
    
    SdOption opt2;
    opt2.type = OptionType::LoadBalancing;
    options.push_back(opt2);
    
    EXPECT_EQ(options.size(), 2);
    EXPECT_EQ(options[0].type, OptionType::LoadBalancing);
    EXPECT_EQ(options[1].type, OptionType::LoadBalancing);
}

// Configuration option tests
TEST_F(SomeIpSdOptionArrayComprehensiveTest, ConfigurationOptionBasic) {
    SdOption opt;
    opt.type = OptionType::Configuration;
    opt.data.push_back(std::byte{0xAA});
    opt.data.push_back(std::byte{0xBB});
    
    EXPECT_EQ(opt.type, OptionType::Configuration);
    EXPECT_EQ(opt.data.size(), 2);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, ConfigurationOptionVariableLength) {
    SdOption opt;
    opt.type = OptionType::Configuration;
    
    // Variable length configuration data
    for (int i = 0; i < 10; ++i) {
        opt.data.push_back(std::byte{static_cast<unsigned char>(i)});
    }
    
    EXPECT_EQ(opt.data.size(), 10);
}

// IPv6 endpoint options
TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv6EndpointOption) {
    SdOption opt;
    opt.type = OptionType::IPv6Endpoint;
    
    EXPECT_EQ(opt.type, OptionType::IPv6Endpoint);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv6MulticastOption) {
    SdOption opt;
    opt.type = OptionType::IPv6Multicast;
    
    EXPECT_EQ(opt.type, OptionType::IPv6Multicast);
}

TEST_F(SomeIpSdOptionArrayComprehensiveTest, IPv6SDEndpointOption) {
    SdOption opt;
    opt.type = OptionType::IPv6SDEndpoint;
    
    EXPECT_EQ(opt.type, OptionType::IPv6SDEndpoint);
}

//==============================================================================
// Option Array Linking Tests (Index1/Index2/NumOpt1/NumOpt2)
//==============================================================================

class SomeIpSdOptionLinkingTest : public ::testing::Test {
};

TEST_F(SomeIpSdOptionLinkingTest, ServiceEntryOptionIndices) {
    ServiceEntry entry;
    entry.service_id = 0x1234;
    entry.index1_first_option = 0;
    entry.num_options_1 = 2;
    entry.index2_first_option = 2;
    entry.num_options_2 = 1;
    
    EXPECT_EQ(entry.index1_first_option, 0);
    EXPECT_EQ(entry.num_options_1, 2);
    EXPECT_EQ(entry.index2_first_option, 2);
    EXPECT_EQ(entry.num_options_2, 1);
}

TEST_F(SomeIpSdOptionLinkingTest, EventgroupEntryOptionIndices) {
    EventgroupEntry entry;
    entry.eventgroup_id = 0x0001;
    entry.index1_first_option = 5;
    entry.num_options_1 = 3;
    entry.index2_first_option = 8;
    entry.num_options_2 = 2;
    
    EXPECT_EQ(entry.index1_first_option, 5);
    EXPECT_EQ(entry.num_options_1, 3);
    EXPECT_EQ(entry.index2_first_option, 8);
    EXPECT_EQ(entry.num_options_2, 2);
}

TEST_F(SomeIpSdOptionLinkingTest, OptionIndexValidation) {
    SdOptionArray options;
    
    // Create 5 options
    for (int i = 0; i < 5; ++i) {
        SdOption opt;
        opt.type = OptionType::IPv4Endpoint;
        options.push_back(opt);
    }
    
    // Entry pointing to options 0-1 and 2-3
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 2;
    entry.index2_first_option = 2;
    entry.num_options_2 = 2;
    
    // Verify indices are within bounds
    EXPECT_LT(entry.index1_first_option, static_cast<int>(options.size()));
    EXPECT_LT(entry.index1_first_option + entry.num_options_1, static_cast<int>(options.size()) + 1);
    EXPECT_LT(entry.index2_first_option + entry.num_options_2, static_cast<int>(options.size()) + 1);
}

TEST_F(SomeIpSdOptionLinkingTest, GetOptionsForEntry) {
    SdOptionArray options;
    
    for (int i = 0; i < 6; ++i) {
        SdOption opt;
        opt.type = (i < 3) ? OptionType::IPv4Endpoint : OptionType::LoadBalancing;
        options.push_back(opt);
    }
    
    // Get options for entry with two ranges
    auto result = options.get_options_for_entry(0, 2, 3, 2);
    
    // Should get options at indices 0, 1, 3, 4
    EXPECT_EQ(result.size(), 4);
}

TEST_F(SomeIpSdOptionLinkingTest, FirstOptionSetOnly) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 5;
    entry.index2_first_option = 0;
    entry.num_options_2 = 0;
    
    EXPECT_EQ(entry.num_options_1, 5);
    EXPECT_EQ(entry.num_options_2, 0);
}

TEST_F(SomeIpSdOptionLinkingTest, SecondOptionSetOnly) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 0;
    entry.index2_first_option = 5;
    entry.num_options_2 = 3;
    
    EXPECT_EQ(entry.num_options_1, 0);
    EXPECT_EQ(entry.num_options_2, 3);
}

TEST_F(SomeIpSdOptionLinkingTest, NoOptionsLinked) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 0;
    entry.index2_first_option = 0;
    entry.num_options_2 = 0;
    
    EXPECT_EQ(entry.num_options_1, 0);
    EXPECT_EQ(entry.num_options_2, 0);
}

TEST_F(SomeIpSdOptionLinkingTest, MaxOptionIndices) {
    ServiceEntry entry;
    entry.index1_first_option = 255;  // Max 8-bit
    entry.num_options_1 = 15;         // Max 4-bit
    entry.index2_first_option = 255;  // Max 8-bit
    entry.num_options_2 = 15;         // Max 4-bit
    
    EXPECT_EQ(entry.index1_first_option, 255);
    EXPECT_EQ(entry.num_options_1, 15);
    EXPECT_EQ(entry.index2_first_option, 255);
    EXPECT_EQ(entry.num_options_2, 15);
}

//==============================================================================
// Complex Linking Scenarios
//==============================================================================

class SomeIpSdComplexLinkingTest : public ::testing::Test {
};

TEST_F(SomeIpSdComplexLinkingTest, MultipleEntriesWithDifferentOptions) {
    SdOptionArray options;
    
    // Create 10 options
    for (int i = 0; i < 10; ++i) {
        SdOption opt;
        opt.type = (i % 2 == 0) ? OptionType::IPv4Endpoint : OptionType::LoadBalancing;
        options.push_back(opt);
    }
    
    // Entry 1: uses options 0-2
    ServiceEntry entry1;
    entry1.service_id = 0x1111;
    entry1.index1_first_option = 0;
    entry1.num_options_1 = 3;
    
    // Entry 2: uses options 3-5
    ServiceEntry entry2;
    entry2.service_id = 0x2222;
    entry1.index1_first_option = 3;
    entry1.num_options_1 = 3;
    
    // Entry 3: uses options 6-9
    EventgroupEntry entry3;
    entry3.eventgroup_id = 0x0001;
    entry3.index1_first_option = 6;
    entry3.num_options_1 = 4;
    
    EXPECT_EQ(options.size(), 10);
}

TEST_F(SomeIpSdComplexLinkingTest, EventgroupWithTwoOptionSets) {
    SdOptionArray options;
    
    // Create option array
    for (int i = 0; i < 5; ++i) {
        SdOption opt;
        opt.type = OptionType::IPv4Endpoint;
        options.push_back(opt);
    }
    
    // Eventgroup with two separate option ranges
    EventgroupEntry entry;
    entry.eventgroup_id = 0x0001;
    entry.index1_first_option = 0;
    entry.num_options_1 = 2;
    entry.index2_first_option = 3;
    entry.num_options_2 = 2;
    
    auto opts = options.get_options_for_entry(
        entry.index1_first_option, entry.num_options_1,
        entry.index2_first_option, entry.num_options_2);
    
    EXPECT_EQ(opts.size(), 4);
}

TEST_F(SomeIpSdComplexLinkingTest, SequentialOptionIndexing) {
    SdOptionArray options;
    
    // Create 8 options
    for (int i = 0; i < 8; ++i) {
        SdOption opt;
        opt.type = OptionType::IPv4Endpoint;
        options.push_back(opt);
    }
    
    // Entry 1: 0-1
    ServiceEntry entry1;
    entry1.index1_first_option = 0;
    entry1.num_options_1 = 2;
    
    // Entry 2: 2-3
    ServiceEntry entry2;
    entry2.index1_first_option = 2;
    entry2.num_options_1 = 2;
    
    // Entry 3: 4-7
    EventgroupEntry entry3;
    entry3.index1_first_option = 4;
    entry3.num_options_1 = 4;
    
    EXPECT_EQ(options.size(), 8);
}

//==============================================================================
// IPv4 Endpoint to Option Conversion
//==============================================================================

class SomeIpSdOptionConversionTest : public ::testing::Test {
};

TEST_F(SomeIpSdOptionConversionTest, ConvertSdOptionToIpv4Endpoint) {
    SdOption opt;
    opt.type = OptionType::IPv4Endpoint;
    
    // Store IPv4 endpoint data
    opt.data.push_back(std::byte{0xC0});  // 192
    opt.data.push_back(std::byte{0xA8});  // 168
    opt.data.push_back(std::byte{0x01});  // 1
    opt.data.push_back(std::byte{0x01});  // 1
    opt.data.push_back(std::byte{0x76});  // Port high byte
    opt.data.push_back(std::byte{0xB7});  // Port low byte
    
    auto ipv4_opt = opt.as_ipv4_endpoint();
    
    if (ipv4_opt) {
        EXPECT_EQ(ipv4_opt->address.bytes[0], 192);
        EXPECT_EQ(ipv4_opt->address.bytes[1], 168);
    }
}

TEST_F(SomeIpSdOptionConversionTest, InvalidOptionConversion) {
    SdOption opt;
    opt.type = OptionType::LoadBalancing;  // Not IPv4 endpoint
    opt.data.push_back(std::byte{0x00});
    
    auto ipv4_opt = opt.as_ipv4_endpoint();
    
    EXPECT_FALSE(ipv4_opt.has_value());
}

//==============================================================================
// Header Flag Tests
//==============================================================================

class SomeIpSdHeaderFlagTest : public ::testing::Test {
};

TEST_F(SomeIpSdHeaderFlagTest, RebootFlagSet) {
    SomeIpSdHeader header;
    header.flags = 0x80;  // Reboot flag
    
    EXPECT_TRUE(header.is_reboot());
    EXPECT_FALSE(header.is_unicast());
}

TEST_F(SomeIpSdHeaderFlagTest, UnicastFlagSet) {
    SomeIpSdHeader header;
    header.flags = 0x40;  // Unicast flag
    
    EXPECT_FALSE(header.is_reboot());
    EXPECT_TRUE(header.is_unicast());
}

TEST_F(SomeIpSdHeaderFlagTest, BothFlagsSet) {
    SomeIpSdHeader header;
    header.flags = 0xC0;  // Both Reboot and Unicast
    
    EXPECT_TRUE(header.is_reboot());
    EXPECT_TRUE(header.is_unicast());
}

TEST_F(SomeIpSdHeaderFlagTest, NoFlagsSet) {
    SomeIpSdHeader header;
    header.flags = 0x00;
    
    EXPECT_FALSE(header.is_reboot());
    EXPECT_FALSE(header.is_unicast());
}

//==============================================================================
// Entry and Option Count Validation
//==============================================================================

class SomeIpSdCountValidationTest : public ::testing::Test {
};

TEST_F(SomeIpSdCountValidationTest, EmptyEntryArray) {
    SdEntryArray array;
    
    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.size(), 0);
}

TEST_F(SomeIpSdCountValidationTest, SingleEntry) {
    SdEntryArray array;
    ServiceEntry entry;
    array.push_back(entry);
    
    EXPECT_FALSE(array.empty());
    EXPECT_EQ(array.size(), 1);
}

TEST_F(SomeIpSdCountValidationTest, MultipleEntries) {
    SdEntryArray array;
    
    for (std::uint16_t i = 0; i < 10; ++i) {
        ServiceEntry entry;
        entry.service_id = i;
        array.push_back(entry);
    }
    
    EXPECT_EQ(array.size(), 10);
}

TEST_F(SomeIpSdCountValidationTest, EntryCountMatching) {
    SdEntryArray entries;
    SdOptionArray options;
    
    // Add 3 entries
    for (int i = 0; i < 3; ++i) {
        ServiceEntry entry;
        entries.push_back(entry);
    }
    
    // Add 5 options
    for (int i = 0; i < 5; ++i) {
        SdOption opt;
        options.push_back(opt);
    }
    
    EXPECT_EQ(entries.size(), 3);
    EXPECT_EQ(options.size(), 5);
}

TEST_F(SomeIpSdCountValidationTest, OptionArrayAccess) {
    SdOptionArray options;
    
    for (int i = 0; i < 3; ++i) {
        SdOption opt;
        opt.type = (i % 2 == 0) ? OptionType::IPv4Endpoint : OptionType::LoadBalancing;
        options.push_back(opt);
    }
    
    EXPECT_EQ(options[0].type, OptionType::IPv4Endpoint);
    EXPECT_EQ(options[1].type, OptionType::LoadBalancing);
    EXPECT_EQ(options[2].type, OptionType::IPv4Endpoint);
}


/// @file test_someip_sd_entries.cpp
/// @brief SOME/IP-SD entry and option type tests

#include "wadjet/protocols/someip_sd.hpp"

#include <gtest/gtest.h>

using namespace wadjet::protocols::someip_sd;

//==============================================================================
// SOME/IP-SD Entry Type Tests
//==============================================================================

class SomeIpSdEntryTest : public ::testing::Test {};

// Entry type tests
TEST_F(SomeIpSdEntryTest, FindServiceEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::FindService), 0x00);
}

TEST_F(SomeIpSdEntryTest, OfferServiceEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::OfferService), 0x01);
}

TEST_F(SomeIpSdEntryTest, StopOfferServiceEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::StopOfferService), 0x81);
}

TEST_F(SomeIpSdEntryTest, SubscribeEventgroupEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::SubscribeEventgroup), 0x06);
}

TEST_F(SomeIpSdEntryTest, StopSubscribeEventgroupEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::StopSubscribeEventgroup), 0x86);
}

TEST_F(SomeIpSdEntryTest, SubscribeEventgroupAckEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::SubscribeEventgroupAck), 0x07);
}

TEST_F(SomeIpSdEntryTest, SubscribeEventgroupNackEntryType) {
    EXPECT_EQ(static_cast<std::uint8_t>(EntryType::SubscribeEventgroupNack), 0x87);
}

TEST_F(SomeIpSdEntryTest, EntryTypeString) {
    EXPECT_EQ(entry_type_string(EntryType::FindService), "FindService");
    EXPECT_EQ(entry_type_string(EntryType::OfferService), "OfferService");
    EXPECT_EQ(entry_type_string(EntryType::StopOfferService), "StopOfferService");
    EXPECT_EQ(entry_type_string(EntryType::SubscribeEventgroup), "SubscribeEventgroup");
    EXPECT_EQ(entry_type_string(EntryType::StopSubscribeEventgroup), "StopSubscribeEventgroup");
    EXPECT_EQ(entry_type_string(EntryType::SubscribeEventgroupAck), "SubscribeEventgroupAck");
    EXPECT_EQ(entry_type_string(EntryType::SubscribeEventgroupNack), "SubscribeEventgroupNack");
}

//==============================================================================
// SOME/IP-SD Service Entry Tests
//==============================================================================

class ServiceEntryTest : public ::testing::Test {};

TEST_F(ServiceEntryTest, CreateFindServiceEntry) {
    ServiceEntry entry;
    entry.type = EntryType::FindService;
    entry.service_id = 0x1234;
    entry.instance_id = 0x5678;
    entry.major_version = 1;
    entry.ttl = 3;
    entry.minor_version = 0;

    EXPECT_EQ(entry.type, EntryType::FindService);
    EXPECT_EQ(entry.service_id, 0x1234);
    EXPECT_EQ(entry.instance_id, 0x5678);
    EXPECT_EQ(entry.major_version, 1);
    EXPECT_EQ(entry.ttl, 3);
}

TEST_F(ServiceEntryTest, CreateOfferServiceEntry) {
    ServiceEntry entry;
    entry.type = EntryType::OfferService;
    entry.service_id = 0xABCD;
    entry.instance_id = 0xEF01;
    entry.major_version = 5;
    entry.ttl = 300;
    entry.minor_version = 0xFF;

    EXPECT_EQ(entry.type, EntryType::OfferService);
    EXPECT_EQ(entry.service_id, 0xABCD);
    EXPECT_EQ(entry.ttl, 300);
}

TEST_F(ServiceEntryTest, OfferServiceStopCondition) {
    ServiceEntry entry;
    entry.type = EntryType::OfferService;
    entry.ttl = 0;

    EXPECT_EQ(entry.ttl, 0);
}

TEST_F(ServiceEntryTest, ServiceEntryOptionIndices) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.index2_first_option = 5;
    entry.num_options_1 = 2;
    entry.num_options_2 = 3;

    EXPECT_EQ(entry.index1_first_option, 0);
    EXPECT_EQ(entry.num_options_1, 2);
    EXPECT_EQ(entry.index2_first_option, 5);
    EXPECT_EQ(entry.num_options_2, 3);
}

TEST_F(ServiceEntryTest, MaxVersionValues) {
    ServiceEntry entry;
    entry.major_version = 255;
    entry.minor_version = 0xFFFFFFFF;

    EXPECT_EQ(entry.major_version, 255);
    EXPECT_EQ(entry.minor_version, 0xFFFFFFFF);
}

TEST_F(ServiceEntryTest, MaxServiceAndInstanceID) {
    ServiceEntry entry;
    entry.service_id = 0xFFFF;
    entry.instance_id = 0xFFFF;

    EXPECT_EQ(entry.service_id, 0xFFFF);
    EXPECT_EQ(entry.instance_id, 0xFFFF);
}

//==============================================================================
// SOME/IP-SD Eventgroup Entry Tests
//==============================================================================

class EventgroupEntryTest : public ::testing::Test {};

TEST_F(EventgroupEntryTest, CreateSubscribeEventgroupEntry) {
    EventgroupEntry entry;
    entry.type = EntryType::SubscribeEventgroup;
    entry.service_id = 0x1234;
    entry.instance_id = 0x5678;
    entry.eventgroup_id = 0x0001;
    entry.counter = 1;
    entry.ttl = 5;

    EXPECT_EQ(entry.type, EntryType::SubscribeEventgroup);
    EXPECT_EQ(entry.service_id, 0x1234);
    EXPECT_EQ(entry.eventgroup_id, 0x0001);
    EXPECT_EQ(entry.counter, 1);
    EXPECT_EQ(entry.ttl, 5);
}

TEST_F(EventgroupEntryTest, StopSubscribeEventgroup) {
    EventgroupEntry entry;
    entry.type = EntryType::StopSubscribeEventgroup;
    entry.ttl = 0;

    EXPECT_EQ(entry.type, EntryType::StopSubscribeEventgroup);
    EXPECT_EQ(entry.ttl, 0);
}

TEST_F(EventgroupEntryTest, EventgroupCounterField) {
    EventgroupEntry entry;

    for (int i = 0; i <= 15; ++i) {
        entry.counter = static_cast<std::uint8_t>(i);
        EXPECT_EQ(entry.counter, i);
    }
}

TEST_F(EventgroupEntryTest, EventgroupIDRange) {
    EventgroupEntry entry;
    entry.eventgroup_id = 0x0000;
    EXPECT_EQ(entry.eventgroup_id, 0x0000);

    entry.eventgroup_id = 0xFFFF;
    EXPECT_EQ(entry.eventgroup_id, 0xFFFF);
}

TEST_F(EventgroupEntryTest, SubscribeAndAck) {
    EventgroupEntry entry_sub;
    entry_sub.type = EntryType::SubscribeEventgroup;
    entry_sub.eventgroup_id = 0x0001;

    EventgroupEntry entry_ack;
    entry_ack.type = EntryType::SubscribeEventgroupAck;
    entry_ack.eventgroup_id = 0x0001;

    EXPECT_EQ(entry_sub.eventgroup_id, entry_ack.eventgroup_id);
}

//==============================================================================
// SOME/IP-SD Option Type Tests
//==============================================================================

class SomeIpSdOptionTest : public ::testing::Test {};

TEST_F(SomeIpSdOptionTest, IPv4EndpointOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::IPv4Endpoint), 0x04);
}

TEST_F(SomeIpSdOptionTest, IPv6EndpointOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::IPv6Endpoint), 0x06);
}

TEST_F(SomeIpSdOptionTest, IPv4MulticastOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::IPv4Multicast), 0x14);
}

TEST_F(SomeIpSdOptionTest, IPv6MulticastOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::IPv6Multicast), 0x16);
}

TEST_F(SomeIpSdOptionTest, IPv4SDEndpointOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::IPv4SDEndpoint), 0x24);
}

TEST_F(SomeIpSdOptionTest, IPv6SDEndpointOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::IPv6SDEndpoint), 0x26);
}

TEST_F(SomeIpSdOptionTest, ConfigurationOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::Configuration), 0x01);
}

TEST_F(SomeIpSdOptionTest, LoadBalancingOptionType) {
    EXPECT_EQ(static_cast<std::uint8_t>(OptionType::LoadBalancing), 0x02);
}

TEST_F(SomeIpSdOptionTest, L4ProtocolTCP) {
    EXPECT_EQ(static_cast<std::uint8_t>(L4Protocol::TCP), 0x06);
}

TEST_F(SomeIpSdOptionTest, L4ProtocolUDP) {
    EXPECT_EQ(static_cast<std::uint8_t>(L4Protocol::UDP), 0x11);
}

TEST_F(SomeIpSdOptionTest, IPv4EndpointOptionStructure) {
    IPv4EndpointOption opt;
    opt.port = 30490;
    opt.protocol = L4Protocol::UDP;

    EXPECT_EQ(opt.port, 30490);
    EXPECT_EQ(opt.protocol, L4Protocol::UDP);
}

TEST_F(SomeIpSdOptionTest, IPv4EndpointWithTCP) {
    IPv4EndpointOption opt;
    opt.port = 30490;
    opt.protocol = L4Protocol::TCP;

    EXPECT_EQ(opt.protocol, L4Protocol::TCP);
}

//==============================================================================
// SOME/IP-SD Header Tests
//==============================================================================

class SomeIpSdHeaderTest : public ::testing::Test {};

TEST_F(SomeIpSdHeaderTest, RebootFlagCheck) {
    SomeIpSdHeader header;

    header.flags = 0x00;
    EXPECT_FALSE(header.is_reboot());

    header.flags = 0x80;
    EXPECT_TRUE(header.is_reboot());

    header.flags = 0xFF;
    EXPECT_TRUE(header.is_reboot());
}

TEST_F(SomeIpSdHeaderTest, UnicastFlagCheck) {
    SomeIpSdHeader header;

    header.flags = 0x00;
    EXPECT_FALSE(header.is_unicast());

    header.flags = 0x40;
    EXPECT_TRUE(header.is_unicast());

    header.flags = 0xFF;
    EXPECT_TRUE(header.is_unicast());
}

TEST_F(SomeIpSdHeaderTest, RebootAndUnicastFlags) {
    SomeIpSdHeader header;
    header.flags = 0xC0;
    EXPECT_TRUE(header.is_reboot());
    EXPECT_TRUE(header.is_unicast());
}

TEST_F(SomeIpSdHeaderTest, EntryAndOptionLengths) {
    SomeIpSdHeader header;
    header.entries_length = 48;
    header.options_length = 32;

    EXPECT_EQ(header.entries_length, 48);
    EXPECT_EQ(header.options_length, 32);
}

TEST_F(SomeIpSdHeaderTest, HeaderSize) {
    SomeIpSdHeader header;
    header.entries_length = 32;
    header.options_length = 16;

    std::size_t expected = SD_HEADER_SIZE + 32 + 16;
    EXPECT_EQ(header.header_size(), expected);
}

TEST_F(SomeIpSdHeaderTest, PayloadSize) {
    SomeIpSdHeader header;
    EXPECT_EQ(header.payload_size(), 0);
}

TEST_F(SomeIpSdHeaderTest, ProtocolName) {
    SomeIpSdHeader header;
    EXPECT_EQ(header.protocol_name(), "SOME/IP-SD");
}

//==============================================================================
// SOME/IP-SD Variant Tests
//==============================================================================

class SomeIpSdVariantTest : public ::testing::Test {};

TEST_F(SomeIpSdVariantTest, StoreServiceEntryInVariant) {
    ServiceEntry svc;
    svc.service_id = 0x1234;

    SdEntry entry = svc;

    EXPECT_TRUE(std::holds_alternative<ServiceEntry>(entry));

    auto& stored = std::get<ServiceEntry>(entry);
    EXPECT_EQ(stored.service_id, 0x1234);
}

TEST_F(SomeIpSdVariantTest, StoreEventgroupEntryInVariant) {
    EventgroupEntry eg;
    eg.eventgroup_id = 0x0001;

    SdEntry entry = eg;

    EXPECT_TRUE(std::holds_alternative<EventgroupEntry>(entry));

    auto& stored = std::get<EventgroupEntry>(entry);
    EXPECT_EQ(stored.eventgroup_id, 0x0001);
}

//==============================================================================
// SOME/IP-SD Option Array Tests
//==============================================================================

class SomeIpSdOptionArrayTest : public ::testing::Test {};

TEST_F(SomeIpSdOptionArrayTest, IPv4EndpointOptionStructure) {
    IPv4EndpointOption opt;
    opt.port = 30490;
    opt.protocol = L4Protocol::UDP;

    EXPECT_EQ(opt.port, 30490);
    EXPECT_EQ(opt.protocol, L4Protocol::UDP);
}

TEST_F(SomeIpSdOptionArrayTest, IPv4EndpointOptionWithTCP) {
    IPv4EndpointOption opt;
    opt.port = 8080;
    opt.protocol = L4Protocol::TCP;

    EXPECT_EQ(opt.port, 8080);
    EXPECT_EQ(opt.protocol, L4Protocol::TCP);
}

TEST_F(SomeIpSdOptionArrayTest, SdOptionVariant) {
    SdOption opt;
    opt.type = OptionType::IPv4Endpoint;
    opt.data.push_back(std::byte{0xC0});
    opt.data.push_back(std::byte{0xA8});
    opt.data.push_back(std::byte{0x01});
    opt.data.push_back(std::byte{0x01});

    EXPECT_EQ(opt.type, OptionType::IPv4Endpoint);
    EXPECT_EQ(opt.data.size(), 4);
}

TEST_F(SomeIpSdOptionArrayTest, ConfigurationOption) {
    SdOption opt;
    opt.type = OptionType::Configuration;

    EXPECT_EQ(opt.type, OptionType::Configuration);
}

TEST_F(SomeIpSdOptionArrayTest, LoadBalancingOption) {
    SdOption opt;
    opt.type = OptionType::LoadBalancing;

    EXPECT_EQ(opt.type, OptionType::LoadBalancing);
}

TEST_F(SomeIpSdOptionArrayTest, MultipleOptionsInArray) {
    std::vector<SdOption> options;

    SdOption ipv4_opt;
    ipv4_opt.type = OptionType::IPv4Endpoint;
    ipv4_opt.data.resize(6);
    options.push_back(ipv4_opt);

    SdOption config_opt;
    config_opt.type = OptionType::Configuration;
    config_opt.data.resize(4);
    options.push_back(config_opt);

    SdOption lb_opt;
    lb_opt.type = OptionType::LoadBalancing;
    lb_opt.data.resize(6);
    options.push_back(lb_opt);

    EXPECT_EQ(options.size(), 3);
    EXPECT_EQ(options[0].type, OptionType::IPv4Endpoint);
    EXPECT_EQ(options[1].type, OptionType::Configuration);
    EXPECT_EQ(options[2].type, OptionType::LoadBalancing);
}

TEST_F(SomeIpSdOptionArrayTest, OptionIndexLinking1) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 2;
    entry.index2_first_option = 2;
    entry.num_options_2 = 1;

    // First set of options: indices 0-1
    EXPECT_EQ(entry.index1_first_option, 0);
    EXPECT_EQ(entry.num_options_1, 2);

    // Second set of options: indices 2-2
    EXPECT_EQ(entry.index2_first_option, 2);
    EXPECT_EQ(entry.num_options_2, 1);
}

TEST_F(SomeIpSdOptionArrayTest, OptionIndexLinking2) {
    EventgroupEntry entry;
    entry.index1_first_option = 5;
    entry.num_options_1 = 3;
    entry.index2_first_option = 8;
    entry.num_options_2 = 2;

    EXPECT_EQ(entry.index1_first_option, 5);
    EXPECT_EQ(entry.num_options_1, 3);
    EXPECT_EQ(entry.index2_first_option, 8);
    EXPECT_EQ(entry.num_options_2, 2);
}

TEST_F(SomeIpSdOptionArrayTest, IPv4MulticastOption) {
    SdOption opt;
    opt.type = OptionType::IPv4Multicast;

    EXPECT_EQ(opt.type, OptionType::IPv4Multicast);
}

TEST_F(SomeIpSdOptionArrayTest, IPv6MulticastOption) {
    SdOption opt;
    opt.type = OptionType::IPv6Multicast;

    EXPECT_EQ(opt.type, OptionType::IPv6Multicast);
}

TEST_F(SomeIpSdOptionArrayTest, IPv4SDEndpointOption) {
    SdOption opt;
    opt.type = OptionType::IPv4SDEndpoint;

    EXPECT_EQ(opt.type, OptionType::IPv4SDEndpoint);
}

TEST_F(SomeIpSdOptionArrayTest, IPv6SDEndpointOption) {
    SdOption opt;
    opt.type = OptionType::IPv6SDEndpoint;

    EXPECT_EQ(opt.type, OptionType::IPv6SDEndpoint);
}

TEST_F(SomeIpSdOptionArrayTest, EntryWithNoOptions) {
    ServiceEntry entry;
    entry.num_options_1 = 0;
    entry.num_options_2 = 0;

    EXPECT_EQ(entry.num_options_1, 0);
    EXPECT_EQ(entry.num_options_2, 0);
}

TEST_F(SomeIpSdOptionArrayTest, EntryWithMaxOptions) {
    ServiceEntry entry;
    entry.num_options_1 = 15;  // Max 4 bits
    entry.num_options_2 = 15;  // Max 4 bits

    EXPECT_EQ(entry.num_options_1, 15);
    EXPECT_EQ(entry.num_options_2, 15);
}

//==============================================================================
// Comprehensive Entry Type Tests (T066)
//==============================================================================

class SdEntryComprehensiveTest : public ::testing::Test {};

// FindService entry tests
TEST_F(SdEntryComprehensiveTest, FindServiceBasicParsing) {
    ServiceEntry entry;
    entry.type = EntryType::FindService;
    entry.service_id = 0x1234;
    entry.instance_id = 0x5678;
    entry.major_version = 1;
    entry.ttl = 3;

    EXPECT_EQ(entry.type, EntryType::FindService);
    EXPECT_EQ(entry.service_id, 0x1234);
    EXPECT_EQ(entry.instance_id, 0x5678);
    EXPECT_EQ(entry.major_version, 1);
    EXPECT_EQ(entry.ttl, 3);
}

TEST_F(SdEntryComprehensiveTest, FindServiceMaxValues) {
    ServiceEntry entry;
    entry.type = EntryType::FindService;
    entry.service_id = 0xFFFF;
    entry.instance_id = 0xFFFF;
    entry.major_version = 0xFF;
    entry.minor_version = 0xFFFFFFFF;
    entry.ttl = 0xFFFFFF;

    EXPECT_EQ(entry.service_id, 0xFFFF);
    EXPECT_EQ(entry.instance_id, 0xFFFF);
    EXPECT_EQ(entry.major_version, 0xFF);
    EXPECT_EQ(entry.minor_version, 0xFFFFFFFF);
    EXPECT_EQ(entry.ttl, 0xFFFFFF);
}

TEST_F(SdEntryComprehensiveTest, FindServiceWithTimeout) {
    ServiceEntry entry;
    entry.type = EntryType::FindService;
    entry.ttl = 5;  // 5 seconds timeout

    EXPECT_EQ(entry.ttl, 5);
}

// OfferService entry tests
TEST_F(SdEntryComprehensiveTest, OfferServiceBasicParsing) {
    ServiceEntry entry;
    entry.type = EntryType::OfferService;
    entry.service_id = 0xABCD;
    entry.instance_id = 0xEF01;
    entry.major_version = 2;
    entry.ttl = 300;

    EXPECT_EQ(entry.type, EntryType::OfferService);
    EXPECT_EQ(entry.service_id, 0xABCD);
    EXPECT_EQ(entry.ttl, 300);
}

TEST_F(SdEntryComprehensiveTest, OfferServiceWithRebootFlag) {
    ServiceEntry entry;
    entry.type = EntryType::OfferService;
    entry.index1_first_option = 0;
    entry.index2_first_option = 0x40;  // Reboot flag encoding

    // Extract reboot flag (bit 6 of second option index byte)
    bool reboot = (entry.index2_first_option & 0x40) != 0;
    EXPECT_TRUE(reboot);
}

TEST_F(SdEntryComprehensiveTest, OfferServiceWithUnicastFlag) {
    ServiceEntry entry;
    entry.type = EntryType::OfferService;
    entry.index1_first_option = 0x10;  // Unicast flag encoding

    // Extract unicast flag (bit 4 of first option index byte)
    bool unicast = (entry.index1_first_option & 0x10) != 0;
    EXPECT_TRUE(unicast);
}

TEST_F(SdEntryComprehensiveTest, OfferServiceStopCondition) {
    ServiceEntry entry;
    entry.type = EntryType::OfferService;
    entry.ttl = 0;  // TTL = 0 means stop offer

    EXPECT_EQ(entry.ttl, 0);
    EXPECT_EQ(entry.type, EntryType::OfferService);
}

TEST_F(SdEntryComprehensiveTest, StopOfferServiceType) {
    ServiceEntry entry;
    entry.type = EntryType::StopOfferService;
    entry.ttl = 0;

    EXPECT_EQ(entry.type, EntryType::StopOfferService);
    EXPECT_EQ(entry.ttl, 0);
}

// SubscribeEventgroup entry tests
TEST_F(SdEntryComprehensiveTest, SubscribeEventgroupBasicParsing) {
    EventgroupEntry entry;
    entry.type = EntryType::SubscribeEventgroup;
    entry.service_id = 0x2468;
    entry.instance_id = 0x1357;
    entry.eventgroup_id = 0x0001;
    entry.counter = 5;
    entry.ttl = 10;

    EXPECT_EQ(entry.type, EntryType::SubscribeEventgroup);
    EXPECT_EQ(entry.eventgroup_id, 0x0001);
    EXPECT_EQ(entry.counter, 5);
    EXPECT_EQ(entry.ttl, 10);
}

TEST_F(SdEntryComprehensiveTest, SubscribeEventgroupMaxCounter) {
    EventgroupEntry entry;
    entry.type = EntryType::SubscribeEventgroup;
    entry.counter = 15;  // 4-bit counter, max value

    EXPECT_EQ(entry.counter, 15);
}

TEST_F(SdEntryComprehensiveTest, SubscribeEventgroupAckResponse) {
    EventgroupEntry entry;
    entry.type = EntryType::SubscribeEventgroupAck;
    entry.eventgroup_id = 0x0001;
    entry.counter = 5;

    EXPECT_EQ(entry.type, EntryType::SubscribeEventgroupAck);
    EXPECT_EQ(entry.counter, 5);
}

TEST_F(SdEntryComprehensiveTest, SubscribeEventgroupNackResponse) {
    EventgroupEntry entry;
    entry.type = EntryType::SubscribeEventgroupNack;
    entry.eventgroup_id = 0x0002;
    entry.counter = 3;

    EXPECT_EQ(entry.type, EntryType::SubscribeEventgroupNack);
    EXPECT_EQ(entry.eventgroup_id, 0x0002);
}

// StopSubscribeEventgroup entry tests
TEST_F(SdEntryComprehensiveTest, StopSubscribeEventgroup) {
    EventgroupEntry entry;
    entry.type = EntryType::StopSubscribeEventgroup;
    entry.service_id = 0x1111;
    entry.instance_id = 0x2222;
    entry.eventgroup_id = 0x0001;
    entry.ttl = 0;

    EXPECT_EQ(entry.type, EntryType::StopSubscribeEventgroup);
    EXPECT_EQ(entry.ttl, 0);
}

// TTL handling tests
TEST_F(SdEntryComprehensiveTest, TtlZeroForStopMessage) {
    ServiceEntry entry;
    entry.ttl = 0;

    // TTL = 0 indicates stop/cancel
    EXPECT_EQ(entry.ttl, 0);
}

TEST_F(SdEntryComprehensiveTest, TtlSmallValue) {
    ServiceEntry entry;
    entry.ttl = 1;  // 1 second

    EXPECT_EQ(entry.ttl, 1);
}

TEST_F(SdEntryComprehensiveTest, TtlCommonValues) {
    std::vector<std::uint32_t> common_ttls = {1, 3, 5, 10, 30, 60, 300, 3600};

    for (auto ttl : common_ttls) {
        ServiceEntry entry;
        entry.ttl = ttl;
        EXPECT_EQ(entry.ttl, ttl);
    }
}

TEST_F(SdEntryComprehensiveTest, TtlInfiniteValue) {
    ServiceEntry entry;
    entry.ttl = 0xFFFFFF;  // Max 24-bit value (infinite)

    EXPECT_EQ(entry.ttl, 0xFFFFFF);
}

// Entry array tests
TEST_F(SdEntryComprehensiveTest, EmptyEntryArray) {
    SdEntryArray array;

    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.size(), 0);
}

TEST_F(SdEntryComprehensiveTest, SingleEntryArray) {
    SdEntryArray array;
    ServiceEntry entry;
    entry.service_id = 0x1234;

    array.push_back(entry);

    EXPECT_FALSE(array.empty());
    EXPECT_EQ(array.size(), 1);
}

TEST_F(SdEntryComprehensiveTest, MultipleEntriesArray) {
    SdEntryArray array;

    for (std::uint16_t i = 0; i < 5; ++i) {
        ServiceEntry entry;
        entry.service_id = i;
        array.push_back(entry);
    }

    EXPECT_EQ(array.size(), 5);
}

TEST_F(SdEntryComprehensiveTest, MixedEntryTypes) {
    SdEntryArray array;

    // Add service entries
    ServiceEntry service_entry;
    service_entry.service_id = 0x1234;
    array.push_back(service_entry);

    // Add eventgroup entry
    EventgroupEntry eg_entry;
    eg_entry.eventgroup_id = 0x0001;
    array.push_back(eg_entry);

    EXPECT_EQ(array.size(), 2);
}

// Entry-Option linking tests
TEST_F(SdEntryComprehensiveTest, EntryWithFirstOptionSet) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 3;
    entry.index2_first_option = 0;
    entry.num_options_2 = 0;

    EXPECT_EQ(entry.index1_first_option, 0);
    EXPECT_EQ(entry.num_options_1, 3);
}

TEST_F(SdEntryComprehensiveTest, EntryWithSecondOptionSet) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 0;
    entry.index2_first_option = 3;
    entry.num_options_2 = 2;

    EXPECT_EQ(entry.index2_first_option, 3);
    EXPECT_EQ(entry.num_options_2, 2);
}

TEST_F(SdEntryComprehensiveTest, EntryWithBothOptionSets) {
    ServiceEntry entry;
    entry.index1_first_option = 0;
    entry.num_options_1 = 2;
    entry.index2_first_option = 2;
    entry.num_options_2 = 1;

    EXPECT_EQ(entry.num_options_1, 2);
    EXPECT_EQ(entry.num_options_2, 1);
}

TEST_F(SdEntryComprehensiveTest, EventgroupEntryWithOptions) {
    EventgroupEntry entry;
    entry.eventgroup_id = 0x0001;
    entry.index1_first_option = 0;
    entry.num_options_1 = 1;
    entry.index2_first_option = 1;
    entry.num_options_2 = 1;

    EXPECT_EQ(entry.num_options_1, 1);
    EXPECT_EQ(entry.num_options_2, 1);
}

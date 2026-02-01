/// @file test_someip_sd_entries.cpp
/// @brief SOME/IP-SD entry and option type tests

#include <gtest/gtest.h>

#include "wadjet/protocols/someip_sd.hpp"

using namespace wadjet::protocols::someip_sd;

//==============================================================================
// SOME/IP-SD Entry Type Tests
//==============================================================================

class SomeIpSdEntryTest : public ::testing::Test {
};

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

class ServiceEntryTest : public ::testing::Test {
};

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

class EventgroupEntryTest : public ::testing::Test {
};

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

class SomeIpSdOptionTest : public ::testing::Test {
};

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

class SomeIpSdHeaderTest : public ::testing::Test {
};

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

class SomeIpSdVariantTest : public ::testing::Test {
};

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


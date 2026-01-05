#pragma once

/// @file discovery.hpp
/// @brief RTPS Discovery protocol structures (SPDP/SEDP)
///
/// This file contains structures for DDS discovery protocols:
/// - SPDP (Simple Participant Discovery Protocol): Discovers participants
/// - SEDP (Simple Endpoint Discovery Protocol): Discovers readers/writers
///
/// These protocols are used to automatically discover DDS participants
/// and their endpoints (DataReaders/DataWriters) on the network.

#include "wadjet/protocols/dds/rtps_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::protocols::dds {

// =============================================================================
// QoS Policies
// =============================================================================

/// @brief Durability QoS kind
enum class DurabilityKind : std::uint32_t {
    Volatile = 0,
    TransientLocal = 1,
    Transient = 2,
    Persistent = 3,
};

/// @brief Convert durability kind to string
[[nodiscard]] constexpr std::string_view durability_kind_string(DurabilityKind kind) {
    switch (kind) {
        case DurabilityKind::Volatile:
            return "VOLATILE";
        case DurabilityKind::TransientLocal:
            return "TRANSIENT_LOCAL";
        case DurabilityKind::Transient:
            return "TRANSIENT";
        case DurabilityKind::Persistent:
            return "PERSISTENT";
        default:
            return "UNKNOWN";
    }
}

/// @brief Reliability QoS kind
enum class ReliabilityKind : std::uint32_t {
    BestEffort = 1,
    Reliable = 2,
};

/// @brief Convert reliability kind to string
[[nodiscard]] constexpr std::string_view reliability_kind_string(ReliabilityKind kind) {
    switch (kind) {
        case ReliabilityKind::BestEffort:
            return "BEST_EFFORT";
        case ReliabilityKind::Reliable:
            return "RELIABLE";
        default:
            return "UNKNOWN";
    }
}

/// @brief Liveliness QoS kind
enum class LivelinessKind : std::uint32_t {
    Automatic = 0,
    ManualByParticipant = 1,
    ManualByTopic = 2,
};

/// @brief Convert liveliness kind to string
[[nodiscard]] constexpr std::string_view liveliness_kind_string(LivelinessKind kind) {
    switch (kind) {
        case LivelinessKind::Automatic:
            return "AUTOMATIC";
        case LivelinessKind::ManualByParticipant:
            return "MANUAL_BY_PARTICIPANT";
        case LivelinessKind::ManualByTopic:
            return "MANUAL_BY_TOPIC";
        default:
            return "UNKNOWN";
    }
}

/// @brief Ownership QoS kind
enum class OwnershipKind : std::uint32_t {
    Shared = 0,
    Exclusive = 1,
};

/// @brief History QoS kind
enum class HistoryKind : std::uint32_t {
    KeepLast = 0,
    KeepAll = 1,
};

/// @brief Destination order QoS kind
enum class DestinationOrderKind : std::uint32_t {
    ByReceptionTimestamp = 0,
    BySourceTimestamp = 1,
};

/// @brief Presentation QoS access scope
enum class PresentationAccessScope : std::uint32_t {
    Instance = 0,
    Topic = 1,
    Group = 2,
};

// =============================================================================
// QoS Policy Structures
// =============================================================================

/// @brief Durability QoS policy
struct DurabilityQos {
    DurabilityKind kind = DurabilityKind::Volatile;
};

/// @brief Reliability QoS policy
struct ReliabilityQos {
    ReliabilityKind kind = ReliabilityKind::BestEffort;
    Duration max_blocking_time = Duration::zero();
};

/// @brief Liveliness QoS policy
struct LivelinessQos {
    LivelinessKind kind = LivelinessKind::Automatic;
    Duration lease_duration = Duration::infinite();
};

/// @brief Deadline QoS policy
struct DeadlineQos {
    Duration period = Duration::infinite();
};

/// @brief Latency budget QoS policy
struct LatencyBudgetQos {
    Duration duration = Duration::zero();
};

/// @brief Ownership QoS policy
struct OwnershipQos {
    OwnershipKind kind = OwnershipKind::Shared;
};

/// @brief Ownership strength QoS policy
struct OwnershipStrengthQos {
    std::int32_t value = 0;
};

/// @brief History QoS policy
struct HistoryQos {
    HistoryKind kind = HistoryKind::KeepLast;
    std::int32_t depth = 1;
};

/// @brief Resource limits QoS policy
struct ResourceLimitsQos {
    std::int32_t max_samples = -1;               // Unlimited
    std::int32_t max_instances = -1;             // Unlimited
    std::int32_t max_samples_per_instance = -1;  // Unlimited
};

/// @brief Destination order QoS policy
struct DestinationOrderQos {
    DestinationOrderKind kind = DestinationOrderKind::ByReceptionTimestamp;
};

/// @brief Presentation QoS policy
struct PresentationQos {
    PresentationAccessScope access_scope = PresentationAccessScope::Instance;
    bool coherent_access = false;
    bool ordered_access = false;
};

/// @brief Partition QoS policy
struct PartitionQos {
    std::vector<std::string> names;
};

/// @brief User data QoS policy
struct UserDataQos {
    std::vector<std::byte> value;
};

/// @brief Topic data QoS policy
struct TopicDataQos {
    std::vector<std::byte> value;
};

/// @brief Group data QoS policy
struct GroupDataQos {
    std::vector<std::byte> value;
};

// =============================================================================
// Parameter IDs for Discovery
// =============================================================================

/// @brief Parameter IDs used in SPDP/SEDP discovery
enum class ParameterId : std::uint16_t {
    // Standard parameters
    PID_PAD = 0x0000,
    PID_SENTINEL = 0x0001,
    PID_USER_DATA = 0x002C,
    PID_TOPIC_NAME = 0x0005,
    PID_TYPE_NAME = 0x0007,
    PID_GROUP_DATA = 0x002D,
    PID_TOPIC_DATA = 0x002E,
    PID_DURABILITY = 0x001D,
    PID_DURABILITY_SERVICE = 0x001E,
    PID_DEADLINE = 0x0023,
    PID_LATENCY_BUDGET = 0x0027,
    PID_LIVELINESS = 0x001B,
    PID_RELIABILITY = 0x001A,
    PID_LIFESPAN = 0x002B,
    PID_DESTINATION_ORDER = 0x0025,
    PID_HISTORY = 0x0040,
    PID_RESOURCE_LIMITS = 0x0041,
    PID_OWNERSHIP = 0x001F,
    PID_OWNERSHIP_STRENGTH = 0x0006,
    PID_PRESENTATION = 0x0021,
    PID_PARTITION = 0x0029,
    PID_TIME_BASED_FILTER = 0x0004,
    PID_TRANSPORT_PRIORITY = 0x0049,

    // Participant parameters
    PID_PROTOCOL_VERSION = 0x0015,
    PID_VENDOR_ID = 0x0016,
    PID_UNICAST_LOCATOR = 0x002F,
    PID_MULTICAST_LOCATOR = 0x0030,
    PID_DEFAULT_UNICAST_LOCATOR = 0x0031,
    PID_DEFAULT_MULTICAST_LOCATOR = 0x0048,
    PID_METATRAFFIC_UNICAST_LOCATOR = 0x0032,
    PID_METATRAFFIC_MULTICAST_LOCATOR = 0x0033,
    PID_EXPECTS_INLINE_QOS = 0x0043,
    PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT = 0x0034,
    PID_PARTICIPANT_BUILTIN_ENDPOINTS = 0x0044,
    PID_PARTICIPANT_LEASE_DURATION = 0x0002,
    PID_PARTICIPANT_GUID = 0x0050,
    PID_BUILTIN_ENDPOINT_SET = 0x0058,
    PID_PROPERTY_LIST = 0x0059,
    PID_ENDPOINT_GUID = 0x005A,

    // Endpoint parameters
    PID_ENTITY_NAME = 0x0062,
    PID_KEY_HASH = 0x0070,
    PID_STATUS_INFO = 0x0071,

    // Vendor-specific (eProsima Fast DDS)
    PID_PERSISTENCE_GUID = 0x8002,
    PID_RELATED_SAMPLE_IDENTITY = 0x800F,

    // Vendor-specific (RTI)
    PID_DOMAIN_ID = 0x000F,
};

/// @brief Convert parameter ID to string
[[nodiscard]] std::string_view parameter_id_string(ParameterId id);

// =============================================================================
// Discovery Parameter
// =============================================================================

/// @brief A single parameter in discovery data
struct DiscoveryParameter {
    ParameterId id = ParameterId::PID_PAD;
    std::vector<std::byte> value;

    /// @brief Get value as string (for string parameters)
    [[nodiscard]] std::string as_string() const {
        if (value.empty())
            return "";
        // Remove null terminator if present
        std::size_t len = value.size();
        if (static_cast<char>(value.back()) == '\0') {
            --len;
        }
        return std::string(reinterpret_cast<const char*>(value.data()), len);
    }
};

/// @brief Parameter list (terminated by SENTINEL)
struct ParameterList {
    std::vector<DiscoveryParameter> parameters;

    /// @brief Find parameter by ID
    [[nodiscard]] const DiscoveryParameter* find(ParameterId id) const {
        for (const auto& p : parameters) {
            if (p.id == id)
                return &p;
        }
        return nullptr;
    }

    /// @brief Get string parameter value
    [[nodiscard]] std::optional<std::string> get_string(ParameterId id) const {
        if (const auto* p = find(id)) {
            return p->as_string();
        }
        return std::nullopt;
    }
};

// =============================================================================
// SPDP - Participant Discovery
// =============================================================================

/// @brief SPDP Participant Built-in Topic Data
///
/// Data announced by participants to discover each other.
struct ParticipantBuiltinTopicData {
    // Key
    GUID participant_guid;

    // Participant info
    std::string participant_name;
    ProtocolVersion protocol_version;
    VendorIdValue vendor_id;
    BuiltinEndpointSet builtin_endpoints;

    // Locators
    std::vector<Locator> metatraffic_unicast_locators;
    std::vector<Locator> metatraffic_multicast_locators;
    std::vector<Locator> default_unicast_locators;
    std::vector<Locator> default_multicast_locators;

    // Liveliness
    Duration lease_duration = Duration::infinite();
    Count manual_liveliness_count;

    // QoS
    UserDataQos user_data;

    // Raw parameters for extended parsing
    ParameterList raw_parameters;

    /// @brief Get a descriptive string
    [[nodiscard]] std::string to_string() const {
        std::string result = "Participant{guid=" + participant_guid.to_string();
        if (!participant_name.empty()) {
            result += ",name=" + participant_name;
        }
        result += ",vendor=" + vendor_id.to_string();
        result += "}";
        return result;
    }
};

// =============================================================================
// SEDP - Endpoint Discovery
// =============================================================================

/// @brief SEDP Publication Built-in Topic Data (DataWriter announcement)
struct PublicationBuiltinTopicData {
    // Key
    GUID writer_guid;

    // Participant
    GUID participant_guid;

    // Topic
    std::string topic_name;
    std::string type_name;

    // QoS policies
    DurabilityQos durability;
    DeadlineQos deadline;
    LatencyBudgetQos latency_budget;
    LivelinessQos liveliness;
    ReliabilityQos reliability;
    OwnershipQos ownership;
    DestinationOrderQos destination_order;
    UserDataQos user_data;
    PartitionQos partition;
    TopicDataQos topic_data;
    GroupDataQos group_data;

    // Locators
    std::vector<Locator> unicast_locators;
    std::vector<Locator> multicast_locators;

    // Raw parameters
    ParameterList raw_parameters;

    /// @brief Get a descriptive string
    [[nodiscard]] std::string to_string() const {
        return "Publication{guid=" + writer_guid.to_string() + ",topic=" + topic_name +
               ",type=" + type_name + "}";
    }
};

/// @brief SEDP Subscription Built-in Topic Data (DataReader announcement)
struct SubscriptionBuiltinTopicData {
    // Key
    GUID reader_guid;

    // Participant
    GUID participant_guid;

    // Topic
    std::string topic_name;
    std::string type_name;

    // QoS policies
    DurabilityQos durability;
    DeadlineQos deadline;
    LatencyBudgetQos latency_budget;
    LivelinessQos liveliness;
    ReliabilityQos reliability;
    OwnershipQos ownership;
    DestinationOrderQos destination_order;
    UserDataQos user_data;
    PartitionQos partition;
    TopicDataQos topic_data;
    GroupDataQos group_data;

    // Time filter
    Duration time_based_filter_minimum_separation = Duration::zero();

    // Locators
    std::vector<Locator> unicast_locators;
    std::vector<Locator> multicast_locators;

    // Raw parameters
    ParameterList raw_parameters;

    /// @brief Get a descriptive string
    [[nodiscard]] std::string to_string() const {
        return "Subscription{guid=" + reader_guid.to_string() + ",topic=" + topic_name +
               ",type=" + type_name + "}";
    }
};

/// @brief Topic Built-in Topic Data (optional)
struct TopicBuiltinTopicData {
    // Key
    std::string topic_name;
    std::string type_name;

    // QoS
    DurabilityQos durability;
    DeadlineQos deadline;
    LatencyBudgetQos latency_budget;
    LivelinessQos liveliness;
    ReliabilityQos reliability;
    DestinationOrderQos destination_order;
    HistoryQos history;
    ResourceLimitsQos resource_limits;
    OwnershipQos ownership;
    TopicDataQos topic_data;
};

// =============================================================================
// Discovery Data Variant
// =============================================================================

/// @brief Parsed discovery data
using DiscoveryData =
    std::variant<std::monostate, ParticipantBuiltinTopicData, PublicationBuiltinTopicData,
                 SubscriptionBuiltinTopicData, TopicBuiltinTopicData>;

// =============================================================================
// Discovery Parser
// =============================================================================

/// @brief Parser for SPDP/SEDP discovery payloads
class DiscoveryParser {
public:
    /// @brief Parse SPDP participant data from serialized payload
    [[nodiscard]] static std::optional<ParticipantBuiltinTopicData> parse_participant_data(
        std::span<const std::byte> data, bool little_endian);

    /// @brief Parse SEDP publication data from serialized payload
    [[nodiscard]] static std::optional<PublicationBuiltinTopicData> parse_publication_data(
        std::span<const std::byte> data, bool little_endian);

    /// @brief Parse SEDP subscription data from serialized payload
    [[nodiscard]] static std::optional<SubscriptionBuiltinTopicData> parse_subscription_data(
        std::span<const std::byte> data, bool little_endian);

    /// @brief Parse parameter list from CDR-encoded data
    [[nodiscard]] static std::optional<ParameterList> parse_parameter_list(
        std::span<const std::byte> data, bool little_endian);
};

}  // namespace wadjet::protocols::dds

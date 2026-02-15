#pragma once

#include "wadjet/distributed/result.hpp"
#include "wadjet/distributed/types.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Types of test steps in distributed scenario
 *
 * T072: Defines the different types of operations that can occur in a test scenario
 * T318: Added DIAGNOSTIC step type for multi-ECU diagnostic session sequences
 * T327: Added SEND step type for traffic injection across nodes
 */
enum class StepType {
    UNKNOWN = 0,
    BARRIER = 1,     ///< Synchronization barrier
    CAPTURE = 2,     ///< Packet capture operation
    EXPECT = 3,      ///< Assertion/expectation
    WAIT = 4,        ///< Time delay
    LOG = 5,         ///< Log event
    DIAGNOSTIC = 6,  ///< Diagnostic session operation (T318)
    SEND = 7,        ///< Send/traffic injection step (T327)
};

/**
 * @brief Protocol types for protocol-aware distributed assertions
 *
 * T336: Defines protocol-specific expectation matching for distributed scenarios
 * Used in ExpectStepConfig for typed protocol expectations instead of generic JSON
 */
enum class ProtocolType {
    UNKNOWN = 0,
    ETHERNET = 1,   ///< Ethernet frame matching (M2)
    IPv4 = 2,       ///< IPv4 packet matching (M2)
    UDP = 3,        ///< UDP datagram matching (M2)
    TCP = 4,        ///< TCP segment matching (M2)
    SOMEIP = 5,     ///< SOME/IP protocol matching (M2)
    DoIP = 6,       ///< Diagnostic over IP matching (M2)
    UDS = 7,        ///< UDS service matching (M9)
    GENERIC = 255,  ///< Generic/untyped assertion (backward compatibility)
};

/**
 * @brief Configuration for barrier synchronization step
 *
 * T073: Specifies parameters for barrier steps
 */
struct BarrierStepConfig {
    std::string barrier_id;                        ///< Unique barrier identifier
    std::chrono::milliseconds timeout_ms{5000};    ///< Barrier timeout
    std::vector<std::string> participating_nodes;  ///< Nodes that must reach barrier
};

/**
 * @brief Configuration for packet capture step
 *
 * T073: Specifies parameters for capture steps
 */
struct CaptureStepConfig {
    std::string capture_id;                    ///< Unique capture identifier
    std::vector<std::string> nodes;            ///< Nodes to capture on
    std::string interface;                     ///< Network interface to capture
    std::string bpf_filter;                    ///< BPF filter expression
    std::chrono::milliseconds duration_ms{0};  ///< Capture duration (0 = indefinite)
    bool hardware_timestamps{false};           ///< Use hardware timestamps
    uint32_t snaplen{65535};                   ///< Max bytes per packet
    uint32_t buffer_size{1024 * 1024};         ///< Ring buffer size
};

/**
 * @brief Configuration for expectation/assertion step
 *
 * T073: Specifies parameters for assertion steps
 * T336: Extended with protocol-aware matching for M2/M9 protocol-specific assertions
 *
 * Supports two modes:
 * 1. Generic mode (backward compatible): assertion_type + assertion_params (JSON)
 * 2. Protocol-aware mode: protocol + match_fields (typed struct)
 */
struct ExpectStepConfig {
    std::string assertion_id;  ///< Unique assertion identifier

    // Generic mode (backward compatible, T073)
    std::string
        assertion_type;  ///< Type: "message_flow", "latency", "happens_before", "must_not_see"
    std::string assertion_params;  ///< JSON-encoded assertion parameters (legacy)

    // Protocol-aware mode (T336 - new)
    ProtocolType protocol{ProtocolType::GENERIC};  ///< Protocol layer to match (M2: Ethernet, IPv4,
                                                    ///< UDP, TCP, SOME/IP, DoIP; M9: UDS)

    // Structured match fields per protocol (replaces generic assertion_params)
    std::unordered_map<std::string, std::string>
        match_fields;  ///< Protocol-specific match fields (e.g., "service_id"→"0x1234" for SOME/IP)

    // Assertion context
    std::chrono::milliseconds timeout_ms{5000};  ///< Assertion timeout
    bool should_fail{false};                     ///< Expected to fail (negative test)

    // Node context for distributed protocol assertions
    std::string src_node;  ///< Source node (publisher/sender) for flows
    std::string dst_node;  ///< Destination node (subscriber/receiver) for flows
};

/**
 * @brief Helper to check if ExpectStepConfig uses protocol-aware mode
 *
 * T336: Utility to determine if config uses new protocol-aware fields vs legacy generic mode
 */
inline bool uses_protocol_aware_assertions(const ExpectStepConfig& cfg) {
    return cfg.protocol != ProtocolType::GENERIC && !cfg.match_fields.empty();
}

/**
 * @brief Configuration for wait/delay step
 *
 * T262: Specifies parameters for wait steps per data-model.md
 */
struct WaitStepConfig {
    std::chrono::milliseconds duration;  ///< Duration to wait
};

/**
 * @brief Configuration for log event step
 *
 * T263: Specifies parameters for log steps per data-model.md
 */
struct LogStepConfig {
    std::string message;         ///< Log message text
    std::string level = "INFO";  ///< Log level (INFO, DEBUG, WARN, ERROR)
};

/**
 * @brief Configuration for send/traffic injection step
 *
 * T327: Specifies parameters for sending packets from nodes
 *
 * Supports:
 * - Sending from PCAP file on specified nodes
 * - Sending raw packet data on specified nodes
 * - Delay before sending for synchronization
 */
struct SendStepConfig {
    std::string send_id;                                ///< Unique send operation identifier
    std::vector<std::string> nodes;                     ///< Nodes to send from
    std::string interface;                              ///< Network interface to send on
    std::optional<std::string> pcap_file;               ///< Send packets from PCAP file
    std::optional<std::vector<std::uint8_t>> raw_data;  ///< Raw packet bytes to send
    std::chrono::milliseconds delay_before_ms{0};       ///< Delay before sending
    std::chrono::milliseconds timeout_ms{5000};         ///< Timeout for send operation
};

/**
 * @brief Configuration for diagnostic session step
 *
 * T318: Specifies parameters for diagnostic session operations across ECUs
 *
 * Supports multi-ECU diagnostic sequences such as:
 * - Security access unlock on one ECU
 * - Flash download on multiple ECUs
 * - DTC reading/clearing across network
 */
struct DiagnosticStepConfig {
    std::string diagnostic_id;  ///< Unique diagnostic operation identifier
    std::string ecu_address;    ///< Target ECU logical address
    std::string
        session_type;  ///< Session type (DefaultSession, ProgrammingSession, ExtendedSession)
    std::string
        expected_service;  ///< Expected UDS service (ReadDataByIdentifier, RequestDownload, etc.)
    std::optional<std::string>
        expected_nrc;  ///< Expected negative response code (if failure expected), empty for success
    std::chrono::milliseconds timeout_ms{5000};  ///< Timeout for the diagnostic operation
    std::vector<std::string> target_nodes;       ///< Nodes involved in this diagnostic operation
};

/**
 * @brief Single step in a distributed test scenario
 *
 * T072: Represents one operation in a multi-node test
 * T266: Uses std::variant for type-safe step configuration
 * T318: Added DIAGNOSTIC variant option
 * T327: Added SEND variant option for traffic injection
 */
struct DistributedStep {
    std::string step_id;               ///< Unique step identifier
    std::string step_name;             ///< Human-readable name
    StepType type{StepType::UNKNOWN};  ///< Type of step

    // T266: Type-safe variant-based configuration per data-model.md
    // T318: Added DiagnosticStepConfig to variant
    // T327: Added SendStepConfig to variant for traffic injection
    std::variant<BarrierStepConfig, CaptureStepConfig, ExpectStepConfig, WaitStepConfig,
                 LogStepConfig, DiagnosticStepConfig, SendStepConfig>
        config;

    std::chrono::milliseconds delay_before_ms{0};  ///< Delay before step execution
    std::chrono::milliseconds timeout_ms{5000};    ///< Overall timeout for this step

    std::vector<std::string> target_nodes;  ///< Nodes this step applies to
    bool parallel{false};                   ///< Run in parallel with next step
    std::string depends_on;                 ///< Step ID this depends on
};

/**
 * @brief Node definition for scenario
 *
 * T264: Specifies node information in scenario per data-model.md
 */
struct NodeDefinition {
    std::string id;                       ///< Node identifier
    std::string address;                  ///< Node address (hostname:port)
    std::vector<std::string> interfaces;  ///< Network interfaces available on node
};

/**
 * @brief Node assignment for a scenario
 *
 * Specifies which role each node plays in the scenario
 */
struct NodeAssignment {
    std::string node_id;                  ///< Node identifier
    std::string role;                     ///< Role: "sender", "receiver", "observer"
    std::vector<std::string> interfaces;  ///< Network interfaces available
};

/**
 * @brief Complete distributed test scenario
 *
 * T074: Main class for scenario management with parsing and execution
 */
class DistributedScenario {
public:
    /**
     * @brief Create a new scenario from YAML file
     *
     * T075: Parses YAML scenario definition
     *
     * @param yaml_file Path to YAML scenario file
     * @return Scenario instance or error
     */
    static auto from_yaml(const std::string& yaml_file) -> std::unique_ptr<DistributedScenario>;

    /**
     * @brief Create a new scenario from JSON file
     *
     * T076: Parses JSON scenario definition
     *
     * @param json_file Path to JSON scenario file
     * @return Scenario instance or error
     */
    static auto from_json(const std::string& json_file) -> std::unique_ptr<DistributedScenario>;

    /**
     * @brief Create a new scenario from YAML string
     *
     * @param yaml_content YAML content as string
     * @return Scenario instance or error
     */
    static auto from_yaml_string(const std::string& yaml_content)
        -> std::unique_ptr<DistributedScenario>;

    /**
     * @brief Create a new scenario from JSON string
     *
     * @param json_content JSON content as string
     * @return Scenario instance or error
     */
    static auto from_json_string(const std::string& json_content)
        -> std::unique_ptr<DistributedScenario>;

    virtual ~DistributedScenario() = default;

    /**
     * @brief Get scenario identifier
     *
     * @return Scenario ID
     */
    virtual auto id() const -> const std::string& = 0;

    /**
     * @brief Get scenario name
     *
     * @return Scenario name
     */
    virtual auto name() const -> const std::string& = 0;

    /**
     * @brief Get scenario tags
     *
     * T265: Get list of tags per data-model.md for categorization
     *
     * @return Vector of tag strings
     */
    virtual auto tags() const -> const std::vector<std::string>& = 0;

    /**
     * @brief Get all node definitions
     *
     * T264: Get node layout for scenario per data-model.md
     *
     * @return Vector of node definitions
     */
    virtual auto nodes() const -> const std::vector<NodeDefinition>& = 0;

    /**
     * @brief Get all node assignments
     *
     * @return Vector of node assignments
     */
    virtual auto node_assignments() const -> const std::vector<NodeAssignment>& = 0;

    /**
     * @brief Get all scenario steps
     *
     * @return Vector of steps
     */
    virtual auto steps() const -> const std::vector<DistributedStep>& = 0;

    /**
     * @brief Get steps targeted to a specific node
     *
     * T077: For scenario decomposition
     *
     * @param node_id Node identifier
     * @return Vector of steps for this node
     */
    virtual auto steps_for_node(const std::string& node_id) const
        -> std::vector<DistributedStep> = 0;

    /**
     * @brief Get description
     *
     * @return Scenario description
     */
    virtual auto description() const -> const std::string& = 0;

    /**
     * @brief Validate scenario consistency
     *
     * @return true if valid, false otherwise with error logged
     */
    virtual auto validate() const -> bool = 0;
};

}  // namespace wadjet::distributed

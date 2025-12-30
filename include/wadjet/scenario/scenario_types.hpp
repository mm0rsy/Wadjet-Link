/// @file scenario_types.hpp
/// @brief Data types for test scenario definitions
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This header defines the data model for YAML/JSON test scenarios.
/// Scenarios consist of steps that configure capture, send packets,
/// and assert expectations on received traffic.

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace wadjet::scenario {

// =============================================================================
// Time Types
// =============================================================================

using Duration = std::chrono::milliseconds;

// =============================================================================
// Capture Configuration
// =============================================================================

/// @brief Configuration for packet capture
struct CaptureConfig {
    std::string interface;              ///< Network interface (e.g., "eth0")
    std::string filter;                 ///< BPF filter expression
    std::optional<std::string> pcap_file;  ///< Optional pcap file for replay
    Duration timeout{5000};             ///< Capture timeout
    bool promiscuous{true};             ///< Enable promiscuous mode
};

// =============================================================================
// Protocol Expectations
// =============================================================================

/// @brief Comparison operators for expectations
enum class CompareOp {
    Equal,          ///< ==
    NotEqual,       ///< !=
    GreaterThan,    ///< >
    GreaterEqual,   ///< >=
    LessThan,       ///< <
    LessEqual,      ///< <=
};

/// @brief Convert string to CompareOp
[[nodiscard]] inline std::optional<CompareOp> parse_compare_op(std::string_view str) {
    if (str == "==" || str == "eq" || str.empty()) return CompareOp::Equal;
    if (str == "!=" || str == "ne") return CompareOp::NotEqual;
    if (str == ">" || str == "gt") return CompareOp::GreaterThan;
    if (str == ">=" || str == "ge") return CompareOp::GreaterEqual;
    if (str == "<" || str == "lt") return CompareOp::LessThan;
    if (str == "<=" || str == "le") return CompareOp::LessEqual;
    return std::nullopt;
}

/// @brief Parse count expression like ">= 3" or "5"
struct CountExpression {
    CompareOp op{CompareOp::GreaterEqual};
    std::size_t value{1};

    /// @brief Parse from string (e.g., ">= 3", "== 5", "3")
    [[nodiscard]] static std::optional<CountExpression> parse(std::string_view str);
    
    /// @brief Evaluate the expression against a count
    [[nodiscard]] bool evaluate(std::size_t count) const;
};

/// @brief Ethernet frame expectations
struct EthernetExpect {
    std::optional<std::string> src_mac;     ///< Source MAC (e.g., "00:11:22:33:44:55")
    std::optional<std::string> dst_mac;     ///< Destination MAC
    std::optional<std::uint16_t> ethertype; ///< EtherType
    std::optional<std::uint16_t> vlan_id;   ///< VLAN ID
};

/// @brief IPv4 expectations
struct IPv4Expect {
    std::optional<std::string> src_ip;      ///< Source IP (e.g., "192.168.1.100")
    std::optional<std::string> dst_ip;      ///< Destination IP
    std::optional<std::uint8_t> protocol;   ///< IP protocol number
    std::optional<std::uint8_t> ttl;        ///< Time-to-live
};

/// @brief UDP expectations
struct UDPExpect {
    std::optional<std::uint16_t> src_port;  ///< Source port
    std::optional<std::uint16_t> dst_port;  ///< Destination port
};

/// @brief TCP expectations
struct TCPExpect {
    std::optional<std::uint16_t> src_port;  ///< Source port
    std::optional<std::uint16_t> dst_port;  ///< Destination port
    std::optional<bool> syn;                ///< SYN flag
    std::optional<bool> ack;                ///< ACK flag
    std::optional<bool> fin;                ///< FIN flag
    std::optional<bool> rst;                ///< RST flag
};

/// @brief SOME/IP message type enum for expectations
enum class SomeIpMessageTypeExpect {
    Request,
    RequestNoReturn,
    Notification,
    Response,
    Error,
    Any
};

/// @brief SOME/IP expectations
struct SomeIpExpect {
    std::optional<std::uint16_t> service_id;    ///< Service ID
    std::optional<std::uint16_t> method_id;     ///< Method/Event ID
    std::optional<std::uint16_t> client_id;     ///< Client ID
    std::optional<std::uint16_t> session_id;    ///< Session ID
    std::optional<SomeIpMessageTypeExpect> message_type; ///< Message type
    std::optional<std::uint8_t> return_code;    ///< Return code
};

/// @brief SOME/IP-SD entry type for expectations
enum class SdEntryTypeExpect {
    FindService,
    OfferService,
    Subscribe,
    SubscribeAck,
    Any
};

/// @brief SOME/IP-SD expectations
struct SomeIpSdExpect {
    std::optional<std::uint16_t> service_id;    ///< Service ID
    std::optional<std::uint16_t> instance_id;   ///< Instance ID
    std::optional<SdEntryTypeExpect> entry_type; ///< Entry type
    std::optional<std::uint8_t> major_version;  ///< Major version
};

/// @brief DoIP payload type for expectations
enum class DoIpPayloadTypeExpect {
    VehicleIdentificationRequest,
    VehicleIdentificationResponse,
    RoutingActivationRequest,
    RoutingActivationResponse,
    DiagnosticMessage,
    DiagnosticPositiveAck,
    DiagnosticNegativeAck,
    Any
};

/// @brief DoIP expectations
struct DoIpExpect {
    std::optional<DoIpPayloadTypeExpect> payload_type;  ///< Payload type
    std::optional<std::uint16_t> source_address;        ///< Source address
    std::optional<std::uint16_t> target_address;        ///< Target address
};

/// @brief UDS service type for expectations
enum class UdsServiceTypeExpect {
    DiagnosticSessionControl,         ///< 0x10
    ECUReset,                         ///< 0x11
    SecurityAccess,                   ///< 0x27
    CommunicationControl,             ///< 0x28
    TesterPresent,                    ///< 0x3E
    ControlDTCSetting,                ///< 0x85
    ResponseOnEvent,                  ///< 0x86
    LinkControl,                      ///< 0x87
    ReadDataByIdentifier,             ///< 0x22
    ReadMemoryByAddress,              ///< 0x23
    ReadScalingDataByIdentifier,      ///< 0x24
    ReadDataByPeriodicIdentifier,     ///< 0x2A
    DynamicallyDefineDataIdentifier,  ///< 0x2C
    WriteDataByIdentifier,            ///< 0x2E
    WriteMemoryByAddress,             ///< 0x3D
    ClearDiagnosticInformation,       ///< 0x14
    ReadDTCInformation,               ///< 0x19
    InputOutputControlByIdentifier,   ///< 0x2F
    RoutineControl,                   ///< 0x31
    RequestDownload,                  ///< 0x34
    RequestUpload,                    ///< 0x35
    TransferData,                     ///< 0x36
    RequestTransferExit,              ///< 0x37
    RequestFileTransfer,              ///< 0x38
    NegativeResponse,                 ///< 0x7F
    Any
};

/// @brief UDS NRC (Negative Response Code) for expectations
enum class UdsNRCExpect {
    GeneralReject,                           ///< 0x10
    ServiceNotSupported,                     ///< 0x11
    SubFunctionNotSupported,                 ///< 0x12
    IncorrectMessageLengthOrInvalidFormat,   ///< 0x13
    ResponseTooLong,                         ///< 0x14
    BusyRepeatRequest,                       ///< 0x21
    ConditionsNotCorrect,                    ///< 0x22
    RequestSequenceError,                    ///< 0x24
    RequestOutOfRange,                       ///< 0x31
    SecurityAccessDenied,                    ///< 0x33
    InvalidKey,                              ///< 0x35
    ExceededNumberOfAttempts,                ///< 0x36
    RequiredTimeDelayNotExpired,             ///< 0x37
    UploadDownloadNotAccepted,               ///< 0x70
    TransferDataSuspended,                   ///< 0x71
    GeneralProgrammingFailure,               ///< 0x72
    WrongBlockSequenceCounter,               ///< 0x73
    ResponsePending,                         ///< 0x78
    SubFunctionNotSupportedInActiveSession,  ///< 0x7E
    ServiceNotSupportedInActiveSession,      ///< 0x7F
    Any
};

/// @brief UDS session type for expectations
enum class UdsSessionTypeExpect {
    DefaultSession,                 ///< 0x01
    ProgrammingSession,             ///< 0x02
    ExtendedDiagnosticSession,      ///< 0x03
    SafetySystemDiagnosticSession,  ///< 0x04
    Any
};

/// @brief UDS expectations
struct UdsExpect {
    std::optional<UdsServiceTypeExpect> service;       ///< Service type (0x10, 0x22, etc.)
    std::optional<bool> is_request;                    ///< true=request, false=response
    std::optional<bool> is_negative_response;          ///< Check for negative response (0x7F)
    std::optional<UdsNRCExpect> nrc;                   ///< Negative response code
    std::optional<UdsSessionTypeExpect> session_type;  ///< Session type for 0x10
    std::optional<std::vector<std::uint16_t>> data_identifiers;  ///< DIDs for 0x22/0x2E
    std::optional<std::uint16_t> routine_id;                     ///< Routine ID for 0x31
    std::optional<std::uint8_t> security_level;                  ///< Security level for 0x27
    std::optional<std::uint8_t> reset_type;                      ///< Reset type for 0x11
};

/// @brief Payload content expectations
struct PayloadExpect {
    std::optional<std::vector<std::uint8_t>> contains;  ///< Payload contains bytes
    std::optional<std::vector<std::uint8_t>> equals;    ///< Payload equals bytes
    std::optional<std::size_t> min_size;                ///< Minimum payload size
    std::optional<std::size_t> max_size;                ///< Maximum payload size
};

// =============================================================================
// Scenario Steps
// =============================================================================

/// @brief Capture step - start capturing packets
struct CaptureStep {
    CaptureConfig config;
};

/// @brief Send step - inject a packet
struct SendStep {
    std::string interface;                      ///< Interface to send on
    std::optional<std::string> pcap_file;       ///< Send packet from pcap
    std::optional<std::vector<std::uint8_t>> raw_data; ///< Raw packet bytes
    Duration delay{0};                          ///< Delay before sending
};

/// @brief Wait step - pause execution
struct WaitStep {
    Duration duration;
};

/// @brief Expect step - assert on received packets
struct ExpectStep {
    // Protocol expectations (all optional, checked if present)
    std::optional<EthernetExpect> ethernet;
    std::optional<IPv4Expect> ipv4;
    std::optional<UDPExpect> udp;
    std::optional<TCPExpect> tcp;
    std::optional<SomeIpExpect> someip;
    std::optional<SomeIpSdExpect> someip_sd;
    std::optional<DoIpExpect> doip;
    std::optional<UdsExpect> uds;
    std::optional<PayloadExpect> payload;

    // Timing and count constraints
    Duration within{1000};                      ///< Must match within this time
    CountExpression count{CompareOp::GreaterEqual, 1}; ///< Expected packet count

    // Step metadata
    std::string description;                    ///< Human-readable description
    bool required{true};                        ///< Fail scenario if not met
};

/// @brief Log step - output a message
struct LogStep {
    std::string message;
    std::string level{"info"};                  ///< info, warn, error, debug
};

/// @brief Variant of all step types
using Step = std::variant<
    CaptureStep,
    SendStep,
    WaitStep,
    ExpectStep,
    LogStep
>;

// =============================================================================
// Scenario Definition
// =============================================================================

/// @brief Complete test scenario
struct Scenario {
    std::string name;                           ///< Scenario name
    std::string description;                    ///< Scenario description
    std::string version{"1.0"};                 ///< Scenario format version
    std::vector<std::string> tags;              ///< Tags for filtering
    Duration timeout{30000};                    ///< Overall scenario timeout
    std::vector<Step> steps;                    ///< Ordered list of steps

    /// @brief Check if scenario has any expect steps
    [[nodiscard]] bool has_expectations() const;

    /// @brief Get all expect steps
    [[nodiscard]] std::vector<const ExpectStep*> get_expectations() const;
};

// =============================================================================
// Scenario Result
// =============================================================================

/// @brief Result of a single expectation check
struct ExpectResult {
    std::string description;
    bool passed{false};
    std::string failure_reason;
    std::size_t packets_matched{0};
    Duration elapsed{0};
};

/// @brief Result of running a scenario
struct ScenarioResult {
    std::string scenario_name;
    bool passed{false};
    std::vector<ExpectResult> expect_results;
    Duration total_elapsed{0};
    std::string error_message;                  ///< Set if scenario failed to run
    std::optional<std::string> pcap_file;       ///< Saved pcap on failure

    /// @brief Get count of passed expectations
    [[nodiscard]] std::size_t passed_count() const;

    /// @brief Get count of failed expectations
    [[nodiscard]] std::size_t failed_count() const;

    /// @brief Get total expectation count
    [[nodiscard]] std::size_t total_count() const;
};

// =============================================================================
// Implementation
// =============================================================================

inline std::optional<CountExpression> CountExpression::parse(std::string_view str) {
    CountExpression result;
    
    // Trim whitespace
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    
    if (str.empty()) {
        return std::nullopt;
    }
    
    // Check for operator prefix
    if (str.starts_with(">=")) {
        result.op = CompareOp::GreaterEqual;
        str.remove_prefix(2);
    } else if (str.starts_with("<=")) {
        result.op = CompareOp::LessEqual;
        str.remove_prefix(2);
    } else if (str.starts_with("==")) {
        result.op = CompareOp::Equal;
        str.remove_prefix(2);
    } else if (str.starts_with("!=")) {
        result.op = CompareOp::NotEqual;
        str.remove_prefix(2);
    } else if (str.starts_with(">")) {
        result.op = CompareOp::GreaterThan;
        str.remove_prefix(1);
    } else if (str.starts_with("<")) {
        result.op = CompareOp::LessThan;
        str.remove_prefix(1);
    } else {
        // No operator, assume >= (at least N)
        result.op = CompareOp::GreaterEqual;
    }
    
    // Trim whitespace after operator
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    
    // Parse number
    std::size_t value = 0;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            value = value * 10 + static_cast<std::size_t>(c - '0');
        } else {
            return std::nullopt;  // Invalid character
        }
    }
    
    result.value = value;
    return result;
}

inline bool CountExpression::evaluate(std::size_t count) const {
    switch (op) {
        case CompareOp::Equal:        return count == value;
        case CompareOp::NotEqual:     return count != value;
        case CompareOp::GreaterThan:  return count > value;
        case CompareOp::GreaterEqual: return count >= value;
        case CompareOp::LessThan:     return count < value;
        case CompareOp::LessEqual:    return count <= value;
    }
    return false;
}

inline bool Scenario::has_expectations() const {
    for (const auto& step : steps) {
        if (std::holds_alternative<ExpectStep>(step)) {
            return true;
        }
    }
    return false;
}

inline std::vector<const ExpectStep*> Scenario::get_expectations() const {
    std::vector<const ExpectStep*> result;
    for (const auto& step : steps) {
        if (auto* expect = std::get_if<ExpectStep>(&step)) {
            result.push_back(expect);
        }
    }
    return result;
}

inline std::size_t ScenarioResult::passed_count() const {
    std::size_t count = 0;
    for (const auto& r : expect_results) {
        if (r.passed) ++count;
    }
    return count;
}

inline std::size_t ScenarioResult::failed_count() const {
    return expect_results.size() - passed_count();
}

inline std::size_t ScenarioResult::total_count() const {
    return expect_results.size();
}

}  // namespace wadjet::scenario

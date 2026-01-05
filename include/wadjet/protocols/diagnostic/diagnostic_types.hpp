#pragma once

/// @file diagnostic_types.hpp
/// @brief Common types for diagnostic session management
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wadjet::protocols::diagnostic {

// =============================================================================
// Address Types
// =============================================================================

/// @brief Logical diagnostic address (16-bit as per ISO 13400)
using LogicalAddress = std::uint16_t;

/// @brief Special addresses
namespace address {
/// @brief Broadcast address for vehicle identification
inline constexpr LogicalAddress BROADCAST = 0xFFFF;

/// @brief Functional group address (all ECUs)
inline constexpr LogicalAddress FUNCTIONAL_GROUP = 0xDF00;

/// @brief Typical external tester address range start
inline constexpr LogicalAddress EXTERNAL_TESTER_START = 0x0E00;

/// @brief Typical ECU address range start
inline constexpr LogicalAddress ECU_START = 0x0001;
}  // namespace address

// =============================================================================
// ECU Identification
// =============================================================================

/// @brief VIN (Vehicle Identification Number)
struct VIN {
    std::array<char, 17> data{};

    [[nodiscard]] std::string to_string() const {
        return std::string(data.data(), 17);
    }

    [[nodiscard]] bool is_valid() const {
        // VIN should contain only alphanumeric characters (excluding I, O, Q)
        for (char c : data) {
            if (c == '\0') return false;
            if (!std::isalnum(static_cast<unsigned char>(c))) return false;
            if (c == 'I' || c == 'O' || c == 'Q') return false;
        }
        return true;
    }
};

/// @brief ECU information collected during diagnostic session
struct ECUInfo {
    LogicalAddress logical_address{0};          ///< DoIP logical address
    std::optional<VIN> vin;                     ///< Vehicle Identification Number
    std::optional<std::string> hardware_id;     ///< Hardware part number
    std::optional<std::string> software_id;     ///< Software version
    std::optional<std::string> supplier_id;     ///< Supplier identifier
    std::optional<std::string> ecu_name;        ///< ECU name/description
    std::vector<std::uint8_t> serial_number;    ///< ECU serial number

    /// @brief Check if basic identification is available
    [[nodiscard]] bool has_identification() const {
        return hardware_id.has_value() || software_id.has_value();
    }
};

// =============================================================================
// Diagnostic Timing
// =============================================================================

/// @brief Standard diagnostic timing parameters from ISO 14229
struct DiagnosticTiming {
    /// @brief P2 Server Max - Time for initial response (default 50ms)
    std::chrono::milliseconds p2_server_max{50};

    /// @brief P2* Server Max - Time after ResponsePending (default 5000ms)
    std::chrono::milliseconds p2_star_server_max{5000};

    /// @brief S3 Server - Session keep-alive timeout (default 5000ms)
    std::chrono::milliseconds s3_server{5000};

    /// @brief P3 Client - Time between consecutive requests (default 50ms)
    std::chrono::milliseconds p3_client{50};

    /// @brief P4 Server - Inter-byte timing (not used in IP-based transport)
    std::chrono::milliseconds p4_server{0};

    /// @brief Default timing values
    [[nodiscard]] static DiagnosticTiming defaults() {
        return DiagnosticTiming{};
    }

    /// @brief Programming session timing (typically same as default for DoIP)
    [[nodiscard]] static DiagnosticTiming programming() {
        DiagnosticTiming t;
        t.p2_star_server_max = std::chrono::milliseconds{5000};
        return t;
    }

    /// @brief Check if a response arrived within P2 timeout
    [[nodiscard]] bool within_p2(std::chrono::milliseconds elapsed) const {
        return elapsed <= p2_server_max;
    }

    /// @brief Check if a response arrived within P2* timeout
    [[nodiscard]] bool within_p2_star(std::chrono::milliseconds elapsed) const {
        return elapsed <= p2_star_server_max;
    }
};

// =============================================================================
// Diagnostic Message Direction
// =============================================================================

/// @brief Message direction in diagnostic communication
enum class MessageDirection {
    Request,   ///< Tester → ECU
    Response,  ///< ECU → Tester
    Unknown,   ///< Direction cannot be determined
};

/// @brief Convert direction to string
[[nodiscard]] inline std::string_view direction_string(MessageDirection dir) {
    switch (dir) {
        case MessageDirection::Request: return "Request";
        case MessageDirection::Response: return "Response";
        case MessageDirection::Unknown: return "Unknown";
    }
    return "Unknown";
}

// =============================================================================
// Transport Information
// =============================================================================

/// @brief Transport layer type for diagnostic messages
enum class TransportType {
    DoIP,      ///< Diagnostics over IP (ISO 13400)
    ISO_TP,    ///< ISO 15765-2 over CAN
    Raw_UDP,   ///< Raw UDP transport
    Raw_TCP,   ///< Raw TCP transport
    Unknown,
};

/// @brief Transport layer information
struct TransportInfo {
    TransportType type{TransportType::Unknown};
    LogicalAddress source_address{0};
    LogicalAddress target_address{0};

    /// @brief Timestamp when message was received
    std::chrono::steady_clock::time_point timestamp{};

    /// @brief Optional sequence number (for correlation)
    std::optional<std::uint32_t> sequence_number;
};

}  // namespace wadjet::protocols::diagnostic

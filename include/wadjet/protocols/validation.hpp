#pragma once

/// @file validation.hpp
/// @brief Cross-protocol validation framework for multi-layer packet analysis
///
/// Provides validation of protocol layering, length consistency, and checksums
/// across multiple protocol layers (Ethernet → IPv4 → UDP/TCP → Application).

#include "wadjet/protocols/decoder.hpp"

#include <memory>
#include <string>
#include <vector>

namespace wadjet::protocols {

/// @brief Error handling modes for protocol validation
enum class ValidationMode {
    Strict,   ///< Fail on first validation error
    Lenient,  ///< Log warnings but continue validation
};

/// @brief Validation result containing all errors found
struct ValidationResult {
    bool is_valid;                    ///< True if all validations passed
    ValidationMode mode;              ///< Mode used for validation
    std::vector<DecodeError> errors;  ///< All validation errors found

    ValidationResult() : is_valid(true), mode(ValidationMode::Strict) {}

    explicit ValidationResult(ValidationMode m) : is_valid(true), mode(m) {}

    /// @brief Add an error to the result
    /// @param code Error code
    /// @param offset Byte offset where error occurred
    /// @param message Optional error message
    void add_error(DecodeErrorCode code, std::size_t offset, const std::string& message = "") {
        is_valid = false;
        errors.emplace_back(code, offset, message);
    }

    /// @brief Get error count
    [[nodiscard]] std::size_t error_count() const { return errors.size(); }

    /// @brief Check if any errors exist
    [[nodiscard]] explicit operator bool() const { return is_valid; }
};

/// @brief Protocol layer information for validation
struct ProtocolLayer {
    std::string name;            ///< Layer name (e.g., "IPv4", "TCP")
    std::size_t offset;          ///< Offset in packet where layer starts
    std::size_t header_length;   ///< Length of this layer's header
    std::size_t payload_length;  ///< Length of payload carried by this layer
    std::uint16_t ethertype;     ///< EtherType or protocol number (optional)
    std::uint16_t checksum;      ///< Checksum value (if applicable, 0 if none)
    bool has_checksum;           ///< Whether this layer has a checksum field
};

/// @brief Cross-protocol validator for packet analysis
///
/// Validates:
/// - Protocol stack layering (consistent protocol progression)
/// - Length consistency across layers (header + payload = total)
/// - Checksums (IPv4, UDP, TCP with pseudo-header support)
class ProtocolValidator {
public:
    /// @brief Create a validator with the specified mode
    /// @param mode Validation mode (strict or lenient)
    explicit ProtocolValidator(ValidationMode mode = ValidationMode::Strict);

    ~ProtocolValidator() = default;

    // Non-copyable, non-movable for now
    ProtocolValidator(const ProtocolValidator&) = delete;
    ProtocolValidator& operator=(const ProtocolValidator&) = delete;
    ProtocolValidator(ProtocolValidator&&) = delete;
    ProtocolValidator& operator=(ProtocolValidator&&) = delete;

    /// @brief Validate protocol stack layering integrity
    ///
    /// Checks:
    /// - Valid protocol progression (Ethernet → IPv4/IPv6 → UDP/TCP → Application)
    /// - No unsupported protocol sequences
    /// - Proper frame type matching (EtherType, IP protocol field)
    ///
    /// @param layers Vector of protocol layers in order
    /// @return Validation result with any errors found
    [[nodiscard]] ValidationResult validateLayering(const std::vector<ProtocolLayer>& layers) const;

    /// @brief Validate length consistency across layers
    ///
    /// Checks:
    /// - Each layer's header + payload equals declared length
    /// - No gaps or overlaps between layers
    /// - Packet total length matches Ethernet frame minus FCS
    /// - IPv4/IPv6 total length matches actual data
    ///
    /// @param layers Vector of protocol layers in order
    /// @param total_packet_length Total length of received packet (including headers)
    /// @return Validation result with any errors found
    [[nodiscard]] ValidationResult validateLengths(const std::vector<ProtocolLayer>& layers,
                                                   std::size_t total_packet_length) const;

    /// @brief Validate checksums across protocol layers
    ///
    /// Checks:
    /// - IPv4 header checksum
    /// - UDP checksum (with IPv4 pseudo-header)
    /// - TCP checksum (with IPv4 pseudo-header)
    /// - Optional: IPv6 checksum (future)
    ///
    /// @param packet_data Complete packet data
    /// @param layers Vector of protocol layers with checksum information
    /// @return Validation result with any checksum errors found
    [[nodiscard]] ValidationResult validateChecksums(
        const std::span<const std::byte>& packet_data,
        const std::vector<ProtocolLayer>& layers) const;

    /// @brief Set validation mode
    /// @param mode New validation mode
    void set_mode(ValidationMode mode) { mode_ = mode; }

    /// @brief Get current validation mode
    [[nodiscard]] ValidationMode get_mode() const { return mode_; }

private:
    ValidationMode mode_;

    /// @brief Validate IPv4 checksum
    [[nodiscard]] bool validate_ipv4_checksum(const std::span<const std::byte>& packet_data,
                                              const ProtocolLayer& layer) const;

    /// @brief Validate UDP checksum
    [[nodiscard]] bool validate_udp_checksum(const std::span<const std::byte>& packet_data,
                                             const ProtocolLayer& udp_layer,
                                             const ProtocolLayer* ipv4_layer) const;

    /// @brief Validate TCP checksum
    [[nodiscard]] bool validate_tcp_checksum(const std::span<const std::byte>& packet_data,
                                             const ProtocolLayer& tcp_layer,
                                             const ProtocolLayer* ipv4_layer) const;

    /// @brief Calculate IPv4 checksum from bytes
    [[nodiscard]] static std::uint16_t calculate_ipv4_checksum(
        const std::span<const std::byte>& header_data);

    /// @brief Calculate pseudo-header checksum for UDP/TCP
    [[nodiscard]] static std::uint16_t calculate_pseudo_checksum(
        const std::span<const std::byte>& packet_data, const ProtocolLayer& ipv4_layer,
        std::uint8_t protocol_number, std::size_t transport_length);
};

}  // namespace wadjet::protocols

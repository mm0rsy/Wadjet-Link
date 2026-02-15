#pragma once

#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/protocols/uds/uds_types.hpp"

#include <chrono>
#include <format>
#include <optional>
#include <string>

namespace wadjet::distributed {

/**
 * @brief UDS diagnostic response expectation assertion
 *
 * T316: Validates UDS request→response across multiple nodes
 *
 * ExpectDiagnosticResponse validates that a UDS diagnostic request sent from
 * a source node receives a corresponding response on a destination node within
 * the specified timeout period.
 *
 * This matcher integrates with M9 UDS decoder to identify request/response pairs
 * and M8 gPTP timestamps to measure round-trip time.
 *
 * @example
 * ```cpp
 * auto response_matcher = ExpectDiagnosticResponse(
 *     "ecu_a",  // Source node sending request
 *     "ecu_b",  // Destination node receiving response
 *     protocols::uds::ServiceID::ReadDataByIdentifier
 * );
 * response_matcher.evaluate(context_a, context_b);
 * ```
 */
class ExpectDiagnosticResponse : public DistributedMatcher {
public:
    /**
     * @brief Create expectation for UDS diagnostic response
     *
     * @param src_node Node ID sending the diagnostic request
     * @param dst_node Node ID sending the diagnostic response
     * @param service_id Expected UDS service ID (e.g., ReadDataByIdentifier)
     * @param timeout_ms Maximum time to wait for response
     */
    ExpectDiagnosticResponse(const std::string& src_node, const std::string& dst_node,
                             protocols::uds::ServiceID service_id,
                             std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{
                                 5000});

    /**
     * @brief Create expectation for any UDS diagnostic response
     *
     * @param src_node Node ID sending the diagnostic request
     * @param dst_node Node ID sending the diagnostic response
     * @param timeout_ms Maximum time to wait for response
     */
    ExpectDiagnosticResponse(const std::string& src_node, const std::string& dst_node,
                             std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{
                                 5000});

    /**
     * @brief Evaluate the diagnostic response expectation
     *
     * Checks for:
     * 1. UDS request packet on source node with specified service ID
     * 2. Corresponding UDS response packet on destination node
     * 3. Response arrived within timeout period
     * 4. Response is positive (not a negative response code)
     *
     * @param src_context Capture context from source node
     * @param dst_context Capture context from destination node
     * @return Match result with timing and packet information
     */
    auto evaluate(const DistributedCaptureContext& src_context,
                  const DistributedCaptureContext& dst_context) const
        -> DistributedMatchResult override;

    /**
     * @brief Get the source node ID
     */
    [[nodiscard]] const std::string& source_node() const { return src_node_; }

    /**
     * @brief Get the destination node ID
     */
    [[nodiscard]] const std::string& destination_node() const { return dst_node_; }

    /**
     * @brief Get the expected UDS service ID
     */
    [[nodiscard]] std::optional<protocols::uds::ServiceID> service_id() const {
        return service_id_;
    }

    /**
     * @brief Get the timeout value
     */
    [[nodiscard]] std::chrono::milliseconds timeout() const { return timeout_ms_; }

private:
    std::string src_node_;
    std::string dst_node_;
    std::optional<protocols::uds::ServiceID> service_id_;
    std::chrono::milliseconds timeout_ms_;
};

/**
 * @brief Create ExpectDiagnosticResponse matcher with optional GoogleTest filtering
 *
 * T316: Factory function for creating UDS diagnostic response matchers
 *
 * @param src_node Source node ID
 * @param dst_node Destination node ID
 * @param service_id Expected UDS service ID
 * @param timeout_ms Timeout for response
 * @return ExpectDiagnosticResponse matcher instance
 */
[[nodiscard]] std::unique_ptr<DistributedMatcher> ExpectDiagnosticResponse(
    const std::string& src_node, const std::string& dst_node, protocols::uds::ServiceID service_id,
    std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000});

/**
 * @brief Create ExpectDiagnosticResponse matcher for any UDS service
 *
 * @param src_node Source node ID
 * @param dst_node Destination node ID
 * @param timeout_ms Timeout for response
 * @return ExpectDiagnosticResponse matcher instance
 */
[[nodiscard]] std::unique_ptr<DistributedMatcher> ExpectDiagnosticResponse(
    const std::string& src_node, const std::string& dst_node,
    std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000});

/**
 * @brief Multi-ECU diagnostic session sequence expectation
 *
 * T319: Validates diagnostic sequences across multiple ECUs
 *
 * ExpectDiagnosticSession validates multi-step diagnostic operations across
 * multiple ECUs, such as:
 * - Security access unlock on one ECU followed by flash download
 * - DTCs cleared on one ECU and verified as cleared on another
 * - Diagnostic session transitions in coordinated manner
 *
 * @example
 * ```cpp
 * // Validate SecurityAccess on ECU-A then FlashDownload on ECU-B
 * auto session_matcher = ExpectDiagnosticSession({
 *     {"ecu_a", protocols::uds::ServiceID::SecurityAccess},
 *     {"ecu_b", protocols::uds::ServiceID::RequestDownload}
 * });
 * session_matcher.evaluate(contexts);
 * ```
 */
class ExpectDiagnosticSession : public DistributedMatcher {
public:
    /**
     * @brief Diagnostic step in a session sequence
     *
     * Represents one operation within a multi-step diagnostic session
     */
    struct SessionStep {
        std::string node_id;                         ///< Node/ECU ID
        protocols::uds::ServiceID service_id;        ///< Expected UDS service
        std::chrono::milliseconds timeout_ms{5000};  ///< Timeout for this step
    };

    /**
     * @brief Create expectation for diagnostic session sequence
     *
     * @param steps Vector of diagnostic steps that should occur in order
     * @param total_timeout_ms Total timeout for entire sequence
     */
    ExpectDiagnosticSession(const std::vector<SessionStep>& steps,
                            std::chrono::milliseconds total_timeout_ms = std::chrono::milliseconds{
                                30000});

    /**
     * @brief Evaluate the diagnostic session expectation
     *
     * Checks for:
     * 1. First step service occurs on first node
     * 2. Second step service occurs on second node
     * 3. Services occur in the expected order (causality)
     * 4. All services occur within their individual timeouts
     * 5. Entire sequence completes within total timeout
     *
     * @param contexts Map of node_id to capture contexts
     * @return Match result with timing and sequence information
     */
    auto evaluate(const std::unordered_map<std::string, DistributedCaptureContext>& contexts) const
        -> DistributedMatchResult override;

    /**
     * @brief Get human-readable description of this matcher
     */
    auto describe() const -> std::string override {
        std::string desc = "ExpectDiagnosticSession(";
        for (size_t i = 0; i < steps_.size(); ++i) {
            if (i > 0)
                desc += " -> ";
            desc += std::format("{}:0x{:02X}", steps_[i].node_id,
                                static_cast<uint8_t>(steps_[i].service_id));
        }
        desc += ")";
        return desc;
    }

    /**
     * @brief Clone this matcher
     */
    auto clone() const -> std::unique_ptr<DistributedMatcher> override {
        return std::make_unique<ExpectDiagnosticSession>(steps_, total_timeout_ms_);
    }

    /**
     * @brief Get the diagnostic session steps
     */
    [[nodiscard]] const std::vector<SessionStep>& steps() const { return steps_; }

    /**
     * @brief Get the total timeout value
     */
    [[nodiscard]] std::chrono::milliseconds total_timeout() const { return total_timeout_ms_; }

private:
    std::vector<SessionStep> steps_;
    std::chrono::milliseconds total_timeout_ms_;
};

/**
 * @brief Create ExpectDiagnosticSession matcher for multi-ECU diagnostic sequences
 *
 * T319: Factory function for creating diagnostic session matchers
 *
 * @param steps Vector of diagnostic steps to validate
 * @param total_timeout_ms Total timeout for entire sequence
 * @return ExpectDiagnosticSession matcher instance
 */
[[nodiscard]] std::unique_ptr<DistributedMatcher> ExpectDiagnosticSession(
    const std::vector<ExpectDiagnosticSession::SessionStep>& steps,
    std::chrono::milliseconds total_timeout_ms = std::chrono::milliseconds{30000});

}  // namespace wadjet::distributed

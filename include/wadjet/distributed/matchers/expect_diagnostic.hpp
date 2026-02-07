#pragma once

#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/protocols/uds/uds_types.hpp"

#include <string>
#include <optional>
#include <chrono>

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
    ExpectDiagnosticResponse(
        const std::string& src_node,
        const std::string& dst_node,
        protocols::uds::ServiceID service_id,
        std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000}
    );
    
    /**
     * @brief Create expectation for any UDS diagnostic response
     * 
     * @param src_node Node ID sending the diagnostic request
     * @param dst_node Node ID sending the diagnostic response
     * @param timeout_ms Maximum time to wait for response
     */
    ExpectDiagnosticResponse(
        const std::string& src_node,
        const std::string& dst_node,
        std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000}
    );
    
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
    const std::string& src_node,
    const std::string& dst_node,
    protocols::uds::ServiceID service_id,
    std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000}
);

/**
 * @brief Create ExpectDiagnosticResponse matcher for any UDS service
 * 
 * @param src_node Source node ID
 * @param dst_node Destination node ID
 * @param timeout_ms Timeout for response
 * @return ExpectDiagnosticResponse matcher instance
 */
[[nodiscard]] std::unique_ptr<DistributedMatcher> ExpectDiagnosticResponse(
    const std::string& src_node,
    const std::string& dst_node,
    std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000}
);

}  // namespace wadjet::distributed

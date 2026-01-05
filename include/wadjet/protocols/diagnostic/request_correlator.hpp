#pragma once

/// @file request_correlator.hpp
/// @brief UDS request/response correlation for diagnostic sessions
///
/// Correlates UDS requests with their responses based on service ID,
/// source/target addresses, and timing. Handles ResponsePending (NRC 0x78)
/// and multi-frame responses.
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_types.hpp"
#include "wadjet/protocols/uds/uds.hpp"
#include "wadjet/protocols/uds/uds_nrc.hpp"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <variant>

namespace wadjet::protocols::diagnostic {

// =============================================================================
// Request/Response Pair
// =============================================================================

/// @brief A correlated request/response pair
struct RequestResponsePair {
    /// @brief The original request
    struct Request {
        uds::UdsHeader header;
        uds::UdsServiceMessage message;
        TransportInfo transport;
        std::chrono::steady_clock::time_point timestamp;
    } request;

    /// @brief Response information
    struct Response {
        uds::UdsHeader header;
        uds::UdsServiceMessage message;
        TransportInfo transport;
        std::chrono::steady_clock::time_point timestamp;

        /// @brief Number of ResponsePending (0x78) received before final response
        std::uint32_t pending_count{0};

        /// @brief Total time from request to final response
        [[nodiscard]] std::chrono::milliseconds response_time(
            const std::chrono::steady_clock::time_point& request_time) const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - request_time);
        }
    };

    std::optional<Response> response;

    /// @brief Check if response has been received
    [[nodiscard]] bool is_complete() const { return response.has_value(); }

    /// @brief Check if response is positive
    [[nodiscard]] bool is_positive() const {
        return response && !response->header.is_negative_response();
    }

    /// @brief Check if response is negative
    [[nodiscard]] bool is_negative() const {
        return response && response->header.is_negative_response();
    }

    /// @brief Get negative response code if applicable
    [[nodiscard]] std::optional<std::uint8_t> get_nrc() const {
        if (is_negative()) {
            return response->header.negative_response_code;
        }
        return std::nullopt;
    }

    /// @brief Get response time in milliseconds
    [[nodiscard]] std::optional<std::chrono::milliseconds> response_time() const {
        if (response) {
            return response->response_time(request.timestamp);
        }
        return std::nullopt;
    }

    /// @brief Check if response arrived within P2 timeout
    [[nodiscard]] bool within_p2(const DiagnosticTiming& timing) const {
        if (auto rt = response_time()) {
            return timing.within_p2(*rt);
        }
        return false;
    }

    /// @brief Check if response arrived within P2* timeout (after pending)
    [[nodiscard]] bool within_p2_star(const DiagnosticTiming& timing) const {
        if (auto rt = response_time()) {
            return timing.within_p2_star(*rt);
        }
        return false;
    }
};

// =============================================================================
// Pending Request
// =============================================================================

/// @brief A request waiting for a response
struct PendingRequest {
    RequestResponsePair::Request request;
    std::uint32_t pending_count{0};  ///< Number of ResponsePending received
    bool suppress_positive_response{false};

    /// @brief Time since request was sent
    [[nodiscard]] std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - request.timestamp);
    }

    /// @brief Check if request has timed out
    [[nodiscard]] bool is_timed_out(const DiagnosticTiming& timing) const {
        auto e = elapsed();
        if (pending_count > 0) {
            return e > timing.p2_star_server_max;
        }
        return e > timing.p2_server_max;
    }
};

// =============================================================================
// Correlation Events
// =============================================================================

/// @brief Events emitted by the correlator
enum class CorrelationEvent {
    RequestSent,        ///< New request recorded
    ResponseReceived,   ///< Response matched to request
    ResponsePending,    ///< ResponsePending (0x78) received
    RequestTimeout,     ///< Request timed out without response
    UnmatchedResponse,  ///< Response without matching request
    PositiveResponse,   ///< Positive response received
    NegativeResponse,   ///< Negative response received
};

/// @brief Callback for correlation events
using CorrelationCallback = std::function<void(
    CorrelationEvent event, const RequestResponsePair* pair, const PendingRequest* pending)>;

// =============================================================================
// Request Correlator
// =============================================================================

/// @brief Correlates UDS requests with responses
///
/// The correlator tracks outstanding requests and matches them with
/// incoming responses based on:
/// - Service ID (request SID matches response SID - 0x40)
/// - Source/target address pair (responses have reversed addresses)
/// - Timing constraints
///
/// @example
/// ```cpp
/// RequestCorrelator correlator;
///
/// // Record outgoing request
/// correlator.record_request(uds_header, uds_message, transport_info);
///
/// // Process incoming response
/// auto pair = correlator.process_response(uds_header, uds_message, transport_info);
/// if (pair && pair->is_complete()) {
///     std::cout << "Response time: " << pair->response_time()->count() << "ms\n";
/// }
/// ```
class RequestCorrelator {
public:
    /// @brief Configuration options
    struct Options {
        /// @brief Maximum pending requests per address pair
        std::size_t max_pending_per_address = 100;

        /// @brief Maximum total pending requests
        std::size_t max_total_pending = 1000;

        /// @brief Enable automatic timeout checking
        bool auto_timeout_check = true;

        /// @brief Default timing parameters
        DiagnosticTiming timing{};

        /// @brief Get default options
        static Options defaults() {
            Options opts;
            opts.timing = DiagnosticTiming::defaults();
            return opts;
        }
    };

    explicit RequestCorrelator(Options opts = Options::defaults());

    // =========================================================================
    // Request Recording
    // =========================================================================

    /// @brief Record an outgoing UDS request
    /// @param header Decoded UDS header
    /// @param message Decoded UDS message
    /// @param transport Transport layer information
    void record_request(const uds::UdsHeader& header, const uds::UdsServiceMessage& message,
                        const TransportInfo& transport);

    // =========================================================================
    // Response Processing
    // =========================================================================

    /// @brief Process an incoming UDS response
    /// @param header Decoded UDS header
    /// @param message Decoded UDS message
    /// @param transport Transport layer information
    /// @return Correlated pair if matched, nullptr if no match
    std::shared_ptr<RequestResponsePair> process_response(const uds::UdsHeader& header,
                                                          const uds::UdsServiceMessage& message,
                                                          const TransportInfo& transport);

    // =========================================================================
    // Query
    // =========================================================================

    /// @brief Get pending request count for an address pair
    [[nodiscard]] std::size_t pending_count(LogicalAddress source, LogicalAddress target) const;

    /// @brief Get total pending request count
    [[nodiscard]] std::size_t total_pending() const;

    /// @brief Get all pending requests for an address pair
    [[nodiscard]] std::vector<PendingRequest> get_pending(LogicalAddress source,
                                                          LogicalAddress target) const;

    /// @brief Get completed pairs (limited history)
    [[nodiscard]] std::vector<std::shared_ptr<RequestResponsePair>> get_completed(
        std::size_t max_count = 100) const;

    /// @brief Get all completed pairs for a specific service
    [[nodiscard]] std::vector<std::shared_ptr<RequestResponsePair>> get_completed_by_service(
        uds::ServiceID service, std::size_t max_count = 100) const;

    // =========================================================================
    // Timeout Management
    // =========================================================================

    /// @brief Check for timed-out requests
    /// @return Number of requests that timed out
    std::size_t check_timeouts();

    /// @brief Update timing parameters
    void set_timing(const DiagnosticTiming& timing);

    /// @brief Get current timing parameters
    [[nodiscard]] const DiagnosticTiming& timing() const { return options_.timing; }

    // =========================================================================
    // Event Handling
    // =========================================================================

    /// @brief Register event callback
    void on_event(CorrelationCallback callback);

    /// @brief Clear all callbacks
    void clear_callbacks();

    // =========================================================================
    // State Management
    // =========================================================================

    /// @brief Clear all pending requests
    void clear_pending();

    /// @brief Clear completed history
    void clear_history();

    /// @brief Reset correlator state
    void reset();

    // =========================================================================
    // Statistics
    // =========================================================================

    /// @brief Correlation statistics
    struct Statistics {
        std::uint64_t requests_recorded{0};
        std::uint64_t responses_matched{0};
        std::uint64_t responses_unmatched{0};
        std::uint64_t pending_timeouts{0};
        std::uint64_t positive_responses{0};
        std::uint64_t negative_responses{0};
        std::uint64_t response_pending_count{0};

        /// @brief Match rate (0.0 - 1.0)
        [[nodiscard]] double match_rate() const {
            if (requests_recorded == 0)
                return 0.0;
            return static_cast<double>(responses_matched) / static_cast<double>(requests_recorded);
        }
    };

    /// @brief Get statistics
    [[nodiscard]] Statistics statistics() const;

    /// @brief Reset statistics
    void reset_statistics();

private:
    /// @brief Key for address pair lookup
    struct AddressPairKey {
        LogicalAddress source;
        LogicalAddress target;

        bool operator==(const AddressPairKey& other) const {
            return source == other.source && target == other.target;
        }
    };

    struct AddressPairKeyHash {
        std::size_t operator()(const AddressPairKey& k) const {
            return std::hash<std::uint32_t>{}((static_cast<std::uint32_t>(k.source) << 16) |
                                              k.target);
        }
    };

    /// @brief Find matching pending request
    PendingRequest* find_pending_for_response(const uds::UdsHeader& header,
                                              const TransportInfo& transport);

    /// @brief Emit event to callbacks
    void emit_event(CorrelationEvent event, const RequestResponsePair* pair,
                    const PendingRequest* pending);

    Options options_;
    Statistics stats_;

    /// @brief Pending requests indexed by (source, target) address pair
    std::unordered_map<AddressPairKey, std::deque<PendingRequest>, AddressPairKeyHash>
        pending_requests_;

    /// @brief Completed request/response pairs (circular buffer)
    std::deque<std::shared_ptr<RequestResponsePair>> completed_;
    static constexpr std::size_t MAX_COMPLETED_HISTORY = 1000;

    std::vector<CorrelationCallback> callbacks_;
    mutable std::mutex mutex_;
};

}  // namespace wadjet::protocols::diagnostic

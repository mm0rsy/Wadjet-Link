#pragma once

/// @file diagnostic_matchers.hpp
/// @brief GoogleTest matchers for diagnostic session validation
///
/// Provides custom gMock matchers for validating diagnostic sessions,
/// request/response timing, security access sequences, and more.
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/diagnostic/diagnostic_session.hpp"
#include "wadjet/protocols/diagnostic/request_correlator.hpp"
#include "wadjet/protocols/uds/uds_nrc.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

namespace wadjet::testing::diagnostic {

using namespace wadjet::protocols::diagnostic;
using namespace wadjet::protocols::uds;

// =============================================================================
// Request/Response Pair Matchers
// =============================================================================

/// @brief Matches if the pair is complete (has response)
MATCHER(IsComplete, "is complete") {
    return arg.is_complete();
}

/// @brief Matches if the response is positive
MATCHER(IsPositiveResponse, "is positive response") {
    return arg.is_complete() && arg.is_positive();
}

/// @brief Matches if the response is negative
MATCHER(IsNegativeResponse, "is negative response") {
    return arg.is_complete() && arg.is_negative();
}

/// @brief Matches if the response has a specific NRC
MATCHER_P(HasNRC, expected_nrc, "has NRC " + std::string(nrc_string(expected_nrc))) {
    auto nrc = arg.get_nrc();
    return nrc.has_value() && *nrc == expected_nrc;
}

/// @brief Matches if response arrived within specified time
MATCHER_P(ArrivesWithin, max_time, 
          "arrives within " + std::to_string(max_time.count()) + "ms") {
    if (!arg.is_complete()) {
        return false;
    }
    auto rt = arg.response_time();
    return rt.has_value() && *rt <= max_time;
}

/// @brief Matches if response arrived within P2 timeout
MATCHER_P(WithinP2Timeout, timing, "within P2 timeout") {
    return arg.within_p2(timing);
}

/// @brief Matches if response arrived within P2* timeout
MATCHER_P(WithinP2StarTimeout, timing, "within P2* timeout") {
    return arg.within_p2_star(timing);
}

/// @brief Matches if there were pending responses (0x78)
MATCHER(HadPendingResponse, "had pending response") {
    return arg.is_complete() && arg.response->pending_count > 0;
}

/// @brief Matches if pending count equals expected
MATCHER_P(HasPendingCount, expected, 
          "has pending count " + std::to_string(expected)) {
    return arg.is_complete() && 
           arg.response->pending_count == static_cast<std::uint32_t>(expected);
}

/// @brief Matches request service ID
MATCHER_P(HasRequestService, service, 
          "has request service " + std::string(service_id_string(service))) {
    return arg.request.header.service_id == service;
}

/// @brief Matches if request was sent to specific address
MATCHER_P(SentToAddress, address, 
          "sent to address 0x" + std::to_string(address)) {
    return arg.request.transport.target_address == address;
}

// =============================================================================
// Session State Matchers
// =============================================================================

/// @brief Matches if session is active
MATCHER(IsSessionActive, "session is active") {
    return arg.session_active;
}

/// @brief Matches if routing is active
MATCHER(IsRoutingActive, "routing is active") {
    return arg.routing_active;
}

/// @brief Matches session type
MATCHER_P(HasSessionType, type, 
          "has session type " + std::to_string(static_cast<int>(type))) {
    return arg.session_type == type;
}

/// @brief Matches if in programming session
MATCHER(IsInProgrammingSession, "is in programming session") {
    return arg.is_programming();
}

/// @brief Matches if in extended session
MATCHER(IsInExtendedSession, "is in extended session") {
    return arg.is_extended();
}

/// @brief Matches security level
MATCHER_P(HasSecurityLevel, level, 
          "has security level " + std::to_string(level)) {
    return arg.security_level >= level;
}

/// @brief Matches if security is unlocked (any level)
MATCHER(IsSecurityUnlocked, "security is unlocked") {
    return arg.security_level > 0;
}

/// @brief Matches if session might be timed out
MATCHER(IsPotentiallyTimedOut, "is potentially timed out") {
    return arg.is_potentially_timed_out();
}

// =============================================================================
// Timing Validation Matchers
// =============================================================================

/// @brief Matches if timing P2 is within spec
MATCHER_P(HasP2Within, max_p2, 
          "has P2 within " + std::to_string(max_p2.count()) + "ms") {
    return arg.timing.p2_server_max <= max_p2;
}

/// @brief Matches if timing P2* is within spec
MATCHER_P(HasP2StarWithin, max_p2_star, 
          "has P2* within " + std::to_string(max_p2_star.count()) + "ms") {
    return arg.timing.p2_star_server_max <= max_p2_star;
}

/// @brief Matches if S3 timeout is configured
MATCHER_P(HasS3Timeout, s3_timeout, 
          "has S3 timeout " + std::to_string(s3_timeout.count()) + "ms") {
    return arg.timing.s3_server == s3_timeout;
}

// =============================================================================
// UDS Header Matchers
// =============================================================================

/// @brief Matches service ID
MATCHER_P(HasServiceId, sid, 
          "has service ID " + std::string(service_id_string(sid))) {
    return arg.service_id == sid;
}

/// @brief Matches if has sub-function
MATCHER(HasSubFunction, "has sub-function") {
    return arg.sub_function.has_value();
}

/// @brief Matches sub-function value
MATCHER_P(HasSubFunctionValue, value, 
          "has sub-function value 0x" + std::to_string(value)) {
    return arg.sub_function.has_value() && *arg.sub_function == value;
}

/// @brief Matches if suppress positive response is set
MATCHER(SuppressesPositiveResponse, "suppresses positive response") {
    return arg.suppress_positive_response;
}

// =============================================================================
// Correlator Statistics Matchers
// =============================================================================

/// @brief Matches if match rate is above threshold
MATCHER_P(HasMatchRateAbove, threshold, 
          "has match rate above " + std::to_string(threshold)) {
    return arg.match_rate() > threshold;
}

/// @brief Matches if no timeouts occurred
MATCHER(HasNoTimeouts, "has no timeouts") {
    return arg.pending_timeouts == 0;
}

/// @brief Matches if no negative responses
MATCHER(HasNoNegativeResponses, "has no negative responses") {
    return arg.negative_responses == 0;
}

// =============================================================================
// Assertion Macros
// =============================================================================

/// @brief Assert diagnostic session is in expected state
#define ASSERT_DIAGNOSTIC_SESSION(session_state, expected_type) \
    do { \
        ASSERT_TRUE((session_state).session_active) << "Session not active"; \
        ASSERT_EQ((session_state).session_type, expected_type) \
            << "Expected session type " << static_cast<int>(expected_type); \
    } while (0)

/// @brief Assert security access is at expected level
#define ASSERT_SECURITY_ACCESS(session_state, expected_level) \
    do { \
        ASSERT_GE((session_state).security_level, expected_level) \
            << "Security level " << static_cast<int>((session_state).security_level) \
            << " is less than expected " << static_cast<int>(expected_level); \
    } while (0)

/// @brief Assert response timing is valid
#define ASSERT_RESPONSE_TIMING(pair, timing) \
    do { \
        ASSERT_TRUE((pair).is_complete()) << "Response not received"; \
        auto rt = (pair).response_time(); \
        ASSERT_TRUE(rt.has_value()) << "Response time not available"; \
        if ((pair).response->pending_count > 0) { \
            ASSERT_LE(*rt, (timing).p2_star_server_max) \
                << "Response time " << rt->count() << "ms exceeds P2* timeout"; \
        } else { \
            ASSERT_LE(*rt, (timing).p2_server_max) \
                << "Response time " << rt->count() << "ms exceeds P2 timeout"; \
        } \
    } while (0)

/// @brief Assert no diagnostic errors
#define ASSERT_NO_DIAGNOSTIC_ERRORS(stats) \
    do { \
        ASSERT_EQ((stats).negative_responses, 0) \
            << "Expected no negative responses, got " << (stats).negative_responses; \
        ASSERT_EQ((stats).pending_timeouts, 0) \
            << "Expected no timeouts, got " << (stats).pending_timeouts; \
    } while (0)

/// @brief Expect response within specific timeout
#define EXPECT_RESPONSE_WITHIN(pair, timeout) \
    EXPECT_THAT(pair, ArrivesWithin(timeout))

/// @brief Expect positive response
#define EXPECT_POSITIVE_RESPONSE(pair) \
    EXPECT_THAT(pair, IsPositiveResponse())

/// @brief Expect negative response with specific NRC
#define EXPECT_NEGATIVE_RESPONSE_NRC(pair, nrc) \
    do { \
        EXPECT_THAT(pair, IsNegativeResponse()); \
        EXPECT_THAT(pair, HasNRC(nrc)); \
    } while (0)

}  // namespace wadjet::testing::diagnostic

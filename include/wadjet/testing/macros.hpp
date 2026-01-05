/// @file macros.hpp
/// @brief Convenience macros for Wadjet-Link packet testing
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// These macros provide shorthand for common packet assertions in tests.
///
/// Example:
/// @code
/// TEST_F(MySOMEIPTest, ServiceOffersWithin100ms) {
///     WADJET_EXPECT_PACKET(HasSOMEIPServiceId(0x1234), 100ms);
///     WADJET_ASSERT_SOMEIP_SERVICE(0x1234, 100ms);
/// }
/// @endcode

#pragma once

#include "wadjet/testing/matchers.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

/// Assert that a packet matches a given matcher
/// @param packet The packet to check
/// @param matcher gMock matcher
#define WADJET_ASSERT_PACKET_MATCHES(packet, matcher) ASSERT_THAT(packet, matcher)

/// Expect that a packet matches a given matcher
/// @param packet The packet to check
/// @param matcher gMock matcher
#define WADJET_EXPECT_PACKET_MATCHES(packet, matcher) EXPECT_THAT(packet, matcher)

/// Assert that a packet is received matching the given matcher within timeout
/// @param matcher gMock matcher for the packet
/// @param timeout std::chrono duration for timeout
/// @note Must be used inside a LiveCaptureTestFixture-derived test
#define WADJET_ASSERT_PACKET(matcher, timeout)                                                 \
    do {                                                                                       \
        auto __wadjet_pkt = wait_for_match(matcher, timeout);                                  \
        ASSERT_TRUE(__wadjet_pkt.has_value())                                                  \
            << "No packet matching " #matcher " received within "                              \
            << std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count() << "ms"; \
    } while (0)

/// Expect that a packet is received matching the given matcher within timeout
/// @param matcher gMock matcher for the packet
/// @param timeout std::chrono duration for timeout
/// @note Must be used inside a LiveCaptureTestFixture-derived test
#define WADJET_EXPECT_PACKET(matcher, timeout)                                                 \
    do {                                                                                       \
        auto __wadjet_pkt = wait_for_match(matcher, timeout);                                  \
        EXPECT_TRUE(__wadjet_pkt.has_value())                                                  \
            << "No packet matching " #matcher " received within "                              \
            << std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count() << "ms"; \
    } while (0)

/// Assert that no packet matching the given matcher is received within timeout
/// @param matcher gMock matcher for the packet
/// @param timeout std::chrono duration for timeout
/// @note Must be used inside a LiveCaptureTestFixture-derived test
#define WADJET_ASSERT_NO_PACKET(matcher, timeout)                  \
    do {                                                           \
        auto __wadjet_pkt = wait_for_match(matcher, timeout);      \
        ASSERT_FALSE(__wadjet_pkt.has_value())                     \
            << "Unexpected packet matching " #matcher " received"; \
    } while (0)

/// Expect that no packet matching the given matcher is received within timeout
/// @param matcher gMock matcher for the packet
/// @param timeout std::chrono duration for timeout
/// @note Must be used inside a LiveCaptureTestFixture-derived test
#define WADJET_EXPECT_NO_PACKET(matcher, timeout)                  \
    do {                                                           \
        auto __wadjet_pkt = wait_for_match(matcher, timeout);      \
        EXPECT_FALSE(__wadjet_pkt.has_value())                     \
            << "Unexpected packet matching " #matcher " received"; \
    } while (0)

//==============================================================================
// Protocol-Specific Macros
//==============================================================================

/// Assert that a SOME/IP packet with the given service ID is received
/// @param service_id Expected SOME/IP service ID
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_SOMEIP_SERVICE(service_id, timeout) \
    WADJET_ASSERT_PACKET(::wadjet::testing::HasSOMEIPServiceId(service_id), timeout)

/// Expect that a SOME/IP packet with the given service ID is received
/// @param service_id Expected SOME/IP service ID
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_SOMEIP_SERVICE(service_id, timeout) \
    WADJET_EXPECT_PACKET(::wadjet::testing::HasSOMEIPServiceId(service_id), timeout)

/// Assert that a SOME/IP request for the given service/method is received
/// @param service_id Expected SOME/IP service ID
/// @param method_id Expected SOME/IP method ID
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_SOMEIP_REQUEST(service_id, method_id, timeout)                         \
    WADJET_ASSERT_PACKET(::testing::AllOf(::wadjet::testing::HasSOMEIPServiceId(service_id), \
                                          ::wadjet::testing::HasSOMEIPMethodId(method_id),   \
                                          ::wadjet::testing::IsSOMEIPRequest()),             \
                         timeout)

/// Expect that a SOME/IP request for the given service/method is received
/// @param service_id Expected SOME/IP service ID
/// @param method_id Expected SOME/IP method ID
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_SOMEIP_REQUEST(service_id, method_id, timeout)                         \
    WADJET_EXPECT_PACKET(::testing::AllOf(::wadjet::testing::HasSOMEIPServiceId(service_id), \
                                          ::wadjet::testing::HasSOMEIPMethodId(method_id),   \
                                          ::wadjet::testing::IsSOMEIPRequest()),             \
                         timeout)

/// Assert that a SOME/IP response for the given service/method is received
/// @param service_id Expected SOME/IP service ID
/// @param method_id Expected SOME/IP method ID
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_SOMEIP_RESPONSE(service_id, method_id, timeout)                        \
    WADJET_ASSERT_PACKET(::testing::AllOf(::wadjet::testing::HasSOMEIPServiceId(service_id), \
                                          ::wadjet::testing::HasSOMEIPMethodId(method_id),   \
                                          ::wadjet::testing::IsSOMEIPResponse()),            \
                         timeout)

/// Expect that a SOME/IP response for the given service/method is received
/// @param service_id Expected SOME/IP service ID
/// @param method_id Expected SOME/IP method ID
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_SOMEIP_RESPONSE(service_id, method_id, timeout)                        \
    WADJET_EXPECT_PACKET(::testing::AllOf(::wadjet::testing::HasSOMEIPServiceId(service_id), \
                                          ::wadjet::testing::HasSOMEIPMethodId(method_id),   \
                                          ::wadjet::testing::IsSOMEIPResponse()),            \
                         timeout)

/// Assert that a SOME/IP notification for the given service is received
/// @param service_id Expected SOME/IP service ID
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_SOMEIP_NOTIFICATION(service_id, timeout)                               \
    WADJET_ASSERT_PACKET(::testing::AllOf(::wadjet::testing::HasSOMEIPServiceId(service_id), \
                                          ::wadjet::testing::IsSOMEIPNotification()),        \
                         timeout)

/// Expect that a SOME/IP notification for the given service is received
/// @param service_id Expected SOME/IP service ID
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_SOMEIP_NOTIFICATION(service_id, timeout)                               \
    WADJET_EXPECT_PACKET(::testing::AllOf(::wadjet::testing::HasSOMEIPServiceId(service_id), \
                                          ::wadjet::testing::IsSOMEIPNotification()),        \
                         timeout)

/// Assert that a DoIP routing activation request is received
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_DOIP_ROUTING_ACTIVATION_REQUEST(timeout) \
    WADJET_ASSERT_PACKET(::wadjet::testing::IsDoIPRoutingActivationRequest(), timeout)

/// Expect that a DoIP routing activation request is received
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_DOIP_ROUTING_ACTIVATION_REQUEST(timeout) \
    WADJET_EXPECT_PACKET(::wadjet::testing::IsDoIPRoutingActivationRequest(), timeout)

/// Assert that a DoIP routing activation response is received
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_DOIP_ROUTING_ACTIVATION_RESPONSE(timeout) \
    WADJET_ASSERT_PACKET(::wadjet::testing::IsDoIPRoutingActivationResponse(), timeout)

/// Expect that a DoIP routing activation response is received
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_DOIP_ROUTING_ACTIVATION_RESPONSE(timeout) \
    WADJET_EXPECT_PACKET(::wadjet::testing::IsDoIPRoutingActivationResponse(), timeout)

/// Assert that a DoIP diagnostic message is received
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_DOIP_DIAGNOSTIC_MESSAGE(timeout) \
    WADJET_ASSERT_PACKET(::wadjet::testing::IsDoIPDiagnosticMessage(), timeout)

/// Expect that a DoIP diagnostic message is received
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_DOIP_DIAGNOSTIC_MESSAGE(timeout) \
    WADJET_EXPECT_PACKET(::wadjet::testing::IsDoIPDiagnosticMessage(), timeout)

/// Assert that a DoIP vehicle identification request is received
/// @param timeout std::chrono duration for timeout
#define WADJET_ASSERT_DOIP_VEHICLE_IDENTIFICATION_REQUEST(timeout) \
    WADJET_ASSERT_PACKET(::wadjet::testing::IsDoIPVehicleIdentificationRequest(), timeout)

/// Expect that a DoIP vehicle identification request is received
/// @param timeout std::chrono duration for timeout
#define WADJET_EXPECT_DOIP_VEHICLE_IDENTIFICATION_REQUEST(timeout) \
    WADJET_EXPECT_PACKET(::wadjet::testing::IsDoIPVehicleIdentificationRequest(), timeout)

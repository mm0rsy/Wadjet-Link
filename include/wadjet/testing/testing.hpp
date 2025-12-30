/// @file testing.hpp
/// @brief Main include file for Wadjet-Link testing framework
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
///
/// This header provides all testing utilities including:
/// - gMock-style packet matchers
/// - LiveCaptureTestFixture for live traffic testing
/// - Convenience macros for common assertions
/// - Property-based testing generators
/// - Record-then-assert mode
/// - Live-assert mode
///
/// Example:
/// @code
/// #include <wadjet/testing/testing.hpp>
///
/// using namespace wadjet::testing;
///
/// class MySOMEIPTest : public LiveCaptureTestFixture {
/// protected:
///     void SetUp() override {
///         set_interface("eth0");
///         set_filter("udp port 30490");
///         LiveCaptureTestFixture::SetUp();
///     }
/// };
///
/// TEST_F(MySOMEIPTest, ServiceOffersWithin100ms) {
///     WADJET_EXPECT_SOMEIP_SERVICE(0x1234, 100ms);
/// }
///
/// TEST_F(MySOMEIPTest, RequestResponseCycle) {
///     auto pkt = wait_for_match(
///         AllOf(HasSOMEIPServiceId(0x1234), IsSOMEIPRequest()),
///         200ms);
///     ASSERT_TRUE(pkt.has_value());
///     EXPECT_THAT(*pkt, PayloadContains({0xDE, 0xAD}));
/// }
/// @endcode

#pragma once

// GoogleTest/GoogleMock
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Wadjet testing components
#include "wadjet/testing/matchers.hpp"
#include "wadjet/testing/live_capture_fixture.hpp"
#include "wadjet/testing/macros.hpp"
#include "wadjet/testing/generators.hpp"
#include "wadjet/testing/record_replay.hpp"
#include "wadjet/testing/live_assert.hpp"

// Also include commonly needed Wadjet types
#include "wadjet/core/types.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/dispatcher.hpp"

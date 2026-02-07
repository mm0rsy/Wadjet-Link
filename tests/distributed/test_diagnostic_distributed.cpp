#include "wadjet/distributed/matchers/expect_diagnostic.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/net/packet.hpp"
#include "wadjet/protocols/uds/uds_types.hpp"

#include <gtest/gtest.h>

namespace wadjet::distributed::testing {

/**
 * @brief Test suite for distributed diagnostic session tracking
 *
 * T321: Validates that DiagnosticSessionManager integration with TestNode
 * correctly tracks per-ECU session state across distributed captures
 */
class DistributedDiagnosticSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test context
    }

    void TearDown() override {
        // Cleanup
    }
};

/**
 * @brief Test single ECU diagnostic session
 */
TEST_F(DistributedDiagnosticSessionTest, SingleEcuDiagnosticSession) {
    // Create a diagnostic session step for a single ECU
    std::vector<ExpectDiagnosticSession::SessionStep> steps = {
        {"ecu_a", protocols::uds::ServiceID::DiagnosticSessionControl,
         std::chrono::milliseconds{1000}}};

    // Create the matcher
    auto session_matcher = ExpectDiagnosticSession(steps);
    EXPECT_NE(nullptr, session_matcher);
}

/**
 * @brief Test multi-ECU diagnostic sequence validation
 */
TEST_F(DistributedDiagnosticSessionTest, MultiEcuDiagnosticSequence) {
    // Create a diagnostic session with multiple steps across ECUs
    std::vector<ExpectDiagnosticSession::SessionStep> steps = {
        {"ecu_a", protocols::uds::ServiceID::SecurityAccess, std::chrono::milliseconds{2000}},
        {"ecu_b", protocols::uds::ServiceID::RequestDownload, std::chrono::milliseconds{2000}}};

    // Create the matcher
    auto session_matcher = ExpectDiagnosticSession(steps, std::chrono::milliseconds{10000});
    EXPECT_NE(nullptr, session_matcher);
}

/**
 * @brief Test diagnostic session timeout
 */
TEST_F(DistributedDiagnosticSessionTest, DiagnosticSessionTimeout) {
    // Create context with missing diagnostic step
    std::unordered_map<std::string, DistributedCaptureContext> contexts;

    DistributedCaptureContext ecu_a_context;
    ecu_a_context.node_id = "ecu_a";
    ecu_a_context.start_timestamp_ns = 0;
    ecu_a_context.end_timestamp_ns = 100000000;  // 100ms

    contexts["ecu_a"] = ecu_a_context;

    std::vector<ExpectDiagnosticSession::SessionStep> steps = {
        {"ecu_a", protocols::uds::ServiceID::DiagnosticSessionControl,
         std::chrono::milliseconds{1000}}};

    auto session_matcher = ExpectDiagnosticSession(steps);
    auto result = session_matcher->evaluate(contexts);

    // Should fail because no packets in the context
    EXPECT_FALSE(result.matched);
}

/**
 * @brief Test diagnostic response validation
 */
TEST_F(DistributedDiagnosticSessionTest, DiagnosticResponseValidation) {
    // Create contexts for source and destination nodes
    DistributedCaptureContext src_context;
    src_context.node_id = "ecu_a";

    DistributedCaptureContext dst_context;
    dst_context.node_id = "ecu_b";

    // Create diagnostic response matcher
    auto response_matcher =
        ExpectDiagnosticResponse("ecu_a", "ecu_b", protocols::uds::ServiceID::ReadDataByIdentifier);

    EXPECT_NE(nullptr, response_matcher);
}

/**
 * @brief Test distributed diagnostic matcher description
 */
TEST_F(DistributedDiagnosticSessionTest, MatcherDescription) {
    std::vector<ExpectDiagnosticSession::SessionStep> steps = {
        {"ecu_a", protocols::uds::ServiceID::SecurityAccess, std::chrono::milliseconds{2000}},
        {"ecu_b", protocols::uds::ServiceID::RequestDownload, std::chrono::milliseconds{2000}}};

    auto matcher = ExpectDiagnosticSession(steps);
    auto desc = matcher->describe();

    // Verify description contains node IDs and service IDs
    EXPECT_NE(desc.find("ecu_a"), std::string::npos);
    EXPECT_NE(desc.find("ecu_b"), std::string::npos);
}

/**
 * @brief Test matcher cloning
 */
TEST_F(DistributedDiagnosticSessionTest, MatcherCloning) {
    std::vector<ExpectDiagnosticSession::SessionStep> steps = {
        {"ecu_a", protocols::uds::ServiceID::DiagnosticSessionControl,
         std::chrono::milliseconds{1000}}};

    auto original_matcher = ExpectDiagnosticSession(steps);
    auto cloned_matcher = original_matcher->clone();

    EXPECT_NE(nullptr, cloned_matcher);
    EXPECT_EQ(original_matcher->describe(), cloned_matcher->describe());
}

/**
 * @brief Test missing diagnostic step detection
 */
TEST_F(DistributedDiagnosticSessionTest, MissingDiagnosticStep) {
    // Create contexts without the required steps
    std::unordered_map<std::string, DistributedCaptureContext> contexts;

    DistributedCaptureContext ecu_context;
    ecu_context.node_id = "ecu_a";
    ecu_context.start_timestamp_ns = 0;
    ecu_context.end_timestamp_ns = 5000000000;  // 5 seconds

    contexts["ecu_a"] = ecu_context;

    std::vector<ExpectDiagnosticSession::SessionStep> steps = {
        {"ecu_a", protocols::uds::ServiceID::SecurityAccess, std::chrono::milliseconds{1000}},
        {"ecu_a", protocols::uds::ServiceID::RequestDownload, std::chrono::milliseconds{2000}}};

    auto matcher = ExpectDiagnosticSession(steps);
    auto result = matcher->evaluate(contexts);

    // Should fail because steps are missing
    EXPECT_FALSE(result.matched);
}

}  // namespace wadjet::distributed::testing

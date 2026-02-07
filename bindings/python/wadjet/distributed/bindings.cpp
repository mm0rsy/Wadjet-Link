/**
 * @file bindings/python/wadjet/distributed/bindings.cpp
 * @brief pybind11 bindings for Wadjet-Link distributed testing primitives
 *
 * Provides Python bindings for:
 * - T105: SyncBarrier class
 * - T106: TimestampNormalizer class
 * - T107: DistributedMatcher classes
 * - T108: TestCoordinator class
 * - T109: TestNode class
 * - T110: Result types
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/sync_barrier.hpp"
#include "wadjet/distributed/timestamp_normalizer.hpp"
#include "wadjet/distributed/distributed_matcher.hpp"
#include "wadjet/distributed/types.hpp"

namespace py = pybind11;
using namespace wadjet::distributed;

// ============================================================================
// Module Definition
// ============================================================================

PYBIND11_MODULE(_distributed, m) {
    m.doc() = R"(
        Wadjet-Link Distributed Testing Framework

        High-level distributed packet capture and testing primitives:
        - SyncBarrier: Clock-synchronized barrier for multi-node tests
        - TimestampNormalizer: Clock synchronization detection and status
        - DistributedMatcher: Matchers for distributed assertions
        - TestCoordinator: Test orchestration and result aggregation
        - TestNode: Individual node capture and test execution
    )";

    // ========================================================================
    // Clock Synchronization Support Types
    // ========================================================================

    // ClockSyncMethod enum
    py::enum_<ClockSyncMethod>(m, "ClockSyncMethod")
        .value("NONE", ClockSyncMethod::None)
        .value("NTP", ClockSyncMethod::NTP)
        .value("GPTP", ClockSyncMethod::GPTP)
        .value("UNKNOWN", ClockSyncMethod::Unknown)
        .export_values();

    // ClockSyncStatus struct
    py::class_<ClockSyncStatus>(m, "ClockSyncStatus")
        .def(py::init<>())
        .def_readwrite("method", &ClockSyncStatus::method)
        .def_readwrite("is_synchronized", &ClockSyncStatus::is_synchronized)
        .def_readwrite("estimated_offset_ns", &ClockSyncStatus::estimated_offset_ns)
        .def_readwrite("max_error_ns", &ClockSyncStatus::max_error_ns)
        .def_readwrite("grandmaster_id", &ClockSyncStatus::grandmaster_id)
        .def("__repr__", [](const ClockSyncStatus& status) {
            return std::string("<ClockSyncStatus: ") +
                   (status.is_synchronized ? "SYNCHRONIZED" : "NOT_SYNCHRONIZED") + ">";
        });

    // ========================================================================
    // T105: SyncBarrier Class Binding
    // ========================================================================

    py::class_<SyncBarrier, std::shared_ptr<SyncBarrier>>(m, "SyncBarrier", R"(
        Synchronization barrier for coordinating multi-node test execution.

        Ensures that all nodes reach a specific point before proceeding.
        Useful for synchronized start of packet capture or test validation.

        Example:
            barrier = wadjet.distributed.SyncBarrier("test_start", 3)
            barrier.wait()  # Blocks until 3 participants arrive
    )")
        .def(py::init<const std::string&, uint32_t>(),
             py::arg("barrier_id"),
             py::arg("expected_participants"),
             "Create a barrier for multi-node synchronization")
        
        .def("wait", &SyncBarrier::wait,
             py::arg("timeout_ns") = 0,
             "Wait at barrier until all participants arrive or timeout")
        
        .def("is_satisfied", &SyncBarrier::is_satisfied,
             "Check if barrier has been satisfied (all participants arrived)")
        
        .def("get_participant_count", &SyncBarrier::get_participant_count,
             "Get current number of participants waiting at barrier")
        
        .def("get_barrier_id", &SyncBarrier::get_barrier_id,
             "Get the barrier identifier")
        
        .def("get_expected_participants", &SyncBarrier::get_expected_participants,
             "Get expected participant count");

    // ========================================================================
    // T106: TimestampNormalizer Class Binding
    // ========================================================================

    py::class_<TimestampNormalizer>(m, "TimestampNormalizer", R"(
        Detects and manages clock synchronization across distributed nodes.

        Determines if gPTP or NTP is available and provides synchronized
        timestamp conversion for distributed test assertions.

        Example:
            normalizer = wadjet.distributed.TimestampNormalizer()
            status = normalizer.detect_sync_status()
            if status.is_synchronized:
                print(f"Synchronized via {status.method}")
    )")
        .def(py::init<>(),
             "Create timestamp normalizer")
        
        .def("detect_sync_status", &TimestampNormalizer::detect_sync_status,
             "Detect clock synchronization method (gPTP or NTP)")
        
        .def("is_synchronized", &TimestampNormalizer::is_synchronized,
             "Check if clock is synchronized")
        
        .def("normalize_timestamp",
             [](TimestampNormalizer& self, int64_t ts_ns) {
                 return self.normalize_timestamp(
                     std::chrono::nanoseconds(ts_ns)).count();
             },
             py::arg("timestamp_ns"),
             "Normalize timestamp to reference clock (returns nanoseconds)")
        
        .def("verify_gptp_health", &TimestampNormalizer::verify_gptp_health,
             "Verify gPTP health by decoding Announce/Sync messages")
        
        .def("get_sync_status", &TimestampNormalizer::get_sync_status,
             "Get current synchronization status");

    // ========================================================================
    // T107: DistributedMatcher Classes Binding
    // ========================================================================

    py::class_<DistributedMatchResult>(m, "DistributedMatchResult", R"(
        Result of a distributed assertion match.

        Contains match status, error information, and packet context for
        debugging distributed test failures.
    )")
        .def(py::init<>())
        .def_readwrite("matched", &DistributedMatchResult::matched)
        .def_readwrite("node_id", &DistributedMatchResult::node_id)
        .def_readwrite("timestamp_ns", &DistributedMatchResult::timestamp_ns)
        .def_readwrite("error_code", &DistributedMatchResult::error_code)
        .def_readwrite("error_message", &DistributedMatchResult::error_message)
        .def_readwrite("expected_condition", &DistributedMatchResult::expected_condition)
        .def_readwrite("actual_condition", &DistributedMatchResult::actual_condition)
        .def_readwrite("packet_context", &DistributedMatchResult::packet_context)
        .def_readwrite("failure_timestamp_ns", &DistributedMatchResult::failure_timestamp_ns)
        .def("__repr__", [](const DistributedMatchResult& result) {
            return std::string("<DistributedMatchResult: ") +
                   (result.matched ? "PASS" : "FAIL") + ">";
        });

    py::class_<DistributedMatcher>(m, "DistributedMatcher", R"(
        Base class for distributed packet matchers.

        Provides factory methods for creating matcher instances that work
        with distributed packet validation.
    )")
        .def("match", &DistributedMatcher::match,
             "Execute match against captured packet");

    // ========================================================================
    // T108: TestCoordinator Class Binding
    // ========================================================================

    py::class_<TestCoordinator, std::shared_ptr<TestCoordinator>>(
        m, "TestCoordinator", R"(
        Orchestrates distributed packet capture tests across multiple nodes.

        Manages:
        - Node registration and heartbeat monitoring
        - Test synchronization via barriers
        - Result collection and aggregation
        - PCAP file management

        Example:
            config = wadjet.distributed.CoordinatorConfig()
            coordinator = wadjet.distributed.TestCoordinator(config)
            coordinator.start()
            # ... add nodes, run tests
            results = coordinator.get_results()
    )")
        .def(py::init<std::shared_ptr<CoordinatorConfig>>(),
             py::arg("config"),
             "Create test coordinator with configuration")
        
        .def("start", &TestCoordinator::start,
             "Start coordinator and begin heartbeat monitoring")
        
        .def("stop", &TestCoordinator::stop,
             "Stop coordinator and clean up resources")
        
        .def("register_node", &TestCoordinator::register_node,
             py::arg("node_info"),
             "Register a node to participate in tests")
        
        .def("create_barrier", &TestCoordinator::create_barrier,
             py::arg("barrier_id"),
             py::arg("expected_count"),
             "Create synchronization barrier for test")
        
        .def("run_test",
             [](TestCoordinator& self, const std::string& test_name,
                uint32_t duration_ms) {
                 return self.run_test(test_name,
                     std::chrono::milliseconds(duration_ms));
             },
             py::arg("test_name"),
             py::arg("duration_ms"),
             "Run distributed test for specified duration")
        
        .def("get_results", &TestCoordinator::get_results,
             py::return_value_policy::reference,
             "Get aggregated results from last test run")
        
        .def("save_junit_report", &TestCoordinator::save_junit_report,
             py::arg("output_path"),
             "Save test results as JUnit XML report")
        
        .def("detect_coordinator_failure", &TestCoordinator::detect_coordinator_failure,
             "Detect if coordinator has failed based on heartbeats");

    py::class_<CoordinatorConfig, std::shared_ptr<CoordinatorConfig>>(
        m, "CoordinatorConfig")
        .def(py::init<>())
        .def_readwrite("grpc_host", &CoordinatorConfig::grpc_host)
        .def_readwrite("grpc_port", &CoordinatorConfig::grpc_port)
        .def_readwrite("heartbeat_timeout_ms", &CoordinatorConfig::heartbeat_timeout_ms)
        .def_readwrite("barrier_sync_timeout_ms", &CoordinatorConfig::barrier_sync_timeout_ms)
        .def_readwrite("enable_gptp_sync", &CoordinatorConfig::enable_gptp_sync)
        .def_readwrite("enable_ntp_sync", &CoordinatorConfig::enable_ntp_sync);

    // ========================================================================
    // T109: TestNode Class Binding
    // ========================================================================

    py::class_<TestNode, std::shared_ptr<TestNode>>(m, "TestNode", R"(
        Captures packets on a single node in a distributed test.

        Responsible for:
        - Connecting to coordinator
        - Starting/stopping packet capture
        - Synchronizing with other nodes via barriers
        - Saving captured packets to PCAP

        Example:
            config = wadjet.distributed.NodeConfig()
            node = wadjet.distributed.TestNode(config)
            node.start_capture("eth0")
            # ... wait for barrier
            node.stop_capture()
    )")
        .def(py::init<std::shared_ptr<NodeConfig>>(),
             py::arg("config"),
             "Create test node with configuration")
        
        .def("start_capture", &TestNode::start_capture,
             py::arg("interface_name"),
             "Start capturing packets on specified interface")
        
        .def("stop_capture", &TestNode::stop_capture,
             "Stop capturing and save PCAP file")
        
        .def("wait_barrier",
             [](TestNode& self, uint32_t timeout_ms) {
                 return self.wait_barrier(std::chrono::milliseconds(timeout_ms));
             },
             py::arg("timeout_ms") = 0,
             "Wait at barrier until all nodes are ready")
        
        .def("get_clock_sync_status", &TestNode::get_clock_sync_status,
             "Get current clock synchronization status")
        
        .def("detect_coordinator_failure", &TestNode::detect_coordinator_failure,
             "Detect if coordinator heartbeat has been lost")
        
        .def("save_partial_results", &TestNode::save_partial_results,
             "Save partial PCAP results on coordinator failure")
        
        .def("get_node_id", &TestNode::get_node_id,
             "Get node identifier");

    py::class_<NodeConfig, std::shared_ptr<NodeConfig>>(m, "NodeConfig")
        .def(py::init<>())
        .def_readwrite("node_id", &NodeConfig::node_id)
        .def_readwrite("coordinator_host", &NodeConfig::coordinator_host)
        .def_readwrite("coordinator_port", &NodeConfig::coordinator_port)
        .def_readwrite("grpc_port", &NodeConfig::grpc_port)
        .def_readwrite("heartbeat_timeout_ms", &NodeConfig::heartbeat_timeout_ms);

    // ========================================================================
    // T110: Result Types Binding
    // ========================================================================

    py::class_<NodeResult>(m, "NodeResult", "Test results from a single node")
        .def(py::init<>())
        .def_readwrite("node_id", &NodeResult::node_id)
        .def_readwrite("healthy", &NodeResult::healthy)
        .def_readwrite("assertions", &NodeResult::assertions)
        .def_readwrite("passed_count", &NodeResult::passed_count)
        .def_readwrite("failed_count", &NodeResult::failed_count)
        .def_readwrite("total_duration_ns", &NodeResult::total_duration_ns)
        .def_readwrite("pcap_file_path", &NodeResult::pcap_file_path)
        .def_readwrite("error_message", &NodeResult::error_message);

    py::class_<AggregatedResult>(m, "AggregatedResult",
        "Combined test results from all nodes")
        .def(py::init<>())
        .def_readwrite("test_name", &AggregatedResult::test_name)
        .def_readwrite("node_results", &AggregatedResult::node_results)
        .def_readwrite("test_start_time_ns", &AggregatedResult::test_start_time_ns)
        .def_readwrite("test_end_time_ns", &AggregatedResult::test_end_time_ns)
        .def_readwrite("overall_status", &AggregatedResult::overall_status)
        .def_readwrite("total_duration_ns", &AggregatedResult::total_duration_ns)
        .def_readwrite("total_assertions", &AggregatedResult::total_assertions)
        .def_readwrite("passed_assertions", &AggregatedResult::passed_assertions)
        .def_readwrite("failed_assertions", &AggregatedResult::failed_assertions)
        .def("__repr__", [](const AggregatedResult& result) {
            return std::string("<AggregatedResult: test=") + result.test_name +
                   " nodes=" + std::to_string(result.node_results.size()) + ">";
        });
}

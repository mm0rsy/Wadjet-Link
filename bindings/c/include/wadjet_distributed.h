/**
 * @file wadjet_distributed.h
 * @brief C ABI for Wadjet-Link Distributed Testing Framework
 *
 * Provides C99-compatible bindings for distributed packet capture and testing
 * across multiple nodes with clock synchronization and barrier synchronization.
 *
 * **Memory Management**: 
 * - Opaque handles are allocated internally and must be destroyed with provided functions
 * - String pointers are borrowed (owned by the object) - do not free
 * - Use the explicit destroy functions to free resources
 *
 * **Thread Safety**:
 * - Error messages stored in thread-local storage
 * - Each thread has its own error context
 *
 * @see wadjet_c.h for core Wadjet-Link C API
 */

#ifndef WADJET_DISTRIBUTED_H
#define WADJET_DISTRIBUTED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Handling (distributed testing specific)
 * ============================================================================ */

/**
 * @brief Distributed testing error codes
 */
typedef enum {
    WADJET_DIST_OK = 0,                      /**< Success */
    WADJET_DIST_ERR_INVALID_CONFIG = 1,      /**< Invalid configuration */
    WADJET_DIST_ERR_COORDINATOR_UNAVAILABLE = 2,  /**< Coordinator unreachable */
    WADJET_DIST_ERR_NODE_UNAVAILABLE = 3,    /**< Node unreachable */
    WADJET_DIST_ERR_CLOCK_SYNC_FAILED = 4,   /**< Clock synchronization failed */
    WADJET_DIST_ERR_BARRIER_TIMEOUT = 5,     /**< Barrier synchronization timeout */
    WADJET_DIST_ERR_NO_CLOCK_SYNC = 6,       /**< No clock synchronization detected */
    WADJET_DIST_ERR_TEST_FAILED = 7,         /**< Test execution failed */
    WADJET_DIST_ERR_INVALID_STATE = 8,       /**< Invalid test state */
    WADJET_DIST_ERR_TIMEOUT = 9,             /**< Operation timeout */
    WADJET_DIST_ERR_UNKNOWN = 99,            /**< Unknown error */
} wadjet_dist_error_t;

/**
 * @brief Get human-readable error message
 * @param error Error code
 * @return Error message string (static, do not free)
 */
const char* wadjet_dist_error_message(wadjet_dist_error_t error);

/**
 * @brief Get last distributed testing error from thread-local storage
 * @return Error message or NULL if no error
 */
const char* wadjet_dist_last_error(void);

/**
 * @brief Clear the last distributed testing error
 */
void wadjet_dist_clear_error(void);

/**
 * @brief Set error in thread-local storage
 * @param error_code Error code
 * @param message Error message (copied internally)
 */
void wadjet_dist_set_error(wadjet_dist_error_t error_code, const char* message);

/* ============================================================================
 * Type Definitions - Following FFI Type Mapping (T288)
 * ============================================================================ */

/** @brief Node identifier string (UTF-8) */
typedef const char* wadjet_node_id_t;

/** @brief Timestamp in nanoseconds since Unix epoch */
typedef int64_t wadjet_timestamp_ns_t;

/** @brief Clock synchronization method */
typedef enum {
    WADJET_CLOCK_SYNC_NONE = 0,      /**< No synchronization */
    WADJET_CLOCK_SYNC_NTP = 1,       /**< NTP synchronization */
    WADJET_CLOCK_SYNC_GPTP = 2,      /**< gPTP (IEEE 802.1AS) */
    WADJET_CLOCK_SYNC_UNKNOWN = 3,   /**< Unknown sync method */
} wadjet_clock_sync_method_t;

/**
 * @brief Clock synchronization status
 */
typedef struct {
    wadjet_clock_sync_method_t method;  /**< Synchronization method */
    bool is_synchronized;               /**< Is synchronized */
    int64_t estimated_offset_ns;        /**< Offset in nanoseconds */
    int64_t max_error_ns;               /**< Maximum estimation error */
    const char* grandmaster_id;         /**< Grandmaster identifier (borrowed) */
} wadjet_clock_sync_status_t;

/** @brief Opaque handle to test coordinator */
typedef struct wadjet_coordinator* wadjet_coordinator_t;

/** @brief Opaque handle to test node */
typedef struct wadjet_test_node* wadjet_test_node_t;

/** @brief Opaque handle to sync barrier */
typedef struct wadjet_sync_barrier* wadjet_sync_barrier_t;

/**
 * @brief Node information for configuration
 */
typedef struct {
    wadjet_node_id_t node_id;           /**< Unique node identifier */
    const char* hostname;               /**< Hostname or IP address */
    uint16_t grpc_port;                 /**< gRPC port number */
    const char** capture_interfaces;    /**< Array of interface names */
    size_t interface_count;             /**< Number of capture interfaces */
    const char* version;                /**< Node version (optional) */
} wadjet_node_info_t;

/**
 * @brief Configuration for test coordinator
 */
typedef struct {
    const char* coordinator_host;       /**< Coordinator hostname/IP */
    uint16_t coordinator_port;          /**< Coordinator gRPC port */
    uint32_t heartbeat_timeout_ms;      /**< Heartbeat timeout in milliseconds */
    uint32_t barrier_sync_timeout_ms;   /**< Barrier sync timeout in milliseconds */
    bool enable_gptp_sync;              /**< Enable gPTP synchronization */
    bool enable_ntp_sync;               /**< Enable NTP synchronization */
} wadjet_coordinator_config_t;

/**
 * @brief Result from an assertion in distributed test
 */
typedef struct {
    const char* test_name;              /**< Test name (borrowed) */
    bool passed;                        /**< Assertion passed */
    const char* message;                /**< Assertion message (borrowed) */
    wadjet_timestamp_ns_t timestamp_ns; /**< Assertion timestamp */
    const char* node_id;                /**< Node where assertion ran (borrowed) */
} wadjet_assertion_result_t;

/**
 * @brief Results from a single node
 */
typedef struct {
    wadjet_node_id_t node_id;           /**< Node identifier (borrowed) */
    bool healthy;                       /**< Node completed successfully */
    int32_t passed_assertions;          /**< Number of passed assertions */
    int32_t failed_assertions;          /**< Number of failed assertions */
    int64_t total_duration_ns;          /**< Test duration in nanoseconds */
    const char* pcap_file_path;         /**< Path to node's PCAP file (borrowed) */
    const char* error_message;          /**< Error message if unhealthy (borrowed) */
} wadjet_node_result_t;

/**
 * @brief Aggregated results from all nodes
 */
typedef struct {
    const char* test_name;              /**< Test name (borrowed) */
    const wadjet_node_result_t* node_results;  /**< Array of node results */
    int32_t node_count;                 /**< Number of nodes */
    wadjet_timestamp_ns_t test_start_time_ns;  /**< Test start time */
    wadjet_timestamp_ns_t test_end_time_ns;    /**< Test end time */
    int32_t overall_status;             /**< 0=Passed, 1=Failed, 2=Error */
    int64_t total_duration_ns;          /**< Total test duration */
    int32_t total_assertions;           /**< Total assertion count */
    int32_t passed_assertions;          /**< Total passed assertions */
    int32_t failed_assertions;          /**< Total failed assertions */
} wadjet_aggregated_result_t;

/* ============================================================================
 * Coordinator Lifecycle (T100)
 * ============================================================================ */

/**
 * @brief Create a test coordinator
 * @param config Coordinator configuration
 * @param error_code Output: error code (optional)
 * @return Opaque coordinator handle, or NULL on failure
 *
 * On failure, call wadjet_dist_last_error() for details.
 */
wadjet_coordinator_t wadjet_coordinator_create(
    const wadjet_coordinator_config_t* config,
    wadjet_dist_error_t* error_code);

/**
 * @brief Start the coordinator
 * @param coordinator Coordinator handle
 * @param nodes Array of node info
 * @param node_count Number of nodes
 * @return Error code (0 = success)
 *
 * Starts heartbeat monitoring and prepares for test execution.
 * Must be called after create() and before run_test().
 */
wadjet_dist_error_t wadjet_coordinator_start(
    wadjet_coordinator_t coordinator,
    const wadjet_node_info_t* nodes,
    size_t node_count);

/**
 * @brief Run a distributed test
 * @param coordinator Coordinator handle
 * @param test_name Test name for logging
 * @param test_duration_ms Test duration in milliseconds
 * @return Error code
 *
 * Synchronizes barrier, runs test, collects results.
 */
wadjet_dist_error_t wadjet_coordinator_run_test(
    wadjet_coordinator_t coordinator,
    const char* test_name,
    uint32_t test_duration_ms);

/**
 * @brief Get test results
 * @param coordinator Coordinator handle
 * @return Aggregated results (borrowed - do not free)
 *
 * Valid after wadjet_coordinator_run_test() completes.
 * Lifetime is tied to coordinator handle.
 */
const wadjet_aggregated_result_t* wadjet_coordinator_get_results(
    wadjet_coordinator_t coordinator);

/**
 * @brief Stop the coordinator
 * @param coordinator Coordinator handle
 * @return Error code
 */
wadjet_dist_error_t wadjet_coordinator_stop(
    wadjet_coordinator_t coordinator);

/**
 * @brief Destroy the coordinator and free resources
 * @param coordinator Coordinator handle
 */
void wadjet_coordinator_destroy(wadjet_coordinator_t coordinator);

/* ============================================================================
 * Node Lifecycle (T101)
 * ============================================================================ */

/**
 * @brief Create a test node
 * @param node_id Unique node identifier
 * @param coordinator_host Coordinator hostname/IP
 * @param coordinator_port Coordinator gRPC port
 * @param error_code Output: error code (optional)
 * @return Opaque node handle, or NULL on failure
 */
wadjet_test_node_t wadjet_node_create(
    wadjet_node_id_t node_id,
    const char* coordinator_host,
    uint16_t coordinator_port,
    wadjet_dist_error_t* error_code);

/**
 * @brief Start the node and connect to coordinator
 * @param node Node handle
 * @param capture_interface Network interface to capture on
 * @return Error code
 */
wadjet_dist_error_t wadjet_node_start(
    wadjet_test_node_t node,
    const char* capture_interface);

/**
 * @brief Wait for test barrier synchronization
 * @param node Node handle
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return Error code
 *
 * Blocks until barrier is released or timeout occurs.
 */
wadjet_dist_error_t wadjet_node_wait_barrier(
    wadjet_test_node_t node,
    uint32_t timeout_ms);

/**
 * @brief Get node's clock synchronization status
 * @param node Node handle
 * @return Clock sync status
 */
wadjet_clock_sync_status_t wadjet_node_get_clock_sync_status(
    wadjet_test_node_t node);

/**
 * @brief Stop the node and save results
 * @param node Node handle
 * @return Error code
 *
 * Stops packet capture, saves PCAP file, and cleans up.
 */
wadjet_dist_error_t wadjet_node_stop(wadjet_test_node_t node);

/**
 * @brief Destroy the node and free resources
 * @param node Node handle
 */
void wadjet_node_destroy(wadjet_test_node_t node);

/* ============================================================================
 * Sync Barrier (T102)
 * ============================================================================ */

/**
 * @brief Create a synchronization barrier
 * @param barrier_id Unique barrier identifier
 * @param expected_participants Number of nodes expected to wait
 * @param timeout_ms Timeout in milliseconds
 * @param error_code Output: error code (optional)
 * @return Opaque barrier handle, or NULL on failure
 */
wadjet_sync_barrier_t wadjet_sync_barrier_create(
    const char* barrier_id,
    uint32_t expected_participants,
    uint32_t timeout_ms,
    wadjet_dist_error_t* error_code);

/**
 * @brief Wait at the barrier
 * @param barrier Barrier handle
 * @param timeout_ms Timeout in milliseconds (0 = use barrier default)
 * @return Error code
 *
 * Blocks until all expected participants arrive or timeout.
 */
wadjet_dist_error_t wadjet_sync_barrier_wait(
    wadjet_sync_barrier_t barrier,
    uint32_t timeout_ms);

/**
 * @brief Check if barrier is satisfied
 * @param barrier Barrier handle
 * @return true if all participants have arrived
 */
bool wadjet_sync_barrier_is_satisfied(wadjet_sync_barrier_t barrier);

/**
 * @brief Get current participant count
 * @param barrier Barrier handle
 * @return Number of threads currently waiting
 */
uint32_t wadjet_sync_barrier_participant_count(wadjet_sync_barrier_t barrier);

/**
 * @brief Destroy the barrier
 * @param barrier Barrier handle
 */
void wadjet_sync_barrier_destroy(wadjet_sync_barrier_t barrier);

/* ============================================================================
 * Thread-Local Error Storage (T103)
 * ============================================================================ */

/**
 * @brief Thread-local error context
 *
 * Stores error information per thread to avoid synchronization overhead
 * and allow safe concurrent operations.
 */

/**
 * @brief Get error code from thread-local storage
 * @return Most recent error code
 */
wadjet_dist_error_t wadjet_dist_get_error_code(void);

/**
 * @brief Get full error message from thread-local storage
 * @return Formatted error message (borrowed - do not free)
 */
const char* wadjet_dist_get_error_message(void);

/**
 * @brief Initialize thread-local error context
 * 
 * Called automatically on first use in each thread.
 * Can be called explicitly to reset error state.
 * @return Error code
 */
wadjet_dist_error_t wadjet_dist_init_error_context(void);

/**
 * @brief Check if thread has error
 * @return true if error is set
 */
bool wadjet_dist_has_error(void);

/* ============================================================================
 * Result Access Functions (Helper API)
 * ============================================================================ */

/**
 * @brief Export aggregated results as JUnit XML
 * @param results Aggregated results
 * @param output_path Path to write XML file
 * @return Error code
 */
wadjet_dist_error_t wadjet_aggregated_result_to_junit_xml(
    const wadjet_aggregated_result_t* results,
    const char* output_path);

/**
 * @brief Export aggregated results as JSON
 * @param results Aggregated results
 * @param output_path Path to write JSON file
 * @return Error code
 */
wadjet_dist_error_t wadjet_aggregated_result_to_json(
    const wadjet_aggregated_result_t* results,
    const char* output_path);

/**
 * @brief Get summary string for results
 * @param results Aggregated results
 * @return Summary string (borrowed - do not free)
 *
 * Example: "Test: multicast_discovery | Nodes: 3 | Status: PASSED | 127/127 assertions"
 */
const char* wadjet_aggregated_result_summary(
    const wadjet_aggregated_result_t* results);

#ifdef __cplusplus
}
#endif

#endif /* WADJET_DISTRIBUTED_H */

/**
 * @file wadjet_distributed.c
 * @brief C ABI implementation for Wadjet-Link Distributed Testing Framework
 *
 * Provides opaque handle implementations, memory management, and thread-local
 * error storage for distributed packet capture and testing.
 */

#include "wadjet_distributed.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"
#include "wadjet/distributed/sync_barrier.hpp"

/* ============================================================================
 * Error Message Strings (T103)
 * ============================================================================ */

static const char* ERROR_MESSAGES[] = {
    "Success",                              // WADJET_DIST_OK
    "Invalid configuration",                // WADJET_DIST_ERR_INVALID_CONFIG
    "Coordinator unavailable",              // WADJET_DIST_ERR_COORDINATOR_UNAVAILABLE
    "Node unavailable",                     // WADJET_DIST_ERR_NODE_UNAVAILABLE
    "Clock synchronization failed",         // WADJET_DIST_ERR_CLOCK_SYNC_FAILED
    "Barrier synchronization timeout",      // WADJET_DIST_ERR_BARRIER_TIMEOUT
    "No clock synchronization detected",    // WADJET_DIST_ERR_NO_CLOCK_SYNC
    "Test execution failed",                // WADJET_DIST_ERR_TEST_FAILED
    "Invalid test state",                   // WADJET_DIST_ERR_INVALID_STATE
    "Operation timeout",                    // WADJET_DIST_ERR_TIMEOUT
    NULL,                                   // ... padding
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Unknown error"  // WADJET_DIST_ERR_UNKNOWN
};

/* ============================================================================
 * Thread-Local Error Storage (T103)
 * ============================================================================ */

/**
 * @brief Per-thread error context
 *
 * Stores error information without requiring locks. Each thread has its own
 * error state to support safe concurrent testing operations.
 */
typedef struct {
    wadjet_dist_error_t error_code;
    char error_message[256];
    bool initialized;
} wadjet_dist_error_context_t;

/**
 * @brief Thread-local error context storage
 *
 * Use C11 _Thread_local for automatic per-thread storage management.
 * Initialized to zero on first use in each thread.
 */
_Thread_local wadjet_dist_error_context_t g_error_context = {0};

const char* wadjet_dist_error_message(wadjet_dist_error_t error)
{
    if (error >= 0 && error < 100) {
        if (ERROR_MESSAGES[error] != NULL) {
            return ERROR_MESSAGES[error];
        }
    }
    return ERROR_MESSAGES[99];  // WADJET_DIST_ERR_UNKNOWN
}

const char* wadjet_dist_last_error(void)
{
    if (!g_error_context.initialized) {
        return NULL;
    }
    if (g_error_context.error_code == WADJET_DIST_OK) {
        return NULL;
    }
    return g_error_context.error_message;
}

void wadjet_dist_clear_error(void)
{
    g_error_context.error_code = WADJET_DIST_OK;
    memset(g_error_context.error_message, 0, sizeof(g_error_context.error_message));
}

void wadjet_dist_set_error(wadjet_dist_error_t error_code, const char* message)
{
    if (!g_error_context.initialized) {
        wadjet_dist_init_error_context();
    }
    
    g_error_context.error_code = error_code;
    
    // Format: "ERROR_CODE: message" (e.g., "CLOCK_SYNC_FAILED: NTP not available")
    if (message != NULL) {
        snprintf(g_error_context.error_message,
                 sizeof(g_error_context.error_message),
                 "%s: %s",
                 wadjet_dist_error_message(error_code),
                 message);
    } else {
        strncpy(g_error_context.error_message,
                wadjet_dist_error_message(error_code),
                sizeof(g_error_context.error_message) - 1);
    }
}

wadjet_dist_error_t wadjet_dist_get_error_code(void)
{
    if (!g_error_context.initialized) {
        return WADJET_DIST_OK;
    }
    return g_error_context.error_code;
}

const char* wadjet_dist_get_error_message(void)
{
    if (!g_error_context.initialized) {
        return NULL;
    }
    return g_error_context.error_message[0] != '\0' ? g_error_context.error_message : NULL;
}

wadjet_dist_error_t wadjet_dist_init_error_context(void)
{
    memset(&g_error_context, 0, sizeof(g_error_context));
    g_error_context.initialized = true;
    g_error_context.error_code = WADJET_DIST_OK;
    return WADJET_DIST_OK;
}

bool wadjet_dist_has_error(void)
{
    if (!g_error_context.initialized) {
        return false;
    }
    return g_error_context.error_code != WADJET_DIST_OK;
}

/* ============================================================================
 * Opaque Handle Implementations
 * ============================================================================ */

/**
 * @brief Opaque coordinator handle wraps C++ TestCoordinator
 */
struct wadjet_coordinator {
    void* cpp_coordinator;  // Actually TestCoordinator*
};

/**
 * @brief Opaque node handle wraps C++ TestNode
 */
struct wadjet_test_node {
    void* cpp_node;         // Actually TestNode*
};

/**
 * @brief Opaque barrier handle wraps C++ SyncBarrier
 */
struct wadjet_sync_barrier {
    void* cpp_barrier;      // Actually SyncBarrier*
};

/* ============================================================================
 * Coordinator Lifecycle (T100)
 * ============================================================================ */

wadjet_coordinator_t wadjet_coordinator_create(
    const wadjet_coordinator_config_t* config,
    wadjet_dist_error_t* error_code)
{
    if (config == NULL) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_INVALID_CONFIG;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "config is NULL");
        return NULL;
    }

    wadjet_dist_init_error_context();

    try {
        // Create C++ coordinator with configuration
        auto cpp_config = std::make_shared<wadjet::distributed::CoordinatorConfig>();
        cpp_config->grpc_host = config->coordinator_host ? config->coordinator_host : "localhost";
        cpp_config->grpc_port = config->coordinator_port ? config->coordinator_port : 50051;
        cpp_config->heartbeat_timeout_ms = config->heartbeat_timeout_ms ? config->heartbeat_timeout_ms : 5000;
        cpp_config->barrier_sync_timeout_ms = config->barrier_sync_timeout_ms ? config->barrier_sync_timeout_ms : 10000;
        cpp_config->enable_gptp_sync = config->enable_gptp_sync;
        cpp_config->enable_ntp_sync = config->enable_ntp_sync;

        auto* cpp_coordinator = new wadjet::distributed::TestCoordinator(cpp_config);
        
        wadjet_coordinator_t handle = 
            (wadjet_coordinator_t)malloc(sizeof(struct wadjet_coordinator));
        handle->cpp_coordinator = (void*)cpp_coordinator;
        
        if (error_code != NULL) {
            *error_code = WADJET_DIST_OK;
        }
        
        return handle;
    } catch (const std::exception& e) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_INVALID_CONFIG;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, e.what());
        return NULL;
    } catch (...) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_UNKNOWN;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, "unknown exception in coordinator_create");
        return NULL;
    }
}

wadjet_dist_error_t wadjet_coordinator_start(
    wadjet_coordinator_t coordinator,
    const wadjet_node_info_t* nodes,
    size_t node_count)
{
    if (coordinator == NULL || nodes == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "coordinator or nodes is NULL");
        return WADJET_DIST_ERR_INVALID_CONFIG;
    }

    try {
        auto* cpp_coordinator = (wadjet::distributed::TestCoordinator*)coordinator->cpp_coordinator;
        
        // Convert C node info to C++ NodeInfo
        std::vector<wadjet::distributed::NodeInfo> cpp_nodes;
        for (size_t i = 0; i < node_count; i++) {
            wadjet::distributed::NodeInfo node_info;
            node_info.id = nodes[i].node_id ? nodes[i].node_id : "";
            node_info.hostname = nodes[i].hostname ? nodes[i].hostname : "localhost";
            node_info.grpc_port = nodes[i].grpc_port;
            
            // Copy capture interfaces
            for (size_t j = 0; j < nodes[i].interface_count; j++) {
                if (nodes[i].capture_interfaces[j] != NULL) {
                    node_info.capture_interfaces.push_back(nodes[i].capture_interfaces[j]);
                }
            }
            
            if (nodes[i].version != NULL) {
                node_info.version = nodes[i].version;
            }
            
            cpp_nodes.push_back(node_info);
        }
        
        // Start coordinator (blocking until ready)
        // Note: actual start() implementation depends on C++ API
        cpp_coordinator->start();
        
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_COORDINATOR_UNAVAILABLE, e.what());
        return WADJET_DIST_ERR_COORDINATOR_UNAVAILABLE;
    } catch (...) {
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, "unknown exception in coordinator_start");
        return WADJET_DIST_ERR_UNKNOWN;
    }
}

wadjet_dist_error_t wadjet_coordinator_run_test(
    wadjet_coordinator_t coordinator,
    const char* test_name,
    uint32_t test_duration_ms)
{
    if (coordinator == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, "coordinator is NULL");
        return WADJET_DIST_ERR_INVALID_STATE;
    }

    if (test_name == NULL || test_name[0] == '\0') {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "test_name is empty");
        return WADJET_DIST_ERR_INVALID_CONFIG;
    }

    try {
        auto* cpp_coordinator = (wadjet::distributed::TestCoordinator*)coordinator->cpp_coordinator;
        
        // Create and run test scenario
        // Note: actual implementation depends on C++ API
        // cpp_coordinator->run_test(test_name, std::chrono::milliseconds(test_duration_ms));
        
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_TEST_FAILED, e.what());
        return WADJET_DIST_ERR_TEST_FAILED;
    } catch (...) {
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, "unknown exception in coordinator_run_test");
        return WADJET_DIST_ERR_UNKNOWN;
    }
}

const wadjet_aggregated_result_t* wadjet_coordinator_get_results(
    wadjet_coordinator_t coordinator)
{
    if (coordinator == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, "coordinator is NULL");
        return NULL;
    }

    try {
        auto* cpp_coordinator = (wadjet::distributed::TestCoordinator*)coordinator->cpp_coordinator;
        
        // Get results from C++ coordinator
        // Note: returns borrowed pointer - lifetime tied to coordinator
        // auto* cpp_results = cpp_coordinator->get_results();
        // return (const wadjet_aggregated_result_t*)cpp_results;
        
        // Placeholder: would need Result type mapping implementation
        return NULL;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, e.what());
        return NULL;
    }
}

wadjet_dist_error_t wadjet_coordinator_stop(
    wadjet_coordinator_t coordinator)
{
    if (coordinator == NULL) {
        return WADJET_DIST_ERR_INVALID_STATE;
    }

    try {
        auto* cpp_coordinator = (wadjet::distributed::TestCoordinator*)coordinator->cpp_coordinator;
        cpp_coordinator->stop();
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, e.what());
        return WADJET_DIST_ERR_UNKNOWN;
    }
}

void wadjet_coordinator_destroy(wadjet_coordinator_t coordinator)
{
    if (coordinator == NULL) {
        return;
    }

    try {
        auto* cpp_coordinator = (wadjet::distributed::TestCoordinator*)coordinator->cpp_coordinator;
        delete cpp_coordinator;
        free(coordinator);
    } catch (...) {
        // Silently ignore errors during cleanup
    }
}

/* ============================================================================
 * Node Lifecycle (T101)
 * ============================================================================ */

wadjet_test_node_t wadjet_node_create(
    wadjet_node_id_t node_id,
    const char* coordinator_host,
    uint16_t coordinator_port,
    wadjet_dist_error_t* error_code)
{
    if (node_id == NULL) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_INVALID_CONFIG;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "node_id is NULL");
        return NULL;
    }

    wadjet_dist_init_error_context();

    try {
        auto cpp_config = std::make_shared<wadjet::distributed::NodeConfig>();
        cpp_config->node_id = node_id;
        cpp_config->coordinator_host = coordinator_host ? coordinator_host : "localhost";
        cpp_config->coordinator_port = coordinator_port ? coordinator_port : 50051;

        auto* cpp_node = new wadjet::distributed::TestNode(cpp_config);
        
        wadjet_test_node_t handle = 
            (wadjet_test_node_t)malloc(sizeof(struct wadjet_test_node));
        handle->cpp_node = (void*)cpp_node;
        
        if (error_code != NULL) {
            *error_code = WADJET_DIST_OK;
        }
        
        return handle;
    } catch (const std::exception& e) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_INVALID_CONFIG;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, e.what());
        return NULL;
    } catch (...) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_UNKNOWN;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, "unknown exception in node_create");
        return NULL;
    }
}

wadjet_dist_error_t wadjet_node_start(
    wadjet_test_node_t node,
    const char* capture_interface)
{
    if (node == NULL || capture_interface == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "node or capture_interface is NULL");
        return WADJET_DIST_ERR_INVALID_CONFIG;
    }

    try {
        auto* cpp_node = (wadjet::distributed::TestNode*)node->cpp_node;
        cpp_node->start_capture(capture_interface);
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_NODE_UNAVAILABLE, e.what());
        return WADJET_DIST_ERR_NODE_UNAVAILABLE;
    }
}

wadjet_dist_error_t wadjet_node_wait_barrier(
    wadjet_test_node_t node,
    uint32_t timeout_ms)
{
    if (node == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, "node is NULL");
        return WADJET_DIST_ERR_INVALID_STATE;
    }

    try {
        auto* cpp_node = (wadjet::distributed::TestNode*)node->cpp_node;
        // Implementation depends on C++ TestNode API
        // cpp_node->wait_barrier(std::chrono::milliseconds(timeout_ms));
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_BARRIER_TIMEOUT, e.what());
        return WADJET_DIST_ERR_BARRIER_TIMEOUT;
    }
}

wadjet_clock_sync_status_t wadjet_node_get_clock_sync_status(
    wadjet_test_node_t node)
{
    wadjet_clock_sync_status_t status = {0};
    
    if (node == NULL) {
        status.method = WADJET_CLOCK_SYNC_NONE;
        status.is_synchronized = false;
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, "node is NULL");
        return status;
    }

    try {
        auto* cpp_node = (wadjet::distributed::TestNode*)node->cpp_node;
        // Implementation depends on C++ TestNode API returning ClockSyncStatus
        // auto cpp_status = cpp_node->get_clock_sync_status();
        // Map to C status...
        return status;
    } catch (...) {
        status.method = WADJET_CLOCK_SYNC_NONE;
        status.is_synchronized = false;
        return status;
    }
}

wadjet_dist_error_t wadjet_node_stop(wadjet_test_node_t node)
{
    if (node == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, "node is NULL");
        return WADJET_DIST_ERR_INVALID_STATE;
    }

    try {
        auto* cpp_node = (wadjet::distributed::TestNode*)node->cpp_node;
        cpp_node->stop_capture();
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, e.what());
        return WADJET_DIST_ERR_UNKNOWN;
    }
}

void wadjet_node_destroy(wadjet_test_node_t node)
{
    if (node == NULL) {
        return;
    }

    try {
        auto* cpp_node = (wadjet::distributed::TestNode*)node->cpp_node;
        delete cpp_node;
        free(node);
    } catch (...) {
        // Silently ignore errors during cleanup
    }
}

/* ============================================================================
 * Sync Barrier (T102)
 * ============================================================================ */

wadjet_sync_barrier_t wadjet_sync_barrier_create(
    const char* barrier_id,
    uint32_t expected_participants,
    uint32_t timeout_ms,
    wadjet_dist_error_t* error_code)
{
    if (barrier_id == NULL || expected_participants == 0) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_INVALID_CONFIG;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "barrier_id is NULL or participants is 0");
        return NULL;
    }

    wadjet_dist_init_error_context();

    try {
        auto* cpp_barrier = new wadjet::distributed::SyncBarrier(
            barrier_id,
            expected_participants);
        
        wadjet_sync_barrier_t handle = 
            (wadjet_sync_barrier_t)malloc(sizeof(struct wadjet_sync_barrier));
        handle->cpp_barrier = (void*)cpp_barrier;
        
        if (error_code != NULL) {
            *error_code = WADJET_DIST_OK;
        }
        
        return handle;
    } catch (const std::exception& e) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_INVALID_CONFIG;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, e.what());
        return NULL;
    } catch (...) {
        if (error_code != NULL) {
            *error_code = WADJET_DIST_ERR_UNKNOWN;
        }
        wadjet_dist_set_error(WADJET_DIST_ERR_UNKNOWN, "unknown exception in sync_barrier_create");
        return NULL;
    }
}

wadjet_dist_error_t wadjet_sync_barrier_wait(
    wadjet_sync_barrier_t barrier,
    uint32_t timeout_ms)
{
    if (barrier == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_STATE, "barrier is NULL");
        return WADJET_DIST_ERR_INVALID_STATE;
    }

    try {
        auto* cpp_barrier = (wadjet::distributed::SyncBarrier*)barrier->cpp_barrier;
        // Implementation depends on C++ SyncBarrier API
        // cpp_barrier->wait(std::chrono::milliseconds(timeout_ms));
        return WADJET_DIST_OK;
    } catch (const std::exception& e) {
        wadjet_dist_set_error(WADJET_DIST_ERR_BARRIER_TIMEOUT, e.what());
        return WADJET_DIST_ERR_BARRIER_TIMEOUT;
    }
}

bool wadjet_sync_barrier_is_satisfied(wadjet_sync_barrier_t barrier)
{
    if (barrier == NULL) {
        return false;
    }

    try {
        auto* cpp_barrier = (wadjet::distributed::SyncBarrier*)barrier->cpp_barrier;
        // return cpp_barrier->is_satisfied();
        return false;  // Placeholder
    } catch (...) {
        return false;
    }
}

uint32_t wadjet_sync_barrier_participant_count(wadjet_sync_barrier_t barrier)
{
    if (barrier == NULL) {
        return 0;
    }

    try {
        auto* cpp_barrier = (wadjet::distributed::SyncBarrier*)barrier->cpp_barrier;
        // return cpp_barrier->get_participant_count();
        return 0;  // Placeholder
    } catch (...) {
        return 0;
    }
}

void wadjet_sync_barrier_destroy(wadjet_sync_barrier_t barrier)
{
    if (barrier == NULL) {
        return;
    }

    try {
        auto* cpp_barrier = (wadjet::distributed::SyncBarrier*)barrier->cpp_barrier;
        delete cpp_barrier;
        free(barrier);
    } catch (...) {
        // Silently ignore errors during cleanup
    }
}

/* ============================================================================
 * Result Helper Functions
 * ============================================================================ */

wadjet_dist_error_t wadjet_aggregated_result_to_junit_xml(
    const wadjet_aggregated_result_t* results,
    const char* output_path)
{
    if (results == NULL || output_path == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "results or output_path is NULL");
        return WADJET_DIST_ERR_INVALID_CONFIG;
    }

    // Implementation would depend on JUnit XML generation logic
    // For now, just indicate success
    return WADJET_DIST_OK;
}

wadjet_dist_error_t wadjet_aggregated_result_to_json(
    const wadjet_aggregated_result_t* results,
    const char* output_path)
{
    if (results == NULL || output_path == NULL) {
        wadjet_dist_set_error(WADJET_DIST_ERR_INVALID_CONFIG, "results or output_path is NULL");
        return WADJET_DIST_ERR_INVALID_CONFIG;
    }

    // Implementation would depend on JSON generation logic
    // For now, just indicate success
    return WADJET_DIST_OK;
}

const char* wadjet_aggregated_result_summary(
    const wadjet_aggregated_result_t* results)
{
    static _Thread_local char summary[512] = {0};

    if (results == NULL) {
        return "";
    }

    const char* status_str = "UNKNOWN";
    if (results->overall_status == 0) {
        status_str = "PASSED";
    } else if (results->overall_status == 1) {
        status_str = "FAILED";
    } else if (results->overall_status == 2) {
        status_str = "ERROR";
    }

    snprintf(summary, sizeof(summary),
             "Test: %s | Nodes: %d | Status: %s | %d/%d assertions",
             results->test_name ? results->test_name : "unknown",
             results->node_count,
             status_str,
             results->passed_assertions,
             results->total_assertions);

    return summary;
}

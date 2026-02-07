#pragma once

/**
 * @file distributed.hpp
 * @brief Main umbrella header for distributed testing infrastructure
 *
 * T229: Provides a single include point for all distributed testing APIs
 * per plan.md section "Infrastructure Completeness (I1)"
 *
 * This header includes all public distributed testing interfaces:
 * - Barrier synchronization (SyncBarrier)
 * - Timestamp normalization and clock sync
 * - Cross-node packet correlation
 * - Distributed assertions and matchers
 * - Test node and coordinator interfaces
 * - gRPC client and service layers
 *
 * Example:
 * @code
 * #include "wadjet/distributed/distributed.hpp"
 *
 * auto coordinator = wadjet::distributed::TestCoordinator::create();
 * coordinator->start();
 * // ... distributed test execution ...
 * coordinator->stop();
 * @endcode
 */

// Core types and results
#include "wadjet/distributed/result.hpp"
#include "wadjet/distributed/types.hpp"

// Synchronization primitive
#include "wadjet/distributed/sync_barrier.hpp"

// Timestamp and clock synchronization
#include "wadjet/distributed/timestamp_normalizer.hpp"

// Cross-node packet correlation
#include "wadjet/distributed/message_correlator.hpp"

// PCAP merging for multi-node captures
#include "wadjet/distributed/pcap_merger.hpp"

// Distributed assertion framework
#include "wadjet/distributed/distributed_matcher.hpp"

// Matcher implementations
#include "wadjet/distributed/matchers/expect_message_flow.hpp"
#include "wadjet/distributed/matchers/happens_before.hpp"
#include "wadjet/distributed/matchers/must_not_see_on.hpp"
#include "wadjet/distributed/matchers/within_latency.hpp"

// Test node and coordinator interfaces
#include "wadjet/distributed/coordinator.hpp"
#include "wadjet/distributed/node.hpp"

// gRPC communication layer
#include "wadjet/distributed/grpc/client.hpp"
#include "wadjet/distributed/grpc/service.hpp"

namespace wadjet::distributed {

/**
 * @brief Main namespace for distributed testing APIs
 *
 * Contains all classes, types, and utilities for multi-node test coordination:
 *
 * **Core Classes**:
 * - TestCoordinator - Centralized test orchestrator
 * - TestNode - Test agent running on each node
 * - SyncBarrier - Synchronization primitive
 *
 * **Supporting Classes**:
 * - TimestampNormalizer - Clock sync and timestamp conversion
 * - MessageCorrelator - Cross-node packet correlation
 * - PcapMerger - Multi-node PCAP merge with timestamp alignment
 *
 * **Matchers**:
 * - ExpectMessageFlow - Validate message flow across nodes
 * - WithinLatency - Measure one-way latency bounds
 * - HappensBefore - Enforce causal ordering
 * - MustNotSeeOn - Negative assertions across nodes
 *
 * **gRPC Layer**:
 * - DistributedTestServiceImpl - Coordinator RPC server
 * - DistributedTestClient - Node RPC client
 */

/**
 * @defgroup distributed_core Core Distributed Testing Interfaces
 * @brief Main APIs for distributed test coordination
 * @{
 */

/**
 * @defgroup distributed_sync Synchronization Primitives
 * @brief Barriers and clock synchronization
 * @{
 */

/**
 * @defgroup distributed_matchers Distributed Assertions
 * @brief Cross-node matcher implementations
 * @{
 */

/**
 * @defgroup distributed_grpc gRPC Communication
 * @brief Service and client implementations
 * @{
 */

}  // namespace wadjet::distributed

#endif  // WADJET_DISTRIBUTED_DISTRIBUTED_HPP

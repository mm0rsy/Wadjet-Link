#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace wadjet::distributed {

/**
 * @brief Node health status enumeration
 * 
 * T253: Enumeration of node health states per data-model.md
 */
enum class NodeHealthStatus {
    Unknown,        ///< Health status unknown
    Healthy,        ///< Node is healthy and responsive
    Degraded,       ///< Node has degraded health (clock drift, high latency)
    Unhealthy,      ///< Node is unhealthy (heartbeat timeout)
    Disconnected    ///< Node is disconnected
};

/**
 * @brief Capture state enumeration
 * 
 * T254: Enumeration of capture states per data-model.md
 */
enum class CaptureState {
    Idle,           ///< No capture in progress
    Starting,       ///< Capture is starting
    Running,        ///< Capture is actively running
    Stopping,       ///< Capture is stopping
    Stopped,        ///< Capture has stopped
    Error           ///< Capture error occurred
};

/**
 * @brief Barrier state enumeration
 * 
 * T255: Enumeration of barrier states per data-model.md
 */
enum class BarrierState {
    Waiting,        ///< Waiting for nodes to arrive
    AllArrived,     ///< All nodes have arrived
    Timeout,        ///< Barrier timeout occurred
    Cancelled       ///< Barrier was cancelled
};

/**
 * @brief Node identifier type
 */
using NodeId = std::string;

/**
 * @brief Information about a test node
 * 
 * T016: Node information for registration and discovery
 */
struct NodeInfo {
    NodeId id;                                      ///< Unique node identifier
    std::string hostname;                           ///< Hostname or IP address
    uint16_t grpc_port;                            ///< gRPC service port
    std::vector<std::string> capture_interfaces;   ///< Network interfaces for capture
    std::string version;                           ///< Protocol version
    std::map<std::string, std::string> metadata;   ///< Additional metadata
    
    /// Check if node info is valid
    auto is_valid() const -> bool {
        return !id.empty() && !hostname.empty() && grpc_port > 0;
    }
};

/**
 * @brief Capture configuration for a test
 */
struct CaptureConfig {
    std::string filter_expression;          ///< BPF filter for packet capture
    uint32_t snaplen = 65535;               ///< Snapshot length for captured packets
    bool promiscuous = true;                ///< Promiscuous mode
    int64_t duration_ns = 0;                ///< Capture duration (0 = until stopped)
};

/**
 * @brief Node capture result
 */
struct NodeCaptureResult {
    NodeId node_id;
    int64_t packet_count = 0;
    int64_t byte_count = 0;
    std::string pcap_path;
    int64_t start_time_ns = 0;
    int64_t end_time_ns = 0;
    bool success = false;
};

}  // namespace wadjet::distributed

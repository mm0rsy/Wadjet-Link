#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Node health status enumeration
 *
 * T253: Enumeration of node health states per data-model.md
 */
enum class NodeHealthStatus {
    Unknown,      ///< Health status unknown
    Healthy,      ///< Node is healthy and responsive
    Degraded,     ///< Node has degraded health (clock drift, high latency)
    Unhealthy,    ///< Node is unhealthy (heartbeat timeout)
    Disconnected  ///< Node is disconnected
};

/**
 * @brief Capture state enumeration
 *
 * T254: Enumeration of capture states per data-model.md
 */
enum class CaptureState {
    Idle,      ///< No capture in progress
    Starting,  ///< Capture is starting
    Running,   ///< Capture is actively running
    Stopping,  ///< Capture is stopping
    Stopped,   ///< Capture has stopped
    Error      ///< Capture error occurred
};

/**
 * @brief Barrier state enumeration
 *
 * T255: Enumeration of barrier states per data-model.md
 */
enum class BarrierState {
    Waiting,     ///< Waiting for nodes to arrive
    AllArrived,  ///< All nodes have arrived
    Timeout,     ///< Barrier timeout occurred
    Cancelled    ///< Barrier was cancelled
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
    NodeId id;                                    ///< Unique node identifier
    std::string hostname;                         ///< Hostname or IP address
    uint16_t grpc_port;                           ///< gRPC service port
    std::vector<std::string> capture_interfaces;  ///< Network interfaces for capture
    std::string version;                          ///< Protocol version
    std::map<std::string, std::string> metadata;  ///< Additional metadata

    /// Check if node info is valid
    auto is_valid() const -> bool { return !id.empty() && !hostname.empty() && grpc_port > 0; }
};

/**
 * @brief Capture configuration for a test
 */
struct CaptureConfig {
    std::string test_name;          ///< T296: Test name for PCAP filename
    std::string filter_expression;  ///< BPF filter for packet capture
    uint32_t snaplen = 65535;       ///< Snapshot length for captured packets
    bool promiscuous = true;        ///< Promiscuous mode
    int64_t duration_ns = 0;        ///< Capture duration (0 = until stopped)
};

/**
 * @brief Node capture result
 *
 * T038: Result of packet capture on a node
 * T153: Performance metrics (throughput, packet loss)
 */
struct NodeCaptureResult {
    NodeId node_id;
    int64_t packet_count = 0;
    int64_t byte_count = 0;
    std::string pcap_path;
    int64_t start_time_ns = 0;
    int64_t end_time_ns = 0;
    bool success = false;

    /**
     * @brief T153: Calculated throughput in packets per second
     *
     * Computed from: packet_count / duration_seconds
     */
    double throughput_packets_per_sec = 0.0;

    /**
     * @brief T154: Count of lost packets (if detectable via gaps)
     *
     * Based on sequence number gaps in captured packets
     */
    int64_t packet_loss_count = 0;

    /**
     * @brief Calculate throughput from capture duration
     *
     * T155: Helper method for performance metrics calculation
     *
     * @return Throughput in packets/sec (0 if duration is 0)
     */
    double calculate_throughput() const {
        if (start_time_ns >= end_time_ns) {
            return 0.0;
        }
        int64_t duration_ns = end_time_ns - start_time_ns;
        double duration_sec = duration_ns / 1e9;
        return duration_sec > 0 ? packet_count / duration_sec : 0.0;
    }

    /**
     * @brief Calculate packet loss percentage
     *
     * T155: Helper method for performance metrics calculation
     *
     * @return Packet loss as percentage (0.0-100.0)
     */
    double calculate_packet_loss_percentage() const {
        if (packet_count + packet_loss_count == 0) {
            return 0.0;
        }
        return (100.0 * packet_loss_count) / (packet_count + packet_loss_count);
    }
};

}  // namespace wadjet::distributed

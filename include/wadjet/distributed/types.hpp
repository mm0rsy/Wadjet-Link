#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace wadjet::distributed {

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

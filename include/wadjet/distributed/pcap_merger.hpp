#pragma once

/// @file pcap_merger.hpp
/// @brief Multi-node PCAP file merger with timestamp-based sorting

#include "wadjet/core/result.hpp"
#include "wadjet/net/packet.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Options for PCAP merging
 */
struct PcapMergerOptions {
    bool maintain_capture_order = false;  ///< Preserve original capture order per node
    bool skip_reordering = false;         ///< Don't reorder packets by timestamp
    bool add_metadata_packets = true;     ///< Add metadata about source nodes
    int64_t timestamp_tolerance_ns = 0;   ///< Tolerance for timestamp-based ordering
};

/**
 * @brief Merged PCAP result with metadata
 */
struct MergedPcapResult {
    std::filesystem::path output_path;      ///< Path to output PCAP file
    int64_t total_packets = 0;              ///< Total packets in merged file
    int64_t total_bytes = 0;                ///< Total bytes in merged file
    std::vector<std::string> source_nodes;  ///< Nodes that contributed packets
    std::vector<int64_t> packets_per_node;  ///< Packet count per source node
};

/**
 * @brief Merger for multi-node PCAP captures
 *
 * Merges PCAP files from multiple nodes while preserving timestamp information
 * and optionally adding metadata about packet sources.
 *
 * T047-T048: Implements PCAP merging with timestamp-sorted output
 */
class PcapMerger {
public:
    /**
     * @brief Create a new PCAP merger
     *
     * @param options Merge options
     */
    explicit PcapMerger(const PcapMergerOptions& options = PcapMergerOptions{});

    ~PcapMerger();

    /**
     * @brief Add a PCAP file from a node
     *
     * @param node_id Identifier of the node this PCAP came from
     * @param pcap_path Path to the PCAP file
     * @return Success or error
     */
    auto add_capture(const std::string& node_id,
                     const std::filesystem::path& pcap_path) -> wadjet::Result<void>;

    /**
     * @brief Add pre-captured packets from a node
     *
     * @param node_id Identifier of the node
     * @param packets Vector of captured packets
     * @return Success or error
     */
    auto add_packets(const std::string& node_id,
                     const std::vector<Packet>& packets) -> wadjet::Result<void>;

    /**
     * @brief Merge all added captures into output file
     *
     * T048: Performs timestamp-sorted merge of all packets
     *
     * @param output_path Path where to write merged PCAP
     * @return Merge result with metadata
     */
    auto merge(const std::filesystem::path& output_path) -> wadjet::Result<MergedPcapResult>;

    /**
     * @brief Get number of nodes added
     *
     * @return Number of unique nodes
     */
    [[nodiscard]] auto node_count() const -> std::size_t;

    /**
     * @brief Get total packet count across all nodes
     *
     * @return Total packets added
     */
    [[nodiscard]] auto total_packets() const -> int64_t;

    /**
     * @brief Clear all added captures
     */
    auto clear() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed

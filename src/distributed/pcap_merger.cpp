#include "wadjet/distributed/pcap_merger.hpp"

#include "wadjet/pcap/pcap_reader.hpp"
#include "wadjet/pcap/pcap_writer.hpp"

#include <algorithm>
#include <map>
#include <vector>

namespace wadjet::distributed {

/**
 * @brief Implementation of PcapMerger
 */
struct PcapMerger::Impl {
    struct NodeCapture {
        std::string node_id;
        std::vector<Packet> packets;
        int64_t total_bytes = 0;
    };
    
    PcapMergerOptions options;
    std::map<std::string, NodeCapture> node_captures;
    
    explicit Impl(const PcapMergerOptions& opts) : options(opts) {}
};

// Constructor
PcapMerger::PcapMerger(const PcapMergerOptions& options)
    : impl_(std::make_unique<Impl>(options)) {}

// Destructor
PcapMerger::~PcapMerger() = default;

// Add a PCAP file from a node
auto PcapMerger::add_capture(const std::string& node_id, const std::filesystem::path& pcap_path)
    -> wadjet::Result<void> {
    if (node_id.empty()) {
        return wadjet::Result<void>::err(
            wadjet::Error(-1, "Node ID cannot be empty"));
    }
    
    if (!std::filesystem::exists(pcap_path)) {
        return wadjet::Result<void>::err(
            wadjet::Error(-1, "PCAP file does not exist: " + pcap_path.string()));
    }
    
    // Create entry if doesn't exist
    if (impl_->node_captures.find(node_id) == impl_->node_captures.end()) {
        impl_->node_captures[node_id].node_id = node_id;
    }

    // T222: Read PCAP file using PcapReader
    try {
        auto reader_result = pcap::PcapReader::create(pcap_path);
        if (!reader_result) {
            return wadjet::Result<void>::err(
                wadjet::Error(-1, "Failed to open PCAP file: " + pcap_path.string()));
        }

        auto& reader = reader_result.value();

        // Read all packets from the PCAP file
        std::vector<Packet> packets;
        while (auto packet = reader.next_packet()) {
            packets.push_back(packet.value());
        }

        // Add packets to the node capture
        auto& node_capture = impl_->node_captures[node_id];
        node_capture.packets.insert(node_capture.packets.end(), packets.begin(), packets.end());

        // Update byte count
        for (const auto& packet : packets) {
            node_capture.total_bytes += static_cast<int64_t>(packet.data().size());
        }

        return wadjet::Result<void>::ok();
    } catch (const std::exception& e) {
        return wadjet::Result<void>::err(
            wadjet::Error(-1, std::string("Exception reading PCAP: ") + e.what()));
    }
}

// Add pre-captured packets from a node
auto PcapMerger::add_packets(const std::string& node_id, const std::vector<Packet>& packets)
    -> wadjet::Result<void> {
    
    if (node_id.empty()) {
        return wadjet::Result<void>::err(
            wadjet::Error(-1, "Node ID cannot be empty"));
    }
    
    // Create entry if doesn't exist
    if (impl_->node_captures.find(node_id) == impl_->node_captures.end()) {
        impl_->node_captures[node_id].node_id = node_id;
    }
    
    auto& node_capture = impl_->node_captures[node_id];
    node_capture.packets.insert(node_capture.packets.end(), packets.begin(), packets.end());
    
    // Update byte count
    for (const auto& packet : packets) {
        node_capture.total_bytes += static_cast<int64_t>(packet.data().size());
    }
    
    return wadjet::Result<void>::ok();
}

// Merge all captures into output file
auto PcapMerger::merge(const std::filesystem::path& output_path)
    -> wadjet::Result<MergedPcapResult> {
    
    // Collect all packets from all nodes
    struct PacketWithSource {
        const Packet* packet;
        std::string node_id;
        size_t order_index;  // Original order in node's capture
    };
    
    std::vector<PacketWithSource> all_packets;
    
    // Collect packets from each node
    for (auto& [node_id, node_cap] : impl_->node_captures) {
        for (size_t i = 0; i < node_cap.packets.size(); ++i) {
            all_packets.push_back({
                &node_cap.packets[i],
                node_id,
                i
            });
        }
    }
    
    // T048: Sort packets by timestamp (ascending)
    if (!impl_->options.skip_reordering) {
        std::stable_sort(all_packets.begin(), all_packets.end(),
            [](const PacketWithSource& a, const PacketWithSource& b) {
                auto ts_a = a.packet->timestamp().total_nanoseconds();
                auto ts_b = b.packet->timestamp().total_nanoseconds();
                if (ts_a != ts_b) {
                    return ts_a < ts_b;
                }
                // If timestamps are equal, maintain node order
                return a.node_id < b.node_id;
            });
    }

    // T223: Write merged packets to output PCAP file using pcap::PcapWriter
    try {
        auto writer_result = pcap::PcapWriter::create(output_path);
        if (!writer_result) {
            return wadjet::Result<MergedPcapResult>::err(
                wadjet::Error(-1, "Failed to create PCAP output file: " + output_path.string()));
        }

        auto& writer = writer_result.value();

        // Write all packets to output file
        for (const auto& packet_with_source : all_packets) {
            if (!writer.write_packet(*packet_with_source.packet)) {
                return wadjet::Result<MergedPcapResult>::err(
                    wadjet::Error(-1, "Failed to write packet to PCAP file"));
            }
        }
    } catch (const std::exception& e) {
        return wadjet::Result<MergedPcapResult>::err(
            wadjet::Error(-1, std::string("Exception writing PCAP: ") + e.what()));
    }

    MergedPcapResult result;
    result.output_path = output_path;
    result.total_packets = static_cast<int64_t>(all_packets.size());
    result.total_bytes = 0;
    
    // Calculate total bytes and per-node counts
    std::map<std::string, int64_t> packets_per_node;
    for (const auto& pws : all_packets) {
        result.total_bytes += static_cast<int64_t>(pws.packet->data().size());
        packets_per_node[pws.node_id]++;
    }
    
    // Build result metadata
    for (const auto& [node_id, _] : impl_->node_captures) {
        result.source_nodes.push_back(node_id);
        if (packets_per_node.count(node_id)) {
            result.packets_per_node.push_back(packets_per_node[node_id]);
        } else {
            result.packets_per_node.push_back(0);
        }
    }
    
    return wadjet::Result<MergedPcapResult>::ok(result);
}

// Get number of nodes
auto PcapMerger::node_count() const -> std::size_t {
    return impl_->node_captures.size();
}

// Get total packet count
auto PcapMerger::total_packets() const -> int64_t {
    int64_t count = 0;
    for (const auto& [_, node_cap] : impl_->node_captures) {
        count += static_cast<int64_t>(node_cap.packets.size());
    }
    return count;
}

// Clear all captures
auto PcapMerger::clear() -> void {
    impl_->node_captures.clear();
}

}  // namespace wadjet::distributed

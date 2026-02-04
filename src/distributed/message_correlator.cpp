#include "wadjet/distributed/message_correlator.hpp"

#include <map>
#include <unordered_map>
#include <openssl/sha.h>

namespace wadjet::distributed {

/**
 * @brief Implementation of MessageCorrelator
 */
struct MessageCorrelator::Impl {
    CorrelationMethod method;
    CorrelationFunc custom_func;
    
    // Stored packets indexed by node_id
    std::unordered_map<std::string, std::vector<net::Packet>> node_packets;
    
    // Correlation ID map for quick lookup
    std::multimap<std::string, std::pair<std::string, net::Packet>> id_to_packet;
    
    explicit Impl(CorrelationMethod m) : method(m) {}
    explicit Impl(CorrelationFunc f) : method(CorrelationMethod::Custom), custom_func(std::move(f)) {}
};

// Constructor with method
MessageCorrelator::MessageCorrelator(CorrelationMethod method)
    : impl_(std::make_unique<Impl>(method)) {}

// Constructor with custom function
MessageCorrelator::MessageCorrelator(CorrelationFunc func)
    : impl_(std::make_unique<Impl>(std::move(func))) {}

// Destructor
MessageCorrelator::~MessageCorrelator() = default;

// Move constructor
MessageCorrelator::MessageCorrelator(MessageCorrelator&&) noexcept = default;

// Move assignment
MessageCorrelator& MessageCorrelator::operator=(MessageCorrelator&&) noexcept = default;

// T044-T045: Add packets from a node
auto MessageCorrelator::add_packets(const std::string& node_id,
                                   std::span<const net::Packet> packets) -> void {
    impl_->node_packets[node_id] = std::vector<net::Packet>(packets.begin(), packets.end());
    
    // Pre-compute correlation IDs for quick lookup
    for (const auto& packet : packets) {
        std::optional<std::string> corr_id;
        
        switch (impl_->method) {
            case CorrelationMethod::PayloadHash: {
                // T044: Compute SHA-256 of payload
                unsigned char hash[SHA256_DIGEST_LENGTH];
                SHA256_CTX sha256;
                SHA256_Init(&sha256);
                SHA256_Update(&sha256, packet.data.data(), packet.data.size());
                SHA256_Final(hash, &sha256);
                
                // Convert to hex string
                char hash_str[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
                for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                    snprintf(hash_str + (i * 2), 3, "%02x", hash[i]);
                }
                corr_id = std::string(hash_str);
                break;
            }
            case CorrelationMethod::SequenceNumber: {
                // T045: Extract sequence number from packet (placeholder)
                // In real implementation, would parse protocol headers
                corr_id = "seq_" + std::to_string(reinterpret_cast<uintptr_t>(&packet));
                break;
            }
            case CorrelationMethod::TransactionId: {
                // Placeholder: would extract transaction ID from protocol
                corr_id = "txn_" + std::to_string(reinterpret_cast<uintptr_t>(&packet));
                break;
            }
            case CorrelationMethod::Timestamp: {
                // Placeholder: group by timestamp proximity
                corr_id = "ts_";
                break;
            }
            case CorrelationMethod::Custom: {
                // Use custom function
                if (impl_->custom_func) {
                    corr_id = impl_->custom_func(packet.view());
                }
                break;
            }
        }
        
        if (corr_id) {
            impl_->id_to_packet.emplace(corr_id.value(), std::make_pair(node_id, packet));
        }
    }
}

// T044-T045: Find all correlated packets
auto MessageCorrelator::correlate() -> std::vector<CorrelatedPackets> {
    std::vector<CorrelatedPackets> result;
    
    // Group packets by correlation ID
    std::map<std::string, std::vector<std::pair<std::string, net::Packet>>> groups;
    
    for (const auto& [corr_id, node_packet] : impl_->id_to_packet) {
        groups[corr_id].push_back(node_packet);
    }
    
    // Create result for each group with packets from multiple nodes
    for (const auto& [corr_id, packets] : groups) {
        if (packets.size() > 1) {  // Only report if found on multiple nodes
            CorrelatedPackets corr;
            corr.correlation_id = corr_id;
            corr.node_packets = packets;
            result.push_back(corr);
        }
    }
    
    return result;
}

// Find specific correlation
auto MessageCorrelator::find_correlation(const net::PacketView& source_packet,
                                        const std::string& target_node)
    -> std::optional<net::Packet> {
    // Create temporary packet from view to compute correlation ID
    net::Packet source_pkt{source_packet.data(), source_packet.size()};
    
    // Compute correlation ID
    std::optional<std::string> corr_id;
    
    switch (impl_->method) {
        case CorrelationMethod::PayloadHash: {
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256_CTX sha256;
            SHA256_Init(&sha256);
            SHA256_Update(&sha256, source_packet.data(), source_packet.size());
            SHA256_Final(hash, &sha256);
            
            char hash_str[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
            for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                snprintf(hash_str + (i * 2), 3, "%02x", hash[i]);
            }
            corr_id = std::string(hash_str);
            break;
        }
        default:
            break;
    }
    
    if (!corr_id) {
        return std::nullopt;
    }
    
    // Search for matching packet on target node
    auto range = impl_->id_to_packet.equal_range(corr_id.value());
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.first == target_node) {
            return it->second.second;
        }
    }
    
    return std::nullopt;
}

// Get method
auto MessageCorrelator::get_method() const -> CorrelationMethod {
    return impl_->method;
}

// Clear packets
auto MessageCorrelator::clear() -> void {
    impl_->node_packets.clear();
    impl_->id_to_packet.clear();
}

}  // namespace wadjet::distributed

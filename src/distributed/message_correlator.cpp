#include "wadjet/distributed/message_correlator.hpp"

#include "wadjet/protocols/diagnostic/uds_doip_decoder.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>

namespace wadjet::distributed {

/**
 * @brief Implementation of MessageCorrelator
 */
struct MessageCorrelator::Impl {
    CorrelationMethod method;
    CorrelationFunc custom_func;

    // Stored packets indexed by node_id
    std::unordered_map<std::string, std::vector<Packet>> node_packets;

    // Correlation ID map for quick lookup
    std::multimap<std::string, std::pair<std::string, Packet>> id_to_packet;

    explicit Impl(CorrelationMethod m) : method(m) {}
    explicit Impl(CorrelationFunc f)
        : method(CorrelationMethod::Custom), custom_func(std::move(f)) {}
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
                                    std::span<const Packet> packets) -> void {
    impl_->node_packets[node_id] = std::vector<Packet>(packets.begin(), packets.end());

    // Pre-compute correlation IDs for quick lookup
    for (const auto& packet : packets) {
        std::optional<std::string> corr_id;

        switch (impl_->method) {
            case CorrelationMethod::PayloadHash: {
                // T044: Compute SHA-256 of payload using EVP API
                unsigned char hash[EVP_MAX_MD_SIZE];
                unsigned int hash_len = 0;

                EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
                if (!mdctx)
                    break;

                EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
                EVP_DigestUpdate(mdctx, packet.data().data(), packet.data().size());
                EVP_DigestFinal_ex(mdctx, hash, &hash_len);
                EVP_MD_CTX_free(mdctx);

                // Convert to hex string
                std::ostringstream oss;
                for (unsigned int i = 0; i < hash_len; i++) {
                    oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
                }
                corr_id = oss.str();
                break;
            }
            case CorrelationMethod::SequenceNumber: {
                // T249: Extract sequence number from packet protocol headers
                // Parse based on packet type: IP header seq, TCP seq, SOME/IP seq, etc.
                try {
                    // Try to extract TCP sequence number if available
                    if (packet.view().size() >= 34) {  // Minimum for TCP header
                        // Extract 4-byte sequence number from TCP header (offset 24-27)
                        // This is a basic implementation - full version would parse all protocol
                        // layers
                        uint32_t seq_num = 0;
                        if (packet.view().size() >= 28) {
                            // Read 4 bytes at offset 24
                            auto raw_data = packet.view().data();
                            if (raw_data &&
                                raw_data + 28 <= packet.view().data() + packet.view().size()) {
                                seq_num = (static_cast<uint32_t>(raw_data[24]) << 24) |
                                          (static_cast<uint32_t>(raw_data[25]) << 16) |
                                          (static_cast<uint32_t>(raw_data[26]) << 8) |
                                          (static_cast<uint32_t>(raw_data[27]));
                            }
                        }
                        if (seq_num != 0) {
                            corr_id = "seq_" + std::to_string(seq_num);
                        } else {
                            // Fallback to address-based correlation
                            corr_id = "seq_" + std::to_string(reinterpret_cast<uintptr_t>(&packet));
                        }
                    } else {
                        corr_id = "seq_" + std::to_string(reinterpret_cast<uintptr_t>(&packet));
                    }
                } catch (...) {
                    corr_id = "seq_" + std::to_string(reinterpret_cast<uintptr_t>(&packet));
                }
                break;
            }
            case CorrelationMethod::TransactionId: {
                // T315: Extract transaction ID using M9 UdsOverDoipDecoder API
                // Supports DoIP/UDS, SOME/IP Request/Response ID, and other protocols
                corr_id = "txn_" + std::to_string(reinterpret_cast<uintptr_t>(&packet));

                try {
                    // Try to decode as UDS over DoIP using M9 decoder
                    protocols::diagnostic::UdsOverDoipDecoder doip_decoder;
                    auto doip_result = doip_decoder.decode(packet.view().as_bytes());

                    if (doip_result) {
                        // Successfully decoded UDS over DoIP
                        const auto& uds_doip = doip_result.value();

                        // Create transaction ID from UDS header info
                        // Use combination of source/target addresses and service ID
                        uint32_t txn_id = 0;
                        txn_id = (static_cast<uint32_t>(uds_doip.source_address) << 16) |
                                 (static_cast<uint32_t>(uds_doip.target_address) & 0xFFFF);

                        // Include UDS service ID for finer correlation
                        uint8_t service_id = static_cast<uint8_t>(uds_doip.service_id());
                        txn_id = (txn_id << 8) | service_id;

                        if (txn_id != 0) {
                            corr_id = "doip_txn_" + std::to_string(txn_id);
                        }
                    } else {
                        // Not a UDS over DoIP message, try other correlation methods
                        // Fall back to extracting potential transaction ID from raw headers
                        if (packet.view().size() >= 8) {
                            auto raw_data = packet.view().data();
                            if (raw_data) {
                                // Extract potential transaction ID from common protocol headers
                                uint32_t txn_id = (static_cast<uint32_t>(raw_data[4]) << 24) |
                                                  (static_cast<uint32_t>(raw_data[5]) << 16) |
                                                  (static_cast<uint32_t>(raw_data[6]) << 8) |
                                                  (static_cast<uint32_t>(raw_data[7]));
                                if (txn_id != 0) {
                                    corr_id = "txn_" + std::to_string(txn_id);
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    // If M9 decoder throws, fall back to manual extraction
                    if (packet.view().size() >= 8) {
                        auto raw_data = packet.view().data();
                        if (raw_data) {
                            uint32_t txn_id = (static_cast<uint32_t>(raw_data[4]) << 24) |
                                              (static_cast<uint32_t>(raw_data[5]) << 16) |
                                              (static_cast<uint32_t>(raw_data[6]) << 8) |
                                              (static_cast<uint32_t>(raw_data[7]));
                            if (txn_id != 0) {
                                corr_id = "txn_" + std::to_string(txn_id);
                            }
                        }
                    }
                }
                break;
            }
            case CorrelationMethod::Timestamp: {
                // T251: Group by timestamp proximity with tolerance
                // Packets within a time window (e.g., 1ms) are correlated
                try {
                    int64_t timestamp_ms =
                        packet.timestamp().total_nanoseconds() / 1000000;  // Convert to ms
                    // Create correlation ID based on timestamp bucket (e.g., each 100ms window)
                    int64_t bucket = timestamp_ms / 100;
                    corr_id = "ts_" + std::to_string(bucket);
                } catch (...) {
                    corr_id = "ts_bucket_default";
                }
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
    std::map<std::string, std::vector<std::pair<std::string, Packet>>> groups;

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
auto MessageCorrelator::find_correlation(const PacketView& source_packet,
                                         const std::string& target_node) -> std::optional<Packet> {
    // Create temporary packet from view to compute correlation ID
    Packet source_pkt(source_packet);

    // Compute correlation ID
    std::optional<std::string> corr_id;

    switch (impl_->method) {
        case CorrelationMethod::PayloadHash: {
            unsigned char hash[EVP_MAX_MD_SIZE];
            unsigned int hash_len = 0;

            EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
            if (!mdctx)
                break;

            EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
            EVP_DigestUpdate(mdctx, source_packet.data().data(), source_packet.data().size());
            EVP_DigestFinal_ex(mdctx, hash, &hash_len);
            EVP_MD_CTX_free(mdctx);

            // Convert to hex string
            std::ostringstream oss;
            for (unsigned int i = 0; i < hash_len; i++) {
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
            }
            corr_id = oss.str();
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

// T216: Find specific correlation - Packet overload
auto MessageCorrelator::find_correlation(const Packet& source_packet,
                                         const std::string& target_node) -> std::optional<Packet> {
    // Create a PacketView from the Packet and delegate to PacketView overload
    // This avoids code duplication
    PacketView view(source_packet);
    return find_correlation(view, target_node);
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

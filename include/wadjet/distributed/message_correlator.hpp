#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>

#include "wadjet/net/packet.hpp"

namespace wadjet::distributed {

/**
 * @brief Result of packet correlation across nodes
 * 
 * T043-T045: Represents correlated packets found across multiple nodes
 */
struct CorrelatedPackets {
    std::string correlation_id;                         ///< Generated or extracted ID
    std::vector<std::pair<std::string, Packet>> node_packets;  ///< (node_id, packet) pairs
    std::chrono::nanoseconds max_time_delta{0};         ///< Max timestamp difference
};

/**
 * @brief Message correlator for finding related packets across nodes
 * 
 * Links packets across multiple nodes by content hash, sequence number, or custom function.
 * Used for distributed assertions like "message flows from node A to node B".
 * 
 * T043: MessageCorrelator interface
 * T044: PayloadHash correlation method
 * T045: SequenceNumber correlation method
 */
class MessageCorrelator {
public:
    /**
     * @brief Correlation method enumeration
     */
    enum class CorrelationMethod {
        PayloadHash,        ///< SHA-256 hash of packet payload
        SequenceNumber,     ///< Protocol sequence number
        TransactionId,      ///< Protocol transaction ID
        Timestamp,          ///< Timestamp proximity
        Custom              ///< User-defined correlation function
    };
    
    /// Custom correlation function type
    using CorrelationFunc = std::function<std::optional<std::string>(const PacketView&)>;
    
    /**
     * @brief Create correlator with standard method
     * 
     * @param method Correlation method to use
     */
    explicit MessageCorrelator(CorrelationMethod method = CorrelationMethod::PayloadHash);
    
    /**
     * @brief Create correlator with custom function
     * 
     * @param func Custom correlation function
     */
    explicit MessageCorrelator(CorrelationFunc func);
    
    /// Destructor
    ~MessageCorrelator();
    
    // Prevent copying
    MessageCorrelator(const MessageCorrelator&) = delete;
    MessageCorrelator& operator=(const MessageCorrelator&) = delete;
    
    // Allow moving
    MessageCorrelator(MessageCorrelator&&) noexcept;
    MessageCorrelator& operator=(MessageCorrelator&&) noexcept;
    
    /**
     * @brief Add packets from a node
     * 
     * T044-T045: Adds packets to the pool for correlation
     * 
     * @param node_id Identifier of the node
     * @param packets Span of packets to add
     */
    auto add_packets(const std::string& node_id,
                     std::span<const Packet> packets) -> void;
    
    /**
     * @brief Find all correlated packets across nodes
     * 
     * T044-T045: Performs correlation using the selected method
     * 
     * @return Vector of CorrelatedPackets groups
     */
    auto correlate() -> std::vector<CorrelatedPackets>;
    
    /**
     * @brief Find specific packet correlation
     * 
     * Finds a correlated packet in the target node that matches the source packet
     * 
     * @param source_packet Packet to find correlation for
     * @param target_node Node ID to search in
     * @return Correlated packet if found, nullopt otherwise
     */
    auto find_correlation(const PacketView& source_packet,
                         const std::string& target_node)
        -> std::optional<Packet>;

    /**
     * @brief Find specific packet correlation (Packet overload)
     *
     * T216: Overload for find_correlation that takes a Packet directly.
     * Finds a correlated packet in the target node that matches the source packet.
     *
     * @param source_packet Packet to find correlation for
     * @param target_node Node ID to search in
     * @return Correlated packet if found, nullopt otherwise
     */
    auto find_correlation(const Packet& source_packet,
                          const std::string& target_node) -> std::optional<Packet>;

    /**
     * @brief Get the correlation method in use
     * 
     * @return Current correlation method
     */
    [[nodiscard]] auto get_method() const -> CorrelationMethod;
    
    /**
     * @brief Clear all stored packets
     */
    auto clear() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wadjet::distributed

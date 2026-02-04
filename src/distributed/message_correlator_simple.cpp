#include "wadjet/distributed/message_correlator.hpp"
#include <openssl/evp.h>

namespace wadjet::distributed {

struct MessageCorrelator::Impl {
    CorrelationMethod method{CorrelationMethod::PayloadHash};
    CorrelationFunc custom_func;
    std::multimap<std::string, std::pair<std::string, Packet>> id_to_packet;
};

MessageCorrelator::MessageCorrelator(CorrelationMethod method)
    : impl_(std::make_unique<Impl>()) {
    impl_->method = method;
}

MessageCorrelator::MessageCorrelator(CorrelationFunc func)
    : impl_(std::make_unique<Impl>()) {
    impl_->method = CorrelationMethod::Custom;
    impl_->custom_func = std::move(func);
}

MessageCorrelator::~MessageCorrelator() = default;

void MessageCorrelator::add_packets(const std::string& node_id,
                                     std::span<const Packet> packets) {
    for (const auto& packet : packets) {
        std::optional<std::string> corr_id;
        
        if (impl_->method == CorrelationMethod::PayloadHash) {
            unsigned char hash[EVP_MAX_MD_SIZE];
            unsigned int hash_len = 0;
            EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
            EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
            auto data = packet.data();
            EVP_DigestUpdate(mdctx, data.data(), data.size());
            EVP_DigestFinal_ex(mdctx, hash, &hash_len);
            EVP_MD_CTX_free(mdctx);
            
            char hex[hash_len * 2 + 1];
            for (unsigned int i = 0; i < hash_len; ++i) {
                snprintf(&hex[i * 2], 3, "%02x", hash[i]);
            }
            corr_id = std::string(hex);
        }
        
        if (corr_id) {
            impl_->id_to_packet.emplace(corr_id.value(), std::make_pair(node_id, packet));
        }
    }
}

std::vector<CorrelatedPackets> MessageCorrelator::correlate() {
    std::map<std::string, std::vector<std::pair<std::string, Packet>>> groups;
    
    for (const auto& [corr_id, node_packet] : impl_->id_to_packet) {
        groups[corr_id].push_back(node_packet);
    }
    
    std::vector<CorrelatedPackets> result;
    for (const auto& [corr_id, packets] : groups) {
        CorrelatedPackets corr;
        corr.correlation_id = corr_id;
        corr.node_packets = packets;
        result.push_back(corr);
    }
    return result;
}

std::optional<Packet> MessageCorrelator::find_correlation(const PacketView& source_packet,
                                                           const std::string& target_node) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    auto data = source_packet.data();
    EVP_DigestUpdate(mdctx, data.data(), data.size());
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);
    
    char hex[hash_len * 2 + 1];
    for (unsigned int i = 0; i < hash_len; ++i) {
        snprintf(&hex[i * 2], 3, "%02x", hash[i]);
    }
    
    auto range = impl_->id_to_packet.equal_range(std::string(hex));
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.first == target_node) {
            return it->second.second;
        }
    }
    return std::nullopt;
}

CorrelationMethod MessageCorrelator::get_method() const {
    return impl_->method;
}

void MessageCorrelator::clear() {
    impl_->id_to_packet.clear();
}

}  // namespace wadjet::distributed

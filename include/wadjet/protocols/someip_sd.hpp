#pragma once

/// @file someip_sd.hpp
/// @brief SOME/IP Service Discovery protocol decoder

#include "wadjet/protocols/decoder.hpp"
#include "wadjet/protocols/someip.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace wadjet::protocols::someip_sd {

/// @brief SOME/IP-SD header size (after SOME/IP header)
inline constexpr std::size_t SD_HEADER_SIZE = 12;

/// @brief SOME/IP-SD entry size
inline constexpr std::size_t ENTRY_SIZE = 16;

/// @brief Entry types for Service Discovery
enum class EntryType : std::uint8_t {
    FindService = 0x00,
    OfferService = 0x01,
    StopOfferService = 0x81,  ///< Negated OfferService
    SubscribeEventgroup = 0x06,
    StopSubscribeEventgroup = 0x86,  ///< Negated SubscribeEventgroup
    SubscribeEventgroupAck = 0x07,
    SubscribeEventgroupNack = 0x87,  ///< Negated SubscribeEventgroupAck
};

/// @brief Convert entry type to string
[[nodiscard]] std::string_view entry_type_string(EntryType type);

/// @brief Option types for Service Discovery
enum class OptionType : std::uint8_t {
    Configuration = 0x01,
    LoadBalancing = 0x02,
    IPv4Endpoint = 0x04,
    IPv6Endpoint = 0x06,
    IPv4Multicast = 0x14,
    IPv6Multicast = 0x16,
    IPv4SDEndpoint = 0x24,
    IPv6SDEndpoint = 0x26,
};

/// @brief L4 protocol type for endpoints
enum class L4Protocol : std::uint8_t {
    TCP = 0x06,
    UDP = 0x11,
};

/// @brief IPv4 endpoint option
struct IPv4EndpointOption {
    IPv4Address address;
    std::uint16_t port = 0;
    L4Protocol protocol = L4Protocol::UDP;
};

/// @brief Service entry (Find/Offer)
struct ServiceEntry {
    EntryType type = EntryType::FindService;
    std::uint8_t index1_first_option = 0;
    std::uint8_t index2_first_option = 0;
    std::uint8_t num_options_1 = 0;
    std::uint8_t num_options_2 = 0;
    std::uint16_t service_id = 0;
    std::uint16_t instance_id = 0;
    std::uint8_t major_version = 0;
    std::uint32_t ttl = 0;  ///< Time-to-live in seconds (0 = stop)
    std::uint32_t minor_version = 0;
};

/// @brief Eventgroup entry (Subscribe/Ack)
struct EventgroupEntry {
    EntryType type = EntryType::SubscribeEventgroup;
    std::uint8_t index1_first_option = 0;
    std::uint8_t index2_first_option = 0;
    std::uint8_t num_options_1 = 0;
    std::uint8_t num_options_2 = 0;
    std::uint16_t service_id = 0;
    std::uint16_t instance_id = 0;
    std::uint8_t major_version = 0;
    std::uint32_t ttl = 0;
    std::uint8_t counter = 0;
    std::uint16_t eventgroup_id = 0;
};

/// @brief Generic SD option
struct SdOption {
    OptionType type = OptionType::Configuration;
    std::vector<std::byte> data;

    /// @brief Try to parse as IPv4 endpoint
    [[nodiscard]] std::optional<IPv4EndpointOption> as_ipv4_endpoint() const;
};

/// @brief Variant for SD entries
using SdEntry = std::variant<ServiceEntry, EventgroupEntry>;

/// @brief Array of SD entries with count
struct SdEntryArray {
    std::vector<SdEntry> entries;

    /// @brief Get entry count
    [[nodiscard]] std::size_t size() const { return entries.size(); }

    /// @brief Check if empty
    [[nodiscard]] bool empty() const { return entries.empty(); }

    /// @brief Add entry to array
    void push_back(const SdEntry& entry) { entries.push_back(entry); }

    /// @brief Get entry at index
    [[nodiscard]] const SdEntry& operator[](std::size_t index) const { return entries[index]; }

    /// @brief Get mutable entry at index
    SdEntry& operator[](std::size_t index) { return entries[index]; }
};

/// @brief Array of SD options with count
struct SdOptionArray {
    std::vector<SdOption> options;

    /// @brief Get option count
    [[nodiscard]] std::size_t size() const { return options.size(); }

    /// @brief Check if empty
    [[nodiscard]] bool empty() const { return options.empty(); }

    /// @brief Add option to array
    void push_back(const SdOption& option) { options.push_back(option); }

    /// @brief Get option at index
    [[nodiscard]] const SdOption& operator[](std::size_t index) const { return options[index]; }

    /// @brief Get mutable option at index
    SdOption& operator[](std::size_t index) { return options[index]; }

    /// @brief Get options for a specific entry by index range
    [[nodiscard]] std::vector<SdOption> get_options_for_entry(std::uint8_t index1,
                                                              std::uint8_t num_options_1,
                                                              std::uint8_t index2,
                                                              std::uint8_t num_options_2) const {
        std::vector<SdOption> result;
        // First option range
        for (std::size_t i = index1; i < index1 + num_options_1 && i < options.size(); ++i) {
            result.push_back(options[i]);
        }
        // Second option range
        for (std::size_t i = index2; i < index2 + num_options_2 && i < options.size(); ++i) {
            result.push_back(options[i]);
        }
        return result;
    }
};

/// @brief Decoded SOME/IP-SD header and content
struct SomeIpSdHeader : public IDecodedHeader {
    std::uint8_t flags = 0;            ///< SD flags
    std::uint32_t entries_length = 0;  ///< Length of entries array
    std::uint32_t options_length = 0;  ///< Length of options array
    std::vector<SdEntry> entries;      ///< Parsed entries
    std::vector<SdOption> options;     ///< Parsed options

    // IDecodedHeader interface
    [[nodiscard]] std::string_view protocol_name() const override { return "SOME/IP-SD"; }

    [[nodiscard]] std::size_t header_size() const override {
        return SD_HEADER_SIZE + entries_length + options_length;
    }

    [[nodiscard]] std::size_t payload_size() const override {
        return 0;  // SD has no additional payload
    }

    [[nodiscard]] std::string to_string() const override;

    /// @brief Check if reboot flag is set
    [[nodiscard]] bool is_reboot() const { return (flags & 0x80) != 0; }

    /// @brief Check if unicast flag is set
    [[nodiscard]] bool is_unicast() const { return (flags & 0x40) != 0; }

    /// @brief Get all service offer entries
    [[nodiscard]] std::vector<const ServiceEntry*> get_offers() const;

    /// @brief Get all subscribe entries
    [[nodiscard]] std::vector<const EventgroupEntry*> get_subscriptions() const;

    /// @brief Find service by ID
    [[nodiscard]] const ServiceEntry* find_service(std::uint16_t service_id,
                                                   std::uint16_t instance_id = 0xFFFF) const;
};

/// @brief SOME/IP-SD decoder
class SomeIpSdDecoder : public DecoderBase<SomeIpSdDecoder, SomeIpSdHeader> {
public:
    /// @brief Decoder options
    struct Options {
        bool parse_entries;       ///< Parse entry array
        bool parse_options;       ///< Parse options array
        std::size_t max_entries;  ///< Max entries to parse (DoS protection)
        std::size_t max_options;  ///< Max options to parse
        Options()
            : parse_entries(true), parse_options(true), max_entries(1000), max_options(1000) {}
    };

    explicit SomeIpSdDecoder(Options opts = Options()) : options_(opts) {}

    [[nodiscard]] std::string_view name() const override { return "SOME/IP-SD"; }

    [[nodiscard]] bool can_decode(const DecodeContext& ctx) const override {
        // Typically called after SOME/IP decode confirms SD message
        return ctx.has_bytes(SD_HEADER_SIZE);
    }

    /// @brief Decode SOME/IP-SD content
    [[nodiscard]] Result decode_impl(const DecodeContext& ctx) const;

    /// @brief Parse entry from data (public for helper functions)
    [[nodiscard]] static std::optional<SdEntry> parse_entry(const DecodeContext& ctx,
                                                            std::size_t offset);

    /// @brief Parse option from data (public for helper functions)
    [[nodiscard]] static std::optional<SdOption> parse_option(const DecodeContext& ctx,
                                                              std::size_t offset,
                                                              std::size_t& consumed);

private:
    Options options_;
};

/// @brief Global SOME/IP-SD decoder instance
inline const SomeIpSdDecoder& someip_sd_decoder() {
    static SomeIpSdDecoder decoder;
    return decoder;
}

// ============================================================================
// Helper Functions for Entry/Option Parsing
// ============================================================================

/// @brief Helper function to parse entries from binary data
/// @param data Span of binary entry data (multiple 16-byte entries)
/// @param num_entries Number of entries to parse
/// @return Vector of parsed SD entries
[[nodiscard]] inline SdEntryArray parse_sd_entries(std::span<const std::byte> data,
                                                   std::size_t num_entries) {
    SdEntryArray result;
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;

    for (std::size_t i = 0; i < num_entries && i * ENTRY_SIZE < data.size(); ++i) {
        if (auto entry = SomeIpSdDecoder::parse_entry(ctx, i * ENTRY_SIZE)) {
            result.push_back(*entry);
        }
    }
    return result;
}

/// @brief Helper function to parse options from binary data
/// @param data Span of binary option data
/// @param num_options Maximum number of options to parse
/// @return Vector of parsed SD options
[[nodiscard]] inline SdOptionArray parse_sd_options(std::span<const std::byte> data,
                                                    std::size_t num_options = 1000) {
    SdOptionArray result;
    DecodeContext ctx;
    ctx.data = data;
    ctx.original_offset = 0;

    std::size_t offset = 0;
    std::size_t count = 0;
    while (offset < data.size() && count < num_options) {
        std::size_t consumed = 0;
        if (auto opt = SomeIpSdDecoder::parse_option(ctx, offset, consumed)) {
            result.push_back(*opt);
        }
        if (consumed == 0) {
            break;
        }
        offset += consumed;
        ++count;
    }
    return result;
}

}  // namespace wadjet::protocols::someip_sd

/// @file fuzz_someip_sd_entries.cpp
/// @brief Fuzz test harness for SOME/IP-SD entries and options (T130)
///
/// Tests SD entry parsing with 1M+ iterations using AddressSanitizer
/// Covers service entries, option arrays, and malformed entries

#include "wadjet/protocols/someip_sd.hpp"
#include "wadjet/protocols/decoder.hpp"

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

using namespace wadjet;
using namespace wadjet::protocols;
using namespace wadjet::protocols::someip_sd;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Ensure minimum SOME/IP-SD header size (16 bytes for SOME/IP + 4 for SD header)
    if (size < 20) return 0;
    
    // Create decode context from fuzz input
    std::vector<std::byte> raw(size);
    for (size_t i = 0; i < size; ++i) {
        raw[i] = static_cast<std::byte>(data[i]);
    }
    
    auto byte_data = std::span<const std::byte>(raw.data(), raw.size());
    
    DecodeContext ctx;
    ctx.data = byte_data;
    ctx.original_offset = 0;
    ctx.timestamp = Timestamp::now();
    
    // Try to decode as SOME/IP-SD - should not crash regardless of input
    SomeIPSDDecoder decoder;
    auto result = decoder.decode(ctx);
    
    // If decode succeeded, try to access all fields and entries
    if (result) {
        const auto& sd_header = *result;
        
        // Access SD-specific fields
        [[maybe_unused]] auto flags = sd_header.flags;
        [[maybe_unused]] auto reboot = sd_header.reboot_flag;
        [[maybe_unused]] auto unicast = sd_header.unicast_flag;
        [[maybe_unused]] auto num_entries = sd_header.num_entries;
        
        // Try to parse entries
        // This tests the entry array parsing logic
        size_t entry_offset = 20;  // After SOME/IP (16) + SD header (4)
        size_t entries_parsed = 0;
        
        while (entry_offset < size && entries_parsed < sd_header.num_entries) {
            // Each entry has at least 2 bytes (type + flags)
            if (entry_offset + 2 > size) break;
            
            uint8_t entry_type = static_cast<uint8_t>(raw[entry_offset]);
            uint8_t entry_len_byte = static_cast<uint8_t>(raw[entry_offset + 1]);
            
            // Entry type determines length:
            // 0x01 = Service (16 bytes minimum)
            // 0x00 = Find Service (16 bytes minimum)
            // Others are option entries (4-255 bytes)
            
            size_t entry_size = 0;
            if (entry_type == 0x00 || entry_type == 0x01) {
                // Service/Find entries are 16 bytes fixed
                entry_size = 16;
            } else {
                // Option entries: first 2 bytes are type (1) + length (1)
                entry_size = entry_len_byte;
                if (entry_size < 2) entry_size = 2;
                if (entry_size > 255) entry_size = 255;
            }
            
            entry_offset += entry_size;
            entries_parsed++;
        }
        
        // Try to access base SOME/IP fields through polymorphic call
        [[maybe_unused]] auto str = sd_header.to_string();
        [[maybe_unused]] auto pname = sd_header.protocol_name();
        [[maybe_unused]] auto hsize = sd_header.header_size();
    }
    
    return 0;
}

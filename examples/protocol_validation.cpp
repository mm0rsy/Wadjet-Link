/// @file protocol_validation.cpp
/// @brief Example demonstrating cross-protocol layer validation
///
/// This example shows how to use the ProtocolValidator to:
/// 1. Validate protocol stack layering (Ethernet → IPv4 → TCP/UDP → Application)
/// 2. Check length consistency across layers
/// 3. Validate checksums with pseudo-headers
/// 4. Handle validation errors with strict/lenient modes

#include <wadjet/protocols/validation.hpp>
#include <iostream>
#include <vector>
#include <iomanip>

using namespace wadjet::protocols;

// Helper function to create a protocol layer
ProtocolLayer create_layer(const std::string& name, std::size_t offset,
                           std::size_t header_len, std::size_t payload_len,
                           std::uint16_t ethertype = 0, std::uint16_t checksum = 0,
                           bool has_checksum = false) {
    return ProtocolLayer{
        .name = name,
        .offset = offset,
        .header_length = header_len,
        .payload_length = payload_len,
        .ethertype = ethertype,
        .checksum = checksum,
        .has_checksum = has_checksum
    };
}

// Print validation result
void print_result(const ValidationResult& result, const std::string& scenario) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Scenario: " << scenario << "\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Result: " << (result.is_valid ? "✓ VALID" : "✗ INVALID") << "\n";
    std::cout << "Mode: " << (result.mode == ValidationMode::Strict ? "Strict" : "Lenient") << "\n";
    if (result.error_count() > 0) {
        std::cout << "Errors: " << result.error_count() << "\n";
    }
}

int main() {
    std::cout << "\n"
              << "╔════════════════════════════════════════════════════════════╗\n"
              << "║     Wadjet-Link: Protocol Validation Example (T145)       ║\n"
              << "║            Cross-Protocol Layer Validation                ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n";

    // ========================================================================
    // Example 1: Basic Ethernet → IPv4 → TCP stack
    // ========================================================================
    {
        ProtocolValidator validator(ValidationMode::Strict);
        
        std::vector<ProtocolLayer> ethernet_ipv4_tcp = {
            create_layer("Ethernet", 0, 14, 486),
            create_layer("IPv4", 14, 20, 466, 0x0800),
            create_layer("TCP", 34, 20, 446, 6),
        };
        
        auto result = validator.validateLayering(ethernet_ipv4_tcp);
        print_result(result, "Ethernet → IPv4 → TCP (Simple Stack)");
        
        std::cout << "Protocol Stack:\n";
        for (size_t i = 0; i < ethernet_ipv4_tcp.size(); i++) {
            const auto& layer = ethernet_ipv4_tcp[i];
            std::cout << "  " << (i + 1) << ". " << layer.name
                      << " (offset=" << layer.offset
                      << ", header=" << layer.header_length
                      << ", payload=" << layer.payload_length << ")\n";
        }
    }

    // ========================================================================
    // Example 2: Diagnostic stack: Ethernet → IPv4 → TCP → DoIP → UDS
    // ========================================================================
    {
        ProtocolValidator validator(ValidationMode::Strict);
        
        std::vector<ProtocolLayer> diagnostic_stack = {
            create_layer("Ethernet", 0, 14, 186),
            create_layer("IPv4", 14, 20, 172, 0x0800),
            create_layer("TCP", 34, 20, 152, 6),
            create_layer("DoIP", 54, 8, 144),
            create_layer("UDS", 62, 2, 142),
        };
        
        auto result = validator.validateLayering(diagnostic_stack);
        print_result(result, "Ethernet → IPv4 → TCP → DoIP → UDS");
        
        std::cout << "Full Diagnostic Stack:\n";
        for (size_t i = 0; i < diagnostic_stack.size(); i++) {
            const auto& layer = diagnostic_stack[i];
            std::cout << "  " << (i + 1) << ". " << layer.name
                      << " @ offset " << layer.offset << "\n";
        }
    }

    // ========================================================================
    // Example 3: SOME/IP Service Discovery over UDP
    // ========================================================================
    {
        ProtocolValidator validator(ValidationMode::Lenient);
        
        std::vector<ProtocolLayer> someip_sd_stack = {
            create_layer("Ethernet", 0, 14, 286),
            create_layer("IPv4", 14, 20, 272, 0x0800),
            create_layer("UDP", 34, 8, 264, 17),
            create_layer("SOME/IP-SD", 42, 24, 240),
        };
        
        auto result = validator.validateLayering(someip_sd_stack);
        print_result(result, "Ethernet → IPv4 → UDP → SOME/IP-SD");
        
        std::cout << "Service Discovery Stack:\n";
        for (size_t i = 0; i < someip_sd_stack.size(); i++) {
            const auto& layer = someip_sd_stack[i];
            std::cout << "  " << (i + 1) << ". " << layer.name << "\n";
        }
    }

    // ========================================================================
    // Example 4: Length validation with checksum checking
    // ========================================================================
    {
        ProtocolValidator validator(ValidationMode::Strict);
        
        std::vector<ProtocolLayer> with_checksums = {
            create_layer("Ethernet", 0, 14, 100),
            create_layer("IPv4", 14, 20, 86, 0x0800, 0xABCD, true),
            create_layer("UDP", 34, 8, 78, 17, 0xDEF0, true),
            create_layer("SOME/IP", 42, 16, 62),
        };
        
        // Validate layering
        auto layer_result = validator.validateLayering(with_checksums);
        print_result(layer_result, "Stack with Checksum Fields");
        
        // Validate lengths
        std::vector<uint8_t> packet_data(256);
        auto length_result = validator.validateLengths(with_checksums, packet_data.size());
        
        std::cout << "\nLength Validation:\n";
        std::cout << "  Packet size: " << packet_data.size() << " bytes\n";
        std::cout << "  Stack requires: " << (with_checksums.back().offset + 
                                               with_checksums.back().header_length +
                                               with_checksums.back().payload_length) 
                  << " bytes minimum\n";
        std::cout << "  Result: " << (length_result.is_valid ? "✓ PASS" : "✗ FAIL") << "\n";
    }

    // ========================================================================
    // Example 5: VLAN-tagged traffic with diagnostic payload
    // ========================================================================
    {
        ProtocolValidator validator(ValidationMode::Strict);
        
        // Note: VLAN adds 4 bytes to the Ethernet frame
        std::vector<ProtocolLayer> vlan_diagnostic = {
            create_layer("Ethernet+VLAN", 0, 18, 182),  // +4 for 802.1Q tag
            create_layer("IPv4", 18, 20, 162, 0x0800),
            create_layer("TCP", 38, 20, 142, 6),
            create_layer("DoIP", 58, 8, 134),
            create_layer("UDS", 66, 2, 132),
        };
        
        auto result = validator.validateLayering(vlan_diagnostic);
        print_result(result, "VLAN-Tagged Diagnostic Stack");
        
        std::cout << "Ethernet VLAN Processing:\n";
        std::cout << "  Frame: Destination MAC (6) + Source MAC (6) + VLAN Tag (4)\n"
                  << "  Total Ethernet header: 18 bytes (with 802.1Q tag)\n";
    }

    // ========================================================================
    // Example 6: Validation mode comparison
    // ========================================================================
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Validation Mode Comparison\n";
        std::cout << std::string(60, '=') << "\n";
        
        // Invalid stack (missing IP layer)
        std::vector<ProtocolLayer> invalid_stack = {
            create_layer("Ethernet", 0, 14, 100),
            create_layer("TCP", 14, 20, 80, 6),  // TCP without IP!
        };
        
        // Strict mode
        {
            ProtocolValidator strict(ValidationMode::Strict);
            auto result = strict.validateLayering(invalid_stack);
            std::cout << "\nStrict Mode:\n";
            std::cout << "  Result: " << (result.is_valid ? "✓ VALID" : "✗ INVALID") << "\n";
            std::cout << "  (Strict mode rejects TCP without IP layer)\n";
        }
        
        // Lenient mode
        {
            ProtocolValidator lenient(ValidationMode::Lenient);
            auto result = lenient.validateLayering(invalid_stack);
            std::cout << "\nLenient Mode:\n";
            std::cout << "  Result: " << (result.is_valid ? "✓ VALID" : "✗ INVALID") << "\n";
            std::cout << "  (Lenient mode continues despite protocol issues)\n";
        }
    }

    // ========================================================================
    // Example 7: Real-world automotive scenario
    // ========================================================================
    {
        ProtocolValidator validator(ValidationMode::Lenient);
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Real-World Automotive Scenario\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "\nScenario: ECU firmware download over Ethernet gateway\n";
        
        // Typical automotive ethernet gateway packet
        std::vector<ProtocolLayer> firmware_download = {
            create_layer("Ethernet", 0, 14, 1486),
            create_layer("IPv4", 14, 20, 1472, 0x0800),
            create_layer("TCP", 34, 20, 1452, 6),
            create_layer("DoIP", 54, 8, 1444),
            create_layer("UDS-RequestDownload", 62, 16, 1428),
        };
        
        auto result = validator.validateLayering(firmware_download);
        
        std::cout << "\nFirmware Download Packet:\n";
        for (size_t i = 0; i < firmware_download.size(); i++) {
            const auto& layer = firmware_download[i];
            std::cout << "  " << std::setw(3) << std::right << layer.offset << "-"
                      << std::setw(3) << std::left << (layer.offset + layer.header_length - 1)
                      << ": " << std::setw(20) << std::left << layer.name
                      << " (payload: " << layer.payload_length << " bytes)\n";
        }
        
        std::cout << "\nValidation Result: " << (result.is_valid ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "Status: Ready to transmit to ECU\n";
    }

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "✓ Protocol Validation Examples Complete\n";
    std::cout << std::string(60, '=') << "\n\n";

    return 0;
}

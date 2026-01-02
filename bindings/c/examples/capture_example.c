/**
 * @file capture_example.c
 * @brief Example: Capture and decode automotive Ethernet packets using C API
 *
 * 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
 *
 * This example demonstrates how to use the Wadjet C bindings to:
 * - Open a PCAP file
 * - Decode packets layer by layer
 * - Access protocol-specific information (SOME/IP, DoIP, etc.)
 *
 * Build:
 *   gcc -o capture_example capture_example.c -lwadjet_c -L../../build
 *
 * Usage:
 *   ./capture_example <pcap_file>
 *   ./capture_example sample.pcap
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wadjet_c.h"

/**
 * @brief Print version information
 */
void print_version(void) {
    int major, minor, patch;
    wadjet_version_components(&major, &minor, &patch);
    printf("Wadjet-Link v%d.%d.%d (C Bindings)\n", major, minor, patch);
    printf("=============================================\n\n");
}

/**
 * @brief Process decoded Ethernet layer
 */
void process_ethernet(wadjet_decode_result_t result) {
    wadjet_ethernet_header_t eth;
    if (wadjet_decode_result_ethernet(result, &eth) == WADJET_OK) {
        printf("  Ethernet: %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth.src_mac.bytes[0], eth.src_mac.bytes[1], eth.src_mac.bytes[2],
               eth.src_mac.bytes[3], eth.src_mac.bytes[4], eth.src_mac.bytes[5],
               eth.dst_mac.bytes[0], eth.dst_mac.bytes[1], eth.dst_mac.bytes[2],
               eth.dst_mac.bytes[3], eth.dst_mac.bytes[4], eth.dst_mac.bytes[5]);
        printf("  EtherType: 0x%04x", eth.ethertype);
        if (eth.has_vlan) {
            printf(" (VLAN ID: %u)", eth.vlan_id);
        }
        printf("\n");
    }
}

/**
 * @brief Process decoded IPv4 layer
 */
void process_ipv4(wadjet_decode_result_t result) {
    wadjet_ipv4_header_t ip;
    if (wadjet_decode_result_ipv4(result, &ip) == WADJET_OK) {
        printf("  IPv4: %u.%u.%u.%u -> %u.%u.%u.%u (proto: %u, len: %u)\n",
               ip.src_ip.bytes[0], ip.src_ip.bytes[1], ip.src_ip.bytes[2], ip.src_ip.bytes[3],
               ip.dst_ip.bytes[0], ip.dst_ip.bytes[1], ip.dst_ip.bytes[2], ip.dst_ip.bytes[3],
               ip.protocol, ip.total_length);
    }
}

/**
 * @brief Process decoded UDP layer
 */
void process_udp(wadjet_decode_result_t result) {
    wadjet_udp_header_t udp;
    if (wadjet_decode_result_udp(result, &udp) == WADJET_OK) {
        printf("  UDP: port %u -> %u (len: %u)\n",
               udp.src_port, udp.dst_port, udp.length);
    }
}

/**
 * @brief Process decoded TCP layer
 */
void process_tcp(wadjet_decode_result_t result) {
    wadjet_tcp_header_t tcp;
    if (wadjet_decode_result_tcp(result, &tcp) == WADJET_OK) {
        printf("  TCP: port %u -> %u (seq: %u, ack: %u)\n",
               tcp.src_port, tcp.dst_port, tcp.sequence_number, tcp.ack_number);
        printf("       flags: SYN=%d ACK=%d FIN=%d RST=%d PSH=%d\n",
               tcp.syn, tcp.ack, tcp.fin, tcp.rst, tcp.psh);
    }
}

/**
 * @brief Process decoded SOME/IP layer
 */
void process_someip(wadjet_decode_result_t result) {
    wadjet_someip_header_t someip;
    if (wadjet_decode_result_someip(result, &someip) == WADJET_OK) {
        printf("  SOME/IP: service=0x%04x method=0x%04x session=%u\n",
               someip.service_id, someip.method_id, someip.session_id);
        printf("           type=%u return=%u\n",
               someip.message_type, someip.return_code);
    }
}

/**
 * @brief Process decoded DoIP layer
 */
void process_doip(wadjet_decode_result_t result) {
    wadjet_doip_header_t doip;
    if (wadjet_decode_result_doip(result, &doip) == WADJET_OK) {
        printf("  DoIP: type=0x%04x len=%u\n",
               doip.payload_type, doip.payload_length);
    }
}

/**
 * @brief Process decoded gPTP layer
 */
void process_gptp(wadjet_decode_result_t result) {
    wadjet_gptp_header_t gptp;
    if (wadjet_decode_result_gptp(result, &gptp) == WADJET_OK) {
        printf("  gPTP: type=%s seq=%u\n",
               wadjet_gptp_message_type_name(gptp.message_type),
               gptp.sequence_id);
    }
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pcap_file>\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s capture.pcap\n", argv[0]);
        return 1;
    }

    // Initialize the library
    if (wadjet_init() != WADJET_OK) {
        fprintf(stderr, "Failed to initialize Wadjet\n");
        return 1;
    }

    print_version();

    const char* pcap_path = argv[1];
    printf("Opening PCAP file: %s\n\n", pcap_path);

    // Open PCAP file
    wadjet_pcap_reader_t reader;
    wadjet_error_t err = wadjet_pcap_reader_open(pcap_path, &reader);
    if (err != WADJET_OK) {
        fprintf(stderr, "Error opening PCAP: %s\n", wadjet_last_error());
        wadjet_cleanup();
        return 1;
    }

    // Process packets
    wadjet_packet_t packet;
    uint32_t packet_count = 0;

    while (wadjet_pcap_reader_next(reader, &packet) == WADJET_OK) {
        packet_count++;
        printf("Packet #%u:\n", packet_count);

        // Get packet data
        const uint8_t* data;
        size_t length;
        if (wadjet_packet_data(packet, &data, &length) == WADJET_OK) {
            printf("  Length: %zu bytes\n", length);

            // Decode the packet
            wadjet_decode_result_t result;
            if (wadjet_decode_packet(data, length, &result) == WADJET_OK) {
                // Process each layer
                process_ethernet(result);
                process_ipv4(result);
                process_udp(result);
                process_tcp(result);
                process_someip(result);
                process_doip(result);
                process_gptp(result);

                // Check if decode was complete
                if (wadjet_decode_result_success(result)) {
                    printf("  [Decode complete]\n");
                }

                wadjet_decode_result_destroy(result);
            }
        }

        wadjet_packet_destroy(packet);
        printf("\n");

        // Limit output for demo
        if (packet_count >= 10) {
            printf("... (showing first 10 packets)\n");
            break;
        }
    }

    printf("\nTotal packets shown: %u\n", packet_count);

    // Cleanup
    wadjet_pcap_reader_destroy(reader);
    wadjet_cleanup();
    return 0;
}

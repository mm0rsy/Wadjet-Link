/// @file create_corpus.cpp
/// @brief Utility to generate seed corpus files for fuzz testing
/// 
/// Build and run this to generate seed packets:
///   g++ -std=c++20 -o create_corpus create_corpus.cpp
///   ./create_corpus

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

void write_file(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    std::cout << "Created: " << filename << " (" << data.size() << " bytes)\n";
}

// Ethernet frame: dst(6) + src(6) + ethertype(2)
std::vector<uint8_t> create_ethernet_frame(uint16_t ethertype = 0x0800) {
    return {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // dst MAC (broadcast)
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // src MAC
        static_cast<uint8_t>(ethertype >> 8), static_cast<uint8_t>(ethertype & 0xff)
    };
}

// VLAN tag: TPID(2) + TCI(2)
std::vector<uint8_t> create_vlan_tag(uint16_t vlan_id = 100) {
    return {
        0x81, 0x00,  // TPID
        static_cast<uint8_t>((vlan_id >> 8) & 0x0f), static_cast<uint8_t>(vlan_id & 0xff)
    };
}

// IPv4 header: 20+ bytes
std::vector<uint8_t> create_ipv4_header(uint8_t protocol = 17, uint16_t total_len = 28) {
    std::vector<uint8_t> hdr = {
        0x45,       // version(4) + IHL(5)
        0x00,       // DSCP + ECN
        static_cast<uint8_t>(total_len >> 8), static_cast<uint8_t>(total_len & 0xff),
        0x12, 0x34, // identification
        0x40, 0x00, // flags + fragment offset (DF set)
        0x40,       // TTL
        protocol,   // protocol (17=UDP, 6=TCP)
        0x00, 0x00, // checksum (zeroed for now)
        192, 168, 1, 100,   // src IP
        192, 168, 1, 200    // dst IP
    };
    return hdr;
}

// UDP header: 8 bytes
std::vector<uint8_t> create_udp_header(uint16_t src_port = 30490, 
                                        uint16_t dst_port = 30490,
                                        uint16_t length = 16) {
    return {
        static_cast<uint8_t>(src_port >> 8), static_cast<uint8_t>(src_port & 0xff),
        static_cast<uint8_t>(dst_port >> 8), static_cast<uint8_t>(dst_port & 0xff),
        static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length & 0xff),
        0x00, 0x00  // checksum
    };
}

// TCP header: 20+ bytes
std::vector<uint8_t> create_tcp_header(uint16_t src_port = 30490, 
                                        uint16_t dst_port = 30490) {
    return {
        static_cast<uint8_t>(src_port >> 8), static_cast<uint8_t>(src_port & 0xff),
        static_cast<uint8_t>(dst_port >> 8), static_cast<uint8_t>(dst_port & 0xff),
        0x00, 0x00, 0x00, 0x01,  // sequence number
        0x00, 0x00, 0x00, 0x00,  // ack number
        0x50, 0x02,              // data offset(5) + flags(SYN)
        0xff, 0xff,              // window
        0x00, 0x00,              // checksum
        0x00, 0x00               // urgent pointer
    };
}

// SOME/IP header: 16 bytes
std::vector<uint8_t> create_someip_header(uint16_t service_id = 0x1234,
                                           uint16_t method_id = 0x0001,
                                           uint32_t length = 8) {
    return {
        static_cast<uint8_t>(service_id >> 8), static_cast<uint8_t>(service_id & 0xff),
        static_cast<uint8_t>(method_id >> 8), static_cast<uint8_t>(method_id & 0xff),
        static_cast<uint8_t>(length >> 24), static_cast<uint8_t>((length >> 16) & 0xff),
        static_cast<uint8_t>((length >> 8) & 0xff), static_cast<uint8_t>(length & 0xff),
        0x00, 0x01,  // client ID
        0x00, 0x01,  // session ID
        0x01,        // protocol version
        0x01,        // interface version
        0x00,        // message type (Request)
        0x00         // return code (OK)
    };
}

// SOME/IP-SD header (after SOME/IP header with service_id=0xFFFF, method_id=0x8100)
std::vector<uint8_t> create_someip_sd_header() {
    // SOME/IP header for SD
    std::vector<uint8_t> someip = {
        0xff, 0xff,  // service ID (SD)
        0x81, 0x00,  // method ID (SD)
        0x00, 0x00, 0x00, 0x14,  // length (20 bytes following)
        0x00, 0x01,  // client ID
        0x00, 0x01,  // session ID
        0x01,        // protocol version
        0x01,        // interface version
        0x02,        // message type (Notification)
        0x00         // return code
    };
    // SD payload
    std::vector<uint8_t> sd = {
        0xc0,        // flags (reboot + unicast)
        0x00, 0x00, 0x00,  // reserved
        0x00, 0x00, 0x00, 0x00,  // entries length
        0x00, 0x00, 0x00, 0x00   // options length
    };
    someip.insert(someip.end(), sd.begin(), sd.end());
    return someip;
}

// DoIP header: 8 bytes
std::vector<uint8_t> create_doip_header(uint16_t payload_type = 0x8001,
                                         uint32_t payload_length = 0) {
    return {
        0x02, 0xfd,  // protocol version + inverse
        static_cast<uint8_t>(payload_type >> 8), static_cast<uint8_t>(payload_type & 0xff),
        static_cast<uint8_t>(payload_length >> 24), static_cast<uint8_t>((payload_length >> 16) & 0xff),
        static_cast<uint8_t>((payload_length >> 8) & 0xff), static_cast<uint8_t>(payload_length & 0xff)
    };
}

template<typename... Vecs>
std::vector<uint8_t> concat(Vecs&&... vecs) {
    std::vector<uint8_t> result;
    (result.insert(result.end(), vecs.begin(), vecs.end()), ...);
    return result;
}

int main() {
    std::string dir = "corpus/";
    
    // Basic Ethernet frame
    write_file(dir + "eth_basic", create_ethernet_frame());
    
    // Ethernet with VLAN
    write_file(dir + "eth_vlan", concat(
        std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
        create_vlan_tag(),
        std::vector<uint8_t>{0x08, 0x00}  // IPv4 ethertype after VLAN
    ));
    
    // IPv4 only
    write_file(dir + "ipv4_basic", create_ipv4_header());
    
    // IPv4 with options
    std::vector<uint8_t> ipv4_opts = create_ipv4_header();
    ipv4_opts[0] = 0x46;  // IHL = 6 (24 bytes)
    ipv4_opts.insert(ipv4_opts.end(), {0x01, 0x01, 0x01, 0x00});  // NOP + NOP + NOP + EOL
    write_file(dir + "ipv4_options", ipv4_opts);
    
    // UDP only
    write_file(dir + "udp_basic", create_udp_header());
    
    // TCP only  
    write_file(dir + "tcp_basic", create_tcp_header());
    
    // TCP with options
    std::vector<uint8_t> tcp_opts = create_tcp_header();
    tcp_opts[12] = 0x60;  // data offset = 6 (24 bytes)
    tcp_opts.insert(tcp_opts.end(), {0x02, 0x04, 0x05, 0xb4});  // MSS option
    write_file(dir + "tcp_options", tcp_opts);
    
    // SOME/IP only
    write_file(dir + "someip_basic", create_someip_header());
    
    // SOME/IP-SD
    write_file(dir + "someip_sd", create_someip_sd_header());
    
    // DoIP only
    write_file(dir + "doip_basic", create_doip_header());
    
    // Full stack: Eth + IPv4 + UDP + SOME/IP
    write_file(dir + "full_eth_ip_udp_someip", concat(
        create_ethernet_frame(),
        create_ipv4_header(17, 52),  // UDP, total length
        create_udp_header(30490, 30490, 32),
        create_someip_header()
    ));
    
    // Full stack: Eth + IPv4 + TCP + DoIP
    write_file(dir + "full_eth_ip_tcp_doip", concat(
        create_ethernet_frame(),
        create_ipv4_header(6, 48),  // TCP, total length
        create_tcp_header(13400, 13400),
        create_doip_header()
    ));
    
    // Edge case: minimum sizes
    write_file(dir + "minimal_eth", std::vector<uint8_t>(14, 0));
    write_file(dir + "minimal_ipv4", std::vector<uint8_t>(20, 0));
    write_file(dir + "minimal_udp", std::vector<uint8_t>(8, 0));
    write_file(dir + "minimal_tcp", std::vector<uint8_t>(20, 0));
    write_file(dir + "minimal_someip", std::vector<uint8_t>(16, 0));
    write_file(dir + "minimal_doip", std::vector<uint8_t>(8, 0));
    
    // Edge case: empty
    write_file(dir + "empty", std::vector<uint8_t>{});
    
    // Edge case: single byte
    write_file(dir + "single_byte", std::vector<uint8_t>{0x45});
    
    std::cout << "\nSeed corpus created successfully!\n";
    return 0;
}

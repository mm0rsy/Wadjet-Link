/**
 * @file wadjet_c.cpp
 * @brief C API implementation for Wadjet-Link
 *
 * 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
 */

#include "wadjet_c.h"

#include <wadjet/io/capture_session.hpp>
#include <wadjet/io/device.hpp>
#include <wadjet/net/packet.hpp>
#include <wadjet/pcap/pcap_reader.hpp>
#include <wadjet/pcap/pcap_writer.hpp>
#include <wadjet/protocols/diagnostic.hpp>
#include <wadjet/protocols/dispatcher.hpp>
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/ethernet.hpp>
#include <wadjet/protocols/gptp/gptp.hpp>
#include <wadjet/protocols/ipv4.hpp>
#include <wadjet/protocols/someip.hpp>
#include <wadjet/protocols/tcp.hpp>
#include <wadjet/protocols/udp.hpp>
#include <wadjet/protocols/uds/uds.hpp>
#include <wadjet/version.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace wadjet;

// ============================================================================
// Thread-local error handling
// ============================================================================

namespace {
thread_local std::string g_last_error;

void set_last_error(const std::string& msg) {
    g_last_error = msg;
}

void set_last_error(const wadjet::Error& err) {
    g_last_error = err.message;
}

template <typename T>
wadjet_error_t handle_result(const Result<T>& result) {
    if (result.is_ok()) {
        return WADJET_OK;
    }
    set_last_error(result.error());
    // Map error codes
    const auto& err = result.error();
    if (err.code == ENOENT) return WADJET_ERR_NOT_FOUND;
    if (err.code == EACCES || err.code == EPERM) return WADJET_ERR_PERMISSION;
    if (err.code == EINVAL) return WADJET_ERR_INVALID_ARGUMENT;
    if (err.code == ETIMEDOUT) return WADJET_ERR_TIMEOUT;
    if (err.code == EIO) return WADJET_ERR_IO;
    return WADJET_ERR_UNKNOWN;
}
}  // namespace

// ============================================================================
// Opaque handle implementations
// ============================================================================

struct wadjet_capture_session {
    std::unique_ptr<io::CaptureSession> session;
    std::string interface_name;
};

struct wadjet_pcap_reader {
    std::unique_ptr<pcap::PcapReader> reader;
};

struct wadjet_pcap_writer {
    std::unique_ptr<pcap::PcapWriter> writer;
};

struct wadjet_packet {
    Packet packet;
};

struct wadjet_decode_result {
    protocols::DecodeStackResult result;
};

struct wadjet_device_list {
    std::vector<io::NetworkDevice> devices;
};

struct wadjet_uds_decoder {
    protocols::uds::UdsDecoder decoder;
};

struct wadjet_uds_session {
    protocols::uds::UdsSession session;
    explicit wadjet_uds_session(std::uint16_t addr) : session(addr) {}
};

struct wadjet_diagnostic_session_manager {
    protocols::diagnostic::DiagnosticSessionManager manager;
    wadjet_diagnostic_event_callback_t callback = nullptr;
    void* user_data = nullptr;

    explicit wadjet_diagnostic_session_manager(
        protocols::diagnostic::DiagnosticSessionManager::Options opts)
        : manager(std::move(opts)) {}
};

// ============================================================================
// Version Information
// ============================================================================

extern "C" {

const char* wadjet_version(void) {
    static std::string version_str = std::string(WADJET_VERSION_STRING);
    return version_str.c_str();
}

void wadjet_version_components(int* major, int* minor, int* patch) {
    if (major)
        *major = wadjet::VERSION_MAJOR;
    if (minor)
        *minor = wadjet::VERSION_MINOR;
    if (patch)
        *patch = wadjet::VERSION_PATCH;
}

// ============================================================================
// Error Handling
// ============================================================================

const char* wadjet_error_message(wadjet_error_t error) {
    switch (error) {
        case WADJET_OK: return "Success";
        case WADJET_ERR_INVALID_ARGUMENT: return "Invalid argument";
        case WADJET_ERR_NOT_FOUND: return "Resource not found";
        case WADJET_ERR_PERMISSION: return "Permission denied";
        case WADJET_ERR_IO: return "I/O error";
        case WADJET_ERR_TIMEOUT: return "Operation timed out";
        case WADJET_ERR_DECODE: return "Protocol decode error";
        case WADJET_ERR_INVALID_STATE: return "Invalid state";
        case WADJET_ERR_OUT_OF_MEMORY: return "Memory allocation failed";
        case WADJET_ERR_NOT_SUPPORTED: return "Operation not supported";
        default: return "Unknown error";
    }
}

const char* wadjet_last_error(void) {
    if (g_last_error.empty()) {
        return nullptr;
    }
    return g_last_error.c_str();
}

void wadjet_clear_error(void) {
    g_last_error.clear();
}

// ============================================================================
// Initialization
// ============================================================================

wadjet_error_t wadjet_init(void) {
    // Currently no global initialization needed
    return WADJET_OK;
}

void wadjet_cleanup(void) {
    // Currently no global cleanup needed
}

// ============================================================================
// Capture Session API
// ============================================================================

void wadjet_capture_options_default(wadjet_capture_options_t* options) {
    if (!options) return;
    options->snaplen = 65535;
    options->promiscuous = true;
    options->immediate_mode = true;
    options->buffer_size = 2 * 1024 * 1024;
    options->timeout_ms = 100;
}

wadjet_error_t wadjet_capture_create(
    const char* interface,
    const wadjet_capture_options_t* options,
    wadjet_capture_session_t* session) {
    
    if (!interface || !session) {
        set_last_error("Invalid argument: interface or session is NULL");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    io::CaptureSessionOptions cpp_opts;
    if (options) {
        cpp_opts.snaplen = options->snaplen;
        cpp_opts.promiscuous = options->promiscuous;
        cpp_opts.immediate_mode = options->immediate_mode;
        cpp_opts.buffer_size = options->buffer_size;
        cpp_opts.timeout_ms = options->timeout_ms;
    }

    auto result = io::CaptureSession::create(interface, cpp_opts);
    if (!result) {
        set_last_error(result.error());
        return handle_result(result);
    }

    auto* handle = new (std::nothrow) wadjet_capture_session;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    handle->session = std::make_unique<io::CaptureSession>(std::move(result.value()));
    handle->interface_name = interface;
    *session = handle;
    return WADJET_OK;
}

void wadjet_capture_destroy(wadjet_capture_session_t session) {
    delete session;
}

wadjet_error_t wadjet_capture_set_filter(
    wadjet_capture_session_t session,
    const char* filter) {
    
    if (!session || !filter) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto result = session->session->set_filter(filter);
    return handle_result(result);
}

wadjet_error_t wadjet_capture_start(wadjet_capture_session_t session) {
    if (!session) {
        set_last_error("Invalid argument: session is NULL");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto result = session->session->start();
    return handle_result(result);
}

void wadjet_capture_stop(wadjet_capture_session_t session) {
    if (session) {
        session->session->stop();
    }
}

bool wadjet_capture_is_running(wadjet_capture_session_t session) {
    if (!session) return false;
    return session->session->is_running();
}

wadjet_error_t wadjet_capture_next_packet(
    wadjet_capture_session_t session,
    int timeout_ms,
    wadjet_packet_t* packet) {
    
    if (!session || !packet) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto opt_packet = session->session->next_packet(std::chrono::milliseconds(timeout_ms));
    if (!opt_packet) {
        return WADJET_ERR_TIMEOUT;
    }

    auto* handle = new (std::nothrow) wadjet_packet;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    handle->packet = std::move(*opt_packet);
    *packet = handle;
    return WADJET_OK;
}

wadjet_error_t wadjet_capture_stats(
    wadjet_capture_session_t session,
    wadjet_capture_stats_t* stats) {
    
    if (!session || !stats) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto cpp_stats = session->session->stats();
    stats->packets_received = cpp_stats.packets_received;
    stats->packets_dropped = cpp_stats.packets_dropped;
    stats->packets_filtered = cpp_stats.packets_filtered;
    stats->bytes_received = cpp_stats.bytes_received;
    return WADJET_OK;
}

const char* wadjet_capture_interface(wadjet_capture_session_t session) {
    if (!session) return nullptr;
    return session->interface_name.c_str();
}

// ============================================================================
// Packet API
// ============================================================================

wadjet_error_t wadjet_packet_create(
    const uint8_t* data,
    size_t length,
    wadjet_packet_t* packet) {
    
    if (!data || !packet) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto* handle = new (std::nothrow) wadjet_packet;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    std::span<const std::uint8_t> span(data, length);
    handle->packet = Packet(span);
    *packet = handle;
    return WADJET_OK;
}

void wadjet_packet_destroy(wadjet_packet_t packet) {
    delete packet;
}

wadjet_error_t wadjet_packet_data(
    wadjet_packet_t packet,
    const uint8_t** data,
    size_t* length) {
    
    if (!packet || !data || !length) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto span = packet->packet.data();
    *data = reinterpret_cast<const uint8_t*>(span.data());
    *length = span.size();
    return WADJET_OK;
}

wadjet_error_t wadjet_packet_timestamp(
    wadjet_packet_t packet,
    wadjet_timestamp_t* timestamp) {
    
    if (!packet || !timestamp) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto ts = packet->packet.timestamp();
    timestamp->seconds = ts.seconds();
    timestamp->nanoseconds = ts.nanoseconds();
    return WADJET_OK;
}

wadjet_error_t wadjet_packet_view(
    wadjet_packet_t packet,
    wadjet_packet_view_t* view) {
    
    if (!packet || !view) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto span = packet->packet.data();
    view->data = reinterpret_cast<const uint8_t*>(span.data());
    view->length = span.size();
    
    auto ts = packet->packet.timestamp();
    view->timestamp.seconds = ts.seconds();
    view->timestamp.nanoseconds = ts.nanoseconds();
    return WADJET_OK;
}

// ============================================================================
// PCAP Reader API
// ============================================================================

wadjet_error_t wadjet_pcap_reader_open(
    const char* path,
    wadjet_pcap_reader_t* reader) {
    
    if (!path || !reader) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto result = pcap::PcapReader::open(path);
    if (!result) {
        set_last_error(result.error());
        return handle_result(result);
    }

    auto* handle = new (std::nothrow) wadjet_pcap_reader;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    handle->reader = std::make_unique<pcap::PcapReader>(std::move(result.value()));
    *reader = handle;
    return WADJET_OK;
}

void wadjet_pcap_reader_destroy(wadjet_pcap_reader_t reader) {
    delete reader;
}

wadjet_error_t wadjet_pcap_reader_next(
    wadjet_pcap_reader_t reader,
    wadjet_packet_t* packet) {
    
    if (!reader || !packet) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto opt_packet = reader->reader->next_packet();
    if (!opt_packet) {
        return WADJET_ERR_NOT_FOUND;  // EOF
    }

    auto* handle = new (std::nothrow) wadjet_packet;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    handle->packet = std::move(*opt_packet);
    *packet = handle;
    return WADJET_OK;
}

bool wadjet_pcap_reader_has_more(wadjet_pcap_reader_t reader) {
    if (!reader) return false;
    return reader->reader->has_more();
}

wadjet_error_t wadjet_pcap_reader_reset(wadjet_pcap_reader_t reader) {
    if (!reader) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }
    reader->reader->reset();
    return WADJET_OK;
}

// ============================================================================
// PCAP Writer API
// ============================================================================

wadjet_error_t wadjet_pcap_writer_create(
    const char* path,
    wadjet_pcap_writer_t* writer) {
    
    if (!path || !writer) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto result = pcap::PcapWriter::create(path);
    if (!result) {
        set_last_error(result.error());
        return handle_result(result);
    }

    auto* handle = new (std::nothrow) wadjet_pcap_writer;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    handle->writer = std::make_unique<pcap::PcapWriter>(std::move(result.value()));
    *writer = handle;
    return WADJET_OK;
}

void wadjet_pcap_writer_destroy(wadjet_pcap_writer_t writer) {
    delete writer;
}

wadjet_error_t wadjet_pcap_writer_write(
    wadjet_pcap_writer_t writer,
    wadjet_packet_t packet) {
    
    if (!writer || !packet) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto result = writer->writer->write_packet(packet->packet);
    return handle_result(result);
}

wadjet_error_t wadjet_pcap_writer_write_raw(
    wadjet_pcap_writer_t writer,
    const uint8_t* data,
    size_t length,
    const wadjet_timestamp_t* timestamp) {
    
    if (!writer || !data || !timestamp) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto ts = Timestamp::from_unix(timestamp->seconds, timestamp->nanoseconds);

    std::span<const std::uint8_t> span(data, length);
    Packet pkt(span, ts);
    auto result = writer->writer->write_packet(pkt);
    return handle_result(result);
}

wadjet_error_t wadjet_pcap_writer_flush(wadjet_pcap_writer_t writer) {
    if (!writer) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }
    writer->writer->flush();
    return WADJET_OK;
}

// ============================================================================
// Protocol Decoding API
// ============================================================================

wadjet_error_t wadjet_decode_packet(
    const uint8_t* data,
    size_t length,
    wadjet_decode_result_t* result) {
    
    if (!data || !result) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto* handle = new (std::nothrow) wadjet_decode_result;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(data), length);
    handle->result = protocols::decode_packet(span);
    *result = handle;
    return WADJET_OK;
}

void wadjet_decode_result_destroy(wadjet_decode_result_t result) {
    delete result;
}

bool wadjet_decode_result_success(wadjet_decode_result_t result) {
    if (!result) return false;
    // DecodeStackResult has 'complete' field and optional 'error' field
    return result->result.complete && !result->result.error.has_value();
}

bool wadjet_decode_result_has_layer(
    wadjet_decode_result_t result,
    wadjet_protocol_t protocol) {
    
    if (!result) return false;

    switch (protocol) {
        case WADJET_PROTOCOL_ETHERNET:
            return result->result.has_layer<protocols::ethernet::EthernetHeader>();
        case WADJET_PROTOCOL_IPV4:
            return result->result.has_layer<protocols::ipv4::IPv4Header>();
        case WADJET_PROTOCOL_UDP:
            return result->result.has_layer<protocols::udp::UdpHeader>();
        case WADJET_PROTOCOL_TCP:
            return result->result.has_layer<protocols::tcp::TcpHeader>();
        case WADJET_PROTOCOL_SOMEIP:
            return result->result.has_layer<protocols::someip::SomeIpHeader>();
        case WADJET_PROTOCOL_DOIP:
            return result->result.has_layer<protocols::doip::DoIPHeader>();
        case WADJET_PROTOCOL_GPTP:
            return result->result.has_layer<protocols::gptp::GptpHeader>();
        default:
            return false;
    }
}

wadjet_error_t wadjet_decode_result_ethernet(
    wadjet_decode_result_t result,
    wadjet_ethernet_header_t* header) {
    
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::ethernet::EthernetHeader>()) {
        set_last_error("Ethernet layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* eth = result->result.get_layer<protocols::ethernet::EthernetHeader>();
    std::memcpy(header->src_mac.bytes, eth->src_mac.bytes.data(), 6);
    std::memcpy(header->dst_mac.bytes, eth->dst_mac.bytes.data(), 6);
    header->ethertype = eth->ethertype;
    header->has_vlan = eth->has_vlan();
    header->vlan_id = eth->vlan_id();
    header->vlan_priority = eth->vlan ? eth->vlan->pcp() : 0;
    return WADJET_OK;
}

wadjet_error_t wadjet_decode_result_ipv4(
    wadjet_decode_result_t result,
    wadjet_ipv4_header_t* header) {
    
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::ipv4::IPv4Header>()) {
        set_last_error("IPv4 layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* ipv4 = result->result.get_layer<protocols::ipv4::IPv4Header>();
    std::memcpy(header->src_ip.bytes, ipv4->src_ip.bytes.data(), 4);
    std::memcpy(header->dst_ip.bytes, ipv4->dst_ip.bytes.data(), 4);
    header->protocol = ipv4->protocol;
    header->ttl = ipv4->ttl;
    header->total_length = ipv4->total_length;
    header->identification = ipv4->identification;
    header->dont_fragment = ipv4->flags.dont_fragment;
    header->more_fragments = ipv4->flags.more_fragments;
    header->fragment_offset = ipv4->fragment_offset;
    return WADJET_OK;
}

wadjet_error_t wadjet_decode_result_udp(
    wadjet_decode_result_t result,
    wadjet_udp_header_t* header) {
    
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::udp::UdpHeader>()) {
        set_last_error("UDP layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* udp = result->result.get_layer<protocols::udp::UdpHeader>();
    header->src_port = udp->src_port;
    header->dst_port = udp->dst_port;
    header->length = udp->length;
    header->checksum = udp->checksum;
    return WADJET_OK;
}

wadjet_error_t wadjet_decode_result_tcp(
    wadjet_decode_result_t result,
    wadjet_tcp_header_t* header) {
    
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::tcp::TcpHeader>()) {
        set_last_error("TCP layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* tcp = result->result.get_layer<protocols::tcp::TcpHeader>();
    header->src_port = tcp->src_port;
    header->dst_port = tcp->dst_port;
    header->sequence_number = tcp->seq_num;
    header->ack_number = tcp->ack_num;
    header->data_offset = tcp->data_offset;
    header->syn = tcp->flags.syn;
    header->ack = tcp->flags.ack;
    header->fin = tcp->flags.fin;
    header->rst = tcp->flags.rst;
    header->psh = tcp->flags.psh;
    header->urg = tcp->flags.urg;
    header->window_size = tcp->window;
    return WADJET_OK;
}

wadjet_error_t wadjet_decode_result_someip(
    wadjet_decode_result_t result,
    wadjet_someip_header_t* header) {
    
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::someip::SomeIpHeader>()) {
        set_last_error("SOME/IP layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* someip = result->result.get_layer<protocols::someip::SomeIpHeader>();
    header->service_id = someip->service_id;
    header->method_id = someip->method_id;
    header->length = someip->length;
    header->client_id = someip->client_id;
    header->session_id = someip->session_id;
    header->protocol_version = someip->protocol_version;
    header->interface_version = someip->interface_version;
    header->message_type = static_cast<wadjet_someip_message_type_t>(someip->message_type);
    header->return_code = static_cast<wadjet_someip_return_code_t>(someip->return_code);
    header->is_service_discovery = someip->is_service_discovery();
    return WADJET_OK;
}

wadjet_error_t wadjet_decode_result_doip(
    wadjet_decode_result_t result,
    wadjet_doip_header_t* header) {
    
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::doip::DoIPHeader>()) {
        set_last_error("DoIP layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* doip = result->result.get_layer<protocols::doip::DoIPHeader>();
    header->protocol_version = doip->protocol_version;
    header->inverse_version = doip->inverse_protocol_version;
    header->payload_type = static_cast<wadjet_doip_payload_type_t>(doip->payload_type);
    header->payload_length = doip->payload_length;
    header->version_valid = doip->is_version_valid();
    return WADJET_OK;
}

wadjet_error_t wadjet_decode_result_gptp(wadjet_decode_result_t result,
                                         wadjet_gptp_header_t* header) {
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!result->result.has_layer<protocols::gptp::GptpHeader>()) {
        set_last_error("gPTP layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* gptp = result->result.get_layer<protocols::gptp::GptpHeader>();

    // Convert enum to uint8_t
    header->transport_specific = static_cast<uint8_t>(gptp->transport_specific);
    header->message_type =
        static_cast<wadjet_gptp_message_type_t>(static_cast<std::uint8_t>(gptp->message_type));
    header->version = gptp->version_ptp;  // Use version_ptp field
    header->message_length = gptp->message_length;
    header->domain_number = gptp->domain_number;
    header->correction_field = gptp->correction_field.scaled_ns;

    // Copy clock identity
    std::memcpy(header->source_port_identity.clock_identity.bytes,
                gptp->source_port_identity.clock_identity.bytes.data(), 8);
    header->source_port_identity.port_number = gptp->source_port_identity.port_number;

    header->sequence_id = gptp->sequence_id;
    header->control = gptp->control_field;                            // Use control_field
    header->log_message_interval = gptp->log_message_interval.value;  // Extract .value
    header->two_step = gptp->is_two_step();
    header->is_event = gptp->is_event();
    return WADJET_OK;
}

size_t wadjet_gptp_clock_identity_to_string(const wadjet_gptp_clock_identity_t* clock, char* buffer,
                                            size_t buffer_size) {
    if (!clock || !buffer || buffer_size < 24) {
        return 0;
    }

    return std::snprintf(buffer, buffer_size, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                         clock->bytes[0], clock->bytes[1], clock->bytes[2], clock->bytes[3],
                         clock->bytes[4], clock->bytes[5], clock->bytes[6], clock->bytes[7]);
}

const char* wadjet_gptp_message_type_name(wadjet_gptp_message_type_t type) {
    switch (type) {
        case WADJET_GPTP_SYNC:
            return "Sync";
        case WADJET_GPTP_DELAY_REQ:
            return "Delay_Req";
        case WADJET_GPTP_PDELAY_REQ:
            return "Pdelay_Req";
        case WADJET_GPTP_PDELAY_RESP:
            return "Pdelay_Resp";
        case WADJET_GPTP_FOLLOW_UP:
            return "Follow_Up";
        case WADJET_GPTP_DELAY_RESP:
            return "Delay_Resp";
        case WADJET_GPTP_PDELAY_RESP_FOLLOW_UP:
            return "Pdelay_Resp_Follow_Up";
        case WADJET_GPTP_ANNOUNCE:
            return "Announce";
        case WADJET_GPTP_SIGNALING:
            return "Signaling";
        case WADJET_GPTP_MANAGEMENT:
            return "Management";
        default:
            return "Unknown";
    }
}

wadjet_error_t wadjet_decode_result_payload(
    wadjet_decode_result_t result,
    wadjet_protocol_t protocol,
    const uint8_t** data,
    size_t* length) {
    
    if (!result || !data || !length) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    // Check that the requested protocol layer exists
    bool has_layer = false;
    switch (protocol) {
        case WADJET_PROTOCOL_ETHERNET:
            has_layer = result->result.has_layer<protocols::ethernet::EthernetHeader>();
            break;
        case WADJET_PROTOCOL_IPV4:
            has_layer = result->result.has_layer<protocols::ipv4::IPv4Header>();
            break;
        case WADJET_PROTOCOL_UDP:
            has_layer = result->result.has_layer<protocols::udp::UdpHeader>();
            break;
        case WADJET_PROTOCOL_TCP:
            has_layer = result->result.has_layer<protocols::tcp::TcpHeader>();
            break;
        case WADJET_PROTOCOL_SOMEIP:
            has_layer = result->result.has_layer<protocols::someip::SomeIpHeader>();
            break;
        case WADJET_PROTOCOL_DOIP:
            has_layer = result->result.has_layer<protocols::doip::DoIPHeader>();
            break;
        case WADJET_PROTOCOL_GPTP:
            has_layer = result->result.has_layer<protocols::gptp::GptpHeader>();
            break;
        default:
            set_last_error("Unknown protocol");
            return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (!has_layer) {
        set_last_error("Protocol layer not present");
        return WADJET_ERR_NOT_FOUND;
    }

    // Return the final payload (data after all decoded headers)
    // Note: DecodeStackResult.payload contains the remaining data after all layers
    const auto& payload = result->result.payload;
    *data = reinterpret_cast<const uint8_t*>(payload.data());
    *length = payload.size();
    return WADJET_OK;
}

// ============================================================================
// Device Enumeration API
// ============================================================================

wadjet_error_t wadjet_device_enumerate(wadjet_device_list_t* list) {
    if (!list) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto* handle = new (std::nothrow) wadjet_device_list;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    auto result = io::enumerate_devices();
    if (!result.is_ok()) {
        delete handle;
        set_last_error(result.error());
        return handle_result(result);
    }
    handle->devices = std::move(result.value());
    *list = handle;
    return WADJET_OK;
}

void wadjet_device_list_destroy(wadjet_device_list_t list) {
    delete list;
}

size_t wadjet_device_list_count(wadjet_device_list_t list) {
    if (!list) return 0;
    return list->devices.size();
}

wadjet_error_t wadjet_device_list_get(
    wadjet_device_list_t list,
    size_t index,
    wadjet_device_info_t* info) {
    
    if (!list || !info) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    if (index >= list->devices.size()) {
        set_last_error("Index out of range");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    const auto& dev = list->devices[index];
    info->name = dev.name.c_str();
    info->description = dev.description.empty() ? nullptr : dev.description.c_str();
    info->is_up = dev.is_up;
    info->is_loopback = dev.is_loopback;
    return WADJET_OK;
}

// ============================================================================
// Utility Functions
// ============================================================================

size_t wadjet_mac_to_string(
    const wadjet_mac_address_t* mac,
    char* buffer,
    size_t buffer_size) {
    
    if (!mac || !buffer || buffer_size < 18) {
        return 0;
    }

    MacAddress addr;
    std::memcpy(addr.bytes.data(), mac->bytes, 6);
    auto str = addr.to_string();
    
    if (str.size() >= buffer_size) {
        return 0;
    }
    
    std::strcpy(buffer, str.c_str());
    return str.size();
}

wadjet_error_t wadjet_mac_from_string(
    const char* str,
    wadjet_mac_address_t* mac) {
    
    if (!str || !mac) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    try {
        auto addr = MacAddress::from_string(str);
        std::memcpy(mac->bytes, addr.bytes.data(), 6);
        return WADJET_OK;
    } catch (...) {
        set_last_error("Invalid MAC address format");
        return WADJET_ERR_INVALID_ARGUMENT;
    }
}

size_t wadjet_ipv4_to_string(
    const wadjet_ipv4_address_t* ip,
    char* buffer,
    size_t buffer_size) {
    
    if (!ip || !buffer || buffer_size < 16) {
        return 0;
    }

    IPv4Address addr;
    std::memcpy(addr.bytes.data(), ip->bytes, 4);
    auto str = addr.to_string();
    
    if (str.size() >= buffer_size) {
        return 0;
    }
    
    std::strcpy(buffer, str.c_str());
    return str.size();
}

wadjet_error_t wadjet_ipv4_from_string(
    const char* str,
    wadjet_ipv4_address_t* ip) {
    
    if (!str || !ip) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    try {
        auto addr = IPv4Address::from_string(str);
        std::memcpy(ip->bytes, addr.bytes.data(), 4);
        return WADJET_OK;
    } catch (...) {
        set_last_error("Invalid IPv4 address format");
        return WADJET_ERR_INVALID_ARGUMENT;
    }
}

void wadjet_timestamp_now(wadjet_timestamp_t* timestamp) {
    if (!timestamp) return;
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);
    timestamp->seconds = seconds.count();
    timestamp->nanoseconds = nanos.count();
}

int64_t wadjet_timestamp_diff_ms(
    const wadjet_timestamp_t* start,
    const wadjet_timestamp_t* end) {
    
    if (!start || !end) return 0;
    
    int64_t start_ms = start->seconds * 1000 + start->nanoseconds / 1000000;
    int64_t end_ms = end->seconds * 1000 + end->nanoseconds / 1000000;
    return end_ms - start_ms;
}

// ============================================================================
// UDS (Unified Diagnostic Services) API
// ============================================================================

wadjet_error_t wadjet_uds_decoder_create(wadjet_uds_decoder_t* decoder) {
    if (!decoder) {
        set_last_error("Invalid argument: decoder is NULL");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto* handle = new (std::nothrow) wadjet_uds_decoder;
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    *decoder = handle;
    return WADJET_OK;
}

void wadjet_uds_decoder_destroy(wadjet_uds_decoder_t decoder) {
    delete decoder;
}

wadjet_error_t wadjet_uds_decode(wadjet_uds_decoder_t decoder, const uint8_t* data, size_t length,
                                 wadjet_uds_header_t* header) {
    if (!decoder || !data || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    std::span<const std::uint8_t> span(data, length);
    auto result = decoder->decoder.decode(span);

    if (!result.is_ok()) {
        set_last_error("UDS decode error");
        return WADJET_ERR_DECODE;
    }

    const auto& uds_header = result->header;

    // Map to C struct
    header->service_id =
        static_cast<wadjet_uds_service_id_t>(static_cast<std::uint8_t>(uds_header.service_id));
    header->is_request = uds_header.is_request();
    header->is_positive_response = uds_header.is_positive_response();
    header->is_negative_response = uds_header.is_negative_response();
    header->sub_function = uds_header.sub_function.value_or(0);
    header->suppress_positive_response = uds_header.suppress_positive_response;

    if (uds_header.negative_response_code) {
        header->nrc = static_cast<wadjet_uds_nrc_t>(
            static_cast<std::uint8_t>(*uds_header.negative_response_code));
    } else {
        header->nrc = static_cast<wadjet_uds_nrc_t>(0);
    }

    if (uds_header.rejected_service_id) {
        header->rejected_service_id = static_cast<wadjet_uds_service_id_t>(
            static_cast<std::uint8_t>(*uds_header.rejected_service_id));
    } else {
        header->rejected_service_id = static_cast<wadjet_uds_service_id_t>(0);
    }
    header->data = reinterpret_cast<const uint8_t*>(uds_header.service_data.data());
    header->data_length = uds_header.service_data.size();

    return WADJET_OK;
}

bool wadjet_uds_is_request(const uint8_t* data, size_t length) {
    if (!data || length == 0)
        return false;
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(data), length);
    return protocols::uds::UdsDecoder::is_request(span);
}

bool wadjet_uds_is_positive_response(const uint8_t* data, size_t length) {
    if (!data || length == 0)
        return false;
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(data), length);
    return protocols::uds::UdsDecoder::is_positive_response(span);
}

bool wadjet_uds_is_negative_response(const uint8_t* data, size_t length) {
    if (!data || length == 0)
        return false;
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(data), length);
    return protocols::uds::UdsDecoder::is_negative_response(span);
}

const char* wadjet_uds_service_name(wadjet_uds_service_id_t service_id) {
    auto sid = static_cast<protocols::uds::ServiceID>(service_id);
    auto sv = protocols::uds::service_id_string(sid);
    // Note: string_view from constexpr is null-terminated
    return sv.data();
}

const char* wadjet_uds_session_type_name(wadjet_uds_session_type_t session_type) {
    auto type = static_cast<protocols::uds::SessionType>(session_type);
    auto sv = protocols::uds::session_type_string(type);
    return sv.data();
}

const char* wadjet_uds_nrc_name(wadjet_uds_nrc_t nrc) {
    auto code = static_cast<protocols::uds::NRC>(nrc);
    auto sv = protocols::uds::nrc_string(code);
    return sv.data();
}

const char* wadjet_uds_nrc_description(wadjet_uds_nrc_t nrc) {
    auto code = static_cast<protocols::uds::NRC>(nrc);
    auto sv = protocols::uds::nrc_description(code);
    return sv.data();
}

wadjet_error_t wadjet_uds_session_create(uint16_t ecu_address, wadjet_uds_session_t* session) {
    if (!session) {
        set_last_error("Invalid argument: session is NULL");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto* handle = new (std::nothrow) wadjet_uds_session(ecu_address);
    if (!handle) {
        set_last_error("Memory allocation failed");
        return WADJET_ERR_OUT_OF_MEMORY;
    }

    *session = handle;
    return WADJET_OK;
}

void wadjet_uds_session_destroy(wadjet_uds_session_t session) {
    delete session;
}

wadjet_error_t wadjet_uds_session_process(wadjet_uds_session_t session, const uint8_t* data,
                                          size_t length, bool is_request) {
    if (!session || !data) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    std::span<const std::uint8_t> span(data, length);
    bool success = session->session.process_message(span, is_request);

    if (!success) {
        set_last_error("Failed to process UDS message");
        return WADJET_ERR_DECODE;
    }

    return WADJET_OK;
}

wadjet_uds_session_type_t wadjet_uds_session_get_type(wadjet_uds_session_t session) {
    if (!session)
        return WADJET_UDS_SESSION_DEFAULT;
    return static_cast<wadjet_uds_session_type_t>(
        static_cast<std::uint8_t>(session->session.session_type()));
}

bool wadjet_uds_session_is_active(wadjet_uds_session_t session) {
    if (!session)
        return false;
    return session->session.is_active();
}

bool wadjet_uds_session_security_unlocked(wadjet_uds_session_t session, uint8_t level) {
    if (!session)
        return false;
    return session->session.is_security_unlocked(level);
}

void wadjet_uds_session_reset(wadjet_uds_session_t session) {
    if (!session)
        return;
    session->session.reset();
}

wadjet_error_t wadjet_decode_result_uds(wadjet_decode_result_t result,
                                        wadjet_uds_header_t* header) {
    if (!result || !header) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    // UDS is typically extracted from DoIP payload, not directly from decode result
    // Check if we have DoIP with diagnostic message
    if (!result->result.has_layer<protocols::doip::DoIPHeader>()) {
        set_last_error("No DoIP layer found");
        return WADJET_ERR_NOT_FOUND;
    }

    const auto* doip = result->result.get_layer<protocols::doip::DoIPHeader>();
    if (!doip) {
        set_last_error("Failed to get DoIP header");
        return WADJET_ERR_NOT_FOUND;
    }

    // Check if it's a diagnostic message type
    if (!doip->is_diagnostic_message()) {
        set_last_error("DoIP payload is not a diagnostic message");
        return WADJET_ERR_NOT_FOUND;
    }

    // Get the final payload (which should be after DoIP header for diagnostic messages)
    const auto& payload = result->result.payload;
    if (payload.empty()) {
        set_last_error("No payload after DoIP header");
        return WADJET_ERR_NOT_FOUND;
    }

    // Parse the diagnostic message payload
    auto diag_opt = protocols::doip::DoIPDecoder::parse_diagnostic_message(payload);
    if (!diag_opt) {
        set_last_error("Failed to parse DoIP diagnostic message");
        return WADJET_ERR_DECODE;
    }

    const auto& diag = *diag_opt;

    // Decode UDS from user_data
    protocols::uds::UdsDecoder decoder;
    auto uds_result = decoder.decode(diag.user_data);

    if (!uds_result.is_ok()) {
        set_last_error("Failed to decode UDS from DoIP payload");
        return WADJET_ERR_DECODE;
    }

    const auto& uds_header = uds_result->header;

    // Map to C struct
    header->service_id =
        static_cast<wadjet_uds_service_id_t>(static_cast<std::uint8_t>(uds_header.service_id));
    header->is_request = uds_header.is_request();
    header->is_positive_response = uds_header.is_positive_response();
    header->is_negative_response = uds_header.is_negative_response();
    header->sub_function = uds_header.sub_function.value_or(0);
    header->suppress_positive_response = uds_header.suppress_positive_response;

    if (uds_header.negative_response_code) {
        header->nrc = static_cast<wadjet_uds_nrc_t>(
            static_cast<std::uint8_t>(*uds_header.negative_response_code));
    } else {
        header->nrc = static_cast<wadjet_uds_nrc_t>(0);
    }

    if (uds_header.rejected_service_id) {
        header->rejected_service_id = static_cast<wadjet_uds_service_id_t>(
            static_cast<std::uint8_t>(*uds_header.rejected_service_id));
    } else {
        header->rejected_service_id = static_cast<wadjet_uds_service_id_t>(0);
    }
    header->data = reinterpret_cast<const uint8_t*>(uds_header.service_data.data());
    header->data_length = uds_header.service_data.size();

    return WADJET_OK;
}

// ============================================================================
// Diagnostic Session Manager
// ============================================================================

void wadjet_diagnostic_options_default(wadjet_diagnostic_options_t* options) {
    if (!options)
        return;

    auto defaults = protocols::diagnostic::DiagnosticSessionManager::Options::defaults();
    options->enable_correlation = defaults.enable_correlation;
    options->enable_timeout_detection = defaults.enable_timeout_detection;
    options->max_ecus = defaults.max_ecus;
    options->p2_server_max_ms =
        static_cast<uint32_t>(defaults.default_timing.p2_server_max.count());
    options->p2_star_server_max_ms =
        static_cast<uint32_t>(defaults.default_timing.p2_star_server_max.count());
    options->s3_server_ms = static_cast<uint32_t>(defaults.default_timing.s3_server.count());
}

wadjet_error_t wadjet_diagnostic_manager_create(const wadjet_diagnostic_options_t* options,
                                                wadjet_diagnostic_session_manager_t* manager) {
    if (!manager) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    try {
        protocols::diagnostic::DiagnosticSessionManager::Options opts;

        if (options) {
            opts.enable_correlation = options->enable_correlation;
            opts.enable_timeout_detection = options->enable_timeout_detection;
            opts.max_ecus = options->max_ecus;
            opts.default_timing.p2_server_max =
                std::chrono::milliseconds(options->p2_server_max_ms);
            opts.default_timing.p2_star_server_max =
                std::chrono::milliseconds(options->p2_star_server_max_ms);
            opts.default_timing.s3_server = std::chrono::milliseconds(options->s3_server_ms);
        } else {
            opts = protocols::diagnostic::DiagnosticSessionManager::Options::defaults();
        }

        *manager = new wadjet_diagnostic_session_manager(std::move(opts));
        return WADJET_OK;
    } catch (const std::exception& e) {
        set_last_error(e.what());
        return WADJET_ERR_OUT_OF_MEMORY;
    }
}

void wadjet_diagnostic_manager_destroy(wadjet_diagnostic_session_manager_t manager) {
    delete manager;
}

void wadjet_diagnostic_manager_on_event(wadjet_diagnostic_session_manager_t manager,
                                        wadjet_diagnostic_event_callback_t callback,
                                        void* user_data) {
    if (!manager)
        return;

    manager->callback = callback;
    manager->user_data = user_data;

    if (callback) {
        manager->manager.on_event(
            [manager](protocols::diagnostic::DiagnosticEvent event,
                      const protocols::diagnostic::DiagnosticSessionState& state,
                      [[maybe_unused]] const protocols::diagnostic::RequestResponsePair* pair) {
                if (manager->callback) {
                    wadjet_diagnostic_session_state_t c_state;
                    c_state.tester_address = state.tester_address;
                    c_state.gateway_address = state.gateway_address;
                    c_state.session_type = static_cast<wadjet_uds_session_type_t>(
                        static_cast<int>(state.session_type));
                    c_state.session_active = state.session_active;
                    c_state.routing_active = state.routing_active;
                    c_state.security_level = state.security_level;
                    c_state.p2_server_max_ms =
                        static_cast<uint32_t>(state.timing.p2_server_max.count());
                    c_state.p2_star_server_max_ms =
                        static_cast<uint32_t>(state.timing.p2_star_server_max.count());
                    c_state.requests_sent = state.requests_sent;
                    c_state.responses_received = state.responses_received;
                    c_state.negative_responses = state.negative_responses;
                    c_state.timeouts = state.timeouts;

                    manager->callback(
                        static_cast<wadjet_diagnostic_event_t>(static_cast<int>(event)), &c_state,
                        manager->user_data);
                }
            });
    }
}

wadjet_error_t wadjet_diagnostic_manager_process(wadjet_diagnostic_session_manager_t manager,
                                                 const uint8_t* data, size_t length) {
    if (!manager || !data) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(data), length);

    if (manager->manager.process_doip_raw(span)) {
        return WADJET_OK;
    }

    return WADJET_ERR_DECODE;
}

wadjet_error_t wadjet_diagnostic_manager_get_session(wadjet_diagnostic_session_manager_t manager,
                                                     uint16_t ecu_address,
                                                     wadjet_diagnostic_session_state_t* state) {
    if (!manager || !state) {
        set_last_error("Invalid argument");
        return WADJET_ERR_INVALID_ARGUMENT;
    }

    auto* session = manager->manager.get_session_state(ecu_address);
    if (!session) {
        set_last_error("Session not found");
        return WADJET_ERR_NOT_FOUND;
    }

    state->tester_address = session->tester_address;
    state->gateway_address = session->gateway_address;
    state->session_type =
        static_cast<wadjet_uds_session_type_t>(static_cast<int>(session->session_type));
    state->session_active = session->session_active;
    state->routing_active = session->routing_active;
    state->security_level = session->security_level;
    state->p2_server_max_ms = static_cast<uint32_t>(session->timing.p2_server_max.count());
    state->p2_star_server_max_ms =
        static_cast<uint32_t>(session->timing.p2_star_server_max.count());
    state->requests_sent = session->requests_sent;
    state->responses_received = session->responses_received;
    state->negative_responses = session->negative_responses;
    state->timeouts = session->timeouts;

    return WADJET_OK;
}

size_t wadjet_diagnostic_manager_session_count(wadjet_diagnostic_session_manager_t manager) {
    if (!manager)
        return 0;
    return manager->manager.get_tracked_ecus().size();
}

void wadjet_diagnostic_manager_statistics(wadjet_diagnostic_session_manager_t manager,
                                          uint64_t* requests_recorded, uint64_t* responses_matched,
                                          uint64_t* responses_unmatched, uint64_t* timeouts) {
    if (!manager)
        return;

    auto stats = manager->manager.correlator().statistics();

    if (requests_recorded)
        *requests_recorded = stats.requests_recorded;
    if (responses_matched)
        *responses_matched = stats.responses_matched;
    if (responses_unmatched)
        *responses_unmatched = stats.responses_unmatched;
    if (timeouts)
        *timeouts = stats.pending_timeouts;
}

size_t wadjet_diagnostic_manager_check_timeouts(wadjet_diagnostic_session_manager_t manager) {
    if (!manager)
        return 0;
    return manager->manager.correlator().check_timeouts();
}

}  // extern "C"

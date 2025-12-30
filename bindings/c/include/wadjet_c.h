/**
 * @file wadjet_c.h
 * @brief C API for Wadjet-Link - Automotive Ethernet validation framework
 *
 * 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
 *
 * This header provides a C-compatible API for Wadjet-Link, enabling FFI
 * integration with Rust, Python ctypes, and other languages.
 *
 * @note This header is designed to be bindgen-compatible for Rust FFI.
 */

#ifndef WADJET_C_H
#define WADJET_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

/**
 * @brief Get the library version string
 * @return Version string (e.g., "0.1.0")
 */
const char* wadjet_version(void);

/**
 * @brief Get the library version as components
 * @param major Output: major version number
 * @param minor Output: minor version number
 * @param patch Output: patch version number
 */
void wadjet_version_components(int* major, int* minor, int* patch);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * @brief Error codes returned by Wadjet functions
 */
typedef enum {
    WADJET_OK = 0,                    /**< Success */
    WADJET_ERR_INVALID_ARGUMENT = 1,  /**< Invalid argument */
    WADJET_ERR_NOT_FOUND = 2,         /**< Resource not found */
    WADJET_ERR_PERMISSION = 3,        /**< Permission denied */
    WADJET_ERR_IO = 4,                /**< I/O error */
    WADJET_ERR_TIMEOUT = 5,           /**< Operation timed out */
    WADJET_ERR_DECODE = 6,            /**< Protocol decode error */
    WADJET_ERR_INVALID_STATE = 7,     /**< Invalid state */
    WADJET_ERR_OUT_OF_MEMORY = 8,     /**< Memory allocation failed */
    WADJET_ERR_NOT_SUPPORTED = 9,     /**< Operation not supported */
    WADJET_ERR_UNKNOWN = 99,          /**< Unknown error */
} wadjet_error_t;

/**
 * @brief Get human-readable error message for an error code
 * @param error Error code
 * @return Error message string (static, do not free)
 */
const char* wadjet_error_message(wadjet_error_t error);

/**
 * @brief Get the last error message from thread-local storage
 * @return Error message or NULL if no error
 */
const char* wadjet_last_error(void);

/**
 * @brief Clear the last error
 */
void wadjet_clear_error(void);

/* ============================================================================
 * Opaque Handle Types
 * ============================================================================ */

/** @brief Opaque handle to a capture session */
typedef struct wadjet_capture_session* wadjet_capture_session_t;

/** @brief Opaque handle to a PCAP reader */
typedef struct wadjet_pcap_reader* wadjet_pcap_reader_t;

/** @brief Opaque handle to a PCAP writer */
typedef struct wadjet_pcap_writer* wadjet_pcap_writer_t;

/** @brief Opaque handle to a packet */
typedef struct wadjet_packet* wadjet_packet_t;

/** @brief Opaque handle to decode result */
typedef struct wadjet_decode_result* wadjet_decode_result_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief MAC address (6 bytes)
 */
typedef struct {
    uint8_t bytes[6];
} wadjet_mac_address_t;

/**
 * @brief IPv4 address (4 bytes)
 */
typedef struct {
    uint8_t bytes[4];
} wadjet_ipv4_address_t;

/**
 * @brief Timestamp with nanosecond precision
 */
typedef struct {
    int64_t seconds;      /**< Seconds since epoch */
    int64_t nanoseconds;  /**< Nanoseconds within second */
} wadjet_timestamp_t;

/**
 * @brief Packet view (non-owning reference to packet data)
 */
typedef struct {
    const uint8_t* data;  /**< Pointer to packet data */
    size_t length;        /**< Length of packet data */
    wadjet_timestamp_t timestamp;  /**< Packet timestamp */
} wadjet_packet_view_t;

/**
 * @brief Capture session options
 */
typedef struct {
    uint32_t snaplen;       /**< Max bytes to capture per packet (default: 65535) */
    bool promiscuous;       /**< Enable promiscuous mode (default: true) */
    bool immediate_mode;    /**< Minimize latency (default: true) */
    size_t buffer_size;     /**< Ring buffer size in bytes */
    int timeout_ms;         /**< Poll timeout in milliseconds */
} wadjet_capture_options_t;

/**
 * @brief Capture statistics
 */
typedef struct {
    uint64_t packets_received;   /**< Packets received */
    uint64_t packets_dropped;    /**< Packets dropped by kernel */
    uint64_t packets_filtered;   /**< Packets filtered out */
    uint64_t bytes_received;     /**< Total bytes received */
} wadjet_capture_stats_t;

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the Wadjet library
 * @return WADJET_OK on success
 * @note Call once at program start
 */
wadjet_error_t wadjet_init(void);

/**
 * @brief Cleanup the Wadjet library
 * @note Call once at program end
 */
void wadjet_cleanup(void);

/* ============================================================================
 * Capture Session API
 * ============================================================================ */

/**
 * @brief Get default capture options
 * @param options Output: options struct to populate
 */
void wadjet_capture_options_default(wadjet_capture_options_t* options);

/**
 * @brief Create a new capture session
 * @param interface Network interface name (e.g., "eth0")
 * @param options Capture options (NULL for defaults)
 * @param session Output: capture session handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_capture_create(
    const char* interface,
    const wadjet_capture_options_t* options,
    wadjet_capture_session_t* session);

/**
 * @brief Destroy a capture session
 * @param session Capture session handle
 */
void wadjet_capture_destroy(wadjet_capture_session_t session);

/**
 * @brief Set BPF filter on capture session
 * @param session Capture session handle
 * @param filter BPF filter expression (e.g., "udp port 30490")
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_capture_set_filter(
    wadjet_capture_session_t session,
    const char* filter);

/**
 * @brief Start capturing packets
 * @param session Capture session handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_capture_start(wadjet_capture_session_t session);

/**
 * @brief Stop capturing packets
 * @param session Capture session handle
 */
void wadjet_capture_stop(wadjet_capture_session_t session);

/**
 * @brief Check if capture is running
 * @param session Capture session handle
 * @return true if running
 */
bool wadjet_capture_is_running(wadjet_capture_session_t session);

/**
 * @brief Get next packet from capture session
 * @param session Capture session handle
 * @param timeout_ms Timeout in milliseconds (-1 for blocking)
 * @param packet Output: packet handle (caller must destroy)
 * @return WADJET_OK on success, WADJET_ERR_TIMEOUT on timeout
 */
wadjet_error_t wadjet_capture_next_packet(
    wadjet_capture_session_t session,
    int timeout_ms,
    wadjet_packet_t* packet);

/**
 * @brief Get capture statistics
 * @param session Capture session handle
 * @param stats Output: statistics struct
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_capture_stats(
    wadjet_capture_session_t session,
    wadjet_capture_stats_t* stats);

/**
 * @brief Get interface name
 * @param session Capture session handle
 * @return Interface name string (do not free)
 */
const char* wadjet_capture_interface(wadjet_capture_session_t session);

/* ============================================================================
 * Packet API
 * ============================================================================ */

/**
 * @brief Create a new packet with given data
 * @param data Packet data to copy
 * @param length Length of packet data
 * @param packet Output: packet handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_packet_create(
    const uint8_t* data,
    size_t length,
    wadjet_packet_t* packet);

/**
 * @brief Destroy a packet
 * @param packet Packet handle
 */
void wadjet_packet_destroy(wadjet_packet_t packet);

/**
 * @brief Get packet data
 * @param packet Packet handle
 * @param data Output: pointer to data
 * @param length Output: length of data
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_packet_data(
    wadjet_packet_t packet,
    const uint8_t** data,
    size_t* length);

/**
 * @brief Get packet timestamp
 * @param packet Packet handle
 * @param timestamp Output: timestamp struct
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_packet_timestamp(
    wadjet_packet_t packet,
    wadjet_timestamp_t* timestamp);

/**
 * @brief Get packet view (non-owning reference)
 * @param packet Packet handle
 * @param view Output: packet view struct
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_packet_view(
    wadjet_packet_t packet,
    wadjet_packet_view_t* view);

/* ============================================================================
 * PCAP Reader API
 * ============================================================================ */

/**
 * @brief Open a PCAP file for reading
 * @param path File path
 * @param reader Output: reader handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_pcap_reader_open(
    const char* path,
    wadjet_pcap_reader_t* reader);

/**
 * @brief Destroy a PCAP reader
 * @param reader Reader handle
 */
void wadjet_pcap_reader_destroy(wadjet_pcap_reader_t reader);

/**
 * @brief Read next packet from PCAP file
 * @param reader Reader handle
 * @param packet Output: packet handle (caller must destroy)
 * @return WADJET_OK on success, WADJET_ERR_NOT_FOUND at EOF
 */
wadjet_error_t wadjet_pcap_reader_next(
    wadjet_pcap_reader_t reader,
    wadjet_packet_t* packet);

/**
 * @brief Check if more packets are available
 * @param reader Reader handle
 * @return true if more packets available
 */
bool wadjet_pcap_reader_has_more(wadjet_pcap_reader_t reader);

/**
 * @brief Reset reader to beginning of file
 * @param reader Reader handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_pcap_reader_reset(wadjet_pcap_reader_t reader);

/* ============================================================================
 * PCAP Writer API
 * ============================================================================ */

/**
 * @brief Create a new PCAP file for writing
 * @param path File path
 * @param writer Output: writer handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_pcap_writer_create(
    const char* path,
    wadjet_pcap_writer_t* writer);

/**
 * @brief Destroy a PCAP writer (closes file)
 * @param writer Writer handle
 */
void wadjet_pcap_writer_destroy(wadjet_pcap_writer_t writer);

/**
 * @brief Write a packet to PCAP file
 * @param writer Writer handle
 * @param packet Packet to write
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_pcap_writer_write(
    wadjet_pcap_writer_t writer,
    wadjet_packet_t packet);

/**
 * @brief Write raw packet data to PCAP file
 * @param writer Writer handle
 * @param data Packet data
 * @param length Data length
 * @param timestamp Packet timestamp
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_pcap_writer_write_raw(
    wadjet_pcap_writer_t writer,
    const uint8_t* data,
    size_t length,
    const wadjet_timestamp_t* timestamp);

/**
 * @brief Flush writer buffer to disk
 * @param writer Writer handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_pcap_writer_flush(wadjet_pcap_writer_t writer);

/* ============================================================================
 * Protocol Decoding API
 * ============================================================================ */

/**
 * @brief Protocol layer types
 */
typedef enum {
    WADJET_PROTOCOL_ETHERNET = 1,
    WADJET_PROTOCOL_VLAN = 2,
    WADJET_PROTOCOL_IPV4 = 3,
    WADJET_PROTOCOL_UDP = 4,
    WADJET_PROTOCOL_TCP = 5,
    WADJET_PROTOCOL_SOMEIP = 6,
    WADJET_PROTOCOL_SOMEIP_SD = 7,
    WADJET_PROTOCOL_DOIP = 8,
} wadjet_protocol_t;

/**
 * @brief SOME/IP message types
 */
typedef enum {
    WADJET_SOMEIP_REQUEST = 0x00,
    WADJET_SOMEIP_REQUEST_NO_RETURN = 0x01,
    WADJET_SOMEIP_NOTIFICATION = 0x02,
    WADJET_SOMEIP_RESPONSE = 0x80,
    WADJET_SOMEIP_ERROR = 0x81,
} wadjet_someip_message_type_t;

/**
 * @brief SOME/IP return codes
 */
typedef enum {
    WADJET_SOMEIP_RC_OK = 0x00,
    WADJET_SOMEIP_RC_NOT_OK = 0x01,
    WADJET_SOMEIP_RC_UNKNOWN_SERVICE = 0x02,
    WADJET_SOMEIP_RC_UNKNOWN_METHOD = 0x03,
    WADJET_SOMEIP_RC_NOT_READY = 0x04,
    WADJET_SOMEIP_RC_NOT_REACHABLE = 0x05,
    WADJET_SOMEIP_RC_TIMEOUT = 0x06,
} wadjet_someip_return_code_t;

/**
 * @brief DoIP payload types
 */
typedef enum {
    WADJET_DOIP_GENERIC_NACK = 0x0000,
    WADJET_DOIP_VEHICLE_ID_REQUEST = 0x0001,
    WADJET_DOIP_VEHICLE_ID_RESPONSE = 0x0004,
    WADJET_DOIP_ROUTING_ACTIVATION_REQUEST = 0x0005,
    WADJET_DOIP_ROUTING_ACTIVATION_RESPONSE = 0x0006,
    WADJET_DOIP_ALIVE_CHECK_REQUEST = 0x0007,
    WADJET_DOIP_ALIVE_CHECK_RESPONSE = 0x0008,
    WADJET_DOIP_DIAGNOSTIC_MESSAGE = 0x8001,
    WADJET_DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK = 0x8002,
    WADJET_DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK = 0x8003,
} wadjet_doip_payload_type_t;

/**
 * @brief Decoded Ethernet header
 */
typedef struct {
    wadjet_mac_address_t src_mac;
    wadjet_mac_address_t dst_mac;
    uint16_t ethertype;
    bool has_vlan;
    uint16_t vlan_id;
    uint8_t vlan_priority;
} wadjet_ethernet_header_t;

/**
 * @brief Decoded IPv4 header
 */
typedef struct {
    wadjet_ipv4_address_t src_ip;
    wadjet_ipv4_address_t dst_ip;
    uint8_t protocol;
    uint8_t ttl;
    uint16_t total_length;
    uint16_t identification;
    bool dont_fragment;
    bool more_fragments;
    uint16_t fragment_offset;
} wadjet_ipv4_header_t;

/**
 * @brief Decoded UDP header
 */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} wadjet_udp_header_t;

/**
 * @brief Decoded TCP header
 */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t sequence_number;
    uint32_t ack_number;
    uint8_t data_offset;
    bool syn;
    bool ack;
    bool fin;
    bool rst;
    bool psh;
    bool urg;
    uint16_t window_size;
} wadjet_tcp_header_t;

/**
 * @brief Decoded SOME/IP header
 */
typedef struct {
    uint16_t service_id;
    uint16_t method_id;
    uint32_t length;
    uint16_t client_id;
    uint16_t session_id;
    uint8_t protocol_version;
    uint8_t interface_version;
    wadjet_someip_message_type_t message_type;
    wadjet_someip_return_code_t return_code;
    bool is_service_discovery;
} wadjet_someip_header_t;

/**
 * @brief Decoded DoIP header
 */
typedef struct {
    uint8_t protocol_version;
    uint8_t inverse_version;
    wadjet_doip_payload_type_t payload_type;
    uint32_t payload_length;
    bool version_valid;
} wadjet_doip_header_t;

/**
 * @brief Decode packet and return result handle
 * @param data Packet data
 * @param length Data length
 * @param result Output: decode result handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_decode_packet(
    const uint8_t* data,
    size_t length,
    wadjet_decode_result_t* result);

/**
 * @brief Destroy decode result
 * @param result Decode result handle
 */
void wadjet_decode_result_destroy(wadjet_decode_result_t result);

/**
 * @brief Check if decode was successful
 * @param result Decode result handle
 * @return true if at least one layer decoded
 */
bool wadjet_decode_result_success(wadjet_decode_result_t result);

/**
 * @brief Check if result has a specific protocol layer
 * @param result Decode result handle
 * @param protocol Protocol type
 * @return true if layer present
 */
bool wadjet_decode_result_has_layer(
    wadjet_decode_result_t result,
    wadjet_protocol_t protocol);

/**
 * @brief Get Ethernet header from decode result
 * @param result Decode result handle
 * @param header Output: Ethernet header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_ethernet(
    wadjet_decode_result_t result,
    wadjet_ethernet_header_t* header);

/**
 * @brief Get IPv4 header from decode result
 * @param result Decode result handle
 * @param header Output: IPv4 header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_ipv4(
    wadjet_decode_result_t result,
    wadjet_ipv4_header_t* header);

/**
 * @brief Get UDP header from decode result
 * @param result Decode result handle
 * @param header Output: UDP header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_udp(
    wadjet_decode_result_t result,
    wadjet_udp_header_t* header);

/**
 * @brief Get TCP header from decode result
 * @param result Decode result handle
 * @param header Output: TCP header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_tcp(
    wadjet_decode_result_t result,
    wadjet_tcp_header_t* header);

/**
 * @brief Get SOME/IP header from decode result
 * @param result Decode result handle
 * @param header Output: SOME/IP header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_someip(
    wadjet_decode_result_t result,
    wadjet_someip_header_t* header);

/**
 * @brief Get DoIP header from decode result
 * @param result Decode result handle
 * @param header Output: DoIP header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_doip(
    wadjet_decode_result_t result,
    wadjet_doip_header_t* header);

/**
 * @brief Get payload data after a specific protocol layer
 * @param result Decode result handle
 * @param protocol Protocol layer to get payload after
 * @param data Output: pointer to payload data
 * @param length Output: payload length
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_payload(
    wadjet_decode_result_t result,
    wadjet_protocol_t protocol,
    const uint8_t** data,
    size_t* length);

/* ============================================================================
 * Device Enumeration API
 * ============================================================================ */

/**
 * @brief Network device information
 */
typedef struct {
    const char* name;         /**< Device name (e.g., "eth0") */
    const char* description;  /**< Device description (may be NULL) */
    bool is_up;               /**< Interface is up */
    bool is_loopback;         /**< Loopback interface */
} wadjet_device_info_t;

/**
 * @brief Device list handle
 */
typedef struct wadjet_device_list* wadjet_device_list_t;

/**
 * @brief Enumerate available network devices
 * @param list Output: device list handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_device_enumerate(wadjet_device_list_t* list);

/**
 * @brief Destroy device list
 * @param list Device list handle
 */
void wadjet_device_list_destroy(wadjet_device_list_t list);

/**
 * @brief Get number of devices in list
 * @param list Device list handle
 * @return Number of devices
 */
size_t wadjet_device_list_count(wadjet_device_list_t list);

/**
 * @brief Get device info by index
 * @param list Device list handle
 * @param index Device index
 * @param info Output: device info
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_device_list_get(
    wadjet_device_list_t list,
    size_t index,
    wadjet_device_info_t* info);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert MAC address to string
 * @param mac MAC address
 * @param buffer Output buffer (at least 18 bytes)
 * @param buffer_size Buffer size
 * @return Number of characters written, or 0 on error
 */
size_t wadjet_mac_to_string(
    const wadjet_mac_address_t* mac,
    char* buffer,
    size_t buffer_size);

/**
 * @brief Parse MAC address from string
 * @param str MAC address string (e.g., "aa:bb:cc:dd:ee:ff")
 * @param mac Output: MAC address
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_mac_from_string(
    const char* str,
    wadjet_mac_address_t* mac);

/**
 * @brief Convert IPv4 address to string
 * @param ip IPv4 address
 * @param buffer Output buffer (at least 16 bytes)
 * @param buffer_size Buffer size
 * @return Number of characters written, or 0 on error
 */
size_t wadjet_ipv4_to_string(
    const wadjet_ipv4_address_t* ip,
    char* buffer,
    size_t buffer_size);

/**
 * @brief Parse IPv4 address from string
 * @param str IPv4 address string (e.g., "192.168.1.1")
 * @param ip Output: IPv4 address
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_ipv4_from_string(
    const char* str,
    wadjet_ipv4_address_t* ip);

/**
 * @brief Get current timestamp
 * @param timestamp Output: current timestamp
 */
void wadjet_timestamp_now(wadjet_timestamp_t* timestamp);

/**
 * @brief Calculate duration between timestamps in milliseconds
 * @param start Start timestamp
 * @param end End timestamp
 * @return Duration in milliseconds
 */
int64_t wadjet_timestamp_diff_ms(
    const wadjet_timestamp_t* start,
    const wadjet_timestamp_t* end);

#ifdef __cplusplus
}
#endif

#endif /* WADJET_C_H */

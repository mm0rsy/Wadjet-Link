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

/** @brief Opaque handle to a protocol validator */
typedef struct wadjet_protocol_validator* wadjet_protocol_validator_t;

/** @brief Opaque handle to a validation result */
typedef struct wadjet_validation_result* wadjet_validation_result_t;

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
    WADJET_PROTOCOL_GPTP = 9,
    WADJET_PROTOCOL_UDS = 10,
    WADJET_PROTOCOL_RTPS = 11, /**< DDS/RTPS protocol */
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
 * @brief gPTP (IEEE 802.1AS) message types
 */
typedef enum {
    WADJET_GPTP_SYNC = 0x0,
    WADJET_GPTP_DELAY_REQ = 0x1,
    WADJET_GPTP_PDELAY_REQ = 0x2,
    WADJET_GPTP_PDELAY_RESP = 0x3,
    WADJET_GPTP_FOLLOW_UP = 0x8,
    WADJET_GPTP_DELAY_RESP = 0x9,
    WADJET_GPTP_PDELAY_RESP_FOLLOW_UP = 0xA,
    WADJET_GPTP_ANNOUNCE = 0xB,
    WADJET_GPTP_SIGNALING = 0xC,
    WADJET_GPTP_MANAGEMENT = 0xD,
} wadjet_gptp_message_type_t;

/**
 * @brief RTPS/DDS vendor identifiers
 */
typedef enum {
    WADJET_RTPS_VENDOR_UNKNOWN = 0x0000,
    WADJET_RTPS_VENDOR_FASTDDS = 0x0101,     /**< eProsima FastDDS */
    WADJET_RTPS_VENDOR_RTI_CONNEXT = 0x0102, /**< RTI Connext DDS */
    WADJET_RTPS_VENDOR_OPENSPLICE = 0x0103,  /**< OpenSplice DDS */
    WADJET_RTPS_VENDOR_CYCLONEDDS = 0x0105,  /**< Eclipse CycloneDDS */
    WADJET_RTPS_VENDOR_OPENDDS = 0x0106,     /**< OpenDDS */
    WADJET_RTPS_VENDOR_COREDX = 0x0107,      /**< CoreDX DDS */
    WADJET_RTPS_VENDOR_DUST = 0x0108,        /**< DUST DDS */
    WADJET_RTPS_VENDOR_GURUM = 0x0109,       /**< GurumDDS */
    WADJET_RTPS_VENDOR_INTERCOM = 0x010A,    /**< InterCOM DDS */
    WADJET_RTPS_VENDOR_ZHENRONG = 0x010B,    /**< Zhenrong DDS */
    WADJET_RTPS_VENDOR_ROS2 = 0x010F,        /**< ROS2 default middleware */
} wadjet_rtps_vendor_t;

/**
 * @brief RTPS submessage kinds (RTPS v2.4)
 */
typedef enum {
    WADJET_RTPS_SUBMSG_PAD = 0x01,
    WADJET_RTPS_SUBMSG_ACKNACK = 0x06,
    WADJET_RTPS_SUBMSG_HEARTBEAT = 0x07,
    WADJET_RTPS_SUBMSG_GAP = 0x08,
    WADJET_RTPS_SUBMSG_INFO_TS = 0x09,
    WADJET_RTPS_SUBMSG_INFO_SRC = 0x0C,
    WADJET_RTPS_SUBMSG_INFO_REPLY_IP4 = 0x0D,
    WADJET_RTPS_SUBMSG_INFO_DST = 0x0E,
    WADJET_RTPS_SUBMSG_INFO_REPLY = 0x0F,
    WADJET_RTPS_SUBMSG_NACK_FRAG = 0x12,
    WADJET_RTPS_SUBMSG_HEARTBEAT_FRAG = 0x13,
    WADJET_RTPS_SUBMSG_DATA = 0x15,
    WADJET_RTPS_SUBMSG_DATA_FRAG = 0x16,
} wadjet_rtps_submessage_kind_t;

/**
 * @brief UDS service identifiers (ISO 14229)
 */
typedef enum {
    WADJET_UDS_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    WADJET_UDS_ECU_RESET = 0x11,
    WADJET_UDS_SECURITY_ACCESS = 0x27,
    WADJET_UDS_COMMUNICATION_CONTROL = 0x28,
    WADJET_UDS_TESTER_PRESENT = 0x3E,
    WADJET_UDS_ACCESS_TIMING_PARAMETER = 0x83,
    WADJET_UDS_SECURED_DATA_TRANSMISSION = 0x84,
    WADJET_UDS_CONTROL_DTC_SETTING = 0x85,
    WADJET_UDS_RESPONSE_ON_EVENT = 0x86,
    WADJET_UDS_LINK_CONTROL = 0x87,
    WADJET_UDS_READ_DATA_BY_IDENTIFIER = 0x22,
    WADJET_UDS_READ_MEMORY_BY_ADDRESS = 0x23,
    WADJET_UDS_READ_SCALING_DATA_BY_IDENTIFIER = 0x24,
    WADJET_UDS_READ_DATA_BY_PERIODIC_IDENTIFIER = 0x2A,
    WADJET_UDS_DYNAMICALLY_DEFINE_DATA_IDENTIFIER = 0x2C,
    WADJET_UDS_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    WADJET_UDS_WRITE_MEMORY_BY_ADDRESS = 0x3D,
    WADJET_UDS_CLEAR_DIAGNOSTIC_INFORMATION = 0x14,
    WADJET_UDS_READ_DTC_INFORMATION = 0x19,
    WADJET_UDS_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER = 0x2F,
    WADJET_UDS_ROUTINE_CONTROL = 0x31,
    WADJET_UDS_REQUEST_DOWNLOAD = 0x34,
    WADJET_UDS_REQUEST_UPLOAD = 0x35,
    WADJET_UDS_TRANSFER_DATA = 0x36,
    WADJET_UDS_REQUEST_TRANSFER_EXIT = 0x37,
    WADJET_UDS_REQUEST_FILE_TRANSFER = 0x38,
} wadjet_uds_service_id_t;

/**
 * @brief UDS session types
 */
typedef enum {
    WADJET_UDS_SESSION_DEFAULT = 0x01,
    WADJET_UDS_SESSION_PROGRAMMING = 0x02,
    WADJET_UDS_SESSION_EXTENDED = 0x03,
    WADJET_UDS_SESSION_SAFETY_SYSTEM = 0x04,
} wadjet_uds_session_type_t;

/**
 * @brief UDS reset types
 */
typedef enum {
    WADJET_UDS_RESET_HARD = 0x01,
    WADJET_UDS_RESET_KEY_OFF_ON = 0x02,
    WADJET_UDS_RESET_SOFT = 0x03,
    WADJET_UDS_RESET_ENABLE_RAPID_SHUTDOWN = 0x04,
    WADJET_UDS_RESET_DISABLE_RAPID_SHUTDOWN = 0x05,
} wadjet_uds_reset_type_t;

/**
 * @brief UDS negative response codes (NRC)
 */
typedef enum {
    WADJET_UDS_NRC_GENERAL_REJECT = 0x10,
    WADJET_UDS_NRC_SERVICE_NOT_SUPPORTED = 0x11,
    WADJET_UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED = 0x12,
    WADJET_UDS_NRC_INCORRECT_MESSAGE_LENGTH = 0x13,
    WADJET_UDS_NRC_RESPONSE_TOO_LONG = 0x14,
    WADJET_UDS_NRC_BUSY_REPEAT_REQUEST = 0x21,
    WADJET_UDS_NRC_CONDITIONS_NOT_CORRECT = 0x22,
    WADJET_UDS_NRC_REQUEST_SEQUENCE_ERROR = 0x24,
    WADJET_UDS_NRC_NO_RESPONSE_FROM_SUBNET = 0x25,
    WADJET_UDS_NRC_FAILURE_PREVENTS_EXECUTION = 0x26,
    WADJET_UDS_NRC_REQUEST_OUT_OF_RANGE = 0x31,
    WADJET_UDS_NRC_SECURITY_ACCESS_DENIED = 0x33,
    WADJET_UDS_NRC_INVALID_KEY = 0x35,
    WADJET_UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS = 0x36,
    WADJET_UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37,
    WADJET_UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,
    WADJET_UDS_NRC_TRANSFER_DATA_SUSPENDED = 0x71,
    WADJET_UDS_NRC_GENERAL_PROGRAMMING_FAILURE = 0x72,
    WADJET_UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,
    WADJET_UDS_NRC_RESPONSE_PENDING = 0x78,
    WADJET_UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_SESSION = 0x7E,
    WADJET_UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION = 0x7F,
} wadjet_uds_nrc_t;

/**
 * @brief UDS routine control types
 */
typedef enum {
    WADJET_UDS_ROUTINE_START = 0x01,
    WADJET_UDS_ROUTINE_STOP = 0x02,
    WADJET_UDS_ROUTINE_REQUEST_RESULTS = 0x03,
} wadjet_uds_routine_control_type_t;

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
 * @brief gPTP clock identity (8 bytes EUI-64)
 */
typedef struct {
    uint8_t bytes[8];
} wadjet_gptp_clock_identity_t;

/**
 * @brief gPTP port identity
 */
typedef struct {
    wadjet_gptp_clock_identity_t clock_identity;
    uint16_t port_number;
} wadjet_gptp_port_identity_t;

/**
 * @brief gPTP timestamp (80-bit)
 */
typedef struct {
    uint16_t seconds_msb;
    uint32_t seconds_lsb;
    uint32_t nanoseconds;
} wadjet_gptp_timestamp_t;

/**
 * @brief Decoded gPTP header
 */
typedef struct {
    uint8_t transport_specific;
    wadjet_gptp_message_type_t message_type;
    uint8_t version;
    uint16_t message_length;
    uint8_t domain_number;
    int64_t correction_field;
    wadjet_gptp_port_identity_t source_port_identity;
    uint16_t sequence_id;
    uint8_t control;
    int8_t log_message_interval;
    bool two_step;
    bool is_event;
} wadjet_gptp_header_t;

/**
 * @brief Decoded UDS header
 */
typedef struct {
    wadjet_uds_service_id_t service_id; /**< Service ID (SID) */
    bool is_request;                    /**< True if request, false if response */
    bool is_positive_response;          /**< True if positive response (SID + 0x40) */
    bool is_negative_response;          /**< True if negative response (0x7F) */
    uint8_t sub_function;               /**< Sub-function byte (if applicable) */
    bool suppress_positive_response;    /**< SPRMIB bit set */
    wadjet_uds_nrc_t nrc;               /**< NRC (only valid if is_negative_response) */
    uint8_t rejected_service_id;        /**< Rejected SID (only for negative response) */
    const uint8_t* data;                /**< Pointer to service data */
    size_t data_length;                 /**< Length of service data */
} wadjet_uds_header_t;

/**
 * @brief RTPS GUID prefix (12 bytes)
 */
typedef struct {
    uint8_t bytes[12];
} wadjet_rtps_guid_prefix_t;

/**
 * @brief RTPS Entity ID (4 bytes)
 */
typedef struct {
    uint8_t entity_key[3]; /**< Entity key (3 bytes) */
    uint8_t entity_kind;   /**< Entity kind */
} wadjet_rtps_entity_id_t;

/**
 * @brief RTPS submessage header
 */
typedef struct {
    wadjet_rtps_submessage_kind_t kind; /**< Submessage kind */
    uint8_t flags;                      /**< Submessage flags */
    uint16_t octets_to_next_header;     /**< Submessage length */
    bool endianness_flag;               /**< E flag (little endian if true) */
} wadjet_rtps_submessage_t;

/**
 * @brief Decoded RTPS header
 */
typedef struct {
    uint8_t protocol[4];                   /**< "RTPS" magic */
    uint8_t version_major;                 /**< Protocol version major */
    uint8_t version_minor;                 /**< Protocol version minor */
    wadjet_rtps_vendor_t vendor_id;        /**< Vendor identifier */
    wadjet_rtps_guid_prefix_t guid_prefix; /**< GUID prefix */
    wadjet_rtps_submessage_t* submessages; /**< Array of submessages */
    size_t submessage_count;               /**< Number of submessages */
    bool is_discovery;                     /**< True if discovery traffic */
    bool has_data;                         /**< Contains DATA submessage */
    bool has_heartbeat;                    /**< Contains HEARTBEAT submessage */
    bool has_acknack;                      /**< Contains ACKNACK submessage */
} wadjet_rtps_header_t;

/** @brief Opaque handle to UDS decoder */
typedef struct wadjet_uds_decoder* wadjet_uds_decoder_t;

/** @brief Opaque handle to UDS session tracker */
typedef struct wadjet_uds_session* wadjet_uds_session_t;

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
 * @brief Get gPTP header from decode result
 * @param result Decode result handle
 * @param header Output: gPTP header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_gptp(wadjet_decode_result_t result,
                                         wadjet_gptp_header_t* header);

/**
 * @brief Convert gPTP clock identity to string
 * @param clock Clock identity
 * @param buffer Output buffer (at least 24 bytes)
 * @param buffer_size Buffer size
 * @return Number of characters written, or 0 on error
 */
size_t wadjet_gptp_clock_identity_to_string(const wadjet_gptp_clock_identity_t* clock, char* buffer,
                                            size_t buffer_size);

/**
 * @brief Get gPTP message type name
 * @param type Message type
 * @return Message type name string (static, do not free)
 */
const char* wadjet_gptp_message_type_name(wadjet_gptp_message_type_t type);

/* ============================================================================
 * RTPS/DDS API
 * ============================================================================ */

/**
 * @brief Get RTPS header from decode result
 * @param result Decode result handle
 * @param header Output: RTPS header
 * @return WADJET_OK if layer present
 */
wadjet_error_t wadjet_decode_result_rtps(wadjet_decode_result_t result,
                                         wadjet_rtps_header_t* header);

/**
 * @brief Destroy RTPS header (frees submessage array)
 * @param header RTPS header to destroy
 */
void wadjet_rtps_header_destroy(wadjet_rtps_header_t* header);

/**
 * @brief Convert RTPS GUID prefix to string
 * @param guid_prefix GUID prefix
 * @param buffer Output buffer (at least 36 bytes)
 * @param buffer_size Buffer size
 * @return Number of characters written, or 0 on error
 */
size_t wadjet_rtps_guid_prefix_to_string(const wadjet_rtps_guid_prefix_t* guid_prefix, char* buffer,
                                         size_t buffer_size);

/**
 * @brief Convert RTPS entity ID to string
 * @param entity_id Entity ID
 * @param buffer Output buffer (at least 12 bytes)
 * @param buffer_size Buffer size
 * @return Number of characters written, or 0 on error
 */
size_t wadjet_rtps_entity_id_to_string(const wadjet_rtps_entity_id_t* entity_id, char* buffer,
                                       size_t buffer_size);

/**
 * @brief Get RTPS vendor name
 * @param vendor Vendor ID
 * @return Vendor name string (static, do not free)
 */
const char* wadjet_rtps_vendor_name(wadjet_rtps_vendor_t vendor);

/**
 * @brief Get RTPS submessage kind name
 * @param kind Submessage kind
 * @return Submessage kind name string (static, do not free)
 */
const char* wadjet_rtps_submessage_kind_name(wadjet_rtps_submessage_kind_t kind);

/**
 * @brief Check if RTPS traffic is discovery (ports 7400-7410)
 * @param src_port Source UDP port
 * @param dst_port Destination UDP port
 * @return true if discovery traffic
 */
bool wadjet_rtps_is_discovery_port(uint16_t src_port, uint16_t dst_port);

/**
 * @brief Check if port range indicates RTPS traffic
 * @param port UDP port
 * @return true if likely RTPS port (7400-7500 range)
 */
bool wadjet_rtps_is_likely_port(uint16_t port);

/* ============================================================================
 * UDS (Unified Diagnostic Services) API
 * ============================================================================ */

/**
 * @brief Create a UDS decoder
 * @param decoder Output: decoder handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_uds_decoder_create(wadjet_uds_decoder_t* decoder);

/**
 * @brief Destroy a UDS decoder
 * @param decoder Decoder handle
 */
void wadjet_uds_decoder_destroy(wadjet_uds_decoder_t decoder);

/**
 * @brief Decode UDS message from raw bytes
 * @param decoder Decoder handle
 * @param data UDS message data
 * @param length Data length
 * @param header Output: decoded UDS header
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_uds_decode(wadjet_uds_decoder_t decoder, const uint8_t* data, size_t length,
                                 wadjet_uds_header_t* header);

/**
 * @brief Check if data looks like a UDS request
 * @param data UDS message data
 * @param length Data length
 * @return true if data appears to be a UDS request
 */
bool wadjet_uds_is_request(const uint8_t* data, size_t length);

/**
 * @brief Check if data looks like a UDS positive response
 * @param data UDS message data
 * @param length Data length
 * @return true if data appears to be a positive response
 */
bool wadjet_uds_is_positive_response(const uint8_t* data, size_t length);

/**
 * @brief Check if data looks like a UDS negative response
 * @param data UDS message data
 * @param length Data length
 * @return true if data appears to be a negative response
 */
bool wadjet_uds_is_negative_response(const uint8_t* data, size_t length);

/**
 * @brief Get UDS service name
 * @param service_id Service ID
 * @return Service name string (static, do not free)
 */
const char* wadjet_uds_service_name(wadjet_uds_service_id_t service_id);

/**
 * @brief Get UDS session type name
 * @param session_type Session type
 * @return Session type name string (static, do not free)
 */
const char* wadjet_uds_session_type_name(wadjet_uds_session_type_t session_type);

/**
 * @brief Get UDS NRC name
 * @param nrc Negative response code
 * @return NRC name string (static, do not free)
 */
const char* wadjet_uds_nrc_name(wadjet_uds_nrc_t nrc);

/**
 * @brief Get UDS NRC description
 * @param nrc Negative response code
 * @return NRC description string (static, do not free)
 */
const char* wadjet_uds_nrc_description(wadjet_uds_nrc_t nrc);

/**
 * @brief Create a UDS session tracker
 * @param ecu_address ECU diagnostic address
 * @param session Output: session handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_uds_session_create(uint16_t ecu_address, wadjet_uds_session_t* session);

/**
 * @brief Destroy a UDS session tracker
 * @param session Session handle
 */
void wadjet_uds_session_destroy(wadjet_uds_session_t session);

/**
 * @brief Process a UDS message through the session tracker
 * @param session Session handle
 * @param data UDS message data
 * @param length Data length
 * @param is_request True if this is a request message
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_uds_session_process(wadjet_uds_session_t session, const uint8_t* data,
                                          size_t length, bool is_request);

/**
 * @brief Get current session type
 * @param session Session handle
 * @return Current session type
 */
wadjet_uds_session_type_t wadjet_uds_session_get_type(wadjet_uds_session_t session);

/**
 * @brief Check if session is active
 * @param session Session handle
 * @return true if session is active
 */
bool wadjet_uds_session_is_active(wadjet_uds_session_t session);

/**
 * @brief Check if a security level is unlocked
 * @param session Session handle
 * @param level Security level (1-33)
 * @return true if level is unlocked
 */
bool wadjet_uds_session_security_unlocked(wadjet_uds_session_t session, uint8_t level);

/**
 * @brief Reset session to default state
 * @param session Session handle
 */
void wadjet_uds_session_reset(wadjet_uds_session_t session);

/**
 * @brief Get UDS header from decode result
 * @param result Decode result handle
 * @param header Output: UDS header
 * @return WADJET_OK if UDS layer present
 */
wadjet_error_t wadjet_decode_result_uds(wadjet_decode_result_t result, wadjet_uds_header_t* header);

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
 * Diagnostic Session Manager API (UDS over DoIP)
 * ============================================================================ */

/** @brief Opaque handle to diagnostic session manager */
typedef struct wadjet_diagnostic_session_manager* wadjet_diagnostic_session_manager_t;

/**
 * @brief Diagnostic events
 */
typedef enum {
    WADJET_DIAG_EVENT_ROUTING_ACTIVATED = 0,
    WADJET_DIAG_EVENT_ROUTING_DEACTIVATED = 1,
    WADJET_DIAG_EVENT_CONNECTION_LOST = 2,
    WADJET_DIAG_EVENT_SESSION_STARTED = 3,
    WADJET_DIAG_EVENT_SESSION_CHANGED = 4,
    WADJET_DIAG_EVENT_SESSION_TIMEOUT = 5,
    WADJET_DIAG_EVENT_SESSION_ENDED = 6,
    WADJET_DIAG_EVENT_SECURITY_UNLOCKED = 7,
    WADJET_DIAG_EVENT_SECURITY_LOCKED = 8,
    WADJET_DIAG_EVENT_SECURITY_LOCKOUT = 9,
    WADJET_DIAG_EVENT_REQUEST_SENT = 10,
    WADJET_DIAG_EVENT_RESPONSE_RECEIVED = 11,
    WADJET_DIAG_EVENT_RESPONSE_PENDING = 12,
    WADJET_DIAG_EVENT_RESPONSE_TIMEOUT = 13,
    WADJET_DIAG_EVENT_NEGATIVE_RESPONSE = 14,
    WADJET_DIAG_EVENT_DTCS_READ = 15,
    WADJET_DIAG_EVENT_DTCS_CLEARED = 16,
    WADJET_DIAG_EVENT_DATA_IDENTIFIER_READ = 17,
    WADJET_DIAG_EVENT_FLASH_STARTED = 18,
    WADJET_DIAG_EVENT_FLASH_PROGRESS = 19,
    WADJET_DIAG_EVENT_FLASH_COMPLETED = 20,
    WADJET_DIAG_EVENT_FLASH_FAILED = 21,
} wadjet_diagnostic_event_t;

/**
 * @brief Diagnostic session state
 */
typedef struct {
    uint16_t tester_address;                /**< Tester logical address */
    uint16_t gateway_address;               /**< Gateway logical address */
    wadjet_uds_session_type_t session_type; /**< Current UDS session type */
    bool session_active;                    /**< Is session currently active */
    bool routing_active;                    /**< Is routing activated */
    uint8_t security_level;                 /**< Current security level (0 = locked) */
    uint32_t p2_server_max_ms;              /**< P2 Server Max in milliseconds */
    uint32_t p2_star_server_max_ms;         /**< P2* Server Max in milliseconds */
    uint64_t requests_sent;                 /**< Total requests sent to this ECU */
    uint64_t responses_received;            /**< Total responses received */
    uint64_t negative_responses;            /**< Negative responses received */
    uint64_t timeouts;                      /**< Timeout count */
} wadjet_diagnostic_session_state_t;

/**
 * @brief Diagnostic session manager options
 */
typedef struct {
    bool enable_correlation;        /**< Enable request/response correlation */
    bool enable_timeout_detection;  /**< Enable automatic timeout detection */
    size_t max_ecus;                /**< Maximum ECUs to track */
    uint32_t p2_server_max_ms;      /**< Default P2 Server Max (ms) */
    uint32_t p2_star_server_max_ms; /**< Default P2* Server Max (ms) */
    uint32_t s3_server_ms;          /**< Default S3 Server timeout (ms) */
} wadjet_diagnostic_options_t;

/**
 * @brief Callback for diagnostic events
 * @param event Event type
 * @param state Session state at time of event
 * @param user_data User context pointer
 */
typedef void (*wadjet_diagnostic_event_callback_t)(wadjet_diagnostic_event_t event,
                                                   const wadjet_diagnostic_session_state_t* state,
                                                   void* user_data);

/**
 * @brief Get default diagnostic options
 * @param options Output: default options
 */
void wadjet_diagnostic_options_default(wadjet_diagnostic_options_t* options);

/**
 * @brief Create diagnostic session manager
 * @param options Configuration options (NULL for defaults)
 * @param manager Output: manager handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_diagnostic_manager_create(const wadjet_diagnostic_options_t* options,
                                                wadjet_diagnostic_session_manager_t* manager);

/**
 * @brief Destroy diagnostic session manager
 * @param manager Manager handle
 */
void wadjet_diagnostic_manager_destroy(wadjet_diagnostic_session_manager_t manager);

/**
 * @brief Register event callback
 * @param manager Manager handle
 * @param callback Event callback function
 * @param user_data User context passed to callback
 */
void wadjet_diagnostic_manager_on_event(wadjet_diagnostic_session_manager_t manager,
                                        wadjet_diagnostic_event_callback_t callback,
                                        void* user_data);

/**
 * @brief Process raw DoIP packet
 * @param manager Manager handle
 * @param data Raw DoIP packet data
 * @param length Data length
 * @return WADJET_OK if packet was processed
 */
wadjet_error_t wadjet_diagnostic_manager_process(wadjet_diagnostic_session_manager_t manager,
                                                 const uint8_t* data, size_t length);

/**
 * @brief Get session state for an ECU
 * @param manager Manager handle
 * @param ecu_address ECU logical address
 * @param state Output: session state
 * @return WADJET_OK if session found, WADJET_ERR_NOT_FOUND otherwise
 */
wadjet_error_t wadjet_diagnostic_manager_get_session(wadjet_diagnostic_session_manager_t manager,
                                                     uint16_t ecu_address,
                                                     wadjet_diagnostic_session_state_t* state);

/**
 * @brief Get count of active sessions
 * @param manager Manager handle
 * @return Number of active ECU sessions
 */
size_t wadjet_diagnostic_manager_session_count(wadjet_diagnostic_session_manager_t manager);

/**
 * @brief Get correlation statistics
 * @param manager Manager handle
 * @param requests_recorded Output: total requests recorded
 * @param responses_matched Output: responses successfully matched
 * @param responses_unmatched Output: unmatched responses
 * @param timeouts Output: requests that timed out
 */
void wadjet_diagnostic_manager_statistics(wadjet_diagnostic_session_manager_t manager,
                                          uint64_t* requests_recorded, uint64_t* responses_matched,
                                          uint64_t* responses_unmatched, uint64_t* timeouts);

/**
 * @brief Check for timed out requests
 * @param manager Manager handle
 * @return Number of requests that timed out
 */
size_t wadjet_diagnostic_manager_check_timeouts(wadjet_diagnostic_session_manager_t manager);

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
 * Cross-Protocol Validation API
 * ============================================================================ */

/**
 * @brief Validation error handling modes
 */
typedef enum {
    WADJET_VALIDATION_MODE_STRICT = 0,  /**< Fail on first error */
    WADJET_VALIDATION_MODE_LENIENT = 1, /**< Continue on error */
} wadjet_validation_mode_t;

/**
 * @brief Protocol layer information for validation
 */
typedef struct {
    const char* name;           /**< Layer name (e.g., "IPv4", "TCP") */
    size_t offset;              /**< Offset in packet where layer starts */
    size_t header_length;       /**< Length of this layer's header */
    size_t payload_length;      /**< Length of payload carried by this layer */
    uint16_t ethertype;         /**< EtherType or protocol number */
    uint16_t checksum;          /**< Checksum value (0 if none) */
    bool has_checksum;          /**< Whether layer has checksum field */
} wadjet_protocol_layer_t;

/**
 * @brief Validation result with error details
 */
typedef struct {
    bool is_valid;              /**< True if validation passed */
    wadjet_validation_mode_t mode;  /**< Mode used for validation */
    size_t error_count;         /**< Number of errors found */
    // Use wadjet_validation_result_get_error() to access individual errors
} wadjet_validation_result_t;

/**
 * @brief Create a protocol validator
 * @param mode Validation mode (strict or lenient)
 * @param validator Output: validator handle
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_protocol_validator_create(
    wadjet_validation_mode_t mode,
    wadjet_protocol_validator_t* validator);

/**
 * @brief Destroy a protocol validator
 * @param validator Validator handle
 */
void wadjet_protocol_validator_destroy(wadjet_protocol_validator_t validator);

/**
 * @brief Validate protocol stack layering integrity
 *
 * Checks for valid protocol progression (Ethernet → IPv4 → UDP/TCP → Application)
 * and proper frame type matching.
 *
 * @param validator Validator handle
 * @param layers Array of protocol layers
 * @param layer_count Number of layers
 * @param result Output: validation result
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_validate_layering(
    wadjet_protocol_validator_t validator,
    const wadjet_protocol_layer_t* layers,
    size_t layer_count,
    wadjet_validation_result_t* result);

/**
 * @brief Validate length consistency across layers
 *
 * Checks that header + payload = total length for each layer,
 * no gaps or overlaps between layers.
 *
 * @param validator Validator handle
 * @param layers Array of protocol layers
 * @param layer_count Number of layers
 * @param total_packet_length Total packet length in bytes
 * @param result Output: validation result
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_validate_lengths(
    wadjet_protocol_validator_t validator,
    const wadjet_protocol_layer_t* layers,
    size_t layer_count,
    size_t total_packet_length,
    wadjet_validation_result_t* result);

/**
 * @brief Validate checksums across protocol layers
 *
 * Checks IPv4, UDP, TCP checksums with pseudo-header support.
 *
 * @param validator Validator handle
 * @param packet_data Complete packet data
 * @param packet_length Packet length in bytes
 * @param layers Array of protocol layers
 * @param layer_count Number of layers
 * @param result Output: validation result
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_validate_checksums(
    wadjet_protocol_validator_t validator,
    const uint8_t* packet_data,
    size_t packet_length,
    const wadjet_protocol_layer_t* layers,
    size_t layer_count,
    wadjet_validation_result_t* result);

/**
 * @brief Set validator mode
 * @param validator Validator handle
 * @param mode New validation mode
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_protocol_validator_set_mode(
    wadjet_protocol_validator_t validator,
    wadjet_validation_mode_t mode);

/**
 * @brief Get validator mode
 * @param validator Validator handle
 * @param mode Output: current validation mode
 * @return WADJET_OK on success
 */
wadjet_error_t wadjet_protocol_validator_get_mode(
    wadjet_protocol_validator_t validator,
    wadjet_validation_mode_t* mode);

/**
 * @brief Destroy a validation result
 * @param result Result handle
 */
void wadjet_validation_result_destroy(wadjet_validation_result_t result);

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

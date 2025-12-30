//! Protocol decoding functionality.

use crate::packet::Packet;
use crate::types::{MacAddress, Ipv4Address};
use std::ffi::CStr;
use std::ptr;

/// Protocol identifiers
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Protocol {
    /// Unknown protocol
    Unknown,
    /// Ethernet II
    Ethernet,
    /// IPv4
    Ipv4,
    /// IPv6
    Ipv6,
    /// UDP
    Udp,
    /// TCP
    Tcp,
    /// SOME/IP
    SomeIp,
    /// SOME/IP-SD (Service Discovery)
    SomeIpSd,
    /// DoIP (Diagnostics over IP)
    DoIp,
    /// gPTP (IEEE 802.1AS)
    Gptp,
    /// UDS (Unified Diagnostic Services)
    Uds,
    /// ARP
    Arp,
    /// ICMP
    Icmp,
    /// VLAN (802.1Q)
    Vlan,
}

impl Protocol {
    fn from_c(proto: wadjet_sys::wadjet_protocol_t) -> Self {
        use wadjet_sys::wadjet_protocol_t::*;
        match proto {
            WADJET_PROTOCOL_UNKNOWN => Protocol::Unknown,
            WADJET_PROTOCOL_ETHERNET => Protocol::Ethernet,
            WADJET_PROTOCOL_IPV4 => Protocol::Ipv4,
            WADJET_PROTOCOL_IPV6 => Protocol::Ipv6,
            WADJET_PROTOCOL_UDP => Protocol::Udp,
            WADJET_PROTOCOL_TCP => Protocol::Tcp,
            WADJET_PROTOCOL_SOMEIP => Protocol::SomeIp,
            WADJET_PROTOCOL_SOMEIP_SD => Protocol::SomeIpSd,
            WADJET_PROTOCOL_DOIP => Protocol::DoIp,
            WADJET_PROTOCOL_GPTP => Protocol::Gptp,
            WADJET_PROTOCOL_UDS => Protocol::Uds,
            WADJET_PROTOCOL_ARP => Protocol::Arp,
            WADJET_PROTOCOL_ICMP => Protocol::Icmp,
            WADJET_PROTOCOL_VLAN => Protocol::Vlan,
            _ => Protocol::Unknown,
        }
    }
}

/// Ethernet header information
#[derive(Debug, Clone)]
pub struct EthernetHeader {
    /// Destination MAC address
    pub dst_mac: MacAddress,
    /// Source MAC address
    pub src_mac: MacAddress,
    /// EtherType
    pub ether_type: u16,
}

impl EthernetHeader {
    fn from_c(c: &wadjet_sys::wadjet_ethernet_header_t) -> Self {
        Self {
            dst_mac: MacAddress::from_c(&c.dst_mac),
            src_mac: MacAddress::from_c(&c.src_mac),
            ether_type: c.ether_type,
        }
    }
}

/// IPv4 header information
#[derive(Debug, Clone)]
pub struct Ipv4Header {
    /// IP version (should be 4)
    pub version: u8,
    /// Header length in 32-bit words
    pub ihl: u8,
    /// Type of Service / DSCP
    pub tos: u8,
    /// Total length including header
    pub total_length: u16,
    /// Identification
    pub identification: u16,
    /// Flags
    pub flags: u8,
    /// Fragment offset
    pub fragment_offset: u16,
    /// Time to Live
    pub ttl: u8,
    /// Protocol number
    pub protocol: u8,
    /// Header checksum
    pub checksum: u16,
    /// Source IP address
    pub src_ip: Ipv4Address,
    /// Destination IP address
    pub dst_ip: Ipv4Address,
}

impl Ipv4Header {
    fn from_c(c: &wadjet_sys::wadjet_ipv4_header_t) -> Self {
        Self {
            version: c.version,
            ihl: c.ihl,
            tos: c.tos,
            total_length: c.total_length,
            identification: c.identification,
            flags: c.flags,
            fragment_offset: c.fragment_offset,
            ttl: c.ttl,
            protocol: c.protocol,
            checksum: c.checksum,
            src_ip: Ipv4Address::from_c(&c.src_ip),
            dst_ip: Ipv4Address::from_c(&c.dst_ip),
        }
    }
}

/// UDP header information
#[derive(Debug, Clone)]
pub struct UdpHeader {
    /// Source port
    pub src_port: u16,
    /// Destination port
    pub dst_port: u16,
    /// Length including header
    pub length: u16,
    /// Checksum
    pub checksum: u16,
}

impl UdpHeader {
    fn from_c(c: &wadjet_sys::wadjet_udp_header_t) -> Self {
        Self {
            src_port: c.src_port,
            dst_port: c.dst_port,
            length: c.length,
            checksum: c.checksum,
        }
    }
}

/// TCP header information
#[derive(Debug, Clone)]
pub struct TcpHeader {
    /// Source port
    pub src_port: u16,
    /// Destination port
    pub dst_port: u16,
    /// Sequence number
    pub seq_num: u32,
    /// Acknowledgment number
    pub ack_num: u32,
    /// Data offset
    pub data_offset: u8,
    /// TCP flags
    pub flags: u8,
    /// Window size
    pub window: u16,
    /// Checksum
    pub checksum: u16,
    /// Urgent pointer
    pub urgent_ptr: u16,
}

impl TcpHeader {
    fn from_c(c: &wadjet_sys::wadjet_tcp_header_t) -> Self {
        Self {
            src_port: c.src_port,
            dst_port: c.dst_port,
            seq_num: c.seq_num,
            ack_num: c.ack_num,
            data_offset: c.data_offset,
            flags: c.flags,
            window: c.window,
            checksum: c.checksum,
            urgent_ptr: c.urgent_ptr,
        }
    }

    /// Check if SYN flag is set
    pub fn is_syn(&self) -> bool {
        self.flags & 0x02 != 0
    }

    /// Check if ACK flag is set
    pub fn is_ack(&self) -> bool {
        self.flags & 0x10 != 0
    }

    /// Check if FIN flag is set
    pub fn is_fin(&self) -> bool {
        self.flags & 0x01 != 0
    }

    /// Check if RST flag is set
    pub fn is_rst(&self) -> bool {
        self.flags & 0x04 != 0
    }

    /// Check if PSH flag is set
    pub fn is_psh(&self) -> bool {
        self.flags & 0x08 != 0
    }
}

/// SOME/IP header information
#[derive(Debug, Clone)]
pub struct SomeIpHeader {
    /// Service ID
    pub service_id: u16,
    /// Method ID
    pub method_id: u16,
    /// Length of message payload
    pub length: u32,
    /// Client ID
    pub client_id: u16,
    /// Session ID
    pub session_id: u16,
    /// Protocol version
    pub protocol_version: u8,
    /// Interface version
    pub interface_version: u8,
    /// Message type
    pub message_type: u8,
    /// Return code
    pub return_code: u8,
}

impl SomeIpHeader {
    fn from_c(c: &wadjet_sys::wadjet_someip_header_t) -> Self {
        Self {
            service_id: c.service_id,
            method_id: c.method_id,
            length: c.length,
            client_id: c.client_id,
            session_id: c.session_id,
            protocol_version: c.protocol_version,
            interface_version: c.interface_version,
            message_type: c.message_type,
            return_code: c.return_code,
        }
    }

    /// Check if this is a request
    pub fn is_request(&self) -> bool {
        self.message_type == 0x00
    }

    /// Check if this is a response
    pub fn is_response(&self) -> bool {
        self.message_type == 0x80
    }

    /// Check if this is a notification
    pub fn is_notification(&self) -> bool {
        self.message_type == 0x02
    }

    /// Check if this is an error
    pub fn is_error(&self) -> bool {
        self.message_type == 0x81
    }
}

/// DoIP header information
#[derive(Debug, Clone)]
pub struct DoIpHeader {
    /// Protocol version
    pub protocol_version: u8,
    /// Inverse protocol version
    pub inverse_version: u8,
    /// Payload type
    pub payload_type: u16,
    /// Payload length
    pub payload_length: u32,
}

impl DoIpHeader {
    fn from_c(c: &wadjet_sys::wadjet_doip_header_t) -> Self {
        Self {
            protocol_version: c.protocol_version,
            inverse_version: c.inverse_version,
            payload_type: c.payload_type,
            payload_length: c.payload_length,
        }
    }

    /// Get the payload type name
    pub fn payload_type_name(&self) -> &'static str {
        match self.payload_type {
            0x0000 => "Generic NACK",
            0x0001 => "Vehicle Identification Request",
            0x0002 => "Vehicle Identification Request with EID",
            0x0003 => "Vehicle Identification Request with VIN",
            0x0004 => "Vehicle Announcement",
            0x0005 => "Routing Activation Request",
            0x0006 => "Routing Activation Response",
            0x0007 => "Alive Check Request",
            0x0008 => "Alive Check Response",
            0x8001 => "Diagnostic Message",
            0x8002 => "Diagnostic Message Positive Ack",
            0x8003 => "Diagnostic Message Negative Ack",
            _ => "Unknown",
        }
    }
}

/// gPTP message type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GptpMessageType {
    /// Sync message
    Sync = 0x0,
    /// Delay Request
    DelayReq = 0x1,
    /// Peer Delay Request
    PdelayReq = 0x2,
    /// Peer Delay Response
    PdelayResp = 0x3,
    /// Follow Up
    FollowUp = 0x8,
    /// Delay Response
    DelayResp = 0x9,
    /// Peer Delay Response Follow Up
    PdelayRespFollowUp = 0xA,
    /// Announce
    Announce = 0xB,
    /// Signaling
    Signaling = 0xC,
    /// Management
    Management = 0xD,
    /// Unknown
    Unknown = 0xFF,
}

impl From<u8> for GptpMessageType {
    fn from(value: u8) -> Self {
        match value {
            0x0 => GptpMessageType::Sync,
            0x1 => GptpMessageType::DelayReq,
            0x2 => GptpMessageType::PdelayReq,
            0x3 => GptpMessageType::PdelayResp,
            0x8 => GptpMessageType::FollowUp,
            0x9 => GptpMessageType::DelayResp,
            0xA => GptpMessageType::PdelayRespFollowUp,
            0xB => GptpMessageType::Announce,
            0xC => GptpMessageType::Signaling,
            0xD => GptpMessageType::Management,
            _ => GptpMessageType::Unknown,
        }
    }
}

impl GptpMessageType {
    /// Get the message type name
    pub fn name(&self) -> &'static str {
        match self {
            GptpMessageType::Sync => "Sync",
            GptpMessageType::DelayReq => "Delay_Req",
            GptpMessageType::PdelayReq => "Pdelay_Req",
            GptpMessageType::PdelayResp => "Pdelay_Resp",
            GptpMessageType::FollowUp => "Follow_Up",
            GptpMessageType::DelayResp => "Delay_Resp",
            GptpMessageType::PdelayRespFollowUp => "Pdelay_Resp_Follow_Up",
            GptpMessageType::Announce => "Announce",
            GptpMessageType::Signaling => "Signaling",
            GptpMessageType::Management => "Management",
            GptpMessageType::Unknown => "Unknown",
        }
    }
}

/// Clock identity (8-byte EUI-64)
#[derive(Debug, Clone)]
pub struct ClockIdentity {
    /// Raw bytes
    pub bytes: [u8; 8],
}

impl ClockIdentity {
    /// Convert to string representation
    pub fn to_string(&self) -> String {
        format!("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                self.bytes[0], self.bytes[1], self.bytes[2], self.bytes[3],
                self.bytes[4], self.bytes[5], self.bytes[6], self.bytes[7])
    }
}

/// Port identity (clock identity + port number)
#[derive(Debug, Clone)]
pub struct PortIdentity {
    /// Clock identity
    pub clock_identity: ClockIdentity,
    /// Port number
    pub port_number: u16,
}

/// gPTP header information (IEEE 802.1AS)
#[derive(Debug, Clone)]
pub struct GptpHeader {
    /// Transport specific (4 bits)
    pub transport_specific: u8,
    /// Message type
    pub message_type: GptpMessageType,
    /// PTP version
    pub version: u8,
    /// Message length
    pub message_length: u16,
    /// Domain number
    pub domain_number: u8,
    /// Correction field (scaled nanoseconds)
    pub correction_field: i64,
    /// Source port identity
    pub source_port_identity: PortIdentity,
    /// Sequence ID
    pub sequence_id: u16,
    /// Control field
    pub control: u8,
    /// Log message interval
    pub log_message_interval: i8,
    /// Two-step flag
    pub two_step: bool,
    /// Is event message
    pub is_event: bool,
}

impl GptpHeader {
    fn from_c(c: &wadjet_sys::wadjet_gptp_header_t) -> Self {
        Self {
            transport_specific: c.transport_specific,
            message_type: GptpMessageType::from(c.message_type as u8),
            version: c.version,
            message_length: c.message_length,
            domain_number: c.domain_number,
            correction_field: c.correction_field,
            source_port_identity: PortIdentity {
                clock_identity: ClockIdentity {
                    bytes: c.source_port_identity.clock_identity.bytes,
                },
                port_number: c.source_port_identity.port_number,
            },
            sequence_id: c.sequence_id,
            control: c.control,
            log_message_interval: c.log_message_interval,
            two_step: c.two_step,
            is_event: c.is_event,
        }
    }

    /// Get the message type name
    pub fn message_type_name(&self) -> &'static str {
        self.message_type.name()
    }
}

/// A decoded protocol layer
#[derive(Debug, Clone)]
pub struct DecodedLayer {
    protocol: Protocol,
    offset: usize,
    length: usize,
    // Protocol-specific headers
    ethernet: Option<EthernetHeader>,
    ipv4: Option<Ipv4Header>,
    udp: Option<UdpHeader>,
    tcp: Option<TcpHeader>,
    someip: Option<SomeIpHeader>,
    doip: Option<DoIpHeader>,
    gptp: Option<GptpHeader>,
}

impl DecodedLayer {
    /// Get the protocol type
    pub fn protocol(&self) -> Protocol {
        self.protocol
    }

    /// Get the offset of this layer in the packet
    pub fn offset(&self) -> usize {
        self.offset
    }

    /// Get the length of this layer
    pub fn length(&self) -> usize {
        self.length
    }

    /// Get Ethernet header if this is an Ethernet layer
    pub fn ethernet(&self) -> Option<&EthernetHeader> {
        self.ethernet.as_ref()
    }

    /// Get IPv4 header if this is an IPv4 layer
    pub fn ipv4(&self) -> Option<&Ipv4Header> {
        self.ipv4.as_ref()
    }

    /// Get UDP header if this is a UDP layer
    pub fn udp(&self) -> Option<&UdpHeader> {
        self.udp.as_ref()
    }

    /// Get TCP header if this is a TCP layer
    pub fn tcp(&self) -> Option<&TcpHeader> {
        self.tcp.as_ref()
    }

    /// Get SOME/IP header if this is a SOME/IP layer
    pub fn someip(&self) -> Option<&SomeIpHeader> {
        self.someip.as_ref()
    }

    /// Get DoIP header if this is a DoIP layer
    pub fn doip(&self) -> Option<&DoIpHeader> {
        self.doip.as_ref()
    }

    /// Get gPTP header if this is a gPTP layer
    pub fn gptp(&self) -> Option<&GptpHeader> {
        self.gptp.as_ref()
    }
}

/// Result of decoding a packet
pub struct DecodeResult {
    handle: *mut wadjet_sys::wadjet_decode_result_t,
    layers: Vec<DecodedLayer>,
}

impl DecodeResult {
    /// Decode a packet
    pub(crate) fn decode_packet(packet: &Packet) -> Option<Self> {
        let mut handle: *mut wadjet_sys::wadjet_decode_result_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_decode_packet(packet.handle(), &mut handle)
        };

        if err != wadjet_sys::wadjet_error_t::WADJET_OK || handle.is_null() {
            return None;
        }

        // Get the number of layers
        let layer_count = unsafe { wadjet_sys::wadjet_decode_layer_count(handle) };
        
        // Extract layer information
        let mut layers = Vec::with_capacity(layer_count);
        
        for i in 0..layer_count {
            let proto = unsafe { wadjet_sys::wadjet_decode_layer_protocol(handle, i) };
            let offset = unsafe { wadjet_sys::wadjet_decode_layer_offset(handle, i) };
            let length = unsafe { wadjet_sys::wadjet_decode_layer_length(handle, i) };
            
            let protocol = Protocol::from_c(proto);
            
            // Extract protocol-specific headers
            let (ethernet, ipv4, udp, tcp, someip, doip, gptp) = unsafe {
                let mut eth = None;
                let mut ip4 = None;
                let mut u = None;
                let mut t = None;
                let mut sip = None;
                let mut dip = None;
                let mut gtp = None;

                match protocol {
                    Protocol::Ethernet => {
                        let mut hdr = wadjet_sys::wadjet_ethernet_header_t::default();
                        if wadjet_sys::wadjet_decode_get_ethernet_header(handle, i, &mut hdr) {
                            eth = Some(EthernetHeader::from_c(&hdr));
                        }
                    }
                    Protocol::Ipv4 => {
                        let mut hdr = wadjet_sys::wadjet_ipv4_header_t::default();
                        if wadjet_sys::wadjet_decode_get_ipv4_header(handle, i, &mut hdr) {
                            ip4 = Some(Ipv4Header::from_c(&hdr));
                        }
                    }
                    Protocol::Udp => {
                        let mut hdr = wadjet_sys::wadjet_udp_header_t::default();
                        if wadjet_sys::wadjet_decode_get_udp_header(handle, i, &mut hdr) {
                            u = Some(UdpHeader::from_c(&hdr));
                        }
                    }
                    Protocol::Tcp => {
                        let mut hdr = wadjet_sys::wadjet_tcp_header_t::default();
                        if wadjet_sys::wadjet_decode_get_tcp_header(handle, i, &mut hdr) {
                            t = Some(TcpHeader::from_c(&hdr));
                        }
                    }
                    Protocol::SomeIp => {
                        let mut hdr = wadjet_sys::wadjet_someip_header_t::default();
                        if wadjet_sys::wadjet_decode_get_someip_header(handle, i, &mut hdr) {
                            sip = Some(SomeIpHeader::from_c(&hdr));
                        }
                    }
                    Protocol::DoIp => {
                        let mut hdr = wadjet_sys::wadjet_doip_header_t::default();
                        if wadjet_sys::wadjet_decode_get_doip_header(handle, i, &mut hdr) {
                            dip = Some(DoIpHeader::from_c(&hdr));
                        }
                    }
                    Protocol::Gptp => {
                        let mut hdr = wadjet_sys::wadjet_gptp_header_t::default();
                        if wadjet_sys::wadjet_decode_get_gptp_header(handle, i, &mut hdr) {
                            gtp = Some(GptpHeader::from_c(&hdr));
                        }
                    }
                    _ => {}
                }

                (eth, ip4, u, t, sip, dip, gtp)
            };

            layers.push(DecodedLayer {
                protocol,
                offset,
                length,
                ethernet,
                ipv4,
                udp,
                tcp,
                someip,
                doip,
                gptp,
            });
        }

        Some(Self { handle, layers })
    }

    /// Get the decoded layers
    pub fn layers(&self) -> &[DecodedLayer] {
        &self.layers
    }

    /// Get the number of layers
    pub fn layer_count(&self) -> usize {
        self.layers.len()
    }

    /// Get a specific layer by index
    pub fn layer(&self, index: usize) -> Option<&DecodedLayer> {
        self.layers.get(index)
    }

    /// Get the payload data (data after all headers)
    pub fn payload(&self) -> &[u8] {
        unsafe {
            let ptr = wadjet_sys::wadjet_decode_payload(self.handle);
            let len = wadjet_sys::wadjet_decode_payload_length(self.handle);
            if ptr.is_null() || len == 0 {
                return &[];
            }
            std::slice::from_raw_parts(ptr, len)
        }
    }

    /// Get a summary string of the decoded packet
    pub fn summary(&self) -> String {
        unsafe {
            let ptr = wadjet_sys::wadjet_decode_summary(self.handle);
            if ptr.is_null() {
                return String::new();
            }
            CStr::from_ptr(ptr).to_string_lossy().into_owned()
        }
    }

    /// Check if this packet contains a specific protocol
    pub fn has_protocol(&self, protocol: Protocol) -> bool {
        self.layers.iter().any(|l| l.protocol == protocol)
    }

    /// Find the first layer of a specific protocol
    pub fn find_layer(&self, protocol: Protocol) -> Option<&DecodedLayer> {
        self.layers.iter().find(|l| l.protocol == protocol)
    }
}

impl Drop for DecodeResult {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                wadjet_sys::wadjet_decode_result_destroy(self.handle);
            }
        }
    }
}

impl std::fmt::Debug for DecodeResult {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DecodeResult")
            .field("layer_count", &self.layer_count())
            .field("layers", &self.layers)
            .finish()
    }
}

// DecodeResult owns its handle
unsafe impl Send for DecodeResult {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tcp_flags() {
        let tcp = TcpHeader {
            src_port: 80,
            dst_port: 12345,
            seq_num: 0,
            ack_num: 0,
            data_offset: 5,
            flags: 0x12, // SYN + ACK
            window: 65535,
            checksum: 0,
            urgent_ptr: 0,
        };

        assert!(tcp.is_syn());
        assert!(tcp.is_ack());
        assert!(!tcp.is_fin());
        assert!(!tcp.is_rst());
    }

    #[test]
    fn test_someip_message_types() {
        let mut someip = SomeIpHeader {
            service_id: 0x1234,
            method_id: 0x8001,
            length: 8,
            client_id: 1,
            session_id: 1,
            protocol_version: 1,
            interface_version: 1,
            message_type: 0x00,
            return_code: 0,
        };

        assert!(someip.is_request());
        
        someip.message_type = 0x80;
        assert!(someip.is_response());
        
        someip.message_type = 0x02;
        assert!(someip.is_notification());
    }

    #[test]
    fn test_doip_payload_types() {
        let doip = DoIpHeader {
            protocol_version: 0x02,
            inverse_version: 0xFD,
            payload_type: 0x8001,
            payload_length: 100,
        };

        assert_eq!(doip.payload_type_name(), "Diagnostic Message");
    }
}

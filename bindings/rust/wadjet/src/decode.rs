//! Protocol decoding functionality.

use crate::packet::Packet;
use crate::types::{MacAddress, Ipv4Address};
use std::ptr;

/// Protocol identifiers
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Protocol {
    /// Ethernet II
    Ethernet,
    /// VLAN (802.1Q)
    Vlan,
    /// IPv4
    Ipv4,
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
    /// DDS/RTPS
    Rtps,
    /// Unknown protocol
    Unknown(u32),
}

impl Protocol {
    fn from_c(proto: wadjet_sys::wadjet_protocol_t) -> Self {
        use wadjet_sys::wadjet_protocol_t::*;
        match proto {
            WADJET_PROTOCOL_ETHERNET => Protocol::Ethernet,
            WADJET_PROTOCOL_VLAN => Protocol::Vlan,
            WADJET_PROTOCOL_IPV4 => Protocol::Ipv4,
            WADJET_PROTOCOL_UDP => Protocol::Udp,
            WADJET_PROTOCOL_TCP => Protocol::Tcp,
            WADJET_PROTOCOL_SOMEIP => Protocol::SomeIp,
            WADJET_PROTOCOL_SOMEIP_SD => Protocol::SomeIpSd,
            WADJET_PROTOCOL_DOIP => Protocol::DoIp,
            WADJET_PROTOCOL_GPTP => Protocol::Gptp,
            WADJET_PROTOCOL_UDS => Protocol::Uds,
            WADJET_PROTOCOL_RTPS => Protocol::Rtps,
            _ => Protocol::Unknown(proto as u32),
        }
    }
    
    fn to_c(&self) -> wadjet_sys::wadjet_protocol_t {
        use wadjet_sys::wadjet_protocol_t::*;
        match self {
            Protocol::Ethernet => WADJET_PROTOCOL_ETHERNET,
            Protocol::Vlan => WADJET_PROTOCOL_VLAN,
            Protocol::Ipv4 => WADJET_PROTOCOL_IPV4,
            Protocol::Udp => WADJET_PROTOCOL_UDP,
            Protocol::Tcp => WADJET_PROTOCOL_TCP,
            Protocol::SomeIp => WADJET_PROTOCOL_SOMEIP,
            Protocol::SomeIpSd => WADJET_PROTOCOL_SOMEIP_SD,
            Protocol::DoIp => WADJET_PROTOCOL_DOIP,
            Protocol::Gptp => WADJET_PROTOCOL_GPTP,
            Protocol::Uds => WADJET_PROTOCOL_UDS,
            Protocol::Rtps => WADJET_PROTOCOL_RTPS,
            Protocol::Unknown(_) => WADJET_PROTOCOL_ETHERNET, // Default fallback
        }
    }
}

/// Ethernet header information
#[derive(Debug, Clone)]
pub struct EthernetHeader {
    /// Source MAC address
    pub src_mac: MacAddress,
    /// Destination MAC address
    pub dst_mac: MacAddress,
    /// EtherType
    pub ethertype: u16,
    /// Whether VLAN tag is present
    pub has_vlan: bool,
    /// VLAN ID (if has_vlan)
    pub vlan_id: u16,
    /// VLAN priority (if has_vlan)
    pub vlan_priority: u8,
}

impl EthernetHeader {
    fn from_c(c: &wadjet_sys::wadjet_ethernet_header_t) -> Self {
        Self {
            src_mac: MacAddress::from_c(&c.src_mac),
            dst_mac: MacAddress::from_c(&c.dst_mac),
            ethertype: c.ethertype,
            has_vlan: c.has_vlan,
            vlan_id: c.vlan_id,
            vlan_priority: c.vlan_priority,
        }
    }
}

/// IPv4 header information
#[derive(Debug, Clone)]
pub struct Ipv4Header {
    /// Source IP address
    pub src_ip: Ipv4Address,
    /// Destination IP address
    pub dst_ip: Ipv4Address,
    /// Protocol number
    pub protocol: u8,
    /// Time to Live
    pub ttl: u8,
    /// Total length including header
    pub total_length: u16,
    /// Identification
    pub identification: u16,
    /// Don't fragment flag
    pub dont_fragment: bool,
    /// More fragments flag
    pub more_fragments: bool,
    /// Fragment offset
    pub fragment_offset: u16,
}

impl Ipv4Header {
    fn from_c(c: &wadjet_sys::wadjet_ipv4_header_t) -> Self {
        Self {
            src_ip: Ipv4Address::from_c(&c.src_ip),
            dst_ip: Ipv4Address::from_c(&c.dst_ip),
            protocol: c.protocol,
            ttl: c.ttl,
            total_length: c.total_length,
            identification: c.identification,
            dont_fragment: c.dont_fragment,
            more_fragments: c.more_fragments,
            fragment_offset: c.fragment_offset,
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
    pub sequence_number: u32,
    /// Acknowledgment number
    pub ack_number: u32,
    /// Data offset
    pub data_offset: u8,
    /// SYN flag
    pub syn: bool,
    /// ACK flag
    pub ack: bool,
    /// FIN flag
    pub fin: bool,
    /// RST flag
    pub rst: bool,
    /// PSH flag
    pub psh: bool,
    /// URG flag
    pub urg: bool,
    /// Window size
    pub window_size: u16,
}

impl TcpHeader {
    fn from_c(c: &wadjet_sys::wadjet_tcp_header_t) -> Self {
        Self {
            src_port: c.src_port,
            dst_port: c.dst_port,
            sequence_number: c.sequence_number,
            ack_number: c.ack_number,
            data_offset: c.data_offset,
            syn: c.syn,
            ack: c.ack,
            fin: c.fin,
            rst: c.rst,
            psh: c.psh,
            urg: c.urg,
            window_size: c.window_size,
        }
    }

    /// Check if SYN flag is set
    pub fn is_syn(&self) -> bool {
        self.syn
    }

    /// Check if ACK flag is set
    pub fn is_ack(&self) -> bool {
        self.ack
    }

    /// Check if FIN flag is set
    pub fn is_fin(&self) -> bool {
        self.fin
    }

    /// Check if RST flag is set
    pub fn is_rst(&self) -> bool {
        self.rst
    }

    /// Check if PSH flag is set
    pub fn is_psh(&self) -> bool {
        self.psh
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
    pub message_type: SomeIpMessageType,
    /// Return code
    pub return_code: SomeIpReturnCode,
    /// Whether this is a Service Discovery message
    pub is_service_discovery: bool,
}

/// SOME/IP message types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SomeIpMessageType {
    /// Request
    Request,
    /// Request (no return)
    RequestNoReturn,
    /// Notification
    Notification,
    /// Response
    Response,
    /// Error
    Error,
    /// Unknown
    Unknown(u8),
}

impl From<wadjet_sys::wadjet_someip_message_type_t> for SomeIpMessageType {
    fn from(value: wadjet_sys::wadjet_someip_message_type_t) -> Self {
        use wadjet_sys::wadjet_someip_message_type_t::*;
        match value {
            WADJET_SOMEIP_REQUEST => SomeIpMessageType::Request,
            WADJET_SOMEIP_REQUEST_NO_RETURN => SomeIpMessageType::RequestNoReturn,
            WADJET_SOMEIP_NOTIFICATION => SomeIpMessageType::Notification,
            WADJET_SOMEIP_RESPONSE => SomeIpMessageType::Response,
            WADJET_SOMEIP_ERROR => SomeIpMessageType::Error,
            _ => SomeIpMessageType::Unknown(value as u8),
        }
    }
}

/// SOME/IP return codes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SomeIpReturnCode {
    /// OK
    Ok,
    /// Not OK
    NotOk,
    /// Unknown service
    UnknownService,
    /// Unknown method
    UnknownMethod,
    /// Not ready
    NotReady,
    /// Not reachable
    NotReachable,
    /// Timeout
    Timeout,
    /// Unknown
    Unknown(u8),
}

impl From<wadjet_sys::wadjet_someip_return_code_t> for SomeIpReturnCode {
    fn from(value: wadjet_sys::wadjet_someip_return_code_t) -> Self {
        use wadjet_sys::wadjet_someip_return_code_t::*;
        match value {
            WADJET_SOMEIP_RC_OK => SomeIpReturnCode::Ok,
            WADJET_SOMEIP_RC_NOT_OK => SomeIpReturnCode::NotOk,
            WADJET_SOMEIP_RC_UNKNOWN_SERVICE => SomeIpReturnCode::UnknownService,
            WADJET_SOMEIP_RC_UNKNOWN_METHOD => SomeIpReturnCode::UnknownMethod,
            WADJET_SOMEIP_RC_NOT_READY => SomeIpReturnCode::NotReady,
            WADJET_SOMEIP_RC_NOT_REACHABLE => SomeIpReturnCode::NotReachable,
            WADJET_SOMEIP_RC_TIMEOUT => SomeIpReturnCode::Timeout,
            _ => SomeIpReturnCode::Unknown(value as u8),
        }
    }
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
            message_type: SomeIpMessageType::from(c.message_type),
            return_code: SomeIpReturnCode::from(c.return_code),
            is_service_discovery: c.is_service_discovery,
        }
    }

    /// Check if this is a request
    pub fn is_request(&self) -> bool {
        matches!(self.message_type, SomeIpMessageType::Request)
    }

    /// Check if this is a response
    pub fn is_response(&self) -> bool {
        matches!(self.message_type, SomeIpMessageType::Response)
    }

    /// Check if this is a notification
    pub fn is_notification(&self) -> bool {
        matches!(self.message_type, SomeIpMessageType::Notification)
    }

    /// Check if this is an error
    pub fn is_error(&self) -> bool {
        matches!(self.message_type, SomeIpMessageType::Error)
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
    pub payload_type: DoIpPayloadType,
    /// Payload length
    pub payload_length: u32,
    /// Whether the version is valid
    pub version_valid: bool,
}

/// DoIP payload types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DoIpPayloadType {
    /// Generic NACK
    GenericNack,
    /// Vehicle ID Request
    VehicleIdRequest,
    /// Vehicle ID Response
    VehicleIdResponse,
    /// Routing Activation Request
    RoutingActivationRequest,
    /// Routing Activation Response
    RoutingActivationResponse,
    /// Alive Check Request
    AliveCheckRequest,
    /// Alive Check Response
    AliveCheckResponse,
    /// Diagnostic Message
    DiagnosticMessage,
    /// Diagnostic Message Positive Ack
    DiagnosticMessagePositiveAck,
    /// Diagnostic Message Negative Ack
    DiagnosticMessageNegativeAck,
    /// Unknown
    Unknown(u16),
}

impl From<wadjet_sys::wadjet_doip_payload_type_t> for DoIpPayloadType {
    fn from(value: wadjet_sys::wadjet_doip_payload_type_t) -> Self {
        use wadjet_sys::wadjet_doip_payload_type_t::*;
        match value {
            WADJET_DOIP_GENERIC_NACK => DoIpPayloadType::GenericNack,
            WADJET_DOIP_VEHICLE_ID_REQUEST => DoIpPayloadType::VehicleIdRequest,
            WADJET_DOIP_VEHICLE_ID_RESPONSE => DoIpPayloadType::VehicleIdResponse,
            WADJET_DOIP_ROUTING_ACTIVATION_REQUEST => DoIpPayloadType::RoutingActivationRequest,
            WADJET_DOIP_ROUTING_ACTIVATION_RESPONSE => DoIpPayloadType::RoutingActivationResponse,
            WADJET_DOIP_ALIVE_CHECK_REQUEST => DoIpPayloadType::AliveCheckRequest,
            WADJET_DOIP_ALIVE_CHECK_RESPONSE => DoIpPayloadType::AliveCheckResponse,
            WADJET_DOIP_DIAGNOSTIC_MESSAGE => DoIpPayloadType::DiagnosticMessage,
            WADJET_DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK => DoIpPayloadType::DiagnosticMessagePositiveAck,
            WADJET_DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK => DoIpPayloadType::DiagnosticMessageNegativeAck,
            _ => DoIpPayloadType::Unknown(value as u16),
        }
    }
}

impl DoIpHeader {
    fn from_c(c: &wadjet_sys::wadjet_doip_header_t) -> Self {
        Self {
            protocol_version: c.protocol_version,
            inverse_version: c.inverse_version,
            payload_type: DoIpPayloadType::from(c.payload_type),
            payload_length: c.payload_length,
            version_valid: c.version_valid,
        }
    }

    /// Get the payload type name
    pub fn payload_type_name(&self) -> &'static str {
        match self.payload_type {
            DoIpPayloadType::GenericNack => "Generic NACK",
            DoIpPayloadType::VehicleIdRequest => "Vehicle Identification Request",
            DoIpPayloadType::VehicleIdResponse => "Vehicle Announcement",
            DoIpPayloadType::RoutingActivationRequest => "Routing Activation Request",
            DoIpPayloadType::RoutingActivationResponse => "Routing Activation Response",
            DoIpPayloadType::AliveCheckRequest => "Alive Check Request",
            DoIpPayloadType::AliveCheckResponse => "Alive Check Response",
            DoIpPayloadType::DiagnosticMessage => "Diagnostic Message",
            DoIpPayloadType::DiagnosticMessagePositiveAck => "Diagnostic Message Positive Ack",
            DoIpPayloadType::DiagnosticMessageNegativeAck => "Diagnostic Message Negative Ack",
            DoIpPayloadType::Unknown(_) => "Unknown",
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
        let msg_type = match c.message_type {
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_SYNC => GptpMessageType::Sync,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_DELAY_REQ => GptpMessageType::DelayReq,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_PDELAY_REQ => GptpMessageType::PdelayReq,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_PDELAY_RESP => GptpMessageType::PdelayResp,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_FOLLOW_UP => GptpMessageType::FollowUp,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_DELAY_RESP => GptpMessageType::DelayResp,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_PDELAY_RESP_FOLLOW_UP => GptpMessageType::PdelayRespFollowUp,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_ANNOUNCE => GptpMessageType::Announce,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_SIGNALING => GptpMessageType::Signaling,
            wadjet_sys::wadjet_gptp_message_type_t::WADJET_GPTP_MANAGEMENT => GptpMessageType::Management,
            _ => GptpMessageType::Unknown,
        };
        
        Self {
            transport_specific: c.transport_specific,
            message_type: msg_type,
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

// =============================================================================
// DDS/RTPS Types
// =============================================================================

/// DDS/RTPS vendor identifiers
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u16)]
pub enum RtpsVendor {
    /// Unknown vendor
    Unknown = 0x0000,
    /// eProsima Fast DDS
    FastDds = 0x0101,
    /// RTI Connext DDS
    RtiConnext = 0x0102,
    /// PrismTech OpenSplice
    OpenSplice = 0x0103,
    /// Eclipse CycloneDDS
    CycloneDds = 0x0105,
    /// OCI OpenDDS
    OpenDds = 0x0106,
}

impl From<u16> for RtpsVendor {
    fn from(value: u16) -> Self {
        match value {
            0x0101 => RtpsVendor::FastDds,
            0x0102 => RtpsVendor::RtiConnext,
            0x0103 => RtpsVendor::OpenSplice,
            0x0105 => RtpsVendor::CycloneDds,
            0x0106 => RtpsVendor::OpenDds,
            _ => RtpsVendor::Unknown,
        }
    }
}

impl RtpsVendor {
    /// Get vendor name
    pub fn name(&self) -> &'static str {
        match self {
            RtpsVendor::Unknown => "Unknown",
            RtpsVendor::FastDds => "eProsima Fast DDS",
            RtpsVendor::RtiConnext => "RTI Connext DDS",
            RtpsVendor::OpenSplice => "PrismTech OpenSplice",
            RtpsVendor::CycloneDds => "Eclipse CycloneDDS",
            RtpsVendor::OpenDds => "OCI OpenDDS",
        }
    }
}

/// RTPS submessage kinds
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum RtpsSubmessageKind {
    /// Pad submessage
    Pad = 0x01,
    /// AckNack submessage
    AckNack = 0x06,
    /// Heartbeat submessage  
    Heartbeat = 0x07,
    /// Gap submessage
    Gap = 0x08,
    /// Info Timestamp submessage
    InfoTs = 0x09,
    /// Info Source submessage
    InfoSrc = 0x0C,
    /// Info Reply IPv4 submessage
    InfoReplyIp4 = 0x0D,
    /// Info Destination submessage
    InfoDst = 0x0E,
    /// Info Reply submessage
    InfoReply = 0x0F,
    /// NackFrag submessage
    NackFrag = 0x12,
    /// HeartbeatFrag submessage
    HeartbeatFrag = 0x13,
    /// Data submessage
    Data = 0x15,
    /// DataFrag submessage
    DataFrag = 0x16,
    /// Unknown submessage
    Unknown = 0xFF,
}

impl From<u8> for RtpsSubmessageKind {
    fn from(value: u8) -> Self {
        match value {
            0x01 => RtpsSubmessageKind::Pad,
            0x06 => RtpsSubmessageKind::AckNack,
            0x07 => RtpsSubmessageKind::Heartbeat,
            0x08 => RtpsSubmessageKind::Gap,
            0x09 => RtpsSubmessageKind::InfoTs,
            0x0C => RtpsSubmessageKind::InfoSrc,
            0x0D => RtpsSubmessageKind::InfoReplyIp4,
            0x0E => RtpsSubmessageKind::InfoDst,
            0x0F => RtpsSubmessageKind::InfoReply,
            0x12 => RtpsSubmessageKind::NackFrag,
            0x13 => RtpsSubmessageKind::HeartbeatFrag,
            0x15 => RtpsSubmessageKind::Data,
            0x16 => RtpsSubmessageKind::DataFrag,
            _ => RtpsSubmessageKind::Unknown,
        }
    }
}

impl RtpsSubmessageKind {
    /// Get submessage kind name
    pub fn name(&self) -> &'static str {
        match self {
            RtpsSubmessageKind::Pad => "PAD",
            RtpsSubmessageKind::AckNack => "ACKNACK",
            RtpsSubmessageKind::Heartbeat => "HEARTBEAT",
            RtpsSubmessageKind::Gap => "GAP",
            RtpsSubmessageKind::InfoTs => "INFO_TS",
            RtpsSubmessageKind::InfoSrc => "INFO_SRC",
            RtpsSubmessageKind::InfoReplyIp4 => "INFO_REPLY_IP4",
            RtpsSubmessageKind::InfoDst => "INFO_DST",
            RtpsSubmessageKind::InfoReply => "INFO_REPLY",
            RtpsSubmessageKind::NackFrag => "NACK_FRAG",
            RtpsSubmessageKind::HeartbeatFrag => "HEARTBEAT_FRAG",
            RtpsSubmessageKind::Data => "DATA",
            RtpsSubmessageKind::DataFrag => "DATA_FRAG",
            RtpsSubmessageKind::Unknown => "UNKNOWN",
        }
    }
}

/// GUID prefix (12 bytes)
#[derive(Debug, Clone)]
pub struct GuidPrefix {
    /// Raw bytes
    pub bytes: [u8; 12],
}

impl GuidPrefix {
    /// Convert to hex string
    pub fn to_string(&self) -> String {
        self.bytes.iter()
            .map(|b| format!("{:02x}", b))
            .collect::<Vec<_>>()
            .join("")
    }
}

/// Entity ID (4 bytes)
#[derive(Debug, Clone)]
pub struct EntityId {
    /// Entity key (3 bytes)
    pub entity_key: [u8; 3],
    /// Entity kind
    pub entity_kind: u8,
}

impl EntityId {
    /// Convert to hex string
    pub fn to_string(&self) -> String {
        format!("{:02x}{:02x}{:02x}{:02x}",
                self.entity_key[0], self.entity_key[1], 
                self.entity_key[2], self.entity_kind)
    }
}

/// RTPS submessage info
#[derive(Debug, Clone)]
pub struct RtpsSubmessage {
    /// Submessage kind
    pub kind: RtpsSubmessageKind,
    /// Submessage length
    pub length: u16,
    /// Endianness (true = little-endian)
    pub endian_little: bool,
}

/// RTPS/DDS header information
#[derive(Debug, Clone)]
pub struct RtpsHeader {
    /// Protocol version major
    pub version_major: u8,
    /// Protocol version minor
    pub version_minor: u8,
    /// Vendor ID
    pub vendor_id: u16,
    /// Vendor enum
    pub vendor: RtpsVendor,
    /// GUID prefix
    pub guid_prefix: GuidPrefix,
    /// Submessages
    pub submessages: Vec<RtpsSubmessage>,
}

impl RtpsHeader {
    /// Get vendor name
    pub fn vendor_name(&self) -> &'static str {
        self.vendor.name()
    }
    
    /// Get protocol version string
    pub fn version_string(&self) -> String {
        format!("{}.{}", self.version_major, self.version_minor)
    }
    
    /// Check if packet has DATA submessage
    pub fn has_data(&self) -> bool {
        self.submessages.iter().any(|s| s.kind == RtpsSubmessageKind::Data)
    }
    
    /// Check if packet has HEARTBEAT submessage
    pub fn has_heartbeat(&self) -> bool {
        self.submessages.iter().any(|s| s.kind == RtpsSubmessageKind::Heartbeat)
    }
    
    /// Check if packet has ACKNACK submessage
    pub fn has_acknack(&self) -> bool {
        self.submessages.iter().any(|s| s.kind == RtpsSubmessageKind::AckNack)
    }
    
    /// Check if this appears to be discovery traffic
    pub fn is_discovery(&self) -> bool {
        self.has_data() && self.submessages.iter().any(|s| s.kind == RtpsSubmessageKind::InfoTs)
    }
}

// =============================================================================
// UDS (ISO 14229) Types
// =============================================================================

/// UDS service identifiers (ISO 14229)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum UdsServiceId {
    /// Diagnostic Session Control (0x10)
    DiagnosticSessionControl = 0x10,
    /// ECU Reset (0x11)
    EcuReset = 0x11,
    /// Security Access (0x27)
    SecurityAccess = 0x27,
    /// Communication Control (0x28)
    CommunicationControl = 0x28,
    /// Tester Present (0x3E)
    TesterPresent = 0x3E,
    /// Control DTC Setting (0x85)
    ControlDtcSetting = 0x85,
    /// Response On Event (0x86)
    ResponseOnEvent = 0x86,
    /// Link Control (0x87)
    LinkControl = 0x87,
    /// Read Data By Identifier (0x22)
    ReadDataByIdentifier = 0x22,
    /// Read Memory By Address (0x23)
    ReadMemoryByAddress = 0x23,
    /// Write Data By Identifier (0x2E)
    WriteDataByIdentifier = 0x2E,
    /// Write Memory By Address (0x3D)
    WriteMemoryByAddress = 0x3D,
    /// Clear Diagnostic Information (0x14)
    ClearDiagnosticInformation = 0x14,
    /// Read DTC Information (0x19)
    ReadDtcInformation = 0x19,
    /// Input Output Control By Identifier (0x2F)
    InputOutputControlByIdentifier = 0x2F,
    /// Routine Control (0x31)
    RoutineControl = 0x31,
    /// Request Download (0x34)
    RequestDownload = 0x34,
    /// Request Upload (0x35)
    RequestUpload = 0x35,
    /// Transfer Data (0x36)
    TransferData = 0x36,
    /// Request Transfer Exit (0x37)
    RequestTransferExit = 0x37,
    /// Request File Transfer (0x38)
    RequestFileTransfer = 0x38,
    /// Unknown Service
    Unknown = 0xFF,
}

impl From<u8> for UdsServiceId {
    fn from(value: u8) -> Self {
        match value {
            0x10 => UdsServiceId::DiagnosticSessionControl,
            0x11 => UdsServiceId::EcuReset,
            0x27 => UdsServiceId::SecurityAccess,
            0x28 => UdsServiceId::CommunicationControl,
            0x3E => UdsServiceId::TesterPresent,
            0x85 => UdsServiceId::ControlDtcSetting,
            0x86 => UdsServiceId::ResponseOnEvent,
            0x87 => UdsServiceId::LinkControl,
            0x22 => UdsServiceId::ReadDataByIdentifier,
            0x23 => UdsServiceId::ReadMemoryByAddress,
            0x2E => UdsServiceId::WriteDataByIdentifier,
            0x3D => UdsServiceId::WriteMemoryByAddress,
            0x14 => UdsServiceId::ClearDiagnosticInformation,
            0x19 => UdsServiceId::ReadDtcInformation,
            0x2F => UdsServiceId::InputOutputControlByIdentifier,
            0x31 => UdsServiceId::RoutineControl,
            0x34 => UdsServiceId::RequestDownload,
            0x35 => UdsServiceId::RequestUpload,
            0x36 => UdsServiceId::TransferData,
            0x37 => UdsServiceId::RequestTransferExit,
            0x38 => UdsServiceId::RequestFileTransfer,
            _ => UdsServiceId::Unknown,
        }
    }
}

impl UdsServiceId {
    /// Get the service name
    pub fn name(&self) -> &'static str {
        match self {
            UdsServiceId::DiagnosticSessionControl => "DiagnosticSessionControl",
            UdsServiceId::EcuReset => "ECUReset",
            UdsServiceId::SecurityAccess => "SecurityAccess",
            UdsServiceId::CommunicationControl => "CommunicationControl",
            UdsServiceId::TesterPresent => "TesterPresent",
            UdsServiceId::ControlDtcSetting => "ControlDTCSetting",
            UdsServiceId::ResponseOnEvent => "ResponseOnEvent",
            UdsServiceId::LinkControl => "LinkControl",
            UdsServiceId::ReadDataByIdentifier => "ReadDataByIdentifier",
            UdsServiceId::ReadMemoryByAddress => "ReadMemoryByAddress",
            UdsServiceId::WriteDataByIdentifier => "WriteDataByIdentifier",
            UdsServiceId::WriteMemoryByAddress => "WriteMemoryByAddress",
            UdsServiceId::ClearDiagnosticInformation => "ClearDiagnosticInformation",
            UdsServiceId::ReadDtcInformation => "ReadDTCInformation",
            UdsServiceId::InputOutputControlByIdentifier => "InputOutputControlByIdentifier",
            UdsServiceId::RoutineControl => "RoutineControl",
            UdsServiceId::RequestDownload => "RequestDownload",
            UdsServiceId::RequestUpload => "RequestUpload",
            UdsServiceId::TransferData => "TransferData",
            UdsServiceId::RequestTransferExit => "RequestTransferExit",
            UdsServiceId::RequestFileTransfer => "RequestFileTransfer",
            UdsServiceId::Unknown => "Unknown",
        }
    }

    /// Check if this is a request service ID (< 0x40)
    pub fn is_request(&self) -> bool {
        (*self as u8) < 0x40
    }
}

/// UDS session types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum UdsSessionType {
    /// Default session (0x01)
    Default = 0x01,
    /// Programming session (0x02)
    Programming = 0x02,
    /// Extended diagnostic session (0x03)
    ExtendedDiagnostic = 0x03,
    /// Safety system diagnostic session (0x04)
    SafetySystemDiagnostic = 0x04,
    /// Unknown session type
    Unknown = 0xFF,
}

impl From<u8> for UdsSessionType {
    fn from(value: u8) -> Self {
        match value {
            0x01 => UdsSessionType::Default,
            0x02 => UdsSessionType::Programming,
            0x03 => UdsSessionType::ExtendedDiagnostic,
            0x04 => UdsSessionType::SafetySystemDiagnostic,
            _ => UdsSessionType::Unknown,
        }
    }
}

impl UdsSessionType {
    /// Get the session type name
    pub fn name(&self) -> &'static str {
        match self {
            UdsSessionType::Default => "DefaultSession",
            UdsSessionType::Programming => "ProgrammingSession",
            UdsSessionType::ExtendedDiagnostic => "ExtendedDiagnosticSession",
            UdsSessionType::SafetySystemDiagnostic => "SafetySystemDiagnosticSession",
            UdsSessionType::Unknown => "Unknown",
        }
    }
}

/// UDS ECU reset types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum UdsResetType {
    /// Hard reset (0x01)
    HardReset = 0x01,
    /// Key off on reset (0x02)
    KeyOffOnReset = 0x02,
    /// Soft reset (0x03)
    SoftReset = 0x03,
    /// Unknown reset type
    Unknown = 0xFF,
}

impl From<u8> for UdsResetType {
    fn from(value: u8) -> Self {
        match value {
            0x01 => UdsResetType::HardReset,
            0x02 => UdsResetType::KeyOffOnReset,
            0x03 => UdsResetType::SoftReset,
            _ => UdsResetType::Unknown,
        }
    }
}

impl UdsResetType {
    /// Get the reset type name
    pub fn name(&self) -> &'static str {
        match self {
            UdsResetType::HardReset => "HardReset",
            UdsResetType::KeyOffOnReset => "KeyOffOnReset",
            UdsResetType::SoftReset => "SoftReset",
            UdsResetType::Unknown => "Unknown",
        }
    }
}

/// UDS Negative Response Codes (ISO 14229)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum UdsNrc {
    /// General reject (0x10)
    GeneralReject = 0x10,
    /// Service not supported (0x11)
    ServiceNotSupported = 0x11,
    /// Sub-function not supported (0x12)
    SubFunctionNotSupported = 0x12,
    /// Incorrect message length or invalid format (0x13)
    IncorrectMessageLengthOrInvalidFormat = 0x13,
    /// Response too long (0x14)
    ResponseTooLong = 0x14,
    /// Busy repeat request (0x21)
    BusyRepeatRequest = 0x21,
    /// Conditions not correct (0x22)
    ConditionsNotCorrect = 0x22,
    /// Request sequence error (0x24)
    RequestSequenceError = 0x24,
    /// Request out of range (0x31)
    RequestOutOfRange = 0x31,
    /// Security access denied (0x33)
    SecurityAccessDenied = 0x33,
    /// Invalid key (0x35)
    InvalidKey = 0x35,
    /// Exceeded number of attempts (0x36)
    ExceededNumberOfAttempts = 0x36,
    /// Required time delay not expired (0x37)
    RequiredTimeDelayNotExpired = 0x37,
    /// Upload/download not accepted (0x70)
    UploadDownloadNotAccepted = 0x70,
    /// Transfer data suspended (0x71)
    TransferDataSuspended = 0x71,
    /// General programming failure (0x72)
    GeneralProgrammingFailure = 0x72,
    /// Wrong block sequence counter (0x73)
    WrongBlockSequenceCounter = 0x73,
    /// Request correctly received, response pending (0x78)
    RequestCorrectlyReceivedResponsePending = 0x78,
    /// Sub-function not supported in active session (0x7E)
    SubFunctionNotSupportedInActiveSession = 0x7E,
    /// Service not supported in active session (0x7F)
    ServiceNotSupportedInActiveSession = 0x7F,
    /// Unknown NRC
    Unknown = 0x00,
}

impl From<u8> for UdsNrc {
    fn from(value: u8) -> Self {
        match value {
            0x10 => UdsNrc::GeneralReject,
            0x11 => UdsNrc::ServiceNotSupported,
            0x12 => UdsNrc::SubFunctionNotSupported,
            0x13 => UdsNrc::IncorrectMessageLengthOrInvalidFormat,
            0x14 => UdsNrc::ResponseTooLong,
            0x21 => UdsNrc::BusyRepeatRequest,
            0x22 => UdsNrc::ConditionsNotCorrect,
            0x24 => UdsNrc::RequestSequenceError,
            0x31 => UdsNrc::RequestOutOfRange,
            0x33 => UdsNrc::SecurityAccessDenied,
            0x35 => UdsNrc::InvalidKey,
            0x36 => UdsNrc::ExceededNumberOfAttempts,
            0x37 => UdsNrc::RequiredTimeDelayNotExpired,
            0x70 => UdsNrc::UploadDownloadNotAccepted,
            0x71 => UdsNrc::TransferDataSuspended,
            0x72 => UdsNrc::GeneralProgrammingFailure,
            0x73 => UdsNrc::WrongBlockSequenceCounter,
            0x78 => UdsNrc::RequestCorrectlyReceivedResponsePending,
            0x7E => UdsNrc::SubFunctionNotSupportedInActiveSession,
            0x7F => UdsNrc::ServiceNotSupportedInActiveSession,
            _ => UdsNrc::Unknown,
        }
    }
}

impl UdsNrc {
    /// Get the NRC name
    pub fn name(&self) -> &'static str {
        match self {
            UdsNrc::GeneralReject => "GeneralReject",
            UdsNrc::ServiceNotSupported => "ServiceNotSupported",
            UdsNrc::SubFunctionNotSupported => "SubFunctionNotSupported",
            UdsNrc::IncorrectMessageLengthOrInvalidFormat => "IncorrectMessageLengthOrInvalidFormat",
            UdsNrc::ResponseTooLong => "ResponseTooLong",
            UdsNrc::BusyRepeatRequest => "BusyRepeatRequest",
            UdsNrc::ConditionsNotCorrect => "ConditionsNotCorrect",
            UdsNrc::RequestSequenceError => "RequestSequenceError",
            UdsNrc::RequestOutOfRange => "RequestOutOfRange",
            UdsNrc::SecurityAccessDenied => "SecurityAccessDenied",
            UdsNrc::InvalidKey => "InvalidKey",
            UdsNrc::ExceededNumberOfAttempts => "ExceededNumberOfAttempts",
            UdsNrc::RequiredTimeDelayNotExpired => "RequiredTimeDelayNotExpired",
            UdsNrc::UploadDownloadNotAccepted => "UploadDownloadNotAccepted",
            UdsNrc::TransferDataSuspended => "TransferDataSuspended",
            UdsNrc::GeneralProgrammingFailure => "GeneralProgrammingFailure",
            UdsNrc::WrongBlockSequenceCounter => "WrongBlockSequenceCounter",
            UdsNrc::RequestCorrectlyReceivedResponsePending => "RequestCorrectlyReceivedResponsePending",
            UdsNrc::SubFunctionNotSupportedInActiveSession => "SubFunctionNotSupportedInActiveSession",
            UdsNrc::ServiceNotSupportedInActiveSession => "ServiceNotSupportedInActiveSession",
            UdsNrc::Unknown => "Unknown",
        }
    }

    /// Get a human-readable description of the NRC
    pub fn description(&self) -> &'static str {
        match self {
            UdsNrc::GeneralReject => "Service was rejected",
            UdsNrc::ServiceNotSupported => "Service is not supported",
            UdsNrc::SubFunctionNotSupported => "Sub-function is not supported",
            UdsNrc::IncorrectMessageLengthOrInvalidFormat => "Incorrect message length or invalid format",
            UdsNrc::ResponseTooLong => "Response is too long",
            UdsNrc::BusyRepeatRequest => "Server is busy, try again",
            UdsNrc::ConditionsNotCorrect => "Conditions not correct for requested service",
            UdsNrc::RequestSequenceError => "Request sequence error",
            UdsNrc::RequestOutOfRange => "Request parameter out of range",
            UdsNrc::SecurityAccessDenied => "Security access denied",
            UdsNrc::InvalidKey => "Invalid security key",
            UdsNrc::ExceededNumberOfAttempts => "Exceeded number of security access attempts",
            UdsNrc::RequiredTimeDelayNotExpired => "Security time delay not expired",
            UdsNrc::UploadDownloadNotAccepted => "Upload/download not accepted",
            UdsNrc::TransferDataSuspended => "Transfer data suspended",
            UdsNrc::GeneralProgrammingFailure => "General programming failure",
            UdsNrc::WrongBlockSequenceCounter => "Wrong block sequence counter",
            UdsNrc::RequestCorrectlyReceivedResponsePending => "Response pending",
            UdsNrc::SubFunctionNotSupportedInActiveSession => "Sub-function not supported in current session",
            UdsNrc::ServiceNotSupportedInActiveSession => "Service not supported in current session",
            UdsNrc::Unknown => "Unknown NRC",
        }
    }
}

/// UDS Data Identifier (DID)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct UdsDataIdentifier(pub u16);

impl UdsDataIdentifier {
    /// Create a new DID
    pub fn new(value: u16) -> Self {
        Self(value)
    }

    /// Get the raw value
    pub fn value(&self) -> u16 {
        self.0
    }

    /// Check if this is an OEM-specific DID (0xF100-0xF1FF)
    pub fn is_oem_specific(&self) -> bool {
        (0xF100..=0xF1FF).contains(&self.0)
    }

    /// Check if this is a vehicle identification DID (0xF190-0xF19F)
    pub fn is_vehicle_identification(&self) -> bool {
        (0xF190..=0xF19F).contains(&self.0)
    }
}

impl std::fmt::Display for UdsDataIdentifier {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "0x{:04X}", self.0)
    }
}

/// UDS Routine Identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct UdsRoutineIdentifier(pub u16);

impl UdsRoutineIdentifier {
    /// Create a new routine identifier
    pub fn new(value: u16) -> Self {
        Self(value)
    }

    /// Get the raw value
    pub fn value(&self) -> u16 {
        self.0
    }
}

impl std::fmt::Display for UdsRoutineIdentifier {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "0x{:04X}", self.0)
    }
}

/// UDS header information (ISO 14229)
#[derive(Debug, Clone)]
pub struct UdsHeader {
    /// Service ID
    pub service_id: UdsServiceId,
    /// Raw service ID byte
    pub service_id_raw: u8,
    /// Sub-function (if applicable)
    pub sub_function: Option<u8>,
    /// Suppress positive response flag
    pub suppress_positive_response: bool,
    /// Negative response code (for NRC responses)
    pub negative_response_code: Option<UdsNrc>,
    /// Rejected service ID (for NRC responses)
    pub rejected_service_id: Option<UdsServiceId>,
}

impl UdsHeader {
    /// Check if this is a request
    pub fn is_request(&self) -> bool {
        self.service_id_raw < 0x40 && self.negative_response_code.is_none()
    }

    /// Check if this is a positive response
    pub fn is_positive_response(&self) -> bool {
        self.service_id_raw >= 0x40 && self.service_id_raw != 0x7F
    }

    /// Check if this is a negative response
    pub fn is_negative_response(&self) -> bool {
        self.service_id_raw == 0x7F
    }

    /// Get the service name
    pub fn service_name(&self) -> &'static str {
        self.service_id.name()
    }
}

/// UDS decoder for parsing UDS messages
pub struct UdsDecoder {
    // Placeholder for future state
}

impl UdsDecoder {
    /// Create a new UDS decoder
    pub fn new() -> Self {
        Self {}
    }

    /// Decode a UDS message from bytes
    pub fn decode(&self, data: &[u8]) -> Option<UdsHeader> {
        if data.is_empty() {
            return None;
        }

        let service_id_raw = data[0];

        // Check for negative response
        if service_id_raw == 0x7F && data.len() >= 3 {
            let rejected_sid = UdsServiceId::from(data[1]);
            let nrc = UdsNrc::from(data[2]);
            return Some(UdsHeader {
                service_id: UdsServiceId::Unknown,
                service_id_raw,
                sub_function: None,
                suppress_positive_response: false,
                negative_response_code: Some(nrc),
                rejected_service_id: Some(rejected_sid),
            });
        }

        // Determine if request or positive response
        let base_sid = if service_id_raw >= 0x40 && service_id_raw != 0x7F {
            service_id_raw - 0x40 // Response
        } else {
            service_id_raw // Request
        };

        let service_id = UdsServiceId::from(base_sid);

        // Extract sub-function if present
        let (sub_function, suppress_positive_response) = if data.len() > 1 {
            let sf = data[1];
            // Bit 7 is suppress positive response flag
            (Some(sf & 0x7F), (sf & 0x80) != 0)
        } else {
            (None, false)
        };

        Some(UdsHeader {
            service_id,
            service_id_raw,
            sub_function,
            suppress_positive_response,
            negative_response_code: None,
            rejected_service_id: None,
        })
    }

    /// Check if data represents a UDS request
    pub fn is_request(data: &[u8]) -> bool {
        if data.is_empty() {
            return false;
        }
        data[0] < 0x40
    }

    /// Check if data represents a UDS positive response
    pub fn is_positive_response(data: &[u8]) -> bool {
        if data.is_empty() {
            return false;
        }
        data[0] >= 0x40 && data[0] != 0x7F
    }

    /// Check if data represents a UDS negative response
    pub fn is_negative_response(data: &[u8]) -> bool {
        if data.is_empty() {
            return false;
        }
        data[0] == 0x7F
    }
}

impl Default for UdsDecoder {
    fn default() -> Self {
        Self::new()
    }
}

/// Result of decoding a packet
pub struct DecodeResult {
    handle: wadjet_sys::wadjet_decode_result_t,
    // Cached headers - populated on first access
    ethernet: Option<EthernetHeader>,
    ipv4: Option<Ipv4Header>,
    udp: Option<UdpHeader>,
    tcp: Option<TcpHeader>,
    someip: Option<SomeIpHeader>,
    doip: Option<DoIpHeader>,
    gptp: Option<GptpHeader>,
}

impl DecodeResult {
    /// Decode a packet
    pub(crate) fn decode_packet(packet: &Packet) -> Option<Self> {
        let data = packet.data();
        if data.is_empty() {
            return None;
        }
        
        let mut handle: wadjet_sys::wadjet_decode_result_t = ptr::null_mut();
        
        let err = unsafe {
            wadjet_sys::wadjet_decode_packet(data.as_ptr(), data.len(), &mut handle)
        };

        if err != wadjet_sys::wadjet_error_t::WADJET_OK || handle.is_null() {
            return None;
        }

        // Check if decode was successful
        let success = unsafe { wadjet_sys::wadjet_decode_result_success(handle) };
        if !success {
            unsafe { wadjet_sys::wadjet_decode_result_destroy(handle) };
            return None;
        }

        // Extract all available headers
        let ethernet = unsafe {
            let mut hdr: wadjet_sys::wadjet_ethernet_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_ethernet(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(EthernetHeader::from_c(&hdr))
            } else {
                None
            }
        };

        let ipv4 = unsafe {
            let mut hdr: wadjet_sys::wadjet_ipv4_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_ipv4(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(Ipv4Header::from_c(&hdr))
            } else {
                None
            }
        };

        let udp = unsafe {
            let mut hdr: wadjet_sys::wadjet_udp_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_udp(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(UdpHeader::from_c(&hdr))
            } else {
                None
            }
        };

        let tcp = unsafe {
            let mut hdr: wadjet_sys::wadjet_tcp_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_tcp(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(TcpHeader::from_c(&hdr))
            } else {
                None
            }
        };

        let someip = unsafe {
            let mut hdr: wadjet_sys::wadjet_someip_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_someip(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(SomeIpHeader::from_c(&hdr))
            } else {
                None
            }
        };

        let doip = unsafe {
            let mut hdr: wadjet_sys::wadjet_doip_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_doip(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(DoIpHeader::from_c(&hdr))
            } else {
                None
            }
        };

        let gptp = unsafe {
            let mut hdr: wadjet_sys::wadjet_gptp_header_t = std::mem::zeroed();
            if wadjet_sys::wadjet_decode_result_gptp(handle, &mut hdr) == wadjet_sys::wadjet_error_t::WADJET_OK {
                Some(GptpHeader::from_c(&hdr))
            } else {
                None
            }
        };

        Some(Self {
            handle,
            ethernet,
            ipv4,
            udp,
            tcp,
            someip,
            doip,
            gptp,
        })
    }

    /// Get Ethernet header if present
    pub fn ethernet(&self) -> Option<&EthernetHeader> {
        self.ethernet.as_ref()
    }

    /// Get IPv4 header if present
    pub fn ipv4(&self) -> Option<&Ipv4Header> {
        self.ipv4.as_ref()
    }

    /// Get UDP header if present
    pub fn udp(&self) -> Option<&UdpHeader> {
        self.udp.as_ref()
    }

    /// Get TCP header if present
    pub fn tcp(&self) -> Option<&TcpHeader> {
        self.tcp.as_ref()
    }

    /// Get SOME/IP header if present
    pub fn someip(&self) -> Option<&SomeIpHeader> {
        self.someip.as_ref()
    }

    /// Get DoIP header if present
    pub fn doip(&self) -> Option<&DoIpHeader> {
        self.doip.as_ref()
    }

    /// Get gPTP header if present
    pub fn gptp(&self) -> Option<&GptpHeader> {
        self.gptp.as_ref()
    }

    /// Check if the result has a specific protocol layer
    pub fn has_layer(&self, protocol: Protocol) -> bool {
        unsafe {
            wadjet_sys::wadjet_decode_result_has_layer(self.handle, protocol.to_c())
        }
    }

    /// Get the payload data after a specific protocol layer
    pub fn payload_after(&self, protocol: Protocol) -> Option<&[u8]> {
        let mut data: *const u8 = ptr::null();
        let mut length: usize = 0;
        
        let err = unsafe {
            wadjet_sys::wadjet_decode_result_payload(
                self.handle,
                protocol.to_c(),
                &mut data,
                &mut length,
            )
        };
        
        if err != wadjet_sys::wadjet_error_t::WADJET_OK || data.is_null() || length == 0 {
            return None;
        }
        
        Some(unsafe { std::slice::from_raw_parts(data, length) })
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
            .field("has_ethernet", &self.ethernet.is_some())
            .field("has_ipv4", &self.ipv4.is_some())
            .field("has_udp", &self.udp.is_some())
            .field("has_tcp", &self.tcp.is_some())
            .field("has_someip", &self.someip.is_some())
            .field("has_doip", &self.doip.is_some())
            .field("has_gptp", &self.gptp.is_some())
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
            sequence_number: 0,
            ack_number: 0,
            data_offset: 5,
            syn: true,
            ack: true,
            fin: false,
            rst: false,
            psh: false,
            urg: false,
            window_size: 65535,
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
            message_type: SomeIpMessageType::Request,
            return_code: SomeIpReturnCode::Ok,
            is_service_discovery: false,
        };

        assert!(someip.is_request());
        
        someip.message_type = SomeIpMessageType::Response;
        assert!(someip.is_response());
        
        someip.message_type = SomeIpMessageType::Notification;
        assert!(someip.is_notification());
    }

    #[test]
    fn test_doip_payload_types() {
        let doip = DoIpHeader {
            protocol_version: 0x02,
            inverse_version: 0xFD,
            payload_type: DoIpPayloadType::DiagnosticMessage,
            payload_length: 100,
            version_valid: true,
        };

        assert_eq!(doip.payload_type_name(), "Diagnostic Message");
    }
}

/// @file protocol_bindings.cpp
/// @brief Protocol header type bindings
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include "wadjet/protocols/doip.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/gptp/gptp.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/someip_sd.hpp"
#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/udp.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_protocols(py::module_& m) {
    using namespace wadjet::protocols;

    // =========================================================================
    // Ethernet
    // =========================================================================
    py::class_<ethernet::VlanTag>(m, "VlanTag",
        "802.1Q VLAN tag")
        .def_readonly("tci", &ethernet::VlanTag::tci, "Tag Control Information")
        .def_readonly("tpid", &ethernet::VlanTag::tpid, "Tag Protocol ID")
        .def("vid", &ethernet::VlanTag::vid, "Get VLAN ID (12 bits)")
        .def("pcp", &ethernet::VlanTag::pcp, "Get Priority Code Point (3 bits)")
        .def("dei", &ethernet::VlanTag::dei, "Get Drop Eligible Indicator")
        .def("__repr__", [](const ethernet::VlanTag& tag) {
            return "<VlanTag vid=" + std::to_string(tag.vid()) + ">";
        });

    py::class_<ethernet::EthernetHeader>(m, "EthernetHeader",
        "Decoded Ethernet frame header")
        .def_readonly("src_mac", &ethernet::EthernetHeader::src_mac,
                     "Source MAC address (6 bytes)")
        .def_readonly("dst_mac", &ethernet::EthernetHeader::dst_mac,
                     "Destination MAC address (6 bytes)")
        .def_readonly("ethertype", &ethernet::EthernetHeader::ethertype,
                     "EtherType (or length for 802.3)")
        .def_readonly("vlan", &ethernet::EthernetHeader::vlan,
                     "Outer VLAN tag (if present)")
        .def_readonly("vlan_inner", &ethernet::EthernetHeader::vlan_inner,
                     "Inner VLAN tag (QinQ)")
        .def_readonly("header_length", &ethernet::EthernetHeader::header_length,
                     "Total header length including VLAN tags")
        .def("has_vlan", &ethernet::EthernetHeader::has_vlan,
             "Check if frame has VLAN tag")
        .def("src_mac_string", &ethernet::EthernetHeader::src_mac_string,
             "Get source MAC as string (xx:xx:xx:xx:xx:xx)")
        .def("dst_mac_string", &ethernet::EthernetHeader::dst_mac_string,
             "Get destination MAC as string")
        .def("__repr__", [](const ethernet::EthernetHeader& hdr) {
            return "<EthernetHeader " + hdr.src_mac_string() + 
                   " -> " + hdr.dst_mac_string() + 
                   " type=0x" + ([&]() {
                       char buf[5];
                       std::snprintf(buf, sizeof(buf), "%04x", hdr.ethertype);
                       return std::string(buf);
                   })() + ">";
        });

    // =========================================================================
    // IPv4
    // =========================================================================
    py::class_<ipv4::IPv4Header>(m, "IPv4Header",
        "Decoded IPv4 header")
        .def_readonly("version", &ipv4::IPv4Header::version)
        .def_readonly("ihl", &ipv4::IPv4Header::ihl, "Internet Header Length")
        .def_readonly("dscp", &ipv4::IPv4Header::dscp, 
                     "Differentiated Services Code Point")
        .def_readonly("ecn", &ipv4::IPv4Header::ecn,
                     "Explicit Congestion Notification")
        .def_readonly("total_length", &ipv4::IPv4Header::total_length)
        .def_readonly("identification", &ipv4::IPv4Header::identification)
        .def_readonly("flags", &ipv4::IPv4Header::flags)
        .def_readonly("fragment_offset", &ipv4::IPv4Header::fragment_offset)
        .def_readonly("ttl", &ipv4::IPv4Header::ttl, "Time To Live")
        .def_readonly("protocol", &ipv4::IPv4Header::protocol,
                     "Protocol number (6=TCP, 17=UDP)")
        .def_readonly("header_checksum", &ipv4::IPv4Header::header_checksum)
        .def_readonly("src_addr", &ipv4::IPv4Header::src_addr, 
                     "Source IP (network byte order)")
        .def_readonly("dst_addr", &ipv4::IPv4Header::dst_addr,
                     "Destination IP (network byte order)")
        .def_readonly("header_length", &ipv4::IPv4Header::header_length,
                     "Header length in bytes")
        .def("src_ip_string", &ipv4::IPv4Header::src_ip_string,
             "Get source IP as string")
        .def("dst_ip_string", &ipv4::IPv4Header::dst_ip_string,
             "Get destination IP as string")
        .def("is_fragment", &ipv4::IPv4Header::is_fragment,
             "Check if packet is a fragment")
        .def("__repr__", [](const ipv4::IPv4Header& hdr) {
            return "<IPv4Header " + hdr.src_ip_string() + 
                   " -> " + hdr.dst_ip_string() + 
                   " proto=" + std::to_string(hdr.protocol) + ">";
        });

    // =========================================================================
    // UDP
    // =========================================================================
    py::class_<udp::UdpHeader>(m, "UdpHeader",
        "Decoded UDP header")
        .def_readonly("src_port", &udp::UdpHeader::src_port)
        .def_readonly("dst_port", &udp::UdpHeader::dst_port)
        .def_readonly("length", &udp::UdpHeader::length, "UDP length")
        .def_readonly("checksum", &udp::UdpHeader::checksum)
        .def("__repr__", [](const udp::UdpHeader& hdr) {
            return "<UdpHeader " + std::to_string(hdr.src_port) + 
                   " -> " + std::to_string(hdr.dst_port) + ">";
        });

    // =========================================================================
    // TCP
    // =========================================================================
    py::class_<tcp::TcpHeader>(m, "TcpHeader",
        "Decoded TCP header")
        .def_readonly("src_port", &tcp::TcpHeader::src_port)
        .def_readonly("dst_port", &tcp::TcpHeader::dst_port)
        .def_readonly("seq_num", &tcp::TcpHeader::seq_num, "Sequence number")
        .def_readonly("ack_num", &tcp::TcpHeader::ack_num, "Acknowledgment number")
        .def_readonly("data_offset", &tcp::TcpHeader::data_offset,
                     "Data offset (header length / 4)")
        .def_readonly("window", &tcp::TcpHeader::window, "Window size")
        .def_readonly("checksum", &tcp::TcpHeader::checksum)
        .def_readonly("urgent_ptr", &tcp::TcpHeader::urgent_ptr)
        .def_readonly("header_length", &tcp::TcpHeader::header_length,
                     "Header length in bytes")
        .def("is_syn", &tcp::TcpHeader::is_syn, "Check SYN flag")
        .def("is_ack", [](const tcp::TcpHeader& hdr) { 
            return hdr.flags.ack; 
        }, "Check ACK flag")
        .def("is_fin", &tcp::TcpHeader::is_fin, "Check FIN flag")
        .def("is_rst", &tcp::TcpHeader::is_rst, "Check RST flag")
        .def("is_psh", [](const tcp::TcpHeader& hdr) { 
            return hdr.flags.psh; 
        }, "Check PSH flag")
        .def("__repr__", [](const tcp::TcpHeader& hdr) {
            std::string flags;
            if (hdr.is_syn()) flags += "S";
            if (hdr.flags.ack) flags += "A";
            if (hdr.is_fin()) flags += "F";
            if (hdr.is_rst()) flags += "R";
            if (hdr.flags.psh) flags += "P";
            return "<TcpHeader " + std::to_string(hdr.src_port) + 
                   " -> " + std::to_string(hdr.dst_port) + 
                   " [" + flags + "]>";
        });

    // =========================================================================
    // SOME/IP
    // =========================================================================
    py::enum_<someip::MessageType>(m, "MessageType",
        "SOME/IP message types")
        .value("Request", someip::MessageType::Request)
        .value("RequestNoReturn", someip::MessageType::RequestNoReturn)
        .value("Notification", someip::MessageType::Notification)
        .value("Response", someip::MessageType::Response)
        .value("Error", someip::MessageType::Error)
        .export_values();

    py::enum_<someip::ReturnCode>(m, "ReturnCode",
        "SOME/IP return codes")
        .value("Ok", someip::ReturnCode::Ok)
        .value("NotOk", someip::ReturnCode::NotOk)
        .value("UnknownService", someip::ReturnCode::UnknownService)
        .value("UnknownMethod", someip::ReturnCode::UnknownMethod)
        .value("NotReady", someip::ReturnCode::NotReady)
        .value("NotReachable", someip::ReturnCode::NotReachable)
        .value("Timeout", someip::ReturnCode::Timeout)
        .export_values();

    py::class_<someip::SomeIpHeader>(m, "SomeIpHeader",
        "Decoded SOME/IP header")
        .def_readonly("service_id", &someip::SomeIpHeader::service_id)
        .def_readonly("method_id", &someip::SomeIpHeader::method_id,
                     "Method ID or Event ID")
        .def_readonly("length", &someip::SomeIpHeader::length)
        .def_readonly("client_id", &someip::SomeIpHeader::client_id)
        .def_readonly("session_id", &someip::SomeIpHeader::session_id)
        .def_readonly("protocol_version", &someip::SomeIpHeader::protocol_version)
        .def_readonly("interface_version", &someip::SomeIpHeader::interface_version)
        .def_readonly("message_type", &someip::SomeIpHeader::message_type)
        .def_readonly("return_code", &someip::SomeIpHeader::return_code)
        .def("is_request", &someip::SomeIpHeader::is_request)
        .def("is_response", &someip::SomeIpHeader::is_response)
        .def("is_notification", &someip::SomeIpHeader::is_notification)
        .def("is_error", &someip::SomeIpHeader::is_error)
        .def("is_service_discovery", &someip::SomeIpHeader::is_service_discovery)
        .def("payload_length", &someip::SomeIpHeader::payload_length)
        .def("__repr__", [](const someip::SomeIpHeader& hdr) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), 
                         "<SomeIpHeader service=0x%04x method=0x%04x %s>",
                         hdr.service_id, hdr.method_id,
                         std::string(someip::message_type_string(hdr.message_type)).c_str());
            return std::string(buf);
        });

    // =========================================================================
    // SOME/IP-SD
    // =========================================================================
    py::class_<someip_sd::SomeIpSdHeader>(m, "SomeIpSdHeader",
        "Decoded SOME/IP Service Discovery header")
        .def_readonly("flags", &someip_sd::SomeIpSdHeader::flags)
        .def_readonly("entries", &someip_sd::SomeIpSdHeader::entries)
        .def_readonly("options", &someip_sd::SomeIpSdHeader::options)
        .def("reboot_flag", &someip_sd::SomeIpSdHeader::reboot_flag)
        .def("unicast_flag", &someip_sd::SomeIpSdHeader::unicast_flag)
        .def("__repr__", [](const someip_sd::SomeIpSdHeader& hdr) {
            return "<SomeIpSdHeader entries=" + 
                   std::to_string(hdr.entries.size()) + 
                   " options=" + std::to_string(hdr.options.size()) + ">";
        });

    // =========================================================================
    // DoIP
    // =========================================================================
    py::enum_<doip::PayloadType>(m, "PayloadType",
        "DoIP payload types")
        .value("GenericNack", doip::PayloadType::GenericNack)
        .value("VehicleIdentificationRequest", 
               doip::PayloadType::VehicleIdentificationRequest)
        .value("VehicleIdentificationRequestWithEID",
               doip::PayloadType::VehicleIdentificationRequestWithEID)
        .value("VehicleIdentificationRequestWithVIN",
               doip::PayloadType::VehicleIdentificationRequestWithVIN)
        .value("VehicleAnnouncementOrIdentificationResponse",
               doip::PayloadType::VehicleAnnouncementOrIdentificationResponse)
        .value("RoutingActivationRequest",
               doip::PayloadType::RoutingActivationRequest)
        .value("RoutingActivationResponse",
               doip::PayloadType::RoutingActivationResponse)
        .value("DiagnosticMessage",
               doip::PayloadType::DiagnosticMessage)
        .value("DiagnosticMessagePositiveAck",
               doip::PayloadType::DiagnosticMessagePositiveAck)
        .value("DiagnosticMessageNegativeAck",
               doip::PayloadType::DiagnosticMessageNegativeAck)
        .export_values();

    py::class_<doip::DoIPHeader>(m, "DoIPHeader",
        "Decoded DoIP header")
        .def_readonly("version", &doip::DoIPHeader::version)
        .def_readonly("inverse_version", &doip::DoIPHeader::inverse_version)
        .def_readonly("payload_type", &doip::DoIPHeader::payload_type)
        .def_readonly("payload_length", &doip::DoIPHeader::payload_length)
        .def("is_version_valid", &doip::DoIPHeader::is_version_valid)
        .def("is_diagnostic_message", &doip::DoIPHeader::is_diagnostic_message)
        .def("is_routing_activation", [](const doip::DoIPHeader& hdr) {
            return hdr.payload_type == doip::PayloadType::RoutingActivationRequest ||
                   hdr.payload_type == doip::PayloadType::RoutingActivationResponse;
        })
        .def("__repr__", [](const doip::DoIPHeader& hdr) {
            return "<DoIPHeader type=" + 
                   std::to_string(static_cast<int>(hdr.payload_type)) +
                   " len=" + std::to_string(hdr.payload_length) + ">";
        });

    // =========================================================================
    // gPTP (IEEE 802.1AS)
    // =========================================================================
    py::enum_<gptp::MessageType>(m, "GptpMessageType", "gPTP message types")
        .value("Sync", gptp::MessageType::Sync)
        .value("Delay_Req", gptp::MessageType::Delay_Req)
        .value("Pdelay_Req", gptp::MessageType::Pdelay_Req)
        .value("Pdelay_Resp", gptp::MessageType::Pdelay_Resp)
        .value("Follow_Up", gptp::MessageType::Follow_Up)
        .value("Delay_Resp", gptp::MessageType::Delay_Resp)
        .value("Pdelay_Resp_Follow_Up", gptp::MessageType::Pdelay_Resp_Follow_Up)
        .value("Announce", gptp::MessageType::Announce)
        .value("Signaling", gptp::MessageType::Signaling)
        .value("Management", gptp::MessageType::Management)
        .export_values();

    py::class_<gptp::ClockIdentity>(m, "ClockIdentity",
                                    "PTP clock identity (8 bytes, typically EUI-64)")
        .def_readonly("bytes", &gptp::ClockIdentity::bytes)
        .def("to_string", &gptp::ClockIdentity::to_string)
        .def("__repr__", [](const gptp::ClockIdentity& ci) {
            return "<ClockIdentity " + ci.to_string() + ">";
        });

    py::class_<gptp::PortIdentity>(m, "PortIdentity",
                                   "PTP port identity (clock identity + port number)")
        .def_readonly("clock_identity", &gptp::PortIdentity::clock_identity)
        .def_readonly("port_number", &gptp::PortIdentity::port_number)
        .def("__repr__", [](const gptp::PortIdentity& pi) {
            return "<PortIdentity " + pi.clock_identity.to_string() + ":" +
                   std::to_string(pi.port_number) + ">";
        });

    py::class_<gptp::ScaledNs>(m, "ScaledNs", "Scaled nanoseconds (64.16 fixed-point)")
        .def_readonly("scaled_ns", &gptp::ScaledNs::scaled_ns)
        .def("to_nanoseconds", &gptp::ScaledNs::to_nanoseconds)
        .def("__repr__", [](const gptp::ScaledNs& ns) {
            return "<ScaledNs " + std::to_string(ns.to_nanoseconds()) + "ns>";
        });

    py::class_<gptp::GptpTimestamp>(m, "GptpTimestamp",
                                    "PTP timestamp (48-bit seconds + 32-bit nanoseconds)")
        .def_readonly("seconds_msb", &gptp::GptpTimestamp::seconds_msb)
        .def_readonly("seconds_lsb", &gptp::GptpTimestamp::seconds_lsb)
        .def_readonly("nanoseconds", &gptp::GptpTimestamp::nanoseconds)
        .def("seconds", &gptp::GptpTimestamp::seconds)
        .def("__repr__", [](const gptp::GptpTimestamp& ts) {
            return "<GptpTimestamp " + std::to_string(ts.seconds()) + "s " +
                   std::to_string(ts.nanoseconds) + "ns>";
        });

    py::class_<gptp::GptpHeader>(m, "GptpHeader", "Decoded gPTP header (IEEE 802.1AS)")
        .def_readonly("transport_specific", &gptp::GptpHeader::transport_specific)
        .def_readonly("message_type", &gptp::GptpHeader::message_type)
        .def_readonly("version", &gptp::GptpHeader::version)
        .def_readonly("message_length", &gptp::GptpHeader::message_length)
        .def_readonly("domain_number", &gptp::GptpHeader::domain_number)
        .def_readonly("flags", &gptp::GptpHeader::flags)
        .def_readonly("correction_field", &gptp::GptpHeader::correction_field)
        .def_readonly("source_port_identity", &gptp::GptpHeader::source_port_identity)
        .def_readonly("sequence_id", &gptp::GptpHeader::sequence_id)
        .def_readonly("control", &gptp::GptpHeader::control)
        .def_readonly("log_message_interval", &gptp::GptpHeader::log_message_interval)
        .def("is_two_step", &gptp::GptpHeader::is_two_step)
        .def("is_event", &gptp::GptpHeader::is_event)
        .def("is_general", &gptp::GptpHeader::is_general)
        .def("message_type_string",
             [](const gptp::GptpHeader& hdr) {
                 return std::string(gptp::message_type_to_string(hdr.message_type));
             })
        .def("__repr__", [](const gptp::GptpHeader& hdr) {
            return "<GptpHeader type=" +
                   std::string(gptp::message_type_to_string(hdr.message_type)) +
                   " seq=" + std::to_string(hdr.sequence_id) +
                   " domain=" + std::to_string(hdr.domain_number) + ">";
        });
}

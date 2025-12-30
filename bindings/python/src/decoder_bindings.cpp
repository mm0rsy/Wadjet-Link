/// @file decoder_bindings.cpp
/// @brief Protocol decoder and dispatcher bindings
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "wadjet/protocols/dispatcher.hpp"
#include "wadjet/protocols/ethernet.hpp"
#include "wadjet/protocols/ipv4.hpp"
#include "wadjet/protocols/udp.hpp"
#include "wadjet/protocols/tcp.hpp"
#include "wadjet/protocols/someip.hpp"
#include "wadjet/protocols/someip_sd.hpp"
#include "wadjet/protocols/doip.hpp"
#include "wadjet/net/packet.hpp"

namespace py = pybind11;
using namespace wadjet::protocols;

void bind_decoders(py::module_& m) {
    // =========================================================================
    // DecodeResult
    // =========================================================================
    py::class_<DecodeResult>(m, "DecodeResult",
        R"doc(
            Result of decoding a packet through the protocol stack.
            
            Contains decoded headers for each layer that was successfully parsed.
            Use the has_* methods to check which protocols are present, then
            access the headers via properties.
            
            Example:
                >>> result = wadjet.decode_packet(packet_data)
                >>> if result.has_ethernet():
                ...     eth = result.ethernet()
                ...     print(f"Src MAC: {eth.src_mac_string()}")
                >>> if result.has_someip():
                ...     someip = result.someip()
                ...     print(f"Service: {someip.service_id:#06x}")
        )doc")
        // Layer presence checks
        .def("has_ethernet", [](const DecodeResult& r) {
            return r.get_layer<ethernet::EthernetHeader>() != nullptr;
        }, "Check if Ethernet header was decoded")
        .def("has_ipv4", [](const DecodeResult& r) {
            return r.get_layer<ipv4::IPv4Header>() != nullptr;
        }, "Check if IPv4 header was decoded")
        .def("has_udp", [](const DecodeResult& r) {
            return r.get_layer<udp::UdpHeader>() != nullptr;
        }, "Check if UDP header was decoded")
        .def("has_tcp", [](const DecodeResult& r) {
            return r.get_layer<tcp::TcpHeader>() != nullptr;
        }, "Check if TCP header was decoded")
        .def("has_someip", [](const DecodeResult& r) {
            return r.get_layer<someip::SomeIpHeader>() != nullptr;
        }, "Check if SOME/IP header was decoded")
        .def("has_someip_sd", [](const DecodeResult& r) {
            return r.get_layer<someip_sd::SomeIpSdHeader>() != nullptr;
        }, "Check if SOME/IP-SD header was decoded")
        .def("has_doip", [](const DecodeResult& r) {
            return r.get_layer<doip::DoIPHeader>() != nullptr;
        }, "Check if DoIP header was decoded")
        
        // Layer accessors (return copies to ensure Python ownership)
        .def("ethernet", [](const DecodeResult& r) 
             -> std::optional<ethernet::EthernetHeader> {
            auto* hdr = r.get_layer<ethernet::EthernetHeader>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get Ethernet header (or None if not present)")
        .def("ipv4", [](const DecodeResult& r) 
             -> std::optional<ipv4::IPv4Header> {
            auto* hdr = r.get_layer<ipv4::IPv4Header>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get IPv4 header (or None if not present)")
        .def("udp", [](const DecodeResult& r) 
             -> std::optional<udp::UdpHeader> {
            auto* hdr = r.get_layer<udp::UdpHeader>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get UDP header (or None if not present)")
        .def("tcp", [](const DecodeResult& r) 
             -> std::optional<tcp::TcpHeader> {
            auto* hdr = r.get_layer<tcp::TcpHeader>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get TCP header (or None if not present)")
        .def("someip", [](const DecodeResult& r) 
             -> std::optional<someip::SomeIpHeader> {
            auto* hdr = r.get_layer<someip::SomeIpHeader>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get SOME/IP header (or None if not present)")
        .def("someip_sd", [](const DecodeResult& r) 
             -> std::optional<someip_sd::SomeIpSdHeader> {
            auto* hdr = r.get_layer<someip_sd::SomeIpSdHeader>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get SOME/IP-SD header (or None if not present)")
        .def("doip", [](const DecodeResult& r) 
             -> std::optional<doip::DoIPHeader> {
            auto* hdr = r.get_layer<doip::DoIPHeader>();
            if (hdr) return *hdr;
            return std::nullopt;
        }, "Get DoIP header (or None if not present)")
        
        // Payload access
        .def_property_readonly("payload", [](const DecodeResult& r) {
            return py::bytes(reinterpret_cast<const char*>(r.payload.data()),
                           r.payload.size());
        }, "Get payload data after all decoded headers")
        .def_property_readonly("payload_size", [](const DecodeResult& r) {
            return r.payload.size();
        }, "Get payload size in bytes")
        
        // Success/error status
        .def_property_readonly("success", &DecodeResult::success,
                              "Check if decoding succeeded")
        .def_property_readonly("error_message", [](const DecodeResult& r) {
            return r.error_message;
        }, "Get error message if decoding failed")
        
        .def("__repr__", [](const DecodeResult& r) {
            std::string layers;
            if (r.get_layer<ethernet::EthernetHeader>()) layers += "Eth/";
            if (r.get_layer<ipv4::IPv4Header>()) layers += "IPv4/";
            if (r.get_layer<udp::UdpHeader>()) layers += "UDP/";
            if (r.get_layer<tcp::TcpHeader>()) layers += "TCP/";
            if (r.get_layer<someip::SomeIpHeader>()) layers += "SOME-IP/";
            if (r.get_layer<someip_sd::SomeIpSdHeader>()) layers += "SD/";
            if (r.get_layer<doip::DoIPHeader>()) layers += "DoIP/";
            if (!layers.empty()) layers.pop_back();  // Remove trailing /
            return "<DecodeResult [" + layers + "] payload=" + 
                   std::to_string(r.payload.size()) + ">";
        })
        .def("__bool__", &DecodeResult::success);

    // =========================================================================
    // ProtocolDispatcher
    // =========================================================================
    py::class_<ProtocolDispatcher>(m, "ProtocolDispatcher",
        R"doc(
            Protocol decoder that dispatches to appropriate decoders based on
            EtherType and IP protocol numbers.
            
            Supports the full decode chain:
            Ethernet -> VLAN? -> IPv4 -> UDP/TCP -> SOME/IP/DoIP
            
            Example:
                >>> dispatcher = wadjet.ProtocolDispatcher()
                >>> result = dispatcher.decode(packet_data)
                >>> if result.has_someip():
                ...     print(f"SOME/IP service: {result.someip().service_id:#x}")
        )doc")
        .def(py::init<>())
        .def("decode", [](ProtocolDispatcher& dispatcher, py::bytes data) {
            std::string_view sv = data;
            std::span<const std::byte> span(
                reinterpret_cast<const std::byte*>(sv.data()),
                sv.size()
            );
            return dispatcher.decode(span);
        }, py::arg("data"), "Decode packet data through the protocol stack")
        .def("decode", [](ProtocolDispatcher& dispatcher, 
                          const wadjet::Packet& packet) {
            return dispatcher.decode(packet.data());
        }, py::arg("packet"), "Decode a Packet through the protocol stack")
        .def("decode", [](ProtocolDispatcher& dispatcher,
                          const wadjet::PacketView& view) {
            return dispatcher.decode(view.data());
        }, py::arg("packet_view"), "Decode a PacketView through the protocol stack");

    // =========================================================================
    // Convenience function
    // =========================================================================
    m.def("decode_packet", [](py::bytes data) {
        std::string_view sv = data;
        std::span<const std::byte> span(
            reinterpret_cast<const std::byte*>(sv.data()),
            sv.size()
        );
        return decode_packet(span);
    }, py::arg("data"),
       R"doc(
           Decode packet data using the default protocol dispatcher.
           
           This is a convenience function equivalent to:
               dispatcher = ProtocolDispatcher()
               result = dispatcher.decode(data)
           
           Example:
               >>> result = wadjet.decode_packet(packet_bytes)
               >>> if result.has_someip():
               ...     print(result.someip().service_id)
       )doc");

    m.def("decode_packet", [](const wadjet::Packet& packet) {
        return decode_packet(packet.data());
    }, py::arg("packet"), "Decode a Packet using the default dispatcher");

    m.def("decode_packet", [](const wadjet::PacketView& view) {
        return decode_packet(view.data());
    }, py::arg("packet_view"), "Decode a PacketView using the default dispatcher");
}

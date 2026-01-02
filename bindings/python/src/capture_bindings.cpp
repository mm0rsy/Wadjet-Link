/// @file capture_bindings.cpp
/// @brief CaptureSession bindings for live packet capture
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "wadjet/io/capture_session.hpp"
#include "wadjet/io/device.hpp"

namespace py = pybind11;
using namespace wadjet::io;

void bind_capture(py::module_& m) {
    // =========================================================================
    // TimestampSource enum
    // =========================================================================
    py::enum_<TimestampSource>(m, "TimestampSource",
        "Timestamp source for packet capture")
        .value("Software", TimestampSource::Software, 
               "Software timestamp (kernel receive time)")
        .value("Hardware", TimestampSource::Hardware, 
               "Hardware timestamp from NIC")
        .value("System", TimestampSource::System, 
               "System hardware clock")
        .value("Auto", TimestampSource::Auto, 
               "Automatically select best available")
        .export_values();

    // =========================================================================
    // CaptureStats
    // =========================================================================
    py::class_<CaptureStats>(m, "CaptureStats",
        "Capture session statistics")
        .def(py::init<>())
        .def_readwrite("packets_received", &CaptureStats::packets_received,
                      "Packets received")
        .def_readwrite("packets_dropped", &CaptureStats::packets_dropped,
                      "Packets dropped (kernel)")
        .def_readwrite("packets_filtered", &CaptureStats::packets_filtered,
                      "Packets filtered out")
        .def_readwrite("bytes_received", &CaptureStats::bytes_received,
                      "Total bytes received")
        .def("__repr__", [](const CaptureStats& stats) {
            return "<CaptureStats received=" + 
                   std::to_string(stats.packets_received) +
                   " dropped=" + std::to_string(stats.packets_dropped) + ">";
        });

    // =========================================================================
    // HardwareTimestampCaps
    // =========================================================================
    py::class_<HardwareTimestampCaps>(m, "HardwareTimestampCaps",
        "Hardware timestamp capabilities")
        .def_readonly("supports_tx_hardware", 
                     &HardwareTimestampCaps::supports_tx_hardware)
        .def_readonly("supports_tx_software", 
                     &HardwareTimestampCaps::supports_tx_software)
        .def_readonly("supports_rx_hardware", 
                     &HardwareTimestampCaps::supports_rx_hardware)
        .def_readonly("supports_rx_software", 
                     &HardwareTimestampCaps::supports_rx_software)
        .def_readonly("supports_raw_hardware", 
                     &HardwareTimestampCaps::supports_raw_hardware);

    // =========================================================================
    // CaptureSessionOptions
    // =========================================================================
    py::class_<CaptureSessionOptions>(m, "CaptureSessionOptions",
        R"doc(
            Options for configuring a capture session.
            
            Example:
                >>> opts = wadjet.CaptureSessionOptions()
                >>> opts.promiscuous = True
                >>> opts.snaplen = 1500
                >>> session = wadjet.CaptureSession.create("eth0", opts)
        )doc")
        .def(py::init<>())
        .def_readwrite("snaplen", &CaptureSessionOptions::snaplen,
                      "Max bytes to capture per packet (default: 65535)")
        .def_readwrite("promiscuous", &CaptureSessionOptions::promiscuous,
                      "Enable promiscuous mode (default: True)")
        .def_readwrite("immediate_mode", &CaptureSessionOptions::immediate_mode,
                      "Minimize latency (default: True)")
        .def_readwrite("buffer_size", &CaptureSessionOptions::buffer_size,
                      "Ring buffer size (default: 2MB)")
        .def_readwrite("timeout_ms", &CaptureSessionOptions::timeout_ms,
                      "Poll timeout in milliseconds (default: 100)")
        .def_readwrite("timestamp_source", &CaptureSessionOptions::timestamp_source,
                      "Timestamp source (default: Auto)")
        .def_readwrite("use_tpacket_v3", &CaptureSessionOptions::use_tpacket_v3,
                      "Use TPACKET_V3 for better performance")
        .def_readwrite("use_ring_buffer", &CaptureSessionOptions::use_ring_buffer,
                      "Use mmap ring buffer (default: True)")
        .def("__repr__", [](const CaptureSessionOptions& opts) {
            return "<CaptureSessionOptions snaplen=" + 
                   std::to_string(opts.snaplen) +
                   " promiscuous=" + (opts.promiscuous ? "True" : "False") + ">";
        });

    // =========================================================================
    // CaptureSession
    // =========================================================================
    py::class_<CaptureSession>(m, "CaptureSession",
        R"doc(
            Live packet capture session using Linux AF_PACKET.
            
            CaptureSession provides high-performance packet capture with
            optional ring buffer support and hardware timestamping.
            
            Example:
                >>> session = wadjet.CaptureSession.create("eth0")
                >>> if session:
                ...     session.set_filter("udp port 30490")
                ...     session.start()
                ...     while packet := session.next_packet(timeout_ms=1000):
                ...         process(packet)
                ...     session.stop()
            
            For easier usage, use the LiveCapture context manager:
                >>> with wadjet.LiveCapture("eth0") as cap:
                ...     for packet in cap.stream(timeout_ms=1000):
                ...         process(packet)
        )doc")
        .def_static("create", [](const std::string& interface,
                                  CaptureSessionOptions options) -> CaptureSession {
            auto result = CaptureSession::create(interface, options);
            if (!result) {
                throw std::runtime_error("Failed to create capture session: " +
                                        result.error().message);
            }
            return std::move(*result);
        }, py::arg("interface"), 
           py::arg("options") = CaptureSessionOptions{},
           "Create a capture session on the specified interface")
        .def("set_filter", [](CaptureSession& session, std::string_view expr) -> void {
            auto result = session.set_filter(expr);
            if (!result) {
                throw std::runtime_error("Failed to set filter: " +
                                        result.error().message);
            }
        }, py::arg("expression"),
           "Set a BPF filter (e.g., 'udp port 30490')")
        .def("start", [](CaptureSession& session) -> void {
            auto result = session.start();
            if (!result) {
                throw std::runtime_error("Failed to start capture: " +
                                        result.error().message);
            }
        }, "Start capturing (non-blocking)")
        .def("stop", &CaptureSession::stop, "Stop capturing")
        .def("is_running", &CaptureSession::is_running, 
             "Check if capture is running")
        .def("next_packet", [](CaptureSession& session, int timeout_ms) {
            return session.next_packet(std::chrono::milliseconds(timeout_ms));
        }, py::arg("timeout_ms") = 1000,
           "Capture the next packet (blocking with timeout)")
        .def("stats", &CaptureSession::stats, "Get capture statistics")
        .def_property_readonly("interface_name", &CaptureSession::interface_name,
                              "Get the interface name")
        .def_property_readonly("fd", &CaptureSession::fd,
                              "Get the socket file descriptor")
        .def_property_readonly("active_timestamp_source", 
                              &CaptureSession::active_timestamp_source,
                              "Get the active timestamp source")
        .def_static("query_hw_timestamp_caps", [](const std::string& interface) -> HardwareTimestampCaps {
            auto result = CaptureSession::query_hw_timestamp_caps(interface);
            if (!result) {
                throw std::runtime_error("Failed to query capabilities: " +
                                        result.error().message);
            }
            return *result;
        }, py::arg("interface"),
           "Query hardware timestamp capabilities for an interface")
        // Iterator support
        .def("__iter__", [](CaptureSession& session) {
            return &session;
        })
        .def("__next__", [](CaptureSession& session) {
            auto pkt = session.next_packet(std::chrono::milliseconds(100));
            if (!pkt) {
                throw py::stop_iteration();
            }
            return *pkt;
        });

    // =========================================================================
    // Device enumeration - use enumerate_devices() and get_device()
    // =========================================================================
    m.def("list_interfaces", []() -> std::vector<NetworkDevice> {
        auto result = enumerate_devices();
        if (!result) {
            throw std::runtime_error("Failed to enumerate devices: " +
                                    result.error().message);
        }
        return std::move(*result);
    }, "List available network interfaces");

    m.def("get_interface_info", [](const std::string& name) -> NetworkDevice {
        auto result = get_device(name);
        if (!result) {
            throw std::runtime_error("Failed to get interface info: " +
                                    result.error().message);
        }
        return std::move(*result);
    }, py::arg("name"), "Get information about a network interface");

    // =========================================================================
    // NetworkDevice (the actual struct name in device.hpp)
    // =========================================================================
    py::class_<NetworkDevice>(m, "InterfaceInfo",
        "Information about a network interface")
        .def_readonly("name", &NetworkDevice::name)
        .def_readonly("description", &NetworkDevice::description)
        .def_property_readonly("mac_address", [](const NetworkDevice& dev) {
            return dev.mac.to_string();
        })
        .def_readonly("mac", &NetworkDevice::mac)
        .def_readonly("index", &NetworkDevice::index)
        .def_readonly("mtu", &NetworkDevice::mtu)
        .def_readonly("is_up", &NetworkDevice::is_up)
        .def_readonly("is_running", &NetworkDevice::is_running)
        .def_readonly("is_loopback", &NetworkDevice::is_loopback)
        .def_readonly("supports_promiscuous", &NetworkDevice::supports_promiscuous)
        .def("__repr__", [](const NetworkDevice& info) {
            return "<InterfaceInfo " + info.name + 
                   (info.is_up ? " UP" : " DOWN") + ">";
        });
}

/// @file pcap_bindings.cpp
/// @brief PcapReader and PcapWriter bindings
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "wadjet/pcap/pcap_reader.hpp"
#include "wadjet/pcap/pcap_writer.hpp"

namespace py = pybind11;
using namespace wadjet::pcap;

void bind_pcap(py::module_& m) {
    // =========================================================================
    // PcapReader
    // =========================================================================
    py::class_<PcapReader>(m, "PcapReader",
        R"doc(
            Read packets from a PCAP file.
            
            Example:
                >>> with wadjet.PcapReader.open("capture.pcap") as reader:
                ...     for packet in reader:
                ...         print(f"Packet: {len(packet)} bytes")
            
            Or using the high-level wrapper:
                >>> packets = wadjet.read_pcap("capture.pcap")
        )doc")
        .def_static("open", [](const std::string& path) -> PcapReader {
            auto result = PcapReader::open(path);
            if (!result) {
                throw std::runtime_error("Failed to open PCAP file: " +
                                        result.error().message);
            }
            return std::move(*result);
        }, py::arg("path"), "Open a PCAP file for reading")
        .def("next_packet", &PcapReader::next_packet,
             "Read the next packet from the file")
        .def("has_more", &PcapReader::has_more,
             "Check if more packets are available")
        .def_property_readonly("link_type", &PcapReader::link_type,
                              "Get the link layer type")
        .def_property_readonly("snaplen", &PcapReader::snaplen,
                              "Get the snapshot length")
        // Use description() instead of path()
        .def_property_readonly("path", [](const PcapReader& reader) {
            return reader.description();
        }, "Get the file path/description")
        // Iterator support
        .def("__iter__", [](PcapReader& reader) {
            return &reader;
        })
        .def("__next__", [](PcapReader& reader) {
            auto pkt = reader.next_packet();
            if (!pkt) {
                throw py::stop_iteration();
            }
            return *pkt;
        })
        // Context manager support
        .def("__enter__", [](PcapReader& reader) -> PcapReader& {
            return reader;
        })
        .def("__exit__", [](PcapReader& /*reader*/, 
                           py::object /*exc_type*/,
                           py::object /*exc_val*/,
                           py::object /*exc_tb*/) {
            // PcapReader closes automatically in destructor
            return false;  // Don't suppress exceptions
        })
        .def("__repr__", [](const PcapReader& reader) {
            return "<PcapReader '" + reader.description() + "'>";
        });

    // =========================================================================
    // PcapWriter
    // =========================================================================
    py::class_<PcapWriter>(m, "PcapWriter",
        R"doc(
            Write packets to a PCAP file.
            
            Example:
                >>> with wadjet.PcapWriter.create("output.pcap") as writer:
                ...     for packet in packets:
                ...         writer.write_packet(packet)
            
            Or using the high-level wrapper:
                >>> wadjet.write_pcap("output.pcap", packets)
        )doc")
        // PcapWriter::create takes 2 args: path and Options struct
        .def_static("create", [](const std::string& path,
                                 std::uint32_t link_type,
                                 std::uint32_t snaplen) -> PcapWriter {
            PcapWriterOptions options;
            options.link_type = static_cast<LinkType>(link_type);
            options.snaplen = snaplen;
            auto result = PcapWriter::create(path, options);
            if (!result) {
                throw std::runtime_error("Failed to create PCAP file: " +
                                        result.error().message);
            }
            return std::move(*result);
        }, py::arg("path"), 
           py::arg("link_type") = 1,  // DLT_EN10MB (Ethernet)
           py::arg("snaplen") = 65535,
           "Create a PCAP file for writing")
        .def("write_packet", [](PcapWriter& writer, const wadjet::Packet& packet) -> void {
            auto result = writer.write_packet(packet);
            if (!result) {
                throw std::runtime_error("Failed to write packet: " +
                                        result.error().message);
            }
        }, py::arg("packet"), "Write a packet to the file")
        .def("write_packet", [](PcapWriter& writer, 
                                const wadjet::PacketView& view) -> void {
            auto result = writer.write_packet(view);
            if (!result) {
                throw std::runtime_error("Failed to write packet: " +
                                        result.error().message);
            }
        }, py::arg("packet_view"), "Write a packet view to the file")
        .def("flush", &PcapWriter::flush, "Flush buffered data to disk")
        // Use packet_count() instead of packets_written()
        .def_property_readonly("packets_written", &PcapWriter::packet_count,
                              "Get number of packets written")
        .def_property_readonly("bytes_written", &PcapWriter::bytes_written,
                              "Get number of bytes written")
        // Use description() instead of path()
        .def_property_readonly("path", [](const PcapWriter& writer) {
            return writer.description();
        }, "Get the file path/description")
        // Context manager support
        .def("__enter__", [](PcapWriter& writer) -> PcapWriter& {
            return writer;
        })
        .def("__exit__", [](PcapWriter& writer,
                           py::object /*exc_type*/,
                           py::object /*exc_val*/,
                           py::object /*exc_tb*/) {
            writer.flush();
            return false;  // Don't suppress exceptions
        })
        .def("__repr__", [](const PcapWriter& writer) {
            return "<PcapWriter '" + writer.description() + 
                   "' packets=" + std::to_string(writer.packet_count()) + ">";
        });
}

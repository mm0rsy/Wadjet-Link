/// @file packet_bindings.cpp
/// @brief Packet and PacketView bindings
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/buffer_info.h>

#include "wadjet/net/packet.hpp"
#include "wadjet/net/packet_view.hpp"

namespace py = pybind11;

void bind_packet(py::module_& m) {
    // =========================================================================
    // PacketView (immutable, zero-copy view)
    // =========================================================================
    py::class_<wadjet::PacketView>(m, "PacketView", py::buffer_protocol(),
        R"doc(
            Immutable zero-copy view into packet data.
            
            PacketView provides efficient read-only access to packet data
            without copying. It implements the buffer protocol for seamless
            integration with NumPy and other Python libraries.
            
            Example:
                >>> view = packet.view()
                >>> data = bytes(view)  # Copy to Python bytes
                >>> arr = np.frombuffer(view, dtype=np.uint8)  # Zero-copy NumPy
        )doc")
        .def_buffer([](wadjet::PacketView& view) -> py::buffer_info {
            return py::buffer_info(
                const_cast<std::byte*>(view.data().data()),
                sizeof(std::byte),
                py::format_descriptor<std::uint8_t>::format(),
                1,
                {view.size()},
                {sizeof(std::byte)}
            );
        })
        .def_property_readonly("size", &wadjet::PacketView::size,
                              "Get packet size in bytes")
        .def_property_readonly("timestamp", &wadjet::PacketView::timestamp,
                              "Get packet timestamp")
        .def("__len__", &wadjet::PacketView::size)
        .def("__bytes__", [](const wadjet::PacketView& view) {
            auto data = view.data();
            return py::bytes(reinterpret_cast<const char*>(data.data()), 
                           data.size());
        })
        .def("__getitem__", [](const wadjet::PacketView& view, std::size_t index) {
            if (index >= view.size()) {
                throw py::index_error("Packet index out of range");
            }
            return static_cast<std::uint8_t>(view.data()[index]);
        })
        .def("__getitem__", [](const wadjet::PacketView& view, py::slice slice) {
            std::size_t start, stop, step, slicelength;
            if (!slice.compute(view.size(), &start, &stop, &step, &slicelength)) {
                throw py::error_already_set();
            }
            
            std::string result;
            result.reserve(slicelength);
            for (std::size_t i = 0; i < slicelength; ++i) {
                result += static_cast<char>(view.data()[start + i * step]);
            }
            return py::bytes(result);
        })
        .def("hex", [](const wadjet::PacketView& view) {
            std::string hex;
            auto data = view.data();
            for (std::size_t i = 0; i < data.size(); ++i) {
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02x", 
                             static_cast<unsigned char>(data[i]));
                hex += buf;
            }
            return hex;
        }, "Get hexadecimal representation of packet data")
        .def("__repr__", [](const wadjet::PacketView& view) {
            return "<PacketView size=" + std::to_string(view.size()) + ">";
        });

    // =========================================================================
    // Packet (mutable, owning)
    // =========================================================================
    py::class_<wadjet::Packet>(m, "Packet", py::buffer_protocol(),
        R"doc(
            Mutable packet buffer that owns its data.
            
            Packet provides an owning container for packet data, useful for:
            - Constructing packets for transmission
            - Storing captured packets
            - Modifying packet contents
            
            Example:
                >>> pkt = wadjet.Packet(b'\x00\x01\x02\x03')
                >>> pkt.append(b'\x04\x05')
                >>> view = pkt.view()
        )doc")
        .def(py::init<>(), "Create an empty packet")
        .def(py::init<std::size_t>(), py::arg("capacity"),
             "Create packet with reserved capacity")
        .def(py::init([](py::bytes data) {
            std::string_view sv = data;
            return wadjet::Packet(
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(sv.data()),
                    sv.size()
                )
            );
        }), py::arg("data"), "Create packet from bytes")
        .def(py::init([](py::bytes data, wadjet::Timestamp ts) {
            std::string_view sv = data;
            return wadjet::Packet(
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(sv.data()),
                    sv.size()
                ),
                ts
            );
        }), py::arg("data"), py::arg("timestamp"),
            "Create packet from bytes with timestamp")
        .def_buffer([](wadjet::Packet& pkt) -> py::buffer_info {
            return py::buffer_info(
                pkt.begin(),
                sizeof(std::byte),
                py::format_descriptor<std::uint8_t>::format(),
                1,
                {pkt.size()},
                {sizeof(std::byte)},
                false  // not readonly
            );
        })
        .def_property_readonly("size", &wadjet::Packet::size,
                              "Get packet size in bytes")
        .def_property_readonly("capacity", &wadjet::Packet::capacity,
                              "Get buffer capacity")
        .def_property_readonly("empty", &wadjet::Packet::empty,
                              "Check if packet is empty")
        .def_property("timestamp", &wadjet::Packet::timestamp, 
                     &wadjet::Packet::set_timestamp,
                     "Packet timestamp")
        .def("view", &wadjet::Packet::view,
             "Create an immutable view of this packet")
        .def("resize", &wadjet::Packet::resize, py::arg("new_size"),
             "Resize packet data")
        .def("reserve", &wadjet::Packet::reserve, py::arg("capacity"),
             "Reserve buffer capacity")
        .def("clear", &wadjet::Packet::clear,
             "Clear packet data")
        .def("append", [](wadjet::Packet& pkt, py::bytes data) {
            std::string_view sv = data;
            std::span<const std::byte> span(
                reinterpret_cast<const std::byte*>(sv.data()),
                sv.size()
            );
            pkt.append(span);
        }, py::arg("data"), "Append data to packet")
        .def("__len__", &wadjet::Packet::size)
        .def("__bytes__", [](const wadjet::Packet& pkt) {
            auto data = pkt.data();
            return py::bytes(reinterpret_cast<const char*>(data.data()), 
                           data.size());
        })
        .def("__getitem__", [](const wadjet::Packet& pkt, std::size_t index) {
            if (index >= pkt.size()) {
                throw py::index_error("Packet index out of range");
            }
            return static_cast<std::uint8_t>(pkt[index]);
        })
        .def("__setitem__", [](wadjet::Packet& pkt, std::size_t index, 
                               std::uint8_t value) {
            if (index >= pkt.size()) {
                throw py::index_error("Packet index out of range");
            }
            pkt[index] = static_cast<std::byte>(value);
        })
        .def("hex", [](const wadjet::Packet& pkt) {
            std::string hex;
            auto data = pkt.data();
            for (std::size_t i = 0; i < data.size(); ++i) {
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02x", 
                             static_cast<unsigned char>(data[i]));
                hex += buf;
            }
            return hex;
        }, "Get hexadecimal representation of packet data")
        .def("__repr__", [](const wadjet::Packet& pkt) {
            return "<Packet size=" + std::to_string(pkt.size()) + 
                   " capacity=" + std::to_string(pkt.capacity()) + ">";
        });
}

/// @file core_bindings.cpp
/// @brief Core type bindings (Timestamp, ByteSpan, etc.)
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

#include "wadjet/core/timestamp.hpp"
#include "wadjet/core/types.hpp"

namespace py = pybind11;

void bind_core(py::module_& m) {
    // =========================================================================
    // Timestamp
    // =========================================================================
    py::class_<wadjet::Timestamp>(m, "Timestamp",
        R"doc(
            High-resolution timestamp for packet capture.
            
            Supports nanosecond precision when hardware timestamping is available.
        )doc")
        .def(py::init<>(), "Create a zero timestamp")
        .def(py::init<std::int64_t, std::int64_t>(),
             py::arg("seconds"), py::arg("nanoseconds"),
             "Create timestamp from seconds and nanoseconds")
        .def_property_readonly("seconds", &wadjet::Timestamp::seconds,
                              "Get seconds component")
        .def_property_readonly("nanoseconds", &wadjet::Timestamp::nanoseconds,
                              "Get nanoseconds component")
        .def_property_readonly("total_nanoseconds", &wadjet::Timestamp::total_nanoseconds,
                              "Get total time in nanoseconds")
        .def("to_duration", [](const wadjet::Timestamp& ts) {
            return std::chrono::nanoseconds(ts.total_nanoseconds());
        }, "Convert to Python timedelta")
        .def("__repr__", [](const wadjet::Timestamp& ts) {
            return "<Timestamp " + std::to_string(ts.seconds()) + "s " +
                   std::to_string(ts.nanoseconds()) + "ns>";
        })
        .def("__eq__", [](const wadjet::Timestamp& a, const wadjet::Timestamp& b) {
            return a == b;
        })
        .def("__lt__", [](const wadjet::Timestamp& a, const wadjet::Timestamp& b) {
            return a < b;
        })
        .def("__le__", [](const wadjet::Timestamp& a, const wadjet::Timestamp& b) {
            return a <= b;
        })
        .def("__gt__", [](const wadjet::Timestamp& a, const wadjet::Timestamp& b) {
            return a > b;
        })
        .def("__ge__", [](const wadjet::Timestamp& a, const wadjet::Timestamp& b) {
            return a >= b;
        })
        .def_static("now", &wadjet::Timestamp::now,
                   "Get current timestamp");

    // =========================================================================
    // ByteSpan (exposed as bytes in Python)
    // =========================================================================
    // ByteSpan is typically returned as Python bytes objects via buffer protocol
    // We expose a helper class for explicit conversions
    
    m.def("bytes_to_hex", [](py::bytes data) {
        std::string hex;
        std::string_view sv = data;
        for (char c : sv) {
            char buf[3];
            std::snprintf(buf, sizeof(buf), "%02x", 
                         static_cast<unsigned char>(c));
            hex += buf;
        }
        return hex;
    }, py::arg("data"),
       "Convert bytes to hexadecimal string");

    m.def("hex_to_bytes", [](const std::string& hex) {
        std::string result;
        for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
            auto byte = std::stoul(hex.substr(i, 2), nullptr, 16);
            result += static_cast<char>(byte);
        }
        return py::bytes(result);
    }, py::arg("hex_string"),
       "Convert hexadecimal string to bytes");
}

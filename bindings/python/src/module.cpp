/// @file module.cpp
/// @brief Main pybind11 module definition
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "wadjet/version.hpp"

namespace py = pybind11;

// Forward declarations for submodule binding functions
void bind_core(py::module_& m);
void bind_packet(py::module_& m);
void bind_capture(py::module_& m);
void bind_pcap(py::module_& m);
void bind_protocols(py::module_& m);
void bind_decoders(py::module_& m);

PYBIND11_MODULE(_wadjet, m) {
    m.doc() = R"doc(
        𓆓 Wadjet-Link Python Bindings
        
        Automotive Ethernet validation framework for Python.
        
        This module provides low-level bindings to the C++ library.
        For high-level usage, import `wadjet` instead.
    )doc";

    // Version information
    m.def("version_string", &wadjet::version_string,
          "Get the full version string");
    m.def("version_major", []() { return wadjet::VERSION_MAJOR; },
          "Get the major version number");
    m.def("version_minor", []() { return wadjet::VERSION_MINOR; },
          "Get the minor version number");
    m.def("version_patch", []() { return wadjet::VERSION_PATCH; },
          "Get the patch version number");
    
    m.attr("__version__") = wadjet::version_string();

    // Bind all submodules
    bind_core(m);
    bind_packet(m);
    bind_capture(m);
    bind_pcap(m);
    bind_protocols(m);
    bind_decoders(m);
}

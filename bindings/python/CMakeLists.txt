# ============================================================================
# Wadjet Python Bindings
# ============================================================================
#
# 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
#
# Python bindings using pybind11 for fast adoption, Jupyter analysis, and 
# pytest integration.

cmake_minimum_required(VERSION 3.20)

# Find Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Try to find pybind11 - first via CMake config, then via Python module
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    # Try to find pybind11 via Python
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
        OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE PYBIND11_PYTHON_RESULT
    )
    if(PYBIND11_PYTHON_RESULT EQUAL 0 AND EXISTS "${PYBIND11_CMAKE_DIR}")
        list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_CMAKE_DIR}")
        find_package(pybind11 CONFIG REQUIRED)
    else()
        message(FATAL_ERROR 
            "pybind11 not found. Install it via:\n"
            "  pip install pybind11\n"
            "  OR\n"
            "  apt install pybind11-dev (Debian/Ubuntu)\n"
            "  brew install pybind11 (macOS)")
    endif()
endif()

# ============================================================================
# Python Extension Module
# ============================================================================
pybind11_add_module(_wadjet
    src/module.cpp
    src/core_bindings.cpp
    src/packet_bindings.cpp
    src/capture_bindings.cpp
    src/pcap_bindings.cpp
    src/protocol_bindings.cpp
    src/decoder_bindings.cpp
)

target_link_libraries(_wadjet
    PRIVATE
        wadjet
)

target_include_directories(_wadjet
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set output directory to match Python package structure
set_target_properties(_wadjet PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/wadjet"
)

# ============================================================================
# Python Package Files
# ============================================================================
# Copy Python package files to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wadjet/__init__.py
    ${CMAKE_CURRENT_BINARY_DIR}/wadjet/__init__.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wadjet/capture.py
    ${CMAKE_CURRENT_BINARY_DIR}/wadjet/capture.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wadjet/protocols.py
    ${CMAKE_CURRENT_BINARY_DIR}/wadjet/protocols.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wadjet/testing.py
    ${CMAKE_CURRENT_BINARY_DIR}/wadjet/testing.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/wadjet/pcap.py
    ${CMAKE_CURRENT_BINARY_DIR}/wadjet/pcap.py
    COPYONLY
)

# ============================================================================
# Install
# ============================================================================
install(TARGETS _wadjet
    LIBRARY DESTINATION ${Python3_SITEARCH}/wadjet
)

install(DIRECTORY wadjet/
    DESTINATION ${Python3_SITEARCH}/wadjet
    FILES_MATCHING PATTERN "*.py"
)

# ============================================================================
# Tests
# ============================================================================
if(WADJET_BUILD_TESTS)
    # Add pytest as a test
    add_test(
        NAME python_tests
        COMMAND ${Python3_EXECUTABLE} -m pytest 
                ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    # Set PYTHONPATH so pytest can find the module
    set_tests_properties(python_tests PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

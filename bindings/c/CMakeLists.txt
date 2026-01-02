# ============================================================================
# Wadjet C Bindings
# ============================================================================
#
# 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
#
# C bindings providing FFI compatibility for Rust, Python ctypes, and other
# languages that can interface with C libraries.

cmake_minimum_required(VERSION 3.16)

# Include GNUInstallDirs for standard install paths
include(GNUInstallDirs)

# C shared library
add_library(wadjet_c SHARED
    src/wadjet_c.cpp
)

target_include_directories(wadjet_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wadjet_c
    PRIVATE wadjet
)

target_compile_features(wadjet_c PRIVATE cxx_std_20)

# Set shared library properties
set_target_properties(wadjet_c PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "include/wadjet_c.h"
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Also build a static library for Rust linking
add_library(wadjet_c_static STATIC
    src/wadjet_c.cpp
)

target_include_directories(wadjet_c_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wadjet_c_static
    PRIVATE wadjet
)

target_compile_features(wadjet_c_static PRIVATE cxx_std_20)

set_target_properties(wadjet_c_static PROPERTIES
    OUTPUT_NAME wadjet_c
    POSITION_INDEPENDENT_CODE ON
)

# Install targets
install(TARGETS wadjet_c wadjet_c_static
    EXPORT wadjet_c-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install header
install(FILES include/wadjet_c.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT wadjet_c-targets
    FILE wadjet_c-targets.cmake
    NAMESPACE wadjet::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wadjet_c
)

# Build examples if requested
option(WADJET_BUILD_C_EXAMPLES "Build C bindings examples" ON)
if(WADJET_BUILD_C_EXAMPLES)
    add_subdirectory(examples)
endif()

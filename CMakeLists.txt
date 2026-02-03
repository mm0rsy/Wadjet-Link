cmake_minimum_required(VERSION 3.20)

project(wadjet-link
    VERSION 0.1.0
    DESCRIPTION "Automotive Ethernet validation framework"
    LANGUAGES CXX
)

# ============================================================================
# Project Options
# ============================================================================
option(WADJET_BUILD_TESTS "Build unit tests" ON)
option(WADJET_BUILD_EXAMPLES "Build examples" ON)
option(WADJET_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(WADJET_BUILD_DOCS "Build documentation" OFF)
option(WADJET_BUILD_FUZZ "Build fuzz tests (requires Clang with libFuzzer)" OFF)
option(WADJET_BUILD_C_BINDINGS "Build C bindings (for FFI with Rust, Python, etc.)" OFF)
option(WADJET_BUILD_PYTHON_BINDINGS "Build Python bindings (requires pybind11)" OFF)
option(WADJET_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug builds" ON)
option(WADJET_ENABLE_DISTRIBUTED "Enable distributed testing infrastructure" ON)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Distributed Testing Dependencies (gRPC + Protobuf)
# ============================================================================
if(WADJET_ENABLE_DISTRIBUTED)
    include(FetchContent)
    
    # Fetch gRPC and its dependencies
    FetchContent_Declare(grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG        v1.50.0
        CMAKE_ARGS -DgRPC_BUILD_TESTS=OFF
    )
    FetchContent_MakeAvailable(grpc)
endif()

# ============================================================================
# Compiler Warnings
# ============================================================================
add_library(wadjet_warnings INTERFACE)
target_compile_options(wadjet_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wcast-qual
        -Wformat=2
        -Wundef
        -Wshadow
        -Wcast-align
        -Wunused
        -Wnull-dereference
        -Wdouble-promotion
    >
    $<$<CXX_COMPILER_ID:GNU>:
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
    >
)

# ============================================================================
# Sanitizers (Debug builds only)
# ============================================================================
add_library(wadjet_sanitizers INTERFACE)
if(WADJET_ENABLE_SANITIZERS)
    target_compile_options(wadjet_sanitizers INTERFACE
        $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        >
    )
    target_link_options(wadjet_sanitizers INTERFACE
        $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:
            -fsanitize=address,undefined
        >
    )
endif()

# ============================================================================
# Main Library
# ============================================================================
add_subdirectory(src)

# ============================================================================
# CLI Tools
# ============================================================================
add_subdirectory(tools)

# ============================================================================
# Tests
# ============================================================================
if(WADJET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Examples
# ============================================================================
if(WADJET_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(WADJET_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Fuzz Tests
# ============================================================================
if(WADJET_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif()

# ============================================================================
# Documentation
# ============================================================================
if(WADJET_BUILD_DOCS)
    add_subdirectory(docs)
endif()

# ============================================================================
# C Bindings
# ============================================================================
if(WADJET_BUILD_C_BINDINGS)
    add_subdirectory(bindings/c)
endif()

# ============================================================================
# Python Bindings
# ============================================================================
if(WADJET_BUILD_PYTHON_BINDINGS)
    add_subdirectory(bindings/python)
endif()

# ============================================================================
# Install Configuration
# ============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS wadjet wadjet_warnings wadjet_sanitizers
    EXPORT WadjetTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/wadjet
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT WadjetTargets
    FILE WadjetTargets.cmake
    NAMESPACE Wadjet::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wadjet
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WadjetConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/WadjetConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wadjet
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/WadjetConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/WadjetConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/WadjetConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wadjet
)

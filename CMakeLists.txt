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
option(WADJET_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug builds" ON)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
# Documentation
# ============================================================================
if(WADJET_BUILD_DOCS)
    add_subdirectory(docs)
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

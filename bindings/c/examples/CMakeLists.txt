# ============================================================================
# Wadjet C Bindings Examples
# ============================================================================
#
# 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
#
# Build examples demonstrating C bindings usage.

# Enable C language for this subdirectory
enable_language(C)

# Example: PCAP capture and protocol decoding
add_executable(c_capture_example capture_example.c)
target_link_libraries(c_capture_example PRIVATE wadjet_c_static)
target_include_directories(c_capture_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Example: List network devices
add_executable(c_list_devices list_devices.c)
target_link_libraries(c_list_devices PRIVATE wadjet_c_static)
target_include_directories(c_list_devices PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Set output directory
set_target_properties(c_capture_example c_list_devices
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/c_examples
)

# Don't install examples by default, but allow it
if(WADJET_INSTALL_EXAMPLES)
    install(TARGETS c_capture_example c_list_devices
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    )
endif()

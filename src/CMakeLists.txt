# ============================================================================
# Wadjet Core Library
# ============================================================================

# Find libpcap
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)

# Find yaml-cpp
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    pkg_check_modules(YAML_CPP yaml-cpp)
endif()

# Find nlohmann_json
find_package(nlohmann_json QUIET)

# Check if scenario dependencies are available
set(WADJET_SCENARIO_ENABLED OFF PARENT_SCOPE)
set(WADJET_SCENARIO_ENABLED OFF)
if((yaml-cpp_FOUND OR YAML_CPP_FOUND) AND nlohmann_json_FOUND)
    set(WADJET_SCENARIO_ENABLED ON PARENT_SCOPE)
    set(WADJET_SCENARIO_ENABLED ON)
    message(STATUS "Scenario module: ENABLED (yaml-cpp and nlohmann_json found)")
else()
    message(STATUS "Scenario module: DISABLED (install yaml-cpp and nlohmann_json to enable)")
endif()

add_library(wadjet
    # Core modules
    version.cpp
    core/types.cpp
    core/timestamp.cpp
    net/packet_view.cpp
    
    # PCAP I/O
    pcap/pcap_reader.cpp
    pcap/pcap_writer.cpp
    pcap/pcapng_writer.cpp
    
    # Live capture
    io/device.cpp
    io/frame_filter.cpp
    io/capture_session.cpp
    io/replay_session.cpp
    io/pcap_capture_session.cpp
    
    # Protocol decoders
    protocols/ethernet.cpp
    protocols/ipv4.cpp
    protocols/ipv4_reassembler.cpp
    protocols/udp.cpp
    protocols/tcp.cpp
    protocols/someip.cpp
    protocols/someip_sd.cpp
    protocols/doip.cpp
    protocols/gptp_decoder.cpp
    protocols/dds/rtps_decoder.cpp
    protocols/dispatcher.cpp
    
    # Common protocol utilities
    protocols/common/timeout_manager.cpp
    protocols/common/checksum.cpp
    
    # UDS protocol
    protocols/uds_decoder.cpp
    protocols/uds_session.cpp
    
    # Diagnostic integration (UDS over DoIP)
    protocols/diagnostic/diagnostic_session.cpp
    protocols/diagnostic/dtc_manager.cpp
    protocols/diagnostic/flash_sequence.cpp
    protocols/diagnostic/request_correlator.cpp
    protocols/diagnostic/uds_doip_decoder.cpp
)

# Add scenario module sources if dependencies are available
if(WADJET_SCENARIO_ENABLED)
    target_sources(wadjet PRIVATE
        scenario/yaml_parser.cpp
        scenario/json_parser.cpp
        scenario/runner.cpp
        scenario/report.cpp
    )
    target_compile_definitions(wadjet PUBLIC WADJET_SCENARIO_ENABLED=1)
endif()

target_include_directories(wadjet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PCAP_INCLUDE_DIRS}
)

# Enable PIC for shared library linking (e.g., C bindings shared library)
set_target_properties(wadjet PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(wadjet
    PRIVATE
        wadjet_warnings
        wadjet_sanitizers
        ${PCAP_LIBRARIES}
)

# Link scenario dependencies
if(WADJET_SCENARIO_ENABLED)
    if(yaml-cpp_FOUND)
        target_link_libraries(wadjet PRIVATE yaml-cpp)
    else()
        target_include_directories(wadjet PRIVATE ${YAML_CPP_INCLUDE_DIRS})
        target_link_libraries(wadjet PRIVATE ${YAML_CPP_LIBRARIES})
    endif()
    target_link_libraries(wadjet PRIVATE nlohmann_json::nlohmann_json)
endif()

# Future: Add subdirectories as they are implemented
# add_subdirectory(testing)

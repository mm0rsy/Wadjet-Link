# ============================================================================
# Wadjet Core Library
# ============================================================================

# Find libpcap
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)

add_library(wadjet
    # Core modules
    version.cpp
    core/types.cpp
    core/timestamp.cpp
    net/packet_view.cpp
    
    # PCAP I/O
    pcap/pcap_reader.cpp
    pcap/pcap_writer.cpp
    
    # Live capture
    io/device.cpp
    io/frame_filter.cpp
    io/capture_session.cpp
    
    # Protocol decoders
    protocols/ethernet.cpp
    protocols/ipv4.cpp
    protocols/udp.cpp
    protocols/tcp.cpp
    protocols/someip.cpp
    protocols/someip_sd.cpp
    protocols/doip.cpp
    protocols/dispatcher.cpp
)

target_include_directories(wadjet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PCAP_INCLUDE_DIRS}
)

target_link_libraries(wadjet
    PRIVATE
        wadjet_warnings
        wadjet_sanitizers
        ${PCAP_LIBRARIES}
)

# Future: Add subdirectories as they are implemented
# add_subdirectory(testing)

# ============================================================================
# Wadjet Core Library
# ============================================================================

add_library(wadjet
    version.cpp
)

target_include_directories(wadjet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wadjet
    PRIVATE
        wadjet_warnings
        wadjet_sanitizers
)

# Future: Add subdirectories as they are implemented
# add_subdirectory(io)
# add_subdirectory(pcap)
# add_subdirectory(net)
# add_subdirectory(protocols)
# add_subdirectory(testing)
# add_subdirectory(utils)

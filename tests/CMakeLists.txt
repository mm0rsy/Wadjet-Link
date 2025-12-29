# ============================================================================
# Wadjet Tests
# ============================================================================

include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# Prevent GoogleTest from overriding compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ============================================================================
# Test executable
# ============================================================================
add_executable(wadjet_tests
    test_version.cpp
    # Core tests
    core/test_types.cpp
    core/test_result.cpp
    core/test_byte_order.cpp
    core/test_timestamp.cpp
    # Network tests
    net/test_packet.cpp
    # PCAP tests
    pcap/test_pcap.cpp
    # I/O tests
    io/test_device.cpp
    io/test_frame_filter.cpp
)

target_link_libraries(wadjet_tests
    PRIVATE
        wadjet
        wadjet_warnings
        wadjet_sanitizers
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(wadjet_tests)

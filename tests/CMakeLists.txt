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
# Test executable (Unit Tests)
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
    pcap/test_pcapng.cpp
    # I/O tests
    io/test_device.cpp
    io/test_frame_filter.cpp
    io/test_replay_session.cpp
    # Protocol tests
    protocols/test_decoders.cpp
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

# ============================================================================
# Integration Tests executable
# ============================================================================
add_executable(wadjet_integration_tests
    # Pipeline integration tests
    integration/test_decode_pipeline.cpp
    # PCAP decode tests
    integration/test_pcap_decode.cpp
    # Capture session tests (requires CAP_NET_RAW)
    integration/test_capture_session.cpp
)

target_link_libraries(wadjet_integration_tests
    PRIVATE
        wadjet
        wadjet_warnings
        wadjet_sanitizers
        GTest::gtest_main
)

# Integration tests are discovered separately so they can be run independently
gtest_discover_tests(wadjet_integration_tests
    TEST_PREFIX "integration."
)

# ============================================================================
# Testing Framework Tests (for the matchers and fixtures)
# ============================================================================
add_executable(wadjet_testing_framework_tests
    # Matcher tests
    testing/test_matchers.cpp
    # Live capture fixture tests (require CAP_NET_RAW or root)
    testing/test_live_capture_fixture.cpp
    # Property-based testing generators
    testing/test_generators.cpp
    # Record-then-assert mode
    testing/test_record_replay.cpp
    # Live-assert mode
    testing/test_live_assert.cpp
)

target_link_libraries(wadjet_testing_framework_tests
    PRIVATE
        wadjet
        wadjet_warnings
        wadjet_sanitizers
        GTest::gtest_main
        GTest::gmock
)

gtest_discover_tests(wadjet_testing_framework_tests
    TEST_PREFIX "testing."
)

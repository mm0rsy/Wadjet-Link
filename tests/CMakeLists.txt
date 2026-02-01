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
    protocols/test_gptp.cpp
    protocols/test_uds.cpp
    protocols/test_uds_regression.cpp
    protocols/test_dds.cpp
    protocols/test_uds_doip_integration.cpp
    protocols/test_flash_sequence.cpp
    protocols/test_dtc_manager.cpp
    # IPv4 tests (Phase 3)
    protocols/test_ipv4.cpp
    protocols/test_ipv4_options.cpp
    protocols/test_ipv4_fragmentation.cpp
    protocols/test_ipv4_minimal.cpp
    # TCP tests (Phase 4)
    protocols/test_tcp_state.cpp
    protocols/test_tcp_options.cpp
    protocols/test_tcp_retransmission.cpp
    # UDP tests (Phase 5)
    protocols/test_udp_checksum.cpp
    # SOME/IP-TP tests (Phase 6)
    protocols/test_someip_tp.cpp
    # SOME/IP-SD tests (Phase 7)
    protocols/test_someip_sd_entries.cpp
    protocols/test_someip_sd.cpp
    # DoIP Power Mode tests (Phase 8)
    protocols/test_doip_power.cpp
)

target_link_libraries(wadjet_tests
    PRIVATE
        wadjet
        wadjet_warnings
        wadjet_sanitizers
        GTest::gtest_main
        GTest::gmock
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
    # UDS integration tests
    integration/test_uds_integration.cpp
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

# ============================================================================
# Scenario Tests (requires yaml-cpp and nlohmann_json)
# ============================================================================
if(WADJET_SCENARIO_ENABLED)
    add_executable(wadjet_scenario_tests
        scenario/test_scenario_parser.cpp
        scenario/test_scenario_runner.cpp
    )

    target_link_libraries(wadjet_scenario_tests
        PRIVATE
            wadjet
            wadjet_warnings
            wadjet_sanitizers
            GTest::gtest_main
    )

    gtest_discover_tests(wadjet_scenario_tests
        TEST_PREFIX "scenario."
    )
endif()

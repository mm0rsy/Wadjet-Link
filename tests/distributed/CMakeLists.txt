# ============================================================================
# Distributed Testing Unit Tests
# ============================================================================

if(WADJET_ENABLE_DISTRIBUTED)
    # Create unified distributed tests executable
    if(NOT TARGET wadjet_distributed_tests)
        add_executable(wadjet_distributed_tests
            test_timestamp_normalizer.cpp
            test_sync_barrier.cpp
            test_coordinator.cpp
            test_coordinator_clock_sync.cpp
            test_node.cpp
            test_pcap_merger.cpp
            test_message_correlator.cpp
            test_config_loader.cpp
            test_coordination_overhead.cpp
        )

        target_link_libraries(wadjet_distributed_tests
            PRIVATE
                wadjet_distributed
                gtest
                gtest_main
                wadjet_warnings
                wadjet_sanitizers
                ssl
                crypto
        )

        # Register tests
        include(GoogleTest)
        gtest_discover_tests(wadjet_distributed_tests
            TEST_PREFIX "distributed."
        )
    endif()
endif()

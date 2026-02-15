# ============================================================================
# Distributed Testing Unit Tests
# ============================================================================

if(WADJET_ENABLE_DISTRIBUTED)
    # Create unified distributed tests executable
    if(NOT TARGET wadjet_distributed_tests)
        add_executable(wadjet_distributed_tests
            # Core synchronization and utilities (fully implemented)
            test_timestamp_normalizer.cpp
            test_sync_barrier.cpp
            
            # PCAP handling (fully implemented)
            test_pcap_merger.cpp
            
            # Message correlation (fully implemented)
            test_message_correlator.cpp
            
            # Configuration (fully implemented)
            test_config_loader.cpp
            
            # Scenario management (fully implemented)
            test_scenario_parser.cpp
            test_scenario_adapter.cpp  # T326/T327/T328: ScenarioAdapter conversion tests
            
            # Result aggregation (fully implemented)
            test_result_aggregation.cpp
            
            # Distributed matchers (simplified, fully implemented)
            test_distributed_matchers.cpp
            
            # TSN (Time-Sensitive Networking) distributed integration
            # T325: TSN stream tracking and latency assertions (fully implemented)
            test_tsn_distributed.cpp
            
            # Protocol-aware scenarios (SOME/IP, DoIP, etc.)
            # T338: Protocol-aware distributed scenario testing
            test_protocol_aware_scenarios.cpp
            
            # TODO: Uncomment when coordinator.cpp is completed
            # test_coordinator.cpp
            # test_coordinator_clock_sync.cpp
            # test_coordination_overhead.cpp
            
            # TODO: Uncomment when node.cpp is completed
            # test_node.cpp
            
            # TODO: Uncomment when gRPC service/client are completed
            # test_grpc_service.cpp
            # test_grpc_client.cpp
            
            # TODO: Uncomment when diagnostic integration is completed
            # test_diagnostic_distributed.cpp
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

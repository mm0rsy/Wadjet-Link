# ============================================================================
# Distributed Testing Unit Tests
# ============================================================================

# Timestamp Normalizer Tests
add_executable(test_timestamp_normalizer
    test_timestamp_normalizer.cpp
)

target_link_libraries(test_timestamp_normalizer
    PRIVATE
        wadjet_distributed
        gtest
        gtest_main
        wadjet_warnings
        wadjet_sanitizers
)

add_test(NAME TimestampNormalizer COMMAND test_timestamp_normalizer)

# SyncBarrier Tests
add_executable(test_sync_barrier
    test_sync_barrier.cpp
)

target_link_libraries(test_sync_barrier
    PRIVATE
        wadjet_distributed
        gtest
        gtest_main
        wadjet_warnings
        wadjet_sanitizers
)

add_test(NAME SyncBarrier COMMAND test_sync_barrier)

# TestCoordinator Tests
add_executable(test_coordinator
    test_coordinator.cpp
)

target_link_libraries(test_coordinator
    PRIVATE
        wadjet_distributed
        gtest
        gtest_main
        wadjet_warnings
        wadjet_sanitizers
)

add_test(NAME Coordinator COMMAND test_coordinator)

# TestNode Tests
add_executable(test_node
    test_node.cpp
)

target_link_libraries(test_node
    PRIVATE
        wadjet_distributed
        gtest
        gtest_main
        wadjet_warnings
        wadjet_sanitizers
)

add_test(NAME Node COMMAND test_node)


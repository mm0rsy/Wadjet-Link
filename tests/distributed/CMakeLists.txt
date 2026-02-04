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


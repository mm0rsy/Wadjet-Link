# ============================================================================
# Wadjet Examples
# ============================================================================

# SOME/IP Service Discovery Monitor
add_executable(someip_discovery someip_discovery.cpp)
target_link_libraries(someip_discovery PRIVATE wadjet)
target_compile_features(someip_discovery PRIVATE cxx_std_20)

# DoIP Routing Activation Validator
add_executable(doip_routing doip_routing.cpp)
target_link_libraries(doip_routing PRIVATE wadjet)
target_compile_features(doip_routing PRIVATE cxx_std_20)

# ECU Bootup Monitor
add_executable(ecu_bootup ecu_bootup.cpp)
target_link_libraries(ecu_bootup PRIVATE wadjet)
target_compile_features(ecu_bootup PRIVATE cxx_std_20)

# gPTP (IEEE 802.1AS) Monitor
add_executable(gptp_monitor gptp_monitor.cpp)
target_link_libraries(gptp_monitor PRIVATE wadjet)
target_compile_features(gptp_monitor PRIVATE cxx_std_20)

# UDS Diagnostic Traffic Monitor
add_executable(uds_monitor uds_monitor.cpp)
target_link_libraries(uds_monitor PRIVATE wadjet)
target_compile_features(uds_monitor PRIVATE cxx_std_20)

# UDS Protocol Validator
add_executable(uds_validator uds_validator.cpp)
target_link_libraries(uds_validator PRIVATE wadjet)
target_compile_features(uds_validator PRIVATE cxx_std_20)

# PCAP Regression Testing (requires GoogleTest)
if(TARGET GTest::gtest AND TARGET GTest::gmock)
    add_executable(pcap_regression pcap_regression.cpp)
    target_link_libraries(pcap_regression PRIVATE
        wadjet
        GTest::gtest
        GTest::gmock
    )
    target_compile_features(pcap_regression PRIVATE cxx_std_20)
    target_compile_definitions(pcap_regression PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../testdata"
    )

    # Add to CTest
    include(GoogleTest)
    gtest_discover_tests(pcap_regression)
endif()

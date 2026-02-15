# ============================================================================
# Distributed Testing Infrastructure Library
# ============================================================================

# Build proto library first (if protoc is available)
if(WADJET_ENABLE_DISTRIBUTED AND NOT SKIP_PROTO_GENERATION)
    add_subdirectory(${CMAKE_SOURCE_DIR}/proto proto_build)
    set(HAVE_PROTO_LIB ON)
else()
    set(HAVE_PROTO_LIB OFF)
endif()

# Create the distributed testing library
add_library(wadjet_distributed
    # Core synchronization primitives
    sync_barrier.cpp
    timestamp_normalizer.cpp
    
    # Message correlation
    message_correlator.cpp
    
    # PCAP merging
    pcap_merger.cpp
    
    # Distributed matchers
    distributed_matcher.cpp
    matchers/expect_message_flow.cpp
    matchers/within_latency.cpp
    matchers/happens_before.cpp
    matchers/must_not_see_on.cpp
    matchers/expect_stream_latency.cpp  # T323: TSN stream latency matcher
    
    # Scenario management
    scenario.cpp
    scenario_adapter.cpp  # T326: ScenarioAdapter for M4 to M14 conversion
    
    # Results aggregation
    result_aggregation.cpp
    
    # Configuration loading
    config_loader.cpp
    
    # Coordinator and Node (TODO: Fix broken implementation)
    # coordinator.cpp  # TODO: Fix broken implementation - too many compilation errors
    # node.cpp  # TODO: Fix broken implementation - incomplete virtual methods
    
    # gRPC service and client (TODO: Fix broken implementation)
    # grpc/service.cpp  # TODO: Fix broken implementation - syntax errors and incomplete methods
    # grpc/client.cpp  # TODO: Fix broken implementation
)

# Include proto generated code if available
target_include_directories(wadjet_distributed PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(HAVE_PROTO_LIB)
    target_include_directories(wadjet_distributed PUBLIC
        ${CMAKE_BINARY_DIR}/proto_build
        # Add gmock includes from grpc dependencies
        ${CMAKE_BINARY_DIR}/_deps/grpc-src/third_party/googletest/googlemock/include
        ${CMAKE_BINARY_DIR}/_deps/grpc-src/third_party/googletest/googletest/include
    )
endif()

# Link against generated proto code and gRPC
set(WADJET_DISTRIBUTED_LIBS
    PUBLIC
        wadjet
    PRIVATE
        wadjet_warnings
        wadjet_sanitizers
        protobuf::libprotobuf
)

if(HAVE_PROTO_LIB)
    list(APPEND WADJET_DISTRIBUTED_LIBS PUBLIC wadjet_distributed_proto)
    # Only add gRPC libraries if they're available
    if(TARGET gRPC::grpc)
        list(APPEND WADJET_DISTRIBUTED_LIBS PUBLIC gRPC::grpc gRPC::grpc++)
    endif()
    # Add gmock for Matcher support
    if(TARGET gmock)
        list(APPEND WADJET_DISTRIBUTED_LIBS PUBLIC gmock)
    endif()
endif()

target_link_libraries(wadjet_distributed ${WADJET_DISTRIBUTED_LIBS})

# Distributed header include paths
target_include_directories(wadjet_distributed
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Enable PIC for shared library linking
set_target_properties(wadjet_distributed PROPERTIES POSITION_INDEPENDENT_CODE ON)

# C++20 requirement
set_target_properties(wadjet_distributed PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Install library
install(TARGETS wadjet_distributed
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/wadjet/distributed
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wadjet
)

# Add tests subdirectory if building tests
if(WADJET_BUILD_TESTS)
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests/distributed tests_distributed_build)
endif()

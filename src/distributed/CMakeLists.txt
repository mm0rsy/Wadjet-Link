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
    
    # Coordinator and Node
    coordinator.cpp
    node.cpp
    
    # gRPC service and client
    grpc/service.cpp
    grpc/client.cpp
)

# Separate target for proto generated files (without -Werror)
if(HAVE_PROTO_LIB)
    add_library(wadjet_distributed_proto_obj OBJECT
        ${CMAKE_BINARY_DIR}/src/distributed/proto_build/distributed_test.pb.cc
    )
    
    target_include_directories(wadjet_distributed_proto_obj PRIVATE
        ${CMAKE_BINARY_DIR}/src/distributed/proto_build
        ${CMAKE_BINARY_DIR}/_deps/grpc-src/third_party/protobuf/src
    )
    
    # Remove -Werror from proto object files to allow protobuf library warnings
    get_target_property(PROTO_FLAGS wadjet_distributed_proto_obj COMPILE_FLAGS)
    if(PROTO_FLAGS)
        string(REPLACE "-Werror" "" PROTO_FLAGS "${PROTO_FLAGS}")
    else()
        set(PROTO_FLAGS "")
    endif()
    set_target_properties(wadjet_distributed_proto_obj PROPERTIES COMPILE_FLAGS "${PROTO_FLAGS}")
    
    target_compile_options(wadjet_distributed_proto_obj PRIVATE
        -Wno-conversion
        -Wno-sign-conversion
        -Wno-error=conversion
    )
    
    # Compile gRPC service and client with special handling
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/grpc/service.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/grpc/client.cpp
        PROPERTIES
        COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion"
    )
endif()

# Include proto generated code if available
target_include_directories(wadjet_distributed PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(HAVE_PROTO_LIB)
    target_include_directories(wadjet_distributed PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/proto_build
        ${CMAKE_BINARY_DIR}/src/distributed/proto_build
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

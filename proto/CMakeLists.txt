cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Protocol Buffer and gRPC Code Generation
# ============================================================================

# This file handles protobuf compilation and gRPC code generation.
# Generated code goes to CMAKE_CURRENT_BINARY_DIR for use in the build.

if(WADJET_ENABLE_DISTRIBUTED)
    # Proto files
    set(PROTO_FILES
        distributed_test.proto
    )
    
    # Use protoc compiler to generate C++ code
    find_program(PROTOC protoc REQUIRED)
    
    get_filename_component(PROTO_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
    get_filename_component(PROTO_FILE "${CMAKE_CURRENT_LIST_DIR}/distributed_test.proto" ABSOLUTE)
    
    # Generate proto code
    set(PROTO_SRCS
        "${CMAKE_CURRENT_BINARY_DIR}/distributed_test.pb.cc"
    )
    set(PROTO_HDRS
        "${CMAKE_CURRENT_BINARY_DIR}/distributed_test.pb.h"
    )
    set(GRPC_SRCS
        "${CMAKE_CURRENT_BINARY_DIR}/distributed_test.grpc.pb.cc"
    )
    set(GRPC_HDRS
        "${CMAKE_CURRENT_BINARY_DIR}/distributed_test.grpc.pb.h"
    )
    # Determine grpc_cpp_plugin location
    # The grpc_cpp_plugin is built as part of gRPC, we need to use an explicit path
    # Since add_custom_command with VERBATIM can't use generator expressions,
    # we'll set it up after knowing the build directory
    set(_PLUGIN_CANDIDATES
        "${CMAKE_BINARY_DIR}/_deps/grpc-build/grpc_cpp_plugin"
        "${grpc_BINARY_DIR}/grpc_cpp_plugin"
        "$ENV{GRPC_PLUGIN_PATH}/grpc_cpp_plugin"
    )
    
    set(_PLUGIN_PATH "")
    set(_PLUGIN_FOUND FALSE)
    
    # We'll add a pre-build target to find the plugin at build time
    # For now, we'll use a placeholder and resolve it during build
    set(_PLUGIN "grpc_cpp_plugin")
    set(_IMPLICIT_DEPS grpc_cpp_plugin)
    
    # Custom command to run protoc with grpc plugin
    # We need to handle the plugin path carefully since it's built during the build process
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
        COMMAND ${CMAKE_COMMAND} -E echo "Generating protobuf and gRPC code..."
        COMMAND ${PROTOC}
            --grpc_out ${CMAKE_CURRENT_BINARY_DIR}
            --plugin=protoc-gen-grpc=${CMAKE_BINARY_DIR}/_deps/grpc-build/grpc_cpp_plugin
            --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
            --experimental_allow_proto3_optional
            -I ${PROTO_DIR}
            ${PROTO_FILE}
        DEPENDS "${PROTO_FILE}" ${_IMPLICIT_DEPS}
        COMMENT "Generating protobuf and gRPC code for distributed_test.proto"
    )
    
    # NOW generate protobuf C++ code library with the output files
    add_library(wadjet_distributed_proto OBJECT
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        ${GRPC_SRCS}
        ${GRPC_HDRS}
    )
    
    # Link dependencies
    target_link_libraries(wadjet_distributed_proto PUBLIC
        protobuf::libprotobuf
    )
    
    # Only link gRPC targets if available
    if(TARGET gRPC::grpc)
        target_link_libraries(wadjet_distributed_proto PUBLIC
            gRPC::grpc
            gRPC::grpc++
            gRPC::grpc++_reflection
        )
    endif()
    
    # Include directories for generated code
    target_include_directories(wadjet_distributed_proto PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}"
    )
    
endif()

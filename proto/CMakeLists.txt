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
    # Wait until the plugin is built, then use it
    if(TARGET _grpc_cpp_plugin)
        # Use target to track dependency
        set(_PLUGIN "$<TARGET_FILE:_grpc_cpp_plugin>")
        set(_IMPLICIT_DEPS _grpc_cpp_plugin)
    elseif(TARGET grpc_cpp_plugin)
        # Fallback target
        get_target_property(_PLUGIN grpc_cpp_plugin IMPORTED_LOCATION)
        set(_IMPLICIT_DEPS grpc_cpp_plugin)
    else()
        # Try to find it in the build directory (if built from FetchContent)
        set(_PLUGIN "${CMAKE_BINARY_DIR}/_deps/grpc-build/grpc_cpp_plugin")
        set(_IMPLICIT_DEPS)
    endif()
    
    # Custom command to run protoc with grpc plugin - BEFORE adding the library
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
        COMMAND ${CMAKE_COMMAND} -E echo "Waiting for grpc_cpp_plugin..."
        COMMAND ${PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
             --plugin=protoc-gen-grpc="${_PLUGIN}"
             --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
             -I "${PROTO_DIR}"
             "${PROTO_FILE}"
        DEPENDS "${PROTO_FILE}" ${_IMPLICIT_DEPS}
        COMMENT "Generating protobuf and gRPC code for distributed_test.proto"
        VERBATIM
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

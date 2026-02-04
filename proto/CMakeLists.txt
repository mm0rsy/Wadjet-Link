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
    
    # Generate protobuf C++ code
    add_library(wadjet_distributed_proto OBJECT)
    
    target_sources(wadjet_distributed_proto PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/distributed_test.proto
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
    if(TARGET gRPC::grpc_cpp_plugin)
        get_target_property(_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
    else()
        # Try to find it in the build directory (if built from FetchContent)
        set(_PLUGIN "${CMAKE_BINARY_DIR}/_deps/grpc-build/grpc_cpp_plugin")
    endif()
    
    # Custom command to run protoc with grpc plugin
    # Add IMPLICIT_DEPENDS for grpc_cpp_plugin if available
    set(_IMPLICIT_DEPS)
    if(TARGET grpc_cpp_plugin)
        list(APPEND _IMPLICIT_DEPS grpc_cpp_plugin)
    endif()
    
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
        COMMAND ${PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
             --plugin=protoc-gen-grpc="${_PLUGIN}"
             --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
             -I "${PROTO_DIR}"
             "${PROTO_FILE}"
        DEPENDS "${PROTO_FILE}" ${_IMPLICIT_DEPS}
        COMMENT "Generating protobuf and gRPC code for distributed_test.proto"
    )
    
    # Add generated files to the library
    target_sources(wadjet_distributed_proto PRIVATE
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

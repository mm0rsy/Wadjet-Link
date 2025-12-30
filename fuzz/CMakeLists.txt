# ============================================================================
# Wadjet Fuzz Tests
# ============================================================================
#
# Fuzz testing using libFuzzer (LLVM/Clang)
#
# Build:
#   cmake -B build-fuzz -DWADJET_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
#   cmake --build build-fuzz
#
# Run:
#   ./build-fuzz/fuzz/fuzz_ethernet fuzz/corpus/ethernet -max_total_time=300
#
# ============================================================================

# Check for Clang (libFuzzer requires Clang)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzz tests require Clang with libFuzzer. Skipping fuzz targets.")
    return()
endif()

# Fuzzer compile/link options
add_library(wadjet_fuzzer_options INTERFACE)
target_compile_options(wadjet_fuzzer_options INTERFACE
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer
    -g
)
target_link_options(wadjet_fuzzer_options INTERFACE
    -fsanitize=fuzzer,address,undefined
)

# ============================================================================
# Fuzz Targets
# ============================================================================

# Ethernet decoder fuzz target
add_executable(fuzz_ethernet
    fuzz_ethernet.cpp
)
target_link_libraries(fuzz_ethernet
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# IPv4 decoder fuzz target
add_executable(fuzz_ipv4
    fuzz_ipv4.cpp
)
target_link_libraries(fuzz_ipv4
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# UDP decoder fuzz target
add_executable(fuzz_udp
    fuzz_udp.cpp
)
target_link_libraries(fuzz_udp
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# TCP decoder fuzz target
add_executable(fuzz_tcp
    fuzz_tcp.cpp
)
target_link_libraries(fuzz_tcp
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# SOME/IP decoder fuzz target
add_executable(fuzz_someip
    fuzz_someip.cpp
)
target_link_libraries(fuzz_someip
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# SOME/IP-SD decoder fuzz target
add_executable(fuzz_someip_sd
    fuzz_someip_sd.cpp
)
target_link_libraries(fuzz_someip_sd
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# DoIP decoder fuzz target
add_executable(fuzz_doip
    fuzz_doip.cpp
)
target_link_libraries(fuzz_doip
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# Full protocol stack fuzz target
add_executable(fuzz_dispatcher
    fuzz_dispatcher.cpp
)
target_link_libraries(fuzz_dispatcher
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# ============================================================================
# Corpus setup
# ============================================================================
# The corpus directory contains pre-generated seed files
# Run create_corpus.cpp to regenerate if needed:
#   g++ -std=c++20 -o create_corpus create_corpus.cpp && ./create_corpus

# Custom target to help users set up fuzzing
add_custom_target(fuzz_help
    COMMAND ${CMAKE_COMMAND} -E echo "=== Wadjet Fuzz Testing ==="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available fuzz targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_ethernet   - Ethernet decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_ipv4       - IPv4 decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_udp        - UDP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_tcp        - TCP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_someip     - SOME/IP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_someip_sd  - SOME/IP-SD decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_doip       - DoIP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_dispatcher - Full protocol stack"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Run a fuzz test:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./fuzz/fuzz_dispatcher fuzz/corpus -max_total_time=300"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Useful libFuzzer options:"
    COMMAND ${CMAKE_COMMAND} -E echo "  -max_total_time=N  Run for N seconds"
    COMMAND ${CMAKE_COMMAND} -E echo "  -max_len=N         Max input size"
    COMMAND ${CMAKE_COMMAND} -E echo "  -jobs=N            Parallel jobs"
    COMMAND ${CMAKE_COMMAND} -E echo "  -workers=N         Parallel workers"
    COMMAND ${CMAKE_COMMAND} -E echo "  -dict=file         Dictionary file"
    COMMENT "Wadjet fuzz testing help"
)

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

# IPv4 options fuzz target (T127)
add_executable(fuzz_ipv4_options
    fuzz_ipv4_options.cpp
)
target_link_libraries(fuzz_ipv4_options
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# IPv4 fragmentation fuzz target
add_executable(fuzz_ipv4_fragmentation
    fuzz_ipv4_fragmentation.cpp
)
target_link_libraries(fuzz_ipv4_fragmentation
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

# TCP options fuzz target (T128)
add_executable(fuzz_tcp_options
    fuzz_tcp_options.cpp
)
target_link_libraries(fuzz_tcp_options
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

# SOME/IP-TP segmentation fuzz target (T129)
add_executable(fuzz_someip_tp
    fuzz_someip_tp.cpp
)
target_link_libraries(fuzz_someip_tp
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

# SOME/IP-SD entries and options fuzz target (T130)
add_executable(fuzz_someip_sd_entries
    fuzz_someip_sd_entries.cpp
)
target_link_libraries(fuzz_someip_sd_entries
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

# gPTP (IEEE 802.1AS) decoder fuzz target
add_executable(fuzz_gptp
    fuzz_gptp.cpp
)
target_link_libraries(fuzz_gptp
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# gPTP TLV fuzz target (T131)
add_executable(fuzz_gptp_tlv
    fuzz_gptp_tlv.cpp
)
target_link_libraries(fuzz_gptp_tlv
    PRIVATE
        wadjet
        wadjet_fuzzer_options
)

# UDS (ISO 14229) decoder fuzz target
add_executable(fuzz_uds
    fuzz_uds.cpp
)
target_link_libraries(fuzz_uds
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
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_ipv4_options - IPv4 options parsing (T127)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_ipv4_fragmentation - IPv4 fragmentation"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_udp        - UDP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_tcp        - TCP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_tcp_options - TCP options parsing (T128)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_someip     - SOME/IP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_someip_tp  - SOME/IP-TP segmentation (T129)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_someip_sd  - SOME/IP-SD decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_someip_sd_entries - SOME/IP-SD entries/options (T130)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_doip       - DoIP decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_gptp       - gPTP (IEEE 802.1AS) decoder"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_gptp_tlv   - gPTP TLV parsing (T131)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fuzz_uds        - UDS (ISO 14229) decoder"
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

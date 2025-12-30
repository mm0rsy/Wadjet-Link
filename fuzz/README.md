# Wadjet Fuzz Testing

This directory contains fuzz test harnesses for all Wadjet protocol decoders, powered by [libFuzzer](https://llvm.org/docs/LibFuzzer.html).

## Prerequisites

- **Clang/LLVM** with libFuzzer support (Clang 6.0+)
- AddressSanitizer and UndefinedBehaviorSanitizer (included with Clang)

## Building

```bash
# Configure with Clang and fuzzing enabled
cmake -B build-fuzz \
    -DWADJET_BUILD_FUZZ=ON \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang

# Build all fuzz targets
cmake --build build-fuzz
```

## Available Fuzz Targets

| Target | Description |
|--------|-------------|
| `fuzz_ethernet` | Ethernet frame decoder (including VLAN) |
| `fuzz_ipv4` | IPv4 header decoder (including options) |
| `fuzz_udp` | UDP datagram decoder |
| `fuzz_tcp` | TCP segment decoder (including options) |
| `fuzz_someip` | SOME/IP message decoder |
| `fuzz_someip_sd` | SOME/IP Service Discovery decoder |
| `fuzz_doip` | DoIP (Diagnostics over IP) decoder |
| `fuzz_dispatcher` | Full protocol stack (Ethernet→IP→UDP/TCP→App) |

## Running Fuzz Tests

### Basic Usage

```bash
# Run with seed corpus for 5 minutes
./build-fuzz/fuzz/fuzz_dispatcher fuzz/corpus -max_total_time=300

# Run Ethernet decoder fuzz test
./build-fuzz/fuzz/fuzz_ethernet fuzz/corpus -max_total_time=60
```

### Useful libFuzzer Options

| Option | Description |
|--------|-------------|
| `-max_total_time=N` | Stop after N seconds |
| `-max_len=N` | Maximum input length (default: auto) |
| `-jobs=N` | Number of fuzzing jobs in parallel |
| `-workers=N` | Number of worker processes |
| `-dict=file` | Use dictionary file for guided fuzzing |
| `-only_ascii=1` | Generate only ASCII inputs |
| `-print_final_stats=1` | Print statistics at the end |
| `-help=1` | Show all options |

### Example: Long-Running Fuzzing

```bash
# Run for 1 hour with 4 parallel jobs
./build-fuzz/fuzz/fuzz_dispatcher fuzz/corpus \
    -max_total_time=3600 \
    -jobs=4 \
    -workers=4 \
    -print_final_stats=1
```

## Seed Corpus

The `corpus/` directory contains pre-generated seed files for improved fuzzing coverage:

- `eth_basic` - Basic Ethernet frame
- `eth_vlan` - Ethernet with VLAN tag
- `ipv4_basic` - Basic IPv4 header
- `ipv4_options` - IPv4 with options
- `udp_basic` - Basic UDP header
- `tcp_basic` - Basic TCP header
- `tcp_options` - TCP with options
- `someip_basic` - Basic SOME/IP message
- `someip_sd` - SOME/IP Service Discovery message
- `doip_basic` - Basic DoIP header
- `full_eth_ip_udp_someip` - Complete Ethernet/IP/UDP/SOME/IP stack
- `full_eth_ip_tcp_doip` - Complete Ethernet/IP/TCP/DoIP stack
- `minimal_*` - Minimum-sized headers (boundary testing)
- `empty` - Empty input (edge case)
- `single_byte` - Single byte input (edge case)

### Regenerating Seed Corpus

```bash
cd fuzz
g++ -std=c++20 -o create_corpus create_corpus.cpp
./create_corpus
rm create_corpus  # Clean up binary
```

## What the Fuzz Tests Cover

Each fuzz harness:

1. **Creates a DecodeContext** from arbitrary fuzzer-generated data
2. **Calls the decoder's `decode()` method** with the malformed input
3. **Accesses all decoded fields** to trigger any latent crashes
4. **Tests helper methods and string conversions**

The tests are designed to find:

- Buffer overflows / underflows
- Integer overflows
- Null pointer dereferences
- Use-after-free bugs
- Undefined behavior
- Memory leaks (via ASan)

## Crash Triage

When libFuzzer finds a crash, it will:

1. Print the crash type (e.g., `heap-buffer-overflow`)
2. Save the crashing input to `crash-<hash>` or `oom-<hash>`
3. Print a stack trace with source locations

To reproduce a crash:

```bash
./build-fuzz/fuzz/fuzz_dispatcher crash-abc123def456
```

## CI Integration

Add to your CI pipeline:

```yaml
- name: Build Fuzz Tests
  run: |
    cmake -B build-fuzz -DWADJET_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
    cmake --build build-fuzz

- name: Run Fuzz Tests (Short)
  run: |
    for target in build-fuzz/fuzz/fuzz_*; do
      $target fuzz/corpus -max_total_time=60 -print_final_stats=1
    done
```

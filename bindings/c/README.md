# 𓆓 Wadjet-Link C Bindings

C API for the Wadjet-Link automotive Ethernet validation framework.

## Features

- **C99 Compatible** — Works with any C compiler supporting C99
- **FFI Ready** — Designed for easy integration with Rust, Python ctypes, and other FFI systems
- **Opaque Handles** — Memory-safe design with opaque pointers
- **Thread-Local Errors** — Detailed error messages via `wadjet_last_error()`
- **Full API Coverage** — Device enumeration, capture, PCAP I/O, protocol decoding

## Building

### Prerequisites

- CMake 3.20+
- C compiler (GCC, Clang)
- Wadjet-Link core library

### Build with CMake

```bash
# From wadjet-link root directory
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DWADJET_BUILD_C_BINDINGS=ON

cmake --build build

# Library outputs:
# - build/bindings/c/libwadjet_c.so (shared)
# - build/bindings/c/libwadjet_c.a (static)
```

## Installation

```bash
# Install headers and library
sudo cmake --install build --component c-bindings

# Or manually:
sudo cp build/bindings/c/libwadjet_c.so /usr/local/lib/
sudo cp bindings/c/include/wadjet_c.h /usr/local/include/
sudo ldconfig
```

## Quick Start

### List Network Devices

```c
#include <wadjet_c.h>
#include <stdio.h>

int main() {
    // Get device count
    size_t count;
    wadjet_error_t err = wadjet_device_count(&count);
    if (err != WADJET_OK) {
        fprintf(stderr, "Error: %s\n", wadjet_error_message(err));
        return 1;
    }

    printf("Found %zu devices:\n", count);

    // List each device
    for (size_t i = 0; i < count; i++) {
        wadjet_device_info_t info;
        err = wadjet_device_info(i, &info);
        if (err == WADJET_OK) {
            printf("  [%zu] %s - %s\n", i, info.name, info.description);
        }
    }

    return 0;
}
```

### Live Packet Capture

```c
#include <wadjet_c.h>
#include <stdio.h>

int main() {
    wadjet_capture_session_t session = NULL;
    wadjet_capture_options_t options = {
        .promiscuous = true,
        .snaplen = 65535,
        .timeout_ms = 1000
    };

    // Create capture session
    wadjet_error_t err = wadjet_capture_create("eth0", &options, &session);
    if (err != WADJET_OK) {
        fprintf(stderr, "Error: %s\n", wadjet_last_error());
        return 1;
    }

    // Set BPF filter (optional)
    wadjet_capture_set_filter(session, "udp port 30490");

    // Start capturing
    wadjet_capture_start(session);

    // Read packets
    for (int i = 0; i < 10; i++) {
        wadjet_packet_t packet = NULL;
        err = wadjet_capture_next_packet(session, 1000, &packet);
        if (err == WADJET_OK && packet) {
            size_t len;
            wadjet_packet_length(packet, &len);
            printf("Packet %d: %zu bytes\n", i, len);
            wadjet_packet_destroy(packet);
        }
    }

    // Cleanup
    wadjet_capture_stop(session);
    wadjet_capture_destroy(session);

    return 0;
}
```

### Protocol Decoding

```c
#include <wadjet_c.h>
#include <stdio.h>

void decode_packet(wadjet_packet_t packet) {
    wadjet_decode_result_t result = NULL;
    wadjet_error_t err = wadjet_decode_packet(packet, &result);
    if (err != WADJET_OK) return;

    // Check for Ethernet
    if (wadjet_decode_has_ethernet(result)) {
        wadjet_ethernet_header_t eth;
        wadjet_decode_ethernet(result, &eth);
        printf("Ethernet: %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth.src_mac[0], eth.src_mac[1], eth.src_mac[2],
               eth.src_mac[3], eth.src_mac[4], eth.src_mac[5],
               eth.dst_mac[0], eth.dst_mac[1], eth.dst_mac[2],
               eth.dst_mac[3], eth.dst_mac[4], eth.dst_mac[5]);
    }

    // Check for SOME/IP
    if (wadjet_decode_has_someip(result)) {
        wadjet_someip_header_t someip;
        wadjet_decode_someip(result, &someip);
        printf("SOME/IP: Service=0x%04x Method=0x%04x\n",
               someip.service_id, someip.method_id);
    }

    wadjet_decode_destroy(result);
}
```

### PCAP File I/O

```c
#include <wadjet_c.h>
#include <stdio.h>

// Reading PCAP
void read_pcap(const char* filename) {
    wadjet_pcap_reader_t reader = NULL;
    wadjet_error_t err = wadjet_pcap_reader_open(filename, &reader);
    if (err != WADJET_OK) {
        fprintf(stderr, "Error: %s\n", wadjet_last_error());
        return;
    }

    wadjet_packet_t packet = NULL;
    int count = 0;
    while (wadjet_pcap_reader_next(reader, &packet) == WADJET_OK && packet) {
        count++;
        wadjet_packet_destroy(packet);
    }
    printf("Read %d packets from %s\n", count, filename);

    wadjet_pcap_reader_close(reader);
}

// Writing PCAP
void write_pcap(const char* filename, wadjet_packet_t* packets, size_t count) {
    wadjet_pcap_writer_t writer = NULL;
    wadjet_error_t err = wadjet_pcap_writer_open(filename, &writer);
    if (err != WADJET_OK) return;

    for (size_t i = 0; i < count; i++) {
        wadjet_pcap_writer_write_packet(writer, packets[i]);
    }

    wadjet_pcap_writer_close(writer);
}
```

## API Reference

### Error Handling

| Function | Description |
|----------|-------------|
| `wadjet_error_message(error)` | Get static error message for error code |
| `wadjet_last_error()` | Get detailed thread-local error message |
| `wadjet_clear_error()` | Clear thread-local error |

### Device Enumeration

| Function | Description |
|----------|-------------|
| `wadjet_device_count(count*)` | Get number of available network devices |
| `wadjet_device_info(index, info*)` | Get device info by index |
| `wadjet_list_interfaces(names*, count)` | Get all interface names |

### Capture Session

| Function | Description |
|----------|-------------|
| `wadjet_capture_create(iface, opts*, session*)` | Create capture session |
| `wadjet_capture_destroy(session)` | Destroy capture session |
| `wadjet_capture_start(session)` | Start capturing packets |
| `wadjet_capture_stop(session)` | Stop capturing |
| `wadjet_capture_is_running(session)` | Check if capturing |
| `wadjet_capture_set_filter(session, bpf)` | Set BPF filter |
| `wadjet_capture_next_packet(session, timeout, packet*)` | Get next packet |

### Packet Operations

| Function | Description |
|----------|-------------|
| `wadjet_packet_data(packet, data*, len*)` | Get packet data pointer |
| `wadjet_packet_length(packet, len*)` | Get packet length |
| `wadjet_packet_timestamp(packet, ts*)` | Get packet timestamp |
| `wadjet_packet_destroy(packet)` | Free packet memory |

### Protocol Decoding

| Function | Description |
|----------|-------------|
| `wadjet_decode_packet(packet, result*)` | Decode packet protocol stack |
| `wadjet_decode_destroy(result)` | Free decode result |
| `wadjet_decode_has_ethernet(result)` | Check for Ethernet header |
| `wadjet_decode_has_ipv4(result)` | Check for IPv4 header |
| `wadjet_decode_has_udp(result)` | Check for UDP header |
| `wadjet_decode_has_tcp(result)` | Check for TCP header |
| `wadjet_decode_has_someip(result)` | Check for SOME/IP header |
| `wadjet_decode_has_doip(result)` | Check for DoIP header |

### PCAP I/O

| Function | Description |
|----------|-------------|
| `wadjet_pcap_reader_open(filename, reader*)` | Open PCAP file for reading |
| `wadjet_pcap_reader_close(reader)` | Close PCAP reader |
| `wadjet_pcap_reader_next(reader, packet*)` | Read next packet |
| `wadjet_pcap_writer_open(filename, writer*)` | Open PCAP file for writing |
| `wadjet_pcap_writer_close(writer)` | Close PCAP writer |
| `wadjet_pcap_writer_write_packet(writer, packet)` | Write packet to file |

## Examples

The `examples/` directory contains working examples:

- `list_devices.c` — Enumerate network interfaces
- `capture_example.c` — Live packet capture with filtering

Build examples:

```bash
cmake -B build -DWADJET_BUILD_C_BINDINGS=ON -DWADJET_BUILD_EXAMPLES=ON
cmake --build build
./build/bindings/c/examples/c_list_devices
./build/bindings/c/examples/c_capture_example
```

## Thread Safety

- **Thread-local error storage**: Each thread has its own error message buffer
- **Opaque handles**: All handle types are thread-safe for concurrent access
- **Callback safety**: Callbacks are invoked from the calling thread

## Memory Management

All Wadjet C API functions follow these conventions:

1. **Creation functions** return handles via output pointer parameters
2. **Destroy functions** must be called to free resources
3. **Output strings** are either static (do not free) or owned (caller must free)
4. **Packets** must be destroyed with `wadjet_packet_destroy()`

## License

Apache 2.0 — Same as Wadjet-Link

---

*𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.*

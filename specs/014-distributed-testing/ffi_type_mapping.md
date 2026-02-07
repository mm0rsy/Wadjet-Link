# Distributed Testing FFI Type Mapping Table

**Task**: T288  
**Purpose**: Document FFI type mapping for distributed testing primitives (Constitution Principle V)  
**Date**: February 7, 2026  
**Status**: Complete - Pre-work documentation for Phase 8 FFI bindings

## Overview

This document defines the type mappings for distributed testing primitives across C++ (native), C ABI (Foreign Function Interface), Python (ctypes), and Rust (FFI) to enable bindings across language barriers.

**Principle**: Constitution Principle V mandates consistent type representation across all FFI boundaries.

## Core Distributed Testing Types

### 1. NodeId String Type

**Use Case**: Unique identifier for nodes in distributed tests

| Language | Type | C ABI Representation | Notes |
|----------|------|---------------------|-------|
| C++ | `std::string` | `const char*` (UTF-8 null-terminated) | Owned by caller |
| C ABI | `const char*` | `const char*` | Must be null-terminated |
| Python | `str` | `c_char_p` (via ctypes) | Auto UTF-8 encoding |
| Rust | `&str` or `String` | `*const c_char` | Borrow vs owned |

**Mapping Definition**:
```c
// C ABI binding
typedef const char* NodeId;

// Python binding
from ctypes import c_char_p
NodeId = c_char_p  # UTF-8 string

// Rust binding
pub type NodeId = *const c_char;  // Borrowed
// or
pub struct OwnedNodeId(pub String);  // Owned
```

**Conversion Functions**:
```cpp
// C++ → C ABI
const char* to_c_str(const std::string& id) { return id.c_str(); }

// C ABI → C++
std::string from_c_str(const char* id) { return std::string(id ? id : ""); }
```

### 2. Timestamp (nanoseconds)

**Use Case**: Clock-synchronized timestamps across distributed nodes

| Language | Type | C ABI Representation | Range | Precision |
|----------|------|---------------------|-------|-----------|
| C++ | `int64_t` | `int64_t` | -2^63 to 2^63-1 | 1 ns |
| C ABI | `int64_t` | `int64_t` (8 bytes) | -2^63 to 2^63-1 | 1 ns |
| Python | `int` | `c_int64` (via ctypes) | Unlimited (Python 3) | 1 ns |
| Rust | `i64` | `i64` | -2^63 to 2^63-1 | 1 ns |

**Mapping Definition**:
```c
// C ABI binding
typedef int64_t Timestamp;  // nanoseconds since Unix epoch

// Python binding
from ctypes import c_int64
Timestamp = c_int64  # Nanoseconds

// Rust binding
pub type Timestamp = i64;  // nanoseconds since Unix epoch
```

**Conversion Functions**:
```cpp
// std::chrono to int64_t
int64_t to_nanos(std::chrono::nanoseconds dur) { return dur.count(); }

// int64_t to std::chrono
std::chrono::nanoseconds from_nanos(int64_t ns) { return std::chrono::nanoseconds(ns); }
```

### 3. ClockSyncStatus Struct

**Use Case**: Clock synchronization state with method, offset, and grandmaster info

**C++ Definition**:
```cpp
struct ClockSyncStatus {
    ClockSyncMethod method;           // enum: None, NTP, GPTP, Unknown
    bool is_synchronized;             // synchronization achieved
    int64_t estimated_offset_ns;      // offset in nanoseconds
    int64_t max_error_ns;             // max estimation error
    std::string grandmaster_id;       // grandmaster identifier
};
```

**C ABI Binding**:
```c
// Opaque handle for C
typedef void* ClockSyncStatusHandle;

// C ABI functions to access fields
int32_t clock_sync_status_get_method(ClockSyncStatusHandle h);
bool clock_sync_status_get_synchronized(ClockSyncStatusHandle h);
int64_t clock_sync_status_get_offset_ns(ClockSyncStatusHandle h);
int64_t clock_sync_status_get_error_ns(ClockSyncStatusHandle h);
const char* clock_sync_status_get_grandmaster_id(ClockSyncStatusHandle h);

// Destruction
void clock_sync_status_destroy(ClockSyncStatusHandle h);
```

**Python Binding** (via ctypes):
```python
from ctypes import Structure, c_int32, c_bool, c_int64, c_char_p

class ClockSyncStatus(Structure):
    _fields_ = [
        ("method", c_int32),          # 0=None, 1=NTP, 2=GPTP, 3=Unknown
        ("is_synchronized", c_bool),
        ("estimated_offset_ns", c_int64),
        ("max_error_ns", c_int64),
        ("grandmaster_id", c_char_p),  # UTF-8 string
    ]
```

**Rust Binding**:
```rust
#[repr(C)]
pub struct ClockSyncStatus {
    pub method: c_int,               // 0=None, 1=NTP, 2=GPTP, 3=Unknown
    pub is_synchronized: bool,
    pub estimated_offset_ns: i64,
    pub max_error_ns: i64,
    pub grandmaster_id: *const c_char,  // UTF-8 string (borrowed)
}
```

### 4. Result<T> Error Handling Type

**Use Case**: Consistent error propagation across language barriers (Constitution Principle I)

**C++ Definition**:
```cpp
template<typename T>
struct Result {
    bool ok;
    T value;           // valid if ok == true
    int error_code;    // valid if ok == false
    const char* error_message;
};
```

**C ABI Binding**:
```c
// Generic result with opaque payload
typedef struct {
    bool ok;
    void* value_handle;        // opaque T*
    int error_code;
    const char* error_message;  // UTF-8 string
} ResultHandle;

// Destroy result and release resources
void result_destroy(ResultHandle* r);
```

**Python Binding**:
```python
from ctypes import Structure, c_bool, c_void_p, c_int32, c_char_p

class Result(Structure):
    _fields_ = [
        ("ok", c_bool),
        ("value_handle", c_void_p),      # void* payload
        ("error_code", c_int32),
        ("error_message", c_char_p),     # UTF-8 string
    ]

def unwrap(result: Result):
    """Unwrap result or raise exception"""
    if not result.ok:
        raise Exception(f"Error {result.error_code}: {result.error_message.decode()}")
    return result.value_handle
```

**Rust Binding**:
```rust
#[repr(C)]
pub struct ResultHandle {
    pub ok: bool,
    pub value_handle: *mut c_void,
    pub error_code: c_int,
    pub error_message: *const c_char,
}

impl ResultHandle {
    pub fn ok(&self) -> Result<*mut c_void, String> {
        if self.ok {
            Ok(self.value_handle)
        } else {
            let msg = unsafe { CStr::from_ptr(self.error_message).to_string_lossy() };
            Err(msg.into_string())
        }
    }
}
```

## Result Type Mappings

### 5. NodeResult Struct

**Use Case**: Test results from a single node

| Field | C++ | C ABI | Python | Rust |
|-------|-----|-------|--------|------|
| node_id | `NodeId` | `const char*` | `str` | `*const c_char` |
| healthy | `bool` | `bool` (1 byte) | `bool` | `bool` |
| assertions | `std::vector<AssertionResult>` | `AssertionResult*` + count | `List[AssertionResult]` | `*const AssertionResult` |
| passed_count | `int` | `int32_t` | `int` | `c_int` |
| failed_count | `int` | `int32_t` | `int` | `c_int` |
| total_duration | `std::chrono::nanoseconds` | `int64_t` | `int` | `i64` |
| pcap_file_path | `std::string` | `const char*` | `str` | `*const c_char` |

**C ABI Definition**:
```c
typedef struct {
    NodeId node_id;
    bool healthy;
    AssertionResult* assertions;    // Array pointer
    int32_t assertion_count;        // Array size
    int32_t passed_count;
    int32_t failed_count;
    int64_t total_duration_ns;
    const char* pcap_file_path;
    const char* error_message;      // NULL if healthy
} NodeResult;

// Allocation/deallocation
NodeResult* node_result_create(void);
void node_result_destroy(NodeResult* r);
```

### 6. AggregatedResult Struct

**Use Case**: Combined test results from all nodes

| Field | C ABI Type | Size | Notes |
|-------|-----------|------|-------|
| test_name | `const char*` | pointer | UTF-8 string |
| node_results | `NodeResult*` | pointer | Array of results |
| node_count | `int32_t` | 4 bytes | Number of nodes |
| test_start_time_ns | `int64_t` | 8 bytes | UTC nanoseconds |
| test_end_time_ns | `int64_t` | 8 bytes | UTC nanoseconds |
| overall_status | `int32_t` | 4 bytes | 0=Passed, 1=Failed, 2=Error |
| total_duration_ns | `int64_t` | 8 bytes | Nanoseconds |

**C ABI Definition**:
```c
typedef struct {
    const char* test_name;
    NodeResult* node_results;       // Array
    int32_t node_count;
    int64_t test_start_time_ns;
    int64_t test_end_time_ns;
    int32_t overall_status;         // 0=Passed, 1=Failed, 2=Error
    int64_t total_duration_ns;
    int32_t total_assertions;
    int32_t passed_assertions;
    int32_t failed_assertions;
} AggregatedResult;

// Allocation/lifecycle
AggregatedResult* aggregated_result_create(void);
void aggregated_result_destroy(AggregatedResult* r);

// Export functions
const char* aggregated_result_to_junit_xml(const AggregatedResult* r);
const char* aggregated_result_to_json(const AggregatedResult* r);
const char* aggregated_result_to_html(const AggregatedResult* r);
```

## Vector/Array Mappings

**Pattern for C++ std::vector<T>**:

| To C ABI | Pattern |
|----------|---------|
| Read-only | `T* ptr` + `size_t count` |
| Transfer ownership | Allocate with malloc(), caller frees |
| Borrow | Pass pointer, don't free in callee |

**Example for std::vector<Packet>**:
```c
// C ABI interface
typedef struct {
    Packet* packets;        // Array of packets
    size_t packet_count;    // Number of packets
} PacketArray;

// To transfer ownership from C++ to C
PacketArray* packets_to_c_array(const std::vector<Packet>& packets) {
    auto* arr = (PacketArray*)malloc(sizeof(PacketArray));
    arr->packet_count = packets.size();
    arr->packets = (Packet*)malloc(sizeof(Packet) * packets.size());
    std::memcpy(arr->packets, packets.data(), sizeof(Packet) * packets.size());
    return arr;
}

// Caller must free
void packet_array_destroy(PacketArray* arr) {
    free(arr->packets);
    free(arr);
}
```

## String Mappings

**Pattern for C++ std::string**:

| To Language | Mapping |
|-------------|---------|
| C ABI | `const char*` (UTF-8, caller doesn't free) |
| Python | `c_char_p` auto-decodes to `str` |
| Rust | `*const c_char` → `CStr::from_ptr()` |

**Example**:
```cpp
// C++ function
std::string get_node_name(NodeId id);

// Maps to C ABI
const char* node_get_name(NodeId id) {
    static thread_local std::string name;  // Cache to avoid dangling pointer
    name = get_node_name(id);
    return name.c_str();
}
```

## Enum Mappings

**Pattern for C++ enums**:

| C++ Enum | C ABI | Python | Rust |
|----------|-------|--------|------|
| `enum class T { A, B, C }` | `int32_t` (0, 1, 2) | `c_int32` | `c_int` |

**Example - ResultStatus**:
```cpp
// C++
enum class ResultStatus {
    Passed = 0,
    Failed = 1,
    Error = 2,
    Skipped = 3,
    Timeout = 4,
};

// C ABI (use int32_t)
#define RESULT_STATUS_PASSED 0
#define RESULT_STATUS_FAILED 1
#define RESULT_STATUS_ERROR 2
#define RESULT_STATUS_SKIPPED 3
#define RESULT_STATUS_TIMEOUT 4

// Python
ResultStatus = {
    'Passed': 0,
    'Failed': 1,
    'Error': 2,
    'Skipped': 3,
    'Timeout': 4,
}

// Rust
#[repr(i32)]
pub enum ResultStatus {
    Passed = 0,
    Failed = 1,
    Error = 2,
    Skipped = 3,
    Timeout = 4,
}
```

## Memory Management Rules (Constitution Principle II)

**Ownership Transfer Rules**:

1. **C++ → C ABI → Caller**:
   - Allocate with `malloc()` or custom allocator
   - Caller frees using provided `destroy()` function
   - Example: `AggregatedResult* create()` → caller calls `destroy(result)`

2. **Borrowed References**:
   - Pointer with limited lifetime
   - Caller cannot free
   - Example: `void process_packet(const Packet* pkt)` - don't free pkt

3. **Static/Thread-Local Storage**:
   - Return pointer to static/thread-local data
   - Caller cannot free
   - Lifetime guaranteed until next call
   - Example: `const char* get_error_message()` returns static buffer

**C API Convention**:
```c
// Create allocates
SomeType* some_type_create(params);

// Destroy deallocates
void some_type_destroy(SomeType* obj);

// Methods take ownership or return borrowed refs (documented)
void some_type_method(SomeType* obj, const OtherType* borrowed_ref);
```

## Phase 8 Implementation Checklist

This mapping table enables Phase 8 FFI bindings:

- [ ] **C ABI Layer** (bindings/c/wadjet_distributed.h):
  - [ ] Opaque handle types for C++ classes
  - [ ] Create/destroy factory functions
  - [ ] Accessor functions for struct fields
  - [ ] Memory management functions

- [ ] **Python Bindings** (bindings/python/):
  - [ ] ctypes structures matching C ABI
  - [ ] Type conversion helpers
  - [ ] Result unwrapping utilities
  - [ ] Context manager support for cleanup

- [ ] **Rust Bindings** (bindings/rust/src/lib.rs):
  - [ ] repr(C) struct definitions
  - [ ] unsafe function bindings
  - [ ] Safe wrapper types
  - [ ] Ownership management with Deref/Drop

## Summary

This mapping table provides:
✅ Consistent representation across C++, C, Python, and Rust  
✅ Clear ownership and memory management rules  
✅ Constitution Principle compliance (uniform FFI interface)  
✅ Foundation for Phase 8 FFI binding implementation  
✅ Type-safe conversions between languages  

The table enables seamless integration of distributed testing into multiple language ecosystems while maintaining consistent error handling, memory safety, and performance characteristics across all interfaces.

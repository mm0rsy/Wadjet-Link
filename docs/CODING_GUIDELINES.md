# Wadjet-Link Coding Guidelines

## C++ Standard

- **C++20** is required
- Use modern C++ features where they improve clarity
- Prefer standard library over custom implementations

## Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Namespace | `snake_case` | `wadjet::protocols` |
| Class/Struct | `PascalCase` | `CaptureSession` |
| Function | `snake_case` | `start_capture()` |
| Variable | `snake_case` | `packet_count` |
| Private member | `snake_case_` | `buffer_` |
| Constant | `UPPER_CASE` | `MAX_PACKET_SIZE` |
| Enum class | `PascalCase` | `MessageType::Request` |
| Macro | `WADJET_UPPER_CASE` | `WADJET_VERSION_STRING` |

## File Organization

```
include/wadjet/
  module/
    feature.hpp      # Public API
src/
  module/
    feature.cpp      # Implementation
    feature_impl.hpp # Internal headers (if needed)
tests/
  module/
    test_feature.cpp
```

## Header Files

```cpp
#pragma once

/// @file feature.hpp
/// @brief Brief description

#include "wadjet/other.hpp"  // Project headers first

#include <vector>            // Standard library
#include <string>

#include <third_party.h>     // Third party last

namespace wadjet::module {

/// @brief Class documentation
class Feature {
public:
    // Constructors
    // Public methods
    
private:
    // Private members with trailing underscore
    int count_;
};

}  // namespace wadjet::module
```

## Error Handling

- Use `std::expected<T, E>` (C++23) or `tl::expected` for recoverable errors
- Use exceptions only for programming errors (assertions)
- Never throw in destructors

```cpp
// Good
auto result = parse_packet(data);
if (!result) {
    return std::unexpected(result.error());
}

// Avoid raw error codes
int parse_packet(data, &output);  // Don't do this
```

## Memory Management

- Prefer value semantics
- Use `std::unique_ptr` for exclusive ownership
- Use `std::shared_ptr` sparingly
- Use `std::span` for non-owning views
- Never use raw `new`/`delete`

## Zero-Copy Packet Handling

```cpp
// PacketView is a non-owning view
class PacketView {
public:
    explicit PacketView(std::span<const std::byte> data);
    
    // No copy, just view
    std::span<const std::byte> payload() const;
};
```

## Threading

- Document thread safety in comments
- Use `const` for thread-safe read-only access
- Prefer message passing over shared state

## Documentation

- All public APIs must have Doxygen comments
- Use `@brief`, `@param`, `@return`, `@throws`
- Include usage examples for complex APIs

```cpp
/// @brief Start packet capture on the specified interface
/// @param interface Network interface name (e.g., "eth0")
/// @param filter Optional BPF filter expression
/// @return CaptureSession on success, error on failure
/// @throws std::invalid_argument if interface doesn't exist
///
/// @code
/// auto session = start_capture("eth0", "udp port 30490");
/// @endcode
auto start_capture(std::string_view interface, 
                   std::string_view filter = {}) -> Result<CaptureSession>;
```

## Testing

- One test file per source file
- Use descriptive test names
- Follow Arrange-Act-Assert pattern

```cpp
TEST(PacketParser, ParsesValidSOMEIPHeader) {
    // Arrange
    auto raw_data = load_test_pcap("someip_valid.pcap");
    
    // Act
    auto result = parse_someip_header(raw_data);
    
    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->service_id, 0x1234);
}
```

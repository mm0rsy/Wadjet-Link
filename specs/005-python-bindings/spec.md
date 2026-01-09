# Feature Specification: Python Bindings for Wadjet-Link

**Feature Branch**: `milestone/005-python-bindings`  
**Created**: 2026-01-09  
**Status**: Complete  
**Milestone**: M5 - Python Bindings

## User Scenarios & Testing

### User Story 1 - Python Packet Capture (Priority: P1)

As a Python test automation engineer, I need to capture packets from a network interface using Python, so that I can integrate Wadjet-Link into existing Python-based test frameworks without writing C++.

**Why this priority**: Python is the dominant language in test automation. Bindings unlock the entire Python ecosystem.

**Independent Test**: Write a Python script that captures packets on loopback using `wadjet.LiveCapture`, send a UDP packet, and verify it's captured.

**Acceptance Scenarios**:

1. **Given** Python with wadjet module installed, **When** I import wadjet, **Then** no errors occur
2. **Given** a LiveCapture context manager, **When** I use it with an interface name, **Then** packets are captured
3. **Given** captured packets, **When** I iterate through them, **Then** I get Packet objects with data and timestamp
4. **Given** a capture with BPF filter, **When** I specify filter="udp port 30490", **Then** only matching packets are captured
5. **Given** a captured packet, **When** I access packet.data, **Then** I get bytes object (Python buffer protocol)

---

### User Story 2 - Protocol Decoding in Python (Priority: P1)

As a data analyst using Jupyter notebooks, I need to decode packets into protocol headers (Ethernet, IPv4, SOME/IP) using Python, so that I can analyze automotive network traffic with pandas/matplotlib.

**Why this priority**: Data analysis and visualization are killer apps for Python bindings.

**Independent Test**: Capture a SOME/IP packet, decode it in Python, and verify the service ID is accessible as `header.service_id`.

**Acceptance Scenarios**:

1. **Given** a captured packet, **When** I call `wadjet.decode(packet)`, **Then** I get a decoded header object
2. **Given** a SOME/IP packet, **When** decoded, **Then** I can access header.service_id, header.method_id, header.client_id as Python integers
3. **Given** a DoIP packet, **When** decoded, **Then** I get DoIPHeader with payload_type and addresses
4. **Given** an Ethernet packet, **When** decoded, **Then** I get EthernetHeader with source_mac, dest_mac as Python strings
5. **Given** protocol stack decode, **When** I call `wadjet.protocols.ProtocolStack.from_packet(packet)`, **Then** I get all decoded layers (Ethernet → IPv4 → UDP → SOME/IP)

---

### User Story 3 - PCAP File Operations (Priority: P1)

As a Python script author, I need to read and write PCAP files using Python, so that I can process saved network captures without C++ code.

**Why this priority**: PCAP processing is a common workflow in network analysis.

**Independent Test**: Read a PCAP file with `wadjet.read_pcap()`, iterate through packets, and write filtered packets to a new PCAP file.

**Acceptance Scenarios**:

1. **Given** a PCAP file path, **When** I call `wadjet.read_pcap(path)`, **Then** I get an iterable of packets
2. **Given** a list of packets, **When** I call `wadjet.write_pcap(path, packets)`, **Then** a valid PCAP file is created
3. **Given** a PCAP reader, **When** I use it in a `with` statement, **Then** the file is automatically closed on exit
4. **Given** a PCAP writer, **When** I use context manager, **Then** the file is properly finalized
5. **Given** merge_pcaps([file1, file2], output), **When** called, **Then** packets from both files are merged into output file

---

### User Story 4 - pytest Integration (Priority: P2)

As a pytest user, I need Wadjet-Link matchers and assertions integrated with pytest, so that I can write tests using `assert` statements with helpful failure messages.

**Why this priority**: pytest is the standard Python test framework. Integration improves developer experience.

**Independent Test**: Write a pytest test using `wadjet.testing.assert_someip()` and verify test failure produces helpful output.

**Acceptance Scenarios**:

1. **Given** a pytest test function, **When** I use `assert_someip(packet, service_id=0x1234)`, **Then** pytest displays helpful error on mismatch
2. **Given** PacketMatcher class, **When** I create custom matchers, **Then** they work with pytest assert introspection
3. **Given** PacketTestRunner context manager, **When** used in pytest, **Then** capture sessions are setup/torn down correctly
4. **Given** composable matchers (AND/OR/NOT), **When** combined, **Then** pytest reports which part of the compound assertion failed

---

### User Story 5 - Type Hints and IDE Support (Priority: P2)

As a Python developer using VS Code/PyCharm, I need type hints for all Wadjet-Link APIs, so that I get autocompletion, type checking, and inline documentation.

**Why this priority**: Type hints improve developer productivity and reduce errors. Important for professional use.

**Independent Test**: Open a Python file in VS Code with Pylance, type `wadjet.`, and verify autocompletion shows all available APIs with type information.

**Acceptance Scenarios**:

1. **Given** type stub files (.pyi), **When** installed with the package, **Then** IDE provides autocompletion
2. **Given** a function with type hints, **When** I call it with wrong types, **Then** mypy/pyright reports type error
3. **Given** inline documentation in stubs, **When** I hover over a function in VS Code, **Then** I see the docstring
4. **Given** NumPy-compatible buffer protocol, **When** I pass packet.data to NumPy, **Then** type checkers accept it

### Edge Cases

- What happens when pybind11 raises an exception from C++ code (e.g., capture session fails to start)?
- How are Python errors (like passing wrong types) reported to the user?
- What if a packet contains invalid UTF-8 in a field the user tries to decode as string?
- How does the binding handle Python's GIL for concurrent packet capture?
- What happens when a user tries to modify an immutable PacketView from Python?

## Requirements

### Functional Requirements

- **FR-001**: System MUST use pybind11 to create Python bindings for C++ APIs
- **FR-002**: Bindings MUST provide `wadjet._wadjet` native module exposing all C++ functionality
- **FR-003**: Bindings MUST provide pure Python wrapper layer (`wadjet/__init__.py`) for Pythonic API
- **FR-004**: System MUST expose CaptureSession class with start(), stop(), and statistics()
- **FR-005**: System MUST provide LiveCapture context manager for Pythonic capture sessions
- **FR-006**: System MUST provide ReplayCapture context manager for PCAP file replay
- **FR-007**: System MUST expose Packet and PacketView classes with Python buffer protocol support
- **FR-008**: Packet.data MUST be zero-copy (returns memoryview/bytes without copying)
- **FR-009**: System MUST expose all protocol header classes (EthernetHeader, IPv4Header, UDPHeader, TCPHeader, SOMEIPHeader, DoIPHeader)
- **FR-010**: System MUST provide decode() function to decode packets into protocol headers
- **FR-011**: System MUST provide ProtocolStack class for multi-layer decoding
- **FR-012**: System MUST expose PcapReader and PcapWriter with context manager support
- **FR-013**: System MUST provide high-level functions: read_pcap(), write_pcap(), filter_pcap(), merge_pcaps()
- **FR-014**: System MUST expose timestamp utilities: bytes_to_hex(), hex_to_bytes()
- **FR-015**: System MUST provide testing module with assert_someip(), assert_doip(), PacketMatcher, PacketTestRunner
- **FR-016**: System MUST include type stub files (.pyi) for IDE autocompletion and type checking
- **FR-017**: System MUST provide pyproject.toml for modern pip installation
- **FR-018**: System MUST include pytest configuration (pytest.ini) for test discovery
- **FR-019**: All Python classes MUST have proper __repr__() for debugging
- **FR-020**: All context managers MUST properly cleanup resources on exception
- **FR-021**: Bindings MUST handle C++ exceptions and convert to Python exceptions
- **FR-022**: System MUST support Python 3.8+ (no Python 2 compatibility)
- **FR-023**: Module MUST be installable via `pip install .` from bindings/python directory
- **FR-024**: System MUST provide comprehensive examples (analyze_someip.py, live_capture.py, test_protocol.py)
- **FR-025**: All bindings MUST be thread-safe for Python's threading model

### Key Entities

- **wadjet._wadjet**: Native pybind11 module exposing C++ functionality
- **wadjet**: Pure Python package providing Pythonic wrappers
- **LiveCapture**: Context manager for packet capture sessions
- **ReplayCapture**: Context manager for PCAP file replay
- **Packet/PacketView**: Python-wrapped packet objects with buffer protocol
- **EthernetHeader, IPv4Header, etc.**: Python classes mirroring C++ protocol headers
- **ProtocolStack**: Multi-layer protocol decoder
- **PcapReader/PcapWriter**: PCAP file I/O with context managers
- **PacketMatcher**: Base class for pytest-compatible packet assertions
- **PacketTestRunner**: pytest fixture for automotive protocol testing

## Success Criteria

### Measurable Outcomes

- **SC-001**: All C++ core functionality is accessible from Python (capture, decode, PCAP I/O)
- **SC-002**: Python bindings pass comprehensive pytest test suite (tests/test_wadjet.py)
- **SC-003**: Type stubs provide 100% coverage of public APIs (verified by mypy --strict)
- **SC-004**: Zero-copy buffer protocol verified with NumPy integration (no memory doubling)
- **SC-005**: Context managers properly cleanup on exceptions (verified with pytest tests)
- **SC-006**: All examples run successfully (analyze_someip.py, live_capture.py, test_protocol.py)
- **SC-007**: Package installs successfully with `pip install .` on Ubuntu 20.04+
- **SC-008**: VS Code with Pylance provides autocompletion for all APIs
- **SC-009**: pytest integration provides helpful assertion error messages
- **SC-010**: Python bindings introduce <10% performance overhead compared to C++ API

## Assumptions

- Target Python version is 3.8 or later
- pybind11 is available and compatible with the C++20 codebase
- Users install bindings from source (no PyPI publishing initially)
- NumPy is available for users needing numerical processing (optional dependency)

## Dependencies

- **External**: Python 3.8+, pybind11, pytest (for tests)
- **Internal**: All previous milestones (M0-M4) for complete C++ API surface

## Out of Scope

- Python 2.x support (deprecated)
- PyPy compatibility (CPython only)
- Binary wheel distribution on PyPI
- Async/await support for packet capture (synchronous API only)
- Cython-based bindings (pybind11 only)
- Python-based protocol decoder implementations (all decoders in C++)

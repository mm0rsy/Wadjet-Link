# UDS (ISO 14229) Protocol Support

## Overview

Wadjet-Link provides comprehensive support for decoding and analyzing UDS (Unified Diagnostic Services) messages as defined in ISO 14229. UDS is the standard diagnostic protocol used in automotive ECUs for diagnostics, flash programming, and security access.

## Use Cases

UDS is essential for:

- **ECU Diagnostics**: Reading and clearing DTCs, reading sensor data, ECU identification
- **Flash Programming**: Downloading software updates to ECUs over DoIP/CAN
- **Security Access**: Authentication and authorization for protected services
- **End-of-Line Testing**: Factory calibration and programming
- **Tester Development**: Building diagnostic tools and scanners
- **Protocol Validation**: Verifying ECU compliance with UDS specifications

## Protocol Structure

### Service IDs (SID)

| Category | Service ID | Name | Description |
|----------|------------|------|-------------|
| **Diagnostic Management** | 0x10 | DiagnosticSessionControl | Switch diagnostic sessions |
| | 0x11 | ECUReset | Reset the ECU |
| | 0x27 | SecurityAccess | Authentication for protected services |
| | 0x28 | CommunicationControl | Control communication on a channel |
| | 0x3E | TesterPresent | Keep session alive |
| | 0x85 | ControlDTCSetting | Enable/disable DTC setting |
| **Data Access** | 0x22 | ReadDataByIdentifier | Read data by DID |
| | 0x23 | ReadMemoryByAddress | Read memory at address |
| | 0x2E | WriteDataByIdentifier | Write data by DID |
| | 0x3D | WriteMemoryByAddress | Write memory at address |
| **DTC Management** | 0x14 | ClearDiagnosticInformation | Clear DTCs |
| | 0x19 | ReadDTCInformation | Read DTC information |
| **I/O Control** | 0x2F | InputOutputControlByIdentifier | Control actuators |
| | 0x31 | RoutineControl | Execute predefined routines |
| **Upload/Download** | 0x34 | RequestDownload | Request download transfer |
| | 0x35 | RequestUpload | Request upload transfer |
| | 0x36 | TransferData | Transfer data block |
| | 0x37 | RequestTransferExit | End transfer |

### Message Format

**Request Format:**
```
+--------+------------------+
|  SID   |  Sub-Function    |
| 1 byte |   + Parameters   |
+--------+------------------+
```

**Positive Response Format:**
```
+----------+------------------+
| SID+0x40 |  Response Data   |
|  1 byte  |                  |
+----------+------------------+
```

**Negative Response Format:**
```
+------+---------------+------+
| 0x7F | Rejected SID  | NRC  |
+------+---------------+------+
```

### Common Negative Response Codes (NRC)

| NRC | Name | Description |
|-----|------|-------------|
| 0x10 | GeneralReject | Service rejected without specific cause |
| 0x11 | ServiceNotSupported | SID not supported |
| 0x12 | SubFunctionNotSupported | Sub-function not supported |
| 0x13 | IncorrectMessageLengthOrInvalidFormat | Wrong message length |
| 0x14 | ResponseTooLong | Response exceeds buffer |
| 0x22 | ConditionsNotCorrect | Pre-conditions not met |
| 0x31 | RequestOutOfRange | Parameter out of range |
| 0x33 | SecurityAccessDenied | Security not unlocked |
| 0x35 | InvalidKey | Wrong security key |
| 0x36 | ExceededNumberOfAttempts | Too many failed attempts |
| 0x72 | GeneralProgrammingFailure | Flash programming error |
| 0x78 | ResponsePending | ECU needs more time |

### Common Data Identifiers (DID)

| DID Range | Category |
|-----------|----------|
| 0x0000-0x00FF | Reserved |
| 0x0100-0xA5FF | Vehicle Manufacturer Specific |
| 0xF100-0xF17F | System Supplier Specific |
| 0xF180-0xF19F | Vehicle Identification |
| 0xF190 | VIN (Vehicle Identification Number) |
| 0xF195 | Software Version |
| 0xF1A0-0xFEFF | Reserved |

## Quick Start

### Basic Decoding

```cpp
#include <wadjet/protocols/uds/uds.hpp>

using namespace wadjet::protocols::uds;

// Decode UDS data (typically extracted from DoIP diagnostic message)
std::vector<std::uint8_t> uds_data = doip_msg.user_data;

UdsDecoder decoder;
auto result = decoder.decode(uds_data);

if (result.has_value()) {
    std::cout << "Service: " << service_id_string(result->header.service_id) << "\n";
    
    if (result->header.is_request()) {
        std::cout << "Type: Request\n";
    } else if (result->header.is_positive_response()) {
        std::cout << "Type: Positive Response\n";
    } else if (result->header.is_negative_response()) {
        std::cout << "Type: Negative Response\n";
        std::cout << "NRC: " << nrc_string(*result->header.negative_response_code) << "\n";
    }
}
```

### Handling Specific Services

```cpp
// Diagnostic Session Control
if (auto* dsc = result->as<DiagnosticSessionControlRequest>()) {
    std::cout << "Session: " << session_type_string(dsc->session_type) << "\n";
    std::cout << "Suppress response: " << std::boolalpha 
              << dsc->suppress_positive_response << "\n";
}

if (auto* dsc = result->as<DiagnosticSessionControlResponse>()) {
    std::cout << "Session: " << session_type_string(dsc->session_type) << "\n";
    std::cout << "P2 Server Max: " << dsc->p2_server_max << " ms\n";
    std::cout << "P2* Server Max: " << dsc->p2_star_server_max << " ms\n";
}

// Read Data By Identifier
if (auto* rdbi = result->as<ReadDataByIdentifierRequest>()) {
    std::cout << "DIDs requested:\n";
    for (const auto& did : rdbi->data_identifiers) {
        std::cout << "  0x" << std::hex << did.value << " ("
                  << data_identifier_string(did) << ")\n";
    }
}

if (auto* rdbi = result->as<ReadDataByIdentifierResponse>()) {
    std::cout << "DID records:\n";
    for (const auto& record : rdbi->records) {
        std::cout << "  DID 0x" << std::hex << record.did.value 
                  << ": " << record.data.size() << " bytes\n";
    }
}

// Security Access
if (auto* sa = result->as<SecurityAccessRequest>()) {
    if (sa->is_request_seed()) {
        std::cout << "Requesting seed for level " 
                  << static_cast<int>(sa->security_level()) << "\n";
    } else {
        std::cout << "Sending key for level " 
                  << static_cast<int>(sa->security_level()) 
                  << " (" << sa->key.size() << " bytes)\n";
    }
}

// Routine Control
if (auto* rc = result->as<RoutineControlRequest>()) {
    std::cout << "Routine: 0x" << std::hex << rc->routine_identifier.value << "\n";
    std::cout << "Type: " << routine_control_type_string(rc->routine_control_type) << "\n";
}

// Negative Response
if (auto* nrc = result->as<NegativeResponseMessage>()) {
    std::cout << "Service 0x" << std::hex << static_cast<int>(nrc->rejected_service_id)
              << " rejected with NRC: " << nrc_string(nrc->nrc) << "\n";
    std::cout << "Description: " << nrc_description(nrc->nrc) << "\n";
    
    // Check NRC category
    switch (classify_nrc(nrc->nrc)) {
        case NRCCategory::ServiceRelated:
            std::cout << "Category: Service related issue\n";
            break;
        case NRCCategory::SecurityRelated:
            std::cout << "Category: Security/access issue\n";
            break;
        case NRCCategory::TimingRelated:
            if (nrc->nrc == NRC::RequestCorrectlyReceivedResponsePending) {
                std::cout << "Category: Response pending - wait and retry\n";
            }
            break;
        // ... handle other categories
    }
}
```

### UDS over DoIP Integration

```cpp
#include <wadjet/protocols/doip.hpp>
#include <wadjet/protocols/uds/uds.hpp>

using namespace wadjet::protocols;
using namespace wadjet::protocols::uds;

// Decode DoIP packet
auto doip_result = decode_packet(ethernet_frame);

if (doip_result.has_layer<doip::DoIpHeader>()) {
    const auto* doip_header = doip_result.get_layer<doip::DoIpHeader>();
    
    // Check for diagnostic message
    if (std::holds_alternative<doip::DiagnosticMessagePayload>(doip_header->payload)) {
        const auto& diag = std::get<doip::DiagnosticMessagePayload>(doip_header->payload);
        
        std::cout << "DoIP Source: 0x" << std::hex << diag.source_address << "\n";
        std::cout << "DoIP Target: 0x" << std::hex << diag.target_address << "\n";
        
        // Decode embedded UDS
        UdsDecoder uds_decoder;
        auto uds_result = uds_decoder.decode(diag.user_data);
        
        if (uds_result.has_value()) {
            std::cout << "UDS Service: " 
                      << service_id_string(uds_result->header.service_id) << "\n";
            // Process UDS message...
        }
    }
}
```

## Testing with Matchers

Wadjet-Link provides gMock-compatible matchers for testing UDS traffic:

```cpp
#include <wadjet/testing/matchers.hpp>
using namespace wadjet::testing;
using namespace wadjet::protocols::uds;

// Test for UDS request/response
EXPECT_THAT(uds_data, IsUdsRequest());
EXPECT_THAT(uds_data, IsUdsResponse());
EXPECT_THAT(uds_data, IsUdsPositiveResponse());
EXPECT_THAT(uds_data, IsUdsNegativeResponse());

// Test for specific service
EXPECT_THAT(uds_data, HasUdsService(ServiceID::ReadDataByIdentifier));

// Convenience service matchers
EXPECT_THAT(uds_data, IsUdsDiagnosticSessionControl());
EXPECT_THAT(uds_data, IsUdsReadDataByIdentifier());
EXPECT_THAT(uds_data, IsUdsSecurityAccess());
EXPECT_THAT(uds_data, IsUdsTesterPresent());
EXPECT_THAT(uds_data, IsUdsRoutineControl());

// Test for specific DID
EXPECT_THAT(uds_data, HasUdsDID(0xF190));  // VIN
EXPECT_THAT(uds_data, HasUdsVinDID());     // Convenience for VIN

// Test for session type
EXPECT_THAT(uds_data, HasUdsSessionType(SessionType::ExtendedDiagnosticSession));
EXPECT_THAT(uds_data, IsUdsProgrammingSession());
EXPECT_THAT(uds_data, IsUdsExtendedSession());

// Test for NRC
EXPECT_THAT(uds_data, HasUdsNRC(NRC::SecurityAccessDenied));
EXPECT_THAT(uds_data, HasUdsSecurityAccessDenied());
EXPECT_THAT(uds_data, HasUdsResponsePending());
EXPECT_THAT(uds_data, HasUdsConditionsNotCorrect());
```

## Scenario Testing

Define UDS test scenarios in YAML:

```yaml
name: "UDS Diagnostic Session Test"
description: "Verify ECU responds to session control requests"
timeout: 10000

steps:
  - capture:
      interface: "eth0"
      filter: "udp port 13400"

  - send:
      interface: "eth0"
      pcap_file: "uds_session_request.pcap"

  - expect:
      doip:
        payload_type: DiagnosticMessage
      uds:
        service: DiagnosticSessionControl
        is_request: false
        is_negative_response: false
        session_type: ExtendedDiagnosticSession
      within: 1000
      description: "ECU accepts extended session"

  - send:
      interface: "eth0"  
      pcap_file: "uds_read_vin.pcap"

  - expect:
      uds:
        service: ReadDataByIdentifier
        is_request: false
        data_identifiers: [0xF190]
      within: 500
      description: "ECU returns VIN"
```

## NRC Classification

The decoder provides NRC classification helpers:

```cpp
NRC nrc = NRC::SecurityAccessDenied;

// Get category
NRCCategory cat = classify_nrc(nrc);
switch (cat) {
    case NRCCategory::ServiceRelated:
        // Service not supported or invalid format
        break;
    case NRCCategory::SecurityRelated:
        // Security access denied, invalid key, etc.
        break;
    case NRCCategory::TimingRelated:
        // Busy, response pending, time delay
        break;
    case NRCCategory::SequenceRelated:
        // Request sequence error
        break;
    case NRCCategory::ConditionRelated:
        // Conditions not correct
        break;
    case NRCCategory::UploadDownload:
        // Transfer errors
        break;
    case NRCCategory::VehicleManufacturerSpecific:
        // OEM-specific codes 0x80-0xFF
        break;
    case NRCCategory::Other:
        // Reserved or unknown
        break;
}

// Check if retry might help
if (is_temporary_nrc(nrc)) {
    std::cout << "Temporary condition - retry may succeed\n";
}

// Check if security-related
if (is_security_nrc(nrc)) {
    std::cout << "Security issue - check authentication\n";
}
```

## Type Conversions

Convert between UDS types and raw values:

```cpp
// Service ID
ServiceID sid = ServiceID::ReadDataByIdentifier;
std::string name = service_id_string(sid);    // "ReadDataByIdentifier"
std::uint8_t raw = static_cast<std::uint8_t>(sid);  // 0x22

// Session Type
SessionType session = SessionType::ProgrammingSession;
std::string session_name = session_type_string(session);  // "ProgrammingSession"

// Reset Type  
ResetType reset = ResetType::HardReset;
std::string reset_name = reset_type_string(reset);  // "HardReset"

// NRC
NRC nrc = NRC::ConditionsNotCorrect;
std::string nrc_name = nrc_string(nrc);        // "ConditionsNotCorrect"
std::string nrc_desc = nrc_description(nrc);   // Full description

// Data Identifier
DataIdentifier did{0xF190};
std::string did_name = data_identifier_string(did);  // "VIN" or hex

// Routine Identifier
RoutineIdentifier rid{0xFF00};
std::string rid_name = routine_identifier_string(rid);  // Known name or hex
```

## API Reference

### Core Types

| Type | Description |
|------|-------------|
| `ServiceID` | Enum of all UDS service IDs (0x10-0x3E) |
| `NRC` | Enum of all negative response codes |
| `SessionType` | Diagnostic session types |
| `ResetType` | ECU reset types |
| `SecurityAccessType` | Security access sub-functions |
| `RoutineControlType` | Routine control sub-functions |
| `DataIdentifier` | Data identifier (DID) wrapper |
| `RoutineIdentifier` | Routine identifier (RID) wrapper |

### Decoder

| Method | Description |
|--------|-------------|
| `UdsDecoder::decode(span)` | Decode UDS message from bytes |
| `UdsDecoder::is_request(span)` | Check if data is a request |
| `UdsDecoder::is_positive_response(span)` | Check if data is positive response |
| `UdsDecoder::is_negative_response(span)` | Check if data is negative response |

### Helper Functions

| Function | Description |
|----------|-------------|
| `service_id_string(sid)` | Service ID to string |
| `session_type_string(st)` | Session type to string |
| `reset_type_string(rt)` | Reset type to string |
| `nrc_string(nrc)` | NRC to string |
| `nrc_description(nrc)` | NRC detailed description |
| `classify_nrc(nrc)` | Get NRC category |
| `is_temporary_nrc(nrc)` | Check if NRC is temporary |
| `is_security_nrc(nrc)` | Check if NRC is security-related |

## Files

| File | Purpose |
|------|---------|
| `include/wadjet/protocols/uds/uds.hpp` | Main header (includes all) |
| `include/wadjet/protocols/uds/uds_types.hpp` | Core type definitions |
| `include/wadjet/protocols/uds/uds_nrc.hpp` | NRC definitions and helpers |
| `include/wadjet/protocols/uds/uds_services.hpp` | Service request/response structures |
| `src/protocols/uds_decoder.cpp` | Decoder implementation |
| `tests/protocols/test_uds.cpp` | Unit tests |

## References

- ISO 14229-1: Unified Diagnostic Services (UDS) - Part 1: Application layer
- ISO 14229-3: UDS on CAN implementation (UDSonCAN)
- ISO 14229-5: UDS on IP implementation (UDSonIP)
- ISO 13400-2: DoIP transport protocol
- ISO 15765-2: Network layer services (CAN Transport Protocol)

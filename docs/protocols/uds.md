# UDS (ISO 14229) Protocol Support

**Status**: ✅ Complete (M13)  
**Standard**: [ISO 14229-1](https://www.iso.org/standard/72439.html)  
**Implementation**: `include/wadjet/protocols/uds/`

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

## Negative Response Codes (NRC)

UDS defines complete set of NRC values for error reporting. Each NRC indicates specific issue requiring different recovery.

### NRC Categories

#### General Errors (0x00-0x0F)

| Code | Name | Classification | Meaning | Recovery |
|------|------|---|---------|----------|
| 0x00 | — | — | Not used | — |
| 0x10 | generalReject | Temporary | Generic error condition | Retry request |
| 0x11 | serviceNotSupported | Permanent | Service not implemented | Use different service |
| 0x12 | subFunctionNotSupported | Permanent | Sub-function invalid | Check sub-function code |
| 0x13 | incorrectMessageLengthOrInvalidFormat | Permanent | Message format wrong | Verify request format |
| 0x14 | responseTooBig | Temporary | Response too large for channel | Request partial data |
| 0x21 | busyRepeatRequest | Temporary | ECU busy processing | Retry after delay |
| 0x24 | conditionsNotCorrect | Temporary | Request conditions not met | Wait and retry |
| 0x25 | requestSequenceError | Temporary | Wrong request order | Reorder requests |
| 0x26 | noAccessToSubnet | Permanent | No network access | Check routing |
| 0x31 | requestOutOfRange | Permanent | Request parameter invalid | Check ranges |
| 0x33 | securityAccessDenied | Permanent | Authentication failed | Verify credentials |

#### Session/State Errors (0x20-0x3F)

| Code | Name | Classification | Meaning | Recovery |
|------|------|---|---------|----------|
| 0x22 | conditionsNotCorrect | Temporary | Current state prevents operation | Change session/state |
| 0x24 | requestSequenceError | Temporary | Requests out of sequence | Follow correct sequence |
| 0x25 | noAccessToSubnet | Permanent | No access to requested subnet | Verify network access |
| 0x26 | invalidKey | Permanent | Security key rejected | Provide correct key |
| 0x27 | exceedNumberOfAttempts | Permanent | Too many auth attempts | Wait for timeout |
| 0x28 | requiredTimeDelayNotExpired | Temporary | Minimum time not elapsed | Wait longer |

#### Data Errors (0x30-0x3F)

| Code | Name | Classification | Meaning | Recovery |
|------|------|---|---------|----------|
| 0x31 | requestOutOfRange | Permanent | Address/data invalid | Verify ranges |
| 0x32 | securityAccessDenied | Permanent | Not authenticated | Perform security access |
| 0x33 | invalidDataFormat | Permanent | Data format wrong | Check format |
| 0x34 | dataLengthTooLong | Permanent | Data exceeds max length | Send smaller data |
| 0x35 | dataLengthTooShort | Permanent | Data too short | Send complete data |

#### Storage/Memory Errors (0x40-0x4F)

| Code | Name | Classification | Meaning | Recovery |
|------|------|---|---------|----------|
| 0x40 | generalProgrammingFailure | Permanent | Flash write failed | Erase and retry |
| 0x41 | wrongBlockSequenceCounter | Temporary | Data block out of order | Resend blocks in order |
| 0x42 | requestBlockTransferSuspended | Temporary | Transfer paused | Resume transfer |
| 0x43 | illegalBlockTransferType | Permanent | Invalid transfer type | Use correct type |
| 0x44 | blockTransferDataChecksumError | Temporary | Checksum mismatch | Resend block |
| 0x45 | requestCorrectlyReceivedButResponsePending | Temporary | Processing ongoing | Retry later |
| 0x46 | subFunctionNotSupportedInActiveSession | Permanent | Invalid for this session | Switch sessions |
| 0x47 | serviceNotSupportedInActiveSession | Permanent | Service not in session | Enter correct session |
| 0x48 | addressAndDataLengthFormatIdentifierInvalid | Permanent | Format identifier invalid | Use correct format |
| 0x49 | addressAndLengthFormatIdentifierNotSupported | Permanent | Format not supported | Use supported format |
| 0x4A | subFunctionNotSupportedInCurrentPhysicalState | Temporary | State incompatible | Change physical state |
| 0x4C | requestSequenceErrorOrNoProgressIndicator | Temporary | Progress indicator issue | Provide/update indicator |
| 0x4D | requestVehicleManufacturerECUSoftwareNumber | Permanent | Invalid software number | Verify software version |
| 0x4E | requestVehicleManufacturerECUSoftwareVersionNumber | Permanent | Invalid version format | Check version format |

#### Security Errors (0x50-0x5F)

| Code | Name | Classification | Meaning | Recovery |
|------|------|---|---------|----------|
| 0x50 | enableRxAndTx | — | Message flow enable | — |
| 0x51 | enableRxAndDisableTx | — | Message flow control | — |
| 0x52 | disableRxAndEnableTx | — | Message flow control | — |
| 0x53 | disableRxAndTx | — | Message flow disable | — |
| 0x61 | unsupportedNegativeResponseCode | Permanent | Response not supported | Use standard codes |
| 0x62 | securityAccessRequestNumberOutOfSequence | Temporary | Auth sequence invalid | Start auth from beginning |
| 0x63 | lengthOfSecurityAccessDataRecordTooLong | Permanent | Auth data too large | Use valid length |
| 0x64 | authenticationFailed | Permanent | Auth verification failed | Retry with correct key |
| 0x70 | uploadDownloadNotAccepted | Temporary | Transfer not allowed now | Retry later |
| 0x71 | transferDataStartedWithoutRequestDownload | Permanent | Sequence error | Start with RequestDownload |
| 0x72 | transferDataLengthDoesNotMatchLengthSpecifier | Permanent | Length mismatch | Match specified length |
| 0x73 | transferDataStartedWithoutRequestUpload | Permanent | Sequence error | Start with RequestUpload |
| 0x74 | requestTransferExitWithoutActiveTransfer | Permanent | No active transfer | Start transfer first |
| 0x75 | requestTransferExitNegativeResponseNotAllowed | Permanent | Exit not allowed in error | Complete transfer properly |
| 0x76 | requestFileDataDoesNotExist | Permanent | File not found | Check filename |

#### Common Codes (0x80+)

| Code | Name | Classification | Meaning | Recovery |
|------|------|---|---------|----------|
| 0x92 | failedToEnableRxTx | Temporary | Communication enable failed | Retry enable |
| 0x93 | failedToDisableRxTx | Temporary | Communication disable failed | Retry disable |
| 0x94 | failedToEnableRxAndDisableTx | Temporary | Flow control failed | Retry command |
| 0x95 | failedToDisableRxAndEnableTx | Temporary | Flow control failed | Retry command |
| 0x96 | failedToDisableRxAndTx | Temporary | Communication disable failed | Retry command |
| 0xF0 | temporaryNegativeResponse | Temporary | Transient condition | Retry request |
| 0xF1 | tempFailureServiceSpecificToUdsService | Temporary | Service-specific timeout | Retry request |
| 0xF2 | permFailureServiceSpecificToUdsService | Permanent | Service failed | Contact manufacturer |

### NRC Classification Examples

**Temporary NRCs** (can recover by retrying):
- 0x10: generalReject
- 0x21: busyRepeatRequest
- 0x22: conditionsNotCorrect
- 0x24: requestSequenceError
- 0x25: requiredTimeDelayNotExpired
- 0x41: wrongBlockSequenceCounter
- 0x44: blockTransferDataChecksumError
- 0x45: requestCorrectlyReceivedButResponsePending
- 0xF0: temporaryNegativeResponse

**Permanent NRCs** (retry won't help):
- 0x11: serviceNotSupported
- 0x12: subFunctionNotSupported
- 0x13: incorrectMessageLengthOrInvalidFormat
- 0x31: requestOutOfRange
- 0x32: securityAccessDenied
- 0x33: invalidDataFormat
- 0x40: generalProgrammingFailure
- 0x62: securityAccessRequestNumberOutOfSequence
- 0x64: authenticationFailed

### NRC Handling Strategy

| NRC Type | Action | Timeout | Retry |
|----------|--------|---------|-------|
| Temporary | Wait and retry | 100ms-1s | Yes (3-5×) |
| Permanent | Log error | — | No |
| Security | Check credentials | — | After delay |
| State | Change state/session | — | Conditional |
| Transfer | Resume transfer | Per spec | Yes |

## Code Examples

### Parsing UDS Message with NRC

```cpp
#include <wadjet/protocols/uds/uds.hpp>
using namespace wadjet::protocols::uds;

auto result = UdsDecoder::decode(packet_data);
if (result) {
    const auto& msg = result.value();
    
    if (msg.is_negative_response()) {
        uint8_t nrc = msg.nrc_code();
        std::cout << "Error: " << nrc_string(nrc) << " (" 
                  << nrc_description(nrc) << ")\n";
        
        if (is_temporary_nrc(nrc)) {
            std::cout << "Retryable - waiting...\n";
        } else {
            std::cout << "Fatal error\n";
        }
    }
}
```

### NRC Classification

```cpp
uint8_t nrc = received_response[1];

if (is_temporary_nrc(nrc)) {
    // Implement exponential backoff
    retry_after_delay(100ms * (1 << retry_count));
} else if (is_security_nrc(nrc)) {
    // Request new security key
    perform_security_access();
} else {
    // Permanent error - log and abort
    log_permanent_error(nrc);
}
```

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

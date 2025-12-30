# UDS Protocol Test Samples

This directory contains UDS (ISO 14229) protocol test samples for regression testing.

## Directory Structure

```
uds/
├── session_control/      # Diagnostic session control sequences
├── security_access/      # Security access unlock sequences
├── data_transfer/        # Read/Write DID sequences
├── routine_control/      # Routine control sequences
├── flash_sequences/      # Flash programming sequences
├── oem_patterns/         # OEM-specific ECU patterns
└── negative_responses/   # NRC test cases
```

## Sample Descriptions

### Session Control Samples
- `default_to_extended.pcap` - Transition from default to extended session
- `extended_to_programming.pcap` - Programming session entry sequence
- `session_timeout.pcap` - S3 timeout scenario

### Security Access Samples
- `security_level_1_unlock.pcap` - Level 1 seed/key exchange
- `security_level_3_unlock.pcap` - Level 3 seed/key exchange  
- `invalid_key_lockout.pcap` - Lockout after failed attempts

### Data Transfer Samples
- `read_vin.pcap` - Read VIN (DID 0xF190)
- `read_ecu_serial.pcap` - Read ECU serial number (DID 0xF18C)
- `write_programming_date.pcap` - Write programming date (DID 0xF199)
- `multi_did_read.pcap` - Multiple DID read in single request

### Routine Control Samples
- `erase_memory.pcap` - Memory erase routine (0xFF00)
- `check_programming.pcap` - Check programming dependencies (0x0202)
- `request_results.pcap` - Request routine results

### Flash Programming Samples
- `download_sequence.pcap` - Complete download sequence
- `upload_sequence.pcap` - Complete upload sequence
- `transfer_abort.pcap` - Aborted transfer recovery

### Known ECU Patterns
- `bosch_edc17.pcap` - Bosch EDC17 engine ECU
- `continental_abs.pcap` - Continental ABS/ESP ECU
- `denso_airbag.pcap` - Denso airbag ECU

## Generating Test Captures

### Using Wadjet-Link
```cpp
#include "wadjet/testing/generators.hpp"
using namespace wadjet::testing::generators;

PacketGenerator gen(42);

// Generate DoIP+UDS packet
auto packet = gen.uds_session_control_packet(
    protocols::uds::SessionType::ExtendedDiagnosticSession,
    0x0E00,  // Tester
    0x0001   // ECU
);
```

### From Real ECU Traffic
```bash
# Capture UDS over DoIP
tcpdump -i eth0 -w uds_capture.pcap tcp port 13400

# Capture UDS over CAN (using can-utils)
candump can0 -L > uds_can.log
```

## Using Samples in Tests

```cpp
#include "wadjet/io/pcap_reader.hpp"

TEST(UdsRegressionTest, ReadVin) {
    PcapReader reader("pcap_samples/uds/data_transfer/read_vin.pcap");
    
    for (const auto& packet : reader) {
        auto decoded = decoder.decode(packet);
        // Verify expected UDS messages
    }
}
```

## Sample Data Formats

### Binary Sample Format
Samples can be stored as raw binary data for unit tests:

```cpp
// DID 0xF190 (VIN) read request
const std::vector<uint8_t> read_vin_request = {
    // DoIP header (8 bytes)
    0x02, 0xFD,           // Protocol version + inverse
    0x80, 0x01,           // Diagnostic message type
    0x00, 0x00, 0x00, 0x05, // Payload length
    // DoIP payload
    0x0E, 0x00,           // Source address (tester)
    0x00, 0x01,           // Target address (ECU)
    // UDS payload
    0x22,                 // ReadDataByIdentifier
    0xF1, 0x90            // DID: VIN
};

// VIN response
const std::vector<uint8_t> read_vin_response = {
    // DoIP header (8 bytes)
    0x02, 0xFD,
    0x80, 0x01,
    0x00, 0x00, 0x00, 0x18, // 24 bytes: 4 + 3 + 17 (addresses + UDS header + VIN)
    // DoIP payload
    0x00, 0x01,           // Source (ECU)
    0x0E, 0x00,           // Target (tester)
    // UDS payload
    0x62,                 // Positive response (0x22 + 0x40)
    0xF1, 0x90,           // DID
    'W', 'A', 'U', 'Z', 'Z', 'Z', '8', 'V', '5', 'K', 'A', '0', '1', '2', '3', '4', '5'
};
```

## Test Coverage Matrix

| Service | Request | Positive | NRC |
|---------|---------|----------|-----|
| 0x10 DSC | ✓ | ✓ | ✓ |
| 0x11 ECUReset | ✓ | ✓ | ✓ |
| 0x27 SecurityAccess | ✓ | ✓ | ✓ |
| 0x22 RDBI | ✓ | ✓ | ✓ |
| 0x2E WDBI | ✓ | ✓ | ✓ |
| 0x31 RoutineControl | ✓ | ✓ | ✓ |
| 0x34 RequestDownload | ✓ | ✓ | ✓ |
| 0x36 TransferData | ✓ | ✓ | ✓ |
| 0x37 RequestTransferExit | ✓ | ✓ | ✓ |
| 0x3E TesterPresent | ✓ | ✓ | - |
| 0x14 ClearDTC | ✓ | ✓ | ✓ |
| 0x19 ReadDTC | ✓ | ✓ | ✓ |

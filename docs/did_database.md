# UDS Data Identifier (DID) Database

This document describes the DID database format and common DIDs used in UDS (ISO 14229).

## Overview

Data Identifiers (DIDs) are 16-bit values used to identify specific data records in UDS.
They are used with services like `ReadDataByIdentifier` (0x22) and `WriteDataByIdentifier` (0x2E).

## DID Ranges (ISO 14229)

| Range | Description |
|-------|-------------|
| 0x0000 - 0x00FF | ISO SAE Reserved |
| 0x0100 - 0xA5FF | Vehicle Manufacturer Specific |
| 0xA600 - 0xA7FF | Reserved for legislation |
| 0xA800 - 0xACFF | Supplier Specific |
| 0xAD00 - 0xAFFF | Reserved |
| 0xB000 - 0xB1FF | EOBD/OBD PIDs |
| 0xB200 - 0xBFFF | Reserved |
| 0xC000 - 0xC2FF | Reserved |
| 0xC300 - 0xCFFF | Reserved |
| 0xD000 - 0xDFFF | Reserved |
| 0xE000 - 0xE0FF | Externally defined |
| 0xE100 - 0xEFFF | Reserved |
| 0xF000 - 0xF0FF | Network config |
| 0xF100 - 0xF1FF | Identification |
| 0xF200 - 0xF2FF | Periodic |
| 0xF300 - 0xF3FF | Dynamically defined |
| 0xF400 - 0xF5FF | OBD |
| 0xF600 - 0xF6FF | OBD Monitor |
| 0xF700 - 0xF7FF | OBD Info Type |
| 0xF800 - 0xF8FF | Tester |
| 0xF900 - 0xF9FF | ECU Timing |
| 0xFA00 - 0xFBFF | Reserved |
| 0xFC00 - 0xFDFF | System Supplier Specific |
| 0xFE00 - 0xFEFF | Reserved |
| 0xFF00 - 0xFFFF | Reserved |

## Common Standard DIDs

### Identification DIDs (0xF1xx)

| DID | Name | Description | Format |
|-----|------|-------------|--------|
| 0xF186 | ActiveDiagnosticSessionDataIdentifier | Current session type | 1 byte |
| 0xF187 | VehicleManufacturerSparePartNumber | Spare part number | ASCII string |
| 0xF188 | VehicleManufacturerECUSoftwareNumber | SW number | ASCII string |
| 0xF189 | VehicleManufacturerECUSoftwareVersionNumber | SW version | ASCII string |
| 0xF18A | SystemSupplierIdentifier | Supplier ID | ASCII string |
| 0xF18B | ECUManufacturingDate | Manufacturing date | BCD YY MM DD |
| 0xF18C | ECUSerialNumber | Serial number | ASCII string |
| 0xF190 | VIN | Vehicle Identification Number | 17 ASCII chars |
| 0xF191 | VehicleManufacturerECUHardwareNumber | HW number | ASCII string |
| 0xF192 | SystemSupplierECUHardwareNumber | Supplier HW number | ASCII string |
| 0xF193 | SystemSupplierECUHardwareVersionNumber | HW version | ASCII string |
| 0xF194 | SystemSupplierECUSoftwareNumber | Supplier SW number | ASCII string |
| 0xF195 | SystemSupplierECUSoftwareVersionNumber | SW version | ASCII string |
| 0xF197 | SystemNameOrEngineType | System name | ASCII string |
| 0xF198 | RepairShopCodeOrTesterSerialNumber | Workshop code | ASCII string |
| 0xF199 | ProgrammingDate | Flash date | BCD YY MM DD |
| 0xF19E | TesterSerialNumber | Tester serial | ASCII string |

### System DIDs (0xF0xx)

| DID | Name | Description |
|-----|------|-------------|
| 0xF010 | ECUIdentification | ECU ID data |
| 0xF011 | ECUBoardInfo | Board information |
| 0xF012 | ECUMemoryInfo | Memory statistics |
| 0xF020 | ActiveSecurityLevel | Security unlock status |
| 0xF021 | SecurityAttemptCounter | Failed attempts |

### Timing DIDs (0xF9xx)

| DID | Name | Description |
|-----|------|-------------|
| 0xF900 | P2ServerMax | Current P2 timing |
| 0xF901 | P2StarServerMax | Extended response pending |
| 0xF902 | S3Server | Session timeout |

## DID Database File Format

Wadjet-Link supports loading DID definitions from JSON or YAML files:

### JSON Format

```json
{
  "version": "1.0",
  "name": "OEM Standard DIDs",
  "dids": [
    {
      "id": "0xF190",
      "name": "VIN",
      "description": "Vehicle Identification Number",
      "length": 17,
      "format": "ascii",
      "access": ["read"],
      "security_level": 0
    },
    {
      "id": "0xF199",
      "name": "ProgrammingDate",
      "description": "Flash programming date",
      "length": 4,
      "format": "bcd_date",
      "access": ["read", "write"],
      "security_level": 1
    },
    {
      "id": "0x1234",
      "name": "CustomCounter",
      "description": "OEM-specific counter",
      "length": 2,
      "format": "uint16_be",
      "scaling": {
        "factor": 0.1,
        "offset": 0,
        "unit": "counts"
      },
      "access": ["read"],
      "security_level": 0
    }
  ]
}
```

### YAML Format

```yaml
version: "1.0"
name: "OEM Standard DIDs"
dids:
  - id: 0xF190
    name: VIN
    description: Vehicle Identification Number
    length: 17
    format: ascii
    access: [read]
    security_level: 0
    
  - id: 0xF199
    name: ProgrammingDate
    description: Flash programming date
    length: 4
    format: bcd_date
    access: [read, write]
    security_level: 1
    
  - id: 0x1234
    name: CustomCounter
    description: OEM-specific counter
    length: 2
    format: uint16_be
    scaling:
      factor: 0.1
      offset: 0
      unit: counts
    access: [read]
    security_level: 0
```

## Format Types

| Format | Description | Example |
|--------|-------------|---------|
| `ascii` | ASCII string | "WVWZZZ3CZWE123456" |
| `hex` | Raw hex bytes | 0x12 0x34 0x56 |
| `uint8` | 8-bit unsigned | 255 |
| `uint16_be` | 16-bit big-endian | 65535 |
| `uint32_be` | 32-bit big-endian | 4294967295 |
| `int8` | 8-bit signed | -128 to 127 |
| `int16_be` | 16-bit signed BE | -32768 to 32767 |
| `bcd_date` | BCD date YY MM DD | 24 01 15 = 2024-01-15 |
| `bcd_time` | BCD time HH MM SS | 14 30 45 = 14:30:45 |
| `enum` | Enumerated value | Defined in field |

## Using DID Database in Code

### C++ API

```cpp
#include <wadjet/protocols/uds/did_database.hpp>

using namespace wadjet::protocols::uds;

// Load DID database
DidDatabase db;
db.load_from_file("oem_dids.json");

// Lookup DID
if (auto did = db.lookup(0xF190)) {
    std::cout << "Name: " << did->name << "\n";
    std::cout << "Length: " << did->length << "\n";
}

// Decode DID value
auto decoded = db.decode_value(0xF190, vin_bytes);
std::cout << "VIN: " << decoded.as_string() << "\n";

// Check if DID requires security
if (db.requires_security(0xF199, WriteAccess)) {
    std::cout << "Security level required: " 
              << db.security_level(0xF199) << "\n";
}
```

### Python API

```python
from wadjet import DidDatabase

# Load DID database
db = DidDatabase()
db.load("oem_dids.yaml")

# Lookup DID
did = db.lookup(0xF190)
if did:
    print(f"Name: {did.name}")
    print(f"Length: {did.length}")

# Decode DID value
decoded = db.decode(0xF190, vin_bytes)
print(f"VIN: {decoded.as_string()}")

# List all DIDs in a range
for did in db.range(0xF100, 0xF1FF):
    print(f"0x{did.id:04X}: {did.name}")
```

## OEM-Specific DID Documentation

### Automotive OEM Patterns

Different OEMs use different DID ranges and formats:

#### European OEMs
- 0x0100-0x01FF: Powertrain
- 0x0200-0x02FF: Chassis
- 0x0300-0x03FF: Body
- 0x0400-0x04FF: Safety systems

#### US OEMs
- 0x1000-0x1FFF: Engine
- 0x2000-0x2FFF: Transmission
- 0x3000-0x3FFF: ABS/ESC
- 0x4000-0x4FFF: Airbag

### Creating OEM DID Files

1. Start with ISO 14229 standard DIDs
2. Add OEM-specific identification DIDs
3. Document data format and scaling
4. Specify required security levels
5. Test with actual ECU data

## Common DID Operations

### Reading VIN

```cpp
// Request: 22 F1 90
// Response: 62 F1 90 <17 bytes VIN>

auto request = uds::read_data_by_identifier({DataIdentifier{0xF190}});
// Send and receive...
auto vin = response.payload_as_string(3, 17);  // Skip header
```

### Reading Multiple DIDs

```cpp
// Request: 22 F1 90 F1 86 F1 8C
// Response contains all three DIDs concatenated

auto request = uds::read_data_by_identifier({
    DataIdentifier{0xF190},  // VIN
    DataIdentifier{0xF186},  // Active session
    DataIdentifier{0xF18C}   // Serial number
});
```

### Writing a DID

```cpp
// Request: 2E F1 99 24 01 15 (write programming date 2024-01-15)
// Response: 6E F1 99

auto request = uds::write_data_by_identifier(
    DataIdentifier{0xF199},
    {0x24, 0x01, 0x15}  // BCD date
);
```

## See Also

- [ISO 14229-1](https://www.iso.org/standard/72439.html) - UDS standard
- [UDS Protocol Overview](uds_protocol.md) - Protocol documentation
- [Security Access](security_access.md) - Security levels and unlock procedures

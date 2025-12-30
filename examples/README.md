# Wadjet-Link Examples

This directory contains examples demonstrating Wadjet-Link features.

## Contents

| Directory | Description |
|-----------|-------------|
| [scenarios/](scenarios/) | YAML/JSON test scenarios for `wadjet-run` CLI |

## Quick Start

### Running Test Scenarios

```bash
# Dry-run all example scenarios (validates without live capture)
wadjet-run --dry-run --dir examples/scenarios/

# List available scenarios
wadjet-run --list --dir examples/scenarios/

# Run a specific scenario
wadjet-run examples/scenarios/someip_sd_test.yaml -i eth0
```

### C++ Integration Examples

See the main [README.md](../README.md) for C++ code examples using:

- `CaptureSession` for live packet capture
- `PcapReader`/`PcapWriter` for file I/O
- Protocol decoders (SOME/IP, DoIP, etc.)
- GoogleTest matchers and fixtures

## Adding Examples

When adding new examples:

1. Place test scenarios in `scenarios/`
2. Use descriptive filenames
3. Include comments explaining the test purpose
4. Add appropriate tags for filtering

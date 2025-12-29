# PCAP Samples

This directory contains sample packet captures for testing and regression.

## Directory Structure

```
pcap_samples/
├── someip/           # SOME/IP protocol samples
├── doip/             # DoIP protocol samples
├── gptp/             # gPTP timing samples
├── malformed/        # Intentionally malformed packets for fuzz testing
└── regression/       # Regression test fixtures
```

## Adding New Samples

1. Use descriptive filenames: `someip_service_discovery_offer.pcap`
2. Add a comment in the test that uses the file
3. Keep files small (< 1MB) for fast CI

## Generating Test Captures

```bash
# Capture SOME/IP traffic
tcpdump -i eth0 -w someip_sample.pcap udp port 30490

# Capture DoIP traffic  
tcpdump -i eth0 -w doip_sample.pcap tcp port 13400
```

# DDS/RTPS PCAP Samples

This directory contains sample PCAP captures of DDS/RTPS traffic for regression testing
and protocol analysis development.

## Expected Sample Files

The following sample captures should be added for comprehensive testing:

### FastDDS Samples
- `fastdds_discovery.pcap` — SPDP/SEDP discovery sequence from eProsima Fast DDS
- `fastdds_pubsub.pcap` — Publisher/subscriber data exchange
- `fastdds_reliable.pcap` — Reliable QoS communication with HEARTBEAT/ACKNACK

### CycloneDDS Samples  
- `cyclonedds_discovery.pcap` — Discovery sequence from Eclipse CycloneDDS
- `cyclonedds_best_effort.pcap` — Best-effort QoS communication

### RTI Connext Samples
- `rti_discovery.pcap` — Discovery sequence from RTI Connext DDS
- `rti_multicast.pcap` — Multicast data distribution

### OpenDDS Samples
- `opendds_discovery.pcap` — Discovery sequence from OCI OpenDDS
- `opendds_tcp.pcap` — DDS over TCP transport

### ROS2 Samples
- `ros2_talker_listener.pcap` — Simple talker/listener example
- `ros2_image_raw.pcap` — Image topic with large messages
- `ros2_tf.pcap` — TF transform topic
- `ros2_scan.pcap` — LaserScan topic

### Multi-Vendor Interop
- `interop_fastdds_cyclone.pcap` — FastDDS and CycloneDDS communication
- `interop_three_vendors.pcap` — Three different vendors interoperating

## How to Capture DDS Traffic

### Using tcpdump
```bash
# Capture DDS discovery traffic (SPDP multicast)
sudo tcpdump -i eth0 -w dds_discovery.pcap 'udp port 7400'

# Capture all DDS traffic
sudo tcpdump -i eth0 -w dds_traffic.pcap 'udp portrange 7400-7500'

# Capture ROS2 traffic with domain ID 0
sudo tcpdump -i eth0 -w ros2_traffic.pcap 'udp portrange 7400-7500'
```

### Using Wireshark
1. Start capture on the network interface
2. Apply filter: `rtps` or `udp.port >= 7400 and udp.port <= 7500`
3. Save as PCAP format

### Using Wadjet-Link
```cpp
#include <wadjet/io/capture_session.hpp>
#include <wadjet/pcap/pcap_writer.hpp>

auto session = CaptureSession::open("eth0", options);
session.set_filter("udp portrange 7400-7500");

PcapWriter writer("dds_capture.pcap");
while (auto pkt = session.next_packet()) {
    writer.write(*pkt);
}
```

## Sample File Format

Each sample file should be accompanied by a `.txt` file with metadata:

```
# dds_sample.txt
Description: FastDDS discovery and data exchange
Captured: 2024-01-15
Duration: 30 seconds
Participants: 2
Topics: /chatter, /rosout
DDS Implementations: FastDDS 2.10
Domain ID: 0
Transport: UDP multicast + unicast
```

## Contributing Samples

When contributing sample captures:

1. Ensure no sensitive data is included
2. Keep captures small (< 1MB if possible)
3. Include metadata file
4. Verify the capture loads correctly with Wireshark
5. Test with Wadjet-Link decoder

## Notes

- RTPS uses UDP ports in the 7400-7500 range by default
- Domain ID 0 uses port 7400 for discovery multicast
- Different DDS implementations may have vendor-specific extensions
- ROS2 typically uses domain ID 0 by default

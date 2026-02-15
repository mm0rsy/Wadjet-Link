# DDS Distributed Testing Integration Plan

**Document**: DDS-RTPS Integration Roadmap for Wadjet-Link Distributed Testing  
**Phase**: 13 - Cross-Spec Integration & Correctness (Category H: T335)  
**Status**: Planning / Pre-Implementation (M10 DDS decoder not yet available)  
**Target Implementation**: Post-Phase 13 (M10 Milestone)  

---

## Executive Summary

This document outlines the integration strategy for DDS (Data Distribution Service) / RTPS (Real-Time Publish-Subscribe) protocol support into the Wadjet-Link distributed testing framework.

**Key Points**:
- DDS is the middleware of choice for ADAS/Autonomous Driving and ROS2 systems
- Currently no DDS decoder exists (M10 is planned but not yet implemented)
- T334 provides placeholder matcher interface to enable design without implementation
- This plan maps DDS discovery/data protocols to distributed test assertions
- Post-M10, distributed tests can validate multi-ECU DDS topic flows, latency, QoS compliance

---

## Background: Why DDS Matters for Automotive

### DDS in ADAS/AV Systems
- **ROS2 Middleware**: Most ADAS stacks (including autonomous driving systems) use ROS2, which is built on DDS
- **Real-Time Communication**: RTPS provides bounded latency suitable for safety-critical automotive systems
- **Distributed Publish-Subscribe**: Natural fit for multi-ECU sensor fusion (LiDAR, Camera, Radar combining their output)
- **QoS Control**: DDS provides fine-grained Quality of Service: Reliability, Latency Budget, Durability

### Example ADAS Data Flows
```
LiDAR ECU (Publisher) --[DDS RTPS]-->  Fusion ECU (Subscriber)
                           |
                      "lidar_points" topic
                      {QoS: Reliable, MaxLatency: 50ms}

Camera ECU (Publisher) --[DDS RTPS]--> Fusion ECU (Subscriber)
                           |
                      "image_frames" topic
                      {QoS: BestEffort, MaxLatency: 100ms}
```

### Testing Requirements
Multi-node distributed test scenarios must validate:
1. **Topic Discovery**: All expected topics are discovered via SPDP/SEDP
2. **Message Flow**: Publishers send on expected topics to subscribers
3. **Latency Constraints**: Messages arrive within QoS latency bounds
4. **Message Ordering**: Topic messages maintain sequence numbers (if Reliable QoS)
5. **Payload Integrity**: Data payloads match expected types and ranges

---

## DDS-RTPS Protocol Overview

### Protocol Stack
```
Application Layer
       |
DDS (Data Distribution Service) API
       |
DDSI (DDS Interoperability)
       |
RTPS (Real-Time Publish-Subscribe Protocol)
       |
UDP/TCP (Transport)
```

### RTPS Packet Structure
```
RTPS Message Header
├── Magic Number: "RTPS" (0x52 0x54 0x50 0x53)
├── Protocol Version: (e.g., 2.1)
├── Vendor ID: (e.g., 0x0101 = RTI Connext)
├── Guid Prefix: Identifies participant
├── Sequence Number
└── Flags

SubMessages[]
├── SPDP - Simple Participant Discovery Protocol
│   └── Announce new/existing DDS participants
├── SEDP - Simple Endpoint Discovery Protocol
│   └── Announce writers (publishers) and readers (subscribers)
└── DATA - Actual topic data
    └── Topic ID + Sequence Number + Payload
```

---

## SPDP: Participant Discovery

### Purpose
SPDP allows DDS participants (ECUs running DDS middleware) to discover each other via multicast announcements.

### Message Flow
```
Time 0ms:   ECU-A sends SPDP Announce
Time 100ms: ECU-B listens (multicast 239.255.0.1:7400)
Time 100ms: ECU-B detects ECU-A
Time 100ms: ECU-B sends SPDP Announce
Time 200ms: ECU-A detects ECU-B
            ✓ Mutual discovery complete
```

### Distributed Test Validation
```cpp
// T335: Future distributed test with DDS SPDP validation
TEST_F(DistributedDdsParticipantDiscoveryTest, BothEcusDiscoverEachOther) {
    // Setup two nodes with DDS middleware
    AddNode("ecu-a", NodeRole::ADAS_FUSION);
    AddNode("ecu-b", NodeRole::SENSOR_FUSION);
    
    // Load scenario that starts both ECUs
    auto scenario = LoadScenario("dds_startup.yaml");
    
    // Run for 300ms to allow discovery
    auto result = RunScenario(scenario, 300);
    
    // Assertion: Both participants discovered each other
    auto matcher = ExpectDdsParticipantDiscovery("ecu-a", "ecu-b");
    ASSERT_TRUE(matcher->evaluate(captures).matched);
}
```

### Implementation Requirements (Post-M10)
1. **SPDP Message Parsing**: Extract Participant GUID from SPDP Announce
2. **Discovery Timeline**: Track when each participant appears (for startup validation)
3. **Multicast Detection**: Verify messages use correct multicast address (239.255.0.1)
4. **Domain ID Extraction**: Ensure participants are in same DDS domain

---

## SEDP: Endpoint Discovery

### Purpose
SEDP announces the existence of writers (Publishers) and readers (Subscribers) within each participant.

### Message Flow
```
Time 500ms: ECU-A announces Writer for "lidar_points"
Time 500ms: ECU-A announces Reader for "control_commands"
Time 600ms: ECU-B announces Reader for "lidar_points"
Time 600ms: ECU-B announces Writer for "control_commands"
            ✓ Topic subscriptions established
```

### Distributed Test Validation
```cpp
// T335: Future test with SEDP topic discovery
TEST_F(DistributedDdsTopicDiscoveryTest, SensorAndFusionTopicsDiscovered) {
    AddNode("sensor-ecu", NodeRole::SENSOR_NODE);
    AddNode("fusion-ecu", NodeRole::FUSION_NODE);
    
    auto scenario = LoadScenario("dds_topics_startup.yaml");
    auto result = RunScenario(scenario, 1000);
    
    // Assertion: Topics discovered in expected direction
    auto publish_match = ExpectDdsTopicPublished(
        "sensor-ecu",
        "lidar_points"
    );
    ASSERT_TRUE(publish_match->evaluate(captures).matched);
    
    auto subscribe_match = ExpectDdsTopicSubscribed(
        "fusion-ecu",
        "lidar_points"
    );
    ASSERT_TRUE(subscribe_match->evaluate(captures).matched);
}
```

### Implementation Requirements (Post-M10)
1. **Reader/Writer Resolution**: Map SEDP messages to topics
2. **Topic ID Extraction**: DDS assigns numeric topic IDs for efficient matching
3. **QoS Parameters**: Extract Reliability, Latency Budget, other QoS settings
4. **Type Information**: Parse data type information from SEDP

---

## RTPS Data Messages: Actual Topic Flows

### Purpose
After discovery, RTPS DATA messages carry the actual topic payloads.

### Message Structure
```cpp
struct RTPS_DATA_Submessage {
    uint32_t reader_id;          // Target reader entity
    uint32_t writer_id;          // Source writer entity
    SequenceNumber_t sn;         // Sequence: {high: u32, low: u32}
    uint32_t timestamp;          // When message was created
    uint8_t flags;               // Endian, have timestamp, etc.
    Serialized_Payload payload;  // Topic data (CDR encoded)
};
```

### Topic Correlation Challenge
**Problem**: Multiple topics are active simultaneously. How to identify which packet belongs to which topic?

**Solution Strategy**:
1. **Known Topic IDs**: From SEDP discovery phase, we know all active topic IDs
2. **Writer ID Mapping**: SEDP told us which writer publishes which topic
3. **Payload Inspection**: Deserialize CDR (Common Data Representation) payload to validate content
4. **Sequence Number Tracking**: Ensure no gaps in message sequences (for Reliable QoS)

### Distributed Test Validation
```cpp
// T335: Future test with RTPS topic flow validation
TEST_F(DistributedDdsTopicFlowTest, LidarPointsFlowToFusion) {
    AddNode("lidar-ecu", NodeRole::SENSOR_NODE);
    AddNode("fusion-ecu", NodeRole::FUSION_NODE);
    
    auto scenario = LoadScenario("dds_lidar_fusion.yaml");
    auto result = RunScenario(scenario, 5000);
    
    // Assert: lidar_points topic flows from publisher to subscriber
    auto matcher = ExpectDdsTopicFlow(
        "lidar-ecu",       // publisher
        "fusion-ecu",      // subscriber
        "lidar_points"     // topic name
    );
    auto match_result = matcher->evaluate(captures);
    
    ASSERT_TRUE(match_result.matched);
    EXPECT_GE(match_result.message_count, 50);     // ≥50 messages over 5sec
    EXPECT_LT(match_result.latency_ms, 50);        // Within 50ms latency budget
    EXPECT_EQ(match_result.messages_in_order, true); // No out-of-order messages
}
```

### Implementation Requirements (Post-M10)
1. **CDR Deserialization**: Parse Common Data Representation payload format
2. **Payload Validation**: Check topic data against expected schema
3. **Latency Measurement**: Correlate sequence numbers with timestamps
4. **Message Ordering**: For Reliable QoS, detect sequence gaps
5. **Multicast vs Unicast**: Identify primary data path (unicast more common in production)

---

## Integration with Wadjet-Link Distributed Framework

### Phase 1: Placeholder (T334 - Current)
- ✅ Create `ExpectDdsTopicFlow()` matcher interface
- ✅ Returns "M10 not available" error message
- ✅ Allows test code to compile without M10

### Phase 2: M10 Implementation (Post-Phase 13)
- [ ] Implement DDS-RTPS decoder in M10 module
- [ ] Update DistributedMatcher evaluation to parse RTPS packets
- [ ] Implement topic correlation algorithm

### Phase 3: Test Suite Expansion (Post-Phase 13)
- [ ] Add DDS-specific test scenarios in examples/
- [ ] Create DDS QoS compliance tests
- [ ] Add multi-topic latency tests

---

## Proposed Distributed Matchers (Future - Post-M10)

### Basic Topic Flow
```cpp
// T335 planned: Validate topic flows
ExpectDdsTopicFlow(publisher_node, subscriber_node, topic_name)
    -> Asserts at least one message published and delivered
```

### Topic With QoS Constraints
```cpp
// Future enhancement
ExpectDdsTopicFlow(publisher, subscriber, topic)
    .with_latency_constraint(50ms)      // Max 50ms E2E latency
    .with_min_message_count(10)         // At least 10 messages
    .with_reliability(RELIABLE);        // Expect reliable QoS
```

### Participant Discovery
```cpp
// Future enhancement
ExpectDdsParticipantDiscovered(participant_node, within_ms)
    -> Asserts participant discovered within time window
```

### Topic Ordering
```cpp
// Future enhancement
ExpectDdsTopicMessageOrder(publisher, subscriber, topic)
    -> Asserts messages have sequential sequence numbers (Reliable QoS)
```

---

## Current Limitations & Future Work

### Limitations (Until M10 Available)
| Aspect | Current | Post-M10 |
|--------|---------|----------|
| SPDP Discovery Validation | ❌ Not supported | ✅ Supported |
| SEDP Topic Discovery | ❌ Not supported | ✅ Supported |
| RTPS Data Message Parsing | ❌ Not supported | ✅ Supported |
| Topic Correlation | ❌ Manual only | ✅ Automatic |
| QoS Compliance Checking | ❌ Not supported | ✅ Supported |
| Payload Type Validation | ❌ Not supported | ✅ Supported |

### Implementation Roadmap
1. **M10 Phase 1**: SPDP/SEDP parser + basic topic discovery
2. **M10 Phase 2**: RTPS data message parser + CDR deserializer
3. **M10 Phase 3**: Topic correlation + latency validation
4. **M14 Phase 2**: Integration with distributed framework (post-M14.1)
5. **M14 Phase 3**: QoS compliance test suite

---

## References

### DDS-RTPS Specifications
- [OMG DDS 1.4 Spec](https://www.omg.org/spec/DDS/1.4)
- [DDSI-RTPS 2.3 Spec](https://www.omg.org/spec/DDSI-RTPS/)
- [Common Data Representation (CDR)](https://www.omg.org/spec/XCDR/)

### Open Source DDS Implementations
- [RTI Connext DDS](https://www.rti.com/products/dds) - Reference implementation
- [OpenDDS](https://opendds.org/) - Open source C++ implementation
- [Cyclone DDS](https://github.com/eclipse-cyclonedds/cyclonedds) - Eclipse project

### ROS2 + DDS
- [ROS2 DDS Documentation](https://docs.ros.org/en/rolling/Concepts/Intermediate/DDS-and-ROS-middleware-implementations.html)
- [ROS2 QoS Settings](https://docs.ros.org/en/rolling/Concepts/About-Quality-of-Service-Settings.html)

---

## Appendix: Example DDS Scenario (Future)

```yaml
# examples/scenarios/dds_multi_sensor_fusion.yaml
# T335: Distributed test scenario for multi-sensor DDS fusion
# Demonstrates: SPDP discovery + SEDP topic registration + RTPS data flow

name: "Multi-Sensor DDS Fusion Test"
description: "LiDAR + Camera + Radar sensors fusing at central ECU via DDS topics"
scenario_id: "dds-fusion-001"

nodes:
  - id: "lidar-ecu"
    address: "192.168.1.10:20001"
    interfaces:
      - name: "eth0"
        ip: "192.168.1.10"
        
  - id: "camera-ecu"
    address: "192.168.1.11:20001"
    interfaces:
      - name: "eth0"
        ip: "192.168.1.11"
        
  - id: "fusion-ecu"
    address: "192.168.1.20:20001"
    interfaces:
      - name: "eth0"
        ip: "192.168.1.20"

steps:
  - step_id: "discovery"
    step_name: "DDS Participant Discovery"
    type: "wait"
    target_nodes: ["lidar-ecu", "camera-ecu", "fusion-ecu"]
    timeout_ms: 1000
    description: "Wait for all DDS participants to discover each other"
    
  - step_id: "topic_discovery"
    step_name: "Topic Registration"
    type: "wait"
    target_nodes: ["lidar-ecu", "camera-ecu", "fusion-ecu"]
    timeout_ms: 1000
    description: "Wait for all topics to be discovered via SEDP"
    
  - step_id: "capture_period"
    step_name: "Capture Sensor Data"
    type: "capture"
    target_nodes: ["lidar-ecu", "camera-ecu", "fusion-ecu"]
    config:
      duration_ms: 5000
      interfaces: ["eth0"]
      filter: "udp port 7400 or udp port 7410"  # RTPS multicast/unicast
      
  - step_id: "expect_flows"
    step_name: "Validate Data Flows"
    type: "expect"
    target_nodes: ["fusion-ecu"]
    config:
      assertions:
        - type: "dds_topic_flow"
          publisher: "lidar-ecu"
          subscriber: "fusion-ecu"
          topic: "sensor_msgs/msg/PointCloud2"
          timeout_ms: 5000
          
        - type: "dds_topic_flow"
          publisher: "camera-ecu"
          subscriber: "fusion-ecu"
          topic: "sensor_msgs/msg/Image"
          timeout_ms: 5000
          
        - type: "dds_latency"
          topic: "sensor_msgs/msg/PointCloud2"
          max_latency_ms: 50
          
        - type: "dds_latency"
          topic: "sensor_msgs/msg/Image"
          max_latency_ms: 100
```

---

**Document Status**: Ready for M10 Implementation  
**Last Updated**: February 8, 2026  
**Next Review**: Post-M10 DDS Decoder Implementation  

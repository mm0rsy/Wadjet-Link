# Feature Specification: Advanced Operations & Security

**Feature Branch**: `milestone/023-advanced-ops-security`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M23 - Advanced Operations & Security

## Overview

Implement advanced operational features and security capabilities for production deployments. This includes Secure Onboard Communication (SecOC) support, TLS/DTLS for DoIP, plugin framework for extensibility, remote probe mode for distributed testing, and advanced telemetry. These features enable enterprise-grade deployments and secure automotive environments.

## User Scenarios & Testing

### User Story 1 - SecOC Authentication Verification (Priority: P1)

As a security engineer, I need to verify SecOC authentication on Ethernet traffic, so that I can detect spoofed messages.

**Why this priority**: SecOC is mandatory for secure automotive communication - detection prevents security breaches.

**Independent Test**: Capture SecOC-protected SOME/IP traffic and verify authentication status.

**Acceptance Scenarios**:

1. **Given** SOME/IP message with SecOC authentication, **When** decoded, **Then** authentication header (Freshness Value, Authenticator) is extracted
2. **Given** SecOC configuration (key, algorithm), **When** provided, **Then** authenticator is verified
3. **Given** authenticated message, **When** verification passes, **Then** message is marked as authentic
4. **Given** spoofed message, **When** verification fails, **Then** authentication failure is reported
5. **Given** freshness value replay, **When** detected, **Then** replay attack warning is issued

### User Story 2 - TLS/DTLS for DoIP (Priority: P1)

As a security analyst, I need TLS/DTLS support for DoIP, so that I can analyze encrypted diagnostic sessions.

**Why this priority**: Secure DoIP is emerging standard - encryption analysis is needed for validation.

**Independent Test**: Capture TLS-protected DoIP traffic and decrypt with pre-shared keys.

**Acceptance Scenarios**:

1. **Given** DoIP over TLS (ISO 13400-2 Amendment), **When** captured, **Then** TLS handshake is decoded
2. **Given** TLS pre-shared keys (PSK), **When** provided, **Then** encrypted DoIP payload is decrypted
3. **Given** decrypted DoIP messages, **When** analyzed, **Then** UDS services are accessible
4. **Given** TLS cipher suite, **When** detected, **Then** encryption algorithm is identified
5. **Given** certificate validation, **When** enabled, **Then** server certificate is verified (optional)

### User Story 3 - Plugin Framework for Custom Protocols (Priority: P2)

As a protocol developer, I need a plugin framework, so that I can add custom proprietary protocol decoders without modifying core code.

**Why this priority**: Plugin architecture enables extensibility - supports vendor-specific protocols.

**Independent Test**: Load plugin for custom protocol and verify decode.

**Acceptance Scenarios**:

1. **Given** Python plugin for custom protocol, **When** loaded, **Then** plugin is registered with ProtocolDispatcher
2. **Given** custom protocol packet, **When** dispatched, **Then** plugin decoder is invoked
3. **Given** plugin decode result, **When** returned, **Then** result is integrated with standard decode chain
4. **Given** plugin errors, **When** encountered, **Then** errors don't crash main application
5. **Given** Lua plugin (alternative), **When** loaded, **Then** Lua-based decoder works equivalently

### User Story 4 - Remote Probe Mode (Priority: P2)

As a distributed test engineer, I need remote probe capability, so that I can capture traffic from remote nodes and analyze centrally.

**Why this priority**: Remote capture enables distributed testing - essential for multi-ECU validation.

**Independent Test**: Deploy remote probe, capture traffic, stream to central analyzer.

**Acceptance Scenarios**:

1. **Given** remote probe deployed on target ECU, **When** started, **Then** probe captures packets and streams via gRPC
2. **Given** central analyzer, **When** connected to probe, **Then** packets are received in real-time
3. **Given** multiple probes, **When** active, **Then** packets from all probes are multiplexed with source identification
4. **Given** probe configuration, **When** updated remotely, **Then** filter expressions and capture settings are applied
5. **Given** network interruption, **When** reconnected, **Then** probe resumes streaming without data loss

### User Story 5 - Advanced Telemetry & Observability (Priority: P2)

As a DevOps engineer, I need telemetry export, so that I can monitor Wadjet-Link performance in production.

**Why this priority**: Observability enables production monitoring - detects performance degradation.

**Independent Test**: Export telemetry and verify metrics are available.

**Acceptance Scenarios**:

1. **Given** telemetry enabled, **When** Wadjet-Link runs, **Then** metrics (packet rate, decode latency, memory usage) are collected
2. **Given** Prometheus exporter, **When** queried, **Then** metrics are exposed in Prometheus format
3. **Given** OpenTelemetry integration, **When** enabled, **Then** traces and spans are exported
4. **Given** structured logging, **When** enabled, **Then** logs are in JSON format with correlation IDs
5. **Given** dashboard template, **When** imported to Grafana, **Then** real-time metrics are visualized

## Edge Cases

- What happens when SecOC key is incorrect (authentication always fails)?
- How are TLS session resumptions handled?
- What if plugin has memory leaks or crashes?
- How does system handle remote probe connection loss during active capture?
- What happens when telemetry buffer overflows under high load?

## Requirements

### Functional Requirements

#### SecOC Support

- **FR-001**: System MUST detect SecOC-protected SOME/IP messages (SecuredIPdu)
- **FR-002**: System MUST extract SecOC authentication header (Freshness Value, Authenticator)
- **FR-003**: System MUST verify SecOC authenticator using CMAC-AES-128 (most common)
- **FR-004**: System MUST support freshness value validation (counter, timestamp modes)
- **FR-005**: System MUST detect replay attacks (duplicate freshness values)
- **FR-006**: System MUST report authentication status (success, failure, replay)
- **FR-007**: System MUST load SecOC keys from configuration file (not hardcoded)

#### TLS/DTLS for DoIP

- **FR-008**: System MUST detect DoIP over TLS (port 13400 with TLS)
- **FR-009**: System MUST parse TLS handshake messages (ClientHello, ServerHello, Certificate, Finished)
- **FR-010**: System MUST decrypt DoIP payload using pre-shared keys (PSK) or session keys
- **FR-011**: System MUST support TLS 1.2 and TLS 1.3
- **FR-012**: System MUST support common cipher suites (AES-GCM, ChaCha20-Poly1305)
- **FR-013**: System MUST integrate decrypted DoIP with UDS decoder
- **FR-014**: System MUST optionally verify server certificates (if CA provided)

#### Plugin Framework

- **FR-015**: System MUST support Python plugins for custom protocol decoders
- **FR-016**: System MUST support Lua plugins for lightweight protocol extensions
- **FR-017**: Plugins MUST register with ProtocolDispatcher via plugin API
- **FR-018**: Plugins MUST be isolated (errors don't crash main process)
- **FR-019**: Plugins MUST be hot-loadable (load/unload without restart)
- **FR-020**: Plugin API MUST provide access to PacketView and DecodeResult
- **FR-021**: System MUST provide plugin examples and templates

#### Remote Probe Mode

- **FR-022**: System MUST support remote probe deployment (standalone binary: wadjet-probe)
- **FR-023**: Remote probe MUST stream packets via gRPC
- **FR-024**: Central analyzer MUST accept connections from multiple probes
- **FR-025**: Packets MUST be tagged with probe source ID
- **FR-026**: Remote configuration MUST support filter updates without probe restart
- **FR-027**: System MUST handle probe reconnection after network interruption
- **FR-028**: System MUST use TLS for probe-to-analyzer communication (gRPC + TLS)

#### Telemetry & Observability

- **FR-029**: System MUST collect performance metrics (packet rate, decode rate, drop rate, latency)
- **FR-030**: System MUST expose metrics via Prometheus exporter (HTTP endpoint)
- **FR-031**: System MUST support OpenTelemetry tracing (spans for decode operations)
- **FR-032**: System MUST provide structured logging (JSON format with correlation IDs)
- **FR-033**: System MUST include Grafana dashboard template for visualization
- **FR-034**: Telemetry overhead MUST be ≤2% of capture performance

### Key Entities

- **SecOcAuthenticator**: Verifies SecOC authentication
- **TlsDecryptor**: Decrypts TLS/DTLS traffic
- **PluginManager**: Loads and manages plugins
- **PythonPlugin**: Python-based protocol decoder
- **LuaPlugin**: Lua-based protocol decoder
- **RemoteProbe**: Remote packet capture agent
- **GrpcProbeServer**: gRPC server for probe communication
- **TelemetryCollector**: Collects performance metrics
- **PrometheusExporter**: Exposes metrics for Prometheus
- **OpenTelemetryTracer**: Exports traces to OpenTelemetry backend

## Success Criteria

### Measurable Outcomes

- **SC-001**: SecOC authentication verification correctly detects authentic and spoofed messages
- **SC-002**: TLS/DTLS decryption successfully decrypts DoIP traffic with pre-shared keys
- **SC-003**: Python and Lua plugins successfully decode custom protocols without core modification
- **SC-004**: Remote probe mode streams packets from 5+ probes simultaneously with <1% packet loss
- **SC-005**: Prometheus exporter provides metrics accessible by Grafana dashboard
- **SC-006**: OpenTelemetry tracing captures decode latency per protocol layer
- **SC-007**: Structured logging enables log correlation across distributed probes
- **SC-008**: Telemetry overhead measured at ≤2% performance impact
- **SC-009**: Plugin isolation prevents crashes (100 intentional plugin errors don't crash main process)
- **SC-010**: Remote probe reconnection recovers within 5 seconds after network interruption

## Assumptions

- SecOC keys are provided by user (not extracted from network)
- TLS pre-shared keys or session keys are available for decryption
- Focus on passive analysis (no active SecOC message generation)
- Remote probes deployed on Linux systems with network access
- Telemetry backend (Prometheus, OpenTelemetry collector) is user-provided

## Dependencies

- **External**: OpenSSL (TLS/DTLS), gRPC (remote probes), Prometheus client library, OpenTelemetry SDK, pybind11/Lua (plugins)
- **Internal**: M2 (Protocol Decoders), M9 (UDS decoder for DoIP integration)

## Out of Scope

- SecOC key management or key provisioning (user responsibility)
- Full TLS/DTLS certificate authority (CA) infrastructure
- Plugin sandboxing with OS-level isolation (process-level isolation only)
- Remote probe administration UI (CLI configuration only)
- Real-time alerting or anomaly detection (telemetry export only)
- Hardware Security Module (HSM) integration for key storage

## Implementation Notes

### Recommended Approach

**Phase 1 - SecOC Support** (3 weeks)
- Implement SecOC header parsing
- CMAC-AES-128 authentication verification
- Freshness value validation
- Replay detection
- Tests: 20+ for SecOC scenarios

**Phase 2 - TLS/DTLS Decryption** (3 weeks)
- TLS handshake parsing
- Pre-shared key (PSK) decryption
- Session key extraction
- DoIP payload decryption
- Tests: 25+ for TLS/DTLS

**Phase 3 - Plugin Framework** (2 weeks)
- Plugin API design
- Python plugin support (pybind11)
- Lua plugin support (LuaJIT)
- Plugin isolation and error handling
- Tests: 15+ for plugin functionality

**Phase 4 - Remote Probe Mode** (3 weeks)
- gRPC service definition
- Remote probe binary (wadjet-probe)
- Central analyzer integration
- Multi-probe multiplexing
- Reconnection handling
- Tests: 18+ for remote probe scenarios

**Phase 5 - Telemetry & Observability** (2 weeks)
- Prometheus exporter
- OpenTelemetry tracing
- Structured logging
- Grafana dashboard template
- Tests: 12+ for telemetry

**Total Duration**: ~13 weeks

### SecOC Configuration Example

```yaml
# secoc_config.yml
secoc:
  keys:
    - name: "ECU_Gateway_Key"
      id: 0x1234
      algorithm: CMAC-AES-128
      key: "0123456789ABCDEF0123456789ABCDEF"
      freshness_mode: counter  # or timestamp
    
  messages:
    - service_id: 0x5678
      method_id: 0x0001
      key_id: 0x1234
      secured: true
```

### TLS Decryption Configuration

```yaml
# tls_config.yml
tls:
  pre_shared_keys:
    - identity: "DoIP_ECU"
      key: "FEDCBA9876543210FEDCBA9876543210"
  
  cipher_suites:
    - TLS_PSK_WITH_AES_128_GCM_SHA256
    - TLS_PSK_WITH_CHACHA20_POLY1305_SHA256
```

### Python Plugin Example

```python
# custom_protocol_plugin.py
import wadjet_plugin

class CustomProtocolDecoder(wadjet_plugin.ProtocolDecoder):
    def name(self):
        return "CustomProtocol"
    
    def can_decode(self, packet: wadjet_plugin.PacketView) -> bool:
        # Check if packet is custom protocol (e.g., magic bytes)
        return packet.data[0:4] == b'\xAA\xBB\xCC\xDD'
    
    def decode(self, packet: wadjet_plugin.PacketView) -> wadjet_plugin.DecodeResult:
        # Parse custom protocol
        header = struct.unpack('>HH', packet.data[4:8])
        return wadjet_plugin.DecodeResult(
            protocol="CustomProtocol",
            fields={"type": header[0], "length": header[1]}
        )

# Register plugin
wadjet_plugin.register(CustomProtocolDecoder())
```

### Remote Probe CLI

```bash
# Deploy remote probe on ECU
wadjet-probe --interface eth0 --server analyzer.example.com:50051 --tls

# Central analyzer
wadjet --remote-probes \
  --prometheus-port 9090 \
  --opentelemetry-endpoint http://otel-collector:4318
```

### Prometheus Metrics Example

```
# HELP wadjet_packets_captured_total Total packets captured
# TYPE wadjet_packets_captured_total counter
wadjet_packets_captured_total{interface="eth0",protocol="someip"} 12345

# HELP wadjet_decode_latency_seconds Decode latency
# TYPE wadjet_decode_latency_seconds histogram
wadjet_decode_latency_seconds_bucket{protocol="someip",le="0.0001"} 9500
wadjet_decode_latency_seconds_bucket{protocol="someip",le="0.001"} 9990
wadjet_decode_latency_seconds_bucket{protocol="someip",le="0.01"} 10000

# HELP wadjet_memory_usage_bytes Memory usage
# TYPE wadjet_memory_usage_bytes gauge
wadjet_memory_usage_bytes{component="capture"} 524288000
```

### Grafana Dashboard Queries

```promql
# Packet capture rate (packets/sec)
rate(wadjet_packets_captured_total[1m])

# p99 decode latency
histogram_quantile(0.99, rate(wadjet_decode_latency_seconds_bucket[5m]))

# Memory usage trend
wadjet_memory_usage_bytes{component="capture"}

# Drop rate
rate(wadjet_packets_dropped_total[1m]) / rate(wadjet_packets_captured_total[1m])
```

### Test Count Target

**90+ tests** covering:
- SecOC: 20 tests
- TLS/DTLS: 25 tests
- Plugin framework: 15 tests
- Remote probes: 18 tests
- Telemetry: 12 tests

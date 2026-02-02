# Feature Specification: Web-Based Report Viewer

**Feature Branch**: `milestone/014-web-report-viewer`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M14 - Web-Based Report Viewer

## Overview

Create an interactive web-based report viewer for test results, protocol analysis, and network diagnostics. The viewer provides rich visualization of captured traffic, test outcomes, and protocol statistics through a modern single-page application.

## User Scenarios & Testing

### User Story 1 - Protocol Timeline Visualization (Priority: P1)

As a network engineer, I need an interactive timeline view of all protocols in a capture, so that I can visualize protocol interactions over time.

**Why this priority**: Timeline view is the primary visualization for understanding multi-protocol communication sequences.

**Independent Test**: Load PCAP file and render protocol timeline with filtering.

**Acceptance Scenarios**:

1. **Given** a PCAP file with mixed protocols, **When** loaded, **Then** timeline shows all packets chronologically
2. **Given** timeline visualization, **When** rendered, **Then** each protocol uses distinct color (Ethernet: gray, IPv4: blue, UDP: green, TCP: yellow, SOME/IP: purple, DoIP: orange, gPTP: red, UDS: brown)
3. **Given** protocol filter selection, **When** applied, **Then** timeline shows only selected protocols
4. **Given** time range selection, **When** zoomed, **Then** timeline updates to show zoomed range with increased detail
5. **Given** packet in timeline, **When** clicked, **Then** detailed packet view opens with full decode

### User Story 2 - Test Results Dashboard (Priority: P1)

As a test engineer, I need a test results dashboard showing pass/fail statistics, so that I can quickly assess test campaign health.

**Why this priority**: Dashboard is the entry point for test result analysis - critical for CI/CD integration.

**Independent Test**: Load test results and display summary dashboard.

**Acceptance Scenarios**:

1. **Given** JUnit XML test results, **When** loaded, **Then** dashboard shows total tests, passed, failed, skipped counts
2. **Given** test suite results, **When** rendered, **Then** pass rate percentage is calculated and displayed
3. **Given** failed tests, **When** clicked, **Then** failure details and stack traces are shown
4. **Given** test duration, **When** calculated, **Then** total test time and per-test timing are displayed
5. **Given** historical results, **When** compared, **Then** trend visualization shows pass rate over time

### User Story 3 - SOME/IP Service Map (Priority: P2)

As a SOME/IP developer, I need a service map visualization showing all discovered services, so that I can understand service topology.

**Why this priority**: Service map provides architectural overview - helps validate service discovery configuration.

**Independent Test**: Analyze SOME/IP-SD traffic and render service map.

**Acceptance Scenarios**:

1. **Given** SOME/IP-SD traffic, **When** analyzed, **Then** all OfferService entries are extracted
2. **Given** discovered services, **When** rendered, **Then** service map shows Service ID, Instance ID, and endpoint
3. **Given** service lifecycle, **When** tracked, **Then** service appearance/disappearance events are timestamped
4. **Given** EventGroup subscriptions, **When** detected, **Then** subscriber-service relationships are visualized
5. **Given** service filtering, **When** applied, **Then** map shows only selected service IDs

### User Story 4 - DoIP Diagnostic Session View (Priority: P2)

As a diagnostic engineer, I need a diagnostic session view showing UDS request/response pairs, so that I can troubleshoot diagnostic communication.

**Why this priority**: Session view correlates requests and responses - essential for diagnostic troubleshooting.

**Independent Test**: Load DoIP traffic and display diagnostic session timeline.

**Acceptance Scenarios**:

1. **Given** DoIP diagnostic messages, **When** parsed, **Then** routing activation and diagnostic messages are identified
2. **Given** UDS request/response pairs, **When** correlated, **Then** they are displayed as linked items
3. **Given** negative responses, **When** rendered, **Then** NRC codes are highlighted with descriptions
4. **Given** session timing, **When** calculated, **Then** request-to-response latency is displayed
5. **Given** diagnostic session timeline, **When** filtered by source/target address, **Then** specific ECU conversations are isolated

### User Story 5 - Statistics & Charts (Priority: P2)

As a performance analyst, I need statistical charts for bandwidth, packet rates, and protocol distribution, so that I can analyze network performance.

**Why this priority**: Statistical visualization reveals performance trends and bottlenecks.

**Independent Test**: Generate and render statistics charts from capture.

**Acceptance Scenarios**:

1. **Given** captured traffic, **When** analyzed, **Then** bandwidth chart shows Mbps over time
2. **Given** packet counts, **When** calculated, **Then** packet rate chart shows packets/sec
3. **Given** protocol distribution, **When** rendered, **Then** pie chart shows percentage by protocol
4. **Given** time-series data, **When** displayed, **Then** line charts show metrics over time with zoom/pan
5. **Given** export option, **When** clicked, **Then** charts are exported as PNG/SVG

### User Story 6 - Packet Search & Filter (Priority: P1)

As a network analyst, I need full-text search and filter capabilities, so that I can quickly find specific packets or patterns.

**Why this priority**: Search is fundamental for large captures - without it, analysis is impractical.

**Independent Test**: Search for specific protocol fields and payloads.

**Acceptance Scenarios**:

1. **Given** search query (e.g., "service_id == 0x1234"), **When** executed, **Then** matching packets are highlighted
2. **Given** payload search (hex or ASCII), **When** applied, **Then** packets containing payload are found
3. **Given** protocol-specific filters (HasSOMEIPServiceId, IsDoIPRoutingActivation), **When** used, **Then** relevant packets are displayed
4. **Given** multiple filter criteria, **When** combined (AND/OR), **Then** complex queries are supported
5. **Given** search results, **When** rendered, **Then** matching portions are highlighted in packet view

## Edge Cases

- What happens when loading multi-GB PCAP files (memory constraints)?
- How are incomplete or corrupted test result files handled?
- What if service discovery contains duplicate service IDs from different ECUs?
- How does system handle PCAP files with millions of packets (performance)?
- What happens when timezone information is missing from timestamps?
- How are browser compatibility issues addressed (older browsers)?

## Requirements

### Functional Requirements

#### Core Web Application

- **FR-001**: System MUST provide single-page application (SPA) with responsive design
- **FR-002**: System MUST support modern browsers (Chrome 90+, Firefox 88+, Safari 14+, Edge 90+)
- **FR-003**: System MUST be deployable as static HTML/JS/CSS (no server runtime required)
- **FR-004**: System MUST support local file loading (no server upload)
- **FR-005**: System MUST handle PCAP files up to 500MB in browser memory
- **FR-006**: Application MUST be usable offline (no CDN dependencies for core functionality)

#### File Loading & Parsing

- **FR-007**: System MUST load PCAP/PCAPNG files via File API
- **FR-008**: System MUST load JUnit XML test results
- **FR-009**: System MUST load TAP (Test Anything Protocol) results
- **FR-010**: System MUST load JSON test results (scenario runner output)
- **FR-011**: System MUST parse loaded files in Web Worker (non-blocking UI)
- **FR-012**: System MUST show loading progress indicator

#### Protocol Timeline

- **FR-013**: System MUST render interactive timeline with packet chronology
- **FR-014**: System MUST use color coding for protocol types
- **FR-015**: System MUST support timeline zoom and pan
- **FR-016**: System MUST display packet count and time range in timeline
- **FR-017**: System MUST support protocol filtering in timeline
- **FR-018**: Timeline MUST handle up to 100K packets with virtualization

#### Test Results Dashboard

- **FR-019**: System MUST display test summary (total, passed, failed, skipped)
- **FR-020**: System MUST calculate and display pass rate percentage
- **FR-021**: System MUST show test execution time (total and per-test)
- **FR-022**: System MUST display failed test details (message, stack trace)
- **FR-023**: System MUST support test filtering (passed, failed, skipped)
- **FR-024**: System MUST export test results to HTML report

#### Packet Detail View

- **FR-025**: System MUST display detailed packet decode with all protocol layers
- **FR-026**: System MUST highlight protocol fields with tooltips
- **FR-027**: System MUST show hex dump with ASCII representation
- **FR-028**: System MUST support copy to clipboard (packet bytes, decoded fields)
- **FR-029**: System MUST provide packet navigation (prev/next)

#### Visualization Components

- **FR-030**: System MUST render SOME/IP service map (service ID, instance, endpoint)
- **FR-031**: System MUST render DoIP diagnostic session timeline
- **FR-032**: System MUST render bandwidth chart (Mbps over time)
- **FR-033**: System MUST render packet rate chart (packets/sec)
- **FR-034**: System MUST render protocol distribution pie chart
- **FR-035**: All charts MUST support zoom, pan, and export (PNG/SVG)

#### Search & Filter

- **FR-036**: System MUST support full-text search across packet fields
- **FR-037**: System MUST support hex/ASCII payload search
- **FR-038**: System MUST support protocol-specific filter expressions
- **FR-039**: System MUST support filter combination (AND/OR/NOT)
- **FR-040**: System MUST highlight search matches in packet view

#### Export & Sharing

- **FR-041**: System MUST export filtered packets to new PCAP file
- **FR-042**: System MUST export statistics to CSV
- **FR-043**: System MUST generate shareable HTML report
- **FR-044**: System MUST support permalink for current view (URL hash-based state)

### Key Entities

- **ReportData**: Container for PCAP, test results, and analysis metadata
- **TimelineView**: Interactive timeline visualization component
- **DashboardView**: Test results summary dashboard
- **PacketDetailView**: Detailed packet decode panel
- **ServiceMapView**: SOME/IP service topology visualization
- **DiagnosticSessionView**: DoIP/UDS session timeline
- **StatisticsView**: Charts and statistical analysis
- **SearchEngine**: Full-text and filter query engine
- **FileParser**: PCAP/XML/JSON parsing in Web Worker

## Success Criteria

### Measurable Outcomes

- **SC-001**: Web viewer loads and parses 100MB PCAP file in ≤10 seconds
- **SC-002**: Timeline renders 100K packets with smooth scrolling (≥30 FPS)
- **SC-003**: Test dashboard correctly displays all JUnit XML elements (suites, cases, failures)
- **SC-004**: All visualizations (timeline, charts, service map) render correctly across target browsers
- **SC-005**: Search finds packets matching criteria in ≤2 seconds for 10K packet capture
- **SC-006**: Packet detail view displays all protocol layers with 100% decode accuracy
- **SC-007**: Application bundle size ≤2MB (compressed)
- **SC-008**: Zero runtime dependencies (all libraries bundled)
- **SC-009**: Accessibility: Keyboard navigation supported, ARIA labels present, WCAG 2.1 Level AA compliant
- **SC-010**: Documentation includes usage guide with screenshots and examples

## Assumptions

- Users have modern browser with JavaScript enabled
- PCAP files are pre-generated (viewer does not capture traffic)
- Test results are in supported formats (JUnit XML, TAP, JSON)
- Analysis runs client-side (no server-side processing)
- Focus on visualization and analysis, not real-time monitoring

## Dependencies

- **External**: None (standalone web application)
- **Internal**: M4 (scenario test results), existing protocol decoders for parsing

## Out of Scope

- Real-time packet capture from web browser
- Server-side PCAP processing or storage
- User authentication or multi-user support
- Cloud storage integration
- Live streaming of packet data
- Advanced statistical analysis (clustering, ML-based anomaly detection)
- Mobile app versions (iOS/Android native)

## Implementation Notes

### Recommended Approach

**Phase 1 - Project Setup & File Loading** (1 week)
- Set up Vite + TypeScript + React project
- Implement file loading (File API)
- Create Web Worker for parsing
- Tests: 10+ for file loading and parsing

**Phase 2 - PCAP Parser (WebAssembly)** (1.5 weeks)
- Compile Wadjet C++ decoder to WebAssembly (emscripten)
- Create JavaScript bindings
- Implement PCAP/PCAPNG reader in Web Worker
- Tests: 15+ for PCAP parsing

**Phase 3 - Timeline Visualization** (1.5 weeks)
- Implement timeline component (React + D3.js or Recharts)
- Add zoom/pan/filter
- Virtual scrolling for large datasets
- Tests: 20+ for timeline rendering and interaction

**Phase 4 - Test Results Dashboard** (1 week)
- Parse JUnit XML, TAP, JSON
- Render test summary dashboard
- Display failure details
- Tests: 15+ for all result formats

**Phase 5 - Packet Detail View** (1 week)
- Render protocol layer decode
- Hex dump viewer
- Field highlighting and tooltips
- Tests: 10+ for detail view

**Phase 6 - Visualizations (Charts, Service Map)** (1.5 weeks)
- Bandwidth and packet rate charts
- Protocol distribution pie chart
- SOME/IP service map
- DoIP diagnostic session view
- Tests: 20+ for all visualizations

**Phase 7 - Search & Filter** (1 week)
- Implement search engine
- Protocol filter expressions
- Search result highlighting
- Tests: 15+ for search scenarios

**Phase 8 - Export & Polish** (1 week)
- Export to PCAP/CSV/HTML
- URL state management (permalinks)
- Responsive design refinement
- Accessibility improvements
- Tests: 10+ for export functionality

**Total Duration**: ~9 weeks

### Technology Stack

| Layer | Technology |
|-------|------------|
| Framework | React 18+ with TypeScript |
| Build | Vite (fast HMR, optimized bundles) |
| Charts | Recharts or D3.js |
| State | Zustand or Jotai (lightweight state management) |
| Styling | Tailwind CSS |
| Parser | WebAssembly (emscripten-compiled Wadjet decoders) |
| Testing | Vitest + React Testing Library |

### WebAssembly Integration

```typescript
// Example: Load Wadjet WASM module
import wasmModule from './wadjet.wasm?url';

let wadjetModule: WadjetModule;

async function initWadjet() {
    wadjetModule = await loadWadjetWasm(wasmModule);
}

// Web Worker: Parse PCAP
self.onmessage = async (e) => {
    const { type, data } = e.data;
    
    if (type === 'PARSE_PCAP') {
        const packets = await parsePcapWithWasm(data);
        self.postMessage({ type: 'PACKETS', packets });
    }
};
```

### Timeline Component Example

```tsx
interface TimelineProps {
    packets: Packet[];
    onPacketClick: (packet: Packet) => void;
    filters: ProtocolFilter[];
}

const Timeline: React.FC<TimelineProps> = ({ packets, onPacketClick, filters }) => {
    const filteredPackets = useMemo(() => 
        applyFilters(packets, filters), [packets, filters]);
    
    return (
        <VirtualScroller
            items={filteredPackets}
            renderItem={(pkt) => <PacketBar packet={pkt} onClick={onPacketClick} />}
            height={600}
        />
    );
};
```

### Test Count Target

**115+ tests** covering:
- File loading: 10 tests
- PCAP parsing: 15 tests
- Timeline: 20 tests
- Dashboard: 15 tests
- Packet detail: 10 tests
- Visualizations: 20 tests
- Search: 15 tests
- Export: 10 tests

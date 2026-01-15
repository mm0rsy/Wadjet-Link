# Protocol Completeness: API Contracts

## TcpConnection API
- `track_tcp_connection(packet: Packet) -> TcpConnection`
- `get_tcp_state(conn: TcpConnection) -> TcpState`
- `get_window_size(conn: TcpConnection) -> uint16`

## Ipv4Fragment API
- `decode_ipv4_fragment(packet: Packet) -> Ipv4Fragment`
- `reassemble_ipv4_fragments(fragments: List[Ipv4Fragment]) -> Datagram`

## SomeipTpMessage API
- `reassemble_someip_tp(segments: List[SomeipTpSegment]) -> SomeipTpMessage`

## UdpChecksumValidation API
- `validate_udp_checksum(packet: Packet, mode: str) -> UdpChecksumValidation`

## Compliance Gap API
- `report_compliance_gap(protocol: str, gap_type: str, description: str) -> None`

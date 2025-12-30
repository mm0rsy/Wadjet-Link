"""
PCAP file reading and writing utilities.

𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
"""

from __future__ import annotations

from typing import Iterator, List, Optional, Union
from pathlib import Path

from ._wadjet import (
    Packet,
    PcapReader as _PcapReader,
    PcapWriter as _PcapWriter,
)


def read_pcap(path: Union[str, Path]) -> List[Packet]:
    """
    Read all packets from a PCAP file.
    
    Args:
        path: Path to the PCAP file
    
    Returns:
        List of Packet objects
    
    Example:
        >>> packets = read_pcap("capture.pcap")
        >>> print(f"Read {len(packets)} packets")
    """
    path_str = str(path)
    with _PcapReader.open(path_str) as reader:
        return list(reader)


def iter_pcap(path: Union[str, Path]) -> Iterator[Packet]:
    """
    Iterate over packets in a PCAP file.
    
    More memory-efficient than read_pcap for large files.
    
    Args:
        path: Path to the PCAP file
    
    Yields:
        Packet objects
    
    Example:
        >>> for packet in iter_pcap("large_capture.pcap"):
        ...     process(packet)
    """
    path_str = str(path)
    with _PcapReader.open(path_str) as reader:
        yield from reader


def write_pcap(
    path: Union[str, Path],
    packets: List[Packet],
    link_type: int = 1,  # LINKTYPE_ETHERNET
) -> None:
    """
    Write packets to a PCAP file.
    
    Args:
        path: Output path for the PCAP file
        packets: List of packets to write
        link_type: PCAP link type (default: 1 = Ethernet)
    
    Example:
        >>> write_pcap("output.pcap", packets)
    """
    path_str = str(path)
    with _PcapWriter.create(path_str, link_type) as writer:
        for packet in packets:
            writer.write(packet)


class PcapReader:
    """
    Context manager for reading PCAP files.
    
    Example:
        >>> with PcapReader("capture.pcap") as reader:
        ...     for packet in reader:
        ...         process(packet)
    """
    
    def __init__(self, path: Union[str, Path]):
        self.path = str(path)
        self._reader: Optional[_PcapReader] = None
    
    def __enter__(self) -> "_PcapReader":
        self._reader = _PcapReader.open(self.path)
        return self._reader
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self._reader = None
        return False


class PcapWriter:
    """
    Context manager for writing PCAP files.
    
    Example:
        >>> with PcapWriter("output.pcap") as writer:
        ...     writer.write(packet)
    """
    
    def __init__(self, path: Union[str, Path], link_type: int = 1):
        self.path = str(path)
        self.link_type = link_type
        self._writer: Optional[_PcapWriter] = None
    
    def __enter__(self) -> "_PcapWriter":
        self._writer = _PcapWriter.create(self.path, self.link_type)
        return self._writer
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self._writer = None
        return False


def filter_pcap(
    input_path: Union[str, Path],
    output_path: Union[str, Path],
    predicate,
) -> int:
    """
    Filter packets from one PCAP file to another.
    
    Args:
        input_path: Input PCAP file path
        output_path: Output PCAP file path
        predicate: Function that returns True for packets to keep
    
    Returns:
        Number of packets written
    
    Example:
        >>> from wadjet import is_someip
        >>> count = filter_pcap("all.pcap", "someip_only.pcap", is_someip)
        >>> print(f"Wrote {count} SOME/IP packets")
    """
    count = 0
    with _PcapReader.open(str(input_path)) as reader:
        with _PcapWriter.create(str(output_path), 1) as writer:
            for packet in reader:
                if predicate(packet):
                    writer.write(packet)
                    count += 1
    return count


def merge_pcaps(
    input_paths: List[Union[str, Path]],
    output_path: Union[str, Path],
    sort_by_time: bool = True,
) -> int:
    """
    Merge multiple PCAP files into one.
    
    Args:
        input_paths: List of input PCAP file paths
        output_path: Output PCAP file path
        sort_by_time: If True, sort packets by timestamp
    
    Returns:
        Total number of packets written
    
    Example:
        >>> count = merge_pcaps(["a.pcap", "b.pcap"], "merged.pcap")
    """
    # Read all packets
    all_packets = []
    for path in input_paths:
        packets = read_pcap(path)
        all_packets.extend(packets)
    
    # Sort by timestamp if requested
    if sort_by_time and all_packets:
        all_packets.sort(key=lambda p: p.timestamp.total_nanoseconds())
    
    # Write to output
    write_pcap(output_path, all_packets)
    return len(all_packets)


def split_pcap(
    input_path: Union[str, Path],
    output_prefix: str,
    packets_per_file: int,
) -> List[str]:
    """
    Split a PCAP file into multiple smaller files.
    
    Args:
        input_path: Input PCAP file path
        output_prefix: Prefix for output files (e.g., "split" -> "split_000.pcap")
        packets_per_file: Maximum packets per output file
    
    Returns:
        List of output file paths created
    
    Example:
        >>> files = split_pcap("large.pcap", "chunk", 1000)
        >>> print(f"Created {len(files)} files")
    """
    output_files = []
    current_packets = []
    file_index = 0
    
    for packet in iter_pcap(input_path):
        current_packets.append(packet)
        
        if len(current_packets) >= packets_per_file:
            output_path = f"{output_prefix}_{file_index:03d}.pcap"
            write_pcap(output_path, current_packets)
            output_files.append(output_path)
            current_packets = []
            file_index += 1
    
    # Write remaining packets
    if current_packets:
        output_path = f"{output_prefix}_{file_index:03d}.pcap"
        write_pcap(output_path, current_packets)
        output_files.append(output_path)
    
    return output_files

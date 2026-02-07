"""Type stubs for wadjet.distributed module (pybind11 C++ bindings)"""

from typing import List, Optional, Tuple
from enum import IntEnum

class ClockSyncMethod(IntEnum):
    """Clock synchronization method"""
    NONE: int
    NTP: int
    GPTP: int
    UNKNOWN: int

class ClockSyncStatus:
    """Clock synchronization status information"""
    method: ClockSyncMethod
    is_synchronized: bool
    estimated_offset_ns: int
    max_error_ns: int
    grandmaster_id: str
    
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...

class SyncBarrier:
    """Synchronization barrier for coordinating multi-node test execution"""
    
    def __init__(self, barrier_id: str, expected_participants: int) -> None: ...
    def wait(self, timeout_ns: int = 0) -> None: ...
    def is_satisfied(self) -> bool: ...
    def get_participant_count(self) -> int: ...
    def get_barrier_id(self) -> str: ...
    def get_expected_participants(self) -> int: ...

class TimestampNormalizer:
    """Detects and manages clock synchronization for distributed tests"""
    
    def __init__(self) -> None: ...
    def detect_sync_status(self) -> ClockSyncStatus: ...
    def is_synchronized(self) -> bool: ...
    def normalize_timestamp(self, timestamp_ns: int) -> int: ...
    def verify_gptp_health(self) -> bool: ...
    def get_sync_status(self) -> ClockSyncStatus: ...

class DistributedMatchResult:
    """Result of a distributed assertion match"""
    matched: bool
    node_id: str
    timestamp_ns: int
    error_code: int
    error_message: str
    expected_condition: str
    actual_condition: str
    packet_context: str
    failure_timestamp_ns: int
    
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...

class DistributedMatcher:
    """Base class for distributed packet matchers"""
    def match(self) -> DistributedMatchResult: ...

class CoordinatorConfig:
    """Configuration for TestCoordinator"""
    grpc_host: str
    grpc_port: int
    heartbeat_timeout_ms: int
    barrier_sync_timeout_ms: int
    enable_gptp_sync: bool
    enable_ntp_sync: bool
    
    def __init__(self) -> None: ...

class NodeResult:
    """Test results from a single node"""
    node_id: str
    healthy: bool
    assertions: List
    passed_count: int
    failed_count: int
    total_duration_ns: int
    pcap_file_path: str
    error_message: Optional[str]
    
    def __init__(self) -> None: ...

class AggregatedResult:
    """Combined test results from all nodes"""
    test_name: str
    node_results: List[NodeResult]
    test_start_time_ns: int
    test_end_time_ns: int
    overall_status: int  # 0=Passed, 1=Failed, 2=Error
    total_duration_ns: int
    total_assertions: int
    passed_assertions: int
    failed_assertions: int
    
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...

class TestCoordinator:
    """Orchestrates distributed packet capture tests"""
    
    def __init__(self, config: CoordinatorConfig) -> None: ...
    def start(self) -> None: ...
    def stop(self) -> None: ...
    def register_node(self, node_info: dict) -> None: ...
    def create_barrier(self, barrier_id: str, expected_count: int) -> None: ...
    def run_test(self, test_name: str, duration_ms: int) -> None: ...
    def get_results(self) -> AggregatedResult: ...
    def save_junit_report(self, output_path: str) -> None: ...
    def detect_coordinator_failure(self) -> bool: ...

class NodeConfig:
    """Configuration for TestNode"""
    node_id: str
    coordinator_host: str
    coordinator_port: int
    grpc_port: int
    heartbeat_timeout_ms: int
    
    def __init__(self) -> None: ...

class TestNode:
    """Captures packets on a single node in a distributed test"""
    
    def __init__(self, config: NodeConfig) -> None: ...
    def start_capture(self, interface_name: str) -> None: ...
    def stop_capture(self) -> None: ...
    def wait_barrier(self, timeout_ms: int = 0) -> None: ...
    def get_clock_sync_status(self) -> ClockSyncStatus: ...
    def detect_coordinator_failure(self) -> bool: ...
    def save_partial_results(self) -> None: ...
    def get_node_id(self) -> str: ...

"""
pytest fixtures for wadjet.distributed distributed testing.

Provides fixtures for:
- Test coordinator setup/teardown
- Test node setup/teardown
- Barrier synchronization
- Result verification

Example:
    def test_multicast_discovery(coordinator, nodes):
        '''Test multicast packet discovery across 3 nodes'''
        coordinator.run_test("multicast_discovery", 5000)
        results = coordinator.get_results()
        assert results.overall_status == 0  # PASSED
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from typing import List, Tuple

import wadjet.distributed as dist


# ============================================================================
# Coordinator Fixtures
# ============================================================================

@pytest.fixture(scope="function")
def coordinator_config():
    """Create a coordinator configuration with test defaults"""
    config = dist.CoordinatorConfig()
    config.grpc_host = "localhost"
    config.grpc_port = 50051
    config.heartbeat_timeout_ms = 5000
    config.barrier_sync_timeout_ms = 10000
    config.enable_gptp_sync = True
    config.enable_ntp_sync = True
    return config


@pytest.fixture(scope="function")
def coordinator(coordinator_config):
    """Create and start a test coordinator"""
    coordinator = dist.TestCoordinator(coordinator_config)
    coordinator.start()
    yield coordinator
    coordinator.stop()


# ============================================================================
# Node Fixtures
# ============================================================================

@pytest.fixture(scope="function")
def node_config_factory():
    """Factory for creating node configurations"""
    def _make_node_config(node_id: str, port: int = 50051) -> dist.NodeConfig:
        config = dist.NodeConfig()
        config.node_id = node_id
        config.coordinator_host = "localhost"
        config.coordinator_port = port
        config.heartbeat_timeout_ms = 5000
        return config
    return _make_node_config


@pytest.fixture(scope="function")
def node_factory(node_config_factory):
    """Factory for creating test nodes"""
    nodes = []
    
    def _make_node(node_id: str, interface: str = "lo") -> dist.TestNode:
        config = node_config_factory(node_id)
        node = dist.TestNode(config)
        nodes.append(node)
        return node
    
    yield _make_node
    
    # Cleanup
    for node in nodes:
        try:
            node.stop_capture()
        except:
            pass


@pytest.fixture(scope="function")
def nodes(node_factory) -> List[dist.TestNode]:
    """Create 3 test nodes for distributed testing"""
    return [
        node_factory("node1", "lo"),
        node_factory("node2", "lo"),
        node_factory("node3", "lo"),
    ]


# ============================================================================
# Barrier Fixtures
# ============================================================================

@pytest.fixture(scope="function")
def barrier_factory():
    """Factory for creating synchronization barriers"""
    barriers = []
    
    def _make_barrier(barrier_id: str, participants: int, 
                     timeout_ms: int = 10000) -> dist.SyncBarrier:
        barrier = dist.SyncBarrier(barrier_id, participants)
        barriers.append(barrier)
        return barrier
    
    yield _make_barrier
    
    # Cleanup
    for barrier in barriers:
        try:
            del barrier
        except:
            pass


@pytest.fixture(scope="function")
def barrier(barrier_factory) -> dist.SyncBarrier:
    """Create a test barrier for 3-node synchronization"""
    return barrier_factory("test_barrier", 3)


# ============================================================================
# Clock Synchronization Fixtures
# ============================================================================

@pytest.fixture(scope="function")
def timestamp_normalizer() -> dist.TimestampNormalizer:
    """Create a timestamp normalizer for clock synchronization"""
    return dist.TimestampNormalizer()


@pytest.fixture(scope="function")
def clock_sync_status(timestamp_normalizer) -> dist.ClockSyncStatus:
    """Get current clock synchronization status"""
    return timestamp_normalizer.detect_sync_status()


# ============================================================================
# Temporary Directory Fixtures
# ============================================================================

@pytest.fixture(scope="function")
def temp_pcap_dir():
    """Create temporary directory for PCAP files"""
    temp_dir = tempfile.mkdtemp(prefix="wadjet_pcap_")
    yield Path(temp_dir)
    shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture(scope="function")
def temp_results_dir():
    """Create temporary directory for test results"""
    temp_dir = tempfile.mkdtemp(prefix="wadjet_results_")
    yield Path(temp_dir)
    shutil.rmtree(temp_dir, ignore_errors=True)


# ============================================================================
# Result Verification Fixtures
# ============================================================================

@pytest.fixture(scope="function")
def assert_results():
    """Helper for asserting test results"""
    def _assert(results: dist.AggregatedResult,
                expected_status: int = 0,  # 0=Passed, 1=Failed, 2=Error
                min_assertions: int = 1):
        """Assert results meet expectations"""
        assert isinstance(results, dist.AggregatedResult)
        assert results.overall_status == expected_status, \
            f"Expected status {expected_status}, got {results.overall_status}"
        assert results.total_assertions >= min_assertions, \
            f"Expected at least {min_assertions} assertions, got {results.total_assertions}"
        assert results.passed_assertions >= 0
        assert results.failed_assertions >= 0
        assert results.node_count > 0
        return results
    return _assert


# ============================================================================
# Parametrized Test Fixtures
# ============================================================================

@pytest.fixture(params=[1, 2, 3])
def node_count(request) -> int:
    """Parametrize tests to run with 1, 2, or 3 nodes"""
    return request.param


@pytest.fixture(params=[1000, 5000, 10000])
def test_duration_ms(request) -> int:
    """Parametrize tests to run for various durations"""
    return request.param


# ============================================================================
# Integration Test Fixtures (requires running infrastructure)
# ============================================================================

@pytest.fixture(scope="session")
def coordinator_available():
    """Check if coordinator is available for integration tests"""
    try:
        config = dist.CoordinatorConfig()
        coord = dist.TestCoordinator(config)
        coord.start()
        coord.stop()
        return True
    except Exception:
        return False


@pytest.mark.skipif(
    not pytest.config.getini("run_integration"),
    reason="Integration tests disabled"
)
@pytest.fixture(scope="function")
def integration_coordinator():
    """Create coordinator for integration tests"""
    config = dist.CoordinatorConfig()
    coord = dist.TestCoordinator(config)
    coord.start()
    yield coord
    coord.stop()

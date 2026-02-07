"""Wadjet-Link Distributed Testing Framework - Python Package

High-level distributed packet capture and testing primitives.

Example:
    >>> import wadjet.distributed as dist
    >>> 
    >>> # Create coordinator
    >>> config = dist.CoordinatorConfig()
    >>> coordinator = dist.TestCoordinator(config)
    >>> coordinator.start()
    >>> 
    >>> # Create and start nodes
    >>> node_config = dist.NodeConfig()
    >>> node_config.node_id = "node1"
    >>> node = dist.TestNode(node_config)
    >>> node.start_capture("eth0")
    >>> 
    >>> # Run test
    >>> coordinator.run_test("my_test", 5000)  # 5 second test
    >>> results = coordinator.get_results()
    >>> print(f"Passed: {results.passed_assertions}/{results.total_assertions}")
"""

try:
    # Import C++ bindings (compiled pybind11 module)
    from ._distributed import (
        # Enums
        ClockSyncMethod,
        # Classes - Synchronization
        SyncBarrier,
        # Classes - Clock Synchronization
        TimestampNormalizer,
        ClockSyncStatus,
        # Classes - Matchers
        DistributedMatcher,
        DistributedMatchResult,
        # Classes - Coordinator
        TestCoordinator,
        CoordinatorConfig,
        # Classes - Node
        TestNode,
        NodeConfig,
        # Classes - Results
        NodeResult,
        AggregatedResult,
    )
except ImportError as e:
    raise ImportError(
        "Failed to import wadjet.distributed C++ bindings. "
        "Please ensure the pybind11 module is compiled and installed. "
        f"Details: {e}"
    ) from e

__all__ = [
    # Enums
    "ClockSyncMethod",
    # Synchronization
    "SyncBarrier",
    # Clock Management
    "TimestampNormalizer",
    "ClockSyncStatus",
    # Matchers
    "DistributedMatcher",
    "DistributedMatchResult",
    # Coordinator
    "TestCoordinator",
    "CoordinatorConfig",
    # Node
    "TestNode",
    "NodeConfig",
    # Results
    "NodeResult",
    "AggregatedResult",
]

__version__ = "0.1.0"
__author__ = "Wadjet-Link Contributors"

# Convenience function to create a pre-configured coordinator
def create_coordinator(
    host: str = "localhost",
    port: int = 50051,
    heartbeat_timeout_ms: int = 5000,
) -> TestCoordinator:
    """Create a pre-configured test coordinator.
    
    Args:
        host: Coordinator hostname/IP
        port: Coordinator gRPC port
        heartbeat_timeout_ms: Heartbeat timeout in milliseconds
        
    Returns:
        Configured TestCoordinator instance
        
    Example:
        >>> coordinator = dist.create_coordinator(host="localhost", port=50051)
        >>> coordinator.start()
    """
    config = CoordinatorConfig()
    config.grpc_host = host
    config.grpc_port = port
    config.heartbeat_timeout_ms = heartbeat_timeout_ms
    config.enable_gptp_sync = True
    config.enable_ntp_sync = True
    return TestCoordinator(config)


# Convenience function to create a pre-configured node
def create_node(
    node_id: str,
    coordinator_host: str = "localhost",
    coordinator_port: int = 50051,
) -> TestNode:
    """Create a pre-configured test node.
    
    Args:
        node_id: Unique identifier for this node
        coordinator_host: Coordinator hostname/IP
        coordinator_port: Coordinator gRPC port
        
    Returns:
        Configured TestNode instance
        
    Example:
        >>> node = dist.create_node("node1", "localhost", 50051)
        >>> node.start_capture("eth0")
    """
    config = NodeConfig()
    config.node_id = node_id
    config.coordinator_host = coordinator_host
    config.coordinator_port = coordinator_port
    return TestNode(config)

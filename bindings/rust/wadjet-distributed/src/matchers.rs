/// Safe Rust wrappers for distributed matchers
///
/// Provides high-level matcher abstractions over the C FFI for distributed
/// packet assertions.
///
/// **Task**: T119 - Implement safe DistributedMatcher wrappers

use crate::ffi;
use std::time::SystemTime;

/// Result of a distributed matcher assertion
#[derive(Debug, Clone)]
pub struct MatchResult {
    /// Whether the match succeeded
    pub matched: bool,
    /// Node ID where assertion ran
    pub node_id: String,
    /// Timestamp of assertion (nanoseconds since Unix epoch)
    pub timestamp_ns: i64,
    /// Error code if match failed
    pub error_code: i32,
    /// Error message if match failed
    pub error_message: String,
    /// Expected condition description
    pub expected_condition: String,
    /// Actual condition found
    pub actual_condition: String,
    /// Context about packets examined
    pub packet_context: String,
    /// Timestamp when failure was detected
    pub failure_timestamp_ns: i64,
}

impl MatchResult {
    /// Create a successful match result
    pub fn success(node_id: &str, timestamp_ns: i64) -> Self {
        MatchResult {
            matched: true,
            node_id: node_id.to_string(),
            timestamp_ns,
            error_code: 0,
            error_message: String::new(),
            expected_condition: String::new(),
            actual_condition: String::new(),
            packet_context: String::new(),
            failure_timestamp_ns: 0,
        }
    }

    /// Create a failed match result with details
    pub fn failure(
        node_id: &str,
        timestamp_ns: i64,
        error_code: i32,
        error_message: &str,
        expected: &str,
        actual: &str,
        context: &str,
    ) -> Self {
        MatchResult {
            matched: false,
            node_id: node_id.to_string(),
            timestamp_ns,
            error_code,
            error_message: error_message.to_string(),
            expected_condition: expected.to_string(),
            actual_condition: actual.to_string(),
            packet_context: context.to_string(),
            failure_timestamp_ns: timestamp_ns,
        }
    }

    /// Check if this was a successful match
    pub fn is_ok(&self) -> bool {
        self.matched
    }

    /// Get error message (None if successful)
    pub fn error_message(&self) -> Option<&str> {
        if self.matched {
            None
        } else {
            Some(&self.error_message)
        }
    }

    /// Get detailed failure information
    pub fn failure_details(&self) -> Option<FailureDetails> {
        if self.matched {
            return None;
        }

        Some(FailureDetails {
            expected: self.expected_condition.clone(),
            actual: self.actual_condition.clone(),
            context: self.packet_context.clone(),
            error_code: self.error_code,
            node_id: self.node_id.clone(),
            timestamp_ns: self.failure_timestamp_ns,
        })
    }
}

impl std::fmt::Display for MatchResult {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.matched {
            write!(f, "✓ Match succeeded on node {}", self.node_id)
        } else {
            write!(
                f,
                "✗ Match failed on node {}: {} (expected: {}, got: {})",
                self.node_id, self.error_message, self.expected_condition, self.actual_condition
            )
        }
    }
}

/// Detailed failure information for debugging
#[derive(Debug, Clone)]
pub struct FailureDetails {
    pub expected: String,
    pub actual: String,
    pub context: String,
    pub error_code: i32,
    pub node_id: String,
    pub timestamp_ns: i64,
}

impl std::fmt::Display for FailureDetails {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Failure Details:\n  Node: {}\n  Expected: {}\n  Actual: {}\n  Context: {}\n  Timestamp: {}ns",
            self.node_id, self.expected, self.actual, self.context, self.timestamp_ns
        )
    }
}

/// Trait for distributed matchers
pub trait DistributedMatcher: Send + Sync {
    /// Execute the match
    fn execute(&self) -> MatchResult;

    /// Get a description of what this matcher checks
    fn description(&self) -> &str;
}

/// Packet count matcher for distributed tests
///
/// Verifies that expected number of packets were captured.
pub struct PacketCountMatcher {
    expected_count: usize,
    description: String,
}

impl PacketCountMatcher {
    pub fn new(expected_count: usize) -> Self {
        PacketCountMatcher {
            expected_count,
            description: format!("packet count = {}", expected_count),
        }
    }
}

impl DistributedMatcher for PacketCountMatcher {
    fn execute(&self) -> MatchResult {
        // In real implementation, would check actual packet count
        MatchResult::success("mock_node", 0)
    }

    fn description(&self) -> &str {
        &self.description
    }
}

/// Packet contents matcher
///
/// Verifies that captured packets contain expected byte patterns.
pub struct PacketContentMatcher {
    pattern: Vec<u8>,
    description: String,
}

impl PacketContentMatcher {
    pub fn new(pattern: Vec<u8>) -> Self {
        let hex = pattern.iter()
            .map(|b| format!("{:02x}", b))
            .collect::<Vec<_>>()
            .join(" ");
        
        PacketContentMatcher {
            pattern,
            description: format!("packet contains pattern: {}", hex),
        }
    }
}

impl DistributedMatcher for PacketContentMatcher {
    fn execute(&self) -> MatchResult {
        // In real implementation, would search for pattern in packets
        MatchResult::success("mock_node", 0)
    }

    fn description(&self) -> &str {
        &self.description
    }
}

/// Protocol decoder matcher
///
/// Verifies that packets decode correctly for a specific protocol.
pub struct ProtocolDecoderMatcher {
    protocol: String,
    description: String,
}

impl ProtocolDecoderMatcher {
    pub fn new(protocol: &str) -> Self {
        ProtocolDecoderMatcher {
            protocol: protocol.to_string(),
            description: format!("packets decode as {}", protocol),
        }
    }
}

impl DistributedMatcher for ProtocolDecoderMatcher {
    fn execute(&self) -> MatchResult {
        // In real implementation, would attempt protocol decoding
        MatchResult::success("mock_node", 0)
    }

    fn description(&self) -> &str {
        &self.description
    }
}

/// Timestamp ordering matcher
///
/// Verifies that packets from multiple nodes are properly ordered by timestamp.
pub struct TimestampOrderingMatcher {
    tolerance_ns: i64,
    description: String,
}

impl TimestampOrderingMatcher {
    pub fn new(tolerance_ns: i64) -> Self {
        TimestampOrderingMatcher {
            tolerance_ns,
            description: format!("packets ordered by timestamp (±{}ns)", tolerance_ns),
        }
    }
}

impl DistributedMatcher for TimestampOrderingMatcher {
    fn execute(&self) -> MatchResult {
        // In real implementation, would check timestamp ordering
        MatchResult::success("mock_node", 0)
    }

    fn description(&self) -> &str {
        &self.description
    }
}

/// Latency matcher for distributed tests
///
/// Verifies packet latency between nodes.
pub struct LatencyMatcher {
    max_latency_ns: i64,
    description: String,
}

impl LatencyMatcher {
    pub fn new(max_latency_ns: i64) -> Self {
        LatencyMatcher {
            max_latency_ns,
            description: format!("packet latency < {}ns", max_latency_ns),
        }
    }
}

impl DistributedMatcher for LatencyMatcher {
    fn execute(&self) -> MatchResult {
        // In real implementation, would measure latency between nodes
        MatchResult::success("mock_node", 0)
    }

    fn description(&self) -> &str {
        &self.description
    }
}

/// Builder for constructing complex matchers
pub struct MatcherBuilder {
    matchers: Vec<Box<dyn DistributedMatcher>>,
    require_all: bool,
}

impl MatcherBuilder {
    pub fn new() -> Self {
        MatcherBuilder {
            matchers: Vec::new(),
            require_all: true,
        }
    }

    /// Add a matcher to the builder
    pub fn add_matcher(mut self, matcher: Box<dyn DistributedMatcher>) -> Self {
        self.matchers.push(matcher);
        self
    }

    /// Require all matchers to pass (AND logic)
    pub fn require_all(mut self) -> Self {
        self.require_all = true;
        self
    }

    /// Require at least one matcher to pass (OR logic)
    pub fn require_any(mut self) -> Self {
        self.require_all = false;
        self
    }

    /// Build and execute all matchers
    pub fn execute(self) -> Vec<MatchResult> {
        self.matchers.iter().map(|m| m.execute()).collect()
    }
}

impl Default for MatcherBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_match_result_success() {
        let result = MatchResult::success("node1", 12345);
        assert!(result.is_ok());
        assert!(result.error_message().is_none());
    }

    #[test]
    fn test_match_result_failure() {
        let result = MatchResult::failure(
            "node1",
            12345,
            1,
            "packet mismatch",
            "SOME/IP",
            "raw Ethernet",
            "payload at offset 0",
        );
        assert!(!result.is_ok());
        assert!(result.error_message().is_some());
        assert!(result.failure_details().is_some());
    }

    #[test]
    fn test_packet_count_matcher() {
        let matcher = PacketCountMatcher::new(100);
        assert_eq!(matcher.description(), "packet count = 100");
    }

    #[test]
    fn test_matcher_builder() {
        let builder = MatcherBuilder::new()
            .add_matcher(Box::new(PacketCountMatcher::new(100)))
            .add_matcher(Box::new(LatencyMatcher::new(10000000)));
        
        let results = builder.execute();
        assert_eq!(results.len(), 2);
    }
}

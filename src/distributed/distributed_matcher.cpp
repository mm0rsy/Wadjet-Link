#include "wadjet/distributed/distributed_matcher.hpp"

namespace wadjet::distributed {

// Note: Factory functions are implemented in individual matcher files:
// - ExpectMessageFlow() in matchers/expect_message_flow.cpp
// - WithinLatency() in matchers/within_latency.cpp
// - HappensBefore() in matchers/happens_before.cpp
// - MustNotSeeOn() in matchers/must_not_see_on.cpp
//
// This organization allows for better separation of concerns and easier testing.

}  // namespace wadjet::distributed

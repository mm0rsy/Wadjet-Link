/// @file request_correlator.cpp
/// @brief UDS request/response correlation implementation

#include "wadjet/protocols/diagnostic/request_correlator.hpp"

namespace wadjet::protocols::diagnostic {

RequestCorrelator::RequestCorrelator(Options opts)
    : options_(std::move(opts)) {}

void RequestCorrelator::record_request(const uds::UdsHeader& header,
                                       const uds::UdsServiceMessage& message,
                                       const TransportInfo& transport) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create pending request
    PendingRequest pending;
    pending.request.header = header;
    pending.request.message = message;
    pending.request.transport = transport;
    pending.request.timestamp = transport.timestamp;
    pending.suppress_positive_response = header.suppress_positive_response;
    pending.pending_count = 0;

    // Get or create pending list for address pair
    AddressPairKey key{transport.source_address, transport.target_address};
    auto& pending_list = pending_requests_[key];

    // Limit pending requests
    if (pending_list.size() >= options_.max_pending_per_address) {
        // Remove oldest
        pending_list.pop_front();
    }

    pending_list.push_back(std::move(pending));
    stats_.requests_recorded++;

    emit_event(CorrelationEvent::RequestSent, nullptr, &pending_list.back());
}

std::shared_ptr<RequestResponsePair> RequestCorrelator::process_response(
    const uds::UdsHeader& header,
    const uds::UdsServiceMessage& message,
    const TransportInfo& transport) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find matching pending request
    auto* pending = find_pending_for_response(header, transport);

    if (!pending) {
        stats_.responses_unmatched++;
        emit_event(CorrelationEvent::UnmatchedResponse, nullptr, nullptr);
        return nullptr;
    }

    // Check for ResponsePending (NRC 0x78)
    if (header.is_negative_response() &&
        header.negative_response_code == static_cast<std::uint8_t>(uds::NRC::RequestCorrectlyReceivedResponsePending)) {
        pending->pending_count++;
        stats_.response_pending_count++;
        emit_event(CorrelationEvent::ResponsePending, nullptr, pending);
        return nullptr;  // Wait for actual response
    }

    // Create completed pair
    auto pair = std::make_shared<RequestResponsePair>();
    pair->request = pending->request;
    pair->response = RequestResponsePair::Response{};
    pair->response->header = header;
    pair->response->message = message;
    pair->response->transport = transport;
    pair->response->timestamp = transport.timestamp;
    pair->response->pending_count = pending->pending_count;

    stats_.responses_matched++;

    if (header.is_negative_response()) {
        stats_.negative_responses++;
        emit_event(CorrelationEvent::NegativeResponse, pair.get(), nullptr);
    } else {
        stats_.positive_responses++;
        emit_event(CorrelationEvent::PositiveResponse, pair.get(), nullptr);
    }

    emit_event(CorrelationEvent::ResponseReceived, pair.get(), nullptr);

    // Remove pending request (find its position again since we need iterator)
    AddressPairKey response_key{transport.target_address, transport.source_address};
    auto it = pending_requests_.find(response_key);
    if (it != pending_requests_.end()) {
        // Find and remove the matching pending request
        auto& list = it->second;
        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            if (iter->request.header.service_id == header.service_id) {
                list.erase(iter);
                break;
            }
        }
    }

    // Store in completed history
    completed_.push_back(pair);
    if (completed_.size() > MAX_COMPLETED_HISTORY) {
        completed_.pop_front();
    }

    return pair;
}

PendingRequest* RequestCorrelator::find_pending_for_response(
    const uds::UdsHeader& header,
    const TransportInfo& transport) {
    // Response addresses are reversed from request
    AddressPairKey key{transport.target_address, transport.source_address};

    auto it = pending_requests_.find(key);
    if (it == pending_requests_.end()) {
        return nullptr;
    }

    auto& pending_list = it->second;
    if (pending_list.empty()) {
        return nullptr;
    }

    // For negative responses, match by the rejected service ID
    uds::ServiceID request_sid = header.service_id;

    // Find matching request by service ID
    for (auto& pending : pending_list) {
        if (pending.request.header.service_id == request_sid) {
            return &pending;
        }
    }

    // If no exact match, return the oldest pending request
    // (in case of ResponsePending or timing issues)
    return &pending_list.front();
}

std::size_t RequestCorrelator::pending_count(
    LogicalAddress source, LogicalAddress target) const {
    std::lock_guard<std::mutex> lock(mutex_);

    AddressPairKey key{source, target};
    auto it = pending_requests_.find(key);
    if (it == pending_requests_.end()) {
        return 0;
    }
    return it->second.size();
}

std::size_t RequestCorrelator::total_pending() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::size_t total = 0;
    for (const auto& [key, list] : pending_requests_) {
        total += list.size();
    }
    return total;
}

std::vector<PendingRequest> RequestCorrelator::get_pending(
    LogicalAddress source, LogicalAddress target) const {
    std::lock_guard<std::mutex> lock(mutex_);

    AddressPairKey key{source, target};
    auto it = pending_requests_.find(key);
    if (it == pending_requests_.end()) {
        return {};
    }
    return std::vector<PendingRequest>(it->second.begin(), it->second.end());
}

std::vector<std::shared_ptr<RequestResponsePair>>
RequestCorrelator::get_completed(std::size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<RequestResponsePair>> result;
    result.reserve(std::min(max_count, completed_.size()));

    auto it = completed_.rbegin();
    while (result.size() < max_count && it != completed_.rend()) {
        result.push_back(*it);
        ++it;
    }

    return result;
}

std::vector<std::shared_ptr<RequestResponsePair>>
RequestCorrelator::get_completed_by_service(uds::ServiceID service,
                                            std::size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<RequestResponsePair>> result;

    for (auto it = completed_.rbegin();
         it != completed_.rend() && result.size() < max_count; ++it) {
        if ((*it)->request.header.service_id == service) {
            result.push_back(*it);
        }
    }

    return result;
}

std::size_t RequestCorrelator::check_timeouts() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::size_t timeout_count = 0;

    for (auto& [key, list] : pending_requests_) {
        auto it = list.begin();
        while (it != list.end()) {
            if (it->is_timed_out(options_.timing)) {
                emit_event(CorrelationEvent::RequestTimeout, nullptr, &(*it));
                stats_.pending_timeouts++;
                timeout_count++;
                it = list.erase(it);
            } else {
                ++it;
            }
        }
    }

    return timeout_count;
}

void RequestCorrelator::set_timing(const DiagnosticTiming& timing) {
    std::lock_guard<std::mutex> lock(mutex_);
    options_.timing = timing;
}

void RequestCorrelator::on_event(CorrelationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void RequestCorrelator::clear_callbacks() {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.clear();
}

void RequestCorrelator::clear_pending() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_requests_.clear();
}

void RequestCorrelator::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    completed_.clear();
}

void RequestCorrelator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_requests_.clear();
    completed_.clear();
}

RequestCorrelator::Statistics RequestCorrelator::statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void RequestCorrelator::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
}

void RequestCorrelator::emit_event(CorrelationEvent event,
                                   const RequestResponsePair* pair,
                                   const PendingRequest* pending) {
    for (const auto& callback : callbacks_) {
        callback(event, pair, pending);
    }
}

}  // namespace wadjet::protocols::diagnostic

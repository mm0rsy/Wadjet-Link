#include "wadjet/distributed/grpc/client.hpp"
#include "wadjet/distributed/types.hpp"
#include "wadjet/distributed/sync_barrier.hpp"

// Proto generated includes with compiler warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wconversion"
#include "distributed_test.pb.h"
#pragma GCC diagnostic pop

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <fstream>
#include <sstream>

namespace wadjet::distributed {

using namespace v1;

auto DistributedTestClient::create(const std::string& coordinator_address)
    -> std::unique_ptr<DistributedTestClient> {
    auto client = std::make_unique<DistributedTestClient>(coordinator_address);
    if (client->connect()) {
        return client;
    }
    return nullptr;
}

// T261: Create client with TLS credentials
auto DistributedTestClient::create_with_tls(const std::string& coordinator_address,
                                            const std::string& tls_cert_path,
                                            const std::string& tls_key_path,
                                            const std::string& tls_ca_path)
    -> std::unique_ptr<DistributedTestClient> {
    auto client = std::make_unique<DistributedTestClient>(coordinator_address);
    
    // Load TLS credentials from files
    std::ifstream cert_file(tls_cert_path);
    std::ifstream key_file(tls_key_path);
    std::ifstream ca_file(tls_ca_path);
    
    if (!cert_file || !key_file) {
        return nullptr;  // TLS files not found
    }
    
    std::stringstream cert_stream, key_stream, ca_stream;
    cert_stream << cert_file.rdbuf();
    key_stream << key_file.rdbuf();
    if (ca_file) {
        ca_stream << ca_file.rdbuf();
    }
    
    // Create SslCredentialsOptions with client certificate
    grpc::SslCredentialsOptions opts;
    opts.pem_client_cert_chain = cert_stream.str();
    opts.pem_private_key = key_stream.str();
    if (!ca_stream.str().empty()) {
        opts.pem_root_certs = ca_stream.str();
    }
    
    client->tls_credentials_ = grpc::SslCredentials(opts);
    client->use_tls_ = true;
    
    if (client->connect()) {
        return client;
    }
    return nullptr;
}

DistributedTestClient::DistributedTestClient(const std::string& coordinator_address)
    : coordinator_address_(coordinator_address) {}

DistributedTestClient::DistributedTestClient(DistributedTestClient&&) noexcept = default;
DistributedTestClient& DistributedTestClient::operator=(DistributedTestClient&&) noexcept = default;

DistributedTestClient::~DistributedTestClient() = default;

auto DistributedTestClient::connect() -> bool {
    try {
        // Create gRPC channel to coordinator
        // T261: Use TLS credentials if configured, otherwise use insecure
        std::shared_ptr<grpc::ChannelCredentials> credentials;
        if (use_tls_) {
            credentials = tls_credentials_;
        } else {
            credentials = grpc::InsecureChannelCredentials();
        }
        
        channel_ = grpc::CreateChannel(
            coordinator_address_,
            credentials
        );
        
        if (!channel_) {
            return false;
        }
        
        // Create stub for the DistributedTestService
        stub_ = DistributedTestService::NewStub(channel_);
        
        return true;
    } catch (...) {
        return false;
    }
}

// T204: RegisterNode RPC client call
auto DistributedTestClient::register_node(const NodeInfo& node_info) -> bool {
    if (!stub_) {
        return false;
    }
    
    try {
        RegisterNodeRequest request;
        RegisterNodeResponse response;
        
        // Populate request from node info
        request.set_node_id(node_info.id);
        request.set_hostname(node_info.hostname);
        
        for (const auto& iface : node_info.capture_interfaces) {
            request.add_capture_interfaces(iface);
        }
        
        for (const auto& [key, value] : node_info.metadata) {
            (*request.mutable_metadata())[key] = value;
        }
        
        // Set protocol version
        auto version = request.mutable_protocol_version();
        version->set_major(1);
        version->set_minor(0);
        version->set_patch(0);
        
        // Call RegisterNode RPC
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        
        grpc::Status status = stub_->RegisterNode(&context, request, &response);
        
        return status.ok() && response.success();
    } catch (...) {
        return false;
    }
}

// T205: Heartbeat RPC client call
auto DistributedTestClient::send_heartbeat(const std::string& node_id,
                                           std::chrono::milliseconds timeout_ms) -> bool {
    if (!stub_) {
        return false;
    }
    
    try {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + timeout_ms);
        
        auto stream = stub_->Heartbeat(&context);
        
        // Send heartbeat request
        HeartbeatRequest request;
        request.set_node_id(node_id);
        request.set_timestamp_ns(
            static_cast<int64_t>(std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count())
        );
        
        if (!stream->Write(request)) {
            return false;
        }
        
        // Read response
        HeartbeatResponse response;
        if (!stream->Read(&response)) {
            return false;
        }
        
        // Close stream
        stream->WritesDone();
        grpc::Status status = stream->Finish();
        
        return status.ok() && response.acknowledged();
    } catch (...) {
        return false;
    }
}

// T206: WaitBarrier RPC client call
auto DistributedTestClient::wait_barrier(const std::string& node_id,
                                        const std::string& barrier_id,
                                        std::chrono::milliseconds timeout_ms) -> BarrierResult {
    BarrierResult result;
    
    if (!stub_) {
        return result;
    }
    
    try {
        WaitBarrierRequest request;
        WaitBarrierResponse response;
        
        request.set_node_id(node_id);
        request.set_barrier_id(barrier_id);
        request.set_arrival_timestamp_ns(
            static_cast<int64_t>(std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count())
        );
        
        // Call WaitBarrier RPC
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + timeout_ms);
        
        grpc::Status status = stub_->WaitBarrier(&context, request, &response);
        
        if (!status.ok()) {
            return result;
        }
        
        // Convert proto response to BarrierResult
        result.proceed = response.proceed();
        result.sync_timestamp_ns = response.sync_timestamp_ns();
        result.wait_duration = std::chrono::milliseconds(response.wait_duration_ms());
        
        for (const auto& node : response.participating_nodes()) {
            result.participating_nodes.push_back(node);
        }
        
        for (const auto& node : response.missing_nodes()) {
            result.missing_nodes.push_back(node);
        }
        
        return result;
    } catch (...) {
        return result;
    }
}

}  // namespace wadjet::distributed

#include "wadjet/io/frame_filter.hpp"

#include <pcap/pcap.h>

#include <cstring>

namespace wadjet::io {

struct FrameFilter::Impl {
    bpf_program program{};
    bool valid = false;
};

FrameFilter::FrameFilter() : impl_(std::make_unique<Impl>()) {}

FrameFilter::FrameFilter(FrameFilter&&) noexcept = default;
FrameFilter& FrameFilter::operator=(FrameFilter&&) noexcept = default;

FrameFilter::~FrameFilter() {
    if (impl_ && impl_->valid) {
        pcap_freecode(&impl_->program);
    }
}

auto FrameFilter::compile(std::string_view expression, int link_type) -> Result<FrameFilter> {
    FrameFilter filter;
    filter.expression_ = std::string(expression);

    // Use pcap_open_dead + pcap_compile as recommended (pcap_compile_nopcap is deprecated)
    pcap_t* pcap = pcap_open_dead(link_type, 65535);
    if (!pcap) {
        return Result<FrameFilter>::err(
            Error{-1, "Failed to create pcap handle for filter compilation"});
    }

    int result = pcap_compile(pcap, &filter.impl_->program, expression.data(),
                              1,                    // optimize
                              PCAP_NETMASK_UNKNOWN  // netmask
    );

    if (result != 0) {
        std::string error_msg = pcap_geterr(pcap);
        pcap_close(pcap);
        return Result<FrameFilter>::err(Error{-1, "Failed to compile BPF filter: " + error_msg});
    }

    pcap_close(pcap);
    filter.impl_->valid = true;
    return Result<FrameFilter>::ok(std::move(filter));
}

auto FrameFilter::accept_all() -> FrameFilter {
    auto result = compile("");
    if (result) {
        return std::move(*result);
    }
    // Fallback: create empty filter
    return FrameFilter();
}

bool FrameFilter::matches(const void* data, std::size_t len) const {
    if (!impl_ || !impl_->valid) {
        return true;  // No filter = accept all
    }

    // Create a fake pcap packet header for the filter
    pcap_pkthdr header{};
    header.caplen = static_cast<bpf_u_int32>(len);
    header.len = static_cast<bpf_u_int32>(len);

    return pcap_offline_filter(&impl_->program, &header, reinterpret_cast<const u_char*>(data)) !=
           0;
}

const void* FrameFilter::program() const {
    if (!impl_ || !impl_->valid) {
        return nullptr;
    }
    return &impl_->program;
}

std::size_t FrameFilter::program_length() const {
    if (!impl_ || !impl_->valid) {
        return 0;
    }
    return impl_->program.bf_len;
}

}  // namespace wadjet::io

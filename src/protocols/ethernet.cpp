/// @file ethernet.cpp
/// @brief Ethernet frame decoder implementation

#include "wadjet/protocols/ethernet.hpp"

#include <cstdio>
#include <sstream>

namespace wadjet::protocols::ethernet {

std::string EthernetHeader::to_string() const {
    std::ostringstream oss;
    oss << "Ethernet { dst=" << dst_mac.to_string() << ", src=" << src_mac.to_string()
        << ", ethertype=0x" << std::hex << ethertype;

    if (vlan) {
        oss << ", vlan=" << std::dec << vlan->vid() << " (pcp=" << static_cast<int>(vlan->pcp())
            << ")";
    }
    if (vlan_inner) {
        oss << ", inner_vlan=" << std::dec << vlan_inner->vid();
    }
    oss << " }";
    return oss.str();
}

std::optional<VlanTag> EthernetDecoder::parse_vlan(const DecodeContext& ctx, std::size_t offset) {
    if (!ctx.has_bytes(offset + VLAN_TAG_SIZE)) {
        return std::nullopt;
    }

    VlanTag tag;
    tag.tpid = ctx.read_be16(offset);

    if (tag.tpid != static_cast<std::uint16_t>(EtherType::VLAN) && tag.tpid != ETHERTYPE_QINQ) {
        return std::nullopt;
    }

    tag.tci = ctx.read_be16(offset + 2);
    return tag;
}

EthernetDecoder::Result EthernetDecoder::decode_impl(const DecodeContext& ctx) const {
    // Check minimum size
    if (!ctx.has_bytes(MIN_FRAME_SIZE)) {
        return make_error(DecodeErrorCode::BufferTooSmall, "Ethernet frame too small");
    }

    EthernetHeader header;
    std::size_t offset = 0;

    // Parse destination MAC (6 bytes)
    auto dst_bytes = ctx.read_bytes(offset, 6);
    std::memcpy(header.dst_mac.bytes.data(), dst_bytes.data(), 6);
    offset += 6;

    // Parse source MAC (6 bytes)
    auto src_bytes = ctx.read_bytes(offset, 6);
    std::memcpy(header.src_mac.bytes.data(), src_bytes.data(), 6);
    offset += 6;

    // Parse ethertype/length (2 bytes)
    std::uint16_t ethertype_or_len = ctx.read_be16(offset);
    offset += 2;

    // Check for VLAN tag (802.1Q)
    if (ethertype_or_len == static_cast<std::uint16_t>(EtherType::VLAN) ||
        ethertype_or_len == ETHERTYPE_QINQ) {
        // This is a VLAN-tagged frame
        if (!ctx.has_bytes(offset + 2)) {
            return make_error(DecodeErrorCode::BufferTooSmall, "VLAN tag truncated");
        }

        VlanTag outer_tag;
        outer_tag.tpid = ethertype_or_len;
        outer_tag.tci = ctx.read_be16(offset);
        offset += 2;
        header.vlan = outer_tag;

        // Read next ethertype
        if (!ctx.has_bytes(offset + 2)) {
            return make_error(DecodeErrorCode::BufferTooSmall, "Ethertype after VLAN truncated");
        }
        ethertype_or_len = ctx.read_be16(offset);
        offset += 2;

        // Check for QinQ (double VLAN)
        if (ethertype_or_len == static_cast<std::uint16_t>(EtherType::VLAN) ||
            ethertype_or_len == ETHERTYPE_QINQ) {
            if (!ctx.has_bytes(offset + 2)) {
                return make_error(DecodeErrorCode::BufferTooSmall, "Inner VLAN tag truncated");
            }

            VlanTag inner_tag;
            inner_tag.tpid = ethertype_or_len;
            inner_tag.tci = ctx.read_be16(offset);
            offset += 2;
            header.vlan_inner = inner_tag;

            // Read final ethertype
            if (!ctx.has_bytes(offset + 2)) {
                return make_error(DecodeErrorCode::BufferTooSmall,
                                  "Ethertype after inner VLAN truncated");
            }
            ethertype_or_len = ctx.read_be16(offset);
            offset += 2;
        }
    }

    header.ethertype = ethertype_or_len;
    header.header_len = offset;

    // Create context for next layer
    auto next_ctx = ctx.sub_context(offset);
    next_ctx.layer_info.ethertype = header.ethertype;

    return make_success(std::move(header), std::move(next_ctx));
}

}  // namespace wadjet::protocols::ethernet

#include "wadjet/net/packet_view.hpp"

namespace wadjet {

std::optional<PacketView::EthernetHeader> PacketView::ethernet_header() const {
    if (data_.size() < ETHERNET_HEADER_SIZE) {
        return std::nullopt;
    }

    EthernetHeader hdr{};

    // Destination MAC (bytes 0-5)
    std::memcpy(hdr.dst.bytes.data(), data_.data(), 6);

    // Source MAC (bytes 6-11)
    std::memcpy(hdr.src.bytes.data(), data_.data() + 6, 6);

    // EtherType (bytes 12-13)
    hdr.ether_type = static_cast<EtherType>(read_be16(data_.data() + 12));

    return hdr;
}

bool PacketView::has_vlan() const {
    if (data_.size() < ETHERNET_HEADER_SIZE) {
        return false;
    }
    auto ether_type = static_cast<EtherType>(read_be16(data_.data() + 12));
    return ether_type == EtherType::VLAN || ether_type == EtherType::QinQ;
}

std::optional<PacketView::VlanTag> PacketView::vlan_tag() const {
    if (!has_vlan()) {
        return std::nullopt;
    }

    if (data_.size() < ETHERNET_HEADER_SIZE + VLAN_TAG_SIZE) {
        return std::nullopt;
    }

    VlanTag tag{};
    const auto* vlan_data = data_.data() + ETHERNET_HEADER_SIZE;

    tag.tci = read_be16(vlan_data - 2);  // TCI is at offset 14
    // Actually VLAN starts after ethertype...
    tag.tci = read_be16(data_.data() + 14);
    tag.priority = static_cast<std::uint16_t>((tag.tci >> 13) & 0x07);
    tag.dei = ((tag.tci >> 12) & 0x01) != 0;
    tag.vlan_id = tag.tci & 0x0FFF;
    tag.ether_type = static_cast<EtherType>(read_be16(data_.data() + 16));

    return tag;
}

std::size_t PacketView::l3_offset() const {
    std::size_t offset = ETHERNET_HEADER_SIZE;
    if (has_vlan()) {
        offset += VLAN_TAG_SIZE;
        // Check for QinQ (double VLAN)
        if (data_.size() > offset + 2) {
            auto inner_type = static_cast<EtherType>(read_be16(data_.data() + offset - 2));
            if (inner_type == EtherType::VLAN) {
                offset += VLAN_TAG_SIZE;
            }
        }
    }
    return offset;
}

std::optional<PacketView::IPv4Header> PacketView::ipv4_header() const {
    auto offset = l3_offset();
    if (data_.size() < offset + 20) {  // Minimum IPv4 header size
        return std::nullopt;
    }

    // Check EtherType
    auto eth_hdr = ethernet_header();
    if (!eth_hdr) {
        return std::nullopt;
    }

    EtherType check_type = eth_hdr->ether_type;
    if (has_vlan()) {
        auto vlan = vlan_tag();
        if (vlan) {
            check_type = vlan->ether_type;
        }
    }

    if (check_type != EtherType::IPv4) {
        return std::nullopt;
    }

    const auto* ip_data = data_.data() + offset;
    IPv4Header hdr{};

    auto version_ihl = static_cast<std::uint8_t>(ip_data[0]);
    hdr.version = (version_ihl >> 4) & 0x0F;
    hdr.ihl = version_ihl & 0x0F;

    if (hdr.version != 4) {
        return std::nullopt;
    }

    auto dscp_ecn = static_cast<std::uint8_t>(ip_data[1]);
    hdr.dscp = (dscp_ecn >> 2) & 0x3F;
    hdr.ecn = dscp_ecn & 0x03;

    hdr.total_length = read_be16(ip_data + 2);
    hdr.identification = read_be16(ip_data + 4);

    auto flags_fragment = read_be16(ip_data + 6);
    hdr.dont_fragment = ((flags_fragment >> 14) & 0x01) != 0;
    hdr.more_fragments = ((flags_fragment >> 13) & 0x01) != 0;
    hdr.fragment_offset = flags_fragment & 0x1FFF;

    hdr.ttl = static_cast<std::uint8_t>(ip_data[8]);
    hdr.protocol = static_cast<IpProtocol>(static_cast<std::uint8_t>(ip_data[9]));
    hdr.checksum = read_be16(ip_data + 10);

    std::memcpy(hdr.src.bytes.data(), ip_data + 12, 4);
    std::memcpy(hdr.dst.bytes.data(), ip_data + 16, 4);

    return hdr;
}

std::size_t PacketView::l4_offset() const {
    auto ip_hdr = ipv4_header();
    if (!ip_hdr) {
        return l3_offset();  // No IP header, return L3 offset
    }
    return l3_offset() + static_cast<std::size_t>(ip_hdr->ihl) * 4;
}

std::optional<PacketView::UdpHeader> PacketView::udp_header() const {
    auto ip_hdr = ipv4_header();
    if (!ip_hdr || ip_hdr->protocol != IpProtocol::UDP) {
        return std::nullopt;
    }

    auto offset = l4_offset();
    if (data_.size() < offset + 8) {  // UDP header is 8 bytes
        return std::nullopt;
    }

    const auto* udp_data = data_.data() + offset;
    UdpHeader hdr{};

    hdr.src_port = read_be16(udp_data);
    hdr.dst_port = read_be16(udp_data + 2);
    hdr.length = read_be16(udp_data + 4);
    hdr.checksum = read_be16(udp_data + 6);

    return hdr;
}

std::optional<PacketView::TcpHeader> PacketView::tcp_header() const {
    auto ip_hdr = ipv4_header();
    if (!ip_hdr || ip_hdr->protocol != IpProtocol::TCP) {
        return std::nullopt;
    }

    auto offset = l4_offset();
    if (data_.size() < offset + 20) {  // Minimum TCP header is 20 bytes
        return std::nullopt;
    }

    const auto* tcp_data = data_.data() + offset;
    TcpHeader hdr{};

    hdr.src_port = read_be16(tcp_data);
    hdr.dst_port = read_be16(tcp_data + 2);
    hdr.seq_num = read_be32(tcp_data + 4);
    hdr.ack_num = read_be32(tcp_data + 8);

    auto data_offset_flags = read_be16(tcp_data + 12);
    hdr.data_offset = static_cast<std::uint8_t>((data_offset_flags >> 12) & 0x0F);
    hdr.flags = static_cast<std::uint8_t>(data_offset_flags & 0x3F);

    hdr.window = read_be16(tcp_data + 14);
    hdr.checksum = read_be16(tcp_data + 16);
    hdr.urgent_ptr = read_be16(tcp_data + 18);

    return hdr;
}

ByteSpan PacketView::payload() const {
    auto ip_hdr = ipv4_header();
    if (!ip_hdr) {
        return ByteSpan{};
    }

    std::size_t offset = l4_offset();

    if (ip_hdr->protocol == IpProtocol::UDP) {
        offset += 8;  // UDP header size
    } else if (ip_hdr->protocol == IpProtocol::TCP) {
        auto tcp_hdr = tcp_header();
        if (tcp_hdr) {
            offset += static_cast<std::size_t>(tcp_hdr->data_offset) * 4;
        }
    }

    if (offset >= data_.size()) {
        return ByteSpan{};
    }

    return data_.subspan(offset);
}

PacketView PacketView::subview(std::size_t offset) const {
    if (offset >= data_.size()) {
        return PacketView(ByteSpan{}, timestamp_);
    }
    return PacketView(data_.subspan(offset), timestamp_);
}

PacketView PacketView::subview(std::size_t offset, std::size_t length) const {
    if (offset >= data_.size()) {
        return PacketView(ByteSpan{}, timestamp_);
    }
    auto actual_length = std::min(length, data_.size() - offset);
    return PacketView(data_.subspan(offset, actual_length), timestamp_);
}

}  // namespace wadjet

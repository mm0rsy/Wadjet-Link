#pragma once

/// @file wadjet.hpp
/// @brief Main include file for Wadjet-Link library
///
/// 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.

// Version
#include "wadjet/version.hpp"

// Core utilities
#include "wadjet/core/byte_order.hpp"
#include "wadjet/core/result.hpp"
#include "wadjet/core/timestamp.hpp"
#include "wadjet/core/types.hpp"

// Network packets
#include "wadjet/net/packet.hpp"
#include "wadjet/net/packet_view.hpp"

// PCAP file I/O
#include "wadjet/pcap/pcap_file.hpp"
#include "wadjet/pcap/pcap_reader.hpp"
#include "wadjet/pcap/pcap_writer.hpp"

// Capture I/O
#include "wadjet/io/capture_session.hpp"
#include "wadjet/io/device.hpp"
#include "wadjet/io/frame_filter.hpp"

// Future protocols:
// #include "wadjet/protocols/someip.hpp"
// #include "wadjet/protocols/doip.hpp"
// #include "wadjet/protocols/gptp.hpp"
// #include "wadjet/protocols/uds.hpp"

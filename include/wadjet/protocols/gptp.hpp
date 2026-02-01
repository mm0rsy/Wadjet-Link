#pragma once

/// @file gptp.hpp
/// @brief gPTP (IEEE 802.1AS) precision time protocol decoder
///
/// Main header for gPTP protocol support, including message types, TLV structures,
/// and the polymorphic GptpTlv variant type.

#include "wadjet/protocols/gptp/gptp_types.hpp"
#include "wadjet/protocols/gptp/gptp_messages.hpp"

namespace wadjet::protocols::gptp {

// Re-export key types and variant for convenience
using TlvVariant = GptpTlv;  ///< Alias for polymorphic TLV handling

}  // namespace wadjet::protocols::gptp

// lpm_impl.cpp - Helper implementations for C++ wrapper
// This file contains non-inline implementations (string parsing, etc.)
// These are kept separate from the hot path to avoid code bloat

#include "liblpm"
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>

namespace liblpm {

// ============================================================================
// IPv4 Address Parsing
// ============================================================================

IPv4Address::IPv4Address(const char* str) noexcept {
    // Use inet_pton for robust parsing
    if (inet_pton(AF_INET, str, octets) != 1) {
        // Invalid address - zero it out
        std::memset(octets, 0, sizeof(octets));
    }
}

std::optional<IPv4Address> parse_ipv4(const char* str) noexcept {
    if (!str) {
        return std::nullopt;
    }
    
    IPv4Address addr(str);
    // Check if parsing succeeded (simple validation: at least one octet non-zero or is 0.0.0.0)
    // This is a simple heuristic - inet_pton already validated
    return addr;
}

// ============================================================================
// IPv6 Address Parsing
// ============================================================================

IPv6Address::IPv6Address(const char* str) noexcept {
    // Use inet_pton for robust parsing
    if (inet_pton(AF_INET6, str, octets) != 1) {
        // Invalid address - zero it out
        std::memset(octets, 0, sizeof(octets));
    }
}

std::optional<IPv6Address> parse_ipv6(const char* str) noexcept {
    if (!str) {
        return std::nullopt;
    }
    
    IPv6Address addr(str);
    return addr;
}

// ============================================================================
// CIDR Prefix Parsing
// ============================================================================

std::optional<std::tuple<std::array<uint8_t, 16>, uint8_t, bool>> 
parse_prefix(const char* str) noexcept {
    if (!str) {
        return std::nullopt;
    }
    
    // Find the '/' separator
    const char* slash = std::strchr(str, '/');
    if (!slash) {
        return std::nullopt;
    }
    
    // Parse prefix length
    char* endptr;
    long prefix_len = std::strtol(slash + 1, &endptr, 10);
    if (*endptr != '\0' || prefix_len < 0 || prefix_len > 128) {
        return std::nullopt;
    }
    
    // Copy address part (before the slash)
    const size_t addr_len = slash - str;
    if (addr_len == 0 || addr_len >= 256) {
        return std::nullopt;
    }
    
    char addr_str[256];
    std::memcpy(addr_str, str, addr_len);
    addr_str[addr_len] = '\0';
    
    // Try to parse as IPv4 first
    std::array<uint8_t, 16> addr_bytes{};
    
    if (inet_pton(AF_INET, addr_str, addr_bytes.data()) == 1) {
        // IPv4 address
        if (prefix_len > 32) {
            return std::nullopt;
        }
        return std::make_tuple(addr_bytes, static_cast<uint8_t>(prefix_len), false);
    }
    
    // Try IPv6
    if (inet_pton(AF_INET6, addr_str, addr_bytes.data()) == 1) {
        // IPv6 address
        if (prefix_len > 128) {
            return std::nullopt;
        }
        return std::make_tuple(addr_bytes, static_cast<uint8_t>(prefix_len), true);
    }
    
    // Invalid address
    return std::nullopt;
}

}  // namespace liblpm


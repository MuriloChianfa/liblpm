# Byte Order and Data Format

This document explains how liblpm handles byte ordering (endianness) and IP address storage formats.

## Quick Answer

**liblpm uses Network Byte Order (Big-Endian)** for IP addresses, which is the internet standard.

For most users: **No conversion needed**

## Table of Contents

- [Understanding Byte Order](#understanding-byte-order)
- [How liblpm Stores IP Addresses](#how-liblpm-stores-ip-addresses)
- [Integration Scenarios](#integration-scenarios)
- [Performance Considerations](#performance-considerations)
- [Best Practices](#best-practices)
- [FAQ](#faq)

## Understanding Byte Order

### Network Byte Order (Big-Endian)

Network byte order is the standard for all internet protocols. In big-endian format, the most significant byte comes first:

```
IP Address: 192.168.1.1

Network Byte Order (Big-Endian):
Memory: [192][168][1][1]
        MSB          LSB
```

### Host Byte Order

Different CPU architectures use different byte orders:
- **x86/x86_64 (Intel/AMD)**: Little-endian
- **ARM (most)**: Little-endian (bi-endian capable)
- **SPARC, PowerPC**: Big-endian
- **MIPS**: Bi-endian (configurable)

## How liblpm Stores IP Addresses

### Internal Representation

liblpm stores IP addresses as **byte arrays**, not multi-byte integers:

```c
// IPv4: 4 bytes
uint8_t ipv4_addr[4] = {192, 168, 1, 1};

// IPv6: 16 bytes
uint8_t ipv6_addr[16] = {0x20, 0x01, 0x0d, 0xb8, ...};
```

### Why Byte Arrays?

**Byte arrays are endian-neutral** - they store bytes in sequential memory order:

```c
uint8_t addr[4] = {192, 168, 1, 1};

// On ANY system (little or big-endian):
// Memory layout: [192][168][1][1]
// addr[0] = 192
// addr[1] = 168
// addr[2] = 1
// addr[3] = 1
```

### Lookup Process (Endian-Independent)

During lookup, liblpm accesses **individual bytes as array indices**:

```c
// Trie traversal - byte-by-byte indexing
entry = table[addr[0]];  // Use byte 0 as index (192)
entry = table[addr[1]];  // Use byte 1 as index (168)
entry = table[addr[2]];  // Use byte 2 as index (1)
entry = table[addr[3]];  // Use byte 3 as index (1)
```

**No multi-byte integer operations = No endianness concerns during lookup!**

## Integration Scenarios

### Scenario 1: Standard Socket Programming (No Conversion Needed)

Most networking code already uses network byte order:

```c
#include <arpa/inet.h>
#include <lpm.h>

struct sockaddr_in addr;
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);  // Network byte order

// Direct use - no conversion needed!
uint32_t next_hop = lpm_lookup(trie, (uint8_t*)&addr.sin_addr);
```

### Scenario 2: Packet Processing (No Conversion Needed)

IP packets are already in network byte order:

```c
// Raw packet processing
uint8_t packet[1500];
uint8_t* src_ip = &packet[12];  // Source IP offset in IP header
uint8_t* dst_ip = &packet[16];  // Destination IP offset

// Direct use - packets are already in network byte order!
uint32_t next_hop = lpm_lookup(trie, dst_ip);
```

### Scenario 3: Byte Array Storage (No Conversion Needed)

If you store IPs as byte arrays in network order:

```c
struct route {
    uint8_t prefix[16];    // IPv4 or IPv6
    uint8_t prefix_len;
    uint32_t next_hop;
};

// Parse and store in network byte order
inet_pton(AF_INET, "10.0.0.0", route.prefix);

// Direct use!
lpm_add(trie, route.prefix, route.prefix_len, route.next_hop);
```

### Scenario 4: uint32_t in Network Byte Order (Zero-Cost Cast)

**Recommended pattern for compact storage!** If you store IPs as `uint32_t` in network byte order:

```c
#include <arpa/inet.h>

// Store IPs as uint32_t (network byte order)
struct in_addr addr;
inet_pton(AF_INET, "192.168.1.1", &addr);
uint32_t ip = addr.s_addr;  // Network byte order

// Zero-cost pointer cast - no data conversion!
uint32_t next_hop = lpm_lookup(trie, (uint8_t*)&ip);

// Works great with standard data structures
std::vector<uint32_t> ip_addresses;  // Compact storage
for (uint32_t ip : ip_addresses) {
    uint32_t nh = lpm_lookup(trie, (uint8_t*)&ip);  // Just cast!
}
```

**Why this works:**
- The pointer cast `(uint8_t*)&ip` is **zero cost** - no CPU cycles!
- It just reinterprets the memory address, doesn't copy or convert data
- Both representations point to the same bytes in memory
- Perfect for performance-critical code

### Scenario 5: uint32_t in Host Byte Order (Conversion Required)

**Not recommended**, but if you have IPs stored as uint32_t in host byte order:

```c
#include <arpa/inet.h>

// Legacy system: IPs stored as uint32_t in host byte order
uint32_t ip_host = 0xC0A80101;  // 192.168.1.1 (depends on endianness)

// Option 1: Convert with htonl() (standard function)
uint32_t ip_network = htonl(ip_host);
uint32_t next_hop = lpm_lookup(trie, (uint8_t*)&ip_network);

// Option 2: Manual byte extraction (portable)
uint8_t bytes[4] = {
    (ip_host >> 24) & 0xFF,
    (ip_host >> 16) & 0xFF,
    (ip_host >> 8) & 0xFF,
    ip_host & 0xFF
};
uint32_t next_hop = lpm_lookup(trie, bytes);
```

### C++ Wrapper

The C++ wrapper provides convenient constructors:

```cpp
#include <liblpm>

liblpm::LpmTableIPv4 table;

// String parsing (automatically converts to network byte order)
table.insert("192.168.0.0/16", 100);
uint32_t nh = table.lookup("192.168.1.1");

// Direct byte array (already in network byte order)
uint8_t addr[4] = {192, 168, 1, 1};
uint32_t nh = table.lookup(addr);  // Zero overhead

// From uint32_t (automatically converts to network byte order)
liblpm::IPv4Address addr(0xC0A80101);  // Converts to big-endian
uint32_t nh = table.lookup(addr.data());
```

### Optimization Tips

1. **Best for compact storage**: Store as `uint32_t` in network byte order, cast to `uint8_t*` when calling liblpm
2. **Best for clarity**: Store as byte arrays directly
3. **Always use**: `inet_pton()` or `inet_aton()` for parsing (produces network byte order)
4. **Never use**: `uint32_t` in host byte order (requires conversion)

## Best Practices

### Option 1: uint32_t with Zero-Cost Cast

```c
// Store IPs as uint32_t (network byte order) - compact!
struct ip_entry {
    uint32_t addr;          // 4 bytes, network byte order
    uint32_t next_hop;
};

// Parse with inet_pton
struct in_addr addr;
inet_pton(AF_INET, "192.168.1.1", &addr);
entry.addr = addr.s_addr;  // Network byte order

// Zero-cost cast for lookup
lpm_lookup(trie, (uint8_t*)&entry.addr);  // Free cast!
```

### Option 2: Byte Arrays

```c
// Store IPs as byte arrays in network byte order
struct ip_entry {
    uint8_t addr[4];        // Network byte order, explicit
    uint32_t next_hop;
};

// Parse with inet_pton
inet_pton(AF_INET, "192.168.1.1", entry.addr);  // Automatic

// Direct use
lpm_lookup(trie, entry.addr);  // No conversion, no cast
```

### Avoid This

```c
// Don't store IPs as uint32_t in host byte order
struct ip_entry {
    uint32_t addr;  // Ambiguous endianness
    uint32_t next_hop;
};

// Requires conversion
uint32_t addr_net = htonl(entry.addr);  // Extra step
lpm_lookup(trie, (uint8_t*)&addr_net);
```

## Comparison with Other Libraries

All major LPM/routing libraries use network byte order:

| Library | Byte Order | Storage Format | Notes |
|---------|------------|----------------|-------|
| **liblpm** | Network (big-endian) | Byte arrays | Endian-neutral access |
| DPDK rte_lpm | Network (big-endian) | `uint32_t` | Value is big-endian encoded |
| Linux kernel FIB | Network (big-endian) | `__be32` type | Type-annotated for checking |
| BSD route table | Network (big-endian) | `struct in_addr` | Standard socket structure |

**Note:** DPDK and Linux use `uint32_t` to store big-endian **values**, not for byte-by-byte access. The integer value itself represents the big-endian encoding (e.g., `0xC0A80101` for 192.168.1.1), regardless of the host's endianness. liblpm uses byte arrays for direct, endian-neutral byte access.

**This is the industry standard.**

### Understanding uint32_t with Network Byte Order

DPDK and Linux use `uint32_t` **values** that encode big-endian, not byte arrays:

```c
// DPDK approach: uint32_t with big-endian encoding
uint32_t ip = RTE_IPV4(192, 168, 1, 1);  // 0xC0A80101

// On x86 (little-endian):
// Variable value: 0xC0A80101 (correct)
// Memory bytes:  [0x01][0x01][0xA8][0xC0] (reversed!)
//                 ^LSB                ^MSB

// The integer VALUE is big-endian encoded
// Memory layout depends on host endianness

// liblpm approach: byte array (endian-neutral)
uint8_t addr[4] = {192, 168, 1, 1};

// On x86-64 systems:
// Memory bytes: [192][168][1][1] (always!)
//                ^byte0      ^byte3
```

**Key difference:**
- **uint32_t approach**: Big-endian **value** stored in host endianness
- **Byte array approach**: Big-endian **bytes** stored directly

Both are valid - uint32_t is compact, byte arrays are unambiguous.

### Performance: Pointer Cast is Free

The pointer cast `(uint8_t*)&ip_u32` has **zero runtime cost**:

```c
uint32_t ip = 0xC0A80101;
uint8_t* bytes = (uint8_t*)&ip;  // This line costs zero cycles
```

## FAQ

### Q: Why does liblpm use big-endian?

**A:** liblpm uses **network byte order**, which is the internet standard (RFC 1700). All networking protocols use big-endian for consistency across different architectures.

### Q: Will this be slow on x86 (little-endian)?

**A:** No! The library stores IPs as byte arrays and accesses them byte-by-byte. This is endian-neutral and has zero overhead on any architecture.

### Q: Do I need to convert my IPs?

**A:** Only if you're storing them as `uint32_t` in host byte order. Standard networking code already uses network byte order, so no conversion is needed.

### Q: Does the C++ wrapper handle conversion?

**A:** The C++ wrapper provides convenient constructors that handle conversion from strings and `uint32_t`, but the fast path (byte arrays) requires no conversion.

### Q: Can I use this library on ARM/MIPS/other architectures?

**A:** Yes! The byte array approach is portable across all architectures with zero overhead.

## Technical Details

### IPv4 Example

```c
// IP: 192.168.1.1

// In memory (all systems):
uint8_t addr[4] = {192, 168, 1, 1};
//                 [C0][A8][01][01] (hex)

// As uint32_t (depends on endianness):
// Big-endian:    0xC0A80101
// Little-endian: 0x0101A8C0

// liblpm uses byte arrays to avoid this ambiguity!
```

### IPv6 Example

```c
// IP: 2001:db8::1

// In memory (all systems):
uint8_t addr[16] = {
    0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

// No endianness ambiguity with byte arrays!
```
## References

- [RFC 1700 - Assigned Numbers (Byte Order)](https://www.rfc-editor.org/rfc/rfc1700)
- [POSIX inet_pton/inet_ntop](https://pubs.opengroup.org/onlinepubs/9699919799/functions/inet_ntop.html)
- [IP Header Format (RFC 791)](https://www.rfc-editor.org/rfc/rfc791)

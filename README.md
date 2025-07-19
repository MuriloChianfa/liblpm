# liblpm - High-Performance Longest Prefix Match Library

A highly optimized C library for Longest Prefix Match (LPM) lookups supporting both IPv4 and IPv6 addresses. The library uses multi-bit trie with 8-bit stride for optimal performance and supports CPU vectorization features.

## Features

- **High Performance**: Multi-bit trie with 8-bit stride reduces trie depth and improves cache locality
- **Dual Stack Support**: Native support for both IPv4 (32-bit) and IPv6 (128-bit) addresses
- **SIMD Optimizations**: Automatic detection and use of CPU features (SSE2, AVX2, AVX512F)
- **Batch Processing**: Vectorized batch lookup functions for processing multiple addresses simultaneously
- **Branchless Design**: Optimized lookup paths with minimal branch mispredictions
- **Cache-Friendly**: Aligned data structures and prefetching for optimal cache utilization
- **C23 Standard**: Written in modern C with best practices

## Building

### Requirements
- CMake 3.16+
- C compiler with C23 support (GCC 11+, Clang 13+)
- x86-64 processor

### Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Options
- `BUILD_SHARED_LIBS`: Build shared libraries (default: ON)
- `BUILD_TESTS`: Build test programs (default: ON)
- `BUILD_BENCHMARKS`: Build benchmark programs (default: ON)
- `ENABLE_NATIVE_ARCH`: Enable native architecture optimizations (default: ON)

### Install
```bash
sudo make install
```

## Usage

### Basic Example
```c
#include <lpm.h>

int main() {
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    
    uint8_t prefix[] = {192, 168, 0, 0};
    lpm_add(trie, prefix, 16, 100);  // 192.168.0.0/16 -> next hop 100
    
    uint8_t addr[] = {192, 168, 1, 1};
    uint32_t next_hop = lpm_lookup(trie, addr);
    
    lpm_destroy(trie);
    return 0;
}
```

### Batch Lookup
```c
const uint8_t *addrs[256];
uint32_t next_hops[256];
lpm_lookup_batch(trie, addrs, next_hops, 256);
```

### IPv6 Support
```c
lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
uint8_t prefix[16] = {0x20, 0x01, 0x0d, 0xb8, /* ... */};
lpm_add(trie, prefix, 32, 100);
uint32_t next_hop = lpm_lookup_ipv6(trie, addr);
```

## API Reference

### Core Functions
- `lpm_create(max_depth)` - Create LPM trie (32 for IPv4, 128 for IPv6)
- `lpm_add(trie, prefix, prefix_len, next_hop)` - Add prefix to trie
- `lpm_delete(trie, prefix, prefix_len)` - Remove prefix from trie
- `lpm_destroy(trie)` - Free all resources

### Lookup Functions
- `lpm_lookup(trie, addr)` - Single address lookup
- `lpm_lookup_ipv4(trie, addr)` - IPv4-specific lookup
- `lpm_lookup_ipv6(trie, addr)` - IPv6-specific lookup

## Testing

```bash
cd build
ctest                    # Run test suite
./benchmarks/bench_lookup  # Run benchmarks
```

## Fuzzing

The library includes some fuzzing tests to ensure robustness and catch edge cases. The fuzzing tests cover memory safety, API robustness, edge cases, and performance under stress.

```bash
# Setup fuzzing environment
./tests/fuzz_setup.sh

# Build with AFL instrumentation
./build_afl.sh

# Run AFL fuzzing
./run_fuzz.sh
```

For detailed information about the fuzzing tests, coverage areas, and advanced fuzzing techniques, see [tests/FUZZING.md](tests/FUZZING.md).

## License

MIT License - see LICENSE file for details

## Contributing

Contributions are welcome! Please ensure code follows existing style, all tests pass, benchmarks show no regression, and new features include tests.

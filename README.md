# High-Performance Longest Prefix Match Library

[![CI](https://github.com/MuriloChianfa/liblpm/actions/workflows/ci.yml/badge.svg)](https://github.com/MuriloChianfa/liblpm/actions/workflows/ci.yml)
[![Code Coverage](https://codecov.io/gh/MuriloChianfa/liblpm/branch/main/graph/badge.svg)](https://codecov.io/gh/MuriloChianfa/liblpm)
[![Static Analysis](https://github.com/MuriloChianfa/liblpm/actions/workflows/ci.yml/badge.svg?job=static-analysis)](https://github.com/MuriloChianfa/liblpm/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-blue.svg)](https://github.com/MuriloChianfa/liblpm)
[![Benchmark](https://img.shields.io/badge/benchmark-8.91M%20lookups%2Fsec-brightgreen.svg)](https://github.com/MuriloChianfa/liblpm)
[![Documentation](https://img.shields.io/badge/docs-API%20Reference-blue.svg)](https://github.com/MuriloChianfa/liblpm#api-reference)
[![Version](https://img.shields.io/badge/version-1.1.0-blue.svg)](https://github.com/MuriloChianfa/liblpm/releases)
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)
[![GCC](https://img.shields.io/badge/GCC-11+-green.svg)](https://gcc.gnu.org/)

A optimized C library for Longest Prefix Match (LPM) lookups supporting both IPv4 and IPv6 addresses. The library uses multi-bit trie with 8-bit stride for optimal performance and supports CPU vectorization.

## Features

- **High Performance**: Multi-bit trie with 8-bit stride reduces trie depth and improves cache.
- **Dual Stack Support**: Native support for both IPv4 (32-bit) and IPv6 (128-bit) addresses.
- **SIMD Optimizations**: Automatic detection and use of CPU features. (SSE2, AVX2, AVX512F)
- **Batch Processing**: Vectorized batch lookup for processing multiple addresses simultaneously.
- **Branchless Design**: Optimized lookup paths with minimal branch mispredictions.
- **Cache-Friendly**: Aligned data structures and prefetching for optimal cache utilization.
- **C23 Standard**: Written in modern C with best practices.

## Building

### Requirements
- CMake 3.16+
- x86-64 processor
- GCC 11+ or Clang 13+

### Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### Options
- `BUILD_SHARED_LIBS`: Build shared libraries (default: ON)
- `BUILD_TESTS`: Build test programs (default: ON)
- `BUILD_BENCHMARKS`: Build benchmark programs (default: ON)
- `ENABLE_NATIVE_ARCH`: Enable native architecture optimizations (default: ON)

## Usage

### Basic Example
```c
#include <lpm.h>
int main() {
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    
    // 192.168.0.0/16 -> next hop 100
    uint8_t prefix[] = {192, 168, 0, 0};
    lpm_add(trie, prefix, 16, 100);

    uint8_t addr[] = {192, 168, 1, 1};
    uint32_t next_hop = lpm_lookup(trie, addr);
    
    lpm_destroy(trie);
    return 0;
}
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

## Performance benchmarks

| Operation | Protocol | Performance | Latency |
|-----------|----------|-------------|---------|
| Single Lookup | IPv4 | 8.91 M lookups/sec | 112 ns |
| Single Lookup | IPv6 | 11.19 M lookups/sec | 89 ns |
| lookup_all | IPv4 | 2.60 M lookups/sec | 384 ns |
| lookup_all | IPv6 | 2.31 M lookups/sec | 432 ns |

<small>

**Time Complexity**: O(k) where k is the prefix length in bits<br>
**Space Complexity**: O(n × k) where n is number of prefixes, k is average prefix length<br>
**Lookup Performance**: Constant time per trie level (8-bit stride)<br>
**Memory Usage IPv4**: - ~4.9 KB per prefix scaling linearly<br>
**Memory Usage IPv6**: - ~6.1 KB per prefix scaling linearly

</small>

## Tests and Fuzzing

The library includes some fuzzing tests to ensure robustness and catch edge cases. The fuzzing tests cover memory safety, API robustness, edge cases, and performance under stress.

```bash
cd build
ctest --verbose            # Run test suite
./benchmarks/bench_lookup  # Run benchmarks

./tests/fuzz_setup.sh      # Setup fuzzing environment
./build_afl.sh             # Build with AFL instrumentation
./run_fuzz.sh              # Run AFL fuzzing
```

For detailed information about the fuzzing tests, coverage areas,<br> and advanced fuzzing techniques, see [tests/FUZZING.md](tests/FUZZING.md).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE.md)
file for details.

## Security

If you discover any security related issues, instead of using<br> the issue tracker, please email to: murilo.chianfa@outlook.com.

## Credits

- [Murilo Chianfa](https://github.com/MuriloChianfa)
- [All Contributors](../../contributors)

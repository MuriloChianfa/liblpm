# High-Performance Longest Prefix Match Library

[![CI](https://github.com/MuriloChianfa/liblpm/actions/workflows/ci.yml/badge.svg)](https://github.com/MuriloChianfa/liblpm/actions/workflows/ci.yml)
[![Code Coverage](https://codecov.io/gh/MuriloChianfa/liblpm/branch/main/graph/badge.svg)](https://codecov.io/gh/MuriloChianfa/liblpm)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-blue.svg)](https://github.com/MuriloChianfa/liblpm)
[![Version](https://img.shields.io/badge/version-1.2.0-blue.svg)](https://github.com/MuriloChianfa/liblpm/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)
[![GCC](https://img.shields.io/badge/GCC-11+-green.svg)](https://gcc.gnu.org/)

A optimized C library for Longest Prefix Match (LPM) lookups supporting both IPv4 and IPv6 addresses. The library uses multi-bit trie with 8-bit stride for optimal performance and supports CPU vectorization.

## Features

- **High Performance**: Multi-bit trie with 8-bit stride reduces trie depth and improves cache.
- **Dual Stack Support**: Native support for both IPv4 (32-bit) and IPv6 (128-bit) addresses.
- **SIMD Optimizations**: Automatic detection and use of CPU features. (SSE2, AVX, AVX2, AVX512F)
- **Batch Processing**: Vectorized batch lookup for processing multiple addresses simultaneously.
- **Branchless Design**: Optimized lookup paths with minimal branch mispredictions.
- **Cache-Friendly**: Aligned data structures and prefetching for optimal cache utilization.
- **C23 Standard**: Written in modern C with best practices.

## Building

### Requirements

<details open>
  <summary style="font-size: 16px;"><strong>Ubuntu/Debian</strong></summary>

  ```bash
  apt install build-essential cmake gcc-11 g++-11 libc6-dev clang lldb-18 lld-18 python3 python3-pip afl++ libasan6 libubsan1 cppcheck valgrind gdb strace ltrace
  ```
</details>
<details open>
  <summary style="font-size: 16px;"><strong>CentOS/RHEL/Rocky Linux</strong></summary>

  ```bash
  yum install cmake3 gcc-c++ libc-devel clang lldb lld python3 python3-pip afl++ libasan libubsan cppcheck valgrind gdb strace ltrace
  ```
</details>
<details open>
  <summary style="font-size: 16px;"><strong>Fedora</strong></summary>

  ```bash
  dnf install cmake gcc-c++ glibc-devel clang lldb lld python3 python3-pip afl++ libasan libubsan cppcheck valgrind gdb strace ltrace
  ```
</details>

### Build & Install
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### Build flags
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
- `lpm_lookup_all(trie, addr)` - Lookup for multiple match

## Performance benchmarks

> Simple Github runner, AVX2 enabled, Clang *RELEASE* build

| Operation | Protocol | Performance | Latency |
|-----------|----------|-------------|---------|
| Batch Lookup | IPv4 | 71.99M lookups/s | ~13 ns |
| Batch Lookup | IPv6 | 55.89M lookups/s | ~17 ns |
| Single Lookup | IPv4 | 54.72M lookups/s | ~18 ns |
| Single Lookup | IPv6 | 45.51M lookups/s | ~21 ns |
| All Matches | IPv4 | 8.14M lookups/s | ~122 ns |
| All Matches | IPv6 | 6.66M lookups/s | ~150 ns |
| Batch All | IPv4 | 5.75M lookups/s | ~173 ns |
| Batch All | IPv6 | 5.77M lookups/s | ~173 ns |

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

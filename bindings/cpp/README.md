# C++ Bindings for liblpm

C++ wrapper for [liblpm](https://github.com/MuriloChianfa/liblpm), providing zero-cost abstraction over the C library with modern C++17 features.

## Features

- **Zero-Cost Abstraction**: Same performance as raw C library
- **Header-Only**: Fully inlined hot paths for maximum compiler optimization
- **Modern C++17**: RAII, move semantics, templates, `std::span`
- **Type Safety**: Compile-time IPv4/IPv6 distinction via templates
- **Clean API**: `#include <liblpm>` - standard library style
- **Flexible**: Fast byte-array API + convenient string API
- **Batch Optimized**: Zero-copy batch operations with `std::span`
- **Exception Safe**: Optional exception handling (can be disabled)

## Installation

### Prerequisites

Ensure liblpm is installed:

```bash
# Build and install liblpm
cd ../../
mkdir -p build && cd build
cmake -DBUILD_CPP_WRAPPER=ON ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### Use in CMake Projects

```cmake
find_package(liblpm REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE liblpm::lpm_cpp liblpm::lpm_cpp_impl)
target_compile_features(my_app PRIVATE cxx_std_17)
```

## Quick Start

### Simple Include

```cpp
#include <liblpm>
// or
#include <liblpm.hpp>
```

### Basic Example

```cpp
#include <liblpm.hpp>
#include <iostream>

int main() {
    // Create IPv4 routing table
    liblpm::LpmTableIPv4 table;
    
    // Insert routes (string API - convenient)
    table.insert("192.168.0.0/16", 100);
    table.insert("10.0.0.0/8", 200);
    
    // Lookup (string API)
    uint32_t nh = table.lookup("192.168.1.1");
    if (nh != LPM_INVALID_NEXT_HOP) {
        std::cout << "Next hop: " << nh << "\n";
    }
    
    return 0;
}
```

### Fast Path: Byte Array API

For maximum performance, use the byte array API (zero parsing overhead):

```cpp
#include <liblpm>

int main() {
    liblpm::LpmTableIPv4 table;
    
    // Insert with byte arrays
    uint8_t prefix[4] = {192, 168, 0, 0};
    table.insert(prefix, 16, 100);
    
    // Lookup with byte arrays - FULLY INLINED!
    uint8_t addr[4] = {192, 168, 1, 1};
    uint32_t nh = table.lookup(addr);
    
    return 0;
}
```

### Batch Operations

Use `std::span` for zero-copy batch lookups:

```cpp
#include <liblpm>
#include <span>

int main() {
    liblpm::LpmTableIPv4 table;
    table.insert("192.168.0.0/16", 100);
    
    // Prepare addresses
    uint8_t addr1[4] = {192, 168, 1, 1};
    uint8_t addr2[4] = {192, 168, 2, 2};
    
    const uint8_t* addrs[] = {addr1, addr2};
    uint32_t results[2];
    
    // Batch lookup (zero-copy, no vector allocation)
    table.lookup_batch(std::span(addrs, 2), std::span(results, 2));
    
    return 0;
}
```

## API Reference

### Table Creation

```cpp
// IPv4 table
liblpm::LpmTableIPv4 table_v4;

// IPv6 table
liblpm::LpmTableIPv6 table_v6;

// Generic template (not recommended, use type aliases above)
liblpm::LpmTable<false> table_v4;  // IPv4
liblpm::LpmTable<true> table_v6;   // IPv6
```

### Core Operations

#### Fast Path: Byte Array API (Zero Overhead)

```cpp
// Insert
uint8_t prefix[4] = {192, 168, 0, 0};
table.insert(prefix, 16, 100);

// Lookup
uint8_t addr[4] = {192, 168, 1, 1};
uint32_t nh = table.lookup(addr);

// Delete
table.remove(prefix, 16);

// Batch lookup
const uint8_t* addrs[] = {addr1, addr2, addr3};
uint32_t results[3];
table.lookup_batch(std::span(addrs, 3), std::span(results, 3));
```

#### Convenience: String API (Small Overhead)

```cpp
// Insert
table.insert("192.168.0.0/16", 100);

// Lookup
uint32_t nh = table.lookup("192.168.1.1");

// Delete
table.remove("192.168.0.0/16");
```

### Statistics

```cpp
size_t count = table.size();      // Number of nodes
bool is_empty = table.empty();    // Check if empty
size_t cap = table.capacity();    // Node capacity
```

### Move Semantics

```cpp
// Move construct
liblpm::LpmTableIPv4 table2(std::move(table1));

// Move assign
table3 = std::move(table2);

// Copy is deleted (unique ownership)
// table2 = table1;  // ERROR: won't compile
```

### Advanced: Direct C Handle Access

```cpp
// Get underlying C handle for advanced users
lpm_trie_t* c_handle = table.handle();
```

## Performance Optimization

### 1. Use Byte Array API for Hot Paths

```cpp
// Better: Fully inlined, zero overhead
uint8_t addr[4] = {192, 168, 1, 1};
uint32_t nh = table.lookup(addr);

// Worse: String parsing overhead
uint32_t nh = table.lookup("192.168.1.1");
```

### 2. Use Batch Operations

```cpp
// Batch lookups are more efficient than individual lookups
// Compiler can optimize across the batch
table.lookup_batch(std::span(addrs), std::span(results));
```

### 3. Compiler Optimization Flags

```bash
# Use -O3 and link-time optimization
g++ -O3 -flto -march=native main.cpp -llpm -llpm_cpp_impl
```

### 4. Profile-Guided Optimization

```bash
# Generate profile data
g++ -O3 -fprofile-generate main.cpp -llpm -llpm_cpp_impl
./a.out  # Run with representative workload

# Rebuild with profile
g++ -O3 -fprofile-use main.cpp -llpm -llpm_cpp_impl
```

## IPv6 Support

```cpp
#include <liblpm>

int main() {
    // Create IPv6 table
    liblpm::LpmTableIPv6 table;
    
    // Insert routes
    table.insert("2001:db8::/32", 1000);
    table.insert("2001:db8:1::/48", 2000);
    
    // Lookup
    uint32_t nh = table.lookup("2001:db8:1::1");
    
    return 0;
}
```

## Building from Source

### Requirements

- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.16+
- liblpm installed

### Build

```bash
# From liblpm root
mkdir build && cd build
cmake -DBUILD_CPP_WRAPPER=ON ..
make -j$(nproc)

# Run example
./bindings/cpp/examples/cpp_basic_example

# Run tests
./bindings/cpp/tests/cpp_test_lpm
# or
ctest -R cpp_wrapper_test
```

### Manual Compilation

```bash
# Compile with the wrapper
g++ -std=c++17 -O3 -I../../include -I../include \
    main.cpp ../src/lpm_impl.cpp \
    -llpm -o my_app

# Run
./my_app
```

## Exception Handling

By default, the wrapper uses exceptions for error handling:

```cpp
try {
    table.insert("invalid/prefix", 100);
} catch (const liblpm::InvalidPrefixException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

To disable exceptions (for embedded systems or performance-critical code):

```cpp
// Compile with -DLIBLPM_NO_EXCEPTIONS
// Errors are silently ignored (check return values instead)
```

## Thread Safety

- `LpmTable` is **not thread-safe** by default
- For concurrent access, use external synchronization (mutex, etc.)
- Read-only lookups can be done concurrently if no modifications occur

```cpp
#include <mutex>
#include <shared_mutex>

std::shared_mutex mutex;
liblpm::LpmTableIPv4 table;

// Multiple readers
{
    std::shared_lock lock(mutex);
    table.lookup(addr);  // Safe
}

// Single writer
{
    std::unique_lock lock(mutex);
    table.insert(prefix, 16, 100);  // Safe
}
```

## Examples

See the [examples](examples/) directory for more examples:
- [basic_example.cpp](examples/basic_example.cpp) - Complete API demonstration

## Testing

The wrapper includes comprehensive unit tests:

```bash
# Build and run tests
cd build
make cpp_test

# Or use ctest
ctest -R cpp_wrapper_test -V
```

## Contributing

Contributions welcome! Please ensure:
- Code follows C++17 best practices
- All tests pass
- Performance remains zero-overhead
- Documentation is updated

## See Also

- [liblpm main documentation](../../README.md)
- [Go bindings](../go/README.md)
- [C API reference](../../include/lpm.h)

# Go Bindings for liblpm

High-performance Go bindings for the [liblpm](https://github.com/MuriloChianfa/liblpm) C library, providing fast longest prefix match (LPM) routing table operations for both IPv4 and IPv6.

## Features

- **High Performance**: Direct C bindings with minimal overhead
- **IPv4 DIR-24-8**: Optimized IPv4 lookups using DPDK-style algorithm
- **IPv6 Wide Stride**: Efficient IPv6 lookups with 16-bit first-level stride
- **Batch Operations**: Reduced cgo overhead through batch lookup operations
- **Thread-Safe Option**: SafeTable wrapper for concurrent use
- **Idiomatic Go API**: Clean interface using `netip.Addr` and `netip.Prefix`
- **Memory Safety**: Automatic cleanup with finalizers

## Installation

### Prerequisites

First, ensure liblpm is installed:

```bash
# Build and install liblpm
cd ../
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### Install Go Module

```bash
go get github.com/MuriloChianfa/liblpm/go/liblpm
```

## Quick Start

```go
package main

import (
    "fmt"
    "net/netip"
    
    "github.com/MuriloChianfa/liblpm/go/liblpm"
)

func main() {
    // Create IPv4 routing table
    table, _ := liblpm.NewTableIPv4()
    defer table.Close()
    
    // Add routes
    prefix := netip.MustParsePrefix("192.168.0.0/16")
    table.Insert(prefix, 100)
    
    // Lookup
    addr := netip.MustParseAddr("192.168.1.1")
    nextHop, found := table.Lookup(addr)
    if found {
        fmt.Printf("Next hop: %d\n", nextHop)
    }
}
```

## API Reference

### Creating Tables

```go
// IPv4 table with DIR-24-8 optimization (recommended for IPv4)
table, err := liblpm.NewTableIPv4()

// IPv6 table with wide stride optimization
table, err := liblpm.NewTableIPv6()

// Thread-safe variants
safeTable, err := liblpm.NewSafeTableIPv4()
safeTable, err := liblpm.NewSafeTableIPv6()
```

### Basic Operations

```go
// Insert a route
prefix := netip.MustParsePrefix("10.0.0.0/8")
err := table.Insert(prefix, 100)

// Delete a route
err := table.Delete(prefix)

// Lookup an address
addr := netip.MustParseAddr("10.1.1.1")
nextHop, found := table.Lookup(addr)

// Batch lookup (more efficient for multiple addresses)
addrs := []netip.Addr{
    netip.MustParseAddr("10.1.1.1"),
    netip.MustParseAddr("10.2.2.2"),
}
results, err := table.LookupBatch(addrs)

// Close table (frees C resources)
table.Close()
```

### Error Handling

```go
err := table.Insert(prefix, nextHop)
switch err {
case liblpm.ErrInvalidPrefix:
    // Invalid prefix format
case liblpm.ErrInsertFailed:
    // Insert operation failed
case liblpm.ErrTableClosed:
    // Attempted operation on closed table
}
```

## Performance

### cgo Overhead Considerations

cgo calls have inherent overhead (~40-80ns per call). To maximize performance:

1. **Use Batch Operations**: For multiple lookups, use `LookupBatch()` to amortize cgo overhead
2. **Keep Tables Open**: Avoid frequent create/destroy cycles
3. **Prefer Bulk Inserts**: Insert multiple routes before performing lookups

### Benchmark Results

Run benchmarks matching the iprbench format:

```bash
cd benchmarks
go test -bench=. -benchmem
```

Expected performance (will vary by hardware):
- IPv4 Insert: ~50-200 ns/route (includes cgo overhead)
- IPv4 Lookup: ~30-80 ns (single), ~15-40 ns (batch per lookup)
- IPv6 Insert: ~100-300 ns/route
- IPv6 Lookup: ~40-100 ns (single), ~20-50 ns (batch per lookup)

*Note*: These include cgo call overhead. Pure C performance is much faster (see main liblpm benchmarks).

## Examples

### Example 1: Basic Routing Table

See [examples/basic_example.go](examples/basic_example.go) for a complete example.

```bash
cd examples
go run basic_example.go
```

### Example 2: Batch Lookups

```go
// Create and populate table
table, _ := liblpm.NewTableIPv4()
defer table.Close()

// Add 10,000 routes
for i := 0; i < 10000; i++ {
    prefix := generateRandomPrefix()
    table.Insert(prefix, uint32(i))
}

// Batch lookup 1,000 addresses (much faster than 1,000 individual lookups)
addrs := make([]netip.Addr, 1000)
for i := range addrs {
    addrs[i] = generateRandomAddr()
}

results, _ := table.LookupBatch(addrs)
for i, nextHop := range results {
    if nextHop.IsValid() {
        fmt.Printf("%s -> %d\n", addrs[i], nextHop)
    }
}
```

### Example 3: Thread-Safe Table

```go
// SafeTable can be safely accessed from multiple goroutines
table, _ := liblpm.NewSafeTableIPv4()
defer table.Close()

// Add routes
prefix := netip.MustParsePrefix("10.0.0.0/8")
table.Insert(prefix, 100)

// Concurrent lookups from multiple goroutines
var wg sync.WaitGroup
for i := 0; i < 10; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        addr := netip.MustParseAddr("10.1.1.1")
        nextHop, found := table.Lookup(addr)
        // Process result...
    }()
}
wg.Wait()
```

## Integration with iprbench

This package is designed to integrate with the [iprbench](https://github.com/gaissmai/iprbench) Go routing table benchmark suite, enabling performance comparisons between pure Go implementations and C-based liblpm.

See [IPRBENCH_INTEGRATION.md](IPRBENCH_INTEGRATION.md) for details on adding liblpm to iprbench.

## Building from Source

```bash
# Ensure liblpm is built and installed first
cd /path/to/liblpm
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install

# Build Go wrapper
cd ../go
go build ./...

# Run tests
go test -v ./liblpm/

# Run benchmarks
go test -bench=. -benchmem ./benchmarks/
```

## Memory Management

The package uses Go finalizers to automatically clean up C resources when tables are garbage collected. However, it's recommended to explicitly call `Close()` for deterministic resource cleanup:

```go
table, _ := liblpm.NewTableIPv4()
defer table.Close()  // Ensures cleanup
```

## Thread Safety

- `Table`: Not thread-safe. Use from a single goroutine or add external synchronization.
- `SafeTable`: Thread-safe wrapper using read-write mutex.

## Requirements

- Go 1.19 or later (for `netip` package)
- liblpm 1.2.0 or later
- GCC or Clang with cgo support
- Linux or macOS (tested on Linux)

## Limitations

- No support for Windows (liblpm is Linux/Unix-focused)
- cgo required (no pure Go implementation)
- cgo overhead affects single-operation performance

## Contributing

Contributions welcome! Please ensure:
- All tests pass: `go test ./...`
- Code is formatted: `go fmt ./...`
- Benchmarks maintain performance: `go test -bench=. ./benchmarks/`

## License

Same as liblpm: Boost Software License 1.0

## Credits

- [liblpm](https://github.com/MuriloChianfa/liblpm) by Murilo Chianfa
- Go bindings by Murilo Chianfa

## See Also

- [liblpm main documentation](../README.md)



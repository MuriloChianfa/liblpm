# C# Bindings for liblpm

C# wrapper for [liblpm](https://github.com/MuriloChianfa/liblpm), providing high-performance Longest Prefix Match (LPM) operations for IP routing in .NET applications.

## Features

- **High Performance**: P/Invoke bindings with minimal overhead
- **.NET Standard 2.1**: Compatible with .NET Core 3.0+, .NET 5/6/7/8+, Unity 2021.2+
- **Type Safety**: Separate IPv4 and IPv6 APIs with compile-time checks
- **Zero-Copy**: `Span<T>` support for batch operations without allocations
- **Safe Resource Management**: `IDisposable` pattern with `SafeHandle` for automatic cleanup
- **Cross-Platform**: Native library loading for Linux, Windows, and macOS
- **Algorithm Selection**: Choose between different algorithms (DIR-24-8, Wide16, 8-stride)
- **NuGet Package**: Easy installation with bundled native libraries

## Installation

### Via NuGet

```bash
dotnet add package liblpm
```

### Building from Source

Prerequisites:
- .NET SDK 8.0 or later
- CMake 3.16+
- C compiler (GCC, Clang)

```bash
# Clone the repository
git clone --recursive https://github.com/MuriloChianfa/liblpm.git
cd liblpm

# Build liblpm native library
mkdir -p build && cd build
cmake -DBUILD_CSHARP_WRAPPER=ON ..
make -j$(nproc)

# Build and test C# bindings
make csharp_wrapper
make csharp_test
```

## Quick Start

### Basic IPv4 Example

```csharp
using LibLpm;

// Create an IPv4 routing table
using var trie = LpmTrieIPv4.CreateDefault();

// Add routes
trie.Add("192.168.0.0/16", 100);
trie.Add("10.0.0.0/8", 200);
trie.Add("0.0.0.0/0", 1);  // Default route

// Perform lookups
uint? nextHop = trie.Lookup("192.168.1.1");
Console.WriteLine($"Next hop: {nextHop}");  // Output: 100

// Delete a route
trie.Delete("192.168.0.0/16");
```

### Basic IPv6 Example

```csharp
using LibLpm;

// Create an IPv6 routing table
using var trie = LpmTrieIPv6.CreateDefault();

// Add routes
trie.Add("2001:db8::/32", 100);
trie.Add("fe80::/10", 200);
trie.Add("::/0", 1);  // Default route

// Perform lookups
uint? nextHop = trie.Lookup("2001:db8::1");
Console.WriteLine($"Next hop: {nextHop}");  // Output: 100
```

### Fast Path: Byte Array API

For maximum performance, use byte arrays to avoid string parsing overhead:

```csharp
using LibLpm;

using var trie = LpmTrieIPv4.CreateDefault();

// Insert with byte array
byte[] prefix = { 192, 168, 0, 0 };
trie.Add(prefix, prefixLen: 16, nextHop: 100);

// Lookup with byte array
byte[] addr = { 192, 168, 1, 1 };
uint? nextHop = trie.Lookup(addr);

// Even faster: use uint for IPv4
uint addr32 = 0xC0A80101;  // 192.168.1.1
uint? result = trie.Lookup(addr32);
```

### Zero-Copy with Span<T>

Use `Span<T>` for stack-allocated, zero-copy operations:

```csharp
using LibLpm;
using System;

using var trie = LpmTrieIPv4.CreateDefault();

// Stack-allocated prefix
Span<byte> prefix = stackalloc byte[] { 192, 168, 0, 0 };
trie.Add(prefix, 16, 100);

// Stack-allocated lookup
ReadOnlySpan<byte> addr = stackalloc byte[] { 192, 168, 1, 1 };
uint? nextHop = trie.Lookup(addr);
```

### Batch Operations

Process multiple lookups efficiently:

```csharp
using LibLpm;

using var trie = LpmTrieIPv4.CreateDefault();
trie.Add("0.0.0.0/0", 1);

// Batch lookup with arrays
uint[] addresses = { 0xC0A80101, 0x0A010101, 0x08080808 };
uint[] results = new uint[3];
trie.LookupBatch(addresses, results);

// Batch lookup with Span<T> (zero allocation)
Span<uint> addrSpan = stackalloc uint[] { 0xC0A80101, 0x0A010101 };
Span<uint> resultSpan = stackalloc uint[2];
trie.LookupBatch(addrSpan, resultSpan);
```

## API Reference

### Table Creation

```csharp
// IPv4 with default algorithm (DIR-24-8)
using var trie = LpmTrieIPv4.CreateDefault();

// IPv4 with specific algorithms
using var trieDir24 = LpmTrieIPv4.CreateDir24();
using var trieStride8 = LpmTrieIPv4.CreateStride8();

// IPv6 with default algorithm (Wide16)
using var trieV6 = LpmTrieIPv6.CreateDefault();

// IPv6 with specific algorithms
using var trieWide16 = LpmTrieIPv6.CreateWide16();
using var trieV6Stride8 = LpmTrieIPv6.CreateStride8();
```

### Core Operations

#### Add Routes

```csharp
// String-based (convenient)
trie.Add("192.168.0.0/16", nextHop: 100);

// Byte array (fast)
byte[] prefix = { 192, 168, 0, 0 };
trie.Add(prefix, prefixLen: 16, nextHop: 100);

// Span<T> (zero-copy)
ReadOnlySpan<byte> spanPrefix = stackalloc byte[] { 192, 168, 0, 0 };
trie.Add(spanPrefix, 16, 100);

// TryAdd (no exception on failure)
bool success = trie.TryAdd(prefix, 16, 100);
```

#### Lookup

```csharp
// String-based
uint? nextHop = trie.Lookup("192.168.1.1");

// IPAddress
var addr = IPAddress.Parse("192.168.1.1");
uint? nextHop = trie.Lookup(addr);

// Byte array
byte[] addrBytes = { 192, 168, 1, 1 };
uint? nextHop = trie.Lookup(addrBytes);

// uint (IPv4 only, fastest)
uint addr32 = 0xC0A80101;
uint? nextHop = trie.Lookup(addr32);

// Raw lookup (no null check, returns InvalidNextHop on miss)
uint rawResult = trie.LookupRaw(addr32);
if (rawResult != LpmConstants.InvalidNextHop) {
    // Found
}
```

#### Delete Routes

```csharp
// String-based
bool deleted = trie.Delete("192.168.0.0/16");

// Byte array
byte[] prefix = { 192, 168, 0, 0 };
bool deleted = trie.Delete(prefix, 16);
```

### Batch Operations

```csharp
// IPv4 batch lookup
uint[] addresses = new uint[1000];
uint[] results = new uint[1000];
trie.LookupBatch(addresses, results);

// IPv4 batch with Span<T>
Span<uint> addrSpan = stackalloc uint[100];
Span<uint> resultSpan = stackalloc uint[100];
trie.LookupBatch(addrSpan, resultSpan);

// IPv6 batch lookup (byte arrays, 16 bytes per address)
byte[] ipv6Addresses = new byte[1600];  // 100 addresses
uint[] ipv6Results = new uint[100];
trieV6.LookupBatch(ipv6Addresses, ipv6Results);
```

### Constants

```csharp
// Invalid next hop value (returned when no match)
uint invalid = LpmConstants.InvalidNextHop;  // 0xFFFFFFFF

// Maximum prefix lengths
byte ipv4Max = LpmConstants.IPv4MaxDepth;  // 32
byte ipv6Max = LpmConstants.IPv6MaxDepth;  // 128
```

### Utility Methods

```csharp
// Get library version
string? version = LpmTrie.GetVersion();

// IPv4 address conversion
uint addr = LpmTrieIPv4.ParseIPv4Address("192.168.1.1");
byte[] bytes = LpmTrieIPv4.UInt32ToBytes(addr);
uint back = LpmTrieIPv4.BytesToUInt32(bytes);

// IPv6 address conversion
byte[] ipv6 = LpmTrieIPv6.ParseIPv6Address("2001:db8::1");
string str = LpmTrieIPv6.FormatIPv6Address(ipv6);
```

## Performance Tips

### 1. Use Byte Array or uint API

String parsing has overhead. For hot paths, use byte arrays or uint:

```csharp
// Slower (string parsing)
trie.Lookup("192.168.1.1");

// Faster (byte array)
trie.Lookup(new byte[] { 192, 168, 1, 1 });

// Fastest (uint, IPv4 only)
trie.Lookup(0xC0A80101);
```

### 2. Use Batch Operations

Batch lookups are more efficient than individual lookups:

```csharp
// Instead of:
foreach (var addr in addresses) {
    results.Add(trie.Lookup(addr));
}

// Use:
trie.LookupBatch(addresses, results);
```

### 3. Use Span<T> for Zero Allocations

```csharp
// Stack allocation, no GC pressure
Span<uint> addresses = stackalloc uint[100];
Span<uint> results = stackalloc uint[100];
trie.LookupBatch(addresses, results);
```

### 4. Choose the Right Algorithm

- **DIR-24-8** (IPv4 default): Best for IPv4, 1-2 memory accesses, ~64MB memory
- **Wide16** (IPv6 default): Optimized for IPv6 /48 allocations
- **Stride8**: Memory-efficient for sparse prefix sets

## Thread Safety

`LpmTrieIPv4` and `LpmTrieIPv6` are **not thread-safe**. For concurrent access:

```csharp
// Option 1: External synchronization
private readonly object _lock = new object();
private readonly LpmTrieIPv4 _trie = LpmTrieIPv4.CreateDefault();

public uint? Lookup(uint addr) {
    lock (_lock) {
        return _trie.Lookup(addr);
    }
}

// Option 2: ReaderWriterLockSlim (multiple readers, single writer)
private readonly ReaderWriterLockSlim _rwLock = new();
private readonly LpmTrieIPv4 _trie = LpmTrieIPv4.CreateDefault();

public uint? Lookup(uint addr) {
    _rwLock.EnterReadLock();
    try {
        return _trie.Lookup(addr);
    }
    finally {
        _rwLock.ExitReadLock();
    }
}

public void Add(string cidr, uint nextHop) {
    _rwLock.EnterWriteLock();
    try {
        _trie.Add(cidr, nextHop);
    }
    finally {
        _rwLock.ExitWriteLock();
    }
}
```

## Exception Handling

```csharp
try {
    using var trie = LpmTrieIPv4.CreateDefault();
    trie.Add("invalid", 100);
}
catch (LpmCreationException ex) {
    // Failed to create trie (memory allocation failure)
}
catch (LpmInvalidPrefixException ex) {
    // Invalid prefix format or length
}
catch (LpmOperationException ex) {
    // Add/delete operation failed
}
catch (ObjectDisposedException ex) {
    // Trie has been disposed
}
```

## Resource Management

Always dispose tries to release native memory:

```csharp
// Recommended: using statement
using var trie = LpmTrieIPv4.CreateDefault();
// ... use trie ...
// Automatically disposed at end of scope

// Alternative: manual disposal
var trie = LpmTrieIPv4.CreateDefault();
try {
    // ... use trie ...
}
finally {
    trie.Dispose();
}
```

## Native Library Loading

The library automatically searches for the native `liblpm` library in:

1. `runtimes/{rid}/native/` relative to the assembly
2. Same directory as the assembly
3. System library paths (LD_LIBRARY_PATH, etc.)
4. Standard installation paths (`/usr/lib`, `/usr/local/lib`)

To verify the library is found:

```csharp
string? path = NativeLibraryLoader.FindLibraryPath();
Console.WriteLine($"Native library: {path}");

string? version = LpmTrie.GetVersion();
Console.WriteLine($"Version: {version}");
```

## Building the NuGet Package

```bash
cd bindings/csharp
dotnet pack -c Release -o ./nupkg

# The package will be at: ./nupkg/liblpm.2.0.0.nupkg
```

## Examples

See the [LibLpm.Examples](LibLpm.Examples/) directory for complete examples:

- [BasicExample.cs](LibLpm.Examples/BasicExample.cs) - IPv4/IPv6 operations
- [BatchExample.cs](LibLpm.Examples/BatchExample.cs) - High-performance batch lookups

Run examples:

```bash
cd bindings/csharp
dotnet run --project LibLpm.Examples
```

## Testing

```bash
cd bindings/csharp
dotnet test

# With verbose output
dotnet test --verbosity normal

# With coverage
dotnet test --collect:"XPlat Code Coverage"
```

## Contributing

Contributions are welcome! Please ensure:

- All tests pass (`dotnet test`)
- Code follows C# conventions
- XML documentation is provided for public APIs
- Performance is considered

## See Also

- [liblpm main documentation](../../README.md)
- [C++ bindings](../cpp/README.md)
- [Go bindings](../go/README.md)
- [C API reference](../../include/lpm.h)

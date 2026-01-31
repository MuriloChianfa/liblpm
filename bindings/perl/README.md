# Perl Bindings for liblpm

High-performance Perl bindings for the [liblpm](https://github.com/MuriloChianfa/liblpm) C library, providing fast longest prefix match (LPM) routing table operations for both IPv4 and IPv6.

## Features

- **High Performance**: Direct XS bindings to optimized C library
- **Dual Stack**: Full support for both IPv4 and IPv6
- **Multiple Algorithms**: DIR-24-8 for IPv4, Wide 16-bit stride for IPv6
- **Batch Operations**: Efficient batch lookups to reduce XS overhead
- **SIMD Optimized**: Runtime CPU feature detection (SSE2, AVX, AVX2, AVX512)
- **Memory Safe**: Automatic cleanup via Perl's reference counting
- **Idiomatic API**: Clean object-oriented Perl interface

## Installation

### Prerequisites

First, ensure liblpm is installed:

```bash
# Build and install liblpm from source
cd /path/to/liblpm
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

Or install from packages (see main liblpm README).

### Build Perl Module

```bash
cd bindings/perl
perl Makefile.PL
make
make test
sudo make install
```

### Development Build

For development, set the library path:

```bash
cd bindings/perl
perl Makefile.PL
make
LD_LIBRARY_PATH=../../build make test
```

## Quick Start

```perl
use Net::LPM;

# Create IPv4 routing table
my $table = Net::LPM->new_ipv4();

# Add routes (CIDR notation)
$table->insert("192.168.0.0/16", 100);
$table->insert("10.0.0.0/8", 200);
$table->insert("0.0.0.0/0", 999);  # Default route

# Lookup
my $next_hop = $table->lookup("192.168.1.1");
print "Next hop: $next_hop\n";  # Prints: Next hop: 100

# Batch lookup (more efficient for multiple addresses)
my @addresses = qw(192.168.1.1 10.1.1.1 8.8.8.8);
my @next_hops = $table->lookup_batch(\@addresses);

# Table automatically destroyed when out of scope
```

## API Reference

### Creating Tables

```perl
# IPv4 table (uses DIR-24-8 algorithm)
my $ipv4_table = Net::LPM->new_ipv4();

# IPv6 table (uses Wide 16-bit stride algorithm)
my $ipv6_table = Net::LPM->new_ipv6();
```

### Basic Operations

```perl
# Insert a route
$table->insert("192.168.0.0/16", 100);
$table->insert("2001:db8::/32", 200);

# Delete a route
my $deleted = $table->delete("192.168.0.0/16");

# Lookup a single address
my $next_hop = $table->lookup("192.168.1.1");
if (defined $next_hop) {
    print "Found: $next_hop\n";
} else {
    print "No match\n";
}

# Batch lookup (more efficient for multiple addresses)
my @addresses = qw(192.168.1.1 192.168.1.2 10.0.0.1);
my @next_hops = $table->lookup_batch(\@addresses);
```

### Error Handling

```perl
use Try::Tiny;

try {
    $table->insert("invalid-prefix", 100);
} catch {
    warn "Insert failed: $_";
};
```

### Utility Methods

```perl
# Check table type
if ($table->is_ipv4) { ... }
if ($table->is_ipv6) { ... }

# Get liblpm C library version
my $version = Net::LPM->version();

# Print internal statistics (for debugging)
$table->print_stats();
```

### Constants

```perl
use Net::LPM qw(LPM_INVALID_NEXT_HOP);

# LPM_INVALID_NEXT_HOP is 0xFFFFFFFF
# In the Perl API, this is converted to undef
```

## Performance

### XS Overhead

While the C library provides nanosecond-scale lookups, the XS binding adds some overhead (~5-20ns per call). To maximize performance:

1. **Use Batch Operations**: For multiple lookups, use `lookup_batch()` to amortize overhead

   ```perl
   # Slower: N XS calls
   my @results = map { $table->lookup($_) } @addresses;
   
   # Faster: 1 XS call
   my @results = $table->lookup_batch(\@addresses);
   ```

2. **Reuse Table Objects**: Avoid frequent create/destroy cycles

3. **Keep Tables in Scope**: Don't let tables go out of scope during lookup-intensive operations

### Benchmark

Run benchmarks:

```bash
cd bindings/perl
make
LD_LIBRARY_PATH=../../build perl -Iblib/lib -Iblib/arch examples/basic_example.pl
```

## Examples

See the [examples](examples/) directory:

- [basic_example.pl](examples/basic_example.pl) - Complete API demonstration

Run the example:

```bash
cd bindings/perl
make
LD_LIBRARY_PATH=../../build perl -Iblib/lib -Iblib/arch examples/basic_example.pl
```

## Thread Safety

**Net::LPM objects are NOT thread-safe.**

Each table should only be accessed from a single thread. For multi-threaded applications:

- Use separate tables per thread, or
- Add external synchronization (e.g., using `threads::shared`)

Read-only lookups can be done concurrently if no modifications occur.

## Memory Management

The module uses Perl's reference counting for automatic cleanup:

```perl
my $table = Net::LPM->new_ipv4();
# ... use table ...
# Table automatically freed when $table goes out of scope

# Or explicitly:
undef $table;  # Immediately free resources
```

## Testing

```bash
# Run all tests
make test

# Verbose test output
make testv

# Memory leak detection with valgrind
make valgrind
```

## Requirements

- Perl 5.10 or later
- liblpm 2.0.0 or later
- C compiler with C99 support
- Linux or macOS

## Troubleshooting

### Library not found

If you get "liblpm not found" errors:

```bash
# Option 1: Set environment variables
LIBLPM_INC=/path/to/include LIBLPM_LIB=/path/to/lib perl Makefile.PL

# Option 2: Set LD_LIBRARY_PATH at runtime
LD_LIBRARY_PATH=/path/to/lib perl your_script.pl

# Option 3: Install liblpm system-wide
sudo make install
sudo ldconfig
```

### Test failures

Ensure liblpm is properly built:

```bash
cd ../../build
make -j$(nproc)
cd ../bindings/perl
LD_LIBRARY_PATH=../../build make test
```

## Contributing

Contributions welcome! Please ensure:

- All tests pass: `make test`
- Code follows Perl best practices
- Documentation is updated
- Memory safety verified: `make valgrind`

## License

Same as liblpm: Boost Software License 1.0

## Credits

- [liblpm](https://github.com/MuriloChianfa/liblpm) by Murilo Chianfa
- Perl bindings by Murilo Chianfa

## See Also

- [liblpm main documentation](../../README.md)
- [C++ bindings](../cpp/README.md)
- [Go bindings](../go/README.md)
- [perldoc Net::LPM](lib/Net/LPM.pm) - Full API documentation

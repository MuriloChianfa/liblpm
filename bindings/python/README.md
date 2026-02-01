# Python Bindings for liblpm

High-performance Python bindings for the [liblpm](https://github.com/MuriloChianfa/liblpm) C library, providing fast longest prefix match (LPM) routing table operations for both IPv4 and IPv6.

## Features

- **High Performance**: Direct C bindings via Cython with minimal overhead
- **IPv4 DIR-24-8**: Optimized IPv4 lookups using DPDK-style algorithm
- **IPv6 Wide Stride**: Efficient IPv6 lookups with 16-bit first-level stride
- **Batch Operations**: Reduced overhead through batch lookup operations
- **Pythonic API**: Clean interface using `ipaddress` module types
- **Type Hints**: Full mypy support for IDE integration
- **Context Manager**: Automatic resource cleanup with `with` statement
- **Multiple Algorithms**: Choose between performance and memory efficiency

## Installation

Choose the installation method that best fits your environment:

### Method 1: From PyPI (Recommended)

```bash
pip install liblpm
```

### Method 2: From Distribution Packages

Native distribution packages are available for common Linux distributions:

#### Ubuntu/Debian

```bash
# Download the appropriate package for your distribution:
# Ubuntu 22.04, Ubuntu 24.04, or Debian 12

# Install
sudo dpkg -i python3-liblpm_2.0.0-1_amd64.deb

# Or install with dependencies
sudo apt install ./python3-liblpm_2.0.0-1_amd64.deb
```

#### Rocky Linux/Fedora

**Note**: Rocky Linux 9 ships with Python 3.9, which does not meet the minimum Python 3.10 requirement. Use Fedora or install from PyPI instead.

```bash
# Download the appropriate package for your distribution:
# Fedora 41 (Rocky Linux 9 not supported due to Python 3.9)

# Install
sudo rpm -ivh python3-liblpm-2.0.0-1.fc41.x86_64.rpm

# Or using dnf
sudo dnf install python3-liblpm-2.0.0-1.fc41.x86_64.rpm
```

### Method 3: From Source

Ensure liblpm C library is installed first:

```bash
# Build and install liblpm C library
git clone --recursive https://github.com/MuriloChianfa/liblpm.git
cd liblpm
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig

# Install Python bindings
cd ../bindings/python
pip install .
```

### Development Installation

```bash
# Install with development dependencies
pip install -e ".[dev]"
```

### Verifying Installation

After installation, verify the package is working:

```bash
# Check if module can be imported
python3 -c "import liblpm; print('liblpm version:', liblpm.__version__)"

# Verify functionality
python3 -c "
from liblpm import LpmTableIPv4
from ipaddress import IPv4Network, IPv4Address

with LpmTableIPv4() as table:
    table.insert(IPv4Network('10.0.0.0/8'), 1)
    result = table.lookup(IPv4Address('10.1.2.3'))
    print('Lookup result:', result)
    assert result == 1, 'Lookup failed'
    print('✓ Verification successful')
"
```

You should see:

```
liblpm version: 2.0.0
Lookup result: 1
✓ Verification successful
```

## Quick Start

```python
from ipaddress import IPv4Address, IPv4Network
from liblpm import LpmTableIPv4

# Create IPv4 routing table using context manager
with LpmTableIPv4() as table:
    # Insert routes
    table.insert(IPv4Network('192.168.0.0/16'), 100)
    table.insert(IPv4Network('192.168.1.0/24'), 101)
    table.insert('10.0.0.0/8', 200)  # String format also works
    
    # Lookup - returns most specific match
    next_hop = table.lookup(IPv4Address('192.168.1.1'))
    print(f"Next hop: {next_hop}")  # Output: Next hop: 101
    
    # No match returns None
    result = table.lookup(IPv4Address('8.8.8.8'))
    print(f"Result: {result}")  # Output: Result: None
```

## API Reference

### Creating Tables

```python
from liblpm import LpmTableIPv4, LpmTableIPv6, Algorithm

# IPv4 table with DIR-24-8 optimization (recommended)
table = LpmTableIPv4()
table = LpmTableIPv4(Algorithm.DIR24)

# IPv4 table with 8-bit stride (memory efficient)
table = LpmTableIPv4(Algorithm.STRIDE8)

# IPv6 table with wide stride optimization (recommended)
table = LpmTableIPv6()
table = LpmTableIPv6(Algorithm.WIDE16)

# IPv6 table with 8-bit stride (memory efficient)
table = LpmTableIPv6(Algorithm.STRIDE8)
```

### Basic Operations

```python
from ipaddress import IPv4Address, IPv4Network

with LpmTableIPv4() as table:
    # Insert a route
    table.insert(IPv4Network('192.168.0.0/16'), 100)
    table.insert('10.0.0.0/8', 200)  # String also works
    
    # Delete a route
    table.delete(IPv4Network('192.168.0.0/16'))
    
    # Lookup an address
    next_hop = table.lookup(IPv4Address('10.1.1.1'))
    next_hop = table.lookup('10.1.1.1')  # String also works
    
    # Check if match found
    if next_hop is not None:
        print(f"Found: {next_hop}")
    
    # Batch lookup (more efficient for multiple addresses)
    addrs = [IPv4Address('10.1.1.1'), IPv4Address('10.2.2.2')]
    results = table.lookup_batch(addrs)
```

### IPv6 Example

```python
from ipaddress import IPv6Address, IPv6Network
from liblpm import LpmTableIPv6

with LpmTableIPv6() as table:
    # Insert routes
    table.insert(IPv6Network('2001:db8::/32'), 100)
    table.insert(IPv6Network('2001:db8:1::/48'), 101)
    
    # Lookup
    next_hop = table.lookup(IPv6Address('2001:db8:1::1'))
    print(f"Next hop: {next_hop}")  # Output: Next hop: 101
```

### Error Handling

```python
from liblpm import (
    LpmTableIPv4,
    LpmError,
    LpmInsertError,
    LpmDeleteError,
    LpmClosedError,
    LpmInvalidPrefixError,
)

try:
    with LpmTableIPv4() as table:
        table.insert('invalid', 100)
except LpmInvalidPrefixError as e:
    print(f"Invalid prefix: {e}")

# Operations on closed table
table = LpmTableIPv4()
table.close()
try:
    table.lookup('10.0.0.1')
except LpmClosedError:
    print("Table is closed")
```

### Table Statistics

```python
with LpmTableIPv4() as table:
    table.insert('192.168.0.0/16', 100)
    table.insert('10.0.0.0/8', 200)
    
    print(f"Algorithm: {table.algorithm}")
    print(f"Prefixes: {table.num_prefixes}")
    print(f"Nodes: {table.num_nodes}")
    print(f"Closed: {table.closed}")
```

## Performance

### Batch Lookups

For multiple lookups, use `lookup_batch()` to reduce Python/C boundary crossing overhead:

```python
# More efficient than individual lookups
addrs = [IPv4Address(f'{i}.0.0.1') for i in range(10000)]
results = table.lookup_batch(addrs)
```

### Algorithm Selection

| Algorithm | Best For | Memory | Lookup Speed |
|-----------|----------|--------|--------------|
| DIR24 (IPv4) | General IPv4 routing | ~64MB | Fastest |
| Stride8 (IPv4) | Memory-constrained | Low | Fast |
| Wide16 (IPv6) | /48 prefix allocations | Medium | Fastest |
| Stride8 (IPv6) | Sparse prefix sets | Low | Fast |

### Benchmark Results

Run benchmarks with:

```bash
pytest benchmarks/ --benchmark-only -v
```

Expected performance (varies by hardware):
- IPv4 single lookup: ~50-100ns
- IPv4 batch lookup: ~20-50ns per address
- IPv6 single lookup: ~80-150ns
- IPv6 batch lookup: ~40-80ns per address

## Thread Safety

`LpmTable` classes are **not thread-safe**. For concurrent access, use external synchronization:

```python
import threading
from liblpm import LpmTableIPv4

lock = threading.RLock()
table = LpmTableIPv4()

def lookup_thread(addr):
    with lock:
        return table.lookup(addr)
```

For read-heavy workloads, consider `threading.RLock` which allows multiple concurrent readers.

## Type Checking

The package includes type stubs for full mypy support:

```python
from ipaddress import IPv4Address, IPv4Network
from liblpm import LpmTableIPv4

def find_route(table: LpmTableIPv4, addr: IPv4Address) -> int | None:
    return table.lookup(addr)
```

Run mypy:

```bash
mypy your_code.py
```

## Examples

See the [examples/](examples/) directory:

- `basic_example.py` - Fundamental operations
- `batch_lookup.py` - Efficient batch lookups
- `context_manager.py` - Resource management patterns
- `algorithms.py` - Algorithm comparison

Run examples:

```bash
python examples/basic_example.py
```

## Building from Source

### Requirements

- Python 3.10+
- Cython 3.0+
- CMake 3.16+
- GCC or Clang
- liblpm installed

### Build

```bash
# From bindings/python directory
pip install build
python -m build

# Or using pip
pip install .
```

### Development

```bash
# Install in development mode
pip install -e ".[dev]"

# Run tests
pytest tests/ -v

# Run benchmarks
pytest benchmarks/ --benchmark-only

# Type checking
mypy src/liblpm/

# Format code
black src/ tests/ examples/
ruff check src/ tests/ examples/
```

## Platform Support

| Platform | Status |
|----------|--------|
| Linux x86_64 | Fully supported |
| Linux aarch64 | Supported |
| macOS x86_64 | Supported |
| macOS arm64 | Supported |
| Windows | Not supported (liblpm is Unix-focused) |

## Requirements

- Python 3.10 or later
- liblpm 2.0.0 or later
- Linux or macOS

## Contributing

Contributions welcome! Please ensure:

- All tests pass: `pytest tests/`
- Code is formatted: `black . && ruff check .`
- Type hints are correct: `mypy src/liblpm/`
- Documentation is updated

## License

Same as liblpm: [Boost Software License 1.0](https://www.boost.org/LICENSE_1_0.txt)

## Credits

- [liblpm](https://github.com/MuriloChianfa/liblpm) by Murilo Chianfa
- Python bindings by Murilo Chianfa

## See Also

- [liblpm main documentation](../../README.md)
- [Go bindings](../go/README.md)
- [C++ bindings](../cpp/README.md)
- [C API reference](../../include/lpm.h)

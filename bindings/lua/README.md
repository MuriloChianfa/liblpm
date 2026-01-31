# Lua Bindings for liblpm

High-performance Lua bindings for the [liblpm](https://github.com/MuriloChianfa/liblpm) C library, providing fast longest prefix match (LPM) routing table operations for both IPv4 and IPv6.

## Features

- **High Performance**: Direct C bindings with minimal overhead
- **IPv4 DIR-24-8**: Optimized IPv4 lookups with 1-2 memory accesses
- **IPv6 Wide Stride**: Efficient IPv6 lookups with 16-bit first-level stride
- **Batch Operations**: Process multiple addresses in a single call
- **Multiple Input Formats**: CIDR strings, dotted-decimal, colon-hex, byte tables
- **Dual API Style**: Object-oriented and functional interfaces
- **Automatic Cleanup**: Garbage collector integration with optional explicit close
- **Lua 5.3+**: Supports Lua 5.3, 5.4, and LuaJIT 2.1+

## Installation

### Prerequisites

First, ensure liblpm is installed:

```bash
# Build and install liblpm
cd /path/to/liblpm
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### From Source (CMake)

```bash
# Build with Lua bindings
cd /path/to/liblpm
mkdir -p build && cd build
cmake -DBUILD_LUA_WRAPPER=ON ..
make -j$(nproc)

# Run tests
make lua_test

# Install (optional)
sudo make install
```

### LuaRocks

```bash
# Install from local rockspec
cd bindings/lua
luarocks make liblpm-2.0.0-1.rockspec

# Or install directly (once published)
luarocks install liblpm
```

### Manual Compilation

```bash
# Compile the module directly
cd bindings/lua
gcc -shared -fPIC -O2 \
    -I/usr/include/lua5.4 \
    -I../../include \
    src/liblpm.c src/liblpm_utils.c \
    -llpm -llua5.4 \
    -o liblpm.so
```

## Important Note

Due to the use of GNU ifunc for SIMD runtime dispatch in liblpm, when loading
the Lua module via `require()`, you may need to preload `liblpm.so`:

```bash
# If liblpm is installed system-wide
lua your_script.lua

# If using a local build, preload the library
LD_PRELOAD=/path/to/liblpm.so lua your_script.lua
```

This ensures the ifunc resolvers are executed before the Lua module is loaded.
The CMake build system handles this automatically for tests and examples.

## Quick Start

```lua
local lpm = require("liblpm")

-- Create IPv4 routing table
local table = lpm.new_ipv4()

-- Insert routes
table:insert("10.0.0.0/8", 100)
table:insert("192.168.0.0/16", 200)
table:insert("0.0.0.0/0", 999)  -- Default route

-- Lookup addresses
local next_hop = table:lookup("192.168.1.1")
print("Next hop:", next_hop)  -- 200

-- Batch lookup
local results = table:lookup_batch({"10.1.1.1", "192.168.2.1", "8.8.8.8"})
-- results = {100, 200, 999}

-- Clean up
table:close()
```

## API Reference

### Module Functions

#### Table Creation

```lua
-- Create IPv4 table with algorithm selection
local table = lpm.new_ipv4([algorithm])
-- algorithm: "dir24" (default) or "stride8"

-- Create IPv6 table with algorithm selection
local table = lpm.new_ipv6([algorithm])
-- algorithm: "wide16" (default) or "stride8"
```

#### Utility Functions

```lua
-- Get library version
local version = lpm.version()  -- "liblpm 2.0.0"

-- Get Lua binding version
local lua_version = lpm.lua_version()  -- "2.0.0"
```

#### Constants

```lua
lpm.INVALID_NEXT_HOP  -- 0xFFFFFFFF (returned when no match)
lpm.IPV4_MAX_DEPTH    -- 32
lpm.IPV6_MAX_DEPTH    -- 128
lpm._VERSION          -- "2.0.0"
```

### Table Methods (Object-Oriented Style)

#### insert

```lua
-- CIDR notation
local ok, err = table:insert("192.168.0.0/16", next_hop)

-- Address + prefix length
local ok, err = table:insert("192.168.0.0", 16, next_hop)

-- Byte table + prefix length
local ok, err = table:insert({192, 168, 0, 0}, 16, next_hop)
```

Returns `true` on success, or `nil, error_message` on failure.

#### delete

```lua
-- CIDR notation
local ok, err = table:delete("192.168.0.0/16")

-- Address + prefix length
local ok, err = table:delete("192.168.0.0", 16)
```

Returns `true` on success, or `nil, error_message` on failure.

#### lookup

```lua
-- String address
local next_hop = table:lookup("192.168.1.1")

-- Byte table
local next_hop = table:lookup({192, 168, 1, 1})

-- Binary string (4 bytes for IPv4, 16 for IPv6)
local next_hop = table:lookup(binary_addr)
```

Returns `next_hop` integer on match, or `nil` if no match found.

#### lookup_batch

```lua
local results = table:lookup_batch(addresses)
```

- `addresses`: Table of IP addresses (any supported format)
- Returns: Table where `results[i]` is `next_hop` or `nil` for `addresses[i]`

#### close

```lua
table:close()
```

Explicitly release resources. Safe to call multiple times.

#### is_closed

```lua
local closed = table:is_closed()  -- boolean
```

#### is_ipv6

```lua
local ipv6 = table:is_ipv6()  -- boolean
```

### Functional API Style

All table methods are also available as module functions:

```lua
lpm.insert(table, prefix, next_hop)
lpm.delete(table, prefix)
lpm.lookup(table, address)
lpm.lookup_batch(table, addresses)
lpm.close(table)
lpm.is_closed(table)
```

## Address Formats

### IPv4

| Format | Example | Description |
|--------|---------|-------------|
| CIDR string | `"192.168.0.0/16"` | Most common format |
| Dotted-decimal | `"192.168.1.1"` | For lookups or with prefix_len |
| Byte table | `{192, 168, 1, 1}` | Zero parsing overhead |
| Binary string | `"\xC0\xA8\x01\x01"` | 4-byte string |

### IPv6

| Format | Example | Description |
|--------|---------|-------------|
| CIDR string | `"2001:db8::/32"` | Most common format |
| Colon-hex | `"2001:db8::1"` | For lookups or with prefix_len |
| Compressed | `"::1"`, `"fe80::"` | Standard IPv6 compression |
| Byte table | `{0x20, 0x01, ...}` | 16 bytes, zero parsing |
| Binary string | 16-byte string | Zero parsing overhead |

## Algorithm Selection

### IPv4 Algorithms

| Algorithm | Description | Use Case |
|-----------|-------------|----------|
| `dir24` | DIR-24-8 with 24-bit direct table | Default, fastest for most workloads |
| `stride8` | 8-bit stride trie (4 levels) | Memory-constrained environments |

### IPv6 Algorithms

| Algorithm | Description | Use Case |
|-----------|-------------|----------|
| `wide16` | 16-bit first stride, then 8-bit | Default, optimized for /48 allocations |
| `stride8` | 8-bit stride trie (16 levels) | Simpler, may use less memory |

## Error Handling

```lua
-- Insert returns boolean success and optional error
local ok, err = table:insert("invalid/prefix", 100)
if not ok then
    print("Error:", err)
end

-- Lookup returns nil for no match
local nh = table:lookup("8.8.8.8")
if nh then
    print("Found:", nh)
else
    print("No match")
end

-- Operations on closed tables raise errors
local ok, err = pcall(function()
    closed_table:lookup("1.1.1.1")
end)
-- ok = false, err contains error message
```

## Memory Management

The bindings use Lua's garbage collector with a `__gc` metamethod to automatically clean up resources. However, for deterministic cleanup, call `close()` explicitly:

```lua
local table = lpm.new_ipv4()
-- ... use table ...
table:close()  -- Immediate cleanup

-- Or let GC handle it (not recommended for long-lived scripts)
table = nil
collectgarbage()
```

### Best Practices

1. **Explicit close**: Always call `close()` when done
2. **defer pattern**: Use `pcall` or similar for cleanup on error
3. **Reuse tables**: Avoid frequent create/destroy cycles

## Performance Tips

### Use Batch Operations

```lua
-- Better: Single batch call
local results = table:lookup_batch(addresses)

-- Worse: Individual calls
for _, addr in ipairs(addresses) do
    local nh = table:lookup(addr)
end
```

### Use Byte Format for Hot Paths

```lua
-- Fastest: Byte table (no parsing)
local nh = table:lookup({192, 168, 1, 1})

-- Fast: Binary string (no parsing)
local nh = table:lookup("\xC0\xA8\x01\x01")

-- Slower: String (requires parsing)
local nh = table:lookup("192.168.1.1")
```

### Keep Tables Open

```lua
-- Better: Reuse table
local router = lpm.new_ipv4()
-- Insert routes once
for _ = 1, 1000000 do
    router:lookup(...)
end
router:close()

-- Worse: Create/destroy repeatedly
for _ = 1, 1000000 do
    local t = lpm.new_ipv4()
    t:insert(...)
    t:lookup(...)
    t:close()
end
```

## Thread Safety

The Lua bindings are **not thread-safe**. If using with coroutines or multiple Lua states:

- Each Lua state should have its own table instances
- Do not share table userdata between states
- Use external locking if concurrent access is required

## Examples

See the [examples](examples/) directory:

- [basic_example.lua](examples/basic_example.lua) - Getting started
- [ipv6_example.lua](examples/ipv6_example.lua) - IPv6 operations
- [batch_example.lua](examples/batch_example.lua) - Batch processing

Run examples:

```bash
# Via CMake
make lua_example

# Directly
lua examples/basic_example.lua
```

## Testing

```bash
# Run test suite
make lua_test

# Or directly
lua tests/test_lpm.lua

# With verbose output
LUA_CPATH="./?.so;;" lua tests/test_lpm.lua
```

## Requirements

- Lua 5.3 or later (5.4 recommended)
- LuaJIT 2.1+ (alternative)
- liblpm 2.0.0 or later
- GCC or Clang
- Linux or macOS

## Limitations

- Not thread-safe (use external locking for concurrent access)
- IPv4 and IPv6 require separate table instances
- Maximum batch size: 100,000 addresses

## Contributing

Contributions welcome! Please ensure:

- All tests pass: `make lua_test`
- Code follows existing style
- New features include tests and documentation

## License

Same as liblpm: [Boost Software License 1.0](../../LICENSE)

## Credits

- [liblpm](https://github.com/MuriloChianfa/liblpm) by Murilo Chianfa
- Lua bindings by Murilo Chianfa

## See Also

- [liblpm main documentation](../../README.md)
- [C++ bindings](../cpp/README.md)
- [Go bindings](../go/README.md)
- [C API reference](../../include/lpm.h)

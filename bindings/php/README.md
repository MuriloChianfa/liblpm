# PHP Extension for liblpm

High-performance PHP extension for the [liblpm](https://github.com/MuriloChianfa/liblpm) C library, providing fast longest prefix match (LPM) routing table operations for both IPv4 and IPv6.

## Features

- **High Performance**: Native C extension with minimal overhead
- **Dual API**: Object-oriented classes and procedural functions
- **IPv4 Optimizations**: DIR-24-8 algorithm for 1-2 memory accesses per lookup
- **IPv6 Support**: Wide 16-bit stride optimized for common /48 allocations
- **Batch Operations**: Efficient batch lookups for high-throughput scenarios
- **Algorithm Selection**: Choose between DIR-24-8/Wide16 and 8-bit stride
- **Memory Safe**: Proper PHP reference counting and resource cleanup
- **PECL Ready**: Standard PHP extension packaging for easy distribution

## Requirements

- PHP 8.1 or later
- liblpm 1.2.0 or later
- Linux or macOS (tested on Linux)
- GCC or Clang compiler
- PHP development headers (`php-dev` package)

## Installation

Choose the installation method that best fits your environment:

### Method 1: From PECL

PECL automatically compiles the extension for your exact PHP version and platform:

```bash
pecl install liblpm
```

Then add to your `php.ini`:

```ini
extension=liblpm.so
```

### Manual Installation with phpize

```bash
# Ensure liblpm is installed first
cd /path/to/liblpm
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig

# Build PHP extension
cd bindings/php
phpize
./configure --with-liblpm=/usr/local
make -j$(nproc)
sudo make install

# Enable extension
echo "extension=liblpm.so" | sudo tee /etc/php/8.3/cli/conf.d/99-liblpm.ini
```

### Using CMake (Development)

```bash
# From liblpm root directory
mkdir build && cd build
cmake -DBUILD_PHP_WRAPPER=ON ..
make php_wrapper
sudo make php_install
```

### Verifying Installation

After installation, verify the extension is loaded:

```bash
php -m | grep liblpm
php --ri liblpm
```

You should see:

```
liblpm
liblpm support => enabled
liblpm version => 1.0.0
```

## Quick Start

### Object-Oriented API

```php
<?php
// Create IPv4 routing table
$table = new LpmTableIPv4();

// Insert routes
$table->insert("192.168.0.0/16", 100);
$table->insert("10.0.0.0/8", 200);
$table->insert("0.0.0.0/0", 1);  // Default route

// Lookup addresses
$nextHop = $table->lookup("192.168.1.1");  // Returns 100
$nextHop = $table->lookup("10.1.2.3");     // Returns 200
$nextHop = $table->lookup("8.8.8.8");      // Returns 1 (default)

// Batch lookup for high throughput
$addresses = ["192.168.1.1", "10.1.2.3", "8.8.8.8"];
$results = $table->lookupBatch($addresses);  // [100, 200, 1]

// Delete routes
$table->delete("192.168.0.0/16");

// Clean up (optional - destructor handles this)
$table->close();
```

### Procedural API

```php
<?php
// Create table
$table = lpm_create_ipv4();

// Insert routes
lpm_insert($table, "192.168.0.0/16", 100);
lpm_insert($table, "10.0.0.0/8", 200);

// Lookup
$nextHop = lpm_lookup($table, "192.168.1.1");  // Returns 100

// Batch lookup
$results = lpm_lookup_batch($table, ["192.168.1.1", "10.1.2.3"]);

// Delete
lpm_delete($table, "192.168.0.0/16");

// Get library version
echo lpm_version();

// Clean up
lpm_close($table);
```

### IPv6 Support

```php
<?php
// Create IPv6 routing table
$table = new LpmTableIPv6();

// Insert routes
$table->insert("2001:db8::/32", 1000);
$table->insert("2001:db8:1::/48", 2000);
$table->insert("::/0", 1);  // Default route

// Lookup
$nextHop = $table->lookup("2001:db8:1::1");  // Returns 2000 (most specific)

$table->close();
```

### Algorithm Selection

```php
<?php
// IPv4 with DIR-24-8 (default, fastest for most cases)
$table = new LpmTableIPv4(LpmTableIPv4::ALGO_DIR24);

// IPv4 with 8-bit stride (lower memory for sparse tables)
$table = new LpmTableIPv4(LpmTableIPv4::ALGO_8STRIDE);

// IPv6 with Wide 16-bit stride (default, optimized for /48 allocations)
$table6 = new LpmTableIPv6(LpmTableIPv6::ALGO_WIDE16);

// IPv6 with 8-bit stride
$table6 = new LpmTableIPv6(LpmTableIPv6::ALGO_8STRIDE);

// Procedural API with algorithm selection
$table = lpm_create_ipv4(LPM_ALGO_IPV4_DIR24);
$table6 = lpm_create_ipv6(LPM_ALGO_IPV6_WIDE16);
```

## API Reference

### Classes

#### LpmTableIPv4

```php
class LpmTableIPv4 {
    const ALGO_DIR24 = 0;    // DIR-24-8 algorithm (default)
    const ALGO_8STRIDE = 1;  // 8-bit stride algorithm
    
    public function __construct(?int $algorithm = null);
    public function insert(string $prefix, int $nextHop): bool;
    public function delete(string $prefix): bool;
    public function lookup(string $addr): int|false;
    public function lookupBatch(array $addresses): array;
    public function size(): int;
    public function close(): void;
}
```

#### LpmTableIPv6

```php
class LpmTableIPv6 {
    const ALGO_WIDE16 = 0;   // Wide 16-bit stride (default)
    const ALGO_8STRIDE = 1;  // 8-bit stride algorithm
    
    public function __construct(?int $algorithm = null);
    public function insert(string $prefix, int $nextHop): bool;
    public function delete(string $prefix): bool;
    public function lookup(string $addr): int|false;
    public function lookupBatch(array $addresses): array;
    public function size(): int;
    public function close(): void;
}
```

### Exceptions

```php
class LpmException extends Exception {}
class LpmInvalidPrefixException extends LpmException {}
class LpmOperationException extends LpmException {}
```

### Procedural Functions

```php
// Table creation
function lpm_create_ipv4(?int $algorithm = null): resource|false;
function lpm_create_ipv6(?int $algorithm = null): resource|false;

// Operations
function lpm_insert(resource $table, string $prefix, int $nextHop): bool;
function lpm_delete(resource $table, string $prefix): bool;
function lpm_lookup(resource $table, string $addr): int|false;
function lpm_lookup_batch(resource $table, array $addresses): array;
function lpm_size(resource $table): int;
function lpm_close(resource $table): void;

// Utility
function lpm_version(): string;
```

### Constants

```php
const LPM_ALGO_IPV4_DIR24 = 0;
const LPM_ALGO_IPV4_8STRIDE = 1;
const LPM_ALGO_IPV6_WIDE16 = 0;
const LPM_ALGO_IPV6_8STRIDE = 1;
```

## Performance Tips

### 1. Use Batch Operations

For multiple lookups, batch operations are significantly faster:

```php
// Slower: Individual lookups
foreach ($addresses as $addr) {
    $results[] = $table->lookup($addr);
}

// Faster: Batch lookup
$results = $table->lookupBatch($addresses);
```

### 2. Choose the Right Algorithm

- **IPv4 DIR-24-8**: Best for typical routing tables, 1-2 memory accesses
- **IPv4 8-stride**: Better memory efficiency for sparse tables
- **IPv6 Wide16**: Optimized for common /48 ISP allocations
- **IPv6 8-stride**: More memory efficient for sparse IPv6 tables

### 3. Reuse Table Objects

Creating tables has overhead. Reuse tables when possible:

```php
// Less efficient: Create new table each time
function checkRoute($addr) {
    $table = new LpmTableIPv4();
    // ... insert routes ...
    return $table->lookup($addr);
}

// More efficient: Reuse table
$table = new LpmTableIPv4();
// ... insert routes once ...
function checkRoute($addr) use ($table) {
    return $table->lookup($addr);
}
```

## Thread Safety

- Table objects are **not thread-safe** by default
- For multi-threaded PHP (rare), use external synchronization
- Read-only lookups may be safe for concurrent reads if no modifications occur

## Error Handling

The extension throws exceptions for errors:

```php
try {
    $table = new LpmTableIPv4();
    $table->insert("invalid-prefix", 100);  // Throws LpmInvalidPrefixException
} catch (LpmInvalidPrefixException $e) {
    echo "Invalid prefix: " . $e->getMessage();
} catch (LpmOperationException $e) {
    echo "Operation failed: " . $e->getMessage();
}
```

## Testing

```bash
# Run tests
cd bindings/php
phpize
./configure --with-liblpm=/usr/local
make test

# Or with CMake
cd build
make php_test
```

## Examples

See the [examples/](examples/) directory:

- `basic_example.php` - Basic OOP usage
- `batch_lookup.php` - Batch operations
- `procedural_example.php` - Procedural API
- `ipv6_example.php` - IPv6 routing

## License

Boost Software License 1.0 (same as liblpm)

## Credits

- [liblpm](https://github.com/MuriloChianfa/liblpm) by Murilo Chianfa
- PHP bindings by Murilo Chianfa

## See Also

- [liblpm main documentation](../../README.md)
- [C++ bindings](../cpp/README.md)
- [Go bindings](../go/README.md)
- [C API reference](../../include/lpm.h)

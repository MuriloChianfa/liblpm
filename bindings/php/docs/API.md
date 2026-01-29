# liblpm PHP Extension API Reference

Complete API documentation for the liblpm PHP extension.

## Table of Contents

1. [Classes](#classes)
   - [LpmTableIPv4](#lpmtableipv4)
   - [LpmTableIPv6](#lpmtableipv6)
2. [Exceptions](#exceptions)
3. [Procedural Functions](#procedural-functions)
4. [Constants](#constants)
5. [Return Values](#return-values)
6. [Error Handling](#error-handling)

---

## Classes

### LpmTableIPv4

High-performance IPv4 longest prefix match routing table.

#### Constants

```php
const ALGO_DIR24 = 0;    // DIR-24-8 algorithm (default, fastest)
const ALGO_8STRIDE = 1;  // 8-bit stride algorithm (lower memory)
```

#### Constructor

```php
public function __construct(?int $algorithm = null)
```

Creates a new IPv4 routing table.

**Parameters:**
- `$algorithm` (optional): Algorithm to use. One of `ALGO_DIR24` (default) or `ALGO_8STRIDE`.

**Throws:**
- `LpmOperationException` if table creation fails or invalid algorithm specified.

**Example:**
```php
// Default (DIR-24-8)
$table = new LpmTableIPv4();

// With specific algorithm
$table = new LpmTableIPv4(LpmTableIPv4::ALGO_DIR24);
$table = new LpmTableIPv4(LpmTableIPv4::ALGO_8STRIDE);
```

---

#### insert

```php
public function insert(string $prefix, int $nextHop): bool
```

Inserts a route into the routing table.

**Parameters:**
- `$prefix`: IPv4 prefix in CIDR notation (e.g., "192.168.0.0/16")
- `$nextHop`: Next hop value (0 to 4,294,967,295)

**Returns:** `true` on success, `false` on failure.

**Throws:**
- `LpmInvalidPrefixException` if prefix format is invalid
- `LpmOperationException` if table is closed

**Example:**
```php
$table->insert("192.168.0.0/16", 100);
$table->insert("10.0.0.0/8", 200);
$table->insert("0.0.0.0/0", 1);  // Default route
```

---

#### delete

```php
public function delete(string $prefix): bool
```

Deletes a route from the routing table.

**Parameters:**
- `$prefix`: IPv4 prefix in CIDR notation (e.g., "192.168.0.0/16")

**Returns:** `true` on success, `false` if route not found.

**Throws:**
- `LpmInvalidPrefixException` if prefix format is invalid
- `LpmOperationException` if table is closed

**Example:**
```php
$table->delete("192.168.0.0/16");
```

---

#### lookup

```php
public function lookup(string $addr): int|false
```

Performs a longest prefix match lookup for an IPv4 address.

**Parameters:**
- `$addr`: IPv4 address (e.g., "192.168.1.1")

**Returns:**
- Next hop value (`int`) if a matching prefix is found
- `false` if no matching prefix exists

**Throws:**
- `LpmInvalidPrefixException` if address format is invalid
- `LpmOperationException` if table is closed

**Example:**
```php
$table->insert("192.168.0.0/16", 100);
$table->insert("192.168.1.0/24", 200);

$nh = $table->lookup("192.168.1.1");  // Returns 200 (most specific)
$nh = $table->lookup("192.168.2.1");  // Returns 100
$nh = $table->lookup("10.0.0.1");     // Returns false
```

---

#### lookupBatch

```php
public function lookupBatch(array $addresses): array
```

Performs batch lookup for multiple IPv4 addresses.

**Parameters:**
- `$addresses`: Array of IPv4 address strings

**Returns:** Array of results, where each element is either:
- Next hop value (`int`) if found
- `false` if not found

**Throws:**
- `LpmInvalidPrefixException` if any address format is invalid
- `LpmOperationException` if table is closed

**Example:**
```php
$results = $table->lookupBatch([
    "192.168.1.1",
    "10.0.0.1",
    "8.8.8.8"
]);
// Returns: [200, 100, false]
```

---

#### size

```php
public function size(): int
```

Returns the number of prefixes in the table.

**Returns:** Number of prefixes (`int`).

**Example:**
```php
$table->insert("192.168.0.0/16", 100);
$table->insert("10.0.0.0/8", 200);
echo $table->size();  // Outputs: 2 (or more depending on implementation)
```

---

#### close

```php
public function close(): void
```

Explicitly closes the table and releases resources.

**Note:** The destructor automatically calls this, but explicit closing is recommended for deterministic cleanup.

**Example:**
```php
$table = new LpmTableIPv4();
// ... use table ...
$table->close();  // Explicit cleanup
```

---

### LpmTableIPv6

High-performance IPv6 longest prefix match routing table.

#### Constants

```php
const ALGO_WIDE16 = 0;   // Wide 16-bit stride (default, optimized for /48)
const ALGO_8STRIDE = 1;  // 8-bit stride algorithm (lower memory)
```

#### Methods

All methods are identical to `LpmTableIPv4` but operate on IPv6 addresses:

- `__construct(?int $algorithm = null)`
- `insert(string $prefix, int $nextHop): bool` - Prefix: e.g., "2001:db8::/32"
- `delete(string $prefix): bool`
- `lookup(string $addr): int|false` - Address: e.g., "2001:db8::1"
- `lookupBatch(array $addresses): array`
- `size(): int`
- `close(): void`

**Example:**
```php
$table = new LpmTableIPv6();

$table->insert("2001:db8::/32", 1000);
$table->insert("2001:db8:1::/48", 2000);
$table->insert("::/0", 1);  // Default route

$nh = $table->lookup("2001:db8:1::1");  // Returns 2000
$nh = $table->lookup("2001:db8::1");    // Returns 1000
$nh = $table->lookup("fe80::1");        // Returns 1 (default)

$table->close();
```

---

## Exceptions

### LpmException

Base exception class for all liblpm errors.

```php
class LpmException extends Exception {}
```

### LpmInvalidPrefixException

Thrown when an invalid IP prefix or address is provided.

```php
class LpmInvalidPrefixException extends LpmException {}
```

**Common causes:**
- Invalid IP address format
- Missing prefix length (e.g., "192.168.0.0" instead of "192.168.0.0/24")
- Invalid prefix length (e.g., "/33" for IPv4, "/129" for IPv6)
- Non-numeric prefix length

### LpmOperationException

Thrown when an operation fails.

```php
class LpmOperationException extends LpmException {}
```

**Common causes:**
- Table creation failed (out of memory)
- Operating on a closed table
- Invalid algorithm specified
- Cloning attempt (not supported)

---

## Procedural Functions

### lpm_create_ipv4

```php
function lpm_create_ipv4(?int $algorithm = null): resource|false
```

Creates an IPv4 routing table.

**Parameters:**
- `$algorithm` (optional): `LPM_ALGO_IPV4_DIR24` or `LPM_ALGO_IPV4_8STRIDE`

**Returns:** Resource handle or `false` on failure.

---

### lpm_create_ipv6

```php
function lpm_create_ipv6(?int $algorithm = null): resource|false
```

Creates an IPv6 routing table.

**Parameters:**
- `$algorithm` (optional): `LPM_ALGO_IPV6_WIDE16` or `LPM_ALGO_IPV6_8STRIDE`

**Returns:** Resource handle or `false` on failure.

---

### lpm_insert

```php
function lpm_insert(resource $table, string $prefix, int $nextHop): bool
```

Inserts a route into a table.

**Parameters:**
- `$table`: Table resource from `lpm_create_ipv4()` or `lpm_create_ipv6()`
- `$prefix`: IP prefix in CIDR notation
- `$nextHop`: Next hop value (0-4294967295)

**Returns:** `true` on success, `false` on failure.

---

### lpm_delete

```php
function lpm_delete(resource $table, string $prefix): bool
```

Deletes a route from a table.

**Parameters:**
- `$table`: Table resource
- `$prefix`: IP prefix in CIDR notation

**Returns:** `true` on success, `false` on failure.

---

### lpm_lookup

```php
function lpm_lookup(resource $table, string $addr): int|false
```

Performs a longest prefix match lookup.

**Parameters:**
- `$table`: Table resource
- `$addr`: IP address

**Returns:** Next hop value or `false` if not found.

---

### lpm_lookup_batch

```php
function lpm_lookup_batch(resource $table, array $addresses): array
```

Performs batch lookup.

**Parameters:**
- `$table`: Table resource
- `$addresses`: Array of IP addresses

**Returns:** Array of results (int or false for each address).

---

### lpm_size

```php
function lpm_size(resource $table): int
```

Returns number of prefixes in the table.

**Parameters:**
- `$table`: Table resource

**Returns:** Number of prefixes.

---

### lpm_close

```php
function lpm_close(resource $table): void
```

Closes a table and releases resources.

**Parameters:**
- `$table`: Table resource

---

### lpm_version

```php
function lpm_version(): string
```

Returns the liblpm library version.

**Returns:** Version string (e.g., "2.0.0").

---

## Constants

### IPv4 Algorithm Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LPM_ALGO_IPV4_DIR24` | 0 | DIR-24-8 algorithm (fastest, 64MB memory) |
| `LPM_ALGO_IPV4_8STRIDE` | 1 | 8-bit stride (lower memory, 4 levels) |
| `LpmTableIPv4::ALGO_DIR24` | 0 | Same as above (class constant) |
| `LpmTableIPv4::ALGO_8STRIDE` | 1 | Same as above (class constant) |

### IPv6 Algorithm Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LPM_ALGO_IPV6_WIDE16` | 0 | Wide 16-bit stride (optimized for /48) |
| `LPM_ALGO_IPV6_8STRIDE` | 1 | 8-bit stride (16 levels, lower memory) |
| `LpmTableIPv6::ALGO_WIDE16` | 0 | Same as above (class constant) |
| `LpmTableIPv6::ALGO_8STRIDE` | 1 | Same as above (class constant) |

---

## Return Values

### Lookup Results

| Return Value | Meaning |
|--------------|---------|
| `int` (0-4294967295) | Next hop value for matching prefix |
| `false` | No matching prefix found |

### Operation Results

| Return Value | Meaning |
|--------------|---------|
| `true` | Operation succeeded |
| `false` | Operation failed |

### Batch Lookup Results

Returns an array where each element is either:
- `int`: Next hop value
- `false`: No match found

The array indices correspond to the input addresses array.

---

## Error Handling

### Best Practices

```php
// Use try-catch for exception handling
try {
    $table = new LpmTableIPv4();
    $table->insert("192.168.0.0/16", 100);
    $nh = $table->lookup("192.168.1.1");
} catch (LpmInvalidPrefixException $e) {
    // Handle invalid prefix/address
    error_log("Invalid IP: " . $e->getMessage());
} catch (LpmOperationException $e) {
    // Handle operation failure
    error_log("Operation failed: " . $e->getMessage());
} finally {
    if (isset($table)) {
        $table->close();
    }
}
```

### Procedural Error Handling

```php
$table = lpm_create_ipv4();
if ($table === false) {
    die("Failed to create table");
}

if (!lpm_insert($table, "192.168.0.0/16", 100)) {
    error_log("Insert failed");
}

$nh = lpm_lookup($table, "192.168.1.1");
if ($nh === false) {
    echo "No route found";
} else {
    echo "Next hop: $nh";
}

lpm_close($table);
```

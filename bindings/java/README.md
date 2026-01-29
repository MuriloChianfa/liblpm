# Java Bindings for liblpm

Java wrapper for [liblpm](https://github.com/MuriloChianfa/liblpm), providing high-performance Longest Prefix Match (LPM) routing table lookups using JNI.

## Features

- **High Performance**: Direct JNI calls to the C library, batch operations with zero-copy
- **Modern Java**: Requires Java 17+, uses try-with-resources, modern exception handling
- **Dual API**: Both byte[] (fast) and InetAddress/String (convenient) interfaces
- **Algorithm Selection**: DIR-24-8 and 8-bit stride for IPv4, Wide-16 and 8-bit stride for IPv6
- **Type Safety**: Separate classes for IPv4 and IPv6 with compile-time type checking
- **Automatic Memory Management**: AutoCloseable with finalizer safety net
- **Bundled Natives**: Native libraries bundled in JAR for easy deployment

## Installation

### Prerequisites

Ensure liblpm is installed:

```bash
# Build and install liblpm
cd liblpm
mkdir -p build && cd build
cmake -DBUILD_JAVA_WRAPPER=ON ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### Gradle

```gradle
dependencies {
    implementation 'com.github.murilochianfa:liblpm:1.0.0'
}
```

### Maven

```xml
<dependency>
    <groupId>com.github.murilochianfa</groupId>
    <artifactId>liblpm</artifactId>
    <version>1.0.0</version>
</dependency>
```

## Quick Start

### IPv4 Routing Table

```java
import com.github.murilochianfa.liblpm.*;

public class Example {
    public static void main(String[] args) {
        try (LpmTableIPv4 table = LpmTableIPv4.create()) {
            // Insert routes using CIDR notation
            table.insert("192.168.0.0/16", 100);
            table.insert("10.0.0.0/8", 200);
            table.insert("0.0.0.0/0", 1);  // Default route
            
            // Lookup - longest prefix match
            int nextHop = table.lookup("192.168.1.1");
            System.out.println("Next hop: " + nextHop);  // Prints: 100
            
            // Check for no-route
            if (NextHop.isInvalid(nextHop)) {
                System.out.println("No route found");
            }
        }  // Automatically closed
    }
}
```

### IPv6 Routing Table

```java
try (LpmTableIPv6 table = LpmTableIPv6.create()) {
    table.insert("2001:db8::/32", 100);
    table.insert("::/0", 1);
    
    int nextHop = table.lookup("2001:db8::1");
    System.out.println("Next hop: " + nextHop);  // Prints: 100
}
```

## API Reference

### LpmTableIPv4

```java
// Creation
LpmTableIPv4 table = LpmTableIPv4.create();  // Default: DIR24
LpmTableIPv4 table = LpmTableIPv4.create(Algorithm.STRIDE8);

// Insert
table.insert("192.168.0.0/16", nextHop);           // CIDR string
table.insert(inetAddress, 16, nextHop);            // InetAddress
table.insert(new byte[]{192, 168, 0, 0}, 16, nh);  // byte array (fastest)

// Lookup
int result = table.lookup("192.168.1.1");          // String
int result = table.lookup(inetAddress);            // InetAddress
int result = table.lookup(byteArray);              // byte[] (fastest)
int result = table.lookup(0xC0A80101);             // int (fastest for IPv4)

// Batch lookup (high performance)
int[] results = table.lookupBatch(addresses);      // byte[][]
int[] results = table.lookupBatch(intAddresses);   // int[]
table.lookupBatchFast(intAddresses, results);      // Pre-allocated (fastest)

// Delete
boolean found = table.delete("192.168.0.0/16");
boolean found = table.delete(byteArray, 16);

// Cleanup
table.close();  // Or use try-with-resources
```

### LpmTableIPv6

```java
// Creation
LpmTableIPv6 table = LpmTableIPv6.create();  // Default: WIDE16
LpmTableIPv6 table = LpmTableIPv6.create(Algorithm.STRIDE8);

// Insert
table.insert("2001:db8::/32", nextHop);
table.insert(inet6Address, 32, nextHop);
table.insert(new byte[16], 32, nextHop);

// Lookup
int result = table.lookup("2001:db8::1");
int result = table.lookup(inet6Address);
int result = table.lookup(byteArray);  // byte[16]

// Batch lookup
int[] results = table.lookupBatch(addresses);  // byte[][]
table.lookupBatchInto(addresses, results);     // Pre-allocated

// Delete
boolean found = table.delete("2001:db8::/32");
```

### Algorithm Selection

| Algorithm | Protocol | Description |
|-----------|----------|-------------|
| `DIR24`   | IPv4     | DIR-24-8: 1-2 memory accesses, ~64MB memory |
| `STRIDE8` | IPv4/IPv6 | 8-bit stride trie, memory-efficient |
| `WIDE16`  | IPv6     | 16-bit first stride, optimized for /48 |

```java
// IPv4: DIR24 (default, fastest) or STRIDE8 (memory-efficient)
LpmTableIPv4 table = LpmTableIPv4.create(Algorithm.DIR24);

// IPv6: WIDE16 (default, optimized) or STRIDE8 (simple)
LpmTableIPv6 table = LpmTableIPv6.create(Algorithm.WIDE16);
```

### NextHop Utilities

```java
int result = table.lookup(address);

// Check validity
if (NextHop.isValid(result)) {
    System.out.println("Found: " + result);
}

if (NextHop.isInvalid(result)) {
    System.out.println("No route");
}

// Get unsigned value
long unsigned = NextHop.toUnsigned(result);

// Constants
NextHop.INVALID    // -1 (0xFFFFFFFF)
NextHop.MAX_DIR24  // 0x3FFFFFFF (30-bit limit for DIR24)
```

## Performance Tips

### 1. Use byte[] API for Hot Paths

```java
// Fast: Direct byte array
byte[] addr = {192, 168, 1, 1};
int nh = table.lookup(addr);

// Faster for IPv4: int representation
int addrInt = (192 << 24) | (168 << 16) | (1 << 8) | 1;
int nh = table.lookup(addrInt);

// Slower: String parsing
int nh = table.lookup("192.168.1.1");
```

### 2. Use Batch Operations

```java
// Process many addresses at once
int[] addresses = new int[10000];
int[] results = new int[10000];
// ... populate addresses ...

// Single JNI call for all lookups
table.lookupBatchFast(addresses, results);
```

### 3. Pre-allocate Arrays

```java
// Allocate once, reuse
int[] addresses = new int[BATCH_SIZE];
int[] results = new int[BATCH_SIZE];

while (hasMorePackets()) {
    fillAddresses(addresses);
    table.lookupBatchFast(addresses, results);
    processResults(results);
}
```

## Thread Safety

- **Read operations** (`lookup`, `lookupBatch`): Thread-safe, can run concurrently
- **Write operations** (`insert`, `delete`): NOT thread-safe, require synchronization
- **Mixed read/write**: Require synchronization

For thread-safe read/write access, use external synchronization:

```java
ReadWriteLock lock = new ReentrantReadWriteLock();

// Writing
lock.writeLock().lock();
try {
    table.insert("192.168.0.0/16", 100);
} finally {
    lock.writeLock().unlock();
}

// Reading
lock.readLock().lock();
try {
    int nh = table.lookup("192.168.1.1");
} finally {
    lock.readLock().unlock();
}
```

## Building from Source

```bash
# Clone the repository
git clone https://github.com/MuriloChianfa/liblpm.git
cd liblpm

# Build the main library
mkdir -p build && cd build
cmake -DBUILD_JAVA_WRAPPER=ON ..
make -j$(nproc)

# Build Java bindings
cd ../bindings/java
./gradlew build
```

### Using Docker

The easiest way to build and test is using Docker:

```bash
# Build and run tests in Docker
docker build -f docker/Dockerfile.java -t liblpm-java .
docker run --rm liblpm-java

# Interactive development
docker run -it --rm liblpm-java bash

# Extract JAR artifact
docker run --rm -v "$PWD/artifacts:/artifacts" liblpm-java \
    cp /app/build/libs/*.jar /artifacts/
```

### Running Tests

```bash
cd bindings/java
./gradlew test
```

### Running Examples

```bash
cd bindings/java
./gradlew runBasicExample
./gradlew runBatchExample
```

## Requirements

- Java 17 or later
- liblpm C library (built and installed)
- Supported platforms:
  - Linux x86_64
  - Linux aarch64
  - macOS x86_64 (experimental)
  - macOS aarch64 (experimental)

## License

MIT License - see [LICENSE](../../LICENSE) for details.

## See Also

- [liblpm Documentation](https://github.com/MuriloChianfa/liblpm)
- [C++ Bindings](../cpp/README.md)
- [Go Bindings](../go/README.md)
- [API Reference](docs/API.md)

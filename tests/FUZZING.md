# LPM Fuzzing Tests

This directory contains comprehensive fuzzing tests for the LPM (Longest Prefix Match) implementation. The fuzzing tests are designed to find memory safety issues, edge cases, and bugs in the trie data structure.

## Overview

The fuzzing tests cover:

1. **Memory Safety**: Testing memory allocation/deallocation patterns
2. **Edge Cases**: Bitmap operations, prefix boundaries, overlapping prefixes
3. **API Robustness**: Invalid parameters, error conditions
4. **Performance**: Batch operations, large-scale testing
5. **IPv4/IPv6**: Both address family support
6. **Custom Allocators**: Memory management with custom allocators

## Test Files

### `test_fuzz.c`
Basic fuzzing test that runs deterministic tests with random data:
- Memory exhaustion scenarios
- Bitmap edge cases
- Overlapping prefixes
- Lookup_all functionality
- Batch operations
- Custom allocators
- IPv6 functionality
- Result management
- Error conditions

### `test_fuzz_advanced.c`
Advanced fuzzing test that supports AFL-style file-based fuzzing:
- File input parsing
- Memory safety testing
- Edge case detection
- AFL-compatible interface

## Running the Tests

### Basic Fuzzing Tests
```bash
# Build the project
mkdir build && cd build
cmake ..
make

# Run basic fuzzing tests
./tests/test_fuzz

# Run advanced fuzzing tests
./tests/test_fuzz_advanced
```

### Using CTest
```bash
# Run all tests including fuzzing
ctest --verbose

# Run only fuzzing tests
ctest -L fuzz --verbose
```

## AFL-Style Fuzzing Setup

### Prerequisites
```bash
# Install AFL (American Fuzzy Lop)
sudo apt install afl++

# Or build from source
git clone https://github.com/google/AFL.git
cd AFL && make && sudo make install
```

### Setup Fuzzing Environment
```bash
# Run the setup script
./tests/fuzz_setup.sh

# This creates:
# - fuzz_input/ directory with initial test cases
# - build_afl.sh script for AFL build
# - run_fuzz.sh script for running AFL
# - continuous_fuzz.sh script for continuous fuzzing
# - tests/dict.txt dictionary for AFL
```

### Running AFL Fuzzing
```bash
# Build with AFL instrumentation
./build_afl.sh

# Run AFL fuzzing
./run_fuzz.sh

# Or run continuous fuzzing with crash detection
./continuous_fuzz.sh

# Multicore fuzzing
./tests/fuzz_multicore.sh

# Advanced multi-core with different strategies
./tests/fuzz_advanced_multicore.sh
```

## What the Fuzzing Tests Cover

### Memory Safety
- **Memory Exhaustion**: Creating many tries and adding prefixes
- **Custom Allocators**: Testing with different memory management
- **Result Management**: Dynamic allocation of result structures
- **Batch Operations**: Large-scale memory operations

### Edge Cases
- **Bitmap Operations**: Testing bit manipulation at boundaries
- **Prefix Boundaries**: Prefixes at stride boundaries (8-bit, 16-bit, etc.)
- **Overlapping Prefixes**: Multiple prefixes matching the same address
- **Zero Capacity**: Handling edge cases in result creation

### API Robustness
- **NULL Parameters**: Testing with invalid pointers
- **Invalid Prefix Lengths**: Testing bounds checking
- **Invalid Address Families**: Mixing IPv4/IPv6
- **Error Conditions**: Testing error handling paths

### Performance Testing
- **Large Scale**: Adding hundreds of prefixes
- **Batch Operations**: Testing vectorized lookups
- **Memory Patterns**: Testing cache-aligned structures

## Fuzzing Test Categories

### 1. Memory Exhaustion Tests
Tests memory management under stress:
- Creating multiple tries simultaneously
- Adding many prefixes to each trie
- Testing memory cleanup

### 2. Bitmap Edge Cases
Tests the bitmap operations used for prefix tracking:
- Setting/clearing bits at boundaries
- Testing 32-bit word boundaries
- Testing bitmap expansion

### 3. Overlapping Prefixes
Tests longest prefix match correctness:
- Multiple prefixes with different lengths
- Prefixes that overlap in address space
- Verification of longest match selection

### 4. Lookup All Functionality
Tests the `lpm_lookup_all` function:
- Multiple matching prefixes
- Result structure management
- Memory allocation for results

### 5. Batch Operations
Tests batch lookup performance:
- Vectorized lookup operations
- Memory management for batch results
- Consistency with individual lookups

### 6. Custom Allocators
Tests custom memory management:
- Different allocation strategies
- Memory leak detection
- Performance with custom allocators

### 7. IPv6 Functionality
Tests IPv6 address support:
- 128-bit address handling
- IPv6 prefix operations
- Memory usage with large addresses

### 8. Result Management
Tests the result structure API:
- Dynamic capacity expansion
- Memory allocation patterns
- Cleanup and destruction

### 9. Error Conditions
Tests error handling:
- Invalid parameters
- Out-of-bounds access
- Memory allocation failures

## AFL Integration

The advanced fuzzing test (`test_fuzz_advanced.c`) is designed to work with AFL:

### Input Format
The test expects binary input with the following format:
```
[4 bytes] Number of prefixes
[4 bytes] Number of lookups
[For each prefix:]
  [1 byte]  Prefix length in bits
  [4 bytes] Next hop value
  [N bytes] Prefix bytes (N = (prefix_len + 7) / 8)
[For each lookup:]
  [16 bytes] Address to lookup
  [4 bytes]  Expected result
```

### AFL Dictionary
The `tests/dict.txt` file contains common IPv4/IPv6 prefixes that AFL can use to generate more effective test cases.

## Continuous Integration

The fuzzing tests are integrated into the CI pipeline:
- Run as part of the test suite
- Timeout limits to prevent hanging
- Separate labels for fuzzing vs unit tests

## Bug Detection

The fuzzing tests have already found and fixed bugs:
- **Memory Safety**: Fixed `lpm_result_create(0)` handling
- **Edge Cases**: Improved bitmap boundary handling
- **API Robustness**: Enhanced error condition testing

## Contributing

When adding new features to the LPM implementation:
1. Add corresponding fuzzing tests
2. Test with AFL for extended periods
3. Include edge cases in the test suite
4. Update the fuzzing documentation

## Performance Considerations

- Fuzzing tests are designed to run quickly in CI
- AFL fuzzing can run for extended periods
- Memory usage is monitored during testing
- Performance regression testing is included

## Troubleshooting

### Common Issues
1. **Build Failures**: Ensure AFL is properly installed
2. **Test Timeouts**: Increase timeout in CMakeLists.txt
3. **Memory Issues**: Check for memory leaks with valgrind
4. **AFL Crashes**: Check input format and bounds

### Debugging
```bash
# Run with valgrind for memory checking
valgrind --leak-check=full ./build/tests/test_fuzz

# Run with gdb for debugging
gdb --args ./build/tests/test_fuzz_advanced

# Run AFL with debug output
AFL_DEBUG=1 afl-fuzz -i fuzz_input -o fuzz_output ./build/tests/test_fuzz_advanced @@
``` 
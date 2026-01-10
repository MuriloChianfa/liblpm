---
name: Bug Report
about: Report a bug or unexpected behavior in liblpm
title: "[BUG] "
labels: bug
assignees: ''

---

## Bug Description

<!-- A clear and concise description of what the bug is -->

## Environment

**Operating System:**
<!-- e.g., Ubuntu 22.04, macOS 13.0, CentOS 8 -->

**Compiler:**
<!-- e.g., GCC 11.4.0, Clang 14.0.0 -->

**liblpm Version:**
<!-- e.g., 2.0.0, or commit hash if building from source -->

**CPU Architecture:**
<!-- e.g., Intel Core i7-12700K, AMD Ryzen 9 5900X, ARM -->

**SIMD Support:**
<!-- e.g., SSE2, AVX2, AVX512F - run `cat /proc/cpuinfo | grep flags` on Linux -->

**Build Configuration:**
<!-- e.g., Release, Debug, specific CMake options used -->

## Steps to Reproduce

<!-- Provide a minimal, reproducible example -->

1. Build with: <!-- e.g., cmake ..; make -->
2. Run with: <!-- e.g., ./test_program -->
3. Observe: <!-- what happens -->

### Minimal Code Example

```c
/* Minimal code that reproduces the issue */
#include <lpm.h>

int main() {
    // Your code here
    return 0;
}
```

### Build Commands

```bash
# Commands used to build and run
```

## Expected Behavior

<!-- What you expected to happen -->

## Actual Behavior

<!-- What actually happened -->

## Error Messages

<!-- If applicable, paste any error messages, stack traces, or debug output -->

```
Paste error messages here
```

## Additional Context

### Severity

- [ ] Critical - Crashes/data corruption
- [ ] High - Incorrect results
- [ ] Medium - Performance issues
- [ ] Low - Minor inconvenience

### Frequency

- [ ] Always reproducible
- [ ] Intermittent
- [ ] Rare

### Affected Components

<!-- Check all that apply -->

- [ ] IPv4 lookups
- [ ] IPv6 lookups
- [ ] Batch lookups
- [ ] Add operations
- [ ] Delete operations
- [ ] Memory management
- [ ] SIMD optimizations
- [ ] C++ bindings
- [ ] Go bindings
- [ ] Build system
- [ ] Documentation
- [ ] Other: <!-- specify -->

### Debugging Information

<!-- If you've done any debugging, share your findings -->

**Valgrind output:**
```
<!-- If applicable -->
```

**GDB backtrace:**
```
<!-- If applicable -->
```

**AddressSanitizer output:**
```
<!-- If applicable -->
```

## Workarounds

<!-- If you found any workarounds, please describe them -->

## Possible Fix

<!-- If you have suggestions on how to fix the bug -->

## Related Issues

<!-- Link any related issues or PRs -->

## Testing

<!-- Have you tested with different configurations? -->

- [ ] Tested with Debug build
- [ ] Tested with different compiler
- [ ] Tested on different OS
- [ ] Checked with Valgrind
- [ ] Verified with latest version

## Additional Files

<!-- If you have test data, core dumps, or other files, mention them here -->

---

**Note:** For security vulnerabilities, please **DO NOT** open a public issue. Instead, email murilo.chianfa@outlook.com. See [SECURITY.md](../SECURITY.md) for details.

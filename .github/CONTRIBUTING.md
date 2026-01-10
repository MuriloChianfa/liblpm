# Contributing to liblpm

Thank you for your interest in contributing to liblpm! We welcome contributions from the community and appreciate your efforts to make this library better.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Performance Considerations](#performance-considerations)
- [Documentation](#documentation)
- [Pull Request Process](#pull-request-process)
- [Reporting Bugs](#reporting-bugs)
- [Feature Requests](#feature-requests)

## Code of Conduct

This project and everyone participating in it is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to murilo.chianfa@outlook.com.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone --recursive https://github.com/YOUR_USERNAME/liblpm.git
   cd liblpm
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/MuriloChianfa/liblpm.git
   ```
4. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Environment

### Using Docker (Recommended)

We provide Docker containers with all development tools pre-configured:

```bash
# Build development container
./scripts/docker-build.sh dev

# Run interactive development environment
docker run -it --rm -v "$PWD:/workspace" liblpm-dev
```

See [docs/DOCKER.md](../docs/DOCKER.md) for more details.

### Local Setup

#### Requirements

- GCC 11+ or Clang 14+
- CMake 3.16+
- Git with submodules support

#### Ubuntu/Debian

```bash
apt install build-essential cmake libc6-dev git
```

#### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --verbose
```

## How to Contribute

### Types of Contributions

We welcome various types of contributions:

- **Bug fixes**: Fix issues reported in the issue tracker
- **New features**: Add new functionality or optimizations
- **Documentation**: Improve README, API docs, or code comments
- **Tests**: Add test cases or improve test coverage
- **Benchmarks**: Add new benchmarks or improve existing ones
- **Language bindings**: Improve C++/Go bindings or add new language bindings
- **Performance optimizations**: SIMD improvements, cache optimizations, etc.

### Before You Start

1. **Check existing issues** to see if someone is already working on it
2. **Open an issue** for discussion if you're proposing a significant change
3. **Keep changes focused**: One feature/fix per pull request
4. **Follow the coding standards** described below

## Coding Standards

### C Code Style

- **Standard**: C23 with GNU extensions
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 120 characters maximum (soft limit)
- **Naming conventions**:
  - Functions: `lpm_function_name()` (snake_case with `lpm_` prefix)
  - Types: `lpm_type_name_t` (snake_case with `lpm_` prefix and `_t` suffix)
  - Constants/Macros: `LPM_CONSTANT_NAME` (UPPER_CASE with `LPM_` prefix)
  - Variables: `variable_name` (snake_case)

### Code Organization

```c
/* Include order */
#include <system_headers.h>
#include <third_party/headers.h>
#include "lpm.h"
#include "internal_headers.h"

/* Function documentation */
/**
 * Brief description of the function.
 *
 * @param trie The LPM trie structure
 * @param addr The address to lookup
 * @return The next hop value, or LPM_INVALID_NEXT_HOP if not found
 */
uint32_t lpm_lookup(const lpm_trie_t *trie, const uint8_t *addr);
```

### Memory Management

- Always check return values of allocation functions
- Use aligned allocations for performance-critical structures
- Free all allocated memory in error paths
- Document ownership and lifetime of pointers

### SIMD Code

- Use GNU ifunc for runtime CPU feature detection
- Provide scalar fallback implementations
- Use `__attribute__((target("avx2")))` for SIMD variants
- Test on multiple CPU architectures if possible

## Testing Guidelines

### Running Tests

```bash
cd build
ctest --verbose

# Run specific tests
./tests/test_basic
./tests/test_fuzz
```

### Writing Tests

- Add tests in the `tests/` directory
- Use descriptive test names
- Test edge cases: empty trie, maximum depth, invalid inputs
- Test both IPv4 and IPv6
- Include performance regression tests for critical paths

### Fuzzing

We use AFL++ for fuzz testing:

```bash
./tests/fuzz_setup.sh
cd build_afl
./run_fuzz.sh
```

See [tests/FUZZING.md](../tests/FUZZING.md) for details.

### Memory Safety

Run Valgrind to check for memory leaks:

```bash
cd build
valgrind --leak-check=full --suppressions=../tests/valgrind.supp ./tests/test_basic
```

### Static Analysis

```bash
cd tests
./test_cppcheck.sh
```

## Performance Considerations

This is a high-performance library. Keep these in mind:

### Critical Paths

The lookup functions are in the hot path. For these functions:

- Minimize branches (use branchless code where possible)
- Optimize for cache locality
- Consider SIMD optimizations
- Profile before and after changes

### Benchmarking

Always benchmark performance-related changes:

```bash
cd build
./benchmarks/bench_lookup

# Compare with DPDK (if available)
./benchmarks/bench_comparison
```

### Performance Requirements

- Lookup operations: No allocations
- Add/delete operations: O(depth) complexity
- Memory usage: Minimize per-node overhead
- Cache misses: Keep hot data in L1/L2 cache

## Documentation

### Code Comments

- Document public API functions in header files
- Explain complex algorithms with inline comments
- Document performance characteristics and assumptions
- Mark SIMD-specific code clearly

### External Documentation

- Update README.md for user-facing changes
- Update API documentation for new functions
- Add entries to docs/ for architecture changes
- Update CHANGELOG (if present) with notable changes

### Language Bindings

If you modify the C API:

1. Update C++ bindings in `bindings/cpp/`
2. Update Go bindings in `bindings/go/`
3. Update binding examples and tests
4. Update binding documentation

## Pull Request Process

### Before Submitting

1. **Update your branch** with latest upstream:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run all tests**:
   ```bash
   cd build
   make clean
   make -j$(nproc)
   ctest --verbose
   ```

3. **Check for memory leaks**:
   ```bash
   valgrind --leak-check=full ./tests/test_basic
   ```

4. **Run static analysis**:
   ```bash
   cd tests
   ./test_cppcheck.sh
   ```

5. **Format your code** (if using an auto-formatter)

6. **Write clear commit messages**:
   ```
   Brief summary (50 chars or less)
   
   More detailed explanation if needed. Explain the problem
   this commit solves and why you chose this approach.
   
   Closes #123
   ```

### Submitting the PR

1. **Push your branch** to your fork
2. **Open a pull request** against the `main` branch
3. **Fill out the PR template** completely
4. **Link related issues** using "Closes #123" or "Fixes #456"
5. **Wait for CI** to complete
6. **Respond to review comments** promptly

### PR Templates

We provide templates for different types of PRs:

- General changes: Default template
- Bug fixes: `BUG_FIX_TEMPLATE.md`
- New features: `FEATURE_TEMPLATE.md`

Select the appropriate template when creating your PR.

### Review Process

- A maintainer will review your PR within a few days
- Address review comments by pushing new commits
- Once approved, a maintainer will merge your PR
- PRs require at least one approval from a maintainer

## Reporting Bugs

### Security Vulnerabilities

**DO NOT** open public issues for security vulnerabilities. Use email instead. See [SECURITY.md](SECURITY.md) for details.

### Regular Bugs

Use the **Bug Report** issue template and include:

1. **Description**: Clear description of the bug
2. **Environment**: OS, compiler version, CPU architecture
3. **Steps to reproduce**: Minimal code to reproduce the issue
4. **Expected behavior**: What you expected to happen
5. **Actual behavior**: What actually happened
6. **Additional context**: Logs, stack traces, etc.

### Good Bug Report Example

```markdown
## Description
lpm_lookup returns incorrect next hop for specific IPv6 prefix

## Environment
- OS: Ubuntu 22.04
- Compiler: GCC 11.4.0
- CPU: Intel Core i7-12700K (AVX2 support)

## Steps to Reproduce
[minimal code example]

## Expected Behavior
Should return next_hop = 100

## Actual Behavior
Returns LPM_INVALID_NEXT_HOP
```

## Feature Requests

Use the **Feature Request** issue template and include:

1. **Problem statement**: What problem does this solve?
2. **Proposed solution**: Your suggested implementation
3. **Alternatives considered**: Other approaches you've thought about
4. **Additional context**: Use cases, examples, etc.

### Feature Evaluation Criteria

We evaluate features based on:

- **Performance impact**: Does it maintain or improve performance?
- **API consistency**: Does it fit the existing API design?
- **Maintenance burden**: How much code/complexity does it add?
- **Use cases**: How many users will benefit?

## Recognition

Contributors will be:

- Credited in release notes for significant contributions
- Mentioned in commit history (of course!)

Thank you for contributing to liblpm!

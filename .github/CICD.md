# CI/CD Pipeline Documentation

This document describes the automated testing and continuous integration setup for liblpm.

## Overview

The project uses GitHub Actions for automated testing with three main workflows:

1. **CI** (`ci.yml`) - Comprehensive testing for main branch and releases
2. **Pull Request Checks** (`pr.yml`) - Fast checks for pull requests
3. **Nightly Tests** (`nightly.yml`) - Extended testing and fuzzing

## Workflow Details

### CI Workflow (`ci.yml`)

Runs on:
- Push to `main`, `master`, or `develop` branches
- Pull requests to these branches

**Jobs:**

1. **test-linux** - Multi-platform testing
   - Builds with GCC and Clang
   - Debug and Release configurations
   - Runs unit tests, fuzzing tests, and static analysis
   - Tests on Ubuntu with different compiler combinations

2. **test-macos** - macOS compatibility
   - Tests on macOS with both Debug and Release builds
   - Ensures cross-platform compatibility

3. **test-valgrind** - Memory leak detection
   - Runs all tests with Valgrind
   - Checks for memory leaks and invalid memory access
   - Uses suppression file to filter false positives

4. **static-analysis** - Code quality checks
   - Runs cppcheck for static analysis
   - Runs clang-tidy for additional checks
   - Identifies potential issues and code quality problems

5. **build-verification** - Build configuration testing
   - Tests different build configurations:
     - Shared vs static libraries
     - Native optimizations
     - With/without benchmarks
     - With/without tests

6. **fuzzing** - Automated fuzzing (main branch only)
   - Sets up AFL fuzzing environment
   - Runs short fuzzing sessions
   - Checks for crashes and memory issues

7. **coverage** - Code coverage analysis (main branch only)
   - Generates coverage reports
   - Uploads to Codecov for tracking

### Pull Request Checks (`pr.yml`)

Runs on:
- Pull requests to `main`, `master`, or `develop` branches

**Jobs:**

1. **pr-checks** - Fast Linux testing
   - Debug and Release builds
   - Unit tests, static analysis, and basic fuzzing
   - Optimized for speed

2. **pr-macos** - macOS compatibility check
   - Quick macOS build and test
   - Ensures cross-platform compatibility

### Nightly Tests (`nightly.yml`)

Runs on:
- Daily at 2 AM UTC
- Manual trigger via workflow_dispatch

**Jobs:**

1. **nightly-fuzzing** - Extended fuzzing
   - 1-hour fuzzing sessions
   - AFL-based fuzzing with dictionary
   - Uploads fuzzing artifacts

2. **nightly-memory** - Extended memory testing
   - 15-minute Valgrind sessions per test
   - Detailed memory leak analysis
   - Uploads memory test logs

3. **nightly-benchmarks** - Performance testing
   - Runs performance benchmarks
   - Tests with native optimizations
   - Uploads benchmark results

4. **nightly-multicore-fuzzing** - Multi-core fuzzing
   - Uses multiple CPU cores for fuzzing
   - Extended 1-hour sessions
   - Uploads multi-core fuzzing results

5. **nightly-coverage** - Extended coverage analysis
   - Runs tests multiple times for better coverage
   - Generates detailed coverage reports
   - Uploads coverage data

## Test Categories

### Unit Tests
- **basic_tests** - Core functionality testing
- **lookup_all_tests** - Lookup_all API testing
- **fuzz_tests** - Basic fuzzing tests
- **fuzz_advanced_tests** - Advanced fuzzing tests

### Static Analysis
- **cppcheck_tests** - Static analysis with cppcheck
- **clang-tidy** - Additional static analysis

### Fuzzing Tests
- **AFL-based fuzzing** - File-based fuzzing with AFL
- **Multi-core fuzzing** - Parallel fuzzing sessions
- **Memory safety testing** - Valgrind-based memory analysis

## Configuration Options

The workflows test various build configurations:

- **BUILD_SHARED_LIBS**: Shared vs static libraries
- **BUILD_TESTS**: Enable/disable test building
- **BUILD_BENCHMARKS**: Enable/disable benchmark building
- **ENABLE_NATIVE_ARCH**: Native CPU optimizations
- **CMAKE_BUILD_TYPE**: Debug vs Release builds

## Artifacts

The workflows generate several artifacts:

- **Fuzzing results** - Crash files and fuzzing statistics
- **Memory test logs** - Valgrind output files
- **Benchmark results** - Performance measurement data
- **Coverage reports** - Code coverage information

## Local Testing

To run the same tests locally:

```bash
# Basic build and test
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..
make -j$(nproc)
ctest --verbose

# Run specific test categories
ctest -L unit --verbose        # Unit tests only
ctest -L fuzz --verbose        # Fuzzing tests only
ctest -L static_analysis --verbose  # Static analysis only

# Memory testing with Valgrind
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes --error-exitcode=1 \
         --suppressions=../tests/valgrind.supp \
         ./test_basic

# Static analysis
cppcheck --enable=all --inconclusive --force \
         --suppress=missingIncludeSystem \
         --suppress=unusedFunction \
         src include tests

# Fuzzing setup
./tests/fuzz_setup.sh
./build_afl.sh
./run_fuzz.sh
```

## Troubleshooting

### Common Issues

1. **Build failures**: Check compiler version and dependencies
2. **Test failures**: Review test output for specific error messages
3. **Memory leaks**: Check Valgrind logs for detailed information
4. **Fuzzing crashes**: Review crash files in fuzzing artifacts

### Debugging

- Enable verbose output with `--verbose` flags
- Check artifact uploads for detailed logs
- Review workflow logs for step-by-step debugging

## Performance Considerations

- PR checks are optimized for speed (5-10 minutes)
- Full CI runs take 15-30 minutes
- Nightly tests can run for 1-3 hours
- Fuzzing sessions are time-limited to prevent infinite runs

## Security

- All workflows run in isolated environments
- No secrets or sensitive data are exposed
- Artifacts are automatically cleaned up after 90 days
- Fuzzing results are uploaded for analysis

## Contributing

When contributing to the project:

1. Ensure your changes pass PR checks
2. Add tests for new functionality
3. Update documentation as needed
4. Consider adding fuzzing tests for new features

## Monitoring

- Check workflow status on GitHub Actions tab
- Monitor coverage trends on Codecov
- Review fuzzing artifacts for new crashes
- Track performance regressions in benchmarks 
# CI/CD Pipeline

This document describes the automated testing and continuous integration setup for liblpm.

## Workflows

### 1. CI (`ci.yml`)
**Triggers**: Push/PR to main, develop  
**Purpose**: Comprehensive testing with Docker containers

**Jobs**:
- `build-and-test` - Matrix testing (GCC/Clang) in containers
- `test-cpp-bindings` - C++ wrapper testing
- `test-go-bindings` - Go bindings testing
- `code-quality` - cppcheck and clang-format checks
- `ci-summary` - Results aggregation

### 2. Build (`build.yml`)
**Triggers**: Push/PR/manual  
**Purpose**: Build and test all container images

**Jobs**:
- `build-images` - Parallel build of 7 containers (base, dev, test, fuzz, cpp, go, benchmark)
- `test-containers` - Run tests in each container
- `benchmark` - Performance testing with DPDK
- `summary` - Build results

### 3. Pull Request Checks (`pr.yml`)
**Triggers**: Pull requests  
**Purpose**: Fast validation

**Jobs**:
- `pr-checks` - Quick Linux testing
- `pr-macos` - macOS compatibility

### 4. Nightly Tests (`nightly.yml`)
**Triggers**: Daily at 2 AM UTC / manual  
**Purpose**: Extended testing

**Jobs**:
- `nightly-fuzzing` - 1-hour AFL++ sessions
- `nightly-memory` - Extended Valgrind checks
- `nightly-benchmarks` - Performance monitoring
- `nightly-multicore-fuzzing` - Parallel fuzzing
- `nightly-coverage` - Code coverage analysis

## Available Containers

| Container | Purpose |
|-----------|---------|
| liblpm-dev | Interactive development (GCC 15.2, Clang 21.1, CMake 3.31) |
| liblpm-test | Automated testing (unit, fuzz, valgrind, cppcheck) |
| liblpm-cpp | C++ bindings testing |
| liblpm-go | Go bindings testing |
| liblpm-fuzz | AFL++ fuzzing environment |
| liblpm-benchmark | DPDK performance comparison |

## Local Testing

### With Docker (Recommended)
```bash
# Build containers
./scripts/docker-build.sh all

# Run tests
docker run --rm liblpm-test

# Interactive development
docker run -it --rm -v "$PWD:/workspace" liblpm-dev
```

### Native Build
```bash
# Build and test
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON ..
make -j$(nproc)
ctest --verbose

# Memory check
valgrind --leak-check=full ./tests/test_basic

# Static analysis
cppcheck --enable=all -I include -I external/libdynemit/include src/
```

### Using `act` (GitHub Actions locally)
```bash
# Validate workflows
act -l

# Test CI workflow
act push -W .github/workflows/ci.yml --job code-quality

# Test build workflow
act workflow_dispatch -W .github/workflows/build.yml
```

## Test Categories

- **Unit tests**: Core functionality (`test_basic`)
- **Fuzz tests**: Edge case detection (`test_fuzz`, `test_fuzz_advanced`)
- **Static analysis**: Code quality (`cppcheck_tests`)
- **Memory tests**: Leak detection (Valgrind)
- **Benchmarks**: Performance validation

## Build Configurations Tested

- Compilers: GCC 15.2, Clang 21.1
- Build types: Debug, Release
- Libraries: Shared and static
- Optimizations: Native arch enabled/disabled
- Platforms: Linux (Ubuntu 25.10), macOS

## Artifacts

Workflows generate:
- Test results and logs
- Fuzzing crash files
- Memory test reports
- Benchmark results
- Built container images (7 days retention)

## Documentation

- **Container usage**: [docs/DOCKER.md](DOCKER.md)
- **Fuzzing guide**: [tests/FUZZING.md](../tests/FUZZING.md)
- **Container quick ref**: [docker/README.md](../docker/README.md)

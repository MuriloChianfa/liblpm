# Docker Images for liblpm

Quick reference for liblpm Docker images.

## Available Images

| Image | Description | Use Case |
|-------|-------------|----------|
| `liblpm-base` | Base build/runtime images | Foundation for other images |
| `liblpm-dev` | Development environment | Interactive development |
| `liblpm-test` | Testing environment | Automated testing, CI |
| `liblpm-fuzz` | AFL++ fuzzing | Security testing |
| `liblpm-cpp` | C++ bindings | C++ wrapper testing |
| `liblpm-go` | Go bindings | Go wrapper testing |
| `liblpm-benchmark` | DPDK benchmarking | Performance comparison |

## Quick Start

### Build All Images

```bash
# From project root
./scripts/docker-build.sh all
```

### Build Specific Image

```bash
./scripts/docker-build.sh dev
./scripts/docker-build.sh test
./scripts/docker-build.sh fuzz
```

## Common Commands

### Development

```bash
# Interactive development
docker run -it --rm -v "$PWD:/workspace" liblpm-dev

# Build inside container
docker run --rm -v "$PWD:/workspace" liblpm-dev bash -c "
  cd /workspace && mkdir -p build && cd build
  cmake .. && make -j$(nproc)
"
```

### Testing

```bash
# Run all tests
docker run --rm liblpm-test

# Run specific test
docker run --rm liblpm-test bash -c "cd /build/build && ctest -R test_basic -V"
```

### Fuzzing

```bash
# Single-core fuzzing
docker run --rm liblpm-fuzz /fuzz/run_fuzz_single.sh

# Multi-core fuzzing (4 cores)
docker run --rm --cpus=4 -e AFL_CORES=4 liblpm-fuzz

# Persistent fuzzing with output
docker run --rm -v "$PWD/fuzz_output:/fuzz/fuzz_output" --cpus=4 liblpm-fuzz
```

### Language Bindings

```bash
# Test C++ bindings
docker run --rm liblpm-cpp

# Test Go bindings
docker run --rm liblpm-go
```

### Benchmarking

```bash
# Run benchmarks
docker run --rm liblpm-benchmark

# With results output
docker run --rm -v "$PWD/results:/build/results" liblpm-benchmark
```

## Image Details

### liblpm-base

Multi-stage base image with two targets:

- **builder**: Full build environment (gcc-15, clang-21, cmake-4.2)
- **runtime**: Minimal runtime (just libraries)

**Size:** ~800MB (builder), ~200MB (runtime)

```bash
docker build -f docker/Dockerfile.base --target builder -t liblpm-base:builder .
docker build -f docker/Dockerfile.base --target runtime -t liblpm-base:runtime .
```

### liblpm-dev

Complete development environment with all tools.

**Includes:** gcc, clang, cmake, gdb, valgrind, strace, cppcheck, afl++

**Size:** ~1.2GB

**Usage:**

```bash
docker run -it --rm -v "$PWD:/workspace" liblpm-dev bash
```

### liblpm-test

Optimized for automated testing with cppcheck and valgrind.

**Size:** ~900MB

**Default:** Runs all tests automatically

```bash
docker run --rm liblpm-test
```

### liblpm-fuzz

AFL++ instrumented build for fuzzing.

**Size:** ~1GB

**Features:**
- Pre-seeded test inputs
- Multicore support
- Persistent output volumes

**Environment Variables:**
- `AFL_CORES`: Number of fuzzer instances (default: 4)

```bash
docker run --rm -e AFL_CORES=8 --cpus=8 liblpm-fuzz
```

### liblpm-cpp

C++17 environment for building and testing C++ bindings.

**Size:** ~800MB

**Tests:** GCC and Clang builds

```bash
docker run --rm liblpm-cpp
```

### liblpm-go

Go bindings with cgo support.

**Size:** ~600MB

**Multi-stage:** Builder -> Runtime

```bash
docker run --rm liblpm-go
```

### liblpm-benchmark

DPDK 24.11 integration for performance comparison.

**Size:** ~1.5GB

**Note:** May require `--privileged` for hugepages

```bash
docker run --rm --privileged liblpm-benchmark
```

## Build Options

### Custom Tags

```bash
./scripts/docker-build.sh --tag v1.2.0 all
./scripts/docker-build.sh --tag dev-2024-01 dev
```

### Multi-Architecture

```bash
./scripts/docker-build.sh --platform linux/amd64,linux/arm64 all
```

### No Cache

```bash
./scripts/docker-build.sh --no-cache test
```

### Verbose Output

```bash
./scripts/docker-build.sh --verbose dev
```

## Image Sizes

Approximate sizes (uncompressed):

| Image | Size |
|-------|------|
| liblpm-base:builder | ~800MB |
| liblpm-base:runtime | ~200MB |
| liblpm-dev | ~1.2GB |
| liblpm-test | ~900MB |
| liblpm-fuzz | ~1GB |
| liblpm-cpp | ~800MB |
| liblpm-go | ~600MB |
| liblpm-benchmark | ~1.5GB |

**Total:** ~6.5GB for all images

## Toolchain Versions

All images use Ubuntu 25.10 with:

- **GCC:** 15.2.0
- **Clang:** 21.1.2
- **CMake:** 4.2.1
- **Python:** 3.x
- **Go:** 1.21+ (Go image only)
- **DPDK:** 24.11 (benchmark image only)

## Registry Information

Images are currently built locally only. 

For registry push plans, see [TODO.md](../TODO.md) in the project root.

## Documentation

For comprehensive documentation, see [docs/DOCKER.md](../docs/DOCKER.md).

Topics covered:
- Detailed usage examples
- Development workflow
- Testing strategies
- Fuzzing guide
- CI/CD integration
- Troubleshooting

## Support

- **Issues:** https://github.com/MuriloChianfa/liblpm/issues
- **Documentation:** [docs/DOCKER.md](../docs/DOCKER.md)
- **Email:** murilo.chianfa@outlook.com

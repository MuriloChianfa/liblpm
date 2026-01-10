# Docker Guide for liblpm

This guide covers using Docker containers for developing, testing, and benchmarking liblpm.

## Table of Contents

- [Quick Start](#quick-start)
- [Available Images](#available-images)
- [Building Images](#building-images)
- [Development Workflow](#development-workflow)
- [Testing](#testing)
- [Fuzzing](#fuzzing)
- [Language Bindings](#language-bindings)
- [Benchmarking](#benchmarking)
- [CI/CD Integration](#cicd-integration)
- [Troubleshooting](#troubleshooting)

## Quick Start

### Prerequisites

- Docker 20.10+ with BuildKit support
- Docker Buildx (for multi-platform builds)
- 4GB+ available disk space

### Build All Images

```bash
# Using the build script (recommended)
./scripts/docker-build.sh all

# Or build individual images
./scripts/docker-build.sh dev
./scripts/docker-build.sh test
```

### Run Development Container

```bash
# Interactive development environment
docker run -it --rm -v "$PWD:/workspace" liblpm-dev:latest

# Inside container:
cd /workspace
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --verbose
```

## Available Images

### Base Images

| Image | Purpose | Size | Base |
|-------|---------|------|------|
| `liblpm-base:builder` | Build environment | ~800MB | Ubuntu 25.10 |
| `liblpm-base:runtime` | Runtime environment | ~200MB | Ubuntu 25.10 |

**Toolchain:**
- GCC 15.2.0
- Clang 21.1.2
- CMake 4.2.1

### Development Images

| Image | Purpose | Use Case |
|-------|---------|----------|
| `liblpm-dev` | Full dev environment | Interactive development, debugging |
| `liblpm-test` | Testing environment | CI/CD, automated testing |
| `liblpm-fuzz` | AFL++ fuzzing | Security testing, edge case discovery |

### Language Binding Images

| Image | Purpose | Language |
|-------|---------|----------|
| `liblpm-cpp` | C++ bindings | C++17 with GCC/Clang |
| `liblpm-go` | Go bindings | Go 1.21+ with cgo |

### Benchmark Images

| Image | Purpose | Requirements |
|-------|---------|--------------|
| `liblpm-benchmark` | DPDK comparison | May need `--privileged` |

## Building Images

### Using the Build Script

The build script provides convenient options:

```bash
# Build all images
./scripts/docker-build.sh all

# Build specific image
./scripts/docker-build.sh dev

# Build with custom tag
./scripts/docker-build.sh --tag v1.2.0 all

# Build for multiple architectures
./scripts/docker-build.sh --platform linux/amd64,linux/arm64 all

# Build without cache
./scripts/docker-build.sh --no-cache test

# Verbose output
./scripts/docker-build.sh --verbose dev
```

### Manual Docker Build

```bash
# Development container
docker build -f docker/Dockerfile.dev -t liblpm-dev .

# Test container
docker build -f docker/Dockerfile.test -t liblpm-test .

# With build args
docker build \
  --build-arg VERSION=1.2.0 \
  --build-arg GIT_SHA=$(git rev-parse --short HEAD) \
  -f docker/Dockerfile.dev \
  -t liblpm-dev:1.2.0 \
  .
```

## Development Workflow

### Interactive Development

```bash
# Start development container with volume mount
docker run -it --rm \
  -v "$PWD:/workspace" \
  -w /workspace \
  liblpm-dev:latest

# Inside container - typical workflow:
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run tests
ctest --verbose

# Debug with gdb
gdb ./tests/test_basic

# Memory check with valgrind
valgrind --leak-check=full ./tests/test_basic
```

### Code Analysis

```bash
# Run cppcheck
docker run --rm -v "$PWD:/workspace" liblpm-dev:latest \
  bash -c "cppcheck --enable=all -I include -I external/libdynemit/include src/"

# Check formatting
docker run --rm -v "$PWD:/workspace" liblpm-dev:latest \
  bash -c "find src include -name '*.c' -o -name '*.h' | xargs clang-format -i"
```

### Building Different Configurations

```bash
# Release build with native optimizations
docker run --rm -v "$PWD:/workspace" liblpm-dev:latest \
  bash -c "
    mkdir -p build_release && cd build_release
    cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NATIVE_ARCH=ON ..
    ninja
  "

# Debug build with sanitizers
docker run --rm -v "$PWD:/workspace" liblpm-dev:latest \
  bash -c "
    mkdir -p build_debug && cd build_debug
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined' ..
    ninja
  "
```

## Testing

### Run All Tests

```bash
# Run test container (runs all tests automatically)
docker run --rm liblpm-test:latest

# Run with custom test selection
docker run --rm liblpm-test:latest bash -c "
  cd /build/build
  ctest -R test_basic --verbose
"
```

### Extract Test Results

```bash
# Mount volume to save test results
docker run --rm \
  -v "$PWD/test-results:/build/test-results" \
  liblpm-test:latest
```

### Memory Testing

```bash
# Run valgrind in test container
docker run --rm liblpm-test:latest bash -c "
  cd /build/build
  valgrind --leak-check=full --error-exitcode=1 ./tests/test_basic
"
```

## Fuzzing

### Quick Fuzzing Session

```bash
# Run AFL++ fuzzing (single core)
docker run --rm liblpm-fuzz:latest /fuzz/run_fuzz_single.sh

# Run multicore fuzzing (4 cores)
docker run --rm --cpus=4 -e AFL_CORES=4 liblpm-fuzz:latest
```

### Persistent Fuzzing

```bash
# Save fuzzing results to host
docker run --rm \
  -v "$PWD/fuzz_output:/fuzz/fuzz_output" \
  --cpus=4 \
  -e AFL_CORES=4 \
  liblpm-fuzz:latest

# Monitor fuzzing progress
docker run --rm \
  -v "$PWD/fuzz_output:/fuzz/fuzz_output" \
  liblpm-fuzz:latest \
  afl-whatsup /fuzz/fuzz_output
```

### Custom Fuzzing

```bash
# Run with custom inputs
docker run --rm \
  -v "$PWD/my_inputs:/fuzz/fuzz_input" \
  -v "$PWD/fuzz_output:/fuzz/fuzz_output" \
  liblpm-fuzz:latest
```

## Language Bindings

### C++ Bindings

```bash
# Build and test C++ bindings
docker run --rm liblpm-cpp:latest

# Extract built artifacts
docker run --rm \
  -v "$PWD/artifacts:/build/artifacts" \
  liblpm-cpp:latest
```

### Go Bindings

```bash
# Build and test Go bindings
docker run --rm liblpm-go:latest

# Interactive Go development
docker run -it --rm liblpm-go:latest bash

# Inside container:
cd /app
go test -v ./liblpm/
go test -bench=. ./benchmarks/
```

## Benchmarking

### Standard Benchmarks

```bash
# Run liblpm benchmarks
docker run --rm liblpm-benchmark:latest
```

### DPDK Comparison

```bash
# Run with privileged mode for hugepages (if needed)
docker run --rm --privileged liblpm-benchmark:latest

# Save benchmark results
docker run --rm \
  -v "$PWD/results:/build/results" \
  liblpm-benchmark:latest
```

### Custom Benchmarks

```bash
# Run specific benchmark
docker run --rm liblpm-benchmark:latest bash -c "
  cd /build/liblpm/build/benchmarks
  ./bench_lookup
"
```

## CI/CD Integration

### GitHub Actions

The project includes pre-configured GitHub Actions workflows:

- `.github/workflows/build.yml` - Build and test all images
- `.github/workflows/ci.yml` - Continuous integration with containers

### Local CI Testing

```bash
# Simulate CI build
./scripts/docker-build.sh --no-cache all

# Run CI tests locally
docker run --rm liblpm-test:latest
docker run --rm liblpm-cpp:latest
docker run --rm liblpm-go:latest
```

## Troubleshooting

### Build Issues

**Problem:** Build fails with "no space left on device"

```bash
# Clean up old images and containers
docker system prune -a

# Remove build cache
docker builder prune -a
```

**Problem:** Submodules not initialized

```bash
# Clone with submodules
git clone --recursive https://github.com/MuriloChianfa/liblpm.git

# Or initialize after clone
git submodule update --init --recursive
```

### Runtime Issues

**Problem:** Container exits immediately

```bash
# Check logs
docker logs <container-id>

# Run with interactive shell
docker run -it --rm liblpm-dev:latest bash
```

**Problem:** Permission denied for mounted volumes

```bash
# Fix ownership (adjust UID/GID as needed)
docker run --rm -v "$PWD:/workspace" liblpm-dev:latest \
  sudo chown -R developer:developer /workspace
```

### Performance Issues

**Problem:** Builds are slow

```bash
# Enable BuildKit for faster builds
export DOCKER_BUILDKIT=1

# Use build cache
./scripts/docker-build.sh --cache-from liblpm-dev:latest dev
```

**Problem:** Fuzzing is slow

```bash
# Increase CPU allocation
docker run --rm --cpus=8 -e AFL_CORES=8 liblpm-fuzz:latest

# Disable CPU frequency scaling (on host)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### Common Errors

**Error:** `docker: command not found`

Install Docker: https://docs.docker.com/get-docker/

**Error:** `permission denied while trying to connect to Docker daemon`

```bash
# Add user to docker group
sudo usermod -aG docker $USER
newgrp docker
```

**Error:** `manifest unknown` when pulling images

Images are built locally, not available in registry yet. Build them first:

```bash
./scripts/docker-build.sh all
```

## Best Practices

### Development

1. **Use volume mounts** for source code (faster than COPY)
2. **Run as non-root** user inside containers
3. **Use .dockerignore** to exclude build artifacts
4. **Layer caching** - order Dockerfile commands from least to most frequently changed

### Testing

1. **Run tests in containers** for reproducibility
2. **Use specific image tags** (not `latest`) in CI
3. **Clean up** test containers after runs (`--rm` flag)
4. **Save test results** to volumes for analysis

### Production

1. **Multi-stage builds** to minimize image size
2. **Pin dependencies** to specific versions
3. **Security scan** images before deployment
4. **Use minimal base images** for runtime

## Additional Resources

- [Docker Documentation](https://docs.docker.com/)
- [Docker Best Practices](https://docs.docker.com/develop/dev-best-practices/)
- [Multi-stage Builds](https://docs.docker.com/build/building/multi-stage/)
- [BuildKit](https://docs.docker.com/build/buildkit/)

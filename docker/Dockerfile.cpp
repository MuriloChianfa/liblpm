# C++ bindings container for liblpm
# Builds and tests C++17 wrapper with latest compilers

FROM ubuntu:25.10

LABEL maintainer="Murilo Chianfa <murilo.chianfa@outlook.com>"
LABEL description="liblpm C++ binding"

ENV DEBIAN_FRONTEND=noninteractive

# Install C++ development environment
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Core build tools
    build-essential \
    gcc-15 \
    g++-15 \
    clang-21 \
    clang++-21 \
    # Build systems
    cmake \
    ninja-build \
    make \
    git \
    cppcheck \
    pkg-config \
    # Libraries
    libc6-dev \
    libnuma-dev \
    # Python
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set compiler alternatives
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 100 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 100 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-21 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-21 100

WORKDIR /build

# Copy source code
COPY . /build/

# Initialize git submodules
RUN git config --global --add safe.directory /build && \
    if [ -f .gitmodules ]; then git submodule update --init --recursive; fi

# Build liblpm with C++ wrapper
RUN mkdir -p build && cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_CPP_WRAPPER=ON \
        -DBUILD_TESTS=ON \
        -DENABLE_NATIVE_ARCH=OFF \
        -GNinja \
        .. && \
    ninja

# Test script for both GCC and Clang
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
echo "=== liblpm C++ Bindings Test Suite ==="\n\
echo ""\n\
\n\
# Test with GCC\n\
echo "=== Building with GCC 15.2 ==="\n\
cd /build && rm -rf build_gcc && mkdir build_gcc && cd build_gcc\n\
cmake \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CPP_WRAPPER=ON \
    -DBUILD_TESTS=ON \
    -DENABLE_NATIVE_ARCH=OFF \
    -GNinja \
    ..\n\
ninja\n\
\n\
echo ""\n\
echo "=== Running C++ Tests (GCC) ==="\n\
ctest -R cpp --output-on-failure --verbose\n\
\n\
echo ""\n\
echo "=== Running C++ Example (GCC) ==="\n\
./bindings/cpp/examples/cpp_basic_example || echo "Example completed"\n\
\n\
# Test with Clang (C++) and GCC (C)\n\
# Note: libdynemit requires GCC, so we use GCC for C and Clang for C++\n\
echo ""\n\
echo "=== Building with Clang 21.1 (C++) and GCC 15.2 (C) ==="\n\
cd /build && rm -rf build_clang && mkdir build_clang && cd build_clang\n\
cmake \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CPP_WRAPPER=ON \
    -DBUILD_TESTS=ON \
    -DENABLE_NATIVE_ARCH=OFF \
    -GNinja \
    ..\n\
ninja\n\
\n\
echo ""\n\
echo "=== Running C++ Tests (Clang++ / GCC) ==="\n\
ctest -R cpp --output-on-failure --verbose\n\
\n\
echo ""\n\
echo "=== Running C++ Example (Clang++ / GCC) ==="\n\
./bindings/cpp/examples/cpp_basic_example || echo "Example completed"\n\
\n\
echo ""\n\
echo "=== C++ Bindings Test Summary ==="\n\
echo "All tests passed!"\n\
echo "  - GCC 15.2 (both C and C++)"\n\
echo "  - Clang 21.1 (C++) with GCC 15.2 (C)"\n\
echo "Note: libdynemit requires GCC for C compilation"\n\
' > /build/run_cpp_tests.sh && chmod +x /build/run_cpp_tests.sh

# Export volume for built artifacts
VOLUME ["/build/artifacts"]

# Export built libraries
RUN mkdir -p /build/artifacts && \
    cp build/liblpm.so* /build/artifacts/ 2>/dev/null || true && \
    cp build/liblpm.a /build/artifacts/ 2>/dev/null || true

# Default command runs C++ tests
CMD ["/build/run_cpp_tests.sh"]

# Usage:
# Build: docker build -f docker/Dockerfile.cpp -t liblpm-cpp .
# Run tests: docker run --rm liblpm-cpp
# Extract artifacts: docker run --rm -v "$PWD/artifacts:/build/artifacts" liblpm-cpp

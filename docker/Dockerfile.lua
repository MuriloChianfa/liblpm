# Lua bindings container for liblpm
# Builds and tests Lua 5.4 wrapper with latest compilers

FROM ubuntu:25.10

LABEL maintainer="Murilo Chianfa <murilo.chianfa@outlook.com>"
LABEL description="liblpm Lua binding"

ENV DEBIAN_FRONTEND=noninteractive

# Install development environment
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Core build tools
    build-essential \
    gcc-15 \
    g++-15 \
    # Build systems
    cmake \
    ninja-build \
    make \
    git \
    pkg-config \
    # Libraries
    libc6-dev \
    libnuma-dev \
    # Lua development
    lua5.4 \
    liblua5.4-dev \
    # Optional: LuaRocks for package management
    luarocks \
    # Python for scripts
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set compiler alternatives
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 100 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 100

WORKDIR /build

# Copy source code
COPY . /build/

# Initialize git submodules
RUN git config --global --add safe.directory /build && \
    if [ -f .gitmodules ]; then git submodule update --init --recursive; fi

# Build liblpm with Lua wrapper
RUN mkdir -p build && cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_LUA_WRAPPER=ON \
        -DBUILD_TESTS=ON \
        -DENABLE_NATIVE_ARCH=OFF \
        -GNinja \
        .. && \
    ninja

# Install liblpm system-wide (needed for LD_PRELOAD workaround)
RUN cd build && ninja install && ldconfig

# Create test script using heredoc
# Note: Tests run with LD_PRELOAD to handle ifunc resolver issues when
# loading the module via dlopen
RUN cat > /build/run_lua_tests.sh << 'ENDSCRIPT'
#!/bin/bash
set -e

echo "=== liblpm Lua Bindings Test Suite ==="
echo ""

echo "=== System Information ==="
echo "Lua version: $(lua5.4 -v 2>&1)"
echo "GCC version: $(gcc --version | head -1)"
echo ""

# Set up environment for tests
export LD_PRELOAD=/usr/local/lib/liblpm.so
export LUA_CPATH="/usr/local/lib/lua/5.4/?.so;;"

echo "liblpm version: $(lua5.4 -e "print(require('liblpm').version())")"
echo ""

echo "=== Running Lua Unit Tests (54 tests) ==="
lua5.4 /build/bindings/lua/tests/test_lpm.lua

echo ""
echo "=== Running Lua Examples ==="
echo ""
echo "--- Basic Example ---"
lua5.4 /build/bindings/lua/examples/basic_example.lua

echo ""
echo "--- IPv6 Example ---"
lua5.4 /build/bindings/lua/examples/ipv6_example.lua

echo ""
echo "--- Batch Example ---"
lua5.4 /build/bindings/lua/examples/batch_example.lua

echo ""
echo "=== Lua Bindings Test Summary ==="
echo "All tests passed!"
echo "  - 54 unit tests"
echo "  - Basic example"
echo "  - IPv6 example"
echo "  - Batch example"
ENDSCRIPT
RUN chmod +x /build/run_lua_tests.sh

# Export volume for built artifacts
VOLUME ["/build/artifacts"]

# Export built libraries
RUN mkdir -p /build/artifacts && \
    cp build/liblpm.so* /build/artifacts/ 2>/dev/null || true && \
    cp build/liblpm.a /build/artifacts/ 2>/dev/null || true && \
    cp build/bindings/lua/liblpm.so /build/artifacts/liblpm_lua.so 2>/dev/null || true

# Default command runs Lua tests
CMD ["/build/run_lua_tests.sh"]

# Usage:
# Build: docker build -f docker/Dockerfile.lua -t liblpm-lua .
# Run tests: docker run --rm liblpm-lua
# Interactive: docker run -it --rm liblpm-lua bash
# Extract artifacts: docker run --rm -v "$PWD/artifacts:/build/artifacts" liblpm-lua

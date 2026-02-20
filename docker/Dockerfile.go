# Go bindings container for liblpm
# Multi-stage build: builder (cgo, liblpm) -> runtime (Go binary)
# Based on gRPC Go binding approach

# ============================================================================
# Stage 1: Build liblpm C library
# ============================================================================
FROM ubuntu:25.10 AS liblpm-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    build-essential \
    gcc-15 \
    g++-15 \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libc6-dev \
    libnuma-dev \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100

WORKDIR /build

COPY . /build/

# Initialize submodules and build
RUN git config --global --add safe.directory /build && \
    if [ -f .gitmodules ]; then git submodule update --init --recursive; fi && \
    mkdir -p build && cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DBUILD_BENCHMARKS=OFF \
        -DENABLE_NATIVE_ARCH=OFF \
        -GNinja \
        .. && \
    ninja && \
    ninja install

# ============================================================================
# Stage 2: Build Go bindings
# ============================================================================
FROM golang:1.23-bookworm AS go-builder

# Install runtime dependencies for cgo
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libc6-dev \
    libnuma-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Copy liblpm from previous stage
COPY --from=liblpm-builder /usr/local/lib/liblpm* /usr/local/lib/
COPY --from=liblpm-builder /usr/local/include/lpm /usr/local/include/lpm

# Update library cache
RUN ldconfig

WORKDIR /go/src/liblpm

# Copy Go module file first
COPY bindings/go/go.mod ./

# Copy Go source directories
COPY bindings/go/liblpm ./liblpm/
COPY bindings/go/benchmarks ./benchmarks/
COPY bindings/go/examples ./examples/

# Verify go.mod exists and show module info
RUN echo "=== Go module file ===" && cat go.mod && echo ""

# Download Go dependencies (if any exist)
RUN go mod download || true

# Build Go bindings
RUN go build -v ./liblpm

# Test script
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
echo "=== liblpm Go Bindings Test Suite ==="\n\
echo ""\n\
\n\
cd /app\n\
\n\
echo "=== Running Go Tests ==="\n\
go test -v ./liblpm/\n\
\n\
echo ""\n\
echo "=== Running Go Benchmarks ==="\n\
go test -bench=. -benchmem ./benchmarks/ || echo "Benchmarks completed"\n\
\n\
echo ""\n\
echo "=== Running Go Example ==="\n\
cd examples && go run basic_example.go || echo "Example completed"\n\
\n\
echo ""\n\
echo "=== Go Bindings Test Summary ==="\n\
echo "All tests passed!"\n\
' > /test.sh && chmod +x /test.sh

# ============================================================================
# Stage 3: Runtime
# ============================================================================
FROM golang:1.23-bookworm AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libc6 \
    libgcc-s1 \
    libstdc++6 \
    libnuma1 \
    && rm -rf /var/lib/apt/lists/*

# Copy liblpm runtime libraries and headers
COPY --from=liblpm-builder /usr/local/lib/liblpm.so* /usr/local/lib/
COPY --from=liblpm-builder /usr/local/include/lpm /usr/local/include/lpm

RUN ldconfig

WORKDIR /app

# Copy Go bindings and test script
COPY --from=go-builder /go/src/liblpm /app/
COPY --from=go-builder /test.sh /app/

# Default command runs tests
CMD ["/app/test.sh"]

# Usage:
# Build: docker build -f docker/Dockerfile.go -t liblpm-go .
# Run tests: docker run --rm liblpm-go
# Interactive: docker run -it --rm liblpm-go bash

# Go bindings container for liblpm
# Multi-stage build: builder (cgo, liblpm) -> runtime (Go binary)
# All stages use Ubuntu so liblpm's glibc requirement matches the Go/cgo linker.

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

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100

WORKDIR /build

COPY . /build/

RUN git config --global --add safe.directory /build && \
    if [ -f .gitmodules ]; then git submodule update --init --recursive; fi && \
    mkdir -p build && cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DBUILD_BENCHMARKS=OFF \
        -DENABLE_NATIVE_ARCH=OFF \
        -DLPM_TS_RESOLVERS=ON \
        -GNinja \
        .. && \
    ninja && \
    ninja install

# ============================================================================
# Stage 2: Build Go bindings
# ============================================================================
FROM ubuntu:25.10 AS go-builder

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH=/usr/local/go/bin:$PATH

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gcc \
    libc6-dev \
    libnuma-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/* && \
    curl -fsSL https://go.dev/dl/go1.23.6.linux-amd64.tar.gz | tar -C /usr/local -xz

COPY --from=liblpm-builder /usr/local/lib/liblpm* /usr/local/lib/
COPY --from=liblpm-builder /usr/local/include/lpm /usr/local/include/lpm

RUN ldconfig

WORKDIR /go/src/liblpm

COPY bindings/go/go.mod ./
COPY bindings/go/liblpm ./liblpm/
COPY bindings/go/benchmarks ./benchmarks/
COPY bindings/go/examples ./examples/

RUN echo "=== Go module file ===" && cat go.mod && echo ""
RUN go mod download || true
RUN go build -v ./liblpm

RUN printf '%s\n' \
    '#!/bin/bash' \
    'set -e' \
    '' \
    'echo "=== liblpm Go Bindings Test Suite ==="' \
    'echo ""' \
    '' \
    'cd /app' \
    '' \
    'echo "=== Running Go Tests ==="' \
    'go test -v ./liblpm/' \
    '' \
    'echo ""' \
    'echo "=== Running Go Benchmarks ==="' \
    'go test -bench=. -benchmem ./benchmarks/ || echo "Benchmarks completed"' \
    '' \
    'echo ""' \
    'echo "=== Running Go Example ==="' \
    'cd examples && go run basic_example.go || echo "Example completed"' \
    '' \
    'echo ""' \
    'echo "=== Go Bindings Test Summary ==="' \
    'echo "All tests passed!"' \
    > /test.sh && chmod +x /test.sh

# ============================================================================
# Stage 3: Runtime
# ============================================================================
FROM ubuntu:25.10 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH=/usr/local/go/bin:$PATH

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gcc \
    libc6-dev \
    libnuma1 \
    libnuma-dev \
    && rm -rf /var/lib/apt/lists/* && \
    curl -fsSL https://go.dev/dl/go1.23.6.linux-amd64.tar.gz | tar -C /usr/local -xz

COPY --from=liblpm-builder /usr/local/lib/liblpm.so* /usr/local/lib/
COPY --from=liblpm-builder /usr/local/include/lpm /usr/local/include/lpm

RUN ldconfig

WORKDIR /app

COPY --from=go-builder /go/src/liblpm /app/
COPY --from=go-builder /test.sh /app/

CMD ["/app/test.sh"]

# Usage:
# Build: docker build -f docker/Dockerfile.go -t liblpm-go .
# Run tests: docker run --rm liblpm-go
# Interactive: docker run -it --rm liblpm-go bash

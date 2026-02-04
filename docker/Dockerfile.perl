# Perl bindings container for liblpm
# Multi-stage build: builder (liblpm, Perl XS) -> runtime
#
# Usage:
#   Build:     docker build -f docker/Dockerfile.perl -t liblpm-perl .
#   Run tests: docker run --rm liblpm-perl
#   Interactive: docker run -it --rm liblpm-perl bash

# ============================================================================
# Stage 1: Build liblpm C library
# ============================================================================
FROM ubuntu:25.10 AS liblpm-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
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
# Stage 2: Build Perl bindings
# ============================================================================
FROM perl:5.42-bookworm AS perl-builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    libc6-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Copy liblpm from previous stage
COPY --from=liblpm-builder /usr/local/lib/liblpm* /usr/local/lib/
COPY --from=liblpm-builder /usr/local/include/lpm /usr/local/include/lpm
COPY --from=liblpm-builder /usr/local/lib/pkgconfig/liblpm.pc /usr/local/lib/pkgconfig/

# Update library cache
RUN ldconfig

WORKDIR /build/perl

# Copy Perl bindings source
COPY bindings/perl/ ./

# Build Perl module
RUN perl Makefile.PL && make

# Run tests during build to verify
RUN prove -Iblib/lib -Iblib/arch t/*.t

# Create test script
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
echo "=== liblpm Perl Bindings Test Suite ==="\n\
echo ""\n\
\n\
cd /app\n\
\n\
echo "=== Module Information ==="\n\
perl -Iblib/lib -Iblib/arch -MNet::LPM -e '\''print "Net::LPM version: $Net::LPM::VERSION\n"; print "liblpm version: " . Net::LPM->version() . "\n";'\''\n\
echo ""\n\
\n\
echo "=== Running Tests ==="\n\
prove -Iblib/lib -Iblib/arch t/*.t\n\
\n\
echo ""\n\
echo "=== Running Example ==="\n\
perl -Iblib/lib -Iblib/arch examples/basic_example.pl\n\
\n\
echo ""\n\
echo "=== Perl Bindings Test Summary ==="\n\
echo "All tests passed!"\n\
' > /test.sh && chmod +x /test.sh

# ============================================================================
# Stage 3: Runtime
# ============================================================================
FROM perl:5.42-slim-bookworm AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libc6 \
    libgcc-s1 \
    && rm -rf /var/lib/apt/lists/*

# Copy liblpm runtime library
COPY --from=liblpm-builder /usr/local/lib/liblpm.so* /usr/local/lib/

# Update library cache
RUN ldconfig

WORKDIR /app

# Copy built Perl module
COPY --from=perl-builder /build/perl/blib /app/blib/
COPY --from=perl-builder /build/perl/t /app/t/
COPY --from=perl-builder /build/perl/examples /app/examples/
COPY --from=perl-builder /test.sh /app/

# Default command runs tests
CMD ["/app/test.sh"]

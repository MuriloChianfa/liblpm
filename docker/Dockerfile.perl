# Perl bindings container for liblpm
# Multi-stage build: builder (liblpm, Perl XS) -> runtime
# All stages use Ubuntu so liblpm's glibc requirement matches the Perl loader.
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
# Stage 2: Build Perl bindings
# ============================================================================
FROM ubuntu:25.10 AS perl-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    libc6-dev \
    libnuma-dev \
    pkg-config \
    perl \
    libperl-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=liblpm-builder /usr/local/lib/liblpm* /usr/local/lib/
COPY --from=liblpm-builder /usr/local/include/lpm /usr/local/include/lpm
COPY --from=liblpm-builder /usr/local/lib/pkgconfig/liblpm.pc /usr/local/lib/pkgconfig/

RUN ldconfig

WORKDIR /build/perl

COPY bindings/perl/ ./

RUN perl Makefile.PL && make

# dlopen-safe resolvers still need the library mapped early under some loaders
ENV LD_PRELOAD=/usr/local/lib/liblpm.so.1

RUN prove -Iblib/lib -Iblib/arch t/*.t

RUN cat > /test.sh <<'EOF' && chmod +x /test.sh
#!/bin/bash
set -e

echo "=== liblpm Perl Bindings Test Suite ==="
echo ""

cd /app

export LD_PRELOAD=/usr/local/lib/liblpm.so.1

echo "=== Module Information ==="
perl -Iblib/lib -Iblib/arch -MNet::LPM -e 'print "Net::LPM version: $Net::LPM::VERSION\n"; print "liblpm version: " . Net::LPM->version() . "\n";'
echo ""

echo "=== Running Tests ==="
prove -Iblib/lib -Iblib/arch t/*.t

echo ""
echo "=== Running Example ==="
perl -Iblib/lib -Iblib/arch examples/basic_example.pl

echo ""
echo "=== Perl Bindings Test Summary ==="
echo "All tests passed!"
EOF

# ============================================================================
# Stage 3: Runtime
# ============================================================================
FROM ubuntu:25.10 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV LD_PRELOAD=/usr/local/lib/liblpm.so.1

RUN apt-get update && apt-get install -y --no-install-recommends \
    perl \
    libnuma1 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=liblpm-builder /usr/local/lib/liblpm.so* /usr/local/lib/

RUN ldconfig

WORKDIR /app

COPY --from=perl-builder /build/perl/blib /app/blib/
COPY --from=perl-builder /build/perl/t /app/t/
COPY --from=perl-builder /build/perl/examples /app/examples/
COPY --from=perl-builder /test.sh /app/

CMD ["/app/test.sh"]

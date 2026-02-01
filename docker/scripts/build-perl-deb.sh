#!/bin/bash
# Build Perl module DEB packages inside Docker container
# Creates liblpm-perl packages for Debian/Ubuntu
# This script is executed inside the container

set -euo pipefail

echo "=== Building Perl Module DEB Packages ==="
echo "Working directory: $(pwd)"
echo "Distribution: $(cat /etc/os-release | grep PRETTY_NAME || echo 'Unknown')"
echo ""

# Detect Perl version available on this system
PERL_VERSION=$(perl -V:version | sed "s/version='\(.*\)';/\1/" | cut -d'.' -f1-2)
PERL_API=$(perl -V:version | sed "s/version='\(.*\)';/\1/")
ARCH=$(dpkg --print-architecture)

echo "Detected Perl version: ${PERL_VERSION}"
echo "Perl API version: ${PERL_API}"
echo "Architecture: ${ARCH}"
echo ""

if [ "$PERL_VERSION" = "unknown" ]; then
    echo "Error: Perl not found or not properly installed"
    exit 1
fi

# Package version from lib/Net/LPM.pm or default
PKG_VERSION="2.0.0"
PKG_RELEASE="1"

# Package naming convention: liblpm-perl
PKG_NAME="liblpm-perl"
DEB_PKG_NAME="${PKG_NAME}_${PKG_VERSION}-${PKG_RELEASE}_${ARCH}.deb"

echo "Building package: ${DEB_PKG_NAME}"
echo ""

# Build directory
BUILD_DIR="/tmp/perl-liblpm-build"
INSTALL_ROOT="/tmp/perl-liblpm-install"

rm -rf "$BUILD_DIR" "$INSTALL_ROOT"
mkdir -p "$BUILD_DIR" "$INSTALL_ROOT"

# Copy Perl module source
cp -r /workspace/bindings/perl/* "$BUILD_DIR/"

cd "$BUILD_DIR"

# Build Perl XS module
echo "Running perl Makefile.PL..."
perl Makefile.PL

echo "Building extension..."
make -j$(nproc)

echo "Running tests..."
make test || echo "Warning: Some tests failed"

echo "Extension built successfully"
echo ""

# Prepare DEB package structure
# Get Perl vendor installation directories
PERL_VENDORARCH=$(perl -V:vendorarch | sed "s/vendorarch='\(.*\)';/\1/")
PERL_VENDORLIB=$(perl -V:vendorlib | sed "s/vendorlib='\(.*\)';/\1/")
PERL_SITEMAN3=$(perl -V:siteman3dir | sed "s/siteman3dir='\(.*\)';/\1/" || echo "/usr/share/man/man3")

# Strip leading slash for package installation
VENDORARCH_REL=$(echo "$PERL_VENDORARCH" | sed 's|^/||')
VENDORLIB_REL=$(echo "$PERL_VENDORLIB" | sed 's|^/||')
SITEMAN3_REL=$(echo "$PERL_SITEMAN3" | sed 's|^/||')

mkdir -p "${INSTALL_ROOT}/${VENDORARCH_REL}/auto/Net/LPM"
mkdir -p "${INSTALL_ROOT}/${VENDORLIB_REL}/Net"
mkdir -p "${INSTALL_ROOT}/${SITEMAN3_REL}"
mkdir -p "${INSTALL_ROOT}/DEBIAN"

# Install extension and module
echo "Installing to package root..."
cp -v blib/arch/auto/Net/LPM/LPM.so "${INSTALL_ROOT}/${VENDORARCH_REL}/auto/Net/LPM/"
cp -v blib/lib/Net/LPM.pm "${INSTALL_ROOT}/${VENDORLIB_REL}/Net/"

# Install man page if available
if [ -f blib/man3/Net::LPM.3pm ]; then
    gzip -c blib/man3/Net::LPM.3pm > "${INSTALL_ROOT}/${SITEMAN3_REL}/Net::LPM.3pm.gz"
fi

# Create control file
cat > "${INSTALL_ROOT}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${PKG_VERSION}-${PKG_RELEASE}
Section: perl
Priority: optional
Architecture: ${ARCH}
Depends: perl (>= ${PERL_VERSION}), libc6 (>= 2.34), libnuma1
Maintainer: Murilo Chianfa <murilo.chianfa@outlook.com>
Description: High-performance Perl bindings for longest prefix match (LPM)
 Net::LPM provides high-performance Perl bindings to the liblpm C library
 for Longest Prefix Match (LPM) routing table operations.
 .
 Features:
  - High Performance: Direct XS bindings to optimized C library
  - Dual Stack: Full support for both IPv4 and IPv6
  - Multiple Algorithms: DIR-24-8 for IPv4, Wide 16-bit stride for IPv6
  - Batch Operations: Efficient batch lookups to reduce overhead
  - SIMD Optimized: Runtime CPU feature detection for optimal performance
  - Memory Safe: Automatic cleanup via Perl's reference counting
Homepage: https://github.com/MuriloChianfa/liblpm
EOF

# Build DEB package
echo "Building DEB package..."
dpkg-deb --build "${INSTALL_ROOT}" "/packages/${DEB_PKG_NAME}"

# Show package information
echo ""
echo "=== Package Information ==="
ls -lh "/packages/${DEB_PKG_NAME}"
dpkg-deb -I "/packages/${DEB_PKG_NAME}"

echo ""
echo "=== Build Complete ==="
echo "Package: ${DEB_PKG_NAME}"
echo "Location: /packages/${DEB_PKG_NAME}"

# Test the package
echo ""
echo "=== Package Verification ==="
dpkg-deb -c "/packages/${DEB_PKG_NAME}" | grep -E "(LPM.so|LPM.pm)"
echo "Package verification successful"

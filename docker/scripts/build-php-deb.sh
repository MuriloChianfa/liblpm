#!/bin/bash
# Build PHP extension DEB packages inside Docker container
# Creates php{version}-liblpm packages for Debian/Ubuntu
# This script is executed inside the container

set -euo pipefail

echo "=== Building PHP Extension DEB Packages ==="
echo "Working directory: $(pwd)"
echo "Distribution: $(cat /etc/os-release | grep PRETTY_NAME || echo 'Unknown')"
echo ""

# Detect PHP version available on this system
PHP_VERSION=$(php -r 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION;' 2>/dev/null || echo "unknown")
PHP_API=$(php -i 2>/dev/null | grep 'PHP API' | awk '{print $4}' || echo "unknown")
ARCH=$(dpkg --print-architecture)

echo "Detected PHP version: ${PHP_VERSION}"
echo "PHP API version: ${PHP_API}"
echo "Architecture: ${ARCH}"
echo ""

if [ "$PHP_VERSION" = "unknown" ]; then
    echo "Error: PHP not found or not properly installed"
    exit 1
fi

# Package version from package.xml or default
PKG_VERSION="1.0.0"
PKG_RELEASE="1"

# Package naming convention: php{major.minor}-liblpm
PHP_PKG_NAME="php${PHP_VERSION}-liblpm"
DEB_PKG_NAME="${PHP_PKG_NAME}_${PKG_VERSION}-${PKG_RELEASE}_${ARCH}.deb"

echo "Building package: ${DEB_PKG_NAME}"
echo ""

# Build directory
BUILD_DIR="/tmp/php-liblpm-build"
INSTALL_ROOT="/tmp/php-liblpm-install"

rm -rf "$BUILD_DIR" "$INSTALL_ROOT"
mkdir -p "$BUILD_DIR" "$INSTALL_ROOT"

# Copy PHP extension source
cp -r /workspace/bindings/php/* "$BUILD_DIR/"

cd "$BUILD_DIR"

# Build PHP extension
echo "Running phpize..."
phpize

echo "Configuring..."
./configure --with-liblpm=/usr/local

echo "Building extension..."
make -j$(nproc)

echo "Extension built successfully"
echo ""

# Prepare DEB package structure
PHP_EXT_DIR=$(php-config --extension-dir | sed 's|^/||')
PHP_INI_DIR="etc/php/${PHP_VERSION}/mods-available"

mkdir -p "${INSTALL_ROOT}/${PHP_EXT_DIR}"
mkdir -p "${INSTALL_ROOT}/${PHP_INI_DIR}"
mkdir -p "${INSTALL_ROOT}/DEBIAN"

# Install extension
cp -v modules/liblpm.so "${INSTALL_ROOT}/${PHP_EXT_DIR}/"

# Create INI file
cat > "${INSTALL_ROOT}/${PHP_INI_DIR}/liblpm.ini" <<EOF
; Configuration for liblpm PHP extension
; priority=20
extension=liblpm.so
EOF

# Create control file
cat > "${INSTALL_ROOT}/DEBIAN/control" <<EOF
Package: ${PHP_PKG_NAME}
Version: ${PKG_VERSION}-${PKG_RELEASE}
Section: php
Priority: optional
Architecture: ${ARCH}
Depends: php${PHP_VERSION}-cli, libc6 (>= 2.34), libnuma1
Maintainer: Murilo Chianfa <murilo.chianfa@outlook.com>
Description: High-performance PHP extension for longest prefix match (LPM)
 liblpm is a high-performance PHP extension for longest prefix match (LPM)
 operations, optimized for IP routing table lookups. It provides both IPv4
 and IPv6 support with multiple algorithm implementations.
 .
 Features:
  - IPv4 DIR-24-8: 1-2 memory accesses per lookup
  - IPv4 8-bit stride: Lower memory for sparse tables
  - IPv6 Wide 16-bit: Optimized for common /48 allocations
  - IPv6 8-bit stride: Memory efficient for sparse tables
  - Both object-oriented and procedural APIs
  - Batch lookup operations for high throughput
Homepage: https://github.com/MuriloChianfa/liblpm
EOF

# Create postinst script to enable extension
cat > "${INSTALL_ROOT}/DEBIAN/postinst" <<'EOF'
#!/bin/bash
set -e

if [ "$1" = "configure" ]; then
    # Enable the extension using phpenmod if available
    if command -v phpenmod >/dev/null 2>&1; then
        phpenmod -v ALL liblpm || true
    fi
fi

exit 0
EOF

chmod +x "${INSTALL_ROOT}/DEBIAN/postinst"

# Create prerm script to disable extension
cat > "${INSTALL_ROOT}/DEBIAN/prerm" <<'EOF'
#!/bin/bash
set -e

if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    # Disable the extension using phpdismod if available
    if command -v phpdismod >/dev/null 2>&1; then
        phpdismod -v ALL liblpm || true
    fi
fi

exit 0
EOF

chmod +x "${INSTALL_ROOT}/DEBIAN/prerm"

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
dpkg-deb -c "/packages/${DEB_PKG_NAME}" | grep liblpm.so
echo "Package verification successful"

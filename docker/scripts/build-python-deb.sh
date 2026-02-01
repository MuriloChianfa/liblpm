#!/bin/bash
# Build Python module DEB packages inside Docker container
# Creates python3-liblpm packages for Debian/Ubuntu
# This script is executed inside the container

set -euo pipefail

echo "=== Building Python Module DEB Packages ==="
echo "Working directory: $(pwd)"
echo "Distribution: $(cat /etc/os-release | grep PRETTY_NAME || echo 'Unknown')"
echo ""

# Detect Python version available on this system
PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}' | cut -d'.' -f1-2)
PYTHON_MAJOR=$(python3 -c 'import sys; print(sys.version_info.major)')
PYTHON_MINOR=$(python3 -c 'import sys; print(sys.version_info.minor)')
ARCH=$(dpkg --print-architecture)

echo "Detected Python version: ${PYTHON_VERSION}"
echo "Python major.minor: ${PYTHON_MAJOR}.${PYTHON_MINOR}"
echo "Architecture: ${ARCH}"
echo ""

if [ "$PYTHON_VERSION" = "unknown" ]; then
    echo "Error: Python not found or not properly installed"
    exit 1
fi

# Package version from pyproject.toml or default
PKG_VERSION="2.0.0"
PKG_RELEASE="1"

# Package naming convention: python3-liblpm
PKG_NAME="python3-liblpm"
DEB_PKG_NAME="${PKG_NAME}_${PKG_VERSION}-${PKG_RELEASE}_${ARCH}.deb"

echo "Building package: ${DEB_PKG_NAME}"
echo ""

# Build directory
BUILD_DIR="/tmp/python-liblpm-build"
INSTALL_ROOT="/tmp/python3-liblpm-install"

rm -rf "$BUILD_DIR" "$INSTALL_ROOT"
mkdir -p "$BUILD_DIR" "$INSTALL_ROOT"

# Copy Python module source
cp -r /workspace/bindings/python/* "$BUILD_DIR/"

cd "$BUILD_DIR"

# Build Python extension using pip wheel build
echo "Building Python wheel..."
python3 -m pip wheel --no-deps --wheel-dir=./dist .

echo "Python wheel built successfully"
echo ""

# Extract wheel contents to get the built package
WHEEL_FILE=$(ls dist/*.whl)
echo "Extracting wheel: ${WHEEL_FILE}"
python3 -m pip install --target="${INSTALL_ROOT}/tmp-install" --no-deps "${WHEEL_FILE}"

# Prepare DEB package structure
# Get Python site-packages directory for this version
PYTHON_SITEPACKAGES="usr/lib/python${PYTHON_VERSION}/dist-packages"

mkdir -p "${INSTALL_ROOT}/${PYTHON_SITEPACKAGES}"
mkdir -p "${INSTALL_ROOT}/DEBIAN"

# Move installed package to proper location
echo "Installing to package root..."
mv "${INSTALL_ROOT}/tmp-install/liblpm" "${INSTALL_ROOT}/${PYTHON_SITEPACKAGES}/"
mv "${INSTALL_ROOT}/tmp-install/liblpm-${PKG_VERSION}.dist-info" "${INSTALL_ROOT}/${PYTHON_SITEPACKAGES}/"
rm -rf "${INSTALL_ROOT}/tmp-install"

# List installed files for verification
echo "Installed files:"
find "${INSTALL_ROOT}/${PYTHON_SITEPACKAGES}" -type f | head -20

# Create control file
cat > "${INSTALL_ROOT}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${PKG_VERSION}-${PKG_RELEASE}
Section: python
Priority: optional
Architecture: ${ARCH}
Depends: python3 (>= ${PYTHON_VERSION}), libc6 (>= 2.34), libnuma1
Maintainer: Murilo Chianfa <murilo.chianfa@outlook.com>
Description: High-performance Python bindings for longest prefix match (LPM)
 Python bindings for liblpm providing high-performance longest prefix match
 (LPM) routing table operations for both IPv4 and IPv6.
 .
 Features:
  - High Performance: Direct C bindings via Cython with minimal overhead
  - IPv4 DIR-24-8: Optimized IPv4 lookups using DPDK-style algorithm
  - IPv6 Wide Stride: Efficient IPv6 lookups with 16-bit first-level stride
  - Batch Operations: Reduced overhead through batch lookup operations
  - Pythonic API: Clean interface using ipaddress module types
  - Type Hints: Full mypy support for IDE integration
  - Context Manager: Automatic resource cleanup with with statement
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
dpkg-deb -c "/packages/${DEB_PKG_NAME}" | grep -E "(liblpm/__init__|\.so)" | head -10
echo "Package verification successful"

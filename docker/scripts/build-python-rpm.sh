#!/bin/bash
# Build Python module RPM packages inside Docker container
# Creates python3-liblpm packages for RHEL/Rocky/Fedora
# This script is executed inside the container

set -euo pipefail

# Enable GCC toolset 13 if available (for Rocky Linux 9 / EL9)
if [ -f /opt/rh/gcc-toolset-13/enable ]; then
    source /opt/rh/gcc-toolset-13/enable
fi

echo "=== Building Python Module RPM Packages ==="
echo "Working directory: $(pwd)"
echo "Distribution: $(cat /etc/os-release | grep PRETTY_NAME || echo 'Unknown')"
echo "GCC version: $(gcc --version | head -1)"
echo ""

# Package version
PKG_VERSION="2.0.0"
PKG_RELEASE="1"

# Detect OS for release suffix
if grep -q "Rocky Linux" /etc/os-release; then
    OS_VERSION=$(grep VERSION_ID /etc/os-release | cut -d'"' -f2 | cut -d'.' -f1)
    RELEASE_SUFFIX=".el${OS_VERSION}"
elif grep -q "Fedora" /etc/os-release; then
    OS_VERSION=$(grep VERSION_ID /etc/os-release | cut -d'=' -f2)
    RELEASE_SUFFIX=".fc${OS_VERSION}"
else
    RELEASE_SUFFIX=""
fi

echo "Release suffix: ${RELEASE_SUFFIX}"
echo ""

# Detect Python version
PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}' | cut -d'.' -f1-2)
PYTHON_MAJOR=$(python3 -c 'import sys; print(sys.version_info.major)')
PYTHON_MINOR=$(python3 -c 'import sys; print(sys.version_info.minor)')
ARCH=$(uname -m)

echo "Python version: ${PYTHON_VERSION}"
echo "Python major.minor: ${PYTHON_MAJOR}.${PYTHON_MINOR}"
echo "Architecture: ${ARCH}"
echo ""

# Package naming convention for RPM
RPM_PKG_NAME="python3-liblpm"
RPM_FILENAME="${RPM_PKG_NAME}-${PKG_VERSION}-${PKG_RELEASE}${RELEASE_SUFFIX}.${ARCH}.rpm"

echo "Building package: ${RPM_FILENAME}"
echo ""

# Setup RPM build environment
RPMBUILD_ROOT="${HOME}/rpmbuild"
mkdir -p "${RPMBUILD_ROOT}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Build directory
BUILD_DIR="/tmp/python-liblpm-build"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy Python module source
cp -r /workspace/bindings/python/* "$BUILD_DIR/"

cd "$BUILD_DIR"

# Build Python extension using pip wheel build
echo "Building Python wheel..."
python3 -m pip wheel --no-deps --wheel-dir=./dist .

echo "Python wheel built successfully"
echo ""

# Extract wheel contents
WHEEL_FILE=$(ls dist/*.whl)
echo "Extracting wheel: ${WHEEL_FILE}"
TEMP_INSTALL="/tmp/python-liblpm-temp-install"
rm -rf "${TEMP_INSTALL}"
mkdir -p "${TEMP_INSTALL}"
python3 -m pip install --target="${TEMP_INSTALL}" --no-deps "${WHEEL_FILE}"

# Strip RPATH for compatibility
if command -v chrpath >/dev/null 2>&1; then
    find "${TEMP_INSTALL}" -name "*.so" -exec chrpath --delete {} 2>/dev/null \; || true
fi

# Get Python installation directories
if [ "${ARCH}" = "x86_64" ]; then
    PYTHON_SITEPACKAGES="/usr/lib64/python${PYTHON_VERSION}/site-packages"
else
    PYTHON_SITEPACKAGES="/usr/lib/python${PYTHON_VERSION}/site-packages"
fi

# Create SPEC file for RPM
SPEC_FILE="${RPMBUILD_ROOT}/SPECS/${RPM_PKG_NAME}.spec"

cat > "${SPEC_FILE}" <<'SPECEOF'
Name:           RPM_PKG_NAME_PLACEHOLDER
Version:        PKG_VERSION_PLACEHOLDER
Release:        PKG_RELEASE_PLACEHOLDER
Summary:        High-performance Python bindings for longest prefix match (LPM)

License:        Boost Software License 1.0
URL:            https://github.com/MuriloChianfa/liblpm
BuildArch:      ARCH_PLACEHOLDER

Requires:       python3 >= PYTHON_VERSION_PLACEHOLDER
Requires:       libnuma
BuildRequires:  python3-devel
BuildRequires:  gcc
BuildRequires:  make

%description
Python bindings for liblpm providing high-performance longest prefix match
(LPM) routing table operations for both IPv4 and IPv6.

Features:
- High Performance: Direct C bindings via Cython with minimal overhead
- IPv4 DIR-24-8: Optimized IPv4 lookups using DPDK-style algorithm
- IPv6 Wide Stride: Efficient IPv6 lookups with 16-bit first-level stride
- Batch Operations: Reduced overhead through batch lookup operations
- Pythonic API: Clean interface using ipaddress module types
- Type Hints: Full mypy support for IDE integration
- Context Manager: Automatic resource cleanup with with statement

%prep
# Nothing to prep, we install pre-built files

%build
# Already built

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/PYTHON_SITEPACKAGES_PLACEHOLDER

# Install Python package
cp -r TEMP_INSTALL_PLACEHOLDER/liblpm $RPM_BUILD_ROOT/PYTHON_SITEPACKAGES_PLACEHOLDER/
cp -r TEMP_INSTALL_PLACEHOLDER/liblpm-PKG_VERSION_PLACEHOLDER.dist-info $RPM_BUILD_ROOT/PYTHON_SITEPACKAGES_PLACEHOLDER/

%files
%defattr(-,root,root,-)
PYTHON_SITEPACKAGES_PLACEHOLDER/liblpm/*
PYTHON_SITEPACKAGES_PLACEHOLDER/liblpm-PKG_VERSION_PLACEHOLDER.dist-info/*

%changelog
* CHANGELOG_DATE_PLACEHOLDER Murilo Chianfa <murilo.chianfa@outlook.com> - PKG_VERSION_PLACEHOLDER-PKG_RELEASE_PLACEHOLDER
- Initial RPM package for python3-liblpm

SPECEOF

# Replace placeholders
sed -i "s|RPM_PKG_NAME_PLACEHOLDER|${RPM_PKG_NAME}|g" "${SPEC_FILE}"
sed -i "s|PKG_VERSION_PLACEHOLDER|${PKG_VERSION}|g" "${SPEC_FILE}"
sed -i "s|PKG_RELEASE_PLACEHOLDER|${PKG_RELEASE}${RELEASE_SUFFIX}|g" "${SPEC_FILE}"
sed -i "s|ARCH_PLACEHOLDER|${ARCH}|g" "${SPEC_FILE}"
sed -i "s|PYTHON_VERSION_PLACEHOLDER|${PYTHON_VERSION}|g" "${SPEC_FILE}"
sed -i "s|PYTHON_SITEPACKAGES_PLACEHOLDER|${PYTHON_SITEPACKAGES}|g" "${SPEC_FILE}"
sed -i "s|TEMP_INSTALL_PLACEHOLDER|${TEMP_INSTALL}|g" "${SPEC_FILE}"
sed -i "s|CHANGELOG_DATE_PLACEHOLDER|$(date '+%a %b %d %Y')|g" "${SPEC_FILE}"

# Build RPM
echo "Building RPM package..."
rpmbuild -bb "${SPEC_FILE}"

# Copy to output directory
BUILT_RPM=$(find "${RPMBUILD_ROOT}/RPMS" -name "${RPM_PKG_NAME}-*.rpm" | head -1)
if [ -n "$BUILT_RPM" ] && [ -f "$BUILT_RPM" ]; then
    cp -v "$BUILT_RPM" "/packages/"
    
    echo ""
    echo "=== Package Information ==="
    ls -lh "/packages/$(basename $BUILT_RPM)"
    rpm -qip "/packages/$(basename $BUILT_RPM)" || true
    echo ""
    
    echo "=== Package Contents ==="
    rpm -qlp "/packages/$(basename $BUILT_RPM)" | grep -E "(liblpm/__init__|\.so)" | head -10 || true
else
    echo "Error: RPM package not found after build"
    exit 1
fi

echo ""
echo "=== Build Complete ==="
echo "Package: $(basename $BUILT_RPM)"
echo "Location: /packages/$(basename $BUILT_RPM)"

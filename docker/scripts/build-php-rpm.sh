#!/bin/bash
# Build PHP extension RPM packages inside Docker container
# Creates php{version}-liblpm packages for RHEL/Rocky/Fedora
# This script is executed inside the container

set -euo pipefail

# Enable GCC toolset 13 if available (for Rocky Linux 9 / EL9)
if [ -f /opt/rh/gcc-toolset-13/enable ]; then
    source /opt/rh/gcc-toolset-13/enable
fi

echo "=== Building PHP Extension RPM Packages ==="
echo "Working directory: $(pwd)"
echo "Distribution: $(cat /etc/os-release | grep PRETTY_NAME || echo 'Unknown')"
echo "GCC version: $(gcc --version | head -1)"
echo ""

# Package version
PKG_VERSION="1.0.0"
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

# Setup RPM build environment
RPMBUILD_ROOT="${HOME}/rpmbuild"
mkdir -p "${RPMBUILD_ROOT}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Function to build RPM for a specific PHP version
build_php_rpm() {
    local PHP_BIN=$1
    local PHP_CONFIG=$2
    local PHPIZE=$3
    local PHP_SHORT_VERSION=$4  # e.g., "81", "82", "83"
    
    echo "=== Building for PHP ${PHP_SHORT_VERSION} ==="
    echo "PHP Binary: ${PHP_BIN}"
    
    # Get PHP information
    PHP_VERSION=$(${PHP_BIN} -r 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION;' 2>/dev/null || echo "unknown")
    PHP_API=$(${PHP_BIN} -i 2>/dev/null | grep 'PHP API' | awk '{print $4}' || echo "unknown")
    PHP_ZEND_MODULE_API=$(${PHP_BIN} -i 2>/dev/null | grep 'PHP Extension' | awk '{print $4}' || echo "unknown")
    ARCH=$(uname -m)
    
    echo "PHP version: ${PHP_VERSION}"
    echo "PHP API: ${PHP_API}"
    echo "Zend Module API: ${PHP_ZEND_MODULE_API}"
    echo ""
    
    if [ "$PHP_VERSION" = "unknown" ]; then
        echo "Error: PHP ${PHP_SHORT_VERSION} not found or not properly installed"
        return 1
    fi
    
    # Package naming convention for Rocky/Fedora
    # Rocky: php81-liblpm, php82-liblpm, php83-liblpm
    # Fedora: php-liblpm (default PHP)
    if [ "$PHP_SHORT_VERSION" = "default" ]; then
        RPM_PKG_NAME="php-liblpm"
    else
        RPM_PKG_NAME="php${PHP_SHORT_VERSION}-liblpm"
    fi
    
    RPM_FILENAME="${RPM_PKG_NAME}-${PKG_VERSION}-${PKG_RELEASE}${RELEASE_SUFFIX}.${ARCH}.rpm"
    
    echo "Building package: ${RPM_FILENAME}"
    
    # Build directory
    BUILD_DIR="/tmp/php${PHP_SHORT_VERSION}-liblpm-build"
    
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    
    # Copy PHP extension source
    cp -r /workspace/bindings/php/* "$BUILD_DIR/"
    
    cd "$BUILD_DIR"
    
    # Build PHP extension
    echo "Running phpize..."
    ${PHPIZE}
    
    echo "Configuring..."
    ./configure --with-liblpm=/usr/local --with-php-config=${PHP_CONFIG}
    
    echo "Building extension..."
    make -j$(nproc)
    
    echo "Extension built successfully"
    
    # Strip RPATH for Fedora compatibility
    if command -v chrpath >/dev/null 2>&1; then
        chrpath --delete modules/liblpm.so 2>/dev/null || true
    fi
    
    echo ""
    
    # Get extension directory
    PHP_EXT_DIR=$(${PHP_CONFIG} --extension-dir)
    PHP_INI_DIR="/etc/opt/remi/php${PHP_SHORT_VERSION}"
    
    # For Fedora default PHP
    if [ "$PHP_SHORT_VERSION" = "default" ]; then
        PHP_INI_DIR="/etc/php.d"
    fi
    
    # Create SPEC file for RPM
    SPEC_FILE="${RPMBUILD_ROOT}/SPECS/${RPM_PKG_NAME}.spec"
    
    cat > "${SPEC_FILE}" <<'SPECEOF'
Name:           RPM_PKG_NAME_PLACEHOLDER
Version:        PKG_VERSION_PLACEHOLDER
Release:        PKG_RELEASE_PLACEHOLDER
Summary:        High-performance PHP extension for longest prefix match (LPM)

License:        Boost Software License 1.0
URL:            https://github.com/MuriloChianfa/liblpm
BuildArch:      ARCH_PLACEHOLDER

Requires:       php(api) = PHP_API_PLACEHOLDER
Requires:       php(zend-abi) = PHP_ZEND_MODULE_API_PLACEHOLDER
Requires:       libnuma

%description
liblpm is a high-performance PHP extension for longest prefix match (LPM)
operations, optimized for IP routing table lookups. It provides both IPv4
and IPv6 support with multiple algorithm implementations including IPv4
DIR-24-8 (1-2 memory accesses per lookup), IPv4 8-bit stride (lower memory
for sparse tables), IPv6 Wide 16-bit (optimized for common /48 allocations),
and IPv6 8-bit stride (memory efficient for sparse tables). The extension
provides both object-oriented and procedural APIs with batch lookup operations
for high throughput.

%prep
# Nothing to prep, we install pre-built files

%build
# Already built

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/PHP_EXT_DIR_PLACEHOLDER
mkdir -p $RPM_BUILD_ROOT/PHP_INI_DIR_PLACEHOLDER

# Install extension
install -m 755 BUILD_DIR_PLACEHOLDER/modules/liblpm.so $RPM_BUILD_ROOT/PHP_EXT_DIR_PLACEHOLDER/liblpm.so

# Create INI file
cat > $RPM_BUILD_ROOT/PHP_INI_DIR_PLACEHOLDER/40-liblpm.ini <<INIEOF
; Enable liblpm extension module
extension=liblpm.so
INIEOF

%files
%defattr(-,root,root,-)
/PHP_EXT_DIR_PLACEHOLDER/liblpm.so
%config(noreplace) /PHP_INI_DIR_PLACEHOLDER/40-liblpm.ini

%changelog
* CHANGELOG_DATE_PLACEHOLDER Murilo Chianfa <murilo.chianfa@outlook.com> - PKG_VERSION_PLACEHOLDER-PKG_RELEASE_PLACEHOLDER
- Initial RPM package for RPM_PKG_NAME_PLACEHOLDER

SPECEOF
    
    # Replace placeholders
    sed -i "s|RPM_PKG_NAME_PLACEHOLDER|${RPM_PKG_NAME}|g" "${SPEC_FILE}"
    sed -i "s|PKG_VERSION_PLACEHOLDER|${PKG_VERSION}|g" "${SPEC_FILE}"
    sed -i "s|PKG_RELEASE_PLACEHOLDER|${PKG_RELEASE}${RELEASE_SUFFIX}|g" "${SPEC_FILE}"
    sed -i "s|ARCH_PLACEHOLDER|${ARCH}|g" "${SPEC_FILE}"
    sed -i "s|PHP_API_PLACEHOLDER|${PHP_API}|g" "${SPEC_FILE}"
    sed -i "s|PHP_ZEND_MODULE_API_PLACEHOLDER|${PHP_ZEND_MODULE_API}|g" "${SPEC_FILE}"
    sed -i "s|PHP_EXT_DIR_PLACEHOLDER|${PHP_EXT_DIR}|g" "${SPEC_FILE}"
    sed -i "s|PHP_INI_DIR_PLACEHOLDER|${PHP_INI_DIR}|g" "${SPEC_FILE}"
    sed -i "s|BUILD_DIR_PLACEHOLDER|${BUILD_DIR}|g" "${SPEC_FILE}"
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
    else
        echo "Error: RPM package not found after build"
        return 1
    fi
    
    echo "=== Build Complete for PHP ${PHP_SHORT_VERSION} ==="
    echo ""
}

# Build for available PHP versions
if [ -f /opt/remi/php81/root/usr/bin/php ]; then
    build_php_rpm \
        "/opt/remi/php81/root/usr/bin/php" \
        "/opt/remi/php81/root/usr/bin/php-config" \
        "/opt/remi/php81/root/usr/bin/phpize" \
        "81" || echo "Failed to build for PHP 8.1"
fi

if [ -f /opt/remi/php82/root/usr/bin/php ]; then
    build_php_rpm \
        "/opt/remi/php82/root/usr/bin/php" \
        "/opt/remi/php82/root/usr/bin/php-config" \
        "/opt/remi/php82/root/usr/bin/phpize" \
        "82" || echo "Failed to build for PHP 8.2"
fi

if [ -f /opt/remi/php83/root/usr/bin/php ]; then
    build_php_rpm \
        "/opt/remi/php83/root/usr/bin/php" \
        "/opt/remi/php83/root/usr/bin/php-config" \
        "/opt/remi/php83/root/usr/bin/phpize" \
        "83" || echo "Failed to build for PHP 8.3"
fi

# For Fedora, build with default PHP
if command -v php >/dev/null 2>&1 && ! [ -f /opt/remi/php83/root/usr/bin/php ]; then
    build_php_rpm \
        "php" \
        "php-config" \
        "phpize" \
        "default" || echo "Failed to build for default PHP"
fi

echo ""
echo "=== All RPM Builds Complete ==="
echo "Packages available in /packages:"
ls -lh /packages/*.rpm 2>/dev/null || echo "No packages found"

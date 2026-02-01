#!/bin/bash
# Build Perl module RPM packages inside Docker container
# Creates perl-Net-LPM packages for RHEL/Rocky/Fedora
# This script is executed inside the container

set -euo pipefail

# Enable GCC toolset 13 if available (for Rocky Linux 9 / EL9)
if [ -f /opt/rh/gcc-toolset-13/enable ]; then
    source /opt/rh/gcc-toolset-13/enable
fi

echo "=== Building Perl Module RPM Packages ==="
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

# Detect Perl version
PERL_VERSION=$(perl -V:version | sed "s/version='\(.*\)';/\1/")
PERL_ARCHNAME=$(perl -V:archname | sed "s/archname='\(.*\)';/\1/")
ARCH=$(uname -m)

echo "Perl version: ${PERL_VERSION}"
echo "Perl archname: ${PERL_ARCHNAME}"
echo "Architecture: ${ARCH}"
echo ""

# Package naming convention for RPM
RPM_PKG_NAME="perl-Net-LPM"
RPM_FILENAME="${RPM_PKG_NAME}-${PKG_VERSION}-${PKG_RELEASE}${RELEASE_SUFFIX}.${ARCH}.rpm"

echo "Building package: ${RPM_FILENAME}"
echo ""

# Setup RPM build environment
RPMBUILD_ROOT="${HOME}/rpmbuild"
mkdir -p "${RPMBUILD_ROOT}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Build directory
BUILD_DIR="/tmp/perl-liblpm-build"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy Perl module source
cp -r /workspace/bindings/perl/* "$BUILD_DIR/"

cd "$BUILD_DIR"

# Build Perl XS module
echo "Running perl Makefile.PL..."
perl Makefile.PL INSTALLDIRS=vendor

echo "Building extension..."
make -j$(nproc)

echo "Running tests..."
make test || echo "Warning: Some tests failed"

echo "Extension built successfully"
echo ""

# Strip RPATH for compatibility
if command -v chrpath >/dev/null 2>&1; then
    find blib/arch -name "*.so" -exec chrpath --delete {} 2>/dev/null \; || true
fi

# Get Perl installation directories
PERL_VENDORARCH=$(perl -MConfig -e 'print $Config{vendorarch}')
PERL_VENDORLIB=$(perl -MConfig -e 'print $Config{vendorlib}')
PERL_MAN3DIR=$(perl -MConfig -e 'print $Config{man3dir}' || echo "/usr/share/man/man3")

# Get Perl version for MODULE_COMPAT check
PERL_MODULE_COMPAT=$(perl -MConfig -e 'print $Config{version}')

# Create SPEC file for RPM
SPEC_FILE="${RPMBUILD_ROOT}/SPECS/${RPM_PKG_NAME}.spec"

    cat > "${SPEC_FILE}" <<'SPECEOF'
Name:           ###RPM_PKG_NAME###
Version:        ###PKG_VERSION###
Release:        ###PKG_RELEASE###
Summary:        High-performance Perl bindings for longest prefix match (LPM)

License:        Boost Software License 1.0
URL:            https://github.com/MuriloChianfa/liblpm
BuildArch:      ###ARCH###

Requires:       perl(:MODULE_COMPAT_###PERL_MODULE_COMPAT###)
Requires:       libnuma
BuildRequires:  perl-devel
BuildRequires:  perl-ExtUtils-MakeMaker
BuildRequires:  gcc
BuildRequires:  make

%description
Net::LPM provides high-performance Perl bindings to the liblpm C library
for Longest Prefix Match (LPM) routing table operations.

LPM is a fundamental operation in IP routing where, given an IP address,
you need to find the most specific (longest) matching prefix in a routing
table.

Features:
- High Performance: Direct XS bindings to optimized C library
- Dual Stack: Full support for both IPv4 and IPv6
- Multiple Algorithms: DIR-24-8 for IPv4, Wide 16-bit stride for IPv6
- Batch Operations: Efficient batch lookups to reduce overhead
- SIMD Optimized: Runtime CPU feature detection for optimal performance
- Memory Safe: Automatic cleanup via Perl's reference counting

%prep
# Nothing to prep, we install pre-built files

%build
# Already built

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}###PERL_VENDORARCH###/auto/Net/LPM
mkdir -p ${RPM_BUILD_ROOT}###PERL_VENDORLIB###/Net
mkdir -p ${RPM_BUILD_ROOT}###PERL_MAN3DIR###

# Install extension
install -m 755 ###BUILD_DIR###/blib/arch/auto/Net/LPM/LPM.so ${RPM_BUILD_ROOT}###PERL_VENDORARCH###/auto/Net/LPM/LPM.so

# Install module
install -m 644 ###BUILD_DIR###/blib/lib/Net/LPM.pm ${RPM_BUILD_ROOT}###PERL_VENDORLIB###/Net/LPM.pm

# Install man page
if [ -f ###BUILD_DIR###/blib/man3/Net::LPM.3pm ]; then
    gzip -c ###BUILD_DIR###/blib/man3/Net::LPM.3pm > ${RPM_BUILD_ROOT}###PERL_MAN3DIR###/Net::LPM.3pm.gz
fi

%files
%defattr(-,root,root,-)
###PERL_VENDORARCH###/auto/Net/LPM/LPM.so
###PERL_VENDORLIB###/Net/LPM.pm
###PERL_MAN3DIR###/Net::LPM.3pm.gz

%changelog
* ###CHANGELOG_DATE### Murilo Chianfa <murilo.chianfa@outlook.com> - ###PKG_VERSION###-###PKG_RELEASE###
- Initial RPM package for perl-Net-LPM

SPECEOF

# Replace placeholders using ### delimiters to avoid underscore issues
sed -i "s@###RPM_PKG_NAME###@${RPM_PKG_NAME}@g" "${SPEC_FILE}"
sed -i "s@###PKG_VERSION###@${PKG_VERSION}@g" "${SPEC_FILE}"
sed -i "s@###PKG_RELEASE###@${PKG_RELEASE}${RELEASE_SUFFIX}@g" "${SPEC_FILE}"
sed -i "s@###ARCH###@${ARCH}@g" "${SPEC_FILE}"
sed -i "s@###PERL_MODULE_COMPAT###@${PERL_MODULE_COMPAT}@g" "${SPEC_FILE}"
sed -i "s@###PERL_VENDORARCH###@${PERL_VENDORARCH}@g" "${SPEC_FILE}"
sed -i "s@###PERL_VENDORLIB###@${PERL_VENDORLIB}@g" "${SPEC_FILE}"
sed -i "s@###PERL_MAN3DIR###@${PERL_MAN3DIR}@g" "${SPEC_FILE}"
sed -i "s@###BUILD_DIR###@${BUILD_DIR}@g" "${SPEC_FILE}"
sed -i "s@###CHANGELOG_DATE###@$(date '+%a %b %d %Y')@g" "${SPEC_FILE}"

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
    rpm -qlp "/packages/$(basename $BUILT_RPM)" | grep -E "(LPM.so|LPM.pm)" || true
else
    echo "Error: RPM package not found after build"
    exit 1
fi

echo ""
echo "=== Build Complete ==="
echo "Package: $(basename $BUILT_RPM)"
echo "Location: /packages/$(basename $BUILT_RPM)"

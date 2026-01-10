#!/bin/bash
#
# DPDK Installation Script for liblpm Benchmarking
# This script installs a minimal DPDK build with LPM and LPM6 libraries
#

set -e

# Configuration
DPDK_VERSION="${DPDK_VERSION:-24.11}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-$HOME/dpdk-build}"
JOBS="${JOBS:-$(nproc)}"

echo "====================================="
echo "DPDK Installation Script"
echo "====================================="
echo "Version: $DPDK_VERSION"
echo "Install prefix: $INSTALL_PREFIX"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $JOBS"
echo "====================================="

# Check if running as root for system-wide installation
if [ "$INSTALL_PREFIX" = "/usr/local" ] || [ "$INSTALL_PREFIX" = "/usr" ]; then
    if [ "$EUID" -ne 0 ]; then
        echo "Error: System-wide installation requires root privileges"
        echo "Either run with sudo or set INSTALL_PREFIX to a user directory"
        echo "Example: INSTALL_PREFIX=$HOME/.local $0"
        exit 1
    fi
fi

# Detect distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "Error: Cannot detect Linux distribution"
    exit 1
fi

# Install dependencies based on distribution
echo "Installing dependencies for $DISTRO..."
case "$DISTRO" in
    ubuntu|debian)
        apt-get update
        apt-get install -y \
            build-essential \
            python3 \
            python3-pip \
            python3-pyelftools \
            pkg-config \
            libnuma-dev \
            wget \
            tar \
            meson \
            ninja-build
        ;;
    fedora|rhel|centos|rocky)
        yum install -y \
            gcc \
            make \
            python3 \
            python3-pip \
            python3-pyelftools \
            pkgconfig \
            numactl-devel \
            wget \
            tar \
            meson \
            ninja-build
        ;;
    *)
        echo "Warning: Unsupported distribution. Please install dependencies manually:"
        echo "  - build-essential / gcc make"
        echo "  - python3, python3-pip, python3-pyelftools"
        echo "  - pkg-config, libnuma-dev, meson, ninja-build"
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
        ;;
esac

# Install pyelftools if not available
python3 -m pip install --upgrade pyelftools 2>/dev/null || true

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Download DPDK
DPDK_TARBALL="dpdk-${DPDK_VERSION}.tar.xz"
DPDK_URL="https://fast.dpdk.org/rel/${DPDK_TARBALL}"

if [ ! -f "$DPDK_TARBALL" ]; then
    echo "Downloading DPDK ${DPDK_VERSION}..."
    wget "$DPDK_URL" || {
        echo "Error: Failed to download DPDK from $DPDK_URL"
        echo "Please check the version number or download manually"
        exit 1
    }
else
    echo "Using existing $DPDK_TARBALL"
fi

# Extract DPDK
echo "Extracting DPDK..."
if [ -d "dpdk-${DPDK_VERSION}" ]; then
    echo "Removing existing dpdk-${DPDK_VERSION} directory..."
    rm -rf "dpdk-${DPDK_VERSION}"
fi
tar xf "$DPDK_TARBALL"
cd "dpdk-${DPDK_VERSION}"

# Configure DPDK with minimal build (only LPM libraries)
echo "Configuring DPDK..."
meson setup build \
    --prefix="$INSTALL_PREFIX" \
    -Dexamples="" \
    -Dtests=false \
    -Ddisable_drivers="*" \
    -Denable_drivers="" \
    -Dplatform=generic \
    -Dbuildtype=release \
    -Dc_args="-O3 -march=native" \
    -Denable_kmods=false

# Build DPDK
echo "Building DPDK (this may take a few minutes)..."
ninja -C build -j"$JOBS"

# Install DPDK
echo "Installing DPDK to $INSTALL_PREFIX..."
ninja -C build install

# Update library cache for system-wide installations
if [ "$INSTALL_PREFIX" = "/usr/local" ] || [ "$INSTALL_PREFIX" = "/usr" ]; then
    echo "Updating library cache..."
    ldconfig
fi

# Create pkg-config directory if needed
PKGCONFIG_DIR="$INSTALL_PREFIX/lib/pkgconfig"
if [ ! -d "$PKGCONFIG_DIR" ]; then
    PKGCONFIG_DIR="$INSTALL_PREFIX/lib/x86_64-linux-gnu/pkgconfig"
fi

# Add to PKG_CONFIG_PATH if not system-wide
if [ "$INSTALL_PREFIX" != "/usr/local" ] && [ "$INSTALL_PREFIX" != "/usr" ]; then
    echo ""
    echo "====================================="
    echo "DPDK installed to non-standard location"
    echo "Add these to your environment:"
    echo ""
    echo "export PKG_CONFIG_PATH=\"$PKGCONFIG_DIR:\$PKG_CONFIG_PATH\""
    echo "export LD_LIBRARY_PATH=\"$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH\""
    echo "====================================="
fi

echo ""
echo "====================================="
echo "DPDK Installation Complete!"
echo "====================================="
echo "Version: $(pkg-config --modversion libdpdk 2>/dev/null || echo $DPDK_VERSION)"
echo "Location: $INSTALL_PREFIX"
echo ""
echo "To verify installation:"
echo "  pkg-config --modversion libdpdk"
echo "  pkg-config --libs libdpdk"
echo ""
echo "You can now build the DPDK comparison benchmark:"
echo "  cd $HOME/Github/liblpm"
echo "  rm -rf build && mkdir build && cd build"
echo "  cmake .."
echo "  make bench_dpdk_comparison"
echo "====================================="


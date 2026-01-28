#!/bin/bash
# Build RPM packages inside Docker container
# This script is executed inside the container

set -euo pipefail

# Enable GCC toolset 13 if available (for Rocky Linux 9 / EL9)
if [ -f /opt/rh/gcc-toolset-13/enable ]; then
    source /opt/rh/gcc-toolset-13/enable
fi

echo "=== Building RPM packages ==="
echo "Working directory: $(pwd)"
echo "Distribution: $(cat /etc/os-release | grep PRETTY_NAME || echo 'Unknown')"
echo "GCC version: $(gcc --version | head -1)"
echo ""

# Check for git submodules
if [[ ! -f "/workspace/external/libdynemit/CMakeLists.txt" ]]; then
    echo "Initializing git submodules..."
    cd /workspace
    git config --global --add safe.directory /workspace
    git config --global --add safe.directory /workspace/external/libdynemit
    git submodule update --init --recursive
fi

# Create build directory
BUILD_DIR="/workspace/build-pkg-docker"
PACKAGES_DIR="/packages"

# Clean build directory to avoid CMake cache issues
if [[ -d "$BUILD_DIR" ]]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
mkdir -p "$PACKAGES_DIR"

cd "$BUILD_DIR"

# Configure CMake
echo "Configuring CMake..."
cmake /workspace \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DBUILD_GO_WRAPPER=OFF \
    -DBUILD_CPP_WRAPPER=OFF

# Build the library
echo "Building liblpm..."
cmake --build . --parallel "$(nproc)" --target lpm lpm_static || {
    echo "Warning: Some targets failed to build, but checking if main targets succeeded..."
    # Check for any version of the shared library
    if [[ ! -f liblpm.so.* ]] || [[ ! -f "liblpm.a" ]]; then
        echo "Error: Main library targets failed to build"
        exit 1
    fi
    echo "Main library targets built successfully, continuing..."
}

# Generate RPM packages
echo "Generating RPM packages..."
cpack -G RPM

# Copy packages to output directory
echo "Copying packages to /packages..."
cp -v *.rpm "$PACKAGES_DIR/"

# Show package information
echo ""
echo "=== Package Information ==="
for pkg in "$PACKAGES_DIR"/*.rpm; do
    if [[ -f "$pkg" ]]; then
        echo ""
        echo "Package: $(basename $pkg)"
        echo "Size: $(du -h "$pkg" | cut -f1)"
        rpm -qip "$pkg" 2>/dev/null | grep -E "Name|Version|Architecture|Requires" || true
    fi
done

echo ""
echo "=== Build Complete ==="
echo "Packages are available in the packages/ directory"
ls -lh "$PACKAGES_DIR"/*.rpm 2>/dev/null || echo "No packages found"

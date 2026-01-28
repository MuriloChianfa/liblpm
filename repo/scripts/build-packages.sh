#!/bin/bash
# Build .deb and .rpm packages for liblpm
# Usage: ./build-packages.sh [--deb] [--rpm] [--all]
#
# This script builds packages using CPack. It should be run from the project root
# or specify the source directory via environment variable.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${LIBLPM_SOURCE_DIR:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
BUILD_DIR="${LIBLPM_BUILD_DIR:-${PROJECT_ROOT}/build-pkg}"
OUTPUT_DIR="${LIBLPM_OUTPUT_DIR:-${PROJECT_ROOT}/packages}"

BUILD_DEB=false
BUILD_RPM=false

# Parse arguments
if [[ $# -eq 0 ]]; then
    BUILD_DEB=true
    BUILD_RPM=true
else
    while [[ $# -gt 0 ]]; do
        case $1 in
            --deb)
                BUILD_DEB=true
                shift
                ;;
            --rpm)
                BUILD_RPM=true
                shift
                ;;
            --all)
                BUILD_DEB=true
                BUILD_RPM=true
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [--deb] [--rpm] [--all]"
                echo ""
                echo "Options:"
                echo "  --deb   Build .deb packages only"
                echo "  --rpm   Build .rpm packages only"
                echo "  --all   Build both .deb and .rpm packages (default)"
                echo ""
                echo "Environment variables:"
                echo "  LIBLPM_SOURCE_DIR  Source directory (default: project root)"
                echo "  LIBLPM_BUILD_DIR   Build directory (default: build-pkg)"
                echo "  LIBLPM_OUTPUT_DIR  Output directory for packages (default: packages)"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done
fi

echo "=== liblpm Package Builder ==="
echo "Project root: $PROJECT_ROOT"
echo "Build dir: $BUILD_DIR"
echo "Output dir: $OUTPUT_DIR"
echo ""

# Ensure git submodules are initialized
if [[ ! -f "$PROJECT_ROOT/external/libdynemit/CMakeLists.txt" ]]; then
    echo "Initializing git submodules..."
    git -C "$PROJECT_ROOT" submodule update --init --recursive
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo "Configuring CMake..."
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DBUILD_GO_WRAPPER=OFF \
    -DBUILD_CPP_WRAPPER=OFF

# Build the project
echo "Building liblpm..."
cmake --build . --parallel "$(nproc)"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Build DEB packages
if [[ "$BUILD_DEB" == "true" ]]; then
    echo ""
    echo "Building .deb packages..."
    if command -v dpkg &> /dev/null; then
        cpack -G DEB
        mv -f *.deb "$OUTPUT_DIR/" 2>/dev/null || true
        echo "DEB packages created in $OUTPUT_DIR"
    else
        echo "Warning: dpkg not found, skipping .deb build"
    fi
fi

# Build RPM packages
if [[ "$BUILD_RPM" == "true" ]]; then
    echo ""
    echo "Building .rpm packages..."
    if command -v rpmbuild &> /dev/null; then
        cpack -G RPM
        mv -f *.rpm "$OUTPUT_DIR/" 2>/dev/null || true
        echo "RPM packages created in $OUTPUT_DIR"
    else
        echo "Warning: rpmbuild not found, skipping .rpm build"
    fi
fi

echo ""
echo "=== Build Complete ==="
echo "Packages available in: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR"/*.deb "$OUTPUT_DIR"/*.rpm 2>/dev/null || echo "No packages found"

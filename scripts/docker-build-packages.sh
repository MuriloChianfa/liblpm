#!/bin/bash
# Build DEB and RPM packages using Docker
# This script builds packages for multiple distributions

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PACKAGES_DIR="$PROJECT_ROOT/packages"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
print_message() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Print usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS] [TARGETS]

Build DEB and RPM packages for liblpm using Docker containers.

OPTIONS:
    -h, --help              Show this help message
    -c, --clean             Clean packages directory before building
    -n, --no-cache          Build Docker images without cache
    -v, --verbose           Verbose output

TARGETS:
    all                     Build packages for all distributions (default)
    deb                     Build all DEB packages
    rpm                     Build all RPM packages
    debian-bookworm         Build DEB for Debian Bookworm (12)
    debian-bullseye         Build DEB for Debian Bullseye (11)
    ubuntu-24.04            Build DEB for Ubuntu 24.04 (Noble)
    ubuntu-22.04            Build DEB for Ubuntu 22.04 (Jammy)
    rocky-9                 Build RPM for Rocky Linux 9
    rocky-8                 Build RPM for Rocky Linux 8
    fedora                  Build RPM for Fedora (latest)

EXAMPLES:
    # Build packages for all distributions
    $0 all

    # Build only DEB packages
    $0 deb

    # Build only for Debian Bookworm
    $0 debian-bookworm

    # Clean and rebuild all with verbose output
    $0 --clean --verbose all

    # Build for specific distributions
    $0 debian-bookworm rocky-9 ubuntu-24.04

EOF
}

# Build DEB package for specific distribution
build_deb() {
    local dist_name=$1
    local base_image=$2
    local image_tag="liblpm-deb:${dist_name}"
    
    print_message "$BLUE" "=== Building DEB package for ${dist_name} ==="
    
    # Build Docker image
    print_message "$YELLOW" "Building Docker image: ${image_tag}"
    if [[ "${NO_CACHE}" == "true" ]]; then
        docker build ${VERBOSE_FLAG} -f "$PROJECT_ROOT/docker/Dockerfile.deb" \
            --build-arg DEBIAN_VERSION="${base_image}" \
            -t "${image_tag}" \
            "$PROJECT_ROOT"
    else
        docker build ${VERBOSE_FLAG} -f "$PROJECT_ROOT/docker/Dockerfile.deb" \
            --build-arg DEBIAN_VERSION="${base_image}" \
            -t "${image_tag}" \
            "$PROJECT_ROOT"
    fi
    
    # Create distribution-specific output directory
    local dist_packages_dir="${PACKAGES_DIR}/${dist_name}"
    mkdir -p "${dist_packages_dir}"
    
    # Run container to build packages
    print_message "$YELLOW" "Building packages in container..."
    docker run --rm \
        -v "$PROJECT_ROOT:/workspace:ro" \
        -v "${dist_packages_dir}:/packages" \
        "${image_tag}"
    
    # Check if packages were created
    if ls "${dist_packages_dir}"/*.deb 1> /dev/null 2>&1; then
        print_message "$GREEN" "✓ DEB packages for ${dist_name} built successfully"
        print_message "$GREEN" "  Location: ${dist_packages_dir}"
        ls -lh "${dist_packages_dir}"/*.deb
    else
        print_message "$RED" "✗ Failed to build DEB packages for ${dist_name}"
        return 1
    fi
    
    echo ""
}

# Build RPM package for specific distribution
build_rpm() {
    local dist_name=$1
    local version=$2
    local image_tag="liblpm-rpm:${dist_name}"
    
    print_message "$BLUE" "=== Building RPM package for ${dist_name} ==="
    
    # Build Docker image
    print_message "$YELLOW" "Building Docker image: ${image_tag}"
    if [[ "${NO_CACHE}" == "true" ]]; then
        docker build ${VERBOSE_FLAG} -f "$PROJECT_ROOT/docker/Dockerfile.rpm" \
            --build-arg ROCKYLINUX_VERSION="${version}" \
            -t "${image_tag}" \
            "$PROJECT_ROOT"
    else
        docker build ${VERBOSE_FLAG} -f "$PROJECT_ROOT/docker/Dockerfile.rpm" \
            --build-arg ROCKYLINUX_VERSION="${version}" \
            -t "${image_tag}" \
            "$PROJECT_ROOT"
    fi
    
    # Create distribution-specific output directory
    local dist_packages_dir="${PACKAGES_DIR}/${dist_name}"
    mkdir -p "${dist_packages_dir}"
    
    # Run container to build packages
    print_message "$YELLOW" "Building packages in container..."
    docker run --rm \
        -v "$PROJECT_ROOT:/workspace:ro" \
        -v "${dist_packages_dir}:/packages" \
        "${image_tag}"
    
    # Check if packages were created
    if ls "${dist_packages_dir}"/*.rpm 1> /dev/null 2>&1; then
        print_message "$GREEN" "✓ RPM packages for ${dist_name} built successfully"
        print_message "$GREEN" "  Location: ${dist_packages_dir}"
        ls -lh "${dist_packages_dir}"/*.rpm
    else
        print_message "$RED" "✗ Failed to build RPM packages for ${dist_name}"
        return 1
    fi
    
    echo ""
}

# Parse command line arguments
CLEAN=false
NO_CACHE=false
VERBOSE=false
VERBOSE_FLAG=""
TARGETS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -n|--no-cache)
            NO_CACHE=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            VERBOSE_FLAG="--progress=plain"
            shift
            ;;
        *)
            TARGETS+=("$1")
            shift
            ;;
    esac
done

# Default to 'all' if no targets specified
if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=("all")
fi

# Clean packages directory if requested
if [[ "$CLEAN" == "true" ]]; then
    print_message "$YELLOW" "Cleaning packages directory: ${PACKAGES_DIR}"
    rm -rf "${PACKAGES_DIR}"
    mkdir -p "${PACKAGES_DIR}"
fi

# Create packages directory
mkdir -p "${PACKAGES_DIR}"

# Change to project root
cd "$PROJECT_ROOT"

# Build packages for requested targets
for target in "${TARGETS[@]}"; do
    case "$target" in
        all)
            # Build all DEB packages
            build_deb "debian-bookworm" "bookworm"
            build_deb "debian-bullseye" "bullseye"
            build_deb "ubuntu-24.04" "ubuntu:24.04"
            build_deb "ubuntu-22.04" "ubuntu:22.04"
            
            # Build all RPM packages
            build_rpm "rocky-9" "9"
            build_rpm "rocky-8" "8"
            ;;
        deb)
            build_deb "debian-bookworm" "bookworm"
            build_deb "debian-bullseye" "bullseye"
            build_deb "ubuntu-24.04" "ubuntu:24.04"
            build_deb "ubuntu-22.04" "ubuntu:22.04"
            ;;
        rpm)
            build_rpm "rocky-9" "9"
            build_rpm "rocky-8" "8"
            ;;
        debian-bookworm)
            build_deb "debian-bookworm" "bookworm"
            ;;
        debian-bullseye)
            build_deb "debian-bullseye" "bullseye"
            ;;
        ubuntu-24.04)
            build_deb "ubuntu-24.04" "ubuntu:24.04"
            ;;
        ubuntu-22.04)
            build_deb "ubuntu-22.04" "ubuntu:22.04"
            ;;
        rocky-9)
            build_rpm "rocky-9" "9"
            ;;
        rocky-8)
            build_rpm "rocky-8" "8"
            ;;
        fedora)
            build_rpm "fedora" "latest"
            ;;
        *)
            print_message "$RED" "Unknown target: $target"
            echo "Run '$0 --help' for usage information"
            exit 1
            ;;
    esac
done

# Summary
print_message "$GREEN" "=== Package Build Summary ==="
print_message "$GREEN" "All requested packages have been built successfully!"
print_message "$GREEN" "Packages directory: ${PACKAGES_DIR}"
echo ""
print_message "$BLUE" "Directory structure:"
tree -L 2 "${PACKAGES_DIR}" 2>/dev/null || find "${PACKAGES_DIR}" -type f -name "*.deb" -o -name "*.rpm" | sort

echo ""
print_message "$BLUE" "Next steps:"
echo "  1. Test packages: sudo dpkg -i packages/debian-bookworm/*.deb"
echo "  2. Test packages: sudo rpm -ivh packages/rocky-9/*.rpm"
echo "  3. Deploy to repository (see repo/scripts/deploy-packages.sh)"

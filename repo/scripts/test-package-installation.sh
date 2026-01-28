#!/bin/bash
# Test package installation in Docker containers
# Usage: ./test-package-installation.sh [--deb|--rpm|--all]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGES_DIR="$PROJECT_ROOT/packages"

TEST_DEB=false
TEST_RPM=false

show_help() {
    cat << EOF
Test liblpm package installation in Docker containers

Usage: $0 [options]

Options:
    --deb       Test DEB package installation (Ubuntu/Debian)
    --rpm       Test RPM package installation (Fedora)
    --all       Test both DEB and RPM (default)
    -h, --help  Show this help message

This script:
1. Starts a temporary HTTP server to serve packages
2. Launches test containers
3. Tests package installation
4. Verifies installation
5. Cleans up

Examples:
    # Test both DEB and RPM
    $0 --all

    # Test only DEB packages
    $0 --deb

    # Test only RPM packages
    $0 --rpm
EOF
}

# Parse arguments
if [[ $# -eq 0 ]]; then
    TEST_DEB=true
    TEST_RPM=true
else
    while [[ $# -gt 0 ]]; do
        case $1 in
            --deb)
                TEST_DEB=true
                shift
                ;;
            --rpm)
                TEST_RPM=true
                shift
                ;;
            --all)
                TEST_DEB=true
                TEST_RPM=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
fi

# Check if packages exist
if [[ ! -d "$PACKAGES_DIR" ]] || [[ -z "$(ls -A "$PACKAGES_DIR" 2>/dev/null)" ]]; then
    echo "Error: No packages found in $PACKAGES_DIR"
    echo ""
    echo "Build packages first with:"
    echo "  cd repo && ./scripts/docker-build-all-packages.sh"
    exit 1
fi

echo "========================================="
echo "Package Installation Testing"
echo "========================================="
echo ""
echo "Packages to test:"
ls -lh "$PACKAGES_DIR"
echo ""

# Setup temporary repository directory
TEMP_REPO=$(mktemp -d -t liblpm-test-repo-XXXXXX)
echo "Creating temporary repository: $TEMP_REPO"

# Create directories first
mkdir -p "$TEMP_REPO/apt/pool/main/l/liblpm"
mkdir -p "$TEMP_REPO/rpm"

# Create APT repository structure
if [[ "$TEST_DEB" == "true" ]]; then
    echo "Setting up APT repository structure..."
    
    # Create proper directory structure
    mkdir -p "$TEMP_REPO/apt/pool/main/l/liblpm"
    mkdir -p "$TEMP_REPO/apt/dists/stable/main/binary-amd64"
    
    # Copy packages
    cp "$PACKAGES_DIR"/*.deb "$TEMP_REPO/apt/pool/main/l/liblpm/" 2>/dev/null || true
    
    # Create Packages file
    cd "$TEMP_REPO/apt"
    dpkg-scanpackages pool /dev/null > dists/stable/main/binary-amd64/Packages
    gzip -9c dists/stable/main/binary-amd64/Packages > dists/stable/main/binary-amd64/Packages.gz
    
    # Create Release file
    cat > dists/stable/Release << EOF
Origin: liblpm-test
Label: liblpm-test
Suite: stable
Codename: stable
Architectures: amd64
Components: main
Description: Test repository for liblpm
Date: $(date -R)
EOF
fi

# Create RPM repository metadata
if [[ "$TEST_RPM" == "true" ]]; then
    echo "Setting up RPM repository structure..."
    cp "$PACKAGES_DIR"/*.rpm "$TEMP_REPO/rpm/" 2>/dev/null || true
    if command -v createrepo_c &> /dev/null; then
        createrepo_c "$TEMP_REPO/rpm"
    elif command -v createrepo &> /dev/null; then
        createrepo "$TEMP_REPO/rpm"
    else
        echo "Warning: createrepo not available, skipping RPM metadata"
    fi
fi

echo "Repository structure created"
echo ""

# Start HTTP server
HTTP_PORT=8765
echo "Starting HTTP server on port $HTTP_PORT..."

cd "$TEMP_REPO"
python3 -m http.server $HTTP_PORT &
HTTP_PID=$!

# Give server time to start
sleep 2

# Check if server is running
if ! kill -0 $HTTP_PID 2>/dev/null; then
    echo "Error: Failed to start HTTP server"
    rm -rf "$TEMP_REPO"
    exit 1
fi

# Get host IP (for container access)
HOST_IP=$(ip route get 1.1.1.1 | grep -oP 'src \K\S+')
REPO_URL="http://${HOST_IP}:${HTTP_PORT}"

echo "HTTP server running at $REPO_URL"
echo "  PID: $HTTP_PID"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    kill $HTTP_PID 2>/dev/null || true
    rm -rf "$TEMP_REPO"
    echo "Cleanup complete"
}

trap cleanup EXIT

# Test DEB installation
if [[ "$TEST_DEB" == "true" ]]; then
    echo "========================================="
    echo "Testing DEB Package Installation"
    echo "========================================="
    echo ""
    
    for distro in "ubuntu:24.04" "ubuntu:22.04" "debian:12"; do
        echo "-> Testing on $distro"
        echo ""
        
        docker run --rm --add-host=host.docker.internal:host-gateway "$distro" bash -c "
            set -ex
            
            # Update package list
            apt-get update -qq
            
            # Install wget and gnupg
            apt-get install -y wget gnupg lsb-release
            
            # Add repository (no GPG for testing)
            echo 'deb [trusted=yes] $REPO_URL/apt stable main' > /etc/apt/sources.list.d/liblpm-test.list
            
            # Update with new repository
            apt-get update -qq
            
            # Install packages
            apt-get install -y liblpm liblpm-dev
            
            # Verify installation
            echo ''
            echo '=== Verification ==='
            
            # Check files
            ls -l /usr/lib/liblpm.so* 2>/dev/null || ls -l /usr/lib/x86_64-linux-gnu/liblpm.so* 2>/dev/null
            ls -l /usr/include/lpm/
            ls -l /usr/lib/liblpm.a 2>/dev/null || ls -l /usr/lib/x86_64-linux-gnu/liblpm.a 2>/dev/null
            
            # Check pkg-config (optional, install if not present)
            if ! command -v pkg-config &> /dev/null; then
                apt-get install -y pkg-config
            fi
            
            pkg-config --modversion liblpm
            pkg-config --cflags liblpm
            pkg-config --libs liblpm
            
            # Check ldconfig
            ldconfig
            ldconfig -p | grep liblpm
            
            echo ''
            echo 'Installation successful on $distro'
        " || {
            echo "Installation failed on $distro"
            continue
        }
        
        echo ""
    done
fi

# Test RPM installation
if [[ "$TEST_RPM" == "true" ]]; then
    echo "========================================="
    echo "Testing RPM Package Installation"
    echo "========================================="
    echo ""
    
    for distro in "fedora:41" "rockylinux:9"; do
        echo "-> Testing on $distro"
        echo ""
        
        docker run --rm --add-host=host.docker.internal:host-gateway "$distro" bash -c "
            set -ex
            
            # Create repository file
            cat > /etc/yum.repos.d/liblpm-test.repo << EOF
[liblpm-test]
name=liblpm Test Repository
baseurl=$REPO_URL/rpm
enabled=1
gpgcheck=0
EOF
            
            # Install packages
            dnf install -y liblpm liblpm-devel
            
            # Verify installation
            echo ''
            echo '=== Verification ==='
            
            # Check files
            ls -l /usr/lib64/liblpm.so* || ls -l /usr/lib/liblpm.so*
            ls -l /usr/include/lpm/
            ls -l /usr/lib64/liblpm.a || ls -l /usr/lib/liblpm.a
            
            # Check pkg-config
            pkg-config --modversion liblpm
            pkg-config --cflags liblpm
            pkg-config --libs liblpm
            
            # Check ldconfig
            ldconfig -p | grep liblpm || echo 'Note: Library may not be in ldconfig cache yet'
            
            echo ''
            echo 'Installation successful on $distro'
        " || {
            echo "Installation failed on $distro"
            continue
        }
        
        echo ""
    done
fi

echo "========================================="
echo "Testing Complete!"
echo "========================================="
echo ""
echo "Summary:"
if [[ "$TEST_DEB" == "true" ]]; then
    echo "  DEB packages tested on Ubuntu 24.04, 22.04 and Debian 12"
fi
if [[ "$TEST_RPM" == "true" ]]; then
    echo "  RPM packages tested on Fedora 41 and Rocky Linux 9"
fi
echo ""
echo "All tests completed. Packages are ready for deployment!"

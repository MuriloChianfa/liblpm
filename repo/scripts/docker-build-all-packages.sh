#!/bin/bash
# Convenience script to build all or specific distribution packages using Docker
# Usage: ./repo/scripts/docker-build-all-packages.sh [options]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_DIR="$PROJECT_ROOT/repo"

cd "$REPO_DIR"

show_help() {
    cat << EOF
Build liblpm packages for all supported distributions using Docker

Usage: $0 [options]

Options:
    --deb-only          Build only DEB packages (Ubuntu & Debian)
    --rpm-only          Build only RPM packages (Rocky Linux & Fedora)
    --clean             Clean packages directory before building
    -h, --help          Show this help message

Examples:
    # Build all packages (DEB and RPM)
    $0

    # Build only DEB packages
    $0 --deb-only

    # Clean build all packages
    $0 --clean
EOF
}

DEB_ONLY=false
RPM_ONLY=false
CLEAN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deb-only)
            DEB_ONLY=true
            shift
            ;;
        --rpm-only)
            RPM_ONLY=false
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Clean packages if requested
if [[ "$CLEAN" == "true" ]]; then
    echo "Cleaning packages directory..."
    rm -rf "$PROJECT_ROOT/packages"
fi

# Create packages directory
mkdir -p "$PROJECT_ROOT/packages"

echo "========================================="
echo "Building liblpm packages using Docker"
echo "========================================="
echo ""

# Build DEB packages
if [[ "$DEB_ONLY" == "true" ]] || [[ "$RPM_ONLY" == "false" ]]; then
    echo "Building DEB packages..."
    echo ""
    
    echo "→ Ubuntu 24.04 (Noble)"
    docker compose -f docker-compose.packages.yml run --rm ubuntu-noble
    echo ""
fi

# Build RPM packages
if [[ "$RPM_ONLY" == "true" ]] || [[ "$DEB_ONLY" == "false" ]]; then
    echo "Building RPM packages..."
    echo ""
    
    echo "→ Fedora 41"
    docker compose -f docker-compose.packages.yml run --rm fedora-41
    echo ""
fi

echo "========================================="
echo "Build Complete!"
echo "========================================="
echo ""
echo "Generated packages:"
ls -lh "$PROJECT_ROOT/packages/"

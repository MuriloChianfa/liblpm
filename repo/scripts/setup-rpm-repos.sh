#!/bin/bash
# Set up RPM repositories using createrepo_c
# Usage: ./setup-rpm-repos.sh <repo-base-dir> [--import-packages <dir>] [--family <el|fedora>] [--version <ver>]
#
# This script initializes RPM repository structures for EL and Fedora.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    echo "Usage: $0 <repo-base-dir> [options]"
    echo ""
    echo "Arguments:"
    echo "  repo-base-dir       Base directory for the RPM repositories"
    echo ""
    echo "Options:"
    echo "  --import <dir>      Import .rpm packages from directory"
    echo "  --family <name>     Family to import into: el or fedora"
    echo "  --version <ver>     Version to import into (e.g., 8, 9 for EL; 40, 41 for Fedora)"
    echo "  --arch <arch>       Architecture (default: x86_64)"
    echo "  --init-all          Initialize all supported repo directories"
    echo "  -h, --help          Show this help message"
    echo ""
    echo "Supported configurations:"
    echo "  EL (Rocky/RHEL/AlmaLinux): 8, 9"
    echo "  Fedora: 40, 41, 42"
    exit 1
}

if [[ $# -lt 1 ]]; then
    usage
fi

REPO_BASE="$1"
shift

IMPORT_DIR=""
FAMILY=""
VERSION=""
ARCH="x86_64"
INIT_ALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --import)
            IMPORT_DIR="$2"
            shift 2
            ;;
        --family)
            FAMILY="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --init-all)
            INIT_ALL=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

echo "=== RPM Repository Setup ==="
echo "Repository base: $REPO_BASE"

# Check for createrepo_c
if ! command -v createrepo_c &> /dev/null; then
    echo "Error: createrepo_c is not installed"
    echo "Install with:"
    echo "  Debian/Ubuntu: sudo apt install createrepo-c"
    echo "  Fedora/RHEL: sudo dnf install createrepo_c"
    exit 1
fi

RPM_DIR="$REPO_BASE/rpm"

# Initialize repository structure
init_repo() {
    local family=$1
    local ver=$2
    local arch=$3
    local repo_path="$RPM_DIR/$family/$ver/$arch"
    
    echo "Initializing: $repo_path"
    mkdir -p "$repo_path"
    createrepo_c "$repo_path"
}

# Initialize all repositories if requested
if [[ "$INIT_ALL" == "true" ]]; then
    echo "Initializing all repository directories..."
    
    # EL repositories
    for ver in 8 9; do
        init_repo "el" "$ver" "$ARCH"
    done
    
    # Fedora repositories
    for ver in 40 41 42; do
        init_repo "fedora" "$ver" "$ARCH"
    done
fi

# Import packages if specified
if [[ -n "$IMPORT_DIR" ]]; then
    if [[ -z "$FAMILY" ]] || [[ -z "$VERSION" ]]; then
        echo "Error: --family and --version are required when importing packages"
        exit 1
    fi
    
    REPO_PATH="$RPM_DIR/$FAMILY/$VERSION/$ARCH"
    mkdir -p "$REPO_PATH"
    
    echo "Importing packages from $IMPORT_DIR into $REPO_PATH..."
    for rpm in "$IMPORT_DIR"/*.rpm; do
        if [[ -f "$rpm" ]]; then
            echo "  Copying: $(basename "$rpm")"
            cp "$rpm" "$REPO_PATH/"
        fi
    done
    
    echo "Updating repository metadata..."
    createrepo_c --update "$REPO_PATH"
fi

# Copy .repo files
echo "Copying .repo files..."
cp "$SCRIPT_DIR/../rpm/liblpm-el.repo" "$RPM_DIR/" 2>/dev/null || true
cp "$SCRIPT_DIR/../rpm/liblpm-fedora.repo" "$RPM_DIR/" 2>/dev/null || true

echo ""
echo "=== Repository Setup Complete ==="
echo ""
echo "Repository structure created at: $RPM_DIR"
echo ""
echo "To add packages to a repository:"
echo "  cp /path/to/*.rpm $RPM_DIR/<family>/<version>/$ARCH/"
echo "  createrepo_c --update $RPM_DIR/<family>/<version>/$ARCH/"
echo ""
echo "Repository layout:"
echo "  $RPM_DIR/"
echo "    el/"
echo "      8/$ARCH/"
echo "      9/$ARCH/"
echo "    fedora/"
echo "      40/$ARCH/"
echo "      41/$ARCH/"
echo "      42/$ARCH/"
echo ""

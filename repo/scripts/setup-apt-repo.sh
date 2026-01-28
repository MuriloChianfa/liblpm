#!/bin/bash
# Set up APT repository using reprepro
# Usage: ./setup-apt-repo.sh <repo-base-dir> [--import-packages <dir>]
#
# This script initializes an APT repository structure and optionally imports packages.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONF_DIR="$SCRIPT_DIR/../apt/conf"

usage() {
    echo "Usage: $0 <repo-base-dir> [options]"
    echo ""
    echo "Arguments:"
    echo "  repo-base-dir    Base directory for the APT repository"
    echo ""
    echo "Options:"
    echo "  --import <dir>   Import .deb packages from directory"
    echo "  --suite <name>   Suite to import packages into (e.g., jammy, noble, bookworm)"
    echo "  --gpg-key <id>   GPG key ID for signing (or 'default' for default key)"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "Supported suites: jammy, noble, bullseye, bookworm, trixie"
    exit 1
}

if [[ $# -lt 1 ]]; then
    usage
fi

REPO_BASE="$1"
shift

IMPORT_DIR=""
SUITE=""
GPG_KEY=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --import)
            IMPORT_DIR="$2"
            shift 2
            ;;
        --suite)
            SUITE="$2"
            shift 2
            ;;
        --gpg-key)
            GPG_KEY="$2"
            shift 2
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

echo "=== APT Repository Setup ==="
echo "Repository base: $REPO_BASE"

# Create repository structure
APT_DIR="$REPO_BASE/apt"
mkdir -p "$APT_DIR/conf"

# Copy configuration files
echo "Copying configuration files..."
cp "$CONF_DIR/distributions" "$APT_DIR/conf/"
cp "$CONF_DIR/options" "$APT_DIR/conf/"

# Update GPG key in distributions if specified
if [[ -n "$GPG_KEY" ]]; then
    echo "Setting GPG key to: $GPG_KEY"
    sed -i "s/SignWith: default/SignWith: $GPG_KEY/g" "$APT_DIR/conf/distributions"
fi

# Check for reprepro
if ! command -v reprepro &> /dev/null; then
    echo "Error: reprepro is not installed"
    echo "Install with: sudo apt install reprepro"
    exit 1
fi

# Initialize repository (creates db directory)
echo "Initializing repository..."
reprepro -b "$APT_DIR" export

# Import packages if specified
if [[ -n "$IMPORT_DIR" ]]; then
    if [[ -z "$SUITE" ]]; then
        echo "Error: --suite is required when importing packages"
        exit 1
    fi
    
    echo "Importing packages from $IMPORT_DIR into suite $SUITE..."
    for deb in "$IMPORT_DIR"/*.deb; do
        if [[ -f "$deb" ]]; then
            echo "  Importing: $(basename "$deb")"
            reprepro -b "$APT_DIR" includedeb "$SUITE" "$deb"
        fi
    done
fi

# Export public GPG key
if gpg --list-keys &> /dev/null; then
    echo "Exporting GPG public key..."
    gpg --armor --export > "$APT_DIR/gpg.key" 2>/dev/null || true
fi

echo ""
echo "=== Repository Setup Complete ==="
echo ""
echo "Repository structure created at: $APT_DIR"
echo ""
echo "To add packages to a suite:"
echo "  reprepro -b $APT_DIR includedeb <suite> /path/to/package.deb"
echo ""
echo "Available suites: jammy, noble, bullseye, bookworm, trixie"
echo ""

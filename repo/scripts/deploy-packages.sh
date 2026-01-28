#!/bin/bash
# Deploy Docker-built packages to archive.example.com.br
# Usage: ./deploy-packages.sh [options]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Default configuration
REPO_SERVER="${REPO_SERVER:-user@archive.example.com.br}"
REPO_BASE="${REPO_BASE:-/var/www/archive.example.com.br}"
LOCAL_PACKAGES="$PROJECT_ROOT/packages"

# Supported distributions
APT_SUITES="noble jammy bookworm"
RPM_FEDORA_VERSIONS="41 40"
RPM_EL_VERSIONS="9 8"

show_help() {
    cat << EOF
Deploy liblpm packages to archive repository

Usage: $0 [options]

Options:
    --server <host>     Repository server (default: $REPO_SERVER)
    --repo-base <path>  Repository base path (default: $REPO_BASE)
    --apt-only          Deploy only DEB packages
    --rpm-only          Deploy only RPM packages
    --dry-run           Show what would be done without deploying
    -h, --help          Show this help message

Environment Variables:
    REPO_SERVER         Repository server hostname/user
    REPO_BASE           Repository base directory on server

Examples:
    # Deploy all packages
    $0

    # Deploy to specific server
    $0 --server admin@archive.example.com

    # Deploy only DEB packages
    $0 --apt-only

    # Dry run (test without deploying)
    $0 --dry-run
EOF
}

APT_ONLY=false
RPM_ONLY=false
DRY_RUN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --server)
            REPO_SERVER="$2"
            shift 2
            ;;
        --repo-base)
            REPO_BASE="$2"
            shift 2
            ;;
        --apt-only)
            APT_ONLY=true
            shift
            ;;
        --rpm-only)
            RPM_ONLY=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
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

# Check if packages exist
if [[ ! -d "$LOCAL_PACKAGES" ]] || [[ -z "$(ls -A "$LOCAL_PACKAGES" 2>/dev/null)" ]]; then
    echo "Error: No packages found in $LOCAL_PACKAGES"
    echo ""
    echo "Build packages first with:"
    echo "  cd repo && ./scripts/docker-build-all-packages.sh"
    exit 1
fi

echo "========================================="
echo "liblpm Package Deployment"
echo "========================================="
echo ""
echo "Server: $REPO_SERVER"
echo "Repository base: $REPO_BASE"
echo "Local packages: $LOCAL_PACKAGES"
echo ""

# List packages
echo "Packages to deploy:"
ls -lh "$LOCAL_PACKAGES"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    echo "DRY RUN MODE - No changes will be made"
    echo ""
fi

# Confirm deployment
if [[ "$DRY_RUN" == "false" ]]; then
    read -p "Continue with deployment? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Deployment cancelled"
        exit 0
    fi
fi

# Upload packages to server
UPLOAD_DIR="/tmp/liblpm-upload-$$"

if [[ "$DRY_RUN" == "false" ]]; then
    echo "Uploading packages to $REPO_SERVER..."
    rsync -avz "$LOCAL_PACKAGES/" "$REPO_SERVER:$UPLOAD_DIR/"
    echo "Upload complete"
    echo ""
else
    echo "[DRY RUN] Would upload packages with: rsync -avz $LOCAL_PACKAGES/ $REPO_SERVER:$UPLOAD_DIR/"
    echo ""
fi

# Deploy packages on server
echo "Deploying packages on server..."

if [[ "$DRY_RUN" == "false" ]]; then
    ssh "$REPO_SERVER" bash << EOF
set -euo pipefail

REPO_BASE="$REPO_BASE"
UPLOAD_DIR="$UPLOAD_DIR"

echo "=== Server-side Deployment ==="
echo ""

# Deploy DEB packages
if [[ "$APT_ONLY" == "true" ]] || [[ "$RPM_ONLY" == "false" ]]; then
    echo "Deploying DEB packages..."
    
    # Check for reprepro
    if ! command -v reprepro &> /dev/null; then
        echo "Error: reprepro not installed on server"
        echo "Install with: sudo apt install reprepro"
        exit 1
    fi
    
    # Import to each suite
    for suite in $APT_SUITES; do
        echo "  -> Importing to suite: \$suite"
        reprepro -b "\$REPO_BASE/apt" includedeb "\$suite" "\$UPLOAD_DIR"/*.deb 2>&1 | grep -v "already registered" || true
    done
    
    echo "DEB packages deployed"
    echo ""
fi

# Deploy RPM packages
if [[ "$RPM_ONLY" == "true" ]] || [[ "$APT_ONLY" == "false" ]]; then
    echo "Deploying RPM packages..."
    
    # Check for createrepo_c
    if ! command -v createrepo_c &> /dev/null; then
        echo "Error: createrepo_c not installed on server"
        echo "Install with: sudo dnf install createrepo_c (or: sudo apt install createrepo-c)"
        exit 1
    fi
    
    # Deploy Fedora packages
    for version in $RPM_FEDORA_VERSIONS; do
        REPO_DIR="\$REPO_BASE/rpm/fedora/\$version/x86_64"
        if [[ -d "\$REPO_DIR" ]]; then
            echo "  -> Importing to Fedora \$version"
            cp "\$UPLOAD_DIR"/*.rpm "\$REPO_DIR/" 2>/dev/null || true
            createrepo_c --update "\$REPO_DIR"
        else
            echo "  Skipping Fedora \$version (directory not found)"
        fi
    done
    
    # Deploy EL packages
    for version in $RPM_EL_VERSIONS; do
        REPO_DIR="\$REPO_BASE/rpm/el/\$version/x86_64"
        if [[ -d "\$REPO_DIR" ]]; then
            echo "  -> Importing to EL \$version"
            cp "\$UPLOAD_DIR"/*.rpm "\$REPO_DIR/" 2>/dev/null || true
            createrepo_c --update "\$REPO_DIR"
        else
            echo "  Skipping EL \$version (directory not found)"
        fi
    done
    
    echo "RPM packages deployed"
    echo ""
fi

# Cleanup
echo "Cleaning up temporary files..."
rm -rf "\$UPLOAD_DIR"
echo "Cleanup complete"
EOF
else
    echo "[DRY RUN] Would execute deployment commands on $REPO_SERVER"
    echo ""
    echo "DEB deployment:"
    for suite in $APT_SUITES; do
        echo "  - reprepro -b $REPO_BASE/apt includedeb $suite $UPLOAD_DIR/*.deb"
    done
    echo ""
    echo "RPM deployment:"
    for version in $RPM_FEDORA_VERSIONS; do
        echo "  - cp $UPLOAD_DIR/*.rpm $REPO_BASE/rpm/fedora/$version/x86_64/"
        echo "  - createrepo_c --update $REPO_BASE/rpm/fedora/$version/x86_64/"
    done
    for version in $RPM_EL_VERSIONS; do
        echo "  - cp $UPLOAD_DIR/*.rpm $REPO_BASE/rpm/el/$version/x86_64/"
        echo "  - createrepo_c --update $REPO_BASE/rpm/el/$version/x86_64/"
    done
    echo ""
fi

echo ""
echo "========================================="
echo "Deployment Complete!"
echo "========================================="
echo ""
echo "Repository URLs:"
echo "  APT: https://archive.example.com.br/apt/"
echo "  RPM: https://archive.example.com.br/rpm/"
echo ""
echo "User installation:"
echo ""
echo "  Debian/Ubuntu:"
echo "    curl -fsSL https://archive.example.com.br/apt/gpg.key | sudo gpg --dearmor -o /usr/share/keyrings/liblpm.gpg"
echo "    echo 'deb [signed-by=/usr/share/keyrings/liblpm.gpg] https://archive.example.com.br/apt stable main' | sudo tee /etc/apt/sources.list.d/liblpm.list"
echo "    sudo apt update && sudo apt install liblpm liblpm-dev"
echo ""
echo "  Fedora:"
echo "    sudo dnf config-manager --add-repo https://archive.example.com.br/rpm/liblpm-fedora.repo"
echo "    sudo dnf install liblpm liblpm-devel"
echo ""
echo "  RHEL/Rocky/AlmaLinux:"
echo "    sudo dnf config-manager --add-repo https://archive.example.com.br/rpm/liblpm-el.repo"
echo "    sudo dnf install liblpm liblpm-devel"
echo ""

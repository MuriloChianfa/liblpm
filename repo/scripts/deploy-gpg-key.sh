#!/bin/bash
# Deploy GPG public key to archive repository
# Usage: ./deploy-gpg-key.sh [options]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default configuration
REPO_SERVER="${REPO_SERVER:-user@archive.example.com.br}"
REPO_BASE="${REPO_BASE:-/var/www/archive.example.com.br}"
GPG_EMAIL="${GPG_EMAIL:-murilo.chianfa@outlook.com}"

show_help() {
    cat << EOF
Deploy GPG public key to package repository

Usage: $0 [options]

Options:
    --server <host>     Repository server (default: $REPO_SERVER)
    --repo-base <path>  Repository base path (default: $REPO_BASE)
    --email <email>     GPG key email (default: $GPG_EMAIL)
    --key-id <id>       GPG key ID (alternative to email)
    -h, --help          Show this help message

Environment Variables:
    REPO_SERVER         Repository server hostname/user
    REPO_BASE           Repository base directory on server
    GPG_EMAIL           GPG key email address

Examples:
    # Deploy using email
    $0

    # Deploy specific key ID
    $0 --key-id 3E1A1F401A1C47BC77D1705612D0D82387FC53B0

    # Deploy to specific server
    $0 --server admin@archive.example.com
EOF
}

GPG_KEY_ID=""

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
        --email)
            GPG_EMAIL="$2"
            shift 2
            ;;
        --key-id)
            GPG_KEY_ID="$2"
            shift 2
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

echo "========================================="
echo "GPG Key Deployment"
echo "========================================="
echo ""
echo "Server: $REPO_SERVER"
echo "Repository base: $REPO_BASE"

# Determine which key to export
if [[ -n "$GPG_KEY_ID" ]]; then
    KEY_IDENTIFIER="$GPG_KEY_ID"
    echo "GPG Key ID: $GPG_KEY_ID"
else
    KEY_IDENTIFIER="$GPG_EMAIL"
    echo "GPG Email: $GPG_EMAIL"
fi
echo ""

# Check if key exists
if ! gpg --list-keys "$KEY_IDENTIFIER" &> /dev/null; then
    echo "Error: GPG key not found: $KEY_IDENTIFIER"
    echo ""
    echo "Available keys:"
    gpg --list-keys
    echo ""
    echo "Generate a new key with: gpg --full-generate-key"
    exit 1
fi

# Show key information
echo "Key information:"
gpg --list-keys "$KEY_IDENTIFIER" | grep -E "^pub|^uid" | head -2
echo ""

# Export public key to temporary file
TEMP_KEY="/tmp/liblpm-gpg-key-$$.asc"
echo "Exporting GPG public key..."
gpg --armor --export "$KEY_IDENTIFIER" > "$TEMP_KEY"

if [[ ! -s "$TEMP_KEY" ]]; then
    echo "Error: Failed to export GPG key"
    rm -f "$TEMP_KEY"
    exit 1
fi

echo "Key exported ($(wc -c < "$TEMP_KEY") bytes)"
echo ""

# Confirm deployment
read -p "Deploy this GPG key to $REPO_SERVER? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Deployment cancelled"
    rm -f "$TEMP_KEY"
    exit 0
fi

# Deploy to APT repository
echo "Deploying to APT repository..."
if scp "$TEMP_KEY" "$REPO_SERVER:$REPO_BASE/apt/gpg.key"; then
    echo "Deployed to APT repository"
else
    echo "Failed to deploy to APT repository"
    echo "  Ensure $REPO_BASE/apt/ exists on server"
fi
echo ""

# Deploy to RPM repository
echo "Deploying to RPM repository..."
if scp "$TEMP_KEY" "$REPO_SERVER:$REPO_BASE/rpm/RPM-GPG-KEY-liblpm"; then
    echo "Deployed to RPM repository"
else
    echo "Failed to deploy to RPM repository"
    echo "  Ensure $REPO_BASE/rpm/ exists on server"
fi
echo ""

# Cleanup
rm -f "$TEMP_KEY"

echo "========================================="
echo "Deployment Complete!"
echo "========================================="
echo ""
echo "Public key URLs:"
echo "  APT: https://archive.example.com.br/apt/gpg.key"
echo "  RPM: https://archive.example.com.br/rpm/RPM-GPG-KEY-liblpm"
echo ""
echo "Next steps:"
echo "1. Configure APT signing (edit apt/conf/distributions):"
echo "   SignWith: $(gpg --list-keys --keyid-format LONG "$KEY_IDENTIFIER" | grep pub | awk '{print $2}' | cut -d'/' -f2)"
echo ""
echo "2. Configure RPM signing (add to ~/.rpmmacros):"
echo "   %_gpg_name $KEY_IDENTIFIER"
echo ""
echo "3. Users can now verify packages using your GPG key"
echo ""
echo "For detailed setup, see: repo/GPG_SETUP_GUIDE.md"

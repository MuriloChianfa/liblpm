#!/bin/bash
# Show information about built packages
# Usage: ./repo/scripts/show-packages.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGES_DIR="$PROJECT_ROOT/packages"

echo "========================================="
echo "liblpm Package Summary"
echo "========================================="
echo ""

if [[ ! -d "$PACKAGES_DIR" ]] || [[ -z "$(ls -A "$PACKAGES_DIR" 2>/dev/null)" ]]; then
    echo "No packages found in $PACKAGES_DIR"
    echo ""
    echo "Build packages with:"
    echo "  cd repo && ./scripts/docker-build-all-packages.sh"
    exit 0
fi

echo "Package Directory: $PACKAGES_DIR"
echo ""

# Show DEB packages
echo "DEB Packages (Debian/Ubuntu):"
echo "---------------------------------------------"
for pkg in "$PACKAGES_DIR"/*.deb; do
    if [[ -f "$pkg" ]]; then
        filename=$(basename "$pkg")
        size=$(du -h "$pkg" | cut -f1)
        echo "  📦 $filename ($size)"
        dpkg-deb -I "$pkg" 2>/dev/null | grep -E "Package:|Version:|Architecture:|Depends:" | sed 's/^/     /'
        echo ""
    fi
done

if ! ls "$PACKAGES_DIR"/*.deb 1> /dev/null 2>&1; then
    echo "  (none)"
    echo ""
fi

# Show RPM packages
echo "RPM Packages (Fedora/RHEL/Rocky):"
echo "---------------------------------------------"
for pkg in "$PACKAGES_DIR"/*.rpm; do
    if [[ -f "$pkg" ]]; then
        filename=$(basename "$pkg")
        size=$(du -h "$pkg" | cut -f1)
        echo "  📦 $filename ($size)"
        if command -v rpm &> /dev/null; then
            rpm -qip "$pkg" 2>/dev/null | grep -E "Name|Version|Architecture|Requires" | head -4 | sed 's/^/     /'
        else
            echo "     (rpm command not available for detailed info)"
        fi
        echo ""
    fi
done

if ! ls "$PACKAGES_DIR"/*.rpm 1> /dev/null 2>&1; then
    echo "  (none)"
    echo ""
fi

echo "========================================="
echo "Total Packages: $(ls "$PACKAGES_DIR"/*.{deb,rpm} 2>/dev/null | wc -l)"
echo "Total Size: $(du -sh "$PACKAGES_DIR" 2>/dev/null | cut -f1)"
echo "========================================="
echo ""

echo "Installation Examples:"
echo ""
echo "  # Debian/Ubuntu"
echo "  sudo apt install $PACKAGES_DIR/liblpm_2.0.0_amd64.deb"
echo "  sudo apt install $PACKAGES_DIR/liblpm-dev_2.0.0_amd64.deb"
echo ""
echo "  # Fedora/RHEL/Rocky"
echo "  sudo dnf install $PACKAGES_DIR/liblpm-2.0.0.x86_64.rpm"
echo "  sudo dnf install $PACKAGES_DIR/liblpm-devel-2.0.0.x86_64.rpm"
echo ""

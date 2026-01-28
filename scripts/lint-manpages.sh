#!/bin/bash
# lint-manpages.sh - Validate man page formatting
# 
# Usage: ./scripts/lint-manpages.sh [--strict]
#
# Options:
#   --strict    Treat STYLE warnings as errors (default: only real errors fail)
#
# Requires: mandoc (apt install mandoc)

set -e

STRICT=0
if [ "$1" = "--strict" ]; then
    STRICT=1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
MAN_DIR="$PROJECT_ROOT/docs/man"

# Check if mandoc is installed
if ! command -v mandoc &> /dev/null; then
    echo "Error: mandoc is not installed"
    echo "Install with: sudo apt install mandoc"
    exit 1
fi

echo "Linting man pages in $MAN_DIR..."
if [ $STRICT -eq 1 ]; then
    echo "(strict mode: STYLE warnings are errors)"
fi
echo ""

errors=0
warnings=0
checked=0

# Find all man pages (excluding small redirect files <50 bytes)
while IFS= read -r -d '' manpage; do
    filesize=$(stat -c%s "$manpage" 2>/dev/null || stat -f%z "$manpage" 2>/dev/null)
    
    # Skip redirect files (they're typically ~20 bytes with just ".so man3/...")
    if [ "$filesize" -lt 50 ]; then
        continue
    fi
    
    checked=$((checked + 1))
    relpath="${manpage#$PROJECT_ROOT/}"
    
    echo -n "Checking $relpath... "
    
    # Run mandoc lint and capture output
    output=$(mandoc -Tlint "$manpage" 2>&1) || true
    
    if [ -z "$output" ]; then
        echo "OK"
    else
        # Check if output contains only STYLE warnings
        non_style=$(echo "$output" | grep -v "STYLE:" || true)
        style_only=$(echo "$output" | grep "STYLE:" || true)
        
        if [ -n "$non_style" ]; then
            # Has actual errors (not just STYLE)
            echo "ERROR"
            echo "$output" | sed 's/^/  /'
            errors=$((errors + 1))
        elif [ -n "$style_only" ]; then
            # Only STYLE warnings
            if [ $STRICT -eq 1 ]; then
                echo "STYLE ERROR (strict mode)"
                echo "$output" | sed 's/^/  /'
                errors=$((errors + 1))
            else
                echo "OK (style warnings)"
                warnings=$((warnings + 1))
            fi
        fi
    fi
done < <(find "$MAN_DIR" -name '*.3' -print0)

echo ""
echo "=========================================="
echo "Checked:  $checked man pages"
echo "Warnings: $warnings (style only)"
echo "Errors:   $errors"
echo "=========================================="

if [ $errors -gt 0 ]; then
    echo "Man page linting failed!"
    exit 1
fi

echo "All man pages passed linting!"
exit 0

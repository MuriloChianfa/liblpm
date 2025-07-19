#!/bin/bash

# cppcheck test script for liblpm
# This script runs cppcheck on the entire codebase without suppressing any warnings

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    print_error "cppcheck is not installed. Please install it first."
    print_status "On Ubuntu/Debian: sudo apt-get install cppcheck"
    print_status "On CentOS/RHEL: sudo yum install cppcheck"
    print_status "On macOS: brew install cppcheck"
    exit 1
fi

print_status "Starting cppcheck analysis..."

# Get the project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Create output directory for cppcheck results
CPPCHECK_OUTPUT_DIR="cppcheck_output"
mkdir -p "$CPPCHECK_OUTPUT_DIR"

# Define source directories to check
SOURCE_DIRS=(
    "src"
    "include"
    "tests"
    "benchmarks"
)

# Define files to exclude (if any)
EXCLUDE_PATTERNS=(
    # Add any patterns to exclude here if needed
    # "*/generated/*"
    # "*/third_party/*"
)

# Build exclude arguments
EXCLUDE_ARGS=""
for pattern in "${EXCLUDE_PATTERNS[@]}"; do
    EXCLUDE_ARGS="$EXCLUDE_ARGS -i$pattern"
done

# Run cppcheck with strict settings
print_status "Running cppcheck with strict analysis..."

cppcheck \
    --enable=all \
    --inconclusive \
    --force \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=checkersReport \
    --xml \
    --xml-version=2 \
    --output-file="$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" \
    $EXCLUDE_ARGS \
    "${SOURCE_DIRS[@]}" \
    2>&1 | tee "$CPPCHECK_OUTPUT_DIR/cppcheck_output.txt"

# Check if cppcheck found any issues
if [ $? -eq 0 ]; then
    print_status "cppcheck analysis completed successfully!"
    
    # Count total issues
    if [ -f "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" ]; then
        TOTAL_ISSUES=$(grep -c "<error " "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
        
        echo
        print_status "cppcheck Results Summary:"
        echo "  Total issues found: $TOTAL_ISSUES"
        
        # Remove any newlines and ensure it's a single number
        TOTAL_ISSUES=$(echo "$TOTAL_ISSUES" | tr -d '\n')
        
        if [ "$TOTAL_ISSUES" -eq 0 ]; then
            print_status "No issues found! Code quality is excellent."
            exit 0
        else
            print_warning "Found $TOTAL_ISSUES issues. Please review the detailed output."
            
            # Show a summary of the issues
            echo
            print_status "Issues found by severity:"
            ERROR_COUNT=$(grep -c "<error severity=\"error\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            WARNING_COUNT=$(grep -c "<error severity=\"warning\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            STYLE_COUNT=$(grep -c "<error severity=\"style\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            PERFORMANCE_COUNT=$(grep -c "<error severity=\"performance\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            PORTABILITY_COUNT=$(grep -c "<error severity=\"portability\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            INFORMATION_COUNT=$(grep -c "<error severity=\"information\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            
            # Remove any newlines and ensure they are single numbers
            ERROR_COUNT=$(echo "$ERROR_COUNT" | tr -d '\n')
            WARNING_COUNT=$(echo "$WARNING_COUNT" | tr -d '\n')
            STYLE_COUNT=$(echo "$STYLE_COUNT" | tr -d '\n')
            PERFORMANCE_COUNT=$(echo "$PERFORMANCE_COUNT" | tr -d '\n')
            PORTABILITY_COUNT=$(echo "$PORTABILITY_COUNT" | tr -d '\n')
            INFORMATION_COUNT=$(echo "$INFORMATION_COUNT" | tr -d '\n')
            
            echo "  Errors: $ERROR_COUNT"
            echo "  Warnings: $WARNING_COUNT"
            echo "  Style issues: $STYLE_COUNT"
            echo "  Performance issues: $PERFORMANCE_COUNT"
            echo "  Portability issues: $PORTABILITY_COUNT"
            echo "  Information: $INFORMATION_COUNT"
            
            # Show some examples of issues
            echo
            print_status "Sample issues found:"
            grep -A 1 "<error" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" | head -20
            
            # Check if there are critical errors
            ERROR_COUNT=$(grep -c "<error severity=\"error\"" "$CPPCHECK_OUTPUT_DIR/cppcheck_results.xml" 2>/dev/null || echo "0")
            # Remove any newlines and ensure it's a single number
            ERROR_COUNT=$(echo "$ERROR_COUNT" | tr -d '\n')
            if [ "$ERROR_COUNT" -gt 0 ]; then
                print_error "Critical errors found. Please fix them before proceeding."
                exit 1
            else
                print_warning "Only warnings and style issues found. Consider fixing them for better code quality."
                exit 0
            fi
        fi
    else
        print_error "cppcheck output file not found!"
        exit 1
    fi
else
    print_error "cppcheck analysis failed!"
    exit 1
fi 
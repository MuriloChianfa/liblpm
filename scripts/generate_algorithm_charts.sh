#!/bin/bash
#
# Generate algorithm comparison charts including DPDK
#
# This script generates charts comparing different algorithms on the same CPU,
# now including DPDK results.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DATA_DIR="$PROJECT_ROOT/benchmarks/data/algorithm_comparison"
OUTPUT_DIR="$PROJECT_ROOT/docs/images"

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Algorithm Comparison Chart Generator${NC}"
echo -e "${BLUE}  (Including DPDK)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Find all unique CPU/IP/lookup combinations
for dir in "$DATA_DIR"/*; do
    if [ -d "$dir" ]; then
        dirname=$(basename "$dir")
        
        # Count CSV files
        csv_count=$(find "$dir" -name "*.csv" -type f | wc -l)
        
        if [ "$csv_count" -gt 0 ]; then
            echo -e "${GREEN}Generating chart for $dirname ($csv_count algorithms)...${NC}"
            
            # Generate chart
            python3 "$PROJECT_ROOT/scripts/plot_lpm_benchmark.py" \
                "$dir"/*.csv \
                --output "$OUTPUT_DIR/${dirname}.png" \
                --auto-title
            
            echo "  ✓ Saved to: $OUTPUT_DIR/${dirname}.png"
            echo ""
        fi
    fi
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  All algorithm comparison charts generated!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Charts saved to: $OUTPUT_DIR/"
echo ""
echo "Note: DPDK appears as a flat line since bench_comparison tests only 10K prefixes."
echo "      It serves as a reference point for comparison."

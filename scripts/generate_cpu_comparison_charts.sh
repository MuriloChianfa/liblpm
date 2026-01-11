#!/bin/bash
#
# Generate CPU Comparison Charts
#
# This script generates charts comparing the same algorithm across different CPUs
#
# Usage:
#   ./scripts/generate_cpu_comparison_charts.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DATA_DIR="${PROJECT_ROOT}/benchmarks/data/cpu_comparison"
OUTPUT_DIR="${PROJECT_ROOT}/docs/images"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  CPU Comparison Chart Generator${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if data directory exists and has data
if [ ! -d "$DATA_DIR" ] || [ -z "$(ls -A "$DATA_DIR" 2>/dev/null)" ]; then
    echo -e "${YELLOW}Warning: No CPU comparison data found in $DATA_DIR${NC}"
    echo ""
    echo "To collect data from multiple CPUs:"
    echo "  1. Run benchmark on this machine (already done)"
    echo "  2. Upload and run on other machines:"
    echo "     ./scripts/upload_and_benchmark.sh user@server1.com"
    echo "     ./scripts/upload_and_benchmark.sh user@server2.com"
    echo ""
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Find all algorithm/config combinations
chart_count=0

for config_dir in "$DATA_DIR"/*; do
    if [ -d "$config_dir" ]; then
        dir_name=$(basename "$config_dir")
        
        # Count CSV files (CPUs)
        cpu_count=$(ls -1 "$config_dir"/*.csv 2>/dev/null | wc -l)
        
        if [ "$cpu_count" -lt 1 ]; then
            echo -e "${YELLOW}Skipping $dir_name (no data)${NC}"
            continue
        fi
        
        if [ "$cpu_count" -lt 2 ]; then
            echo -e "${YELLOW}Note: $dir_name has only 1 CPU, need at least 2 for comparison${NC}"
            continue
        fi
        
        echo -e "${GREEN}Generating chart for $dir_name ($cpu_count CPUs)...${NC}"
        
        # Parse directory name: {algo}_{ipv4/ipv6}_{single/batch}
        if [[ $dir_name =~ ^(.+)_(ipv[46])_(single|batch)$ ]]; then
            algo="${BASH_REMATCH[1]}"
            ip_version="${BASH_REMATCH[2]}"
            lookup_type="${BASH_REMATCH[3]}"
            
            # Generate friendly title
            case "$algo" in
                "dir24")
                    algo_name="DIR-24-8"
                    ;;
                "4stride8")
                    algo_name="IPv4 8-bit Stride"
                    ;;
                "wide16")
                    algo_name="IPv6 Wide 16-bit"
                    ;;
                "6stride8")
                    algo_name="IPv6 8-bit Stride"
                    ;;
                *)
                    algo_name="$algo"
                    ;;
            esac
            
            ip_upper=$(echo "$ip_version" | tr '[:lower:]' '[:upper:]')
            lookup_cap=$(echo "$lookup_type" | sed 's/.*/\u&/')
            
            title="$algo_name $ip_upper $lookup_cap Lookup - CPU Comparison"
            output_file="$OUTPUT_DIR/${dir_name}_cpu_comparison.png"
            
            # Generate chart
            python3 "$SCRIPT_DIR/plot_lpm_benchmark.py" \
                "$config_dir"/*.csv \
                --title "$title" \
                --output "$output_file"
            
            echo "  ✓ Saved to: $output_file"
            ((chart_count++))
        fi
        
        echo ""
    fi
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Generated $chart_count CPU comparison charts in:"
echo "  $OUTPUT_DIR/"
echo ""

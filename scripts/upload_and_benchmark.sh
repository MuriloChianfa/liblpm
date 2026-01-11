#!/bin/bash
#
# Upload and Run LPM Benchmark on Remote Server
#
# Usage:
#   ./scripts/upload_and_benchmark.sh user@remote-server.com
#
# This script:
#   1. Uploads the static benchmark binary to a remote server
#   2. Runs the benchmark on the remote server
#   3. Downloads the results back to local machine
#   4. Organizes results for CPU comparison
#

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <ssh-target> [options]"
    echo ""
    echo "Examples:"
    echo "  $0 user@server.com"
    echo "  $0 user@server.com -a dir24 -t single"
    echo ""
    echo "SSH target should be in the format: user@hostname"
    exit 1
fi

SSH_TARGET="$1"
shift  # Remove first argument, rest are benchmark options

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BENCHMARK_BIN="${PROJECT_ROOT}/build/benchmarks/bench_algorithm_scaling"
REMOTE_DIR="/tmp/liblpm_benchmark"
LOCAL_DATA_DIR="${PROJECT_ROOT}/benchmarks/data"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Remote LPM Benchmark Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if benchmark binary exists
if [ ! -f "$BENCHMARK_BIN" ]; then
    echo -e "${RED}Error: Benchmark binary not found at $BENCHMARK_BIN${NC}"
    echo "Please build it first:"
    echo "  cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make bench_algorithm_scaling"
    exit 1
fi

echo -e "${YELLOW}Step 1: Uploading benchmark to $SSH_TARGET...${NC}"
ssh "$SSH_TARGET" "mkdir -p $REMOTE_DIR"
scp "$BENCHMARK_BIN" "$SSH_TARGET:$REMOTE_DIR/"

echo ""
echo -e "${YELLOW}Step 2: Running benchmark on remote server...${NC}"
echo "This will take several minutes (5M lookups × 5 trials × 8 configs)..."
echo ""

# Run benchmark with passed options
ssh "$SSH_TARGET" "cd $REMOTE_DIR && chmod +x bench_algorithm_scaling && ./bench_algorithm_scaling -o /tmp/liblpm_results $@"

echo ""
echo -e "${YELLOW}Step 3: Downloading results...${NC}"

# Create temporary directory for download
TMP_DOWNLOAD="${PROJECT_ROOT}/benchmarks/tmp_download_$(date +%s)"
mkdir -p "$TMP_DOWNLOAD"

# Download all results
scp -r "$SSH_TARGET:/tmp/liblpm_results/*" "$TMP_DOWNLOAD/"

echo ""
echo -e "${YELLOW}Step 4: Organizing results for CPU comparison...${NC}"

# Process each directory downloaded
for result_dir in "$TMP_DOWNLOAD"/*; do
    if [ -d "$result_dir" ]; then
        # Extract CPU name and config from directory name
        # Format: {cpu_name}_{ipv4/ipv6}_{single/batch}
        dir_name=$(basename "$result_dir")
        
        # Split by underscores from the end to get ip_version and lookup_type
        # Example: amd_ryzen_9_9950x3d_16_ipv4_single
        if [[ $dir_name =~ (.*)_(ipv[46])_(single|batch)$ ]]; then
            cpu_name="${BASH_REMATCH[1]}"
            ip_version="${BASH_REMATCH[2]}"
            lookup_type="${BASH_REMATCH[3]}"
            
            # For each algorithm CSV in the directory
            for csv_file in "$result_dir"/*.csv; do
                if [ -f "$csv_file" ]; then
                    algo=$(basename "$csv_file" .csv)
                    
                    # Create CPU comparison directory structure
                    # benchmarks/data/cpu_comparison/{algo}_{ip_version}_{lookup_type}/
                    cpu_comp_dir="${LOCAL_DATA_DIR}/cpu_comparison/${algo}_${ip_version}_${lookup_type}"
                    mkdir -p "$cpu_comp_dir"
                    
                    # Copy with CPU name as filename
                    cp "$csv_file" "$cpu_comp_dir/${cpu_name}.csv"
                    
                    echo "  • Copied $algo $ip_version $lookup_type for $cpu_name"
                fi
            done
        fi
    fi
done

# Also keep original results in algorithm_comparison
echo ""
echo "Copying to algorithm_comparison for reference..."
cp -r "$TMP_DOWNLOAD"/* "${LOCAL_DATA_DIR}/algorithm_comparison/"

# Cleanup
rm -rf "$TMP_DOWNLOAD"

# Cleanup remote
ssh "$SSH_TARGET" "rm -rf $REMOTE_DIR /tmp/liblpm_results"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Benchmark Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Results organized in:"
echo "  ${LOCAL_DATA_DIR}/cpu_comparison/"
echo "  ${LOCAL_DATA_DIR}/algorithm_comparison/"
echo ""
echo "To generate CPU comparison charts, run:"
echo "  python scripts/plot_lpm_benchmark.py \\"
echo "    benchmarks/data/cpu_comparison/dir24_ipv4_single/*.csv \\"
echo "    --output docs/images/dir24_ipv4_single_cpu_comparison.png \\"
echo "    --title \"DIR-24 IPv4 Single Lookup - CPU Comparison\""
echo ""

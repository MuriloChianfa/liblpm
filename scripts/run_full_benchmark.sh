#!/bin/bash
#
# Full Benchmark Runner with External LPM Libraries
#
# This script:
#   1. Builds the Docker benchmark image (includes libpatricia, rmindlpm)
#   2. Extracts the static benchmark binary
#   3. Runs benchmarks locally
#   4. Optionally runs on remote server
#   5. Generates comparison charts
#
# Usage:
#   ./scripts/run_full_benchmark.sh [OPTIONS]
#
# Options:
#   -r, --remote USER@HOST  Run on remote server as well
#   -l, --local-only        Run only locally (skip Docker build if binary exists)
#   -d, --debug             Run debug verification first
#   -s, --skip-build        Skip Docker build (use existing binary)
#   -c, --charts            Generate charts after benchmarks
#   -h, --help              Show this help
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DOCKER_IMAGE="liblpm-benchmark:latest"
STATIC_BIN="${PROJECT_ROOT}/build/benchmarks/bench_algorithm_scaling_docker"
DATA_DIR="${PROJECT_ROOT}/benchmarks/data/algorithm_comparison"
REMOTE_TARGET=""
LOCAL_ONLY=false
DEBUG_MODE=false
SKIP_BUILD=false
GENERATE_CHARTS=false

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Full Benchmark Runner with External LPM Libraries (libpatricia, rmindlpm)"
    echo ""
    echo "Options:"
    echo "  -r, --remote USER@HOST  Run on remote server as well"
    echo "  -l, --local-only        Run only locally (skip Docker build if binary exists)"
    echo "  -d, --debug             Run debug verification first"
    echo "  -s, --skip-build        Skip Docker build (use existing binary)"
    echo "  -c, --charts            Generate charts after benchmarks"
    echo "  -h, --help              Show this help"
    echo ""
    echo "Examples:"
    echo "  # Build and run locally"
    echo "  $0"
    echo ""
    echo "  # Run locally and on remote server, then generate charts"
    echo "  $0 -r root@192.168.0.13 -c"
    echo ""
    echo "  # Skip Docker build, just run with existing binary"
    echo "  $0 -s -c"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--remote)
            REMOTE_TARGET="$2"
            shift 2
            ;;
        -l|--local-only)
            LOCAL_ONLY=true
            shift
            ;;
        -d|--debug)
            DEBUG_MODE=true
            shift
            ;;
        -s|--skip-build)
            SKIP_BUILD=true
            shift
            ;;
        -c|--charts)
            GENERATE_CHARTS=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Full LPM Benchmark Suite${NC}"
echo -e "${BLUE}  (with external libraries)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

cd "$PROJECT_ROOT"

# Step 1: Build Docker image and extract binary
if [[ "$SKIP_BUILD" == "false" ]] || [[ ! -f "$STATIC_BIN" ]]; then
    echo -e "${YELLOW}Step 1: Building Docker benchmark image...${NC}"
    echo "This includes: libpatricia, rmindlpm"
    echo ""
    
    docker build -t "$DOCKER_IMAGE" -f docker/Dockerfile.benchmark . 2>&1 | tail -30
    
    echo ""
    echo -e "${YELLOW}Extracting static benchmark binary...${NC}"
    
    # Create a temporary container and copy the binary out
    CONTAINER_ID=$(docker create "$DOCKER_IMAGE")
    mkdir -p "$(dirname "$STATIC_BIN")"
    docker cp "$CONTAINER_ID:/workspace/build/benchmarks/bench_algorithm_scaling" "$STATIC_BIN"
    docker rm "$CONTAINER_ID" > /dev/null
    chmod +x "$STATIC_BIN"
    
    echo -e "${GREEN}Binary extracted to: $STATIC_BIN${NC}"
    echo ""
    
    # Show what's compiled in
    echo "Checking compiled features:"
    "$STATIC_BIN" --help 2>&1 | grep -A 20 "Algorithms:" || true
    echo ""
else
    echo -e "${YELLOW}Step 1: Skipping Docker build (using existing binary)${NC}"
    echo ""
fi

# Check binary exists
if [[ ! -f "$STATIC_BIN" ]]; then
    echo -e "${RED}Error: Benchmark binary not found at $STATIC_BIN${NC}"
    exit 1
fi

# Step 2: Debug verification (optional)
if [[ "$DEBUG_MODE" == "true" ]]; then
    echo -e "${YELLOW}Step 2: Running debug verification...${NC}"
    "$STATIC_BIN" --debug
    echo ""
fi

# Step 3: Run local benchmarks
echo -e "${YELLOW}Step 3: Running local benchmarks...${NC}"
echo "This will take several minutes..."
echo ""

mkdir -p "$DATA_DIR"
"$STATIC_BIN" -o "$DATA_DIR"

echo ""
echo -e "${GREEN}Local benchmarks complete!${NC}"
echo ""

# Step 4: Run remote benchmarks (optional)
if [[ -n "$REMOTE_TARGET" ]]; then
    echo -e "${YELLOW}Step 4: Running benchmarks on remote server: $REMOTE_TARGET${NC}"
    echo ""
    
    REMOTE_DIR="/tmp/liblpm_benchmark"
    REMOTE_OUTPUT="/tmp/liblpm_results"
    
    # Upload binary
    echo "Uploading benchmark binary..."
    ssh "$REMOTE_TARGET" "mkdir -p $REMOTE_DIR"
    scp "$STATIC_BIN" "$REMOTE_TARGET:$REMOTE_DIR/bench_algorithm_scaling"
    
    # Run on remote
    echo "Running benchmarks on remote (this will take several minutes)..."
    ssh "$REMOTE_TARGET" "cd $REMOTE_DIR && chmod +x bench_algorithm_scaling && ./bench_algorithm_scaling -o $REMOTE_OUTPUT"
    
    # Download results
    echo "Downloading results..."
    TMP_DOWNLOAD="${PROJECT_ROOT}/benchmarks/tmp_download_$$"
    mkdir -p "$TMP_DOWNLOAD"
    scp -r "$REMOTE_TARGET:$REMOTE_OUTPUT/*" "$TMP_DOWNLOAD/"
    
    # Organize results
    echo "Organizing results..."
    for result_dir in "$TMP_DOWNLOAD"/*; do
        if [ -d "$result_dir" ]; then
            dir_name=$(basename "$result_dir")
            
            if [[ $dir_name =~ (.*)_(ipv[46])_(single|batch)$ ]]; then
                cpu_name="${BASH_REMATCH[1]}"
                ip_version="${BASH_REMATCH[2]}"
                lookup_type="${BASH_REMATCH[3]}"
                
                for csv_file in "$result_dir"/*.csv; do
                    if [ -f "$csv_file" ]; then
                        algo=$(basename "$csv_file" .csv)
                        
                        cpu_comp_dir="${PROJECT_ROOT}/benchmarks/data/cpu_comparison/${algo}_${ip_version}_${lookup_type}"
                        mkdir -p "$cpu_comp_dir"
                        cp "$csv_file" "$cpu_comp_dir/${cpu_name}.csv"
                        
                        # Also copy to algorithm_comparison
                        mkdir -p "$DATA_DIR/${dir_name}"
                        cp "$csv_file" "$DATA_DIR/${dir_name}/"
                        
                        echo "  • Saved $algo $ip_version $lookup_type for $cpu_name"
                    fi
                done
            fi
        fi
    done
    
    # Cleanup
    rm -rf "$TMP_DOWNLOAD"
    ssh "$REMOTE_TARGET" "rm -rf $REMOTE_DIR $REMOTE_OUTPUT"
    
    echo ""
    echo -e "${GREEN}Remote benchmarks complete!${NC}"
    echo ""
fi

# Step 5: Generate charts (optional)
if [[ "$GENERATE_CHARTS" == "true" ]]; then
    echo -e "${YELLOW}Step 5: Generating comparison charts...${NC}"
    echo ""
    
    OUTPUT_IMG_DIR="${PROJECT_ROOT}/docs/images"
    mkdir -p "$OUTPUT_IMG_DIR"
    
    for dir in "$DATA_DIR"/*; do
        if [ -d "$dir" ]; then
            dirname=$(basename "$dir")
            csv_count=$(find "$dir" -name "*.csv" -type f | wc -l)
            
            if [ "$csv_count" -gt 0 ]; then
                echo "Generating chart for $dirname ($csv_count algorithms)..."
                
                python3 "$PROJECT_ROOT/scripts/plot_lpm_benchmark.py" \
                    "$dir"/*.csv \
                    --output "$OUTPUT_IMG_DIR/${dirname}.png" \
                    --auto-title 2>/dev/null || echo "  Warning: Failed to generate $dirname chart"
            fi
        fi
    done
    
    echo ""
    echo -e "${GREEN}Charts generated in: $OUTPUT_IMG_DIR${NC}"
    echo ""
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  All Benchmarks Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Results saved to:"
echo "  $DATA_DIR"
echo ""
if [[ "$GENERATE_CHARTS" != "true" ]]; then
    echo "To generate charts, run:"
    echo "  $0 -s -c"
    echo ""
fi

#!/bin/bash
#
# Run DPDK Algorithm Scaling Benchmark via Docker
#
# This script builds the DPDK-enabled Docker container and runs the
# algorithm scaling benchmark, saving results to the local benchmarks directory.
#
# Usage:
#   ./scripts/run_dpdk_algorithm_scaling.sh [OPTIONS]
#
# Options:
#   -m, --mode MODE       Benchmark mode: algorithm_scaling, comparison, or both (default: algorithm_scaling)
#   -o, --output DIR      Output directory for results (default: benchmarks/data/algorithm_comparison)
#   -r, --rebuild         Force rebuild of Docker image
#   -h, --help            Show this help
#
# Examples:
#   ./scripts/run_dpdk_algorithm_scaling.sh
#   ./scripts/run_dpdk_algorithm_scaling.sh --mode both
#   ./scripts/run_dpdk_algorithm_scaling.sh -o /tmp/results
#

set -e

# Default values
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_ROOT}/benchmarks/data/algorithm_comparison"
MODE="algorithm_scaling"
REBUILD=false
IMAGE_NAME="liblpm-dpdk"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Run DPDK algorithm scaling benchmark via Docker"
    echo ""
    echo "Options:"
    echo "  -m, --mode MODE       Benchmark mode: algorithm_scaling, comparison, or both"
    echo "                        (default: algorithm_scaling)"
    echo "  -o, --output DIR      Output directory for results"
    echo "                        (default: benchmarks/data/algorithm_comparison)"
    echo "  -r, --rebuild         Force rebuild of Docker image"
    echo "  -h, --help            Show this help"
    echo ""
    echo "Examples:"
    echo "  $0"
    echo "  $0 --mode both"
    echo "  $0 -o /tmp/results"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -m|--mode)
            MODE="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -r|--rebuild)
            REBUILD=true
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
echo -e "${BLUE}  DPDK Algorithm Scaling Benchmark${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed or not in PATH${NC}"
    exit 1
fi

# Check if we need to build the image
IMAGE_EXISTS=$(docker images -q "$IMAGE_NAME" 2>/dev/null)

if [ -z "$IMAGE_EXISTS" ] || [ "$REBUILD" = true ]; then
    echo -e "${YELLOW}Building Docker image: $IMAGE_NAME${NC}"
    echo ""
    
    cd "$PROJECT_ROOT"
    docker build -f docker/Dockerfile.dpdk -t "$IMAGE_NAME" .
    
    echo ""
    echo -e "${GREEN}Docker image built successfully!${NC}"
    echo ""
else
    echo -e "${GREEN}Using existing Docker image: $IMAGE_NAME${NC}"
    echo "(Use --rebuild to force rebuild)"
    echo ""
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Get absolute path for output directory
OUTPUT_DIR_ABS="$(cd "$OUTPUT_DIR" && pwd)"

echo -e "${BLUE}Configuration:${NC}"
echo "  Mode: $MODE"
echo "  Output directory: $OUTPUT_DIR_ABS"
echo ""

echo -e "${YELLOW}Running benchmark in Docker container...${NC}"
echo ""

# Run the benchmark
# --privileged is needed for DPDK to work properly
# -v mounts the output directory for result retrieval
docker run --rm \
    --privileged \
    -v "$OUTPUT_DIR_ABS:/output" \
    "$IMAGE_NAME" \
    "$MODE"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Benchmark Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Results saved to: $OUTPUT_DIR_ABS"
echo ""
echo "To view the results:"
echo "  ls -la $OUTPUT_DIR_ABS/"
echo ""
echo "To generate charts, run:"
echo "  python scripts/plot_lpm_benchmark.py \\"
echo "    $OUTPUT_DIR_ABS/*_ipv4_single/*.csv \\"
echo "    --output docs/images/ipv4_single_with_dpdk.png"

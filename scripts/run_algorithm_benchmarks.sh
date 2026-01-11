#!/bin/bash
#
# LPM Algorithm Benchmark Runner
#
# This script runs the algorithm scaling benchmark and organizes results
# for easy visualization and comparison.
#
# Usage:
#   ./scripts/run_algorithm_benchmarks.sh [OPTIONS]
#
# Options:
#   -a, --algorithm ALGO    Run only specified algorithm (dir24, 4stride8, wide16, 6stride8)
#   -t, --type TYPE         Run only specified lookup type (single, batch)
#   -c, --cpu CPU           Pin to specific CPU core (default: 0)
#   -o, --output DIR        Output directory (default: benchmarks/data/algorithm_comparison)
#   -n, --name NAME         Override hostname for output files
#   -h, --help              Show this help
#
# Examples:
#   # Run all benchmarks
#   ./scripts/run_algorithm_benchmarks.sh
#
#   # Run only DIR-24 benchmarks
#   ./scripts/run_algorithm_benchmarks.sh -a dir24
#
#   # Run only single lookups, pin to CPU 2
#   ./scripts/run_algorithm_benchmarks.sh -t single -c 2
#

set -e

# Default values
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
BENCHMARK_BIN="${BUILD_DIR}/benchmarks/bench_algorithm_scaling"
OUTPUT_DIR="${PROJECT_ROOT}/benchmarks/data/algorithm_comparison"
CPU_CORE=0
HOSTNAME_OVERRIDE=""
ALGORITHM=""
LOOKUP_TYPE=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -a, --algorithm ALGO    Run only specified algorithm (dir24, 4stride8, wide16, 6stride8)"
    echo "  -t, --type TYPE         Run only specified lookup type (single, batch)"
    echo "  -c, --cpu CPU           Pin to specific CPU core (default: 0)"
    echo "  -o, --output DIR        Output directory (default: benchmarks/data/algorithm_comparison)"
    echo "  -n, --name NAME         Override hostname for output files"
    echo "  -b, --build             Build the benchmark before running"
    echo "  -h, --help              Show this help"
    echo ""
    echo "Examples:"
    echo "  # Run all benchmarks"
    echo "  $0"
    echo ""
    echo "  # Run only DIR-24 benchmarks"
    echo "  $0 -a dir24"
    echo ""
    echo "  # Run only single lookups, pin to CPU 2"
    echo "  $0 -t single -c 2"
}

# Parse arguments
BUILD_FIRST=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--algorithm)
            ALGORITHM="$2"
            shift 2
            ;;
        -t|--type)
            LOOKUP_TYPE="$2"
            shift 2
            ;;
        -c|--cpu)
            CPU_CORE="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -n|--name)
            HOSTNAME_OVERRIDE="$2"
            shift 2
            ;;
        -b|--build)
            BUILD_FIRST=true
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
echo -e "${BLUE}  LPM Algorithm Scaling Benchmark${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Build if requested or binary doesn't exist
if [[ "$BUILD_FIRST" == "true" ]] || [[ ! -f "$BENCHMARK_BIN" ]]; then
    echo -e "${YELLOW}Building benchmark...${NC}"
    
    # Create build directory if needed
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with Release build type for optimal performance
    cmake -DCMAKE_BUILD_TYPE=Release ..
    
    # Build just the benchmark target
    make bench_algorithm_scaling -j$(nproc)
    
    cd "$PROJECT_ROOT"
    echo -e "${GREEN}Build complete!${NC}"
    echo ""
fi

# Check if benchmark binary exists
if [[ ! -f "$BENCHMARK_BIN" ]]; then
    echo -e "${RED}Error: Benchmark binary not found at $BENCHMARK_BIN${NC}"
    echo "Run with -b flag to build first, or build manually:"
    echo "  mkdir -p build && cd build"
    echo "  cmake -DCMAKE_BUILD_TYPE=Release .."
    echo "  make bench_algorithm_scaling"
    exit 1
fi

# Build benchmark arguments
BENCH_ARGS=()
BENCH_ARGS+=("-o" "$OUTPUT_DIR")
BENCH_ARGS+=("-c" "$CPU_CORE")

if [[ -n "$ALGORITHM" ]]; then
    BENCH_ARGS+=("-a" "$ALGORITHM")
fi

if [[ -n "$LOOKUP_TYPE" ]]; then
    BENCH_ARGS+=("-t" "$LOOKUP_TYPE")
fi

if [[ -n "$HOSTNAME_OVERRIDE" ]]; then
    BENCH_ARGS+=("-n" "$HOSTNAME_OVERRIDE")
fi

# Display configuration
echo -e "${BLUE}Configuration:${NC}"
echo "  Benchmark binary: $BENCHMARK_BIN"
echo "  Output directory: $OUTPUT_DIR"
echo "  CPU core: $CPU_CORE"
if [[ -n "$ALGORITHM" ]]; then
    echo "  Algorithm filter: $ALGORITHM"
fi
if [[ -n "$LOOKUP_TYPE" ]]; then
    echo "  Lookup type filter: $LOOKUP_TYPE"
fi
if [[ -n "$HOSTNAME_OVERRIDE" ]]; then
    echo "  Hostname override: $HOSTNAME_OVERRIDE"
fi
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run benchmark
echo -e "${YELLOW}Running benchmark...${NC}"
echo ""

"$BENCHMARK_BIN" "${BENCH_ARGS[@]}"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Benchmark Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Results saved to: $OUTPUT_DIR"
echo ""
echo "To generate charts, run:"
echo "  python scripts/plot_lpm_benchmark.py \\"
echo "    $OUTPUT_DIR/$(hostname)_ipv4_single/*.csv \\"
echo "    --output docs/images/ipv4_single_algorithm_comparison.png"
echo ""
